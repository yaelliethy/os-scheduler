#include "mmu.h"
#include <stdarg.h>
#include <string.h>

typedef struct {
    int in_use;
    int is_page_table;
    int locked;
    PCB *owner;
    int page_num;
    int R;
    int M;
} Frame;

static Frame frames[NUM_FRAMES];
static FILE *memory_log = NULL;

static void log_memory(const char *fmt, ...) {
    if (memory_log == NULL) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(memory_log, fmt, args);
    va_end(args);
    fflush(memory_log);
}

static int find_free_frame(void) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!frames[i].in_use) return i;
    }
    return -1;
}

static int select_nru_victim(int *modified_out) {
    for (int nru_class = 0; nru_class < 4; nru_class++) {
        for (int i = 0; i < NUM_FRAMES; i++) {
            if (!frames[i].in_use || frames[i].is_page_table || frames[i].locked) continue;
            int class_val = (frames[i].R ? 2 : 0) + (frames[i].M ? 1 : 0);
            if (class_val == nru_class) {
                if (modified_out) *modified_out = frames[i].M;
                return i;
            }
        }
    }
    return -1;
}

static void clear_frame(int frame) {
    frames[frame].in_use = 0;
    frames[frame].is_page_table = 0;
    frames[frame].locked = 0;
    frames[frame].owner = NULL;
    frames[frame].page_num = -1;
    frames[frame].R = 0;
    frames[frame].M = 0;
}

void mmu_init(void) {
    for (int i = 0; i < NUM_FRAMES; i++) clear_frame(i);
    memory_log = fopen("memory.log", "w");
}

void mmu_shutdown(void) {
    if (memory_log) fclose(memory_log);
    memory_log = NULL;
}

int mmu_allocate_page_table(PCB *pcb) {
    if (pcb == NULL) return -1;
    int frame = find_free_frame();
    if (frame < 0) {
        frame = select_nru_victim(NULL);
        if (frame < 0) return -1;
        if (frames[frame].owner && frames[frame].page_num >= 0) {
            invalidatePage(frames[frame].owner->page_table, frames[frame].page_num);
        }
    } else {
        log_memory("Free Physical page %d allocated\n", frame);
    }
    frames[frame].in_use = 1;
    frames[frame].is_page_table = 1;
    frames[frame].locked = 0;
    frames[frame].owner = pcb;
    frames[frame].page_num = -1;
    frames[frame].R = 0;
    frames[frame].M = 0;
    pcb->page_table_frame = frame;
    pcb->page_table = createPageTable(pcb->limit);
    return frame;
}

int mmu_load_initial_page(PCB *pcb, int current_time) {
    if (pcb == NULL || pcb->limit <= 0 || pcb->page_table == NULL) return -1;
    int page_num = 0;
    int frame = find_free_frame();
    if (frame < 0) {
        frame = select_nru_victim(NULL);
        if (frame < 0) return -1;
        if (frames[frame].owner && frames[frame].page_num >= 0) {
            invalidatePage(frames[frame].owner->page_table, frames[frame].page_num);
        }
    } else {
        log_memory("Free Physical page %d allocated\n", frame);
    }
    frames[frame].in_use = 1;
    frames[frame].is_page_table = 0;
    frames[frame].locked = 0;
    frames[frame].owner = pcb;
    frames[frame].page_num = page_num;
    frames[frame].R = 0;
    frames[frame].M = 0;
    setPageEntry(pcb->page_table, page_num, frame);
    log_memory("At time %d disk address %d for process %d is loaded into memory page %d.\n",
               current_time, pcb->base + page_num, pcb->id, frame);
    return frame;
}

int mmu_access(PCB *pcb, const MemRequest *req, int current_time, PendingIO *io_out) {
    if (pcb == NULL || pcb->page_table == NULL || req == NULL) return 0;
    int page_num = getPageNum(req->virtual_address);
    if (page_num < 0 || page_num >= pcb->limit) return 0;
    if (isPageValid(pcb->page_table, page_num)) {
        updateBits(pcb->page_table, page_num, req->rw);
        int frame = pcb->page_table->entries[page_num].frameNumber;
        if (frame >= 0 && frame < NUM_FRAMES) {
            frames[frame].R = 1;
            if (req->rw == 'w' || req->rw == 'W') frames[frame].M = 1;
        }
        return 0;
    }
    if (io_out == NULL) return 1;
    log_memory("PageFault upon VA %s from process %d\n",
               req->va_str[0] ? req->va_str : "0", pcb->id);
    int frame = find_free_frame();
    int modified = 0;
    if (frame >= 0) {
        log_memory("Free Physical page %d allocated\n", frame);
    } else {
        frame = select_nru_victim(&modified);
        if (frame < 0) return -1;
        if (modified) log_memory("Swapping out page %d to disk\n", frame);
        if (frames[frame].owner && frames[frame].page_num >= 0) {
            invalidatePage(frames[frame].owner->page_table, frames[frame].page_num);
        }
    }
    frames[frame].in_use = 1;
    frames[frame].is_page_table = 0;
    frames[frame].locked = 1;
    frames[frame].owner = pcb;
    frames[frame].page_num = page_num;
    frames[frame].R = 0;
    frames[frame].M = 0;
    io_out->process = pcb;
    io_out->page_num = page_num;
    io_out->frame_num = frame;
    io_out->rw = req->rw;
    io_out->disk_address = pcb->base + page_num;
    io_out->wake_time = current_time + 10 + (modified ? 10 : 0);
    strncpy(io_out->va_str, req->va_str, sizeof(io_out->va_str) - 1);
    io_out->va_str[sizeof(io_out->va_str) - 1] = '\0';
    return 1;
}

void mmu_complete_io(const PendingIO *io, int current_time) {
    if (io == NULL || io->process == NULL) return;
    PCB *pcb = io->process;
    if (pcb->page_table == NULL) return;
    setPageEntry(pcb->page_table, io->page_num, io->frame_num);
    updateBits(pcb->page_table, io->page_num, io->rw);
    frames[io->frame_num].locked = 0;
    frames[io->frame_num].owner = pcb;
    frames[io->frame_num].page_num = io->page_num;
    frames[io->frame_num].is_page_table = 0;
    frames[io->frame_num].R = 1;
    frames[io->frame_num].M = (io->rw == 'w' || io->rw == 'W') ? 1 : 0;
    log_memory("At time %d disk address %d for process %d is loaded into memory page %d.\n",
               current_time, io->disk_address, pcb->id, io->frame_num);
}

void mmu_free_process(PCB *pcb) {
    if (pcb == NULL) return;
    if (pcb->page_table) {
        for (int i = 0; i < pcb->limit; i++) {
            if (pcb->page_table->entries[i].valid) {
                int frame = pcb->page_table->entries[i].frameNumber;
                if (frame >= 0 && frame < NUM_FRAMES) clear_frame(frame);
            }
        }
        freePageTable(pcb->page_table);
        pcb->page_table = NULL;
    }
    if (pcb->page_table_frame >= 0 && pcb->page_table_frame < NUM_FRAMES) {
        clear_frame(pcb->page_table_frame);
    }
}

void mmu_clear_referenced(void) {
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (!frames[i].in_use || frames[i].is_page_table || frames[i].locked) continue;
        frames[i].R = 0;
        if (frames[i].owner && frames[i].page_num >= 0) {
            PTE *entry = &frames[i].owner->page_table->entries[frames[i].page_num];
            entry->R = 0;
        }
    }
}

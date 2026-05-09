#ifndef MMU_H
#define MMU_H

#include "headers.h"
#include "page_table.h"

#define MEMORY_SIZE 512
#define NUM_FRAMES (MEMORY_SIZE / PAGE_SIZE)

typedef struct {
    PCB *process;
    int page_num;
    int frame_num;
    char rw;
    int wake_time;
    int disk_address;
    char va_str[16];
} PendingIO;

void mmu_init(void);
void mmu_shutdown(void);
int mmu_allocate_page_table(PCB *pcb);
int mmu_load_initial_page(PCB *pcb);
int mmu_access(PCB *pcb, const MemRequest *req, int current_time, PendingIO *io_out);
void mmu_complete_io(const PendingIO *io, int current_time);
void mmu_free_process(PCB *pcb);
void mmu_clear_referenced(void);

#endif

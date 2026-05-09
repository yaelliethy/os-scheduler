#include "headers.h"
#include <signal.h>
#include "scheduler_output.h"
#include "memory.h"

CircularQueue *queue;
Deque *deque;
PriQueue *pq;
PCB *currentProcess;
int currentAlgorithm;
int* doneCountPtr;
int processStartTime;
int cpu_number;

Frame physical_memory[TOTAL_FRAMES];
float *allWTAs;
float totalWaiting = 0.0f;
int totalRunTime = 0;
int firstStartTime = -1;
int lastFinishTime = 0;
int completedForPerf = 0;
int processTarget = 0;
#define FRAME_RETRY_DELAY_US 100000

static inline int calculate_waiting_time(PCB *process, int currentTime) {
    int executedTime = process->runtime - process->remaining_time;
    int waiting = currentTime - process->arrival - executedTime;
    return waiting < 0 ? 0 : waiting;
}

void startCurrentProcess() {
    sleep(1);
    int currentTime = getClk();
    if (currentProcess->start_time == -1) {
        int pid = fork();
        if (pid == 0) {
            char id_str[10], runtime_str[10];
            sprintf(id_str, "%d", currentProcess->id);
            sprintf(runtime_str, "%d", currentProcess->runtime);
            execl("./process.out", "./process.out", id_str, runtime_str, NULL);
            return;
        }
        currentProcess->start_time = currentTime;
        currentProcess->pid = pid;
        if (firstStartTime == -1) firstStartTime = currentTime;
        log_started(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, calculate_waiting_time(currentProcess, currentTime), cpu_number);
    } else {
        kill(currentProcess->pid, SIGCONT);
        currentProcess->start_time = currentTime;
        log_resumed(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, calculate_waiting_time(currentProcess, currentTime), cpu_number);
    }
    currentProcess->status = RUNNING;
    processStartTime = currentTime;
}

void finishCurrentProcess() {
    if (currentProcess == NULL) return;
    int currentTime = getClk();
    int ta = currentTime - currentProcess->arrival;
    int waiting = ta - currentProcess->runtime;
    waiting = waiting < 0 ? 0 : waiting;
    float wta = (float)ta / currentProcess->runtime;

    if (completedForPerf < processTarget) allWTAs[completedForPerf] = wta;
    totalWaiting += waiting;
    totalRunTime += currentProcess->runtime;
    if (currentTime > lastFinishTime) lastFinishTime = currentTime;
    completedForPerf++;

    log_finished(currentTime, currentProcess->id, currentProcess->arrival, currentProcess->runtime, waiting, ta, wta, cpu_number);
    
    // Reset currentProcess pointer before picking next
    PCB* finished = currentProcess;
    finished->end_time = currentTime;
    if (finished->page_table_frame_i != -1) {
        free_process_frames(physical_memory, finished->id);
        finished->page_table_frame_i = -1;
    }

    if (currentAlgorithm == 1) {
        PCB *temp;
        dequeueCircular(queue, &temp);
        if (!isCircularQueueEmpty(queue)) {
            currentProcess = queue->front->process;
            startCurrentProcess();
        } else { currentProcess = NULL; }
    } else if (currentAlgorithm == 2) {
        removetop(pq);
        if(!isEmpty(pq)) {
            currentProcess = peek(pq);
            startCurrentProcess();
        } else { currentProcess = NULL; }
    } else if (currentAlgorithm == 3) {
        PCB *temp;
        popFront(deque, &temp);
        if (!isDequeEmpty(deque)) {
            currentProcess = deque->front->process;
            startCurrentProcess();
        } else { currentProcess = NULL; }
    }
}

void stopCurrentProcess() {
    int currentTime = getClk();

    if (currentProcess->start_time != -1) {
        int burstTime = currentTime - currentProcess->start_time;
        currentProcess->cpu_time_used += burstTime;
        currentProcess->remaining_time -= burstTime;
    }

    currentProcess->status = WAITING;
    if (currentProcess->remaining_time < 0) currentProcess->remaining_time = 0;

    log_stopped(currentTime, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, calculate_waiting_time(currentProcess, currentTime), cpu_number);
    kill(currentProcess->pid, SIGSTOP);
}

void processTerminationHandler(int signum) { destroyClk(true); }
void processFinishedHandler(int signum) { finishCurrentProcess(); (*doneCountPtr)++; }

void writePerf() {
    if (completedForPerf == 0) {
        write_scheduler_perf(0.0f, 0.0f, 0.0f, 0.0f, cpu_number);
        return;
    }
    float sumWTA = 0.0f;
    for (int i = 0; i < completedForPerf; i++) sumWTA += allWTAs[i];
    float avgWTA = sumWTA / completedForPerf;
    float avgWaiting = totalWaiting / completedForPerf;
    float stdWTA = 0.0f;
    for (int i = 0; i < completedForPerf; i++) {
        float diff = allWTAs[i] - avgWTA;
        stdWTA += diff * diff;
    }
    stdWTA = sqrtf(stdWTA / completedForPerf);
    float cpuUtil = 0.0f;
    if (firstStartTime != -1 && lastFinishTime > firstStartTime) {
        cpuUtil = ((float)totalRunTime / (float)(lastFinishTime - firstStartTime)) * 100.0f;
    }
    write_scheduler_perf(cpuUtil, avgWTA, avgWaiting, stdWTA, cpu_number);
}

int main(int argc, char *argv[]) {
    initClk();
    currentAlgorithm = atoi(argv[1]);
    cpu_number = atoi(argv[4]);
    int quantum = atoi(argv[2]);
    int count = atoi(argv[3]);
    processTarget = count;
    allWTAs = (float *)malloc(sizeof(float) * count);
    
    int shmid;
    if(currentAlgorithm == 3){
        shmid = shmget((cpu_number == 1) ? SHQUEUE1 : SHQUEUE2, sizeof(Deque), 0666 | IPC_CREAT);
        deque = (Deque*)shmat(shmid, (void *)0, 0);
        initDeque(deque);
    }
    pq = (PriQueue*)malloc(sizeof(PriQueue));
    queue = (CircularQueue*)malloc(sizeof(CircularQueue));
    initCircularQueue(queue);
    initializeQueue(pq);
    initialize_frames(physical_memory);
    signal(SIGTERM, processTerminationHandler);
    signal(SIGUSR1, processFinishedHandler);

    int doneCountShmid = shmget(SHDONE, sizeof(int), 0666 | IPC_CREAT);
    doneCountPtr = (int *)shmat(doneCountShmid, (void *)0, 0);

    bool firstProcess = true;
    int msgqid = msgget((currentAlgorithm == 3 && cpu_number == 2) ? QUEUE_KEY2 : QUEUE_KEY, 0666 | IPC_CREAT);
    int other_msgqid = (currentAlgorithm == 3) ? msgget((cpu_number == 1) ? QUEUE_KEY2 : QUEUE_KEY, 0666 | IPC_CREAT) : -1;

    processStartTime = -1;
    int lastTime = -1;

    while (*doneCountPtr < count) {
        int currentTime = getClk();

 
        if (currentProcess != NULL && currentProcess->status == RUNNING) {
            int elapsed = currentTime - currentProcess->start_time;
            int cpu_now = currentProcess->cpu_time_used + elapsed;

            while (currentProcess->next_req < currentProcess->req_count) {
                MemRequest *req = &currentProcess->requests[currentProcess->next_req];
                if (cpu_now >= req->cpu_time) {
                    printf("Scheduler: process %d memory request at cpu_time=%d va=%d %c\n",
                           currentProcess->id, req->cpu_time, req->virtual_address, req->rw);
                    /* mmu_access(currentProcess, req->virtual_address, req->rw); */
                    currentProcess->next_req++;
                } else break;
            }
        }

        /* Check for Round Robin quantum expiry */
        if (currentAlgorithm == 1 && currentProcess != NULL && processStartTime != -1) {
            if ((currentTime - processStartTime) >= quantum && circularQueueSize(queue) > 1) {
                stopCurrentProcess();
                moveHeadCircular(queue, &currentProcess);
                startCurrentProcess();
            }
        }

        /* Receive new processes */
        struct msgbuff incoming;
        if (msgrcv(msgqid, &incoming, sizeof(incoming) - sizeof(long), 0, IPC_NOWAIT) != -1) {
            if (incoming.mtype == 2) { /* Load Balancing Logic */
                PCB *temp;
                popRear(deque, &temp);
                if (temp != NULL && (currentProcess == NULL || temp->id != currentProcess->id)) {
                    struct msgbuff msg = { .mtype = 1, .id = temp->id, .arrival = temp->arrival, .runtime = temp->runtime, .priority = temp->priority, .base = temp->base, .limit = temp->limit };
                    msgsnd(other_msgqid, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
                }
                continue;
            }

            PCB *pcb = (PCB *)malloc(sizeof(PCB));
            pcb->id = incoming.id;
            pcb->arrival = incoming.arrival;
            pcb->runtime = incoming.runtime;
            pcb->priority = incoming.priority;
            pcb->remaining_time = incoming.runtime;
            pcb->base = incoming.base;
            pcb->limit = incoming.limit;
            pcb->begin_time = currentTime;
            pcb->end_time = -1;
            pcb->pid = -1;
            pcb->cpu_time_used = 0;
            pcb->next_req = 0;
            pcb->start_time = -1;
            pcb->status = READY;
            pcb->page_table_frame_i = -1;
            pcb->req_count = 0;

            if (currentAlgorithm == 1) {
                int frame_index = getfreeframe(physical_memory);
                if (frame_index == -1) {
                    if (msgsnd(msgqid, &incoming, sizeof(incoming) - sizeof(long), IPC_NOWAIT) == -1) {
                        perror("Message queue error while re-queuing deferred process");
                    }
                    free(pcb);
                    usleep(FRAME_RETRY_DELAY_US);
                    continue;
                }
                allocate_frame(physical_memory, pcb->id, frame_index, -1, 1);
                pcb->page_table_frame_i = frame_index;
            }

            if (firstProcess) {
                currentProcess = pcb;
                startCurrentProcess();
                firstProcess = false;
            }
            if (currentAlgorithm == 1) enqueueCircular(queue, pcb);
            else if (currentAlgorithm == 2) {
                insert(pq, pcb);
                if (pcb->priority < currentProcess->priority) {
                    stopCurrentProcess();
                    currentProcess = pcb;
                    startCurrentProcess();
                }
            }
            else if (currentAlgorithm == 3) pushRear(deque, pcb);
        } else {
            usleep(100000);
        }
    }
    writePerf();
    // Cleanup code...
}

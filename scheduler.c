#include "headers.h"
#include <signal.h>
#include "scheduler_output.h"

CircularQueue *queue;
Deque *deque;
PriQueue *pq;
PCB *currentProcess;
int currentAlgorithm;
int* doneCountPtr;
int processStartTime;
int cpu_number;

float *allWTAs;
float totalWaiting = 0.0f;
int totalRunTime = 0;
int firstStartTime = -1;
int lastFinishTime = 0;
int completedForPerf = 0;
int processTarget = 0;

static inline int calculate_waiting_time(PCB *process, int currentTime)
{
    int executedTime = process->runtime - process->remaining_time;
    int waiting = currentTime - process->arrival - executedTime;
    return waiting < 0 ? 0 : waiting;
}

void startCurrentProcess()
{
    sleep(1);
    int currentTime = getClk();
    if (currentProcess->start_time == -1)
    {
        int pid = fork();
        if (pid == 0)
        {
            char id_str[10], runtime_str[10];
            sprintf(id_str, "%d", currentProcess->id);
            sprintf(runtime_str, "%d", currentProcess->runtime);
            execl("./process.out", "./process.out", id_str, runtime_str, NULL);
            return;
        }
        currentProcess->start_time = currentTime;
        currentProcess->pid = pid;
        if (firstStartTime == -1)
            firstStartTime = currentTime;
        log_started(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, calculate_waiting_time(currentProcess, currentTime), cpu_number);
    }
    else
    {
        kill(currentProcess->pid, SIGCONT);
        currentProcess->start_time = currentTime;
        log_resumed(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, calculate_waiting_time(currentProcess, currentTime), cpu_number);
    }
    currentProcess->status = RUNNING;
    processStartTime = currentTime;
}

void finishCurrentProcess()
{
    if (currentProcess == NULL)
        return;

    int currentTime = getClk();
    int ta = currentTime - currentProcess->arrival;
    int waiting = ta - currentProcess->runtime;
    waiting = waiting < 0 ? 0 : waiting;
    float wta = (float)ta / currentProcess->runtime;

    if (completedForPerf < processTarget)
        allWTAs[completedForPerf] = wta;
    totalWaiting += waiting;
    totalRunTime += currentProcess->runtime;
    if (currentTime > lastFinishTime)
        lastFinishTime = currentTime;
    completedForPerf++;

    log_finished(currentTime, currentProcess->id, currentProcess->arrival, currentProcess->runtime, waiting, ta, wta, cpu_number);
    if (currentAlgorithm == 1)
    {
        PCB *temp;
        dequeueCircular(queue, &temp);
        if (!isCircularQueueEmpty(queue)) {
            currentProcess = queue->front->process;
            startCurrentProcess();
        }
    }
    else if (currentAlgorithm == 2)
    {
        removetop(pq);
        if(!isEmpty(pq)){
            currentProcess = peek(pq);
            startCurrentProcess();
        }
    }
    else if (currentAlgorithm == 3){
        PCB *temp;
        popFront(deque, &temp);
        if (!isDequeEmpty(deque)) {
            currentProcess = deque->front->process;
            startCurrentProcess();
        }
    }
    currentProcess->end_time = currentTime;
}

void stopCurrentProcess()
{
    int currentTime = getClk();
    currentProcess->status = WAITING;
    currentProcess->remaining_time -= currentTime - currentProcess->start_time;
    currentProcess->remaining_time = currentProcess->remaining_time < 0 ? 0 : currentProcess->remaining_time;
    log_stopped(currentTime, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, calculate_waiting_time(currentProcess, currentTime), cpu_number);
    kill(currentProcess->pid, SIGSTOP);
}

void processTerminationHandler(int signum)
{
    destroyClk(true);
}
void processFinishedHandler(int signum)
{
    finishCurrentProcess();
    (*doneCountPtr)++;
}
void writePerf() {
    if (completedForPerf == 0) {
        write_scheduler_perf(0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float sumWTA = 0.0f;
    for (int i = 0; i < completedForPerf; i++)
        sumWTA += allWTAs[i];

    float avgWTA = sumWTA / completedForPerf;
    float avgWaiting = totalWaiting / completedForPerf;

    float stdWTA = 0.0f;
    for (int i = 0; i < completedForPerf; i++)
    {
        float diff = allWTAs[i] - avgWTA;
        stdWTA += diff * diff;
    }
    stdWTA = sqrtf(stdWTA / completedForPerf);

    float cpuUtil = 0.0f;
    if (firstStartTime != -1 && lastFinishTime > firstStartTime) {
        cpuUtil = ((float)totalRunTime / (float)(lastFinishTime - firstStartTime)) * 100.0f;
    }

    write_scheduler_perf(cpuUtil, avgWTA, avgWaiting, stdWTA);
}
int main(int argc, char *argv[])
{
    initClk();
    currentAlgorithm = atoi(argv[1]);
    cpu_number = atoi(argv[4]);
    int quantum = atoi(argv[2]);
    int count = atoi(argv[3]);
    processTarget = count;
    allWTAs = (float *)malloc(sizeof(float) * count);
    if (allWTAs == NULL)
    {
        perror("Error allocating WTA array");
        exit(-1);
    }
    //if currentAlgorithm is 3, make a shared memory for each CPU's queue
    int shmid;
    if(currentAlgorithm == 3){
        if (cpu_number == 1)
            shmid = shmget(SHQUEUE1, sizeof(Deque), 0666 | IPC_CREAT);
        else
            shmid = shmget(SHQUEUE2, sizeof(Deque), 0666 | IPC_CREAT);
        deque = (Deque*)shmat(shmid, (void *)0, 0);
        initDeque(deque);
    }
    pq = (PriQueue*)malloc(sizeof(PriQueue));
    queue = (CircularQueue*)malloc(sizeof(CircularQueue));
    initCircularQueue(queue);
    initializeQueue(pq);
    signal(SIGTERM, processTerminationHandler);
    signal(SIGUSR1, processFinishedHandler);
    //get done count from shared memory
    int doneCountShmid = shmget(SHDONE, sizeof(int), 0666 | IPC_CREAT);
    doneCountPtr = (int *)shmat(doneCountShmid, (void *)0, 0);
    if(doneCountPtr == (void *)-1){
        perror("Error creating shared memory for done count");
        exit(-1);
    }
    if(*doneCountPtr == -1){
        printf("Error initializing done count in shared memory");
        *doneCountPtr = 0;
    }
    bool firstProcess = true;
    int msgqid;
    int other_msgqid;
    if (currentAlgorithm == 3){
        int this_cpu_queue_key = (cpu_number == 1) ? QUEUE_KEY : QUEUE_KEY2;
        int other_cpu_queue_key = (cpu_number == 1) ? QUEUE_KEY2 : QUEUE_KEY;
        msgqid = msgget(this_cpu_queue_key, 0666 | IPC_CREAT);
        other_msgqid = msgget(other_cpu_queue_key, 0666 | IPC_CREAT);
    }
    else
        msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT);
    if (msgqid == -1)
    {
        perror("Error getting message queue");
        exit(-1);
    }
    processStartTime = -1;
    while (*doneCountPtr < count)
    {
        int currentTime = getClk();
        if (currentAlgorithm == 1 && currentProcess != NULL && processStartTime != -1)
        {
            int elapsedTime = currentTime - processStartTime;
            if (elapsedTime >= quantum && circularQueueSize(queue) > 1)
            {
                stopCurrentProcess();
                moveHeadCircular(queue, &currentProcess);
                startCurrentProcess();
            }
        }
        struct msgbuff process;
        if (msgrcv(msgqid, &process, sizeof(process) - sizeof(long), 0, IPC_NOWAIT) == -1)
        {
            // No process arrived at this tick
            usleep(100000); // Sleep for 100ms to avoid busy waiting
            continue;
        }
        if (process.mtype == 2) // Remove from this CPU's dequeue and add to the other CPU's queue
        {
            sleep(3);
            currentTime = getClk();
            PCB *temp;
            popRear(deque, &temp);
            if(temp == NULL || temp->id == currentProcess->id) continue; // Don't move if the process is currently running or if the deque is empty
            struct msgbuff msg;
            msg.mtype = 1;
            msg.id = temp->id;
            msg.arrival = temp->arrival;
            msg.runtime = temp->runtime;
            msg.priority = temp->priority;
            printf("Moving process %d from CPU %d to CPU %d\n", temp->id, cpu_number, (cpu_number == 1) ? 2 : 1);
            msgsnd(other_msgqid, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
            continue;
        }
        PCB *pcb = (PCB *)malloc(sizeof(PCB));
        pcb->id = process.id;
        pcb->begin_time = currentTime;
        pcb->start_time = -1;
        pcb->end_time = -1;
        pcb->arrival = process.arrival;
        pcb->runtime = process.runtime;
        pcb->priority = process.priority;
        pcb->remaining_time = process.runtime;

        if (firstProcess)
        {
            currentProcess = pcb;
            startCurrentProcess();
            firstProcess = false;
        }
        if (currentAlgorithm == 1)
        {
            enqueueCircular(queue, pcb);
        }
        if (currentAlgorithm == 2)
        {
            insert(pq, pcb);
            if (pcb->priority < currentProcess->priority)
            {
                stopCurrentProcess();
                currentProcess = pcb;
                startCurrentProcess();
            }
        }
        if(currentAlgorithm == 3){
            //Enqueue process
            pushRear(deque, pcb);
        }
    }

    writePerf();
    close_scheduler_log();

    // Clean up message queue and shared memory
    msgctl(msgqid, IPC_RMID, NULL);
    shmdt(doneCountPtr);
    shmdt(shmaddr);
    shmctl(doneCountShmid, IPC_RMID, NULL);
    if(currentAlgorithm == 3)  shmctl(shmid, IPC_RMID, NULL);
    free(allWTAs);
}
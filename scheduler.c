#include "headers.h"
#include <signal.h>
#include "scheduler_output.h"

CircularQueue *queue;
DoneQueue doneQueue;
PriQueue pq;
PCB *currentProcess;
int currentAlgorithm;
int algoPid;

void startCurrentProcess()
{
    sleep(1);
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
        currentProcess->start_time = getClk();
        currentProcess->pid = pid;
        log_started(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, 0);
    }
    else
    {
        kill(currentProcess->pid, SIGCONT);
        printf("Resuming process with pid %d at time %d\n", currentProcess->pid, getClk());
        currentProcess->start_time = getClk();
        log_resumed(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, getClk() - currentProcess->start_time);
    }
    currentProcess->status = RUNNING;
}

void finishCurrentProcess()
{
    if (currentAlgorithm == 1)
    {
        PCB *temp;
        dequeueCircular(queue, &temp);
    }
    else if (currentAlgorithm == 2)
    {
        removetop(&pq);
    }
    currentProcess->end_time = getClk();
    enqueueDone(&doneQueue, currentProcess);
    log_finished(currentProcess->end_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->end_time - currentProcess->start_time, currentProcess->end_time - currentProcess->arrival, (float)(currentProcess->end_time - currentProcess->arrival) / currentProcess->runtime);
}

void stopCurrentProcess()
{
    printf("Stopping process with pid %d at time %d\n", currentProcess->pid, getClk());
    currentProcess->status = WAITING;
    currentProcess->remaining_time -= getClk() - currentProcess->start_time;
    printf("Process %d has remaining time %d\n", currentProcess->id, currentProcess->remaining_time);
    kill(currentProcess->pid, SIGSTOP);
    log_stopped(getClk(), currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, getClk() - currentProcess->start_time);
}

void processTerminationHandler(int signum)
{
    kill(algoPid, SIGTERM);
    destroyClk(true);
}

int main(int argc, char *argv[])
{
    printf("Scheduler started with algorithm %d\n", atoi(argv[1]));
    initClk();
    queue = (CircularQueue *)malloc(sizeof(CircularQueue));
    initCircularQueue(queue);
    initDoneQueue(&doneQueue);
    initializeQueue(&pq);
    signal(SIGTERM, processTerminationHandler);

    int quantum = atoi(argv[2]);
    int count = atoi(argv[3]);
    int doneCount = 0;
    bool firstProcess = true;
    currentAlgorithm = atoi(argv[1]);

    int msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT);
    if (msgqid == -1)
    {
        perror("Error getting message queue");
        exit(-1);
    }

    if (currentAlgorithm == 1)
    {
        algoPid = fork();
        if (algoPid == 0)
        {
            while (1)
            {
                sleep(1);
                if (isCircularQueueEmpty(queue)){
                    printf("Circular queue is empty at time %d\n", getClk());
                    continue;
                }
                int currentTime = getClk();
                if (currentTime % quantum == 0)
                {
                    printf("Time quantum expired at time %d\n", currentTime);
                    stopCurrentProcess();
                    moveHeadCircular(queue, &currentProcess);
                    startCurrentProcess();
                }
            }
        }
    }

    while (1)
    {
        struct msgbuff process;
        if (msgrcv(msgqid, &process, sizeof(process) - sizeof(long), 0, 0) == -1)
        {
            perror("Error receiving message");
            exit(-1);
        }

        if (process.mtype != 1)
        {
            finishCurrentProcess();
            doneCount++;
            if (doneCount == count)
            {
                break;
            }
            if (currentAlgorithm == 2)
            {
                if (isEmpty(&pq))
                    continue;
                currentProcess = peek(&pq);
                startCurrentProcess();
            }
            continue;
        }

        PCB *pcb = (PCB *)malloc(sizeof(PCB));
        pcb->id = process.id;
        pcb->begin_time = getClk();
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
        else if (currentAlgorithm == 2)
        {
            insert(&pq, pcb);
            printf("Top of priority queue is process with id %d and priority %d\n", peek(&pq)->id, peek(&pq)->priority);
            if (pcb->priority < currentProcess->priority)
            {
                stopCurrentProcess();
                currentProcess = pcb;
                startCurrentProcess();
            }
        }
    }
}
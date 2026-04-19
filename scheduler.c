#include "headers.h"
#include <signal.h>
#include "scheduler_output.h"
#include <math.h>

CircularQueue *queue;
DoneQueue doneQueue;
PriQueue pq;
PCB *currentProcess;
int currentAlgorithm;
int algoPid;

float allWTAs[1000];
float totalWaiting = 0;
int totalRunTime = 0;
int firstStartTime = -1;
int lastFinishTime = 0;
int processCount = 0;

void startCurrentProcess()
{
    sleep(1); // context switch overhead
    if (currentProcess->start_time == -1)
    {
        // first time running - fork it
        pid_t pid = fork();
        if (pid == 0)
        {
            char remaining_str[10];
            char id_str[10];
            sprintf(remaining_str, "%d", currentProcess->remaining_time);
            sprintf(id_str, "%d", currentProcess->id);
            execl("./process.out", "./process.out", id_str, remaining_str, NULL);
            exit(1); // execl failed
        }
        currentProcess->pid = pid;
        currentProcess->start_time = getClk();
        currentProcess->last_run_time = getClk();
        currentProcess->status = RUNNING;
        log_started(
            currentProcess->start_time,
            currentProcess->id,
            currentProcess->arrival,
            currentProcess->runtime,
            currentProcess->remaining_time,
            currentProcess->waiting_time
        );
    }
    else
    {
        // resuming - update waiting time first
        currentProcess->waiting_time += getClk() - currentProcess->stop_time;
        currentProcess->last_run_time = getClk();
        kill(currentProcess->pid, SIGCONT);
        currentProcess->status = RUNNING;
        log_resumed(
            getClk(),
            currentProcess->id,
            currentProcess->arrival,
            currentProcess->runtime,
            currentProcess->remaining_time,
            currentProcess->waiting_time
        );
    }
}

void finishCurrentProcess()
{
    currentProcess->end_time = getClk();
    int TA = currentProcess->end_time - currentProcess->arrival;
    float WTA = (float)TA / currentProcess->runtime;

// update perf tracking
    allWTAs[processCount] = WTA;
    totalWaiting += currentProcess->waiting_time;
    totalRunTime += currentProcess->runtime;
    if (firstStartTime == -1) firstStartTime = currentProcess->start_time;
    if (currentProcess->end_time > lastFinishTime) lastFinishTime = currentProcess->end_time;
    processCount++;

    // enqueue to done queue
    enqueueDone(&doneQueue, *currentProcess);

    log_finished(
        currentProcess->end_time,
        currentProcess->id,
        currentProcess->arrival,
        currentProcess->runtime,
        currentProcess->waiting_time,
        TA,
        WTA
    );

    currentProcess = NULL; // CPU is now free
}

void stopCurrentProcess()
{
    currentProcess->remaining_time -= getClk() - currentProcess->last_run_time; // update remaining time
    currentProcess->stop_time = getClk(); // save when it was stopped
    currentProcess->status = WAITING;
    kill(currentProcess->pid, SIGSTOP);
    log_stopped(
        getClk(),
        currentProcess->id,
        currentProcess->arrival,
        currentProcess->runtime,
        currentProcess->remaining_time,
        currentProcess->waiting_time
    );
}

void writePerf() {
    // calculate averages
    float sumWTA = 0;
    for (int i = 0; i < processCount; i++) sumWTA += allWTAs[i];
    float avgWTA = sumWTA / processCount;
    float avgWaiting = totalWaiting / processCount;
    float cpuUtil = ((float)totalRunTime / (lastFinishTime - firstStartTime)) * 100;

    // calculate standard deviation
    float stdWTA = 0;
    for (int i = 0; i < processCount; i++) {
        float diff = allWTAs[i] - avgWTA;
        stdWTA += diff * diff;
    }
    stdWTA = sqrtf(stdWTA / processCount);

    // write to file
    FILE* perfFile = fopen("scheduler.perf", "w");
    if (perfFile == NULL) {
        perror("failed to open scheduler.perf");
        return;
    }
    fprintf(perfFile, "CPU utilization = %.2f%%\n", cpuUtil);
    fprintf(perfFile, "Avg WTA = %.2f\n", avgWTA);
    fprintf(perfFile, "Avg Waiting = %.2f\n", avgWaiting);
    fprintf(perfFile, "Std WTA = %.2f\n", stdWTA);
    fclose(perfFile);
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

    if (currentAlgorithm == 2)
    {
        algoPid = fork();
        if (algoPid == 0)
        {
            int lastSwitch = getClk(); // track last switch time
            while (1)
            {
                sleep(1);
                if (isCircularQueueEmpty(queue)) continue;

                if (getClk() - lastSwitch >= quantum)
                {
                    stopCurrentProcess();
                    enqueueCircular(queue, *currentProcess); // put back in queue
                    moveHeadCircular(queue, &currentProcess);
                    startCurrentProcess();
                    lastSwitch = getClk(); // reset switch timer
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

        // process finished signal
        if (process.mtype != 1)
        {
            finishCurrentProcess(); // enqueues to done queue, sets currentProcess = NULL
            doneCount++;

            if (doneCount == count) break; // all processes done

            if (currentAlgorithm == 1) //HPF
            {
                if (isEmpty(&pq)) continue;
                PCB temp = removetop(&pq);
                currentProcess = (PCB *)malloc(sizeof(PCB));
                *currentProcess = temp; // ← removetop not peek
                startCurrentProcess();
            }
            continue;
        }

        // new process arrived
        PCB *pcb = (PCB *)malloc(sizeof(PCB));
        pcb->id = process.id;
        pcb->begin_time = getClk();
        pcb->start_time = -1;
        pcb->end_time = -1;
        pcb->stop_time = -1;
        pcb->arrival = process.arrival;
        pcb->runtime = process.runtime;
        pcb->priority = process.priority;
        pcb->remaining_time = process.runtime;
        pcb->waiting_time = 0;
        pcb->status = READY;

        if (currentAlgorithm == 2) // RR
        {
            enqueueCircular(queue, *pcb);
            if (firstProcess)
            {
                moveHeadCircular(queue, &currentProcess);
                startCurrentProcess();
                firstProcess = false;
            }
        }
        else if (currentAlgorithm == 1)
        {
            insert(&pq, *pcb);
            if (firstProcess)
            {
                PCB temp = removetop(&pq);
                currentProcess = (PCB *)malloc(sizeof(PCB));
                *currentProcess = temp;
                startCurrentProcess();
                firstProcess = false;
            }
            // preemption check
            else if (pcb->priority < currentProcess->priority)
            {
                stopCurrentProcess();
                insert(&pq, *currentProcess); // put back in queue
                *currentProcess = removetop(&pq);
                startCurrentProcess();
            }
        }
    }

    writePerf();
    msgctl(msgqid, IPC_RMID, NULL);
    destroyClk(true);
}
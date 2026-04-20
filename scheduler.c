#include "headers.h"
#include <signal.h>
#include "scheduler_output.h"

CircularQueue queue;
DoneQueue doneQueue;
PriQueue pq;
PCB *currentProcess;
int currentAlgorithm;
int doneCount;
int processStartTime;
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
        log_started(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, 0);
    }
    else
    {
        kill(currentProcess->pid, SIGCONT);
        printf("Resuming process with pid %d at time %d\n", currentProcess->pid, currentTime);
        currentProcess->start_time = currentTime;
        log_resumed(currentProcess->start_time, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, getClk() - currentProcess->start_time);
    }
    currentProcess->status = RUNNING;
}

void finishCurrentProcess()
{
    int currentTime = getClk();
    log_finished(currentTime, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentTime - currentProcess->start_time, currentTime - currentProcess->arrival, (float)(currentTime - currentProcess->arrival) / currentProcess->runtime);
    if (currentAlgorithm == 1)
    {
        PCB *temp;
        dequeueCircular(&queue, &temp);
        printf("Finishing process with pid %d at time %d\n", currentProcess->id, currentTime);
        printf("Dequeued process with id %d from circular queue at time %d\n", temp->id, currentTime);
        if (!isCircularQueueEmpty(&queue)) {
            currentProcess = queue.front->process;
            processStartTime = currentTime;
            startCurrentProcess();
        }
    }
    else if (currentAlgorithm == 2)
    {
        removetop(&pq);
        if(!isEmpty(&pq)){
            currentProcess = peek(&pq);
            startCurrentProcess();
        }
    }
    currentProcess->end_time = currentTime;
    enqueueDone(&doneQueue, currentProcess);
}

void stopCurrentProcess()
{
    int currentTime = getClk();
    printf("Stopping process with pid %d at time %d\n", currentProcess->pid, currentTime);
    currentProcess->status = WAITING;
    currentProcess->remaining_time -= currentTime - currentProcess->start_time;
    currentProcess->remaining_time = currentProcess->remaining_time < 0 ? 0 : currentProcess->remaining_time;
    printf("Process %d has remaining time %d\n", currentProcess->id, currentProcess->remaining_time);
    log_stopped(currentTime, currentProcess->id, currentProcess->arrival, currentProcess->runtime, currentProcess->remaining_time, currentTime - currentProcess->start_time);
    kill(currentProcess->pid, SIGSTOP);
}

void processTerminationHandler(int signum)
{
    destroyClk(true);
}
void processFinishedHandler(int signum)
{
    finishCurrentProcess();
    doneCount++;

    if (currentAlgorithm == 2)
    {
        if (!isEmpty(&pq)){
            currentProcess = peek(&pq);
            startCurrentProcess();
        }
    }
}
int main(int argc, char *argv[])
{
    initClk();
    initCircularQueue(&queue);
    initDoneQueue(&doneQueue);
    initializeQueue(&pq);
    signal(SIGTERM, processTerminationHandler);
    signal(SIGUSR1, processFinishedHandler);
    int quantum = atoi(argv[2]);
    int count = atoi(argv[3]);
    doneCount = 0;
    bool firstProcess = true;
    currentAlgorithm = atoi(argv[1]);

    int msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT);
    if (msgqid == -1)
    {
        perror("Error getting message queue");
        exit(-1);
    }
    processStartTime = -1;
    while (doneCount < count)
    {
        int currentTime = getClk();
        if (currentAlgorithm == 1 && currentProcess != NULL && processStartTime != -1)
        {
            int elapsedTime = currentTime - processStartTime;
            if (elapsedTime >= quantum && circularQueueSize(&queue) > 1)
            {
                printf("Time quantum expired at time %d (elapsed: %d)\n", currentTime, elapsedTime);
                stopCurrentProcess();
                moveHeadCircular(&queue, &currentProcess);
                processStartTime = currentTime;
                startCurrentProcess();
            }
        }
        struct msgbuff process;
        if (msgrcv(msgqid, &process, sizeof(process) - sizeof(long), 1, IPC_NOWAIT) == -1)
        {
            // No process arrived at this tick
            usleep(100000); // Sleep for 100ms to avoid busy waiting
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
            processStartTime = currentTime;
            startCurrentProcess();
            firstProcess = false;
        }
        if (currentAlgorithm == 1)
        {
            printf("Enqueuing process with id %d to circular queue at time %d\n", pcb->id, currentTime);
            enqueueCircular(&queue, pcb);
        }
        if (currentAlgorithm == 2)
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

// struct msgbuff {
//     long mtype;
//     PCB p;
// };

// PCB currentprocess;
// void runProcess(PCB *p){
//     if (p->state == READY){ // first time to run
//         char remaining_time[10];
//         sprintf(remaining_time, "%d", p->remainingTime);
//         pid_t pid = fork();
//         if (pid == -1) {
//             perror("fork failed");
//             exit(1);
//         }
//         if (pid == 0) {
//             execl("./process.out", "./process.out", remaining_time, NULL);
//             perror("execl failed");
//             exit(1);
//         }
//         else{
//             p->pid = pid;
//             p->state = RUNNING;
//             p->startTime = currentTime;
//         }

//     }
//     else if (p->state == STOPPED){ // resuming process
//             int waiting_time = getClk() - p->stopTime;
//             p->waitingTime += waiting_time;
//             kill(p->pid, SIGCONT);
//             p->state = RUNNING;
//             fprintf(logFile, "At time %d process %d resumed arr %d total %d remain %d wait %d\n", getClk(), p->id, p->arrivalTime, p->runningTime, p->remainingTime, p->waitingTime);
//         }
// }
// void ProcessFinished(int sig) {
//     int TA = getClk() - currentprocess.arrivalTime;
//     float WTA = (float) TA / currentprocess.runningTime;
//     fprintf(logFile, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n", getClk(), currentprocess.id, currentprocess.arrivalTime, currentprocess.runningTime, currentprocess.waitingTime, TA, WTA);
//     currentprocess.pid = -1;
// }

// FILE* logFile;

// int main(int argc, char * argv[])
// {
//     initClk();

//     //TODO implement the scheduler :)
//     //upon termination release the clock resources.
//     logFile = fopen("scheduler.log", "w");
//     fprintf(logFile, "#At time x process y state arr w total z remain y wait k\n");
//     signal (SIGUSR1, ProcessFinished);
//     key_t key = ftok("keyfile", 65);
//     int msgid = msgget(key, 0666 | IPC_CREAT);
//     if (msgid == -1) {
//         perror("msgget failed");
//         exit(1);
//     }
//     struct msgbuff message;
//     int rec_val;
//     PriQueue pq;
//     pq.size = 0;
//     pid_t pid;
//     currentprocess.pid = -1;
//     int currentTime = -1; // to enter the first tick
//     int overhead = 0;
//     while (1) {
//         if (getClk() == currentTime) continue;  // wait for next tick
//             currentTime = getClk();
//         if (overhead) {
//             overhead = 0;
//             continue; // context switch overhead
//         }
//         while ((rec_val = msgrcv(msgid, &message, sizeof(PCB), 0, IPC_NOWAIT)) != -1) {
//             message.p.state = READY;
//             message.p.waitingTime = 0;
//             message.p.remainingTime = message.p.runningTime;
//             insert(&pq, message.p);
//         }

//         if (currentprocess.pid != -1) {
//             currentprocess.remainingTime--;
//         }
//         if (!isEmpty(&pq)) {
//             PCB next = peek(&pq);
//             if (currentprocess.pid == -1) {
//                 currentprocess = removetop(&pq);
//                 runProcess(&currentprocess);
//             }
//             else if (next.priority < currentprocess.priority) {
//                 kill(currentprocess.pid, SIGSTOP);
//                 currentprocess.state = STOPPED;
//                 currentprocess.stopTime = getClk();
//                 fprintf(logFile, "At time %d process %d stopped arr %d total %d remain %d wait %d\n", getClk(), currentprocess.id, currentprocess.arrivalTime, currentprocess.runningTime, currentprocess.remainingTime, currentprocess.waitingTime);
//                 insert(&pq, currentprocess);
//                 overhead = 1;
//                 currentprocess = removetop(&pq);
//                 runProcess(&currentprocess);
//             }
//         }
//     }
//     fclose(logFile);
//     msgctl(msgid, IPC_RMID, NULL);
//     destroyClk(true);
// }

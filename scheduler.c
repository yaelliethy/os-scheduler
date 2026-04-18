#include "headers.h"
#include <signal.h>
CircularQueue queue;
struct PCB currentProcess;
int currentAlgorithm; //1 for RR, 2 for HPF
int algoPid;
void startProcess(struct PCB *pcb)
{
    // wait 1s (context switch time)
    sleep(1);
    (*pcb).status = RUNNING;
    (*pcb).start_time = getClk();
    kill((*pcb).pid, SIGCONT);
}
void stopProcess(struct PCB *pcb)
{
    kill((*pcb).pid, SIGSTOP);
    (*pcb).status = WAITING;
    (*pcb).remaining_time -= getClk() - (*pcb).start_time;
    if ((*pcb).remaining_time <= 0)
    {
        kill((*pcb).pid, SIGTERM);
        if (currentAlgorithm == 1)
            removeProcessCircular(&queue, (*pcb).id);
    }
}
void RR(int quantum)
{
    while (1)
    {
        if (isCircularQueueEmpty(&queue))
            continue;
        int currentTime = getClk();
        if (currentTime % quantum == 0)
        {
            stopProcess(&currentProcess);
            dequeueCircular(&queue, &currentProcess);
            startProcess(&currentProcess);
            enqueueCircular(&queue, currentProcess);
        }
        usleep(100);
    }
}
//Handler for process termination signal
void processTerminationHandler(int signum)
{
    kill(algoPid, SIGTERM);
    destroyClk(true);
}   
int main(int argc, char *argv[])
{
    initClk();
    initCircularQueue(&queue);
    signal(SIGTERM, processTerminationHandler);
    int algo = atoi(argv[1]);
    int quantum = atoi(argv[2]);
    // Get process from message queue
    int msgqid = msgget(QUEUE_KEY, 0666);
    if (msgqid == -1)
    {
        perror("Error getting message queue");
        exit(-1);
    }
    algoPid = fork();
    if (algoPid == 0){
        if (algo == 1)
            RR(quantum);
        else if (algo == 2)
            RR(quantum);
    }
    while (1)
    {
        struct msgbuff process;
        if (msgrcv(msgqid, &process, sizeof(process), 1, 0) == -1)
        {
            perror("Error receiving message");
            exit(-1);
        }
        if (process.mtype != 1)
        {
            removeProcessCircular(&queue, process.id);
            continue;
        };
        int pid = fork();
        if (pid == 0)
        {
            execl("./process.out", "process.out", process.id, NULL);
        }
        else
        {
            // Immediately stop the process and make its PCB
            kill(pid, SIGSTOP);
            struct PCB pcb;
            pcb.id = process.id;
            pcb.pid = pid;
            pcb.begin_time = getClk();
            pcb.start_time = -1;
            pcb.end_time = -1;
            pcb.arrival = process.arrival;
            pcb.runtime = process.runtime;
            pcb.priority = process.priority;
            pcb.remaining_time = process.runtime;
            // Add to Circular Queue
            if(currentAlgorithm == 1)
                enqueueCircular(&queue, pcb);
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
//             p->startTime = getClk();
//             fprintf(logFile, "At time %d process %d started arr %d total %d remain %d wait %d\n", getClk(), p->id, p->arrivalTime, p->runningTime, p->remainingTime, p->waitingTime); 
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

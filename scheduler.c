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

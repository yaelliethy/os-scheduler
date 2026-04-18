#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300
#define QUEUE_KEY 1234


///==============================
//don't mess with this variable//
int * shmaddr;                 //
//===============================



int getClk()
{
    return *shmaddr;
}


/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}
enum STATUS {
    RUNNING,
    WAITING
};
struct msgbuff {
    long mtype;
    int id;
    int arrival;
    int runtime;
    int priority;
};

//Circular Queue Implementation for the Round Robin Scheduler, with custom fucntion to remove the process from the queue
struct PCB {
    int id;
    int pid;
    int arrival;
    int begin_time;
    int start_time;
    int end_time;
    int runtime;
    int priority;
    int remaining_time;
    enum STATUS status;
};
typedef struct NodeCircular {
    struct PCB process;
    struct NodeCircular* next;
} NodeCircular;
typedef struct CircularQueue {
    NodeCircular* front;
    NodeCircular* rear;
    int size;
} CircularQueue;

void initCircularQueue(CircularQueue* q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

void enqueueCircular(CircularQueue* q, struct PCB process) {
    NodeCircular* newNode = (NodeCircular*)malloc(sizeof(NodeCircular));
    newNode->process = process;
    newNode->next = NULL;
    if (q->rear == NULL) {
        q->front = newNode;
        q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->size++;
}
void dequeueCircular(CircularQueue* q, struct PCB* pcb) {
    if (q->front == NULL) return;
    NodeCircular* temp = q->front;
    q->front = q->front->next;
    if (q->front == NULL) q->rear = NULL;
    free(temp);
    q->size--;
    *pcb = temp->process;
}
bool removeProcessCircular(CircularQueue* q, int id) {
    NodeCircular* temp = q->front;
    while (temp != NULL) {
        if (temp->process.id == id) {
            NodeCircular* temp2 = temp;
            temp = temp->next;
            free(temp2);
            q->size--;
            return 1;
        }
        temp = temp->next;
    }
    return 0;
}
bool isCircularQueueEmpty(CircularQueue* q) {
    return q->size == 0;
}

// typedef enum {
//     READY = 0,
//     RUNNING = 1,
//     STOPPED = 2
// } ProcessState;

// typedef struct {
//     int id;
//     int arrivalTime;
//     int runningTime;
//     int remainingTime;
//     int priority;
//     int waitingTime;
//     int startTime;
//     int stopTime;
//     int state;
//     pid_t pid;
// } PCB;

// typedef struct {
//     PCB processes[MAX_PROCESSES];
//     int size;
// } PriQueue;

// void initializeQueue(PriQueue * pq, int capacity) {
//      pq->size = 0;
// }

// void insert(PriQueue * pq, PCB process) {
//     pq->processes[pq->size] = process;
//     pq->size++;
//     int i = pq->size - 1;
//     while (i > 0) {
//         int parent = (i-1)/2;
//         if (pq->processes[parent].priority > pq->processes[i].priority){
//             PCB temp = pq->processes[parent];
//             pq->processes[parent] = pq->processes[i];
//             pq->processes[i] = temp;
//             i = parent;
//         }
//         else {
//             break;
//         }

//     }
// }

// PCB removetop(PriQueue * pq) {
//     if (pq->size == 0) {
//         PCB empty = {0};
//         return empty;
//     }
//     PCB top = pq->processes[0];
//     pq->processes[0] = pq->processes[pq->size - 1];
//     pq->size--;
//     int i = 0;
//     while (1) {
//         int left = 2*i + 1;
//         int right = 2*i + 2;
//         int smallest;
//         if (left < pq->size && right < pq->size){
//             if (pq->processes[left].priority < pq->processes[right].priority){
//                 smallest = left;
//             }
//             else {
//                 smallest = right;
//             }
//         }
//         else if (left < pq->size) {
//             smallest = left;
//         }
//         else if (right < pq->size) {
//             smallest = right;
//         }
//         if (pq->processes[i].priority > pq->processes[smallest].priority){
//             PCB temp = pq->processes[i];
//             pq->processes[i] = pq->processes[smallest];
//             pq->processes[smallest] = temp;
//             i = smallest;
//         }
//         else {
//             break;
//         }
//     }
//     return top;
// }

// PCB peek(PriQueue* pq){
//     if (pq->size == 0) {
//         PCB empty = {0};
//         return empty;
//     }
//     return pq->processes[0];
// }
//  int isEmpty(PriQueue* pq){
//     return pq->size == 0;
// }
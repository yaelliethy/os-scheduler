#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>
#include "page_table.h"
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

#define MAX_REQUESTS 256   
#define PAGE_SIZE    16    

typedef struct {
    int  cpu_time;         //cpu time when this fires 
    int  virtual_address;  
    char va_str[16];
    char rw;               // r for read w for write
} MemRequest;

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300
#define QUEUE_KEY 1234
#define QUEUE_KEY2 5678
#define SHQUEUE1 91011
#define SHQUEUE2 10112
#define SHDONE 13141
static int * shmaddr;

static inline int getClk()
{
    return *shmaddr;
}

static inline void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}

static inline void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}

enum STATUS {
    READY,
    RUNNING,
    WAITING
};

struct msgbuff {
    long mtype;
    int  id;
    int  arrival;
    int  runtime;
    int  priority;
    int  base;             //starting page number on disk           
    int  limit;            // number of virtual pages for this proc  
};

typedef struct {
    int id;
    int pid;
    int arrival;
    int begin_time;
    int start_time;
    int end_time;
    int runtime;
    int priority;
    int remaining_time;
    int page_table_frame_i;
    enum STATUS status;

    int  base;                      
    int  limit;                    
    PageTable *page_table;   // Member 3 will populate     
    int  page_table_frame;
    int  cpu_time_used;
    MemRequest requests[MAX_REQUESTS];
    int  req_count;
    int  next_req;
} PCB;

typedef struct NodeCircular {
    PCB* process;
    struct NodeCircular* next;
} NodeCircular;

typedef struct CircularQueue {
    NodeCircular* front;
    NodeCircular* rear;
    int size;
} CircularQueue;

static inline void initCircularQueue(CircularQueue* q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

static inline void enqueueCircular(CircularQueue* q, PCB* process) {
    NodeCircular* newNode = (NodeCircular*)malloc(sizeof(NodeCircular));
    newNode->process = process;
    
    if (q->rear == NULL) {
        newNode->next = newNode;
        q->front = newNode;
        q->rear = newNode;
    } else {
        newNode->next = q->front;
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->size++;
}

static inline void dequeueCircular(CircularQueue* q, PCB** pcb) {
    if (q->front == NULL) return;
    NodeCircular* temp = q->front;
    *pcb = temp->process;
    
    if (q->front == q->rear) {
        q->front = NULL;
        q->rear = NULL;
    } else {
        q->front = q->front->next;
        q->rear->next = q->front;
    }
    free(temp);
    q->size--;
}

static inline void moveHeadCircular(CircularQueue* q, PCB** newHead) {
    if (q->front == NULL || q->size <= 1) return;
    
    // Rotate: move front to rear
    q->front = q->front->next;
    q->rear = q->rear->next;
    
    *newHead = q->front->process;
}

static inline bool isCircularQueueEmpty(CircularQueue* q) {
    return q->size == 0;
}
static inline int circularQueueSize(CircularQueue* q) {
    return q->size;
}
static inline bool removeCircular(CircularQueue* q, PCB* process) {
    if (q->front == NULL) return false;
    NodeCircular* current = q->front;
    NodeCircular* prev = q->rear;
    for (int i = 0; i < q->size; i++) {
        if (current->process == process) {
            if (current == q->front && current == q->rear) {
                q->front = NULL;
                q->rear = NULL;
            } else {
                if (current == q->front) q->front = current->next;
                if (current == q->rear) q->rear = prev;
                prev->next = current->next;
            }
            free(current);
            q->size--;
            return true;
        }
        prev = current;
        current = current->next;
    }
    return false;
}
typedef struct PriNode {
    PCB* process;
    struct PriNode* next;
} PriNode;

typedef struct {
    PriNode* head;
    int size;
} PriQueue;

static inline void initializeQueue(PriQueue* pq) {
    pq->head = NULL;
    pq->size = 0;
}

static inline void insert(PriQueue* pq, PCB* process) {
    PriNode* newNode = (PriNode*)malloc(sizeof(PriNode));
    newNode->process = process;
    newNode->next = NULL;

    if (pq->head == NULL || process->priority < pq->head->process->priority) {
        newNode->next = pq->head;
        pq->head = newNode;
        pq->size++;
        return;
    }

    if (process->priority == pq->head->process->priority &&
        process->arrival < pq->head->process->arrival) {
        newNode->next = pq->head;
        pq->head = newNode;
        pq->size++;
        return;
    }

    PriNode* current = pq->head;
    while (current->next != NULL) {
        int nextPriority = current->next->process->priority;
        int nextArrival = current->next->process->arrival;

        if (process->priority < nextPriority) {
            break;
        }
        if (process->priority == nextPriority && process->arrival < nextArrival) {
            break;
        }
        current = current->next;
    }
    newNode->next = current->next;
    current->next = newNode;
    pq->size++;
}

static inline PCB* removetop(PriQueue* pq) {
    if (pq->head == NULL) return NULL;
    PriNode* temp = pq->head;
    PCB* process = temp->process;
    pq->head = pq->head->next;
    free(temp);
    pq->size--;
    return process;
}

static inline PCB* peek(PriQueue* pq) {
    if (pq->head == NULL) return NULL;
    return pq->head->process;
}

static inline int isEmpty(PriQueue* pq) {
    return pq->head == NULL;
}

typedef struct DoneNode {
    PCB* process;
    struct DoneNode* next;
} DoneNode;

typedef struct {
    DoneNode* head;
    DoneNode* tail;
    int size;
} DoneQueue;

static inline void initDoneQueue(DoneQueue* dq) {
    dq->head = NULL;
    dq->tail = NULL;
    dq->size = 0;
}

static inline void enqueueDone(DoneQueue* dq, PCB* process) {
    DoneNode* newNode = (DoneNode*)malloc(sizeof(DoneNode));
    newNode->process = process;
    newNode->next = NULL;
    if (dq->tail == NULL) {
        dq->head = newNode;
        dq->tail = newNode;
    } else {
        dq->tail->next = newNode;
        dq->tail = newNode;
    }
    dq->size++;
}
static inline void dequeueDone(DoneQueue* dq, PCB** pcb) {
    if (dq->head == NULL) return;
    DoneNode* temp = dq->head;
    *pcb = temp->process;
    dq->head = dq->head->next;
    if (dq->head == NULL) {
        dq->tail = NULL;
    }
    free(temp);
    dq->size--;
}
typedef struct NodeDeque {
    PCB* process;
    struct NodeDeque* next;
    struct NodeDeque* prev;
} NodeDeque;

typedef struct Deque {
    NodeDeque* front;
    NodeDeque* rear;
    int size;
} Deque;

static inline void initDeque(Deque* q) {
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}

static inline void pushFront(Deque* q, PCB* process) {
    NodeDeque* newNode = (NodeDeque*)malloc(sizeof(NodeDeque));
    newNode->process = process;

    if (q->front == NULL) {
        newNode->next = NULL;
        newNode->prev = NULL;
        q->front = newNode;
        q->rear = newNode;
    } else {
        newNode->next = q->front;
        newNode->prev = NULL;
        q->front->prev = newNode;
        q->front = newNode;
    }

    q->size++;
}

static inline void pushRear(Deque* q, PCB* process) {
    NodeDeque* newNode = (NodeDeque*)malloc(sizeof(NodeDeque));
    newNode->process = process;

    if (q->rear == NULL) {
        newNode->next = NULL;
        newNode->prev = NULL;
        q->front = newNode;
        q->rear = newNode;
    } else {
        newNode->prev = q->rear;
        newNode->next = NULL;
        q->rear->next = newNode;
        q->rear = newNode;
    }

    q->size++;
}

static inline void popFront(Deque* q, PCB** pcb) {
    if (q->front == NULL) return;

    NodeDeque* temp = q->front;
    *pcb = temp->process;

    if (q->front == q->rear) {
        q->front = NULL;
        q->rear = NULL;
    } else {
        q->front = q->front->next;
        q->front->prev = NULL;
    }

    free(temp);
    q->size--;
}

static inline void popRear(Deque* q, PCB** pcb) {
    if (q->rear == NULL) return;

    NodeDeque* temp = q->rear;
    *pcb = temp->process;

    if (q->front == q->rear) {
        q->front = NULL;
        q->rear = NULL;
    } else {
        q->rear = q->rear->prev;
        q->rear->next = NULL;
    }

    free(temp);
    q->size--;
}
static inline PCB* peekFront(Deque* q) {
    if (q->front == NULL) return NULL;
    return q->front->process;
}
static inline PCB* peekRear(Deque* q) {
    if (q->rear == NULL) return NULL;
    return q->rear->process;
}
static inline bool isDequeEmpty(Deque* q) {
    return q->size == 0;
}

static inline int dequeSize(Deque* q) {
    return q->size;
}
#endif

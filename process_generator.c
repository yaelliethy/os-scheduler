#include "headers.h"
void clearResources(int);
int msgqid = -1; // not initialized
int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.
    FILE *file = fopen("processes.txt", "r");
    if (file == NULL)
    {
        perror("Error opening processes.txt");
        exit(-1);
    }

    // Array to store process data temporarily
    struct msgbuff processes[1000];
    int count = 0;
    char line[256];

    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '#')
            continue;
        if (sscanf(line, "%d\t%d\t%d\t%d",
                   &processes[count].id,
                   &processes[count].arrival,
                   &processes[count].runtime,
                   &processes[count].priority) == 4)
        {
            processes[count].mtype = 1; // message type for MQ
            count++;
        }
    }
    fclose(file);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int algo, quantum, N, M = 0;
    while (1)
    {
        printf("Select algorithm [1:RR,2:HPF,3:FCFS]: ");

        if (scanf("%d", &algo) != 1)
        {
            printf("Invalid.Enter a number\n");
            while (getchar() != '\n')
                ;
            continue;
        }

        if (algo >= 1 && algo <= 3)
            break;

        printf("Invalid.Enter 1/2/3\n");
    }
    if (algo == 1)
    {
        while (1)
        {
            printf("Enter quantum: ");

            if (scanf("%d", &quantum) != 1)
            {
                printf("Invalid.Enter a number\n");
                while (getchar() != '\n')
                    ;
                continue;
            }

            if (quantum > 0)
                break;

            printf("Quantum must be >0\n");
        }
    }
    if (algo == 3)
    {
        while (1)
        {
            printf("Enter N: ");
            scanf("%d", &N);
            if (N > 0)
                break;
            printf("N must be >0\n");
        }
        while (1)
        {
            printf("Enter M: ");
            scanf("%d", &M);
            if (M > 0)
                break;
            printf("M must be >0\n");
        }
    }

    // 3. Initiate and create the scheduler and clock processes.
    // Start Clock
    int clk_pid = fork();
    if (clk_pid == 0)
    {
        execl("./clk.out", "clk.out", NULL);
        perror("Error starting clk.out");
        exit(-1);
    }
    int cpu_number = 1; // default to 1 CPU
    // Start Scheduler
    int sched_pid = fork();
    if (sched_pid == 0)
    {
        if(algo == 3) {
            int pid = fork();
            if (pid == 0)
                cpu_number = 2;
        }
        else
            cpu_number = 0;
        char algo_str[10], param_str[10], count_str[10], cpu_str[10];
        sprintf(algo_str, "%d", algo);
        sprintf(param_str, "%d", quantum);
        sprintf(count_str, "%d", count);
        sprintf(cpu_str, "%d", cpu_number);
        // Passing algorithm, quantum, and total process count as arguments
        execl("./scheduler.out", "scheduler.out", algo_str, param_str, count_str, cpu_str, NULL);
        perror("Error starting scheduler.out");
        exit(-1);
    }
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop
    int msgqid, msgqid2;
    if(algo == 3){
        msgqid2 = msgget(QUEUE_KEY2, 0666 | IPC_CREAT);
    }
    msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT);
    Deque *deque1;
    Deque *deque2;
    if(algo == 3){
        int shqueue = shmget(SHQUEUE1, sizeof(Deque), 0666 | IPC_CREAT);
        int shqueue2 = shmget(SHQUEUE2, sizeof(Deque), 0666 | IPC_CREAT);
        deque1 = (Deque*)shmat(shqueue, (void *)0, 0);
        deque2 = (Deque*)shmat(shqueue2, (void *)0, 0);
    }
    if (msgqid == -1)
    {
        perror("Error creating message queue");
        exit(-1);
    }
    int i = 0;
    int prvTime = -1;
    while (i < count)
    {
        int currentTime = getClk();
        if (currentTime == prvTime) // wait before checking again!!
        {
            usleep(500000); // sleep 0.5 sec!
            continue;
        }
        if(algo == 3 && currentTime % N == 0){
            //If difference between queues is greater than M, move process from larger queue to smaller queue
            int size1 = deque1->size;
            int size2 = deque2->size;
            if(size1 - size2 > M || size2 - size1 > M){
                struct msgbuff msg;
                msg.mtype = 2;
                if(size1 > size2){
                    msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
                }
                else{
                    msgsnd(msgqid2, &msg, sizeof(msg) - sizeof(long), IPC_NOWAIT);
                }
            }
        }
        prvTime = currentTime;

        // Check if any processes have "arrived" based on current clock time
        while (i < count && processes[i].arrival <= currentTime)
        {
            // Send process to scheduler via message queue
            if (algo == 3){
                //Send to the shorter queue
                if(deque1->size <= deque2->size){
                    if (msgsnd(msgqid, &processes[i], sizeof(processes[i]) - sizeof(long), IPC_NOWAIT) == -1)
                    {
                        perror("Error sending message to CPU 1");
                        break;
                    }
                }
                else{
                    if (msgsnd(msgqid2, &processes[i], sizeof(processes[i]) - sizeof(long), IPC_NOWAIT) == -1)
                    {
                        perror("Error sending message to CPU 2");
                        break;
                    }
                }
            }
            else if (msgsnd(msgqid, &processes[i], sizeof(processes[i]) - sizeof(long), IPC_NOWAIT) == -1)
            {
                perror("Error sending message");
                break;
            }
            printf("Generator: Sent process %d at time %d\n", processes[i].id, currentTime);
            i++;
        }
    }

    // Wait for the scheduler to finish its work before exiting
    int status;
    waitpid(sched_pid, &status, 0);
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    if (msgqid != -1)
        msgctl(msgqid, IPC_RMID, (struct msqid_ds *)NULL);
    printf("\nResources cleared\n");
    destroyClk(true);
    exit(0);
}

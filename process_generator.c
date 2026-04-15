#include "headers.h"
#define QUEUE_KEY 1234
void clearResources(int);
int msgqid;
int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources);
    // TODO Initialization
    // 1. Read the input files.
    FILE *file = fopen("processes.txt", "r");
    if (file == NULL) {
        perror("Error opening processes.txt");
        exit(-1);
    }

    // Array to store process data temporarily
    struct msgbuff processes[1000]; 
    int count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#') continue; 
        if (sscanf(line, "%d\t%d\t%d\t%d", 
                   &processes[count].id, 
                   &processes[count].arrival, 
                   &processes[count].runtime, 
                   &processes[count].priority) == 4) {
            processes[count].mtype = 1; //message type for MQ
            count++;
        }
    }
    fclose(file);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    int algo, quantum = 0;
    printf("Choose Scheduling Algorithm:\n1. HPF\n2. RR\n3. FCFS\nSelection: ");
    scanf("%d", &algo);
    if (algo == 2) {
        printf("Enter Time Quantum: ");
        scanf("%d", &quantum);
    }
    // 3. Initiate and create the scheduler and clock processes.
    // Start Clock
    int clk_pid = fork();
    if (clk_pid == 0) {
        execl("./clk.out", "clk.out", NULL);
    }

    // Start Scheduler
    int sched_pid = fork();
    if (sched_pid == 0) {
        char algo_str[10], param_str[10], count_str[10];
        sprintf(algo_str, "%d", algo);
        sprintf(param_str, "%d", quantum);
        sprintf(count_str, "%d", count);
        // Passing algorithm, quantum, and total process count as arguments
        execl("./scheduler.out", "scheduler.out", algo_str, param_str, count_str, NULL);
    }
    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    printf("current time is %d\n", x);
    // TODO Generation Main Loop

    msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT); //setup message queue
    if (msgqid == -1) {
        perror("Error creating message queue");
        exit(-1);
    }
    int i = 0;
    while (i < count) {
        int currentTime = getClk(); //

        // Check if any processes have "arrived" based on current clock time
        while (i < count && processes[i].arrival <= currentTime) {
            // Send process to scheduler via message queue
            if (msgsnd(msgqid, &processes[i], sizeof(processes[i]) - sizeof(long), !IPC_NOWAIT) == -1) {
                perror("Error sending message");
            }
            printf("Generator: Sent process %d at time %d\n", processes[i].id, currentTime);
            i++;
        }
        // Small pause to prevent busy wait
        usleep(1000); 
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
    msgctl(msgqid, IPC_RMID, (struct msqid_ds *)NULL);
    printf("\nResources cleared\n");
    destroyClk(true); //
    exit(0);
}

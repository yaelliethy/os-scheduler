#include "headers.h"

/* Modify this file as needed*/
int remainingtime;
int id;
int main(int agrc, char * argv[])
{
    initClk();
    id = atoi(argv[1]);
    
    //TODO it needs to get the remaining time from somewhere
    //remainingtime = ??;
    while (remainingtime > 0)
    {
        // remainingtime = ??;
    }
    
    destroyClk(false);
    //Add to message queue that it finished
    struct msgbuff process;
    process.mtype = 2;
    process.id = id;
    if (msgsnd(QUEUE_KEY, &process, sizeof(process), !IPC_NOWAIT) == -1) {
        perror("Error sending message");
    }
    exit(0); //exit process
    return 0;
}

// int remainingtime;

// int main(int agrc, char * argv[])
// {
//     initClk();
    
//     //TODO it needs to get the remaining time from somewhere
//     //remainingtime = ??;
//     remainingtime = atoi(argv[1]);
//     int currentTime = getclk(); // to skip the first tick
//     while (remainingtime > 0)
//     {
//         if (getClk() == currentTime) continue;  // wait for next tick
//             currentTime = getClk();
//         remainingtime--; 
//     }
//     kill(getppid(), SIGUSR1); // signal the scheduler that this process has finished
    
//     destroyClk(false);
    
//     return 0;
// }


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

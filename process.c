#include "headers.h"

/* Modify this file as needed*/
int remainingtime;
int id;
int main(int agrc, char *argv[])
{
    initClk();
    id = atoi(argv[1]);

    // TODO it needs to get the remaining time from somewhere
    // remainingtime = ??;
    remainingtime = atoi(argv[2]);
    printf("Process %d started with remaining time %d at time %d\n", id, remainingtime, getClk());
    int currentTime = getClk(); // to skip the first tick
    while (remainingtime > 0)
    {
        if (getClk() == currentTime)
            continue; // wait for next tick
        currentTime = getClk();
        remainingtime--;
        usleep(100000); // sleep for 100ms to avoid busy waiting
    }
    // Add to message queue that it finished
    struct msgbuff process;
    process.mtype = 2;
    process.id = id;
    kill(getppid(), SIGUSR1);
    destroyClk(false);
    return 0;
}
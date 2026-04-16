#include "headers.h"
#define QUANTA 10
int msgqid;
struct msgbuff current_process = {-1};
void timer_handler(int signum)
{
    //Send a SIGSTOP to the currently running process
    if (current_process.id != -1) {
        kill(current_process.id, SIGSTOP);
        //Reschedule the process if needed
        int run_time = (current_process.runtime < QUANTA) ? current_process.runtime : QUANTA;
        current_process.runtime -= run_time;
        if (current_process.runtime > 0) {
            // Re-queue the process with updated remaining time
            if (msgsnd(msgqid, &current_process, sizeof(current_process) - sizeof(long), !IPC_NOWAIT) == -1) {
                perror("Error sending message");
            }
        } else {
            // Process finished, we can log it or perform any necessary cleanup
            printf("Process %d finished at time %d\n", current_process.id, getClk());
            current_process.id = -1; // Update the currently running process ID
        }
        schedule_process(); // Schedule the next process
    }
}
void schedule_process()
{
    struct msgbuff msg;
    if (msgrcv(msgqid, &msg, sizeof(msg) - sizeof(long), 0, 0) == -1) {
        perror("Error receiving message");
        destroyClk(false);
        return;
    }
    //We received a process, now we need to run it for QUANTA time or until it finishes
    int run_time = (msg.runtime < QUANTA) ? msg.runtime : QUANTA;
    alarm(run_time); // Set an alarm for the time quantum
    kill(msg.id, SIGCONT);
    current_process = msg; // Update the currently running process
}
int main(int argc, char * argv[])
{
    initClk();
    signal(SIGALRM, timer_handler);
    msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT);
    schedule_process();
    return 0;
}
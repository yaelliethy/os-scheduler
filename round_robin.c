#include "headers.h"
#define QUANTA 10
int msgqid;
int current_process_id = -1;
void timer_handler(int signum)
{
    //Send a SIGSTOP to the currently running process
    if (current_process_id != -1) {
        kill(current_process_id, SIGSTOP);
    }
}
int main(int argc, char * argv[])
{
    initClk();
    signal(SIGALRM, timer_handler);
    //Read from message queue to get processes and their remaining times
    msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT);
    struct msgbuff msg;
    while (1) {
        if (msgrcv(msgqid, &msg, sizeof(msg) - sizeof(long), 0, 0) == -1) {
            perror("Error receiving message");
            break;
        }
        //We received a process, now we need to run it for QUANTA time or until it finishes
        int run_time = (msg.runtime < QUANTA) ? msg.runtime : QUANTA;
        alarm(run_time); // Set an alarm for the time quantum
        // Send a SIGCONT to the process to start it
        kill(msg.id, SIGCONT);
        pause(); // Wait for the timer to expire or the process to finish
        // After the timer expires, we need to check if the process finished or if it needs to be re-queued with remaining time
        msg.runtime -= run_time;
        if (msg.runtime > 0) {
            // Re-queue the process with updated remaining time
            if (msgsnd(msgqid, &msg, sizeof(msg) - sizeof(long), !IPC_NOWAIT) == -1) {
                perror("Error sending message");
            }
            current_process_id = msg.id; // Update the currently running process ID
        } else {
            // Process finished, we can log it or perform any necessary cleanup
            printf("Process %d finished at time %d\n", msg.id, getClk());
            current_process_id = -1; // Update the currently running process ID
        }
    }
    destroyClk(false);
    
    return 0;
}
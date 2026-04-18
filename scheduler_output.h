#ifndef SCHEDULER_OUTPUT_H
#define SCHEDULER_OUTPUT_H
// Helper functions to write scheduler output files:
// scheduler.log and scheduler.perf
#include <stdio.h>
#include <math.h>
static FILE *scheduler_log_file = NULL; // keep scheduler.log open during execution
static inline FILE *open_scheduler_log(void)// Open scheduler.log once and reuse it
{
    if (scheduler_log_file == NULL)
    {
        scheduler_log_file = fopen("scheduler.log", "w");
        if (scheduler_log_file != NULL)
        {
            fprintf(scheduler_log_file,"#At time x process y state arr w total z remain y wait k\n");
            fflush(scheduler_log_file); //writes immediate
        }
    }
    return scheduler_log_file;
}

static inline void close_scheduler_log(void)
{
    if (scheduler_log_file != NULL)
    {
        fclose(scheduler_log_file);
        scheduler_log_file = NULL;
    }
}

static inline void log_started(int time, int pid, int arrival, int runtime, int remain, int wait)
{
    FILE *f = open_scheduler_log();
    if (f == NULL) return;
    fprintf(f,"At time %d process %d started arr %d total %d remain %d wait %d\n",
                time, pid, arrival, runtime, remain, wait);
        fflush(f);
}

static inline void log_stopped(int time, int pid, int arrival, int runtime, int remain, int wait)
{
    FILE *f = open_scheduler_log();
    if (f == NULL) return;
    fprintf(f,"At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                time, pid, arrival, runtime, remain, wait);
    fflush(f);
}

static inline void log_resumed(int time, int pid, int arrival, int runtime, int remain, int wait)
{
    FILE *f = open_scheduler_log();
    if (f == NULL) return;
    fprintf(f,"At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                time, pid, arrival, runtime, remain, wait);
    fflush(f);
}

static inline void log_finished(int time, int pid, int arrival, int runtime, int wait, int ta, float wta)
{
    FILE *f = open_scheduler_log();
    if (f == NULL) return;
    fprintf(f,
        "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
         time, pid, arrival, runtime, wait, ta, wta);
    fflush(f);
}
static inline void write_scheduler_perf(float CPU_utilization,float avg_WTA,float avg_Waiting,
                                            const float *WTA_values,int n)
{
    FILE *f = fopen("scheduler.perf", "w");
    if (f == NULL) return;
    float std_WTA = 0.0f;
    if (WTA_values != NULL && n > 0)
    {
        for (int i = 0; i < n; i++)
        {
            float diff = WTA_values[i] - avg_WTA;
            std_WTA += diff * diff;
        }
        std_WTA = sqrtf(std_WTA / n);
    }
    fprintf(f, "CPU utilization = %.2f%%\n", CPU_utilization);
    fprintf(f, "Avg WTA = %.2f\n", avg_WTA);
    fprintf(f, "Avg Waiting = %.2f\n", avg_Waiting);
    fprintf(f, "Std WTA = %.2f\n", std_WTA);
    fclose(f);
}            
#endif                                
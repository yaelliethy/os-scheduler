#include "headers.h"
#include <string.h>

void clearResources(int);
static int parse_binary_string(const char *s);
static int load_requests(int id, MemRequest *out);

int msgqid = -1;

static int parse_binary_string(const char *s) {
    int val = 0;
    while (*s) {
        val = (val << 1) | (*s++ - '0');
    }
    return val;
}

static int load_requests(int id, MemRequest *out) {
    char filename[64];
    snprintf(filename, sizeof(filename), "requests_%d.txt", id);

    FILE *f = fopen(filename, "r");
    if (f == NULL) return 0;

    int count = 0;
    char line[256];
    char addr_str[64], rw_char;
    int cpu_time;

    while (fgets(line, sizeof(line), f) && count < MAX_REQUESTS) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%d %63s %c", &cpu_time, addr_str, &rw_char) == 3) {
            out[count].cpu_time = cpu_time;
            out[count].virtual_address = parse_binary_string(addr_str);
            strncpy(out[count].va_str, addr_str, sizeof(out[count].va_str) - 1);
            out[count].va_str[sizeof(out[count].va_str) - 1] = '\0';
            out[count].rw = rw_char;
            count++;
        }
    }
    fclose(f);
    return count;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, clearResources);

    FILE *file = fopen("processes.txt", "r");
    if (file == NULL) exit(-1);

    struct msgbuff processes[1000];
    MemRequest all_requests[1000][MAX_REQUESTS];
    int req_counts[1000];
    int count = 0;
    char line[256];

    while (fgets(line, sizeof(line), file) && count < 1000) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (sscanf(line, "%d\t%d\t%d\t%d\t%d\t%d",
                   &processes[count].id, &processes[count].arrival,
                   &processes[count].runtime, &processes[count].priority,
                   &processes[count].base, &processes[count].limit) == 6) {
            processes[count].mtype = 1;
            req_counts[count] = load_requests(processes[count].id, all_requests[count]);
            count++;
        }
    }
    fclose(file);

    int algo = 1, quantum, k_timeout;
    while (1) {
        printf("Enter quantum: ");
        if (scanf("%d", &quantum) == 1 && quantum > 0) break;
        while (getchar() != '\n');
    }
    while (1) {
        printf("Enter K: ");
        if (scanf("%d", &k_timeout) == 1 && k_timeout > 0) break;
        while (getchar() != '\n');
    }

    int clk_pid = fork();
    if (clk_pid == 0) {
        execl("./clk.out", "clk.out", NULL);
        exit(-1);
    }

    int sched_pid = fork();
    if (sched_pid == 0) {
        char algo_str[10], param_str[10], count_str[10], cpu_str[10], k_str[10];
        sprintf(algo_str, "%d", algo);
        sprintf(param_str, "%d", quantum);
        sprintf(count_str, "%d", count);
        sprintf(cpu_str, "0");
        sprintf(k_str, "%d", k_timeout);
        execl("./scheduler.out", "scheduler.out", algo_str, param_str, count_str, cpu_str, k_str, NULL);
        exit(-1);
    }

    initClk();
    msgqid = msgget(QUEUE_KEY, 0666 | IPC_CREAT);
    if (msgqid == -1) exit(-1);

    int i = 0, last_t = -1;
    while (i < count) {
        int now = getClk();
        if (now == last_t) {
            usleep(100000);
            continue;
        }
        last_t = now;

        while (i < count && processes[i].arrival <= now) {
            if (msgsnd(msgqid, &processes[i], sizeof(processes[i]) - sizeof(long), IPC_NOWAIT) != -1) {
                printf("Generator: sent process %d at time %d\n", processes[i].id, now);
                i++;
            } else break;
        }
    }

    int status;
    waitpid(sched_pid, &status, 0);
    destroyClk(true);
    return 0;
}

void clearResources(int signum) {
    if (msgqid != -1) msgctl(msgqid, IPC_RMID, NULL);
    destroyClk(true);
    exit(0);
}

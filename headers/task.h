#ifndef __TASK_
#define __TASK_
#include <unistd.h>

typedef struct task {
    int type;
    pid_t process_pid;
    union {
        struct single_task {
            char task_name[50];
            long int exec_time;
        } Single;
        struct multi_task {
            char tasks_names[20][50];
            long int exec_times[20];
        } Pipeline;
    } tt;    
} * Task;

#endif
#ifndef __TASK_
#define __TASK_
#include <unistd.h>

typedef struct task {
    char task_name[20];
    pid_t process_pid;
    double exec_time;
} * Task;

#endif
#ifndef __TASK_
#define __TASK_
#include <unistd.h>

#define MAX_TASK_NAME_SIZE 64
#define MAX_NUMBER_OF_TASKS 128

typedef struct task {
    int type;
    pid_t process_pid;
    union {
        struct single_task {
            char task_name[MAX_TASK_NAME_SIZE];
            long int exec_time;
        } Single;
        struct multi_task {
            char tasks_names[MAX_NUMBER_OF_TASKS][MAX_TASK_NAME_SIZE];
            int nr_commands;
            long int exec_time;
        } Pipeline;
        
    } info;    
} * Task;

#endif
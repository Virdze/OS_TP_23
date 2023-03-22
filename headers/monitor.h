#ifndef __MONITOR_
#define __MONITOR_

typedef struct process {
    char * task_name;
    pid_t process_pid;
    float exec_time;
} * Process ;


#endif
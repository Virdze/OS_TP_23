#ifndef __MESSAGE_
#define __MESSAGE_
#include <unistd.h> 

typedef struct message {
    int type;
    union {
        struct execute_start { // 1 - Tracer -> Monitor (Starting Execute Single)
            char task_name[50];
            pid_t process_pid;
            long int start;
        } EStart;

        struct execute_end { // 2 - Tracer -> Monitor (Finish Execute Single)
            char response_path[50];
            pid_t process_pid;
            long int end;
        } EEnd;
        
        struct execute_pipeline_start { // 3 - Tracer -> Monitor (Starting Execute Pipeline)
            char tasks_names[20][50];
            pid_t process_pid;
            int nr_commands;
            long int start;
        } PStart;

        struct execute_pipeline_end { // 4 - Tracer -> Monitor (Finish Execute Pipeline)
            char response_path[50];
            pid_t process_pid;
            long int exec_times;
        } PEnd;

        struct status_request { // 5 - Tracer -> Monitor (Status request)
            char response_path[50];
            long int clock;
        } StatusRequest;

        struct status_response { // 6 - Monitor -> Tracer (Status response)
            char task_name[20];
            pid_t process_pid;
            long int time_elapsed;
        } StatusResponse;

        struct stats_time_request { // 7. Tracer -> Monitor (stats time)
            char response_path[50];
            pid_t request_pids[100];
        } StatsTimeRequest;
    } msg;
} * Message;

#endif
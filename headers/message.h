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
            long int exec_time;
        } PEnd;

        struct status_request { // 5 - Tracer -> Monitor (Status request)
            char response_path[50];
            long int clock;
        } StatusRequest;

        struct status_response_single { // 6 - Monitor -> Tracer (Status response single)
            char task_name[50];
            pid_t process_pid;
            long int time_elapsed;
        } StatusResponseS;

        struct status_response_pipeline { // 7 - Monitor -> Tracer (Status response pipeline)
            char tasks_pipeline[20][50];
            pid_t process_pid;
            int nr_comandos;    
            long int time_elapsed;
        } StatusResponseP;

        struct stats_request { // 8. Tracer -> Monitor (stats time) 
                               // 10- Tracer -> Monitor (stats uniq)
            char response_path[50]; 
            pid_t request_pids[100];
        } StatsRequest;

        struct stats_command_request { // 9. Tracer -> Monitor (stats command)
            char response_path[50];
            char task_name[50];
            pid_t request_pids[100];
        } StatsCommandRequest;

    } data;
} * Message;

#endif
#ifndef __MESSAGE_
#define __MESSAGE_
#include <unistd.h> 

#define MAX_TASK_NAME_SIZE 64
#define MAX_NUMBER_OF_TASKS 128
#define MAX_NUMBER_OF_PIDS 128
#define MAX_RESPONSE_PATH_LENGTH 256

typedef struct message {
    int type;
    union {
        struct execute_start { // 1 - Tracer -> Monitor (Starting Execute Single)
            char task_name[MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            long int start;
        } EStart;

        struct execute_end { // 2 - Tracer -> Monitor (Finish Execute Single)
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            pid_t process_pid;
            long int end;
        } EEnd;
        
        struct execute_pipeline_start { // 3 - Tracer -> Monitor (Starting Execute Pipeline)
            char tasks_names[MAX_NUMBER_OF_TASKS][MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            int nr_commands;
            long int start;
        } PStart;

        struct execute_pipeline_end { // 4 - Tracer -> Monitor (Finish Execute Pipeline)
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            pid_t process_pid;
            long int exec_time;
        } PEnd;

        struct status_request { // 5 - Tracer -> Monitor (status request)
                                // 11 - Tracer -> Monitor (status-all request)
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            long int clock;
        } StatusRequest;

        struct status_response_single { // 6 - Monitor -> Tracer (Status response single)
            char task_name[MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            long int time_elapsed;
        } StatusResponseS;

        struct status_response_pipeline { // 7 - Monitor -> Tracer (Status response pipeline)
            char tasks_pipeline[MAX_NUMBER_OF_TASKS][MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            int nr_comandos;    
            long int time_elapsed;
        } StatusResponseP;

        struct stats_request { // 8. Tracer -> Monitor (stats time) 
                               // 10- Tracer -> Monitor (stats uniq)
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            pid_t request_pids[MAX_NUMBER_OF_PIDS];
            int nr_pids;
        } StatsRequest;

        struct stats_command_request { // 9. Tracer -> Monitor (stats command)
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            char task_name[MAX_TASK_NAME_SIZE];
            pid_t request_pids[MAX_NUMBER_OF_PIDS];
        } StatsCommandRequest;

    } data;
} * Message;

#endif
#ifndef __MESSAGE_
#define __MESSAGE_
#include <unistd.h> 

#define MAX_TASK_NAME_SIZE 64
#define MAX_NUMBER_OF_TASKS 128
#define MAX_NUMBER_OF_PIDS 128
#define MAX_RESPONSE_PATH_LENGTH 256

/*
Types:
    1 - Tracer -> Monitor (Starting Execute Single)
    2 - Tracer -> Monitor (Finish Execute Single)
    3 - Tracer -> Monitor (Starting Execute Pipeline)
    4 - Tracer -> Monitor (Finish Execute Pipeline)
    5 - Tracer -> Monitor (status request)
    6 - Monitor -> Tracer (Status response single)
    7 - Monitor -> Tracer (Status response pipeline)
    8 - Tracer -> Monitor (stats time)
    9 - Tracer -> Monitor (stats command)
    10 - Tracer -> Monitor (stats uniq)
    11 - Tracer -> Monitor (status-all request)
    12/13/14/15 - Tracer -> Monitor (status-all response)
*/

typedef struct message {
    int type;
    union {
        struct execute_start {
            char task_name[MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            long int start;
        } EStart;

        struct execute_end {
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            pid_t process_pid;
            long int end;
        } EEnd;
        
        struct execute_pipeline_start { 
            char tasks_names[MAX_NUMBER_OF_TASKS][MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            int nr_commands;
            long int start;
        } PStart;

        struct execute_pipeline_end {
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            pid_t process_pid;
            long int exec_time;
        } PEnd;

        struct status_request { 
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            long int clock;
        } StatusRequest;

        struct status_response_single {
            char task_name[MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            long int time_elapsed;
        } StatusResponseS;

        struct status_response_pipeline {
            char tasks_pipeline[MAX_NUMBER_OF_TASKS][MAX_TASK_NAME_SIZE];
            pid_t process_pid;
            int nr_comandos;    
            long int time_elapsed;
        } StatusResponseP;

        struct stats_request {
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            pid_t request_pids[MAX_NUMBER_OF_PIDS];
            int nr_pids;
        } StatsRequest;

        struct stats_command_request {
            char response_path[MAX_RESPONSE_PATH_LENGTH];
            char task_name[MAX_TASK_NAME_SIZE];
            pid_t request_pids[MAX_NUMBER_OF_PIDS];
            int nr_pids;
        } StatsCommandRequest;

    } data;
} * Message;

#endif
#ifndef __MESSAGE_
#define __MESSAGE_
#include <unistd.h>


// 1 - Tracer -> Monitor (Starting Execute)
// 2 - Full duplex (Finish Execute)
// 3 - Tracer -> Monitor (Stating request)
// 4 - Monitor -> Tracer (Status response)
// 5 - double time -> Execute Response


typedef struct message {
    int type;
    union {
        struct execute_start {
            char task_name[20];
            pid_t process_pid;
            double start;
        } EStart;

        struct execute_end {
            char response_path[50];
            pid_t process_pid;
            double end;
        } EEnd;
        
        struct status_request{
            /* data */
        } StatusRequest;
    
        struct status_response {
            /* data */
        } StatusResponse;

        double time;
    } msg;

} * Message ;

#endif
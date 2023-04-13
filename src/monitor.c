#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "../headers/message.h"
#include "../headers/task.h"

#define MAIN_FIFO "../tmp/main_fifo"

//  =================================== ** GLOBALS ** ===================================

Task * done;
Task * requests;
int requests_count = 0,
    done_count = 0;
int main_channel_fd;

// =====================================================================================


//  =================================== ** PRINTS ** ===================================

void printTask(Task t){
    printf("%d\n%s\n%fms\n",t->process_pid,t->task_name, t->exec_time);
}
void printDoneList(){
    for(int i = 0; i < done_count; i++)
        printTask(done[i]);
}

void printRequests(){
    for(int i = 0; i < requests_count; i++)
        printTask(requests[i]);
}



// =====================================================================================
/*
void addRequest(Task task){
    if(requests_count == 0){
        requests_count++;
        requests[0] = malloc(sizeof(struct task));
        requests[0]->process_pid = task->process_pid;
        strcpy(requests[0]->task_name,task->task_name);
        requests[0]->exec_time = task->exec_time;
    }
    else{
        requests_count++;
        requests = realloc(requests,sizeof(Task) * (requests_count));
        requests[requests_count] = malloc(sizeof(struct task));
        requests[requests_count]->process_pid = task->process_pid;
        strcpy(requests[requests_count]->task_name,task->task_name);
        requests[requests_count]->exec_time = task->exec_time;
    }
}
*/

void addRequest(Task task){
    requests_count++;
    requests = realloc(requests, requests_count * sizeof(struct task));
    requests[requests_count-1] = task;
}

void finishRequest(Task task){
    int index = -1;
    for(int i = 0; i < requests_count ; i++){
        if(requests[i]->process_pid == task->process_pid){
            index = i;
            break;
        }
    }
    if(index != -1){
        requests_count--;
        for(int i = index; i < requests_count; i++){
            requests[i] = requests[i+1];
        }
        requests = realloc(requests, requests_count * sizeof(struct task));
        done_count++;
        done = realloc(done,sizeof(struct task) * (done_count));
        done[done_count-1] = task;
    }
}

Task createStartTask(Message m){
    Task new_task = malloc(sizeof(struct task));
    new_task->process_pid = m->msg.EStart.process_pid;
    strcpy(new_task->task_name,m->msg.EStart.task_name);
    new_task->exec_time = m->msg.EStart.start;
    return new_task;
}

Task findRequest(pid_t pid){
    Task r = malloc(sizeof(struct task));
    for(int i = 0; i < requests_count;i++){
        if(requests[i]->process_pid == pid)
            r = requests[i];
    }
    return r;
}

void send_exec_time(Message m, Task t){
    int response_fd;
    if((response_fd = open(m->msg.EEnd.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    Message response = malloc(sizeof(struct message));
    response->type = 5;
    response->msg.time = t->exec_time;
    write(response_fd, response, sizeof(response));
    close(response_fd);
}

void status_response(Message m){
    // 1. Abrir comunicação com cliente 
    int response_fd;
    if((response_fd = open(m->msg.StatusRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    for(int i = 0; i < requests_count; i++){
        // 2 . Constroi resposta
        Message response = malloc(sizeof(struct message));
        response->type = 4;
        response->msg.StatusResponse.process_pid = requests[i]->process_pid;
        strcpy(response->msg.StatusResponse.task_name,requests[i]->task_name);
        response->msg.StatusResponse.time_elapsed = m->msg.StatusRequest.clock - requests[i]->exec_time;
        printf("%d\n", response->msg.StatusResponse.process_pid);
        // 3. Write da resposta 
        write(response_fd, response, sizeof(response));
    }


    // 4. Fechar Pipe
    close(response_fd);
}

void monitoring(char * path){
    ssize_t bytes_read;
    Message new_message = malloc(sizeof(struct message));
    pid_t pids[1024];
    int sons = 0;
    int flag = 0;
    while(!flag){
        while((bytes_read = read(main_channel_fd,new_message,sizeof(struct message))) > 0){
            if(new_message->type == 1){
                Task t = createStartTask(new_message);
                addRequest(t);
                printRequests();
            }
            else if(new_message->type == 2){
                pid_t pid;

                Task t = findRequest(new_message->msg.EEnd.process_pid);
                double initial_time = t->exec_time;
                t->exec_time = (new_message->msg.EEnd.end - initial_time) * 1000;
                finishRequest(t);

                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if (!pid){
                    send_exec_time(new_message, t);
                    printf("%d sended message to the user waiting!\n",getpid());
                    _exit(0);
                } else {
                    pids[sons] = pid;
                    sons++;
                    printDoneList();
                }
            }
            else if(new_message->type == 3){
                pid_t pid;

                if((pid = fork()) < 0){
                    perror("Error using fork()!");
                    _exit(-1);
                }
                else if(!pid){
                    status_response(new_message);
                    printf("%d sended message to the user waiting!\n", getpid());
                    _exit(0);
                } else {
                    pids[sons] = pid;
                    sons++;
                }
            }
            else if(new_message->type == -1){
                flag = 1;
                break;
            }
            free(new_message); // Clear message
        }
    }


    // Wait to catch all the sons that were created during the monitor life-span

    for(int i = 0; i < sons; i++){
        int status;
        waitpid(pids[0],&status,0);
    }
}

int initMainChannel(){
    int main_channel_fifo;
    if((main_channel_fifo = mkfifo(MAIN_FIFO, 0666)) < 0){
        perror("Error creating process request fifo!\n");
        return -1;
    }

    if((main_channel_fd = open(MAIN_FIFO, O_RDONLY)) < 0){
        perror("Error opening process request fifo\n");
        return -1;
    }

    return 0;
}

int main(int argc, char * argv[]){
    if(argc < 2){
        printf("Usage:\n");
        printf("1: ./monitor PIDS-folder\n");
    }
    else {
        if (initMainChannel() < 0)
            return -1;
        char * folders_path = strdup(argv[1]);
        // Iniciar monitoring
        monitoring(folders_path);
    }
    close(main_channel_fd);
    unlink(MAIN_FIFO);
    return 0;
} 
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
char * folders_path;

// =====================================================================================


//  =================================== ** PRINTS ** ===================================

void printTask(Task t){
    printf("%d\n%s\n%ldms\n",t->process_pid,t->task_name, t->exec_time);
}
void printDoneList(){
    for(int i = 0; i < done_count; i++)
        printTask(done[i]);
}

void printRequests(){
    for(int i = 0; i < requests_count; i++)
        printTask(requests[i]);
}

void printUsage(){
    printf("Usage:\n");
    printf("1: ./monitor PIDS-folder\n");
}


// =====================================================================================

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
    write(response_fd, response, sizeof(struct message));
    close(response_fd);
}

void status_response(Message m){
    // 1. Abrir comunicação com cliente 
    int response_fd;
    if((response_fd = open(m->msg.StatusRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    Message response = malloc(sizeof(struct message));
    for(int i = 0; i < requests_count; i++){
        // 2 . Constroi resposta
        response->type = 4;
        response->msg.StatusResponse.process_pid = requests[i]->process_pid;
        strcpy(response->msg.StatusResponse.task_name,requests[i]->task_name);
        response->msg.StatusResponse.time_elapsed = m->msg.StatusRequest.clock - requests[i]->exec_time;
        // 3. Write da resposta 
        write(response_fd, response, sizeof(struct message));
        free(response);
    }


    // 4. Fechar Pipe
    close(response_fd);
}

void save_task(Task t){
    char path[50];
    char task_file[20];
    strcpy(path, folders_path);
    sprintf(task_file, "/process_%d.txt",t->process_pid);
    strcat(path,task_file);

    int output_fd;
    if((output_fd = open(path,O_WRONLY | O_CREAT | O_TRUNC ,0644)) < 0){
        perror("Error opening file to write data!\n");
        _exit(-1);
    }
    
    write(output_fd,"Program name: ",sizeof(char) * strlen("Program name: "));
    write(output_fd, t->task_name,sizeof(char) * strlen(t->task_name));
    write(output_fd,"\n",sizeof(char) * strlen("\n"));
    write(output_fd,"Time: ", sizeof(char) * strlen("Time: "));
    char time[16];
    snprintf(time,sizeof(time),"%ldms", t->exec_time); 
    write(output_fd,time,sizeof(char) * strlen(time));  
    close(output_fd);
}

void monitoring(){
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
            }
            else if(new_message->type == 2){
                pid_t pid;

                Task t = findRequest(new_message->msg.EEnd.process_pid);
                long int initial_time = t->exec_time;
                t->exec_time = new_message->msg.EEnd.end - initial_time;
                finishRequest(t);

                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if (!pid){
                    send_exec_time(new_message, t);
                    printf("(%d):$ Child nrº (%d): Message sent to request nrº (%d)!\n",getppid(), getpid(), t->process_pid);
                    save_task(t);
                    _exit(0);
                } else {
                    pids[sons] = pid;
                    sons++;
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
        printUsage();
    }
    else {
        if (initMainChannel() < 0)
            return -1;
        folders_path = strdup(argv[1]);
        // Iniciar monitoring
        monitoring();
    }
    close(main_channel_fd);
    unlink(MAIN_FIFO);
    return 0;
} 
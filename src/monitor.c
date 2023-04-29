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
    done_count = 0,
    pids_count = 0;
int main_channel_fd;
char * folders_path;
pid_t * pids;


// =====================================================================================


//  =================================== ** PRINTS ** ===================================

void printSingleTask(Task t){
    printf("%d\n%s\n%ldms\n",t->process_pid,t->tt.Single.task_name, t->tt.Single.exec_time);
}

void printPipelineTask(Task t){
    printf("PID: %d\n", t->process_pid);
    for(int i = 0; i < (t->tt.Pipeline.nr_commands) - 1 ; i++){
        printf("%s |", t->tt.Pipeline.tasks_names[i]);
    }
    printf("%s \n", t->tt.Pipeline.tasks_names[(t->tt.Pipeline.nr_commands) - 1]);
    printf("%ldms \n", t->tt.Pipeline.exec_time);
}

void printDoneList(){
    for(int i = 0; i < done_count; i++)
        if(done[i]->type == 1)
            printSingleTask(done[i]);
        else if ( done[i]->type == 2)
            printPipelineTask(done[i]);
}

void printRequests(){
    for(int i = 0; i < requests_count; i++)
        if(requests[i]->type == 1)
            printSingleTask(requests[i]);
        else if (requests[i]->type == 2)
            printPipelineTask(requests[i]);
        
}

void printUsage(){
    printf("Usage:\n");
    printf("1: ./monitor PIDS-folder\n");
}


// =====================================================================================

void addPid(pid_t pid){
    pids_count++;
    pids = realloc(pids,pids_count * sizeof(pid));
    pids[pids_count - 1] = pid;
}

void addRequest(Task task){
    Task new_task = malloc(sizeof(struct task));
    new_task->type = task->type;
    new_task->process_pid = task->process_pid;

    if(task->type == 1){
        strncpy(new_task->tt.Single.task_name, task->tt.Single.task_name, sizeof(new_task->tt.Single.task_name) - 1);
        new_task->tt.Single.task_name[sizeof(new_task->tt.Single.task_name) - 1] = '\0';
        new_task->tt.Single.exec_time = task->tt.Single.exec_time;
    }
    else if(task->type == 2){
        for(int i = 0 ; i < task->tt.Pipeline.nr_commands ; i++){
            strncpy(new_task->tt.Pipeline.tasks_names[i], task->tt.Pipeline.tasks_names[i], sizeof(new_task->tt.Pipeline.tasks_names[i]) - 1);
            new_task->tt.Pipeline.tasks_names[i][sizeof(new_task->tt.Pipeline.tasks_names[i]) - 1] = '\0';
        }
        new_task->tt.Pipeline.nr_commands = task->tt.Pipeline.nr_commands;
        new_task->tt.Pipeline.exec_time = task->tt.Pipeline.exec_time;

    }

    requests = realloc(requests, (requests_count + 1) * sizeof(struct task));
    requests[requests_count] = new_task;
    requests_count++;
}

Task findRequest(pid_t pid){
    
    for (int i = 0; i < requests_count; i++) {
        if (requests[i]->process_pid == pid) {
            Task new_task = (Task) malloc(sizeof(struct task));
            memcpy(new_task, requests[i], sizeof(struct task));
            return new_task;
        }
    }

    return NULL;

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

Task createSingleTask(Message m){
    Task new_task = malloc(sizeof(struct task));
    new_task->type = 1;
    new_task->process_pid = m->msg.EStart.process_pid;
    strncpy(new_task->tt.Single.task_name,m->msg.EStart.task_name,sizeof(new_task->tt.Single.task_name) - 1);
    new_task->tt.Single.task_name[sizeof(new_task->tt.Single.task_name) - 1] = '\0';
    new_task->tt.Single.exec_time = m->msg.EStart.start;
    return new_task;
}

Task createPipelineTask(Message m){
    Task new_task = malloc(sizeof(struct task));
    new_task->type = 2;
    new_task->process_pid = m->msg.PStart.process_pid;
    for(int i = 0; i < m->msg.PStart.nr_commands; i++){
        strncpy(new_task->tt.Pipeline.tasks_names[i],m->msg.PStart.tasks_names[i],sizeof(new_task->tt.Pipeline.tasks_names[i]) - 1);
        new_task->tt.Pipeline.tasks_names[i][sizeof(new_task->tt.Pipeline.tasks_names[i]) - 1] = '\0';
    }
    new_task->tt.Pipeline.nr_commands = m->msg.PStart.nr_commands;
    new_task->tt.Pipeline.exec_time = m->msg.PStart.start;
    return new_task;
}

void send_exec_time_single(Message m, Task t){
    int response_fd;
    if((response_fd = open(m->msg.EEnd.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    write(response_fd, &t->tt.Single.exec_time, sizeof(long int));
    close(response_fd);
}

void send_exec_time_pipeline(Message m, Task t){
    int response_fd;
    if((response_fd = open(m->msg.PEnd.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    write(response_fd, &t->tt.Pipeline.exec_time,sizeof(long int));
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
        if(requests[i]->type == 1){
            response->type = 6;
            response->msg.StatusResponseS.process_pid = requests[i]->process_pid;
            strncpy(response->msg.StatusResponseS.task_name,requests[i]->tt.Single.task_name, sizeof(response->msg.StatusResponseS.task_name) - 1);
            response->msg.StatusResponseS.task_name[sizeof(response->msg.StatusResponseS.task_name) - 1] = '\0';
            response->msg.StatusResponseS.time_elapsed =  m->msg.StatusRequest.clock - requests[i]->tt.Single.exec_time;
            // 3. Write da resposta 
            write(response_fd, response, sizeof(struct message));
        }
        else if(requests[i]->type == 2){
            // Resposta para uma task com pipeline
            response->type = 7;
            response->msg.StatusResponseP.process_pid = requests[i]->process_pid;
            for(int i = 0; i < requests[i]->tt.Pipeline.nr_commands ; i++){
                strncpy(response->msg.StatusResponseP.tasks_pipeline[i],requests[i]->tt.Pipeline.tasks_names[i],sizeof(response->msg.StatusResponseP.tasks_pipeline[i]) - 1);
                response->msg.StatusResponseP.tasks_pipeline[i][sizeof(response->msg.StatusResponseP.tasks_pipeline[i]) - 1] = '\0';
            }
            response->msg.StatusResponseP.time_elapsed = requests[i]->tt.Pipeline.exec_time - m->msg.StatusRequest.clock;
            // 3. Write da resposta 
            write(response_fd, response, sizeof(struct message));
        }
    }

    // 4. Fechar Pipe
    close(response_fd);
}

void stats_time_response(Message m){
    int response_fd;
    if((response_fd = open(m->msg.StatusRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    long int response = 0;

    for(int i = 0; m->msg.StatsTimeRequest.request_pids[i] != '\0' ; i++){
        pid_t target = m->msg.StatsTimeRequest.request_pids[i];
        for(int j = 0, flag = 0; !flag ; j++){
            if(target == done[j]->process_pid){
                if(done[j]->type == 1)
                    response += done[j]->tt.Single.exec_time;
                else{
                    response += done[j]->tt.Pipeline.exec_time;
                }
                flag = 1;
            }
        }
    }

    write(response_fd, &response, sizeof(long int));
    close(response_fd);
}

void save_single_task(Task t){
    char path[50];
    char task_file[20];
    strcpy(path, folders_path);
    sprintf(task_file, "/process_%d.txt",t->process_pid);
    strcat(path,task_file);

    int output_fd;
    if((output_fd = open(path,O_WRONLY | O_CREAT | O_TRUNC ,0644)) < 0){
        perror("Error opening to write data!");
        _exit(-1);
    }
    
    write(output_fd,"Program name: ",sizeof(char) * strlen("Program name: "));
    write(output_fd, t->tt.Single.task_name,sizeof(char) * strlen(t->tt.Single.task_name));
    write(output_fd,"\n",sizeof(char) * strlen("\n"));
    write(output_fd,"Time: ", sizeof(char) * strlen("Time: "));
    char time[16];
    snprintf(time,sizeof(time),"%ldms", t->tt.Single.exec_time); 
    write(output_fd,time,sizeof(char) * strlen(time));  
    close(output_fd);
}

void monitoring(){
    ssize_t bytes_read;
    Message new_message = malloc(sizeof(struct message));
    int flag = 0;
    while(!flag){
        while((bytes_read = read(main_channel_fd,new_message,sizeof(struct message))) > 0){
            if(new_message->type == 1){
                Task t = createSingleTask(new_message);
                addRequest(t);
                printRequests();
            }
            else if(new_message->type == 2){
                pid_t pid;

                Task t = findRequest(new_message->msg.EEnd.process_pid);
                long int initial_time = t->tt.Single.exec_time;
                t->tt.Single.exec_time = new_message->msg.EEnd.end - initial_time;
                finishRequest(t);

                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if (!pid){
                    send_exec_time_single(new_message, t);
                    printf("(%d):$ Child nrº (%d): Message sent to request nrº (%d)!\n",getppid(), getpid(), t->process_pid);
                    save_single_task(t);
                    _exit(0);
                } else addPid(pid);
            }
            else if(new_message->type == 3){
                Task t = createPipelineTask(new_message);
                addRequest(t);
                printRequests();
            }
            else if(new_message->type == 4){
                pid_t pid;

                Task t = findRequest(new_message->msg.PEnd.process_pid);
                long int initial_time = t->tt.Pipeline.exec_time;
                t->tt.Pipeline.exec_time = new_message->msg.PEnd.exec_time - initial_time;
                finishRequest(t);

                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if (!pid){
                    send_exec_time_pipeline(new_message, t);
                    printf("(%d):$ Child nrº (%d): Message sent to request nrº (%d)!\n",getppid(), getpid(), t->process_pid);
                    _exit(0);
                } else addPid(pid);
            }
            else if(new_message->type == 5){
                pid_t pid;
                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if(!pid){
                    status_response(new_message);
                    printf("%d sended message to the user waiting!\n", getpid());
                    _exit(0);
                } else addPid(pid);
        
            }
            else if(new_message->type == 7){
                pid_t pid;
                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if(!pid){
                    stats_time_response(new_message);
                    printf("%d sended message to the user waiting!\n", getpid());
                    _exit(0);
                }
                else addPid(pid);
            }
            else if(new_message->type == -1){
                flag = 1;
                break;
            }
            free(new_message);
            new_message = malloc(sizeof(struct message));
        }
    }


    // Wait to catch all the sons that were created during the monitor life-span

    for(int i = 0; i < pids_count; i++){
        int status;
        waitpid(pids[i],&status,0);
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
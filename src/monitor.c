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


// ======================================================================================
// =================================== ** PRINTS ** =====================================

void printSingleTask(Task t){
    printf("%d\n%s\n%ldms\n",t->process_pid,t->info.Single.task_name, t->info.Single.exec_time);
}

void printPipelineTask(Task t){
    printf("PID: %d\n", t->process_pid);
    for(int i = 0; i < (t->info.Pipeline.nr_commands) - 1 ; i++){
        printf("%s | ", t->info.Pipeline.tasks_names[i]);
    }
    printf("%s\n", t->info.Pipeline.tasks_names[(t->info.Pipeline.nr_commands) - 1]);
    printf("%ldms \n", t->info.Pipeline.exec_time);
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
// ====================================== AUX ==========================================

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
        strncpy(new_task->info.Single.task_name, task->info.Single.task_name, sizeof(new_task->info.Single.task_name) - 1);
        new_task->info.Single.task_name[sizeof(new_task->info.Single.task_name) - 1] = '\0';
        new_task->info.Single.exec_time = task->info.Single.exec_time;
    }
    else if(task->type == 2){
        for(int i = 0 ; i < task->info.Pipeline.nr_commands ; i++){
            strncpy(new_task->info.Pipeline.tasks_names[i], task->info.Pipeline.tasks_names[i], sizeof(new_task->info.Pipeline.tasks_names[i]) - 1);
            new_task->info.Pipeline.tasks_names[i][sizeof(new_task->info.Pipeline.tasks_names[i]) - 1] = '\0';
        }
        new_task->info.Pipeline.nr_commands = task->info.Pipeline.nr_commands;
        new_task->info.Pipeline.exec_time = task->info.Pipeline.exec_time;

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

// ======================================================================================
// ================================== Create Tasks ======================================

Task createSingleTask(Message m){
    Task new_task = malloc(sizeof(struct task));
    new_task->type = 1;
    new_task->process_pid = m->data.EStart.process_pid;
    strncpy(new_task->info.Single.task_name,m->data.EStart.task_name,sizeof(new_task->info.Single.task_name) - 1);
    new_task->info.Single.task_name[sizeof(new_task->info.Single.task_name) - 1] = '\0';
    new_task->info.Single.exec_time = m->data.EStart.start;
    return new_task;
}

Task createPipelineTask(Message m){
    Task new_task = malloc(sizeof(struct task));
    new_task->type = 2;
    new_task->process_pid = m->data.PStart.process_pid;
    for(int i = 0; i < m->data.PStart.nr_commands; i++){
        strncpy(new_task->info.Pipeline.tasks_names[i],m->data.PStart.tasks_names[i],sizeof(new_task->info.Pipeline.tasks_names[i]) - 1);
        new_task->info.Pipeline.tasks_names[i][sizeof(new_task->info.Pipeline.tasks_names[i]) - 1] = '\0';
    }
    new_task->info.Pipeline.nr_commands = m->data.PStart.nr_commands;
    new_task->info.Pipeline.exec_time = m->data.PStart.start;
    return new_task;
}

// ======================================================================================
// ================================= Execute Responses ==================================

void send_exec_time_single(Message m, Task t){
    int response_fd;
    if((response_fd = open(m->data.EEnd.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    write(response_fd, &t->info.Single.exec_time, sizeof(long int));
    close(response_fd);
}

void send_exec_time_pipeline(Message m, Task t){
    int response_fd;
    if((response_fd = open(m->data.PEnd.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    write(response_fd, &t->info.Pipeline.exec_time,sizeof(long int));
    close(response_fd);
}
// ======================================================================================
// =================================== Status Responses ==================================

void status_response(Message m){
    // 1. Abrir comunicação com cliente 
    int response_fd;
    if((response_fd = open(m->data.StatusRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    Message response = malloc(sizeof(struct message));
    for(int i = 0; i < requests_count; i++){
        // 2 . Constroi resposta
        if(requests[i]->type == 1){
            response->type = 6;
            response->data.StatusResponseS.process_pid = requests[i]->process_pid;
            strncpy(response->data.StatusResponseS.task_name,requests[i]->info.Single.task_name, sizeof(response->data.StatusResponseS.task_name) - 1);
            response->data.StatusResponseS.task_name[sizeof(response->data.StatusResponseS.task_name) - 1] = '\0';
            response->data.StatusResponseS.time_elapsed =  m->data.StatusRequest.clock - requests[i]->info.Single.exec_time;
            // 3. Write da resposta 
            write(response_fd, response, sizeof(struct message));
        }
        else if(requests[i]->type == 2){
            // Resposta para uma task com pipeline
            response->type = 7;
            response->data.StatusResponseP.process_pid = requests[i]->process_pid;
            for(int j = 0; j < requests[i]->info.Pipeline.nr_commands ; j++){
                strncpy(response->data.StatusResponseP.tasks_pipeline[j],requests[i]->info.Pipeline.tasks_names[j],sizeof(response->data.StatusResponseP.tasks_pipeline[j]) - 1);
                response->data.StatusResponseP.tasks_pipeline[j][sizeof(response->data.StatusResponseP.tasks_pipeline[j]) - 1] = '\0';
            }
            response->data.StatusResponseP.nr_comandos = requests[i]->info.Pipeline.nr_commands;
            response->data.StatusResponseP.time_elapsed = m->data.StatusRequest.clock - requests[i]->info.Pipeline.exec_time;
            // 3. Write da resposta 
            write(response_fd, response, sizeof(struct message));
        }
    }

    // 4. Fechar Pipe
    close(response_fd);
}

void status_all_response(Message m){
    int response_fd;
    if((response_fd = open(m->data.StatusRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }
    Message response = malloc(sizeof(struct message));
    for(int i = 0; i < requests_count; i++){
        if(requests[i]->type == 1){
            response->type = 12;
            response->data.StatusResponseS.process_pid = requests[i]->process_pid;
            strncpy(response->data.StatusResponseS.task_name,requests[i]->info.Single.task_name, sizeof(response->data.StatusResponseS.task_name) - 1);
            response->data.StatusResponseS.task_name[sizeof(response->data.StatusResponseS.task_name) - 1] = '\0';
            response->data.StatusResponseS.time_elapsed =  m->data.StatusRequest.clock - requests[i]->info.Single.exec_time;
            write(response_fd, response, sizeof(struct message));
        }
        else if(requests[i]->type == 2){
            response->type = 13;
            response->data.StatusResponseP.process_pid = requests[i]->process_pid;
            for(int j = 0; j < requests[i]->info.Pipeline.nr_commands ; j++){
                strncpy(response->data.StatusResponseP.tasks_pipeline[j],requests[i]->info.Pipeline.tasks_names[j],sizeof(response->data.StatusResponseP.tasks_pipeline[j]) - 1);
                response->data.StatusResponseP.tasks_pipeline[j][sizeof(response->data.StatusResponseP.tasks_pipeline[j]) - 1] = '\0';
            }
            response->data.StatusResponseP.time_elapsed = m->data.StatusRequest.clock - requests[i]->info.Pipeline.exec_time;
            response->data.StatusResponseP.nr_comandos = requests[i]->info.Pipeline.nr_commands;
            write(response_fd, response, sizeof(struct message));
        }
    }

    for(int i = 0; i < done_count; i++){
        if(done[i]->type == 1){
            response->type = 14;
            response->data.StatusResponseS.process_pid = done[i]->process_pid;
            strncpy(response->data.StatusResponseS.task_name,done[i]->info.Single.task_name, sizeof(response->data.StatusResponseS.task_name) - 1);
            response->data.StatusResponseS.task_name[sizeof(response->data.StatusResponseS.task_name) - 1] = '\0';
            response->data.StatusResponseS.time_elapsed = done[i]->info.Single.exec_time;
            write(response_fd, response, sizeof(struct message));
        }
        else if(done[i]->type == 2){
            response->type = 15;
            response->data.StatusResponseP.process_pid = done[i]->process_pid;
            for(int j = 0; j < done[i]->info.Pipeline.nr_commands ; j++){
                strncpy(response->data.StatusResponseP.tasks_pipeline[j],done[i]->info.Pipeline.tasks_names[j],sizeof(response->data.StatusResponseP.tasks_pipeline[j]) - 1);
                response->data.StatusResponseP.tasks_pipeline[j][sizeof(response->data.StatusResponseP.tasks_pipeline[j]) - 1] = '\0';
            }
            response->data.StatusResponseP.time_elapsed = done[i]->info.Pipeline.exec_time;
            response->data.StatusResponseP.nr_comandos = done[i]->info.Pipeline.nr_commands;
            write(response_fd, response, sizeof(struct message));
        }
    }

    close(response_fd);
}

// ======================================================================================
// ================================== Stats Responses ===================================


void stats_time_response(Message m){
    int response_fd;
    if((response_fd = open(m->data.StatsRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    pid_t pid;
    int fd[2];
    int status;

    if(pipe(fd) < 0){
        perror("Error creating pipe");
        _exit(-1);
    }

    for(int i = 0; i < m->data.StatsRequest.nr_pids ; i++){
        if((pid = fork()) < 0){
            perror("Error using fork()");
            _exit(-1);
        }
        else if(!pid){
            close(fd[0]);
            pid_t target = m->data.StatsRequest.request_pids[i];
            for(int j = 0, flag = 0; !flag ; j++){
                if(target == done[j]->process_pid){
                    if(done[j]->type == 1)
                        write(fd[1], &done[j]->info.Single.exec_time, sizeof(long int));
                    else if(done[j]->type == 2)
                        write(fd[1], &done[j]->info.Pipeline.exec_time, sizeof(long int)); 
                    flag = 1;
                    close(fd[1]);
                    _exit(0);
                }
            }
            close(fd[1]);
            _exit(-1);
        }
    }

    close(fd[1]);
    long int num;
    long int response = 0;

    for(int i = 0; i < m->data.StatsRequest.nr_pids ; i++){
        wait(&status);
        if(WIFEXITED(status)){
            if(WEXITSTATUS(status) == 0)
                if(read(fd[0], &num, sizeof(long int)) > 0)
                    response += num;
        }
    }

    close(fd[0]);
    write(response_fd, &response, sizeof(long int));
    close(response_fd);
}

void stats_command_response(Message m){
    int response_fd;
    if((response_fd = open(m->data.StatsCommandRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }

    pid_t pid;
    int fd[2];
    int status;

    if(pipe(fd) < 0){
        perror("Error creating pipe");
        _exit(-1);
    }

    char * program = strdup(m->data.StatsCommandRequest.task_name);
    for(int i = 0; i < m->data.StatsCommandRequest.nr_pids; i++){
        if((pid = fork()) < 0){
            perror("Error using fork()!");
            _exit(-1);
        }
        else if(!pid){
            close(fd[0]);
            pid_t target = m->data.StatsCommandRequest.request_pids[i];
            for(int j = 0; j < done_count ; j++){
                if(done[j]->process_pid == target){
                    if(done[j]->type == 1){
                        char * c = strtok(done[j]->info.Single.task_name, " ");
                        if(!strcmp(program, c)){
                            int x = 1;
                            write(fd[1], &x, sizeof(int));
                            close(fd[1]);
                            _exit(0);
                        }
                    }
                    else if(done[j]->type == 2){
                        int x = 0;
                        for(int k = 0; k < done[j]->info.Pipeline.nr_commands; k++){
                            char * c = strtok(done[j]->info.Pipeline.tasks_names[k], " ");
                            if(!strcmp(program, c))
                                x++;
                        }
                        if(x > 0){
                            write(fd[1], &x, sizeof(int));
                            close(fd[1]);
                            _exit(0);
                        }
                    }
                }
            }
            close(fd[1]);
            _exit(-1);
        }
    }
    close(fd[1]);

    int response = 0;
    for(int i = 0; i < m->data.StatsCommandRequest.nr_pids; i++){
        wait(&status);
        if(WIFEXITED(status)){
            if(WEXITSTATUS(status) == 0){
                int num;
                read(fd[0], &num, sizeof(int));
                response += num;
            }
        }
    }

    write(response_fd, &response, sizeof(int));
    close(response_fd);
}



void stats_uniq_response(Message m){
    int response_fd;
    if((response_fd = open(m->data.StatsRequest.response_path, O_WRONLY)) < 0){
        perror("Error opening fifo!\n");
        _exit(-1);
    }
    
    pid_t pid;
    int fd[2];
    int status;

    if(pipe(fd) < 0){
        perror("Error creating pipe");
        _exit(-1);
    }

    for(int i = 0; i < m->data.StatsRequest.nr_pids ; i++){
        if((pid = fork()) < 0){
            perror("Error using fork()!");
            _exit(-1);
        }
        else if(!pid){
            close(fd[0]);

            pid_t target = m->data.StatsRequest.request_pids[i];
            for(int j = 0; j < done_count; j++){
                if(target == done[j]->process_pid){
                    if(done[j]->type == 1){
                        char * cmd = strdup(done[j]->info.Single.task_name);
                        char * program;
                        program = strdup(strtok(cmd, " "));
                        write(fd[1], program, MAX_TASK_NAME_SIZE);
                        close(fd[1]);
                        _exit(0);
                    }
                    else if(done[j]->type == 2){
                        for(int k = 0; k < done[j]->info.Pipeline.nr_commands; k++){
                            char * cmd = strdup(done[j]->info.Pipeline.tasks_names[k]);
                            char * program;
                            program  = strdup(strtok(cmd, " "));
                            write(fd[1], program, MAX_TASK_NAME_SIZE);
                        }
                        close(fd[1]);
                        _exit(0);
                    }
                }
            }
            close(fd[1]);
            _exit(-1);
        }
    }
    close(fd[1]);

    
    char * programs[MAX_TASK_NAME_SIZE];
    int size = 0;

    char * task_name = malloc(sizeof(char) * MAX_TASK_NAME_SIZE);
    ssize_t read_bytes;
    while((read_bytes = read(fd[0], task_name, MAX_TASK_NAME_SIZE))){
        int flag = 0;
        for(int j = 0; j < size; j++)
            if(!strcmp(task_name, programs[j])) 
                flag = 1;
        if(!flag){
            programs[size] = malloc(sizeof(char) * MAX_TASK_NAME_SIZE);
            memset(programs[size], 0, sizeof(char) * MAX_TASK_NAME_SIZE);
            strcpy(programs[size], task_name);
            size++;
        }
    }
    close(fd[0]);

    for (int i = 0; i < size ; i++){
        write(response_fd, programs[i], sizeof(char) * MAX_TASK_NAME_SIZE);
    }
    
    for(int i = 0; i < m->data.StatsRequest.nr_pids ; i++){
        wait(&status);
    }
    close(response_fd);
}

// ======================================================================================
// ================================== Save To File ======================================

void save_single_task(Task t){
    char path[64];
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
    write(output_fd, t->info.Single.task_name,sizeof(char) * strlen(t->info.Single.task_name));
    write(output_fd,"\n",sizeof(char) * strlen("\n"));
    write(output_fd,"Time: ", sizeof(char) * strlen("Time: "));
    char time[16];
    snprintf(time,sizeof(time),"%ldms", t->info.Single.exec_time); 
    write(output_fd,time,sizeof(char) * strlen(time));  
    close(output_fd);
}

void save_pipeline_task(Task t){
    char path[64];
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
    int nr_commands = t->info.Pipeline.nr_commands;
    for(int i = 0; i < nr_commands-1; i++){
        char cmd[50];
        strcpy(cmd,t->info.Pipeline.tasks_names[i]);
        write(output_fd, cmd,sizeof(char) * strlen(cmd));
        write(output_fd," | ", sizeof(char) * strlen(" | "));
    }
    write(output_fd, t->info.Pipeline.tasks_names[nr_commands - 1],sizeof(char) * strlen(t->info.Pipeline.tasks_names[nr_commands - 1]));
    write(output_fd,"\n",sizeof(char) * strlen("\n"));
    write(output_fd,"Time: ", sizeof(char) * strlen("Time: "));
    char time[16];
    snprintf(time,sizeof(time),"%ldms", t->info.Pipeline.exec_time); 
    write(output_fd,time,sizeof(char) * strlen(time));  
    close(output_fd);
}
// ======================================================================================

void monitoring(){
    ssize_t bytes_read;
    Message new_message = malloc(sizeof(struct message));
    int flag = 0;
    while(!flag){
        while((bytes_read = read(main_channel_fd,new_message,sizeof(struct message))) > 0){
            if(new_message->type == 1){
                Task t = createSingleTask(new_message);
                addRequest(t);
                printf("[EXECUTE]($): Added request nrº %d to the running tasks!\n",t->process_pid);
            }
            else if(new_message->type == 2){
                pid_t pid;

                Task t = findRequest(new_message->data.EEnd.process_pid);

                long int initial_time = t->info.Single.exec_time;
                t->info.Single.exec_time = new_message->data.EEnd.end - initial_time;

                finishRequest(t);

                printf("[EXECUTE]($): Ended request nrº:(%d)\n",t->process_pid);

                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if (!pid){
                    send_exec_time_single(new_message, t);
                    printf("[EXECUTE]($): Sent execution time to request nrº (%d)!\n", t->process_pid);
                    save_single_task(t);
                    _exit(0);
                } else addPid(pid);
            }
            else if(new_message->type == 3){
                Task t = createPipelineTask(new_message);
                addRequest(t);
                printf("[EXECUTE]($): Added request nrº %d to the running tasks!\n",t->process_pid);
            }
            else if(new_message->type == 4){
                pid_t pid;

                Task t = findRequest(new_message->data.PEnd.process_pid);

                long int initial_time = t->info.Pipeline.exec_time;
                t->info.Pipeline.exec_time = new_message->data.PEnd.exec_time - initial_time;

                finishRequest(t);

                printf("[EXECUTE]($): Ended request nrº (%d)\n",t->process_pid);

                if((pid = fork()) < 0){
                    perror("Error using fork()!\n");
                    _exit(-1);
                }
                else if (!pid){
                    send_exec_time_pipeline(new_message, t);
                    printf("[EXECUTE]($): Sent execution time to request nrº (%d)!\n", t->process_pid);
                    save_pipeline_task(t);
                    _exit(0);
                } else addPid(pid);
            }
            else if(new_message->type == 5){
                pid_t pid;
                if((pid = fork()) < 0){
                    perror("Error using fork()!");
                    _exit(-1);
                }
                else if(!pid){
                    status_response(new_message);
                    printf("[STATUS]($): Sent message to the user waiting!\n");
                    _exit(0);
                } else addPid(pid);
        
            }
            else if(new_message->type == 8){
                pid_t pid;
                if((pid = fork()) < 0){
                    perror("Error using fork()!");
                    _exit(-1);
                }
                else if(!pid){
                    stats_time_response(new_message);
                    printf("[STATS_TIME]($): Sent message to the user waiting!\n");
                    _exit(0);
                }
                else addPid(pid);
            }
            else if(new_message->type == 9){
                pid_t pid;
                if((pid = fork()) < 0){
                    perror("Error using fork()!");
                    _exit(-1);
                }
                else if(!pid){
                    stats_command_response(new_message);
                    printf("[STATS_COMMAND]($): Sent message to the user waiting!\n");
                    _exit(0);
                }
                else addPid(pid);
            }
            else if(new_message->type == 10){
                pid_t pid;
                if((pid = fork()) < 0){
                    perror("Error using fork()!");
                    _exit(-1);
                }
                else if(!pid){
                    stats_uniq_response(new_message);
                    printf("[STATS_UNIQ]($): Sent message to the user waiting!\n");
                    _exit(0);
                }
                else addPid(pid);
            }
            else if(new_message->type == 11){
                pid_t pid;
                if((pid = fork()) < 0){
                    perror("Error using fork()!");
                    _exit(-1);
                }
                else if(!pid){
                    status_all_response(new_message);
                    printf("[STATUS_ALL]($): Sent message to the user waiting!\n");
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
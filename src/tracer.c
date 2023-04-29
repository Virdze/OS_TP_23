#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "../headers/message.h"
#include "../headers/task.h"

#define MAIN_FIFO "../tmp/main_fifo"

//  =================================== ** GLOBALS ** ===================================

int main_channel_fd;

// =====================================================================================

//  =================================== ** AUX ** ===================================

long int get_time_of_day(){
    struct timeval t;
    gettimeofday(&t,NULL);
    long int time = t.tv_sec * 1000 + t.tv_usec;
    return time;
}

void parseCommand(char * command_string, char ** command){
    
    int nr_args = 0;
    char * token = strtok(command_string, " ");
    while (token != NULL){
        command[nr_args++] = token;
        token = strtok(NULL," ");
    }
    command[nr_args] = NULL;
}

//  =================================== ** PRINTS ** ===================================

void printUsage(){
    printf("Usage:.\n");
    printf("1: ./tracer execute -u prog-a arg-1 (...) arg-n\n");
    printf("2: ./tracer execute -p prog-a arg-1 (...) arg-n | prog-b arg-1 (...) arg-n | prog-c arg-1 (...) arg-n\n");
    printf("3: ./tracer status\n");
    printf("4: ./tracer stats-time PID-1 PID-2 (...) PID-N\n");
    printf("5: ./tracer stats-command prog-a PID-1 PID-2 (...) PID-N\n");
    printf("6: ./tracer stats-uniq PID-1 PID-2 (...) PID-N\n");
}

void print_status_response_single(Message m){
    printf("================================\n");
    printf("PID: %d\n", m->msg.StatusResponseS.process_pid);
    printf("Task: %s\n", m->msg.StatusResponseS.task_name);
    printf("Time Elapsed: %ldms\n", m->msg.StatusResponseS.time_elapsed);
    printf("================================\n");
}

void print_status_response_pipeline(Message m){
    printf("================================\n");
    printf("PID: %d\n", m->msg.StatusResponseP.process_pid);
    printf("Tasks:\n");
    for(int i = 0; i < (m->msg.StatusResponseP.nr_comandos) - 1;i++){
        printf("%s |", m->msg.StatusResponseP.tasks_pipeline[i]);
    }
    printf("%s \n", m->msg.StatusResponseP.tasks_pipeline[(m->msg.StatusResponseP.nr_comandos) - 1]);
    printf("Time Elapsed: %ldms\n", m->msg.StatusResponseS.time_elapsed);
    printf("================================\n");
}

void printStatusTimeResponse(long int response){
    printf("================================\n");
    printf("Total execution time is %ld ms\n",response);
    printf("================================\n");
}

// =====================================================================================


//  =================================== ** Messages ** ===================================


// =====================================================================================
void executeSingle(char ** command){
    pid_t pid;
    int status;
    if((pid = fork()) < 0){
        perror("Error using fork()!");
        _exit(-1);
    }
    else if(!pid){
        execvp(command[0], command);
        _exit(1);
    }
    else wait(&status); 
}

void executePipeline(char ** commands, int nr_commands){

    pid_t pid;
    pid_t pids[nr_commands];

    int status;
    int fd[nr_commands-1][2];

    for(int i = 0; i < nr_commands-1; i++){
        if(pipe(fd[i]) < 0){
            perror("Error while creating pipe");
            _exit(-1);
        }
    }

    for(int i = 0; i < nr_commands; i ++){
        if((pid = fork()) < 0){
            perror("Error while using fork()!");
            _exit(-1);
        }
        else if(!pid){
            if(i == 0){ // First command
                // Close reading from 1st pipe,
                // redirecting the STDOUT to the writing end of the pipe
                close(fd[0][0]);
                dup2(fd[0][1],1);
                close(fd[0][1]);
                
                // Close other ends of all pipes that won't be used to ensure EOF
                for(int j = 1 ; j < nr_commands - 1; j++){
                    close(fd[j][0]);
                    close(fd[j][1]);
                }


                // Parse string with command and arguments
                char * command[50];
                parseCommand(commands[i],command);

                // Execute command
                execvp(command[0], command);
                _exit(-1);
            }
            else if(i == nr_commands - 1){
                close(fd[i-1][1]);
                dup2(fd[i-1][0], 0);
                close(fd[i-1][0]);

                for(int j = 0; j < nr_commands - 2; j++){
                    close(fd[j][0]);
                    close(fd[j][1]);
                }

                char * command[50];
                parseCommand(commands[i],command);
                execvp(command[0],command);
                _exit(-1);
            }
            else{
                close(fd[i-1][1]);
                dup2(fd[i-1][0], 0);
                close(fd[i-1][0]);
                close(fd[i][0]);
                dup2(fd[i][1],1);
                close(fd[i][1]);

                for (int j = 0; j < nr_commands - 1; j++) {
                    if (j != i - 1 && j != i) {
                        close(fd[j][0]);
                        close(fd[j][1]);
                    }
                }

                char * command[50];
                parseCommand(commands[i],command);
                execvp(command[0],command);
                _exit(-1);
            }
        }
        else pids[i] = pid;
    }

    for(int i = 0 ; i < nr_commands-1; i++){
        close(fd[i][0]);
        close(fd[i][1]);
    }

    for(int i = 0 ; i < nr_commands; i++){
        waitpid(pids[i],&status,0);
        if(WEXITSTATUS(status) < 0){
            printf("Something went wrong!\n");
            break;
        }
        else printf("Command runned!\n");   
    }    
}

int initMainChannel(){
    //  Abrir comunicação do lado do tracer 
    if ((main_channel_fd = open(MAIN_FIFO,O_WRONLY)) < 0){
        perror("Error opening request fifo!\n");
        return -1;
    }
    return 0;
}

int main(int argc, char * argv[]){
    if (argc < 2){
        printUsage();
        _exit(-1);
    }

    if(initMainChannel() < 0) return -1;

    if (!strcmp(argv[1],"execute")){
        if (!strcmp(argv[2], "-u")){
            // Execução de um comando
            //  1. Recolher informação do pedido

            pid_t pid = getpid();
            char * command[50];
            char * token = strtok(argv[3]," ");
            int i = 0;
            while(token != NULL){
                command[i++] = token;
                token = strtok(NULL," ");
            }

            command[i] = NULL;   
    
            // 2. Criar Mensagem de início de execução
            
            Message start_message = malloc(sizeof(struct message));
            start_message->type = 1;
            start_message->msg.EStart.start = get_time_of_day();
            start_message->msg.EStart.process_pid = pid;
            strncpy(start_message->msg.EStart.task_name,command[0],sizeof(start_message->msg.EStart.task_name) - 1);
            start_message->msg.EStart.task_name[sizeof(start_message->msg.EStart.task_name) - 1] = '\0';

            // 3. Enviar informação para o monitor
            write(main_channel_fd, start_message, sizeof(struct message));
            // 4. Informar utilizador de início da execução
            printf("Running PID %d\n", pid);
        
            // 5. Executar programa
            executeSingle(command);

            // 6. Criar Mensagem de fim de Execução
            Message end_message = malloc(sizeof(struct message));
            end_message->type = 2;
            end_message->msg.EEnd.end = get_time_of_day();
            end_message->msg.EEnd.process_pid = pid;
            char path[50];
            sprintf(path,"../tmp/process_%d", pid);
            strncpy(end_message->msg.EEnd.response_path,path,sizeof(end_message->msg.EEnd.response_path) - 1);
            end_message->msg.EEnd.response_path[sizeof(end_message->msg.EEnd.response_path) - 1] = '\0';
            // 7. Criar fifo para receber resposta 

            int response_fifo;
            if((response_fifo = mkfifo(path, 0666)) < 0){
                perror("Error creating response pipe.\n");
                _exit(-1);
            }
            // 8. Enviar informação para o monitor
            write(main_channel_fd, end_message ,sizeof(struct message));
            

            // 9. Abrir comunicação
            int response_fd;
            if((response_fd = open(path, O_RDONLY)) < 0){
                perror("Error opening fifo!");
                _exit(-1);
            }
            
            // 10. Esperar comunicação no path enviado ao monitor

            ssize_t bytes_read; 
            long int response;
            while((bytes_read = read(response_fd, &response, sizeof(long int))) > 0){
                printf("Ended in %ldms\n", response);
            }

            // 12. Fechar escrita para o pipe por parte do cliente
            close(main_channel_fd);
            close(response_fd);
            unlink(path);
        }
        else if (!strcmp(argv[2], "-p")) {
            // pipeline de programas

            pid_t pid = getpid();
            
            char * tasks[50];
            int nr_commands = 0;
            char * token = strtok(argv[3], "|");
            while(token != NULL){
                if(nr_commands > 20){
                    printf("Resources not enought to support more than 20 commands!\n");
                    _exit(-1);
                }
                tasks[nr_commands] = token;
                nr_commands++;
                token = strtok(NULL,"|");
            }
            

            Message start_message = malloc(sizeof(struct message));
            start_message->type = 3;
            start_message->msg.PStart.process_pid = pid;
            
            for(int i = 0; i < nr_commands; i++){
                strncpy(start_message->msg.PStart.tasks_names[i], tasks[i], sizeof(start_message->msg.PStart.tasks_names[i]) - 1);
                start_message->msg.PStart.tasks_names[i][sizeof(start_message->msg.PStart.tasks_names[i]) - 1] = '\0';
            }
            
            start_message->msg.PStart.nr_commands = nr_commands;
            start_message->msg.PStart.start = get_time_of_day();
            
            write(main_channel_fd, start_message, sizeof(struct message));
            printf("Running PID %d\n", pid);
            
            executePipeline(tasks, nr_commands);

            Message end_message = malloc(sizeof(struct message));
            end_message->type = 4;
            end_message->msg.PEnd.process_pid = pid;
            end_message->msg.PEnd.exec_time = get_time_of_day();
            char path[50];
            sprintf(path,"../tmp/process_%d", pid);
            strncpy(end_message->msg.PEnd.response_path,path,sizeof(end_message->msg.PEnd.response_path) - 1);
            end_message->msg.PEnd.response_path[sizeof(end_message->msg.PEnd.response_path) - 1] = '\0';
            

            int response_fifo;
            if((response_fifo = mkfifo(path, 0666)) < 0){
                perror("Error creating response pipe.\n");
                _exit(-1);
            }

            write(main_channel_fd, end_message, sizeof(struct message));
            
            int response_fd;
            if((response_fd = open(path, O_RDONLY)) < 0){
                perror("Error opening fifo!");
                _exit(-1);
            }
            
            // 10. Esperar comunicação no path enviado ao monitor

            ssize_t bytes_read; 
            long int response;
            while((bytes_read = read(response_fd, &response, sizeof(long int))) > 0){
                printf("Ended in %ldms\n", response);
            }

            // 12. Fechar escrita para o pipe por parte do cliente
            close(main_channel_fd);
            close(response_fd);
            unlink(path);
            

        }
    }
    else if(!strcmp(argv[1],"status")){
        // 1. Pid
        pid_t pid = getpid();

        // 2. Criar request
        Message request = malloc(sizeof(struct message));
        request->type = 5;
        request->msg.StatusRequest.clock = get_time_of_day();
        // 2.1 - Criar request path
        char path[50];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->msg.StatusRequest.response_path,path,sizeof(request->msg.StatusRequest.response_path) - 1);
        request->msg.StatusRequest.response_path[sizeof(request->msg.StatusRequest.response_path) - 1] = '\0';
        
        // 4. Criar fifo
        int response_fifo;
        if((response_fifo = mkfifo(path, 0666)) < 0){
            perror("Error creating response pipe.\n");
            _exit(-1);
        }
        // 3. Enviar request ao monitor
        write(main_channel_fd, request,sizeof(struct message));

        
        // 5. Abrir comunicação 
        int response_fd;
        if((response_fd = open(path, O_RDONLY)) < 0){
            perror("Error opening fifo!\n");
            _exit(-1);
        }
        
        // 6. Esperar resposta 
        ssize_t bytes_read; 
        Message response = malloc(sizeof(struct message));
        while((bytes_read = read(response_fd, response, sizeof(struct message))) > 0){
            if(response->type == 6)
                print_status_response_single(response);
            else if (response->type == 7){
                print_status_response_pipeline(response);
            }
        }

        // 8. Fechar descritores e destruir o fifo criado
        
        close(main_channel_fd);
        close(response_fd);
        unlink(path);
    }
    else if(!strcmp(argv[1], "stats-time")){
        pid_t pid = getpid();
        Message request = malloc(sizeof(struct message));
        request->type=7;

        for(int i = 2, j = 0; i < argc && i < 100;i++, j++){
            request->msg.StatsTimeRequest.request_pids[j] = atoi(argv[i]);
        }
        char path[50];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->msg.StatsTimeRequest.response_path,path,sizeof(request->msg.StatsTimeRequest.response_path) - 1);
        request->msg.StatsTimeRequest.response_path[sizeof(request->msg.StatsTimeRequest.response_path) - 1] = '\0';
        
        int response_fifo;
        if((response_fifo = mkfifo(path, 0666)) < 0){
            perror("Error creating response pipe.\n");
            _exit(-1);
        }

        write(main_channel_fd, request, sizeof(struct message));


        int response_fd;
        if((response_fd = open(path, O_RDONLY)) < 0){
            perror("Error opening fifo!\n");
            _exit(-1);
        }
        
        ssize_t bytes_read;
        long int response;
        while((bytes_read = read(response_fd, &response, sizeof(long int))) > 0){
            printStatusTimeResponse(response);
        }

        close(main_channel_fd);
        close(response_fd);
        unlink(path);
    }
    else if(!strcmp(argv[1],"END")){
        //End monitor (Temporario)
        Message end = malloc(sizeof(struct message));
        end->type = -1;
        write(main_channel_fd,end,sizeof(struct message));
        close(main_channel_fd);
    }

    return 0;
}
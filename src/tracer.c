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


#define MAIN_FIFO "../tmp/main_fifo"

//  =================================== ** GLOBALS ** ===================================

int main_channel_fd;

// =====================================================================================

//  =================================== ** AUX ** ===================================

long int get_time_of_day(){
    struct timeval t;
    gettimeofday(&t,NULL);
    long int time = (t.tv_sec * 1000) + (t.tv_usec / 1000);
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

void clearToken(char * token){
    int size = strlen(token);
    if(token[0] == ' '){
        for(int i = 0; i < size - 1; i++){
            token[i] = token[i+1];
        }
        token[--size] = '\0';
    }
    if(token[size - 1] == ' '){
        token[size-1] = '\0';
    }
}

//  =================================== ** PRINTS ** ===================================

void printUsage(){
    printf("Usage: \n");
    printf("1: ./tracer execute -u prog-a arg-1 (...) arg-n\n");
    printf("2: ./tracer execute -p prog-a arg-1 (...) arg-n | prog-b arg-1 (...) arg-n | prog-c arg-1 (...) arg-n\n");
    printf("3: ./tracer status\n");
    printf("4: ./tracer status-all\n");
    printf("5: ./tracer stats-time PID-1 PID-2 (...) PID-N\n");
    printf("6: ./tracer stats-command prog-a PID-1 PID-2 (...) PID-N\n");
    printf("7: ./tracer stats-uniq PID-1 PID-2 (...) PID-N\n");
}


void print_status_all_response_single(Message m){
    printf("%d %s %ldms\n", m->data.StatusResponseS.process_pid,m->data.StatusResponseS.task_name, m->data.StatusResponseS.time_elapsed);
}

void print_status_all_response_pipeline(Message m){
    printf("%d ", m->data.StatusResponseP.process_pid);
    for(int i = 0; i < (m->data.StatusResponseP.nr_comandos) - 1;i++){
        printf("%s | ", m->data.StatusResponseP.tasks_pipeline[i]);
    }
    printf("%s ", m->data.StatusResponseP.tasks_pipeline[(m->data.StatusResponseP.nr_comandos) - 1]);
    printf("%ldms\n", m->data.StatusResponseP.time_elapsed);
}

void print_status_all_response(Message * requests, int requests_count, Message * done, int done_count){
    printf("Executing:\n");
    for(int i = 0; i < requests_count ; i++){
        if(requests[i]->type == 12)
            print_status_all_response_single(requests[i]);
        else if (requests[i]->type == 13)
            print_status_all_response_pipeline(requests[i]);
    }
    printf("\nFinished:\n");
    for(int i = 0; i < done_count ; i++){
        if(done[i]->type == 14)
            print_status_all_response_single(done[i]);
        else if (done[i]->type == 15)
            print_status_all_response_pipeline(done[i]);
    }
}

void print_status_response_single(Message m){
    printf("%d ", m->data.StatusResponseS.process_pid);
    printf("%s ", m->data.StatusResponseS.task_name);
    printf("%ldms\n", m->data.StatusResponseS.time_elapsed);
}

void print_status_response_pipeline(Message m){
    printf("%d ", m->data.StatusResponseP.process_pid);
    for(int i = 0; i < (m->data.StatusResponseP.nr_comandos) - 1;i++){
        printf("%s | ", m->data.StatusResponseP.tasks_pipeline[i]);
    }
    printf("%s ", m->data.StatusResponseP.tasks_pipeline[(m->data.StatusResponseP.nr_comandos) - 1]);
    printf("%ldms\n", m->data.StatusResponseP.time_elapsed);
}

void printStatsTimeResponse(long int response){
    printf("Total execution time is %ld ms\n",response);
}

void printStatsCommandResponse(char * task_name, int response){
    printf("%s was executed %d time(s)\n",task_name,response);    
}


// =====================================================================================
// =================================== Execute =========================================

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
    } 
}

// =====================================================================================

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
            char * cmd = strdup(argv[3]);
            char * parsed_command[MAX_TASK_NAME_SIZE];
            parseCommand(argv[3], parsed_command); 
    
            // 2. Criar Mensagem de início de execução
            
            Message start_message = malloc(sizeof(struct message));
            start_message->type = 1;
            start_message->data.EStart.start = get_time_of_day();
            start_message->data.EStart.process_pid = pid;
            strncpy(start_message->data.EStart.task_name,cmd,sizeof(start_message->data.EStart.task_name) - 1);
            start_message->data.EStart.task_name[sizeof(start_message->data.EStart.task_name) - 1] = '\0';

            // 3. Enviar informação para o monitor
            write(main_channel_fd, start_message, sizeof(struct message));
            // 4. Informar utilizador de início da execução
            printf("Running PID %d\n", pid);

            // 5. Executar programa
            executeSingle(parsed_command);

            // 6. Criar Mensagem de fim de Execução
            Message end_message = malloc(sizeof(struct message));
            end_message->type = 2;
            end_message->data.EEnd.end = get_time_of_day();
            end_message->data.EEnd.process_pid = pid;
            char path[MAX_RESPONSE_PATH_LENGTH];
            sprintf(path,"../tmp/process_%d", pid);
            strncpy(end_message->data.EEnd.response_path,path,sizeof(end_message->data.EEnd.response_path) - 1);
            end_message->data.EEnd.response_path[sizeof(end_message->data.EEnd.response_path) - 1] = '\0';
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
            
            char * tasks[MAX_TASK_NAME_SIZE];
            int nr_commands = 0;
            char * token = strtok(argv[3], "|");
            while(token != NULL){
                if(nr_commands > 20){
                    printf("Resources not enought to support more than 20 commands!\n");
                    _exit(-1);
                }
                clearToken(token);
                tasks[nr_commands] = token;
                nr_commands++;
                token = strtok(NULL,"|");
            }
            

            Message start_message = malloc(sizeof(struct message));
            start_message->type = 3;
            start_message->data.PStart.process_pid = pid;
            
            for(int i = 0; i < nr_commands; i++){
                strncpy(start_message->data.PStart.tasks_names[i], tasks[i], sizeof(start_message->data.PStart.tasks_names[i]) - 1);
                start_message->data.PStart.tasks_names[i][sizeof(start_message->data.PStart.tasks_names[i]) - 1] = '\0';
            }
            
            start_message->data.PStart.nr_commands = nr_commands;
            start_message->data.PStart.start = get_time_of_day();
            
            write(main_channel_fd, start_message, sizeof(struct message));
            printf("Running PID %d\n", pid);
            
            executePipeline(tasks, nr_commands);

            Message end_message = malloc(sizeof(struct message));
            end_message->type = 4;
            end_message->data.PEnd.process_pid = pid;
            end_message->data.PEnd.exec_time = get_time_of_day();
            char path[MAX_RESPONSE_PATH_LENGTH];
            sprintf(path,"../tmp/process_%d", pid);
            strncpy(end_message->data.PEnd.response_path,path,sizeof(end_message->data.PEnd.response_path) - 1);
            end_message->data.PEnd.response_path[sizeof(end_message->data.PEnd.response_path) - 1] = '\0';
            

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
        request->data.StatusRequest.clock = get_time_of_day();
        // 2.1 - Criar request path
        char path[MAX_RESPONSE_PATH_LENGTH];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->data.StatusRequest.response_path,path,sizeof(request->data.StatusRequest.response_path) - 1);
        request->data.StatusRequest.response_path[sizeof(request->data.StatusRequest.response_path) - 1] = '\0';
        
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
    else if(!strcmp(argv[1],"status-all")){
        pid_t pid = getpid();

        Message request = malloc(sizeof(struct message));
        request->type = 11;
        request->data.StatusRequest.clock = get_time_of_day();
        char path[MAX_RESPONSE_PATH_LENGTH];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->data.StatusRequest.response_path,path,sizeof(request->data.StatusRequest.response_path) - 1);
        request->data.StatusRequest.response_path[sizeof(request->data.StatusRequest.response_path) - 1] = '\0';

        int response_fifo;
        if((response_fifo = mkfifo(path, 0666)) < 0){
            perror("Error creating response pipe.\n");
            _exit(-1);
        }

        write(main_channel_fd,request,sizeof(struct message));

        int response_fd;
        if((response_fd = open(path, O_RDONLY)) < 0){
            perror("Error opening fifo!\n");
            _exit(-1);
        }
        
        ssize_t bytes_read;
        int requests_count = 0,
            done_count = 0;
        Message * requests = NULL;
        Message * done = NULL;
        Message response = malloc(sizeof(struct message));
        while((bytes_read = read(response_fd, response, sizeof(struct message))) > 0){
            if(response->type == 12 || response->type == 13){
                requests = realloc(requests, (requests_count + 1) * sizeof(struct message));
                requests[requests_count] = malloc(sizeof(struct message));
                memcpy(requests[requests_count],response, sizeof(struct message));
                requests_count++;
            }
            else if(response->type == 14 || response->type == 15){
                done = realloc(done, (done_count + 1) * sizeof(struct message));
                done[done_count] = malloc(sizeof(struct message));
                memcpy(done[done_count],response, sizeof(struct message));
                done_count++;
            }
        }

        print_status_all_response(requests, requests_count, done, done_count);

        close(main_channel_fd);
        close(response_fd);
        unlink(path);
    }
    else if(!strcmp(argv[1], "stats-time")){
        pid_t pid = getpid();
        Message request = malloc(sizeof(struct message));
        request->type=8;
        request->data.StatsRequest.nr_pids = 0;

        for(int i = 2, j = 0; i < argc && i < 100;i++, j++){
            request->data.StatsRequest.request_pids[j] = atoi(argv[i]);
            request->data.StatsRequest.nr_pids++;
        }
        
        char path[MAX_RESPONSE_PATH_LENGTH];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->data.StatsRequest.response_path,path,sizeof(request->data.StatsRequest.response_path) - 1);
        request->data.StatsRequest.response_path[sizeof(request->data.StatsRequest.response_path) - 1] = '\0';
        
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
            printStatsTimeResponse(response);
        }

        close(main_channel_fd);
        close(response_fd);
        unlink(path);
    }
    else if(!strcmp(argv[1], "stats-command")){
        pid_t pid = getpid();

        Message request = malloc(sizeof(struct message));
        request->type = 9;
        
        strncpy(request->data.StatsCommandRequest.task_name, argv[2], sizeof(request->data.StatsCommandRequest.task_name) - 1);
        request->data.StatsCommandRequest.task_name[sizeof(request->data.StatsCommandRequest.task_name) - 1] = '\0';
        request->data.StatsCommandRequest.nr_pids = 0;
        for(int i = 3, j = 0; i < argc && i < 100;i++, j++){
            request->data.StatsCommandRequest.request_pids[j] = atoi(argv[i]);
            request->data.StatsCommandRequest.nr_pids++;
        }
        char path[MAX_RESPONSE_PATH_LENGTH];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->data.StatsCommandRequest.response_path,path,sizeof(request->data.StatsCommandRequest.response_path) - 1);
        request->data.StatsCommandRequest.response_path[sizeof(request->data.StatsCommandRequest.response_path) - 1] = '\0';
        
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
        int response;
        while((bytes_read = read(response_fd, &response, sizeof(int))) > 0){
            printStatsCommandResponse(argv[2], response);
        }

        close(main_channel_fd);
        close(response_fd);
        unlink(path);
    }
    else if(!strcmp(argv[1], "stats-uniq")){
        pid_t pid = getpid();
        Message request = malloc(sizeof(struct message));
        request->type = 10;
        request->data.StatsRequest.nr_pids = 0;
        for(int i = 2, j = 0; i < argc && i < 100;i++, j++){
            request->data.StatsRequest.request_pids[j] = atoi(argv[i]);
            request->data.StatsRequest.nr_pids++;
        }
        char path[MAX_RESPONSE_PATH_LENGTH];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->data.StatsRequest.response_path,path,sizeof(request->data.StatsRequest.response_path) - 1);
        request->data.StatsRequest.response_path[sizeof(request->data.StatsRequest.response_path) - 1] = '\0';
        
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
        char response[MAX_TASK_NAME_SIZE];
        while((bytes_read = read(response_fd, response, sizeof(char) * MAX_TASK_NAME_SIZE)) > 0){
            printf("%s\n", response);
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
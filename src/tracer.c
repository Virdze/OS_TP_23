#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "../headers/message.h"
#include "../headers/task.h"

#define MAIN_FIFO "../tmp/main_fifo"

//  =================================== ** GLOBALS ** ===================================

int main_channel_fd;

// =====================================================================================

double get_time_of_day(){
    return (double) (clock() / CLOCKS_PER_SEC);
}

void printStatusResponse(Message m){
    printf("================================\n");
    printf("PID: %d\n", m->msg.StatusResponse.process_pid);
    printf("Task(s): %s\n", m->msg.StatusResponse.task_name);
    printf("Time Elapsed: %fms\n", m->msg.StatusResponse.time_elapsed);
    printf("================================\n");
}

void executeSingle(char ** command){
    pid_t pid;
    int status;
    sleep(5);
    if((pid = fork()) < 0){
        perror("Error using fork()!\n");
        _exit(-1);
    }
    else if(!pid){
        execvp(command[0], command);
        _exit(1);
    }
    else wait(&status); 
}

int main(int argc, char * argv[]){
    if (argc < 2){
        printf("Usage:.\n");
        printf("1: ./tracer execute -u prog-a arg-1 (...) arg-n\n");
        printf("2: ./tracer execute -p prog-a arg-1 (...) arg-n | prog-b arg-1 (...) arg-n | prog-c arg-1 (...) arg-n\n");
        printf("3: ./tracer status\n");
        printf("4: ./tracer stats-time PID-1 PID-2 (...) PID-N\n");
        printf("5: ./tracer stats-command prog-a PID-1 PID-2 (...) PID-N\n");
        printf("6: ./tracer stats-uniq PID-1 PID-2 (...) PID-N\n");
        _exit(-1);
    }

    //  Abrir comunicação do lado do tracer 
    else if ((main_channel_fd = open(MAIN_FIFO,O_WRONLY)) < 0){
        perror("Error opening request fifo!\n");
        _exit(-1);
    }

    if (!strcmp(argv[1],"execute")){
        if (!strcmp(argv[2], "-u")){
            // Execução de um comando
            //  1. Recolher informação do pedido

            pid_t pid = getpid();
            char task_name[20];

            strcpy(task_name,argv[3]);
            char * command[argc - 2];
            command[0] = task_name;
            for(int i = 4, j = 1; i < argc ; i++, j++){
                command[j] = argv[i];
            }
            command[argc - 3] = NULL;   
    
            // 2. Criar Mensagem de início de execução
            
            Message start_message = malloc(sizeof(struct message));
            start_message->type = 1;
            start_message->msg.EStart.start = get_time_of_day();
            start_message->msg.EStart.process_pid = pid;
            strcpy(start_message->msg.EStart.task_name,task_name);        

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
            // 7. Enviar informação para o monitor
            write(main_channel_fd, end_message ,sizeof(struct message));
            
            // 8. Criar fifo para receber resposta 

            int response_fifo;
            if((response_fifo = mkfifo(path, 0666)) < 0){
                perror("Error creating response pipe.\n");
                _exit(-1);
            }

            // 9. Abrir comunicação
            int response_fd;
            if((response_fd = open(path, O_RDONLY)) < 0){
                perror("Error opening fifo!\n");
                _exit(-1);
            }
            
            // 10. Esperar comunicação no path enviado ao monitor

            ssize_t bytes_read; 
            Message response = malloc(sizeof(struct message));
            if((bytes_read = read(response_fd, &response->type, sizeof(int))) > 0){
                if(response->type == 5){
                    // 11. Apresentar resposta no STDOUT
                    printf("Ended in %fms\n", response->msg.time);
                }
            }
            else {
                printf("Something went wrong!\n");
            }

            // 12. Fechar escrita para o pipe por parte do cliente
            close(main_channel_fd);
            close(response_fd);
            unlink(path);
        }
        else if (!strcmp(argv[2], "-p")){
            // pipeline de programas
        }
    }
    else if(!strcmp(argv[1],"status")){
        // 1. Pid
        pid_t pid = getpid();

        // 2. Criar request
        Message request = malloc(sizeof(struct message));
        request->type = 3;
        request->msg.StatusRequest.clock = get_time_of_day();
        // 2.1 - Criar request path
        char path[50];
        sprintf(path,"../tmp/process_%d", pid);
        strncpy(request->msg.StatusRequest.response_path,path,sizeof(request->msg.StatusRequest.response_path) - 1);
        request->msg.StatusRequest.response_path[sizeof(request->msg.StatusRequest.response_path) - 1] = '\0';
        
        // 3. Enviar request ao monitor
        write(main_channel_fd, request,sizeof(struct message));
        

        // 4. Criar fifo
        int response_fifo;
        if((response_fifo = mkfifo(path, 0666)) < 0){
            perror("Error creating response pipe.\n");
            _exit(-1);
        }

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
            if(response->type == 4){
                // 7. Apresentar resposta no STDOUT
                //printf("%d\n",response->msg.StatusResponse.process_pid);
                printStatusResponse(response);
            }
        }

        // 8. Fechar descritores e destruir o fifo criado
        
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
    else ;

    return 0;
}
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

void executeSingle(char ** command){
    pid_t pid;
    int status;

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
    
            // 2. Abrir comunicação do lado do tracer 
            if((main_channel_fd = open(MAIN_FIFO,O_WRONLY)) < 0){
                perror("Error opening request fifo!\n");
                _exit(-1);
            }

            // 3. Criar Mensagem de início de execução
            
            Message start_message = malloc(sizeof(struct message));
            start_message->type = 1;
            start_message->msg.EStart = malloc(sizeof(struct execute_start));
            start_message->msg.EStart->start = get_time_of_day();
            start_message->msg.EStart->process_pid = pid;
            strcpy(start_message->msg.EStart->task_name,task_name);        

            // 4. Enviar informação para o monitor
            write(main_channel_fd, start_message, sizeof(struct message));
            // 5. Informar utilizador de início da execução
            printf("Running PID %d\n", pid);
        
            // 6. Executar programa
            executeSingle(command);

            // 7. Criar Mensagem de fim de Execução
            //double exec_time = (final_time - initial_time) * 1000;
            Message end_message = malloc(sizeof(struct message));
            end_message->type = 2;
            end_message->msg.EEnd = malloc(sizeof(struct execute_end));
            end_message->msg.EEnd->end = get_time_of_day();
            end_message->msg.EEnd->process_pid = pid;
            char path[20];
            sprintf(path,"../tmp/process_%d", pid);
            strcpy(end_message->msg.EEnd->response_path,path);

            // 8. Enviar informação para o monitor
            write(main_channel_fd, end_message ,sizeof(struct message));
            
            // 9. Criar fifo para receber resposta 

            int response_fifo;
            if((response_fifo = mkfifo(path, 0666)) < 0){
                perror("Error creating response pipe.\n");
                _exit(-1);
            }

            // 10. Abrir comunicação
            int response_fd;
            if((response_fd = open(path, O_RDONLY)) < 0){
                perror("Error opening fifo!\n");
                _exit(-1);
            }
            
            // 11. Esperar comunicação no path enviado ao monitor

            ssize_t bytes_read; 
            Message response = malloc(sizeof(struct message));
            if((bytes_read = read(response_fd, &response->type, sizeof(int))) > 0){
                if(response->type == 5){
                    // 12. Apresentar resposta no STDOUT
                    printf("Ended in %fms\n", response->msg.time);
                }
            }
            else {
                printf("Something went wrong!\n");
            }

            // 13. Fechar escrita para o pipe por parte do cliente
            close(main_channel_fd);
            close(response_fd);
            unlink(path);
        }
        else if (!strcmp(argv[2], "-p")){
            // pipeline de programas
        }
    }
    else if(!strcmp(argv[1],"status")){
        //Command
    }
    else if(!strcmp(argv[1],"END")){
        //End monitor (Temporario)
        write(main_channel_fd,"\0",sizeof("\0"));
        close(main_channel_fd);
    }
    else ;

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include "../headers/monitor.h"

#define MAX_BUFFER_SIZE 512
#define process_request_fifo "../tmp/process_request_fifo"
#define process_response_fifo "../tmp/process_response_fifo"
#define status_request_fifo "../tmp/status_request_fifo"
#define status_response_fifo "../tmp/status_response_fifo"


Process * done;
Process * requests;
int requests_count = 0,
    done_count = 0;
int process_request_fd, process_response_fd,
    status_request_fd, status_response_fd;


void addRequest(Process process){
    if(requests_count == 0){
        requests = malloc(sizeof(Process));
        requests[0] = malloc(sizeof(struct process));
        requests[0]->process_pid = process->process_pid;
        requests[0]->task_name = process->task_name;
        requests[0]->exec_time = process->exec_time;
    }
    else{
        requests_count++;
        requests = realloc(requests,sizeof(Process) * (requests_count));
        requests[requests_count] = malloc(sizeof(struct process));
        requests[requests_count]->process_pid = process->process_pid;
        requests[requests_count]->task_name = process->task_name;
        requests[requests_count]->exec_time = process->exec_time;
    }
}

void finishRequest(Process process){
    if(done_count == 0){
        done = malloc(sizeof(Process));
        done[0] = malloc(sizeof(struct process));
        done[0]->process_pid = process->process_pid;
        done[0]->task_name = process->task_name;
        done[0]->exec_time = process->exec_time;
    }
    else{
        for(int i = 0; i < requests_count ; i++){
            if(requests[i]->process_pid == process->process_pid){
                free(requests[i]);
                for (int j = i; j < requests_count - 1; j++){
                    requests[j] = requests[j+1];
                }
                requests_count--;
                realloc(requests,requests_count * sizeof(Process));
            }
        }

        done_count++;
        done = realloc(done,sizeof(Process) * (done_count));
        done[done_count] = malloc(sizeof(struct process));
        done[done_count]->process_pid = process->process_pid;
        done[done_count]->task_name = process->task_name;
        done[done_count]->exec_time = process->exec_time;
    }
}

void monitoring(char * path){
    int flag = 0;
    while(!flag){
        ssize_t readedBytes;
        Process process = malloc(sizeof(struct process));
        while((readedBytes = read(process, process, sizeof(struct process))) > 0){
            addRequest(process);
            // Provavelmente necessário adicionar filhos de forma ao monitor poder responder
            // a varios pedidos de varios clientes em simultaneo
        }
    }
}

int initFifos(){
    int prq_fifo;
    if((prq_fifo = mkfifo(process_request_fifo, 0600)) < 0){
        perror("Error creating process request fifo!\n");
        _exit(-1);
    }
    int prsp_fifo;
    if((prsp_fifo = mkfifo(process_response_fifo, 0600)) < 0){
        perror("Error creating process response fifo!\n");
    }
    int status_rq_fifo;
    if((status_rq_fifo = mkfifo(status_request_fifo, 0600)) < 0){
        perror("Error creating status request fifo!\n");
        _exit(-1);
    }
    int status_rsp_fifo;
    if((status_rsp_fifo = mkfifo(status_response_fifo, 0600)) < 0){
        perror("Error creating status response fifo!\n");
        _exit(-1);
    }

    if((process_request_fd = open(process_request_fifo, O_RDONLY)) < 0){
        perror("Error opening process request fifo\n");
        _exit(-1);
    }
    if((status_request_fd = open(status_request_fifo, O_RDONLY)) < 0){
        perror("Error opening status request fifo\n");
        _exit(-1);
    }
    if((process_response_fd = open(process_response_fifo, O_WRONLY)) < 0){
        perror("Error opening process response fifo\n");
        _exit(-1);
    }
    if((status_response_fd = open(status_response_fifo,O_WRONLY)) < 0){
        perror("Error opening status response fifo\n");
        _exit(-1);
    }
    return 0;
}

int main(int argc, char * argv[]){
    if(argc < 2){
        printf("Usage:\n");
        printf("1: ./monitor PIDS-folder\n");
    }
    else {
        if (initFifos() < 0)
            _exit(-1);
        initGlobals();
        char * folders_path = strdup(argv[1]);
        // Iniciar monitoring
        monitoring(folders_path);
    }
} 
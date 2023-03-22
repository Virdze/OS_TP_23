#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include "../headers/monitor.h"

#define MAX_BUFFER_SIZE 512
#define executePipe "../tmp/executePipe"
#define statusPipe "../tmp/statusPipe"

Process * done;
Process * requests;


Process * initStruct(){}
void addRequest(Process * process){}
void finishRequest(Process * process){}
void monitoring(char * path){}

int main(int argc, char * argv[]){
    if(argc < 2){
        printf("Usage:\n");
        printf("1: ./monitor PIDS-folder\n");
    }
    else {
        char * folders_path = strdup(argv[1]);
        // Iniciar monitoring
        monitoring(folders_path);
    }
} 
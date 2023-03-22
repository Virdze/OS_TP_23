#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_BUFFER_SIZE 512
#define executePipe "../tmp/executePipe"

int main(int argc, char * argv[]){
    if (argc < 2){
        printf("Usage:.\n");
        printf("1: ./tracer execute -u prog-a arg-1 (...) arg-n\n");
        printf("2: ./tracer execute -p prog-a arg-1 (...) arg-n | prog-b arg-1 (...) arg-n | prog-c arg-1 (...) arg-n\n");
        printf("3: ./tracer status\n");
        printf("4: ./tracer stats-time PID-1 PID-2 (...) PID-N\n");
        printf("5: ./tracer stats-command prog-a PID-1 PID-2 (...) PID-N\n");
        printf("6: ./tracer stats-uniq PID-1 PID-2 (...) PID-N\n");
    }
    else if (!strcmp(argv[1],"execute")){
        //Enviar informação para o servidor
        //  1. Abrir pipe com nome 

    }
    else if(!strcmp(argv[1],"status")){

    }
    else ;

    return 0;
}
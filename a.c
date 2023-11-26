#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

int main()
{
    pid_t pid = fork();
    if (pid == 0)
        system("cd; /bin/bash");
    else {
        wait(&pid);
        printf("finish\n");
    }
    return 0;
}
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

#define MAX_CLIENT 100
#define BUF_SIZE 1024

pthread_t clients[MAX_CLIENT];

void* shell(void* client_num)
{
    pid_t pid = fork();
    if (pid == 0) {
        system("cd; /bin/bash");
    }
    else {
        wait(&pid);
    }
}

int main(int ac, char** av)
{
    if (ac != 2)
        return 1;

    int serv_sock, clnt_sock, client_num = 0;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(av[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        return 1;
    if (listen(serv_sock, 5) == -1)
        return 1;

    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == -1)
            continue;

        pthread_create(clients[client_num], NULL, shell, client_num);
        ++client_num;
    }
}

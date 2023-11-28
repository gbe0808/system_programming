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

void err(char* str)
{
    printf("%s\n", str);
    exit(1);
}

void* shell(void* val)
{
    int clnt_sock = *(int*)val;
    pid_t pid = fork();
    if (pid < 0)
        err("fork error");
    else if (pid == 0) {
        dup2(clnt_sock, STDIN_FILENO);
        dup2(clnt_sock, STDOUT_FILENO);
        // close(STDIN_FILENO);
        // close(STDOUT_FILENO);

        system("cd; /bin/bash");
        exit(0);
    }
    else {
        close(clnt_sock);
        wait(&pid);
    }

    return NULL;
}

int main(int ac, char** av)
{
    if (ac != 2)
        return 1;

    int serv_sock, clnt_sock, client_num = 0;
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(av[1]));

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        err("bind error\n");
    if (listen(serv_sock, 5) == -1)
        err("listen error\n");

    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        printf("waiting accept\n");
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        printf("accepted\n");
        if (clnt_sock == -1)
            continue;

        pthread_create(&clients[client_num], NULL, shell, &client_num);
        ++client_num;
    }

    for (int i = 0; i < client_num; i++)
        pthread_join(clients[i], NULL);

    return 0;
}

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

#define BUF_SIZE 1024

void err(char* str)
{
    printf("%s\n", str);
    exit(1);
}

void* thread_input_to_socket(void* sock_ptr) {
    int sock = *(int *)sock_ptr;
    char command[BUF_SIZE];

    while (1) {
        memset(command, 0, sizeof(command));
        read(STDIN_FILENO, command, sizeof(command));
		command[strlen(command)] = '\n';
		printf("command: %s\n", command);
        if (write(sock, command, strlen(command)) == -1)
            err("socket write error");
    }

    return NULL;
}

void* thread_socket_to_output(void* sock_ptr) {
    int sock = *(int *)sock_ptr;
    char message[BUF_SIZE];

    while (1) {
        memset(message, 0, sizeof(message));
		write(1, "\n", 1);
        int len = read(sock, message, BUF_SIZE - 1);
        if (len == -1) {
            printf("read error\n");
            break;
        }
    }

    return NULL;
}

int main(int ac, char** av)
{
    int sock;
    char message[BUF_SIZE];
    struct sockaddr_in serv_addr;
    pthread_t input_thread, output_thread;

    if (ac != 3) {
        printf("Usage : %s <IP> <port>\n", av[0]);
        return 1;
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        err("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(av[1]);
    serv_addr.sin_port = htons(atoi(av[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        err("connect() error");

    pthread_create(&input_thread, NULL, thread_input_to_socket, &sock);
    // close(STDIN_FILENO);
    pthread_create(&output_thread, NULL, thread_socket_to_output, &sock);
    // close(STDOUT_FILENO);

    pthread_join(input_thread, NULL);
    pthread_join(output_thread, NULL);
    close(sock);
    return 0;
}

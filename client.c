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

int main(int ac, char** av)
{
    int sock;
    char message[BUF_SIZE];
    struct sockaddr_in serv_addr;

    if (ac != 3)
        return 1;

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        return 1;

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(av[1]);
    serv_addr.sin_port = htons(atoi(av[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        return 1;

    while (1) {
        memset(message, 0, sizeof(message));
        scanf("%s", message);
        if (message[BUF_SIZE - 1] != 0)
            break;

        if (write(sock, message, strlen(message)) == -1)
            break;

        int len = read(sock, message, BUF_SIZE - 1);
        if (len == -1)
            break;
        message[len] = 0;
        printf("%s\n", message);
    }
    close(sock);
    return 0;
}
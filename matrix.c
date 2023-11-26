#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "gpio.h"

// pin 번호 -> 수정 가능
const int CS[3] = { 10 };
#define DIN 12
#define CLK 14

#define CS_NUM 1

// D0~7에 설정된 값 그대로 이용하여 컨트롤 함
#define DECODE_MODE 0x09
#define DECODE_MODE_VAL 0x00
// 밝기 조정(0x00 ~ 0x0F)
#define INTENSITY 0x0a
#define INTENSITY_VAL 0x01
// 얼마나 많은 digit를 사용할지
#define SCAN_LIMIT 0x0b
#define SCAN_LIMIT_VAL 0x07
#define POWER_DOWN 0x0c 
#define POWER_DOWN_VAL 0x01
#define TEST_DISPLAY 0x0f
#define TEST_DISPLAY_VAL 0x00

#define HEIGHT 8
#define WIDTH 24

unsigned char matrix[3][8];

int init_GPIO()
{
    if (GPIOExport(CS[0]) == -1)
        return -1;
    if (GPIOExport(DIN) == -1)
        return -1;
    if (GPIOExport(CLK) == -1)
        return -1;
    if (GPIODirection(CS[0], OUT) == -1)
        return -1;
    if (GPIODirection(DIN, OUT) == -1)
        return -1;
    if (GPIODirection(CLK, OUT) == -1)
        return -1;
}

void init_matrix()
{
    for (int cs = 0; cs < CS_NUM; cs++) {
        init_MAX7219(CS[cs], DECODE_MODE, DECODE_MODE_VAL);
        init_MAX7219(CS[cs], INTENSITY, INTENSITY_VAL);
        init_MAX7219(CS[cs], SCAN_LIMIT, SCAN_LIMIT_VAL);
        init_MAX7219(CS[cs], POWER_DOWN, POWER_DOWN_VAL);
        init_MAX7219(CS[cs], TEST_DISPLAY, TEST_DISPLAY_VAL);
    }
}

// 직접적으로 DotMatrix 점등, 소등에 관여
void send_SPI_16bits(unsigned short data)
{
    for (int i = 16; i > 0; i--) {
        unsigned short mask = 1 << (i - 1);
        GPIOWrite(CLK, 0);
        GPIOWrite(DIN, (data & mask) != 0);
        GPIOWrite(CLK, 1);
    }
}

// MAX7219 제어
void send_MAX7219(int cs, unsigned short reg_number, unsigned short data)
{
    GPIOWrite(CS[cs], 1);
    send_SPI_16bits((reg_number << 8) + data);
    GPIOWrite(CS[cs], 0); // to latch
    GPIOWrite(CS[cs], 1);
}

void draw_dot(int row, int col)
{
    matrix[row][col] |= (1 << col);
}

void erase_dot(int row, int col)
{
    matrix[row][col] &= ~(1 << col);
}

void update_matrix()
{
    for (int i = 0; i < 8; i++) {
        for (int cs = 0; cs < CS_NUM; cs++) { // 일단은 DM 1개밖에 없음
            GPIOWrite(CS[cs], LOW);
            send_MAX7219(cs, 0, matrix[cs][i]);
            GPIOWrite(CS[cs], HIGH);
        }
    }
}

int main(int argc, char** argv)
{
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_MAX];

    if (argc != 3)
        return 1;

    if (init_GPIO() == -1)
        return 1;

    init_matrix();

    // sock = socket(PF_INET, SOCK_DGRAM, 0);
    // if (sock == -1)
    //     return 1;
    // memset(&serv_addr, 0, sizeof(serv_addr));
    // serv_addr.sin_family = AF_INET;
    // serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    // serv_addr.sin_port = htons(atoi(argv[2]));

    // if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    //     return 1;

    draw_dot(0, 1);
    int a = 1;
    while (1) {
        //read(sock, buffer, sizeof(buffer));

        erase_dot(a, (a + 1) | 0x07);
        a = (a + 1) | 0x07;
        draw_dot(a, (a + 1) | 0x07);

        update_matrix();

        usleep(1000 * 500);
    }

    return (0);
}

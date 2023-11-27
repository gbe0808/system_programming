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
#define DIN 12
#define CLK 18
#define CS0 10
const int CS[3] = {10};
#define CS_NUM 1
// D0~7에 설정된 값 그대로 이용하여 컨트롤 함
#define DECODE_MODE 0x09
#define DECODE_MODE_VAL 0x00
// 밝기 조정(0x00 ~ 0x0F)
#define INTENSITY 0x0a
// 얼마나 많은 digit를 사용할지
#define SCAN_LIMIT 0x0b
#define POWER_DOWN 0x0c
#define TEST_DISPLAY 0x0f

#define INPUT 0
#define OUTPUT 1

#define DISP_HEIGHT 8
#define DISP_WIDTH 24

// matrix[cs][slave][row]
unsigned char matrix[3][8];

void error_handling(char* message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

// SPI 통신으로 16bit 전송
void send_SPI_16bits(unsigned short data)
{
    for (int i = 16; i > 0; i--) {
        unsigned short mask = 1 << (i - 1);
        // Clock을 조절하면서 데이터 전송
        GPIOWrite(CLK, 0);
        GPIOWrite(DIN, (data & mask) != 0);
        GPIOWrite(CLK, 1);
    }
}

// MAX7219 Serial-Data Format(16bit)에 맞게 전송
// [address(8bit)][data(8bit)]
void send_MAX7219(unsigned short address, unsigned short data)
{
    send_SPI_16bits((address << 8) + data);
}

// 초기 설정값을 보내는 함수
void init_MAX7219(int cs, unsigned short address, unsigned short data)
{
    // Daisy-Chain방식으로 연결된 4개의 MAX7219에 데이터를 쓰기 위해 for문에서 64bit(16 * 4) 데이터를 보낸 후 CS핀을 HIGH로 활성화하였다.
    GPIOWrite(cs, LOW);
    send_MAX7219(address, data);
    GPIOWrite(cs, HIGH);
}


void intHandler(int dummy)
{
    send_MAX7219(POWER_DOWN, 0);
    exit(0);
}

void init()
{
    init_MAX7219(CS0, DECODE_MODE, 0x00);
    init_MAX7219(CS0, INTENSITY, 0x01);
    init_MAX7219(CS0, SCAN_LIMIT, 0x07);
    init_MAX7219(CS0, POWER_DOWN, 0x01);
    init_MAX7219(CS0, TEST_DISPLAY, 0x00);

}

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
                        send_MAX7219(i + 1, matrix[cs][i]);
                        GPIOWrite(CS[cs], HIGH);
                }
        }
}

int main(int argc, char** argv)
{
    int sock;
    struct sockaddr_in serv_addr;
    char buf[BUFFER_MAX];

    signal(SIGINT, intHandler);

    if (init_GPIO() < 0) {
        return 1;
    }

    init();

    sleep(1);

   /* if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }*/

    // 서버와 통신할 socket 생성
  /*  sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // 서버와 연결
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");
*/

	int a = 0;
	while (1) {
		memset(matrix, 0, sizeof(matrix));
		a = (a + 1) & 0x07;
		if (a & 1) {
			for (int i = 0; i < 4; i++)
				draw_dot(a, i);
		}
		else {
			for (int i=4;i<8;i++)
				draw_dot(a, i);
		}

		update_matrix();
		printf("a: %d\n", a);

		usleep(1000 * 500);
	}

    return (0);
}

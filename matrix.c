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

const int CS[3] = { 10 };
#define DIN 12
#define CLK 18

#define CS_NUM 1

#define DECODE_MODE 0x09
#define DECODE_MODE_VAL 0x00

#define INTENSITY 0x0a
#define INTENSITY_VAL 0x0a

#define SCAN_LIMIT 0x0b
#define SCAN_LIMIT_VAL 0x07
#define POWER_DOWN 0x0c 
#define POWER_DOWN_VAL 0x01
#define TEST_DISPLAY 0x0f
#define TEST_DISPLAY_VAL 0x00

#define HEIGHT 8
#define WIDTH 24

enum Directions {
	UP = 1,
	DOWN,
	LEFT,
	RIGHT
};

typedef struct s_player {
	short row, col;
	short health;
} Player;

Player player;

unsigned char matrix[3][8];

void send_MAX7219(unsigned short reg_number, unsigned short data);
	
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
	
	player.row = 0;
	player.col = 0;
	player.health = 3;
}

void ready_to_send(int cs, unsigned short address, unsigned short data)
{
	GPIOWrite(cs,LOW);
	send_MAX7219(address, data);
	GPIOWrite(cs,HIGH);
}


void init_matrix()
{
	for (int cs = 0; cs < CS_NUM; cs++) {
		ready_to_send(CS[cs], DECODE_MODE, DECODE_MODE_VAL);
		ready_to_send(CS[cs], INTENSITY, INTENSITY_VAL);
		ready_to_send(CS[cs], SCAN_LIMIT, SCAN_LIMIT_VAL);
		ready_to_send(CS[cs], POWER_DOWN, POWER_DOWN_VAL);
		ready_to_send(CS[cs], TEST_DISPLAY, TEST_DISPLAY_VAL);	
	}
	printf("init matrix fin\n");
}

void send_SPI_16bits(unsigned short data)
{
	for (int i = 16; i > 0; i--) {
		unsigned short mask = 1 << (i - 1);
		GPIOWrite(CLK, 0);
		GPIOWrite(DIN, (data & mask) != 0);
		GPIOWrite(CLK, 1);
	}
}

void send_MAX7219(unsigned short reg_number, unsigned short data)
{
	send_SPI_16bits((reg_number << 8) + data);
}

void draw_dot(int row, int col)
{
	matrix[0][row] |= (1 << col);
}

void erase_dot(int row, int col)
{
	matrix[0][row] &= ~(1 << col);
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

void move_player(int key)
{
	if (key == UP) {
		if (player.row <= 0)
			return;
		--player.row;
	}

	else if (key == DOWN) {
		if (player.row >= 6)
			return;
		++player.row;
	}

	else if (key == LEFT) {
		if (player.col <= 0)
			return;
		--player.col;
	}

	else if (key == RIGHT) {
		if (player.col >= 6)
			return;
		++player.col;
	}
}

void test_led()
{
	usleep(1000 * 1000);
	int a = 0;
	while (1) {
		memset(matrix, 0, sizeof(matrix));
		a = (a + 1) & 0x07;
		if (a & 1) {
			for (int i = 0; i < 4; i++)
				draw_dot(a, i);
		}
		else {
			for (int i = 4; i < 8; i++)
				draw_dot(a, i);
		}

		update_matrix();
		printf("a: %d\n", a);

		usleep(1000 * 500);
	}
}

void test_player_move() 
{
	int k;
	while (1) {
		memset(matrix, 0, sizeof(matrix));
		scanf("%d", &k);
		move_player(k);
		for (int i=player.row;i<player.row+2;i++) {
			for (int j=player.col;j<player.col+2;j++) {
				draw_dot(i, j);
			}
		}
		update_matrix();
	}
}

void test_socket(int sock)
{
	char buffer[BUFFER_MAX];

	while (1) {
		memset(matrix, 0, sizeof(matrix));
		int len = read(sock, buffer, sizeof(buffer));
		printf("cur: %c\n", buffer[0]);	
		move_player(buffer[0]-'0');
		for (int i=player.row;i<player.row+2;i++) {
			for (int j=player.col;j<player.col+2;j++) {
				draw_dot(i, j);
			}
		}
		update_matrix();
	}
}

void error_handling(char *str) {
	printf("%s\n", str);
	exit(1);
}

int main(int argc, char** argv)
{
	int sock;
	struct sockaddr_in serv_addr;

	if (argc != 3) {
		printf("Usage : %s <IP> <port>\n", argv[0]);
//		return 1;
	}

	if (argc == 3) {
		sock = socket(PF_INET, SOCK_STREAM, 0);
		if (sock == -1) 
			error_handling("socket() error");

		memset(&serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
		serv_addr.sin_port = htons(atoi(argv[2]));

		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
			error_handling("connect() error");

		printf("Connection established\n");
	}

	if (init_GPIO() == -1) {
		printf("init failed\n");
		return 1;
	}

	init_matrix();

	memset(matrix, 0, sizeof(matrix));
	update_matrix();
//	test_led();
	test_player_move();
//	test_socket(sock);

	return (0);
}


#include <pthread.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include "gpio.h"
#include "lst.h"

const int CS[3] = { 10, 0, 0 };
#define DIN 12
#define CLK 18

#define CS_NUM 3

#define DECODE_MODE 0x09
#define DECODE_MODE_VAL 0x00

#define INTENSITY 0x0a
#define INTENSITY_VAL 0x01

#define SCAN_LIMIT 0x0b
#define SCAN_LIMIT_VAL 0x07
#define POWER_DOWN 0x0c
#define POWER_DOWN_VAL 0x01
#define TEST_DISPLAY 0x0f
#define TEST_DISPLAY_VAL 0x00

#define HEIGHT 8
#define WIDTH 24

#define FAST_BULLET 200
#define SLOW_BULLET 300

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

typedef struct s_bullet {
        int lane, col, grade;
        long long halt_time;
} Bullet;

Player player;

unsigned char matrix[3][8];
unsigned char board[HEIGHT][WIDTH];
short finished;
int sock;
char msg[6] = {'0', '0', '0', '0', '0'};
t_list *bullets;
pthread_mutex_t string_mtx, board_mtx, matrix_mtx, bullet_mtx;

void ready_to_send(int cs, unsigned short address, unsigned short data);

int init_GPIO()
{
	for (int i = 0; i < CS_NUM; i++) {
		if (GPIOExport(CS[i]) == -1)
			return -1;
	}
    if (GPIOExport(DIN) == -1)
		return -1;
	if (GPIOExport(CLK) == -1)
		return -1;
	for (int i = 0; i < CS_NUM; i++) {
		if (GPIODirection(CS[i], OUT) == -1)
			return -1;
	}
	if (GPIODirection(DIN, OUT) == -1)
		return -1;
	if (GPIODirection(CLK, OUT) == -1)
		return -1;

    player.row = 0;
    player.col = 0;
    player.health = 3;

	return 1;
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
		matrix[0][0] = 0xC0;
		matrix[0][1] = 0xC0;
}

void init_mutex()
{
	pthread_mutex_init(&string_mtx, NULL);
	pthread_mutex_init(&board_mtx, NULL);
	pthread_mutex_init(&matrix_mtx, NULL);
	pthread_mutex_init(&bullet_mtx, NULL);
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

void ready_to_send(int cs, unsigned short address, unsigned short data)
{
	GPIOWrite(cs,LOW);
	send_MAX7219(address, data);
	GPIOWrite(cs,HIGH);
}

void draw_dot(int cs, int row, int col)
{
    matrix[cs][row] |= (1 << col);
}

void update_matrix()
{
	const int d[3] = {0, 2, 1};
	for (int cs = 0; cs < CS_NUM; cs++) {
		for (int i = 0; i < 8; i++) {
			GPIOWrite(CS[d[cs]], LOW);
			send_MAX7219(i + 1, matrix[cs][i]);
			GPIOWrite(CS[d[cs]], HIGH);
		}
	}
}

void board_to_matrix()
{
	int a;

	for (int i = 0; i < CS_NUM; i++) {
		a = 0;
		for (int j = 0; j < 8; j++) {
			for (int k = 0; k < 8; k++) {
				if (board[j][i * 8 + k])
					a |= 1;
				a <<= 1;
			}
			a >>= 1;
			matrix[i][j] = a;
		}
	}
}

void update_board(t_list *bullets)
{
	t_list *prev = bullets;
	t_list *cur = bullets->next;
	bool flag = false;

	memset(board, 0, sizeof(board));

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			board[player.row + i][player.col + j] |= 1;
		}
	}

	while (cur) {
		Bullet* cur_bullet = (Bullet *)cur->bullet;
		int lane = cur_bullet->lane * 2;
		for (int i=0;i<2;i++) {
			board[lane + i][cur_bullet->col] |= 2;
			if (board[lane + i][cur_bullet->col] == 3) {
				flag = true;
				prev->next = cur->next;
				lstdelone(cur);
				cur = prev;
				break;
			}
		}
		prev = cur;
		cur = cur->next;
	}
	
	pthread_mutex_lock(&matrix_mtx);
	board_to_matrix();
	update_matrix();
	pthread_mutex_unlock(&matrix_mtx);
	if (flag) {
		printf("Hit!\n");
//		write(sock, "1", 1);
	}
}


int move_player(short key)
{
        if (key == UP) {
                if (player.row <= 0)
                        return 0;
                --player.row;
        }

        else if (key == DOWN) {
                if (player.row >= 6)
                        return 0;
                ++player.row;
        }

        else if (key == LEFT) {
                if (player.col <= 0)
                        return 0;
                --player.col;
        }

        else if (key == RIGHT) {
                if (player.col >= 6)
                        return 0;
                ++player.col;
        }
		else
			return 0;
		printf("row, col: %d %d\n", player.row, player.col);
		pthread_mutex_lock(&board_mtx);
		update_board(bullets);
		pthread_mutex_unlock(&board_mtx);
		printf("Fin\n");
		return 1;
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
				draw_dot(0, a, i);
			for (int i = 5; i <= 7; i++)
				draw_dot(1, a, i);
		}
		else {
			for (int i = 4; i < 8; i++)
				draw_dot(0, a, i);
			for (int i = 1; i <= 3; i++)
				draw_dot(1, a, i);
		}
		for (int i=1;i<3;i++)
			draw_dot(2, a, i + 2);

		update_matrix();
		printf("a: %d\n", a);

		usleep(1000 * 200);
        }
}

/*
void test_player_move()
{
        int k;
        while (1) {
                memset(matrix, 0, sizeof(matrix));
                scanf("%d", &k);
                move_player(k);
                for (int i=player.row;i<player.row+2;i++) {
                        for (int j=player.col;j<player.col+2;j++) {
                                draw_dot(0, i, j);
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
                move_player(buffer[0] - '0');
                for (int i=player.row;i<player.row+2;i++) {
                        for (int j=player.col;j<player.col+2;j++) {
                                draw_dot(0, i, j);
                        }
                }
                update_matrix();
        }
}
*/

void error_handling(char *str) {
        printf("%s\n", str);
        exit(1);
}

long long get_millisec(char is_first)
{
	static struct timeval start;
	struct timeval cur;

	if (!is_first) {
		gettimeofday(&cur, NULL);
		return (cur.tv_sec - start.tv_sec) * 1000 + \
			(cur.tv_usec - start.tv_usec) / 1000;
	}
	return (gettimeofday(&start, NULL));
}


void *input_from_server(void *arg) {
	int sock = *(int *)arg;
	char tmp[6];

	while (1) {
		int len = read(0, tmp, sizeof(tmp));
		if (tmp[0] == '9' || len < 0) {
			finished = true;
			return NULL;
		}
		pthread_mutex_lock(&string_mtx);
		strcpy(msg, tmp);
		printf("msg: %s\n", msg);
		pthread_mutex_unlock(&string_mtx);
		// exit status -> return NULL
	}
	return NULL;
}

void *move_player_from_input(void *arg) {
	char ch;

	while (1) {
		usleep(10000);
		if (finished)
			return NULL;
		pthread_mutex_lock(&string_mtx);
		ch = msg[0];
		msg[0] = '0';
		pthread_mutex_unlock(&string_mtx);
		if (ch == '0')
			continue;
		ch -= '0';
		printf("ch: %d\n", ch);
		move_player((short)ch);
	}

	return NULL;
}

Bullet *make_new_bullet(int lane, int grade) {
	Bullet *new_bullet = malloc(sizeof(Bullet));

	new_bullet->lane = lane;
	new_bullet->grade = grade;
	new_bullet->col = WIDTH - 1;
	new_bullet->halt_time = get_millisec(0);

	return new_bullet;
}

void *make_bullet(void *arg) {
	char bul[5];

	while (1) {
		if (finished)
			return NULL;
		pthread_mutex_lock(&string_mtx);
		strcpy(bul, msg + 1);
		for (int i=1;i<=4;i++)
			msg[i] = '0';
		pthread_mutex_unlock(&string_mtx);

		int num = 0;
		Bullet *new_bullets[4];
		t_list *lst = NULL;
		t_list *cur;

		for (int i=0;i<4;i++) {
			if (bul[i] == '0')
				continue;
			new_bullets[num++] = make_new_bullet(i, bul[i] - '0');
		}

		if (num == 0)
			continue;

		lst = lstnew(new_bullets[0]);
		cur = lst;
		for (int i = 1; i < num; i++) {
			cur->next = lstnew(new_bullets[i]);
			cur = cur->next;
		}
		
		pthread_mutex_lock(&bullet_mtx);
		lstadd_back(&bullets, lst);
		pthread_mutex_unlock(&bullet_mtx);

		pthread_mutex_lock(&board_mtx);
		update_board(lst);	
		pthread_mutex_unlock(&board_mtx);
	}

	return NULL;
}

void *move_bullet(void *arg) {
	while (1) {
		short flag = 0;
		long long cur_millisec = get_millisec(0);

		if (finished)
			return NULL;
		pthread_mutex_lock(&bullet_mtx);

		t_list *prev = bullets;
		t_list *cur = bullets->next;
		while (cur) {
			Bullet *cur_bullet = (Bullet *)cur->bullet;
			int cool_time;
			if (cur_bullet->grade == 1)
				cool_time = FAST_BULLET;
			else if (cur_bullet->grade == 2)
				cool_time = SLOW_BULLET;

			if (cur_millisec - cur_bullet->halt_time > cool_time) {
				flag = 1;
				cur_bullet->halt_time = cur_millisec;
				if (--cur_bullet->col == -1) {
					prev->next = cur->next;
					lstdelone(cur);
					cur = prev;
				}
			}
			prev = cur;
			cur = cur->next;
		}
		pthread_mutex_unlock(&bullet_mtx);

		if (!flag)
			continue;
		pthread_mutex_lock(&board_mtx);
		update_board(bullets);
		pthread_mutex_unlock(&board_mtx);
	}

	return NULL;
}

int func()
{
	pthread_t input_from_server_t, move_player_from_input_t, make_bullet_t, move_bullet_t;

	init_mutex();

	pthread_create(&input_from_server_t, NULL, input_from_server, &sock);
	pthread_create(&move_player_from_input_t, NULL, move_player_from_input, NULL);
	pthread_create(&make_bullet_t, NULL, make_bullet, NULL);
	pthread_create(&move_bullet_t, NULL, move_bullet, NULL);

	while (1) {
		if (finished)
			break;
		usleep(100);
    }

	lstclear(&bullets);
	pthread_join(input_from_server_t, NULL);
	pthread_join(move_player_from_input_t, NULL);
	pthread_join(make_bullet_t, NULL);
	pthread_join(move_bullet_t, NULL);

	return 1;
}

int main(int argc, char** argv)
{
		struct sockaddr_in serv_addr;

        if (argc != 3) {
                printf("Usage : %s <IP> <port>\n", argv[0]);
//              return 1;
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

        memset(matrix, 0, sizeof(matrix));
        init_matrix();

        update_matrix();
		bullets = lstnew(NULL);
		get_millisec(1);

      test_led();
//      test_player_move();
//      test_socket(sock);


//		func();
        return (0);
}


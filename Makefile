CC		= gcc

SRCS    = gpio.c lst.c matrix.c

CFLAGS  = -lpthread
#CFLAGS	= -Wall -Wextra -Werror -lpthread

NAME    = dotMatrix

all:
	gcc ${SRCS} -o ${NAME} ${CFLAGS}

${NAME} : all

clean:	${RM} ${NAME}

.PHONY: clean

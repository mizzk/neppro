#
# Makefile for tcp_echo
#
MYLIBDIR=../mynet
MYLIB=-lmynet
CFLAGS=-I${MYLIBDIR} -L${MYLIBDIR}

all: echo_server echo_client

echo_server: echo_server.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

echo_client: echo_client.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

clean:
	${RM} *.o echo_server echo_client *~
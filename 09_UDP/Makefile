#
# Makefile for tcp_echo
#
MYLIBDIR=../mynet
MYLIB=-lmynet
CFLAGS=-I${MYLIBDIR} -L${MYLIBDIR}

all: echo_server echo_client echo_server1 echo_client1 echo_client2

echo_server: echo_server.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

echo_client: echo_client.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

echo_server1: echo_server1.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

echo_client1: echo_client1.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

echo_client2: echo_client2.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

clean:
	${RM} *.o echo_server echo_client echo_server1 echo_client1 echo_client2 *~
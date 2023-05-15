#
# Makefile for tcp_echo
#
MYLIBDIR=../mynet
MYLIB=-lmynet
CFLAGS=-I${MYLIBDIR} -L${MYLIBDIR}

all: echofork_server test05

echofork_server: echofork_server.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

test05: test05.o
	${CC} ${CFLAGS} -o $@ $^ ${MYLIB}

clean:
	${RM} *.o echofork_server test05 *~
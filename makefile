PROGS = serv client test
CFLAGS  = -Wall -g

CC:=gcc
LIBS += -pthread
LIBS += -lm
INC := -I ./
INC +=-I ./inc

depends_c = $(wildcard  ./src/*.c)				#找到所有的.c文件
depends_o = $(wildcard  ./*.o)				    #找到所有的.o文件
depends_h = $(wildcard  ./inc/*.h)				#找到所有的.h文件

all: ${PROGS}


test:${depends_c} test.c
	${CC} ${CFLAGS} -o $@ $^ ${INC} ${LIBS}
serv:${depends_c} serv.c
	${CC} ${CFLAGS} -o $@ $^ ${INC} ${LIBS}
client:${depends_c} client.c
	${CC} ${CFLAGS} -o $@ $^ ${INC} ${LIBS}


clean:
	rm -f $(depends_o) $(PROGS)
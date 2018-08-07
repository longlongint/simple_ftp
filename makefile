PROGS = server  client sizeof
CFLAGS  = -Wall -g

CC1:=/usr/local/arm/arm-2009q3/bin/arm-linux-gcc
CC2:=gcc
LIBS += -pthread
VPATH += ./inc/
INC := -I ./
INC +=-I ./inc

depends_c = $(wildcard  ./src/*.c)				#找到所有的.c文件
depends_o = $(wildcard  ./*.o)				    #找到所有的.o文件
depends_h = $(wildcard  ./inc/*.h)				#找到所有的.h文件

all: ${PROGS}

server:${depends_c} server.c
	${CC2} ${CFLAGS} -o $@ $^ ${INC} ${LIBS}
client:${depends_c} client.c
	${CC2} ${CFLAGS} -o $@ $^ ${INC} ${LIBS}
sizeof:sizeof.c
	${CC2} ${CFLAGS} -o $@ $^
clean:
	rm -f $(depends_o) $(PROGS)
# example demo
CC = gcc
CURRENT_PATH = $(shell pwd)
INCLUDE = ${CURRENT_PATH}/
# LIBPATH = ${CURRENT_PATH}/lib
LIBS = -lpthread

all: demo
demo: *.c
	${CC} $^ -I${INCLUDE} ${LIBS} -o $@
clean:
	-rm -f *.o demo
# all:$(subst .c,.o,$(wildcard *.c))
# %.o:%.c
# 	${CC} $^ -I${INCLUDE} ${LIBS} -o $@
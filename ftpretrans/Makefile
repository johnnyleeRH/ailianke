# example demo
CC = gcc
CURRENT_PATH = $(shell pwd)
INCLUDE = -I${CURRENT_PATH}/ \
					-I${CURRENT_PATH}/common/
# LIBPATH = ${CURRENT_PATH}/lib
LIBS = -lpthread

SRC = *.c common/*.c

all: demo
demo: $(SRC)
	${CC} $^ ${INCLUDE} ${LIBS} -o $@
clean:
	-rm -f *.o demo
# all:$(subst .c,.o,$(wildcard *.c))
# %.o:%.c
# 	${CC} $^ -I${INCLUDE} ${LIBS} -o $@
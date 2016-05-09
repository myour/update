# var define

INCDIR	:= ./
TARGET	:= update
SRCS	:= $(wildcard $(INCDIR)/*.c)
SRCS	:= $(notdir $(SRCS))
OBJS	:= $(SRCS:%.c=%.o)


# tool define
#CROSS  := /opt/tqimx6q_toolchain/gcc-4.6.2-glibc-2.13-linaro-multilib/bin/arm-linux-
CROSS  :=
CC      := ${CROSS}gcc
LD      := ld
LDFLAGS	:= -I$(INCDIR) #-Ttext 0
CFLAGS	:= -g -O2 -Wall ${LDFLAGS} -pthread


# export
# export CC LDFLAGS CFLAGS 

# rule
${TARGET}:$(OBJS)
	${CC} $(CFLAGS) $^  -o $@

%.o:%.c
	${CC} $(CFLAGS) -c $< -o $@


# clean
.PHONY:clean
clean:
	rm -rf *.o update





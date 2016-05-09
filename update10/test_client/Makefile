# 编译测试文件的 Makefile #

# 变量定义
CURDIR	:= ./
INCDIR	:= ./
#INCDIR := $(shell pwd)/include
SRC	:= $(wildcard $(CURDIR)*.c)
SRC	:= $(notdir $(SRC))
OBJS	:= $(SRC:%.c=%.o)
TARGET	:= client

# tools
CC	:= gcc
CFLAGS	:= -I$(INCDIR) -O2 -pthread

# rule
$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	-rm -rf *.o client


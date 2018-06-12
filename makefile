# makefile for dither project
# written by rockcarry

# 编译器定义
CC      = gcc
STRIP   = strip
CCFLAGS = -Wall -Os

# 所有的目标文件
OBJS = \
    dither.o \
    palette.o

# 所有的可执行目标
EXES = \
    dither.exe \
    palette.exe

# 编译规则
all : $(EXES)

%.o : %.c
	$(CC) $(CCFLAGS) -o $@ $< -c

%.exe : %.o
	$(CC) $(CCFLAGS) -o $@ $<
	$(STRIP) $@

clean :
	-rm -f *.o
	-rm -f *.exe

# rockcarry
# 2017.8.29
# THE END


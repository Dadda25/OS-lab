# 头文件区域
INLCUDES := -I../../include

# 选出所有后缀为.c或.S的文件,将其名称后缀替换为.o,作为输出目标
SOURCES = $(wildcard *.c *.S)
target = $(SOURCES:.c=.o)
target := $(target:.S=.o)

.PHONY: clean

# 定义了一些通用操作和参数
include ../../Common.mk

%.o: %.S
	$(CC) $(CFLAGS) $(INLCUDES) -c $<

%.o: %.c
	$(CC) $(CFLAGS) $(INLCUDES) -c $<

build: $(target)

clean:
	rm -f *.o *.d
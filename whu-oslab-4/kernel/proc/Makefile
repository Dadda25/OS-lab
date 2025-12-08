# 头文件区域
INLCUDES := -I../../include

# 选出所有后缀为.c或.S的文件,将其名称后缀替换为.o,作为输出目标
target = $(shell ls *.c *.S 2>/dev/null | awk '{gsub(/\.c|\.S/, ".o"); print $0}')

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
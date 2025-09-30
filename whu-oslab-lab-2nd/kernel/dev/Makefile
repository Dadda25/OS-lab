include ../../common.mk

# 源文件
SRCS = uart.c
OBJS = $(SRCS:.c=.o)

.PHONY: build clean

build: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.d

-include *.d
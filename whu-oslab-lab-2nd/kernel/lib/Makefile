include ../../common.mk

# 源文件
SRCS = str.c print.c spinlock.c
OBJS = $(SRCS:.c=.o)

.PHONY: build clean

build: $(OBJS)
	@echo "lib module built"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.da

-include *.d

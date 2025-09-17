include ../../common.mk

# 源文件
SRCS_C = main.c start.c
SRCS_S = entry.S
OBJS = $(SRCS_C:.c=.o) $(SRCS_S:.S=.o)

.PHONY: build clean

build: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.d

-include *.d
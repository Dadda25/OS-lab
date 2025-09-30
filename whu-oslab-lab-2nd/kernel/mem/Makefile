include ../../common.mk

SRCS = pmem.c kvm.c vmem.c
OBJS = $(SRCS:.c=.o)

.PHONY: build clean

build: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o

-include *.d

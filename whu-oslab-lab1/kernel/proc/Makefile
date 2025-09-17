include ../../common.mk

# 源文件 (暂时只编译空的目标文件，因为这些是TODO)
SRCS = 
OBJS = $(SRCS:.c=.o)

.PHONY: build clean

build: $(OBJS)
	@echo "proc module built (empty for now)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.d

-include *.d
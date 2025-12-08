INLCUDES := -I../../include

SOURCES = $(wildcard *.c *.S)
OBJS = $(patsubst %.c,%.o,$(wildcard *.c)) $(patsubst %.S,%.o,$(wildcard *.S))

.PHONY: clean build

include ../../Common.mk

%.o: %.c
	$(CC) $(CFLAGS) $(INLCUDES) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) $(INLCUDES) -c $< -o $@

build: $(OBJS)

clean:
	rm -f *.o *.d
include ../../Common.mk

# 头文件区域
INLCUDES := -I ../../include

modules := fat32 ext4 base

OBJS = procfs.o

.PHONY: build clean $(modules)

%.o: %.c
	$(CC) $(CFLAGS) $(INLCUDES) -c $<

$(modules):
	$(MAKE) build --directory=$@

build: $(modules) $(OBJS)

clean:
	for d in $(modules); \
		do \
			$(MAKE) clean --directory=$$d; \
		done
	rm -rf *~ *.o *.d
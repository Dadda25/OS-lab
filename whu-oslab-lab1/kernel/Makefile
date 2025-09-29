include ../common.mk

# 子目录模块（boot, lib, mem, proc, dev 等）
MODULES = $(shell ls -d */ | tr -d /)

# 内核目标文件
KERNEL_ELF = ../kernel-qemu
KERNEL_LD  = kernel.ld

# 单独的汇编文件（不在子目录里）
TRAMPOLINE = trampoline.o

.PHONY: build clean $(MODULES)

# 编译各个子目录
$(MODULES):
	$(MAKE) build --directory=$@

# 编译 trampoline.S
$(TRAMPOLINE): trampoline.S
	$(CC) $(CFLAGS) -c $< -o $@

# 链接内核
$(KERNEL_ELF): $(MODULES) $(TRAMPOLINE) $(KERNEL_LD)
	@echo "Linking kernel..."
	@OBJS=$$(find . -name "*.o" -type f); \
	if [ -z "$$OBJS" ]; then \
		echo "Error: No .o files found"; \
		exit 1; \
	fi; \
	echo "Found object files: $$OBJS"; \
	$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $(KERNEL_ELF) $$OBJS

build: $(MODULES) $(TRAMPOLINE) $(KERNEL_ELF)

clean:
	@for d in $(MODULES); do \
		$(MAKE) clean --directory=$$d; \
	done
	rm -f $(KERNEL_ELF) $(TRAMPOLINE)

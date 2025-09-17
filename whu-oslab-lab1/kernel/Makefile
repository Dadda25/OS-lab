include ../common.mk

MODULES = $(shell ls -d */ | tr -d /)
KERNEL_ELF = ../kernel-qemu
KERNEL_LD  = kernel.ld

.PHONY: build clean $(MODULES)

# 编译各个模块
$(MODULES):
	$(MAKE) build --directory=$@

# 收集所有 .o 文件并链接
$(KERNEL_ELF): $(MODULES) $(KERNEL_LD)
	@echo "Linking kernel..."
	@OBJS=$$(find . -name "*.o" -type f); \
	if [ -z "$$OBJS" ]; then \
		echo "Error: No .o files found"; \
		exit 1; \
	fi; \
	echo "Found object files: $$OBJS"; \
	$(LD) $(LDFLAGS) -T $(KERNEL_LD) -o $(KERNEL_ELF) $$OBJS

build: $(MODULES) $(KERNEL_ELF)

clean:
	@for d in $(MODULES); do \
		$(MAKE) clean --directory=$$d; \
	done
	rm -f $(KERNEL_ELF)
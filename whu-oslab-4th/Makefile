include Common.mk

# 用到的目录: 内核目录
KERN = kernel

# qemu需要的信息: 内核编译链接得到的可执行文件 + 硬件核心数量
KERNEL_ELF = kernel-rv
CPUNUM = 1  # 单核运行

.PHONY: clean $(KERN)

# 递归编译内核
$(KERN):
	$(MAKE) build --directory=$@

# 默认目标：编译内核
all: build

# 生成内核二进制文件（可选，根据需要保留）
build: $(KERN) 
	$(OBJCOPY) -O binary $(KERNEL_ELF) os.bin

# QEMU相关配置（仅保留必要参数）
QEMU     = qemu-system-riscv64
QEMUOPTS = -machine virt               # 使用virt虚拟平台
QEMUOPTS += -bios default              # 使用默认BIOS
QEMUOPTS += -kernel $(KERNEL_ELF)      # 加载内核镜像
QEMUOPTS += -m 128M                    # 分配128MB内存
QEMUOPTS += -smp $(CPUNUM)             # 启用1个核心
QEMUOPTS += -nographic                 # 无图形界面，使用终端输出
QEMUOPTS += -serial mon:stdio          # 确保串口输出到终端（避免无响应）

# qemu运行目标：仅依赖内核编译
qemu: $(KERN)
	$(QEMU) $(QEMUOPTS)

# 清除编译产物
clean: 
	$(MAKE) --directory=$(KERN) clean  # 清除内核编译产物
	rm -f $(KERNEL_ELF) os.bin .gdbinit  # 清除根目录产物

# 调试相关配置（可选，如需GDB调试保留）
GDBPORT = $(shell expr `id -u` % 5000 + 25000)  # 动态生成调试端口
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

# 生成GDB配置文件
.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

# 带GDB调试的启动目标
qemu-gdb: $(KERN) .gdbinit
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)  # -S：启动后暂停，等待GDB连接
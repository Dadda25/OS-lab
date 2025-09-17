# 工具链配置
TOOLPREFIX = riscv64-linux-gnu-
CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# 编译选项
CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-stack-protector -fno-pie -no-pie
CFLAGS += -I ../../include

# 汇编选项
ASFLAGS = -I ../../include

# 链接选项
LDFLAGS = -z max-page-size=4096
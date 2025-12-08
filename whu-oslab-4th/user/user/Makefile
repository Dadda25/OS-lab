include ../Common.mk

fs_img  = ../sdcard.img      # 文件系统映像
user_ld = ../linker/user.ld  # 链接文件
mountspace = ../sdcard       # 文件系统挂载
modules = lib src            # 模块

PG = $(shell ls ./src/*.out ./src/*.txt)

.PHONY: build clean $(modules) $(fs_img)

$(modules):
	$(MAKE) build --directory=$@

# 生成fs.img

# 基于EXT4
$(fs_img): ${PG}
	mkdir -p fs
	sudo cp -r ${PG} fs
	for file in ./fs/*.out; do \
        mv "$$file" "$${file%.out}"; \
    done
	chmod -R 777 fs/*
	dd if=/dev/zero of=${fs_img} count=2048 bs=1M
	mkfs.ext4 ${fs_img}
	sudo mount ${fs_img} ${mountspace}
	sudo cp -r fs/* ${mountspace}
	sudo umount ${mountspace}

# 基于FAT32
$(fs_img): ${PG}
	mkdir -p fs
	sudo cp -r ${PG} fs
	for file in ./fs/*.out; do \
        mv "$$file" "$${file%.out}"; \
    done
	dd if=/dev/zero of=${fs_img} bs=1M count=512
	mkfs.vfat -F 32 ${fs_img}
	sudo mount ${fs_img} ${mountspace}
	sudo cp -r fs/* ${mountspace}
	sudo umount ${mountspace}

#常用的命令
build: ${modules} ${fs_img}

clean: 
	rm -rf fs
	rm -rf ${fs_img}

# 通过修改init的依赖，可以构建不同的初始化进程
init: initcode.c
	$(CC) $(CFLAGS) -I ./include -march=rv64g -nostdinc -c initcode.c -o initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o initcode.out initcode.o
	$(OBJCOPY) -S -O binary initcode.out initcode
	xxd -i initcode > ../include/proc/initcode.h
	rm -f initcode initcode.d initcode.o initcode.out

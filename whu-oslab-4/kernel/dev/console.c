#include <stdarg.h>
#include "dev/console.h"
#include "dev/uart.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "fs/fat32_file.h"
#include "fs/ext4_file.h"
#include "lib/str.h"

#define BACKSPACE 0x100
#define Ctrl(x) ((x)-'@')  // control-x

// 特殊字符处理：
// '\n' 一行结束
// ctrl-d    end of file
// ctrl-h    backspace
// ctrl-u    kill line
// ctro-p    print process list

console_t cons;
spinlock_t write_lk;

extern fat32_dev_t fat32_devlist[NDEV];
extern ext4_dev_t  ext4_devlist[NDEV];

// 读不出来东西
int null_read(bool user_dst, uint64 dst, uint32 n)
{
    return 0;
}

// 写多少吃多少
int null_write(bool user_src, uint64 src, uint32 n)
{
    return n;
}

int zero_read(bool user_dst, uint64 dst, uint32 n)
{
    uint8 mem[512];
    memset(mem, 0, sizeof(mem));
    for(uint32 i = 0; i < n; i += 512)
        if(vm_copyout(user_dst, dst, mem, min(512, n - i)) < 0)
            return -1;
    return n;
}

void cons_init()
{
    uart_init();
    spinlock_init(&cons.lk,"console");
    spinlock_init(&write_lk, "cons write lock");
#ifdef FS_FAT32
    fat32_devlist[CONSOLE].read = cons_read;     // 设备文件的读函数
    fat32_devlist[CONSOLE].write = cons_write;   // 设备文件的写函数
#else
    ext4_devlist[CONSOLE].read = cons_read;      // 设备文件的读函数
    ext4_devlist[CONSOLE].write = cons_write;    // 设备文件的写函数
    ext4_devlist[DEVNULL].read = null_read;
    ext4_devlist[DEVNULL].write = null_write;
    ext4_devlist[DEVZERO].read = zero_read;
#endif
}

int cons_write(bool user_src, uint64 src, uint32 n)
{
    int ret, i;
    char c;

    spinlock_acquire(&write_lk);
    for(i=0; i<n; i++) {
        ret = vm_copyin(user_src, &c, src+i, 1);
        if(ret == -1) break;
        uart_putc(c);
    }
    spinlock_release(&write_lk);
    return n;
}

int cons_read(bool user_src, uint64 dst, uint32 n)
{
    uint32 tar = n, ret = 0;
    char c;
    spinlock_acquire(&cons.lk);

    while(n > 0) {
        // 若buf为空,则读者休眠
        while(cons.r == cons.w) {
            if(proc_iskilled(myproc())) {
                spinlock_release(&cons.lk);
                return -1;
            }
            proc_sleep(&cons.r, &cons.lk);
        }
        // 读取buf中的一个字符
        c = cons.buf[cons.r % INPUT_BUF_SIZE];
        cons.r++;
        // 如果读到文件末尾
        if(c == Ctrl('D')) {
            if(n < tar) cons.r--;
            break;
        }
        // 字符传递
        char tmp = c;
        ret = vm_copyout(user_src, dst, &tmp, 1);
        if(ret == -1) break;
        // 本轮工作结束，指针向前移动，任务量-1
        dst++;
        n--;
        // 如果遇到'\n'则跳出
        if(c == '\n') break;
    }

    spinlock_release(&cons.lk);
    return tar - n;
}

void cons_puts(char* s)
{
    for(int i=0; s[i]!='\0'; i++)
        cons_putc((int)s[i]);
}

void cons_putc(int c)
{
    // 删除一个字符
    if(c == BACKSPACE) {
        uart_putc_sync('\b'); // 回退        aa|b
        uart_putc_sync(' ');  // 空格覆盖    aa |
        uart_putc_sync('\b'); // 回退        aa|
    } else {
        uart_putc_sync((char)c);
    }
}

void cons_intr(int c)
{
    spinlock_acquire(&cons.lk);

    switch (c) 
    {
        case Ctrl('P'): // 输出进程信息
            proc_print();
            break;
        case Ctrl('U'): // 删去当前行
            while(cons.e != cons.w) {
                if(cons.buf[(cons.e-1) % INPUT_BUF_SIZE] == '\n') break;
                cons.e--;
                cons_putc(BACKSPACE);
            }
            break;
        case Ctrl('H'): // backspace
        case '\x7f':    // delete key
            if(cons.e != cons.w) {
                cons.e--;
                cons_putc(BACKSPACE);
            }
            break;
        default:
            if(c != 0 && cons.e - cons.r < INPUT_BUF_SIZE) {
                if(c == '\r') c = '\n';
                cons_putc(c);
                cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;
                if(c == '\n' || c == Ctrl('D') || cons.e - cons.r == INPUT_BUF_SIZE) {
                    cons.w = cons.e;
                    proc_wakeup(&cons.r);
                }
            }
            break;
    }
    spinlock_release(&cons.lk);
}
#include "lib/print.h"
#include "dev/uart.h"
#include "dev/console.h"
#include "lock/lock.h"
#include <stdarg.h>

const char digit[] = "0123456789ABCDEF";

// 保证一次printf可以完整输出，不会在中途发生进程切换
static spinlock_t print_spinlock;

volatile int panicked = 0;

static void print_10(uint64 xx, bool islong, bool isunsigned) 
{    
    char out[30];
    int index = 29;

    out[index--] = 0; // 字符串终止符

    if (xx == 0) {
        out[index--] = '0';
        goto END;
    }

    if (islong && isunsigned) {
        // 无符号64位整数
        while (xx != 0) {
            out[index--] = (xx % 10) + '0';
            xx = xx / 10;
        }      
    } else if (islong) {
        // 有符号64位整数
        int64 tmp = (int64)xx;
        bool neg = false;
        if (tmp < 0) {
            neg = true;
            tmp = -tmp;
        }
        while (tmp != 0) {
            out[index--] = (tmp % 10) + '0';
            tmp = tmp / 10;
        }
        if (neg) out[index--] = '-';
    } else if (isunsigned) {
        // 无符号32位整数
        uint32 tmp = (uint32)xx;
        while (tmp != 0) {
            out[index--] = (tmp % 10) + '0';
            tmp = tmp / 10;
        }
    } else {
        // 有符号32位整数
        int32 tmp = (int32)xx;
        bool neg = false;
        if (tmp < 0) {
            neg = true;
            tmp = -tmp;
        }
        while (tmp != 0) {
            out[index--] = (tmp % 10) + '0';
            tmp = tmp / 10;
        }
        if (neg) out[index--] = '-';
    }
END:
    cons_puts(out + index + 1);
}

static void print_16(uint64 xx, bool islong) 
{    
    int index = 29;
    char out[30];
    out[index--] = 0;

    if (islong) {
        // 64位十六进制（如指针%p）
        for (int i = 0; i < 16; i++) {
            out[index--] = digit[xx % 16];
            xx = xx >> 4;
            if ((i + 1) % 4 == 0)
                out[index--] = '-'; // 每4位加分隔符
        }
    } else {
        // 32位十六进制（%x）
        uint32 tmp = (uint32)xx;
        for (int i = 0; i < 8; i++) {
            out[index--] = digit[tmp % 16];
            tmp = tmp >> 4;
            if ((i + 1) % 4 == 0) 
                out[index--] = '-'; // 每4位加分隔符
        }   
    }
    out[index + 1] = 'x';
    out[index--] = '0'; // 前缀"0x"
    cons_puts(out + index + 1);
}

static void print_str(char* s) 
{
    if (s == NULL) 
        cons_puts("NULL");
    else 
        cons_puts(s);
}

/* 外部函数 */

void print_init() 
{
    spinlock_init(&print_spinlock, "print");
}

/*
    支持的格式符：
    %d        32位有符号整数
    %ld       64位有符号整数
    %u        32位无符号整数
    %lu       64位无符号整数
    %x        32位十六进制
    %p        64位十六进制（指针）
    %c        字符
    %s        字符串
*/
int printf(const char* fmt, ...) 
{
    if (fmt == NULL) { 
        panic("try to printf null");
        return -1;
    }

    va_list vl;                 // 参数列表
    bool flag = false;          // 标记上一个字符是'%'
    bool long_flag = false;     // 长整数标记（%ld/%lu）
    char c;                     // 当前解析的字符

    va_start(vl, fmt);
    spinlock_acquire(&print_spinlock); // 加锁保证打印原子性

    for (int i = 0; fmt[i] != 0; i++) {
        c = fmt[i];
        if (!flag) {
            // 未处于格式解析状态，直接打印字符
            if (c == '%') {
                flag = true;          // 进入格式解析状态
                long_flag = false;    // 重置长整数标记
            } else {
                uart_putc(c);
            }
            continue;
        }

        // 处理格式符
        switch (c) {
            case 'u':
                // 无符号整数：%u（32位）或%lu（64位）
                if (long_flag) {
                    // %lu：64位无符号
                    print_10(va_arg(vl, uint64), true, true);
                } else {
                    // %u：32位无符号
                    print_10((uint64)va_arg(vl, uint32), false, true);
                }
                flag = false; // 解析完成，退出格式状态
                break;
            case 'l':
                // 长整数标记（后续可能接d/u）
                long_flag = true;
                break;
            case 'd':
                // 有符号整数：%d（32位）或%ld（64位）
                if (long_flag) {
                    print_10(va_arg(vl, int64), true, false);
                } else {
                    print_10((uint64)va_arg(vl, int32), false, false);
                }
                flag = false;
                break;
            // 其他case保持不变...
            case 'x':
                print_16((uint64)va_arg(vl, uint32), false);
                flag = false;
                break;
            case 'p':
                print_16(va_arg(vl, uint64), true);
                flag = false;
                break;
            case 's':
                print_str(va_arg(vl, char*));
                flag = false;
                break;
            case 'c':
                uart_putc((char)va_arg(vl, int));
                flag = false;
                break;
            default:
                uart_putc(c);
                flag = false;
                break;
        }
    }
    
    spinlock_release(&print_spinlock);
    va_end(vl);
    return 0;
}
void panic(const char* warning)
{
    printf("panic! %s\n", warning);
    panicked = 1;
    while (1); // 死循环，防止继续执行
}

void assert(bool condition, const char* warning)
{
    if (!condition) {
        if (warning != NULL) {
            panic(warning);
        } else {
            panic("assert failed");
        }
    }
}
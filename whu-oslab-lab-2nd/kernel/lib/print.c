#include <stdarg.h>
#include <stdbool.h>
#include "lib/print.h"
#include "lib/lock.h"
#include "dev/uart.h"

volatile int panicked = 0;

static spinlock_t print_lk;
static char digits[] = "0123456789abcdef";

static void
printint(long xx, int base, int sign)
{
    char buf[32];
    int i = 0;
    unsigned long x;

    if (sign && xx < 0) {
        x = -xx;
    } else {
        x = xx;
    }

    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign && xx < 0)
        buf[i++] = '-';

    while (--i >= 0)
        uart_putc(buf[i]);
}

void print_init(void)
{
    spinlock_init(&print_lk, "print");
}

// Print to the console. only understands %d, %x, %p, %s.
void printf(const char *fmt, ...)
{
    va_list ap;
    const char *p;
    //int c;
    char *s;

    if (panicked) {
        // 如果 panic 状态，不再输出，避免乱序
        return;
    }

    spinlock_acquire(&print_lk);

    va_start(ap, fmt);
    for (p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }
        p++;
        if (*p == 0) break;

        switch (*p) {
        case 'd':
            printint(va_arg(ap, int), 10, 1);
            break;
        case 'x':
        case 'p':
            printint(va_arg(ap, unsigned long), 16, 0);
            break;
        case 's':
            s = va_arg(ap, char*);
            if (s == 0) s = "(null)";
            while (*s) uart_putc(*s++);
            break;
        case '%':
            uart_putc('%');
            break;
        default:
            // 未知格式，原样输出
            uart_putc('%');
            uart_putc(*p);
            break;
        }
    }
    va_end(ap);

    spinlock_release(&print_lk);
}

void panic(const char *s)
{
    panicked = 1;  // 停止进一步 printf
    printf("panic: %s\n", s);

    // 死循环，避免继续执行
    while (1) {
        asm volatile("wfi"); // 等待中断，降低功耗
    }
}

void assert(bool condition, const char* warning)
{
    if (!condition) {
        panic(warning);
    }
}

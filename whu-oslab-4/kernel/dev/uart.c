// low-level driver routines for 16550a UART
#include "memlayout.h"
#include "dev/uart.h"
#include "dev/console.h"
#include "lock/lock.h"
#include "proc/proc.h"

#define RHR  0      // receive holding reg
#define THR  0      // transmit holding reg
#define IER  1      // interrupt enable reg
#define IER_RX_ENABLE   (1<<0)
#define IER_TX_ENABLE   (1<<1)
#define FCR  2      // FIFO control reg
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR  (3<<1)
#define ISR  2      // interrupt status reg
#define LCR  3      // line control reg
#define LCR_EIGHT_BITS  (3<<0) // special mode to set word len
#define LCR_BAUD_LATCH  (1<<7) // special mode to set baud rate
#define LSR 5       // line status reg
#define LSR_RX_READY    (1<<0) // input is ready
#define LSR_TX_IDLE     (1<<5) // THR可以接受字符

#define UART_REG(reg)     ((volatile uint8 *)(UART_BASE + reg))
#define read_reg(reg)     (*(UART_REG(reg)))
#define write_reg(reg, v) (*(UART_REG(reg)) = (v))


#define UART_TX_SIZE 32
struct uart_tx {
    char buf[UART_TX_SIZE];
    uint64 r;
    uint64 w;
    spinlock_t lk;
} uart_tx;

extern volatile int panicked; // in lib/print.c


static void uart_start()
{
    char c;
    // buffer为空则退出
    while(uart_tx.r != uart_tx.w) {
        // THR不可接受字符则退出
        if( (read_reg(LSR) & LSR_TX_IDLE) == 0) break;
        // 从buf中读出一个字符
        c = uart_tx.buf[uart_tx.r++ % UART_TX_SIZE];
        // 通知写者有空位了
        proc_wakeup(&uart_tx.r);
        // 真正的传输过程
        write_reg(THR, c);
    }
}

void uart_init()
{
    write_reg(IER, 0x00); // 关闭中断

    write_reg(LCR, LCR_BAUD_LATCH); // 进入准备设置波特率的模式
    write_reg(0, 0x03); // LSB of baud rate of 38.4K
    write_reg(1, 0x00); // MSB of baud rate of 38.4K

    write_reg(LCR, LCR_EIGHT_BITS); //进入准备设置字长的模式,并设置为8bit

    write_reg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR); // 清空并使能FIFO队列

    //write_reg(IER, IER_TX_ENABLE | IER_RX_ENABLE); // 使能收发队列
    write_reg(IER, IER_RX_ENABLE); // 只允许接收中断触发
    spinlock_init(&uart_tx.lk, "uart"); 
}

void uart_putc(int c)
{
    spinlock_acquire(&uart_tx.lk);

    if(panicked) {for(;;);}  // 发生panic, 卡死在这里

    while(uart_tx.w == uart_tx.r + UART_TX_SIZE)   // tx队列已满,写者写入失败
        proc_sleep(&uart_tx.r, &uart_tx.lk);       // 等待读者去读
    
    // 写入buf
    uart_tx.buf[uart_tx.w++ % UART_TX_SIZE] = (char)c; 
    
    // 将buf里的内容通过uart传输出去
    uart_start();

    spinlock_release(&uart_tx.lk);
}

void uart_puts(char* s)
{
    while(*s) {
        uart_putc((int)(*s));
        s++;
    }
}

int uart_getc(void)
{
    if(read_reg(LSR) & 0x01) {
        return read_reg(RHR);
    } else {
        return -1;
    }
}

// uart 输入中断处理函数
void uart_intr(void)
{
    int c;
    while(1) {
        c = uart_getc();
        if(c == -1) break;
        else cons_intr(c);
    }

    spinlock_acquire(&uart_tx.lk);
    uart_start();
    spinlock_release(&uart_tx.lk);
}

// 带开关中断的uart_putc
void uart_putc_sync(int c)
{
    push_off();
    
    // 这两步检查是对THR写之前的必须操作
    if(panicked) {for(;;);}
    while ((read_reg(LSR) & LSR_TX_IDLE) == 0);
    write_reg(THR, c);

    pop_off();
}
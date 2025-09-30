#include "dev/uart.h"
#include "memlayout.h"

// UART register offsets
#define UART_THR    0  // Transmit Holding Register
#define UART_LSR    5  // Line Status Register
#define UART_LSR_THRE (1 << 5)  // Transmit Holding Register Empty

static volatile char *uart_base = (volatile char *)UART_BASE;

void uart_init(void) {
    // QEMU's UART is already initialized, so we don't need to do much
    // In real hardware, we would configure baud rate, etc.
}

void uart_putc_sync(int c) {
    // Wait until transmit holding register is empty
    while (!(uart_base[UART_LSR] & UART_LSR_THRE));
    
    // Send the character
    uart_base[UART_THR] = c;
}

void uart_putc(int c) {
    uart_putc_sync(c);
}


void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s);
        s++;
    }
}

int uart_getc_sync(void) {
    // This is a minimal implementation - just return -1 for now
    // In a real implementation, you'd check if data is available
    return -1;
}

void uart_intr(void) {
    // Interrupt handler - empty for now
}
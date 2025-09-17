#include "common.h"
#include "riscv.h"
#include "memlayout.h"
#include "dev/uart.h"

// 声明 main 函数
void main(void);

void start(int cpu_id) {
    // Initialize UART (only CPU 0 does this)
    if (cpu_id == 0) {
        uart_init();
    }
    
    // Wait a bit for UART to be ready and for CPU 0 to initialize
    for (volatile int i = 0; i < 100000; i++);
    
    // Each CPU prints its boot message
    if (cpu_id == 0) {
        uart_puts("cpu 0 is booting!\n");
    } else if (cpu_id == 1) {
        uart_puts("cpu 1 is booting!\n");
    }
    
    // Call main function (only CPU 0 continues to main)
    if (cpu_id == 0) {
        main();
    }
    
    // Other CPUs just wait
    while (1) {
        wfi();  // Wait for interrupt
    }
}
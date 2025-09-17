#include "riscv.h"
#include "dev/uart.h"
#include "common.h"

void main(void) {
    // Main kernel initialization
    // For now, just keep the system running
    
    while (1) {
        // Main kernel loop
        // In a real OS, this would be the scheduler
        wfi();  // Wait for interrupt to save power
    }
}
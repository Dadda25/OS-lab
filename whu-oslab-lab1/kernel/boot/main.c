#include "riscv.h"
#include "dev/uart.h"
#include "common.h"
#include "mem/pmem.h"
#include "lib/str.h"
#include "lib/print.h"

volatile static int started = 0;
volatile static int over_1 = 0, over_2 = 0;

static int* mem[1024];

void main(void)
{
    int cpuid = r_tp();

    if (cpuid == 0) {
        uart_puts("=== CPU 0 starting ===\n");
        pmem_init();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

        // CPU0 分配前 512 页
        for (int i = 0; i < 512; i++) {
            mem[i] = (int*)pmem_alloc(true);
            memset(mem[i], 1, PGSIZE);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
        }
        printf("cpu %d alloc over\n", cpuid);
        over_1 = 1;

        // 等待 CPU1 分配完
        while (over_1 == 0 || over_2 == 0)
            ;

        // 释放前 512 页
        for (int i = 0; i < 512; i++) {
            pmem_free((uint64)mem[i], true);
        }
        printf("cpu %d free over\n", cpuid);

    } else {
        while (started == 0)
            ; // 等待 CPU0 初始化完成
        __sync_synchronize();

        printf("cpu %d is booting!\n", cpuid);

        // CPU1 分配后 512 页
        for (int i = 512; i < 1024; i++) {
            mem[i] = (int*)pmem_alloc(true);
            memset(mem[i], 1, PGSIZE);
            printf("mem = %p, data = %d\n", mem[i], mem[i][0]);
        }
        printf("cpu %d alloc over\n", cpuid);
        over_2 = 1;

        // 等待 CPU0 分配完
        while (over_1 == 0 || over_2 == 0)
            ;

        // 释放后 512 页
        for (int i = 512; i < 1024; i++) {
            pmem_free((uint64)mem[i], true);
        }
        printf("cpu %d free over\n", cpuid);
    }

    // 不退出，继续等待
    while (1) {
        wfi();
    }
}

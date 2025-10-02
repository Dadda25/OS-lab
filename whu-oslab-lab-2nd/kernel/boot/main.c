#include "riscv.h"
#include "dev/uart.h"
#include "common.h"
#include "mem/pmem.h"
#include "mem/kvm.h"
#include "lib/str.h"
#include "lib/print.h"

volatile static int started = 0;

#define TEST_MEM_PAGES 5
uint64 mem[TEST_MEM_PAGES];

void main(void)
{
    int cpuid = r_tp();

    if (cpuid == 0) {
        print_init();
        pmem_init();
        kvminit();       // 创建 kernel_pagetable 并完成映射
        kvminithart();   // 启用分页

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

        // 分配测试用内存页
        pgtbl_t test_pgtbl = create_pagetable();
        for (int i = 0; i < TEST_MEM_PAGES; i++)
            mem[i] = (uint64)pmem_alloc(true);

        printf("\ntest-1\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_R);
        vm_mappages(test_pgtbl, PGSIZE * 10, mem[1], PGSIZE / 2, PTE_R | PTE_W);
        vm_mappages(test_pgtbl, PGSIZE * 512, mem[2], PGSIZE - 1, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, PGSIZE * 512 * 512, mem[3], PGSIZE, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, MAXVA - PGSIZE, mem[4], PGSIZE, PTE_W);
        vm_print(test_pgtbl);

        printf("\ntest-2\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_W);
        vm_unmappages(test_pgtbl, PGSIZE * 10, PGSIZE, true);
        vm_unmappages(test_pgtbl, PGSIZE * 512, PGSIZE, true);
        vm_print(test_pgtbl);

    } else {
        // CPU1 等待 CPU0 初始化完成
        while (started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
    }

    // 不退出，保持 CPU 等待
    while (1) {
        wfi();
    }
}

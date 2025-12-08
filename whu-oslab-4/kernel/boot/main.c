#include "lib/print.h"
#include "mem/pmem.h"
#include "mem/vmem.h"
#include "proc/cpu.h"
#include "trap/trap.h"
#include "dev/console.h"
#include "dev/plic.h"
#include "dev/vio.h"
#include "dev/timer.h"
#include "fs/base_buf.h"
#include "fs/procfs.h"
#include "riscv.h"
volatile static bool first = true;        // 当前核心是否是第一个启动的核心
volatile static bool other = false;       // 其他核心是否可以启动
// 在main.c中添加ticks的外部声明


// 系统初始化 + 第一个用户态进程 + 调度启动
void main()
{
    if(first) {
        first = false;
        __sync_synchronize();

        cons_init();        // 中控台
        print_init();       // 标准输出

        pmem_init(false);   // 物理内存
        uvm_init();         // user mem init
        kvm_init();         // kernel mem init
        kvm_inithart();     // 开启分页
        proc_init();        // 进程列表

        timer_init();       // 时钟模块        
        trap_init();        // 中断和异常
        trap_inithart();    // 修改trap处理函数 + 打开中断总开关
        plic_init();        // 中断控制器
        plic_inithart();    // 使能具体的中断
        //printf("Waiting for UART input... (type any character)\n");
        
        proc_userinit();    // 创建第一个用户态进程（首进程）
        proc_schedule();    // 启动CPU0的调度器（不会返回）
        
        // while (1) {
        //     __asm__ __volatile__("wfi");  // 等待中断（低功耗模式）
        // }
    }
    while(1);
}


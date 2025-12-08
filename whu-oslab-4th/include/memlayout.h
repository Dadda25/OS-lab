/* memory leyout */
#ifndef __MEMLAYOUT_H__
#define __MEMLAYOUT_H__

// platform-level interrupt controller(PLIC)
#define PLIC_BASE 0x0c000000ul
#define PLIC_PRIORITY(id) (PLIC_BASE + (id) * 4)
#define PLIC_PENDING (PLIC_BASE + 0x1000)
#define PLIC_MENABLE(hart) (PLIC_BASE + 0x2000 + (hart)*0x100)
#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2080 + (hart)*0x100)
#define PLIC_MPRIORITY(hart) (PLIC_BASE + 0x200000 + (hart)*0x2000)
#define PLIC_SPRIORITY(hart) (PLIC_BASE + 0x201000 + (hart)*0x2000)
#define PLIC_MCLAIM(hart) (PLIC_BASE + 0x200004 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC_BASE + 0x201004 + (hart)*0x2000)

// core local interruptor(CLINT)
#define CLINT_BASE 0x2000000ul
#define CLINT_MSIP(hartid) (CLINT_BASE + 4 * (hartid))
#define CLINT_MTIMECMP(hartid) (CLINT_BASE + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT_BASE + 0xBFF8)

// virtio mmio
#define VIO_BASE  0x10001000ul
#define VIO_IRQ   1

// RTC 实时时钟
#define RTC_BASE 0x101000ul
#define TIMER_TIME_LOW       0x00
#define TIMER_TIME_HIGH      0x04

// UART
#define UART_BASE  0x10000000ul
#define UART_IRQ   10

// KERNEL
#define KERNEL_BASE 0x80200000ul

// 最大的虚拟地址
#define VA_MAX (1L << 38)

// 虚拟地址的最高处,占一个page,用户态和内核态共享的切换代码
#define TRAMPOLINE (VA_MAX - 4096)

// TRAMPOLINE下面, 内核态和用户态共享数据
#define TRAPFRAME (TRAMPOLINE - 4096)

// mmap的起点和终点
// 可分配区域大小为64MB (4096 * 16个物理页)
#define VM_MMAP_START (TRAPFRAME - 4096 * (4096 * 16 + 128))
#define VM_MMAP_END   (TRAPFRAME - 4096 * 128) 

// kernel stack 被映射到TRAMPOLINE下面的位置,占两个page
#define KSTACK(p) (TRAMPOLINE - ((p)+1)*2*4096)

// 对齐
#define ALIGN_UP(addr,refer) (((addr) + (refer) - 1) & ~((refer) - 1))
#define ALIGN_DOWN(addr,refer) ((addr) & ~((refer) - 1))

#endif
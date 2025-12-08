# _entry是kernel.ld中指定的kernel-rv的入口

    .section .text
    .global _entry

_entry:
    
    # 调整sp到合适的位置
    # sp = kernel_stack + (hartid + 1) * 4096
    # 调用start函数(in start.c) a0 a1 作为参数

    la sp, kernel_stack
    li t0, 4096
    mv t1, a0
    addi t1, t1, 1
    mul t0, t0, t1
    add sp, sp, t0

    call start

# start 不应返回，否则进入低功耗死循环
spin:
    wfi
    j  spin
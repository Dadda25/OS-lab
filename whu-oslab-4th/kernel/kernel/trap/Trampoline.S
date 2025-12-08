# 本模块用于处理用户态和内核态之间的状态转换
# 代码会被映射到虚拟空间的顶部(TRAMPOLINE)占一页
# 由内核态和用户态共享,所以切换页表时不会影响这部分代码

#include "memlayout.h"

.section trampsec

.global trampoline

trampoline:
.align 4

# 用户态代码触发trap后执行流会来到这里
.global uservec
uservec:
    # 将a0的值写入sscratch寄存器中暂存
    csrw sscratch, a0
    # a0 = TRAPFRAME
    li a0, TRAPFRAME
    # 将寄存器环境保存至trapframe
    sd ra, 40(a0)
    sd sp, 48(a0)
    sd gp, 56(a0)
    sd tp, 64(a0)
    sd t0, 72(a0)
    sd t1, 80(a0)
    sd t2, 88(a0)
    sd s0, 96(a0)
    sd s1, 104(a0)
    #sd a0, 112(a0)    
    sd a1, 120(a0)
    sd a2, 128(a0)
    sd a3, 136(a0)
    sd a4, 144(a0)
    sd a5, 152(a0)
    sd a6, 160(a0)
    sd a7, 168(a0)
    sd s2, 176(a0)
    sd s3, 184(a0)
    sd s4, 192(a0)
    sd s5, 200(a0)
    sd s6, 208(a0)
    sd s7, 216(a0)
    sd s8, 224(a0)
    sd s9, 232(a0)
    sd s10, 240(a0)
    sd s11, 248(a0)
    sd t3, 256(a0)
    sd t4, 264(a0)
    sd t5, 272(a0)
    sd t6, 280(a0)
    
    # 补上a0的空缺
    csrr t0, sscratch;
    sd t0, 112(a0)
    # 至此通用寄存器全部保存好了

    # t1 = p->tf->kernel_satp 即 内核页表信息 
    ld t1, 0(a0)
    # sp = p->tf->kernel_sp
    ld sp, 8(a0)
    # tp = p->tf->kernel_hartid
    ld tp, 24(a0)
    # t0 = p->tf->kernel_trap 即 trap_user()
    ld t0, 16(a0)
    
    # 对当前处理器上的全部TLB进行全局无效化操作
    # 切换至内核页表
    sfence.vma zero, zero
    # 切换页表
    csrw satp, t1
    sfence.vma zero, zero

    #寄存器间接跳转,跳转到函数user_trap.c的trap_user()
    jr t0


# trapret_user调用此函数 
# 函数会接受一个参数satp

.global userret
userret:
    # 切换回进程页表
    sfence.vma zero, zero
    csrw satp, a0
    sfence.vma zero, zero

    li a0, TRAPFRAME

    ld ra, 40(a0)
    ld sp, 48(a0)
    ld gp, 56(a0)
    ld tp, 64(a0)
    ld t0, 72(a0)
    ld t1, 80(a0)
    ld t2, 88(a0)
    ld s0, 96(a0)
    ld s1, 104(a0)
    ld a1, 120(a0)
    ld a2, 128(a0)
    ld a3, 136(a0)
    ld a4, 144(a0)
    ld a5, 152(a0)
    ld a6, 160(a0)
    ld a7, 168(a0)
    ld s2, 176(a0)
    ld s3, 184(a0)
    ld s4, 192(a0)
    ld s5, 200(a0)
    ld s6, 208(a0)
    ld s7, 216(a0)
    ld s8, 224(a0)
    ld s9, 232(a0)
    ld s10, 240(a0)
    ld s11, 248(a0)
    ld t3, 256(a0)
    ld t4, 264(a0)
    ld t5, 272(a0)
    ld t6, 280(a0)
    
    ld a0, 112(a0)
    # 至此trapframe中保存的内容全部恢复至通用寄存器

    # 返回U-MODE
    # trapret_user()中完成了sstatus和sepc的设置,保证不出问题
    sret
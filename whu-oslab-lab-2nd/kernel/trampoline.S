# trampoline.S
# Small trampoline code for switching page tables and entering user mode

.section .trampoline, "ax"
.global trampoline

trampoline:
    # 这里通常要切换 satp，然后执行 sret/mret 回到用户态
    # 但目前我们放一个最小版本，保证链接通过

    # 占位：跳回内核 start() 或直接返回
    ret

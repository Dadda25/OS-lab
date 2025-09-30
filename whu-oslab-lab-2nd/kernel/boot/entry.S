# Boot loader for RISC-V
# Entry point for all CPU cores

.section .text
.global _entry
.global trampoline   # 声明符号，保证能被 C 代码引用

_entry:
    # Get CPU ID from mhartid CSR
    csrr a0, mhartid
    
    # Set up stack for each CPU
    # Each CPU gets 4KB stack space
    la sp, stack_base
    li t0, 4096          # 4KB per CPU
    mv t1, a0            # CPU ID
    mul t0, t0, t1       # offset = 4KB * CPU_ID  
    add sp, sp, t0       # sp = stack_base + offset
    li t2, 4096          # Load 4KB into register
    add sp, sp, t2       # Point to top of stack

    # Clear BSS section (only CPU 0 does this)
    beqz a0, clear_bss   # if CPU ID == 0, clear BSS
    j skip_bss

clear_bss:
    la t0, _bss_start
    la t1, _bss_end
1:
    beq t0, t1, skip_bss
    sd zero, 0(t0)
    addi t0, t0, 8
    j 1b

skip_bss:
    # Jump to C code with CPU ID in a0
    call start

    # Should never return, but just in case
spin:
    wfi
    j spin

.section .bss
.align 4
stack_base:
    .space 8192  # 4KB * 2 CPUs = 8KB total stack space

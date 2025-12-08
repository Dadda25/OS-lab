.section .rodata

.global KERNEL_TEXT
KERNEL_TEXT : .dword _KERNEL_TEXT

.global KERNEL_DATA
KERNEL_DATA : .dword _KERNEL_DATA

.global USER_END
USER_END : .dword _USER_END

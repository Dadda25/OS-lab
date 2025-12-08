#ifndef __TRAP_H__
#define __TRAP_H__

// in trap.S 作为trap触发后跳转到的地址
extern void trap_vector(void);

void external_interrupt_handler(void);
void timer_interrupt_handler(bool inkernel);

void trap_init(void);           // 初始化
void trap_inithart(void);       
void trap_kernel(void);         // 内核trap处理
void trap_user(void);           // 用户trap处理
void trapret_user(void);        // 处理完毕返回用户态

#endif
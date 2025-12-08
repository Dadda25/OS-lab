#ifndef __UART_H__
#define __UART_H__

#include "common.h"

void uart_init();                // uart初始化
void uart_putc(int c);           // 单个字符输出
void uart_puts(char* s);         // 字符串输出
void uart_intr(void);            // 输入中断响应
void uart_putc_sync(int c);      // 带开关中断的单个字符输出

#endif
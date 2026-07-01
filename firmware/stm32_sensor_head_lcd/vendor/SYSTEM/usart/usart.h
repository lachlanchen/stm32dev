#ifndef _USART_H
#define _USART_H
#include "sys.h"
#include <stdio.h>
extern UART_HandleTypeDef UART1_Handler;
void uart_init(u32 bound);
void serial_write(const char *s);
#endif

/*
 * uart0.h
 *
 * Created: 2015-01-22 16:37:50
 *  Author: ioannisg
 */ 


#ifndef UART0_H_
#define UART0_H_


#include "contiki-conf.h"

#ifdef UART_CONF_IRQ_PRIORITY
#define UART_IRQ_PRIORITY UART_CONF_IRQ_PRIORITY
#else
#define UART_IRQ_PRIORITY 3
#endif

void uart0_set_input(int (*input) (unsigned char c));

void uart0_writeb(unsigned char c);

void uart0_init(unsigned long ubr);

void uart0_enable_rx_interrupt(void);

void uart0_reset(void);



#endif /* UART0_H_ */
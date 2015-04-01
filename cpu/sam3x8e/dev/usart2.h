/*
 * usart2.h
 *
 * Created: 2015-01-01 00:46:02
 *  Author: ioannisg
 */ 


#ifndef USART2_H_
#define USART2_H_

#include "contiki-conf.h"

#ifdef USART_CONF_IRQ_PRIORITY
#define USART_IRQ_PRIORITY USART_CONF_IRQ_PRIORITY
#else
#define USART_IRQ_PRIORITY 3
#endif

void usart2_set_input(int (*input) (unsigned char c));

void usart2_writeb(unsigned char c);

void usart2_init(unsigned long ubr);

void usart2_enable_rx_interrupt(void);

void usart2_reset(void);


#endif /* USART1_H_ */
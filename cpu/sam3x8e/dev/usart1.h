/*
 * usart1.h
 *
 * Created: 2015-01-01 00:46:02
 *  Author: ioannisg
 */ 


#ifndef USART1_H_
#define USART1_H_

#include "contiki-conf.h"

#ifdef USART_CONF_IRQ_PRIORITY
#define USART_IRQ_PRIORITY USART_CONF_IRQ_PRIORITY
#else
#define USART_IRQ_PRIORITY 3
#endif

void usart1_set_input(int (*input) (unsigned char c));

void usart1_writeb(unsigned char c);

void usart1_init(unsigned long ubr);

void usart1_enable_rx_interrupt(void);

void usart1_reset(void);


#endif /* USART1_H_ */
/**
 * \addtogroup sam3x8e-cpu
 *
 * @{
 */

/**
 * Copyright (c) 2011 - 2013 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

/**
* \file
*					Machine dependent SAM3X8E UART1 Wrapper code.
* \author
*					Ioannis Glaropoulos <ioannisg@kth.se>
* \version
*					1.0
* \since
*					13.01.2014
*/

#include <stdio.h>
#include <stdlib.h>
#include "status_codes.h"
#include "sys/energest.h"
#include "dev/uart1.h"
#include "dev/watchdog.h"
#include "lib/ringbuf.h"
#include "dev/leds.h"
#include "conf_uart_serial.h"
#include "arduino_due_x/arduino_due_x.h"
#include "uart_serial.h"
#include "stdio_serial.h"
#include "conf_uart_serial.h"
#include "uart.h"

#include "platform-conf.h"

/* Define and link the Interrupt handler of the UART interface. 
 * The interrupt handler is called when there is activity in the
 * UART interface. We should handle all interrupts, however we'll
 * concentrate on Receive and Error interrupts.
 */
#define uart_irq_handler	UART0_Handler
/*---------------------------------------------------------------------------*/

/* This is the receive interrupt handler. */
static int (*uart1_input_handler) (unsigned char c);
/*---------------------------------------------------------------------------*/
/* 
 * This function is called externally, to assign a 
 * handler for the received input interrupt. 
 */
void 
uart1_set_input(int (*input) (unsigned char c))
{
	printf("\rSetting UART callback.\n");
	uart1_input_handler = input;
	if (input == NULL ) 
		printf("ERROR setting UART callback.\n");
}
//void uart1_rx_interrupt(void);

void uart1_tx_interrupt(void);

static volatile uint8_t transmitting;

#ifdef UART1_CONF_TX_WITH_INTERRUPT
#define TX_WITH_INTERRUPT UART1_CONF_TX_WITH_INTERRUPT
#else /* UART1_CONF_TX_WITH_INTERRUPT */
#define TX_WITH_INTERRUPT     1
#endif /* UART1_CONF_TX_WITH_INTERRUPT */


#if TX_WITH_INTERRUPT
#ifdef UART1_CONF_TX_BUFSIZE
#define UART1_TX_BUFSIZE UART1_CONF_TX_BUFSIZE
#else /* UART1_CONF_TX_BUFSIZE */
#define UART1_TX_BUFSIZE 64
#endif /* UART1_CONF_TX_BUFSIZE */
static struct ringbuf txbuf;
static uint8_t txbuf_data[UART1_TX_BUFSIZE];
#endif /* TX_WITH_INTERRUPT */

/*--------------------------------------------------------------------------*/
/* 
 * Interrupt handler for received data in the UART line. This functions is 
 * made static. It is called inside UART interrupt context. 
 */
static void 
uart1_rx_interrupt(void) {
	
	/* Store the character that is read from the UART line. */
	uint8_t c;
	/* We must read from the UART register, otherwise the 
	 * interrupt will be thrown again and again. Reading
	 * clears the interrupt status.  
	 */
	uint8_t _result = uart_read(CONSOLE_UART, &c);
	/* Since we have checked before, we should not
	 * get a wrong read result. We, however, do the
	 * check according to the Contiki conventions.
	 */
	if (!_result) {
		/* If the handler has been registered, it should 
		 * be called with the received byte as argument.
		 */		
		if(uart1_input_handler != NULL) {
			uart1_input_handler(c);
		} else {
			printf("Null UART RX handler!\n");
		}
	} 
}
/*---------------------------------------------------------------------------*/
void
uart1_enable_rx_interrupt() {
	
	/* Enable successful & error receive interrupts on the UART port. */
	uart_enable_interrupt(CONSOLE_UART, 
							UART_IER_RXRDY | 
							UART_IER_OVRE | 
							UART_IER_FRAME);
	
	/* Enable UART interrupts in IRQ vector [NVIC]. */
	NVIC_EnableIRQ(CONSOLE_UART_ID);
}
/*---------------------------------------------------------------------------*/
void 
uart1_writeb(unsigned char c)
{
  watchdog_periodic();
#if TX_WITH_INTERRUPT
  /*
   * Put the outgoing byte on the transmission buffer. If the buffer
   * is full, we just keep on trying to put the byte into the buffer
   * until it is possible to put it there.
   */
  while(ringbuf_put(&txbuf, c) == 0);

  /*
   * If there is no transmission going, we need to start it by putting
   * the first byte into the UART.
   */
  if(transmitting == 0) {
    transmitting = 1;
    SC1_DATA = ringbuf_get(&txbuf);
    INT_SC1FLAG = INT_SCTXFREE;
    INT_SC1CFG |= INT_SCTXFREE;
  }
#else /* TX_WITH_INTERRUPT */

  /* Block until the transmission buffer is available. */
  while ((CONSOLE_UART->UART_SR & UART_SR_TXRDY) != UART_SR_TXRDY);
  /* Transmit the data. */
  CONSOLE_UART->UART_THR = c;
  
#endif /* TX_WITH_INTERRUPT */
}
/*---------------------------------------------------------------------------*/
#if 0
void
uart1_writeb(unsigned char c)
{
  watchdog_periodic();
#if TX_WITH_INTERRUPT
  /*
   * Put the outgoing byte on the transmission buffer. If the buffer
   * is full, we just keep on trying to put the byte into the buffer
   * until it is possible to put it there.
   */
  while(ringbuf_put(&txbuf, c) == 0);

  /*
   * If there is no transmission going, we need to start it by putting
   * the first byte into the UART.
   */
  if(transmitting == 0) {
    transmitting = 1;
    SC1_DATA = ringbuf_get(&txbuf);
    INT_SC1FLAG = INT_SCTXFREE;
    INT_SC1CFG |= INT_SCTXFREE;
  }
#else /* TX_WITH_INTERRUPT */

  /* Loop until the transmission buffer is available. */
  while((INT_SC1FLAG & INT_SCTXFREE) == 0);

  /* Transmit the data. */
  SC1_DATA = c;

  INT_SC1FLAG = INT_SCTXFREE;
#endif /* TX_WITH_INTERRUPT */
}
#endif
/*---------------------------------------------------------------------------*/
#if ! WITH_UIP
/* If WITH_UIP is defined, putchar() is defined by the SLIP driver */
#endif /* ! WITH_UIP */
/*---------------------------------------------------------------------------*/

/*
 * Initialize the RS232 port.
 */
void
uart1_init(unsigned long ubr)
{
  static uint8_t is_initialized = 0;
  if (is_initialized)
    return;
  is_initialized = 1;
  sysclk_enable_peripheral_clock(CONSOLE_UART_ID);
  
  const usart_serial_options_t uart_serial_options = {
	  .baudrate = ubr,
	  .paritytype = CONF_UART_PARITY
  };
  
  /* UART initialization. */  
  stdio_serial_init(CONF_UART, &uart_serial_options);
  
  /* Enable the peripheral clock in the PMC. */
  pmc_enable_periph_clk(CONSOLE_UART_ID);
  
  /* Configure interrupts */
  uart_disable_interrupt(CONSOLE_UART, 0xffffffff);
   
  /* Enable transmit and receive on the UART port. */
  uart_enable(CONSOLE_UART);
    
  /* Initially the UART is not transmitting. */
  transmitting = 0;
   
#if TX_WITH_INTERRUPT
  ringbuf_init(&txbuf, txbuf_data, sizeof(txbuf_data));
#endif /* TX_WITH_INTERRUPT */
}
/*---------------------------------------------------------------------------*/
#if 0
/*
 * Initialize the RS232 port.
 */
void
uart1_init(unsigned long ubr)
{
  uint16_t uartper;

  uint32_t rest;

  GPIO_PBCFGL &= 0xF00F;
  GPIO_PBCFGL |= 0x0490;

  uartper = (uint32_t) 24e6 / (2 * ubr);
  rest = (uint32_t) 24e6 % (2 * ubr);

  SC1_UARTFRAC = 0;
  if(rest > (2 * ubr) / 4 && rest < (3 * 2 * ubr) / 4) {
    SC1_UARTFRAC = 1;           /* + 0.5 */
  } else if(rest >= (3 * 2 * ubr) / 4) {
    uartper++;                  /* + 1 */
  }

  SC1_UARTPER = uartper;
  SC1_UARTCFG = SC_UART8BIT;
  SC1_MODE = SC1_MODE_UART;
  /* 
   * Receive buffer has data interrupt mode and Transmit buffer free interrupt
   * mode: Level triggered.
   */
  SC1_INTMODE = SC_RXVALLEVEL | SC_TXFREELEVEL;
  INT_SC1CFG = INT_SCRXVAL;     /* Receive buffer has data interrupt enable */
  transmitting = 0;

#if TX_WITH_INTERRUPT
  ringbuf_init(&txbuf, txbuf_data, sizeof(txbuf_data));
#endif /* TX_WITH_INTERRUPT */

  INT_SC1FLAG = 0xFFFF;
  INT_CFGSET = INT_SC1;
}
#endif
/*---------------------------------------------------------------------------*/
#if 0
void
halSc1Isr(void)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  if(INT_SC1FLAG & INT_SCRXVAL) {
    uart1_rx_interrupt();
    INT_SC1FLAG = INT_SCRXVAL;
  }
#if TX_WITH_INTERRUPT
  else if(INT_SC1FLAG & INT_SCTXFREE) {
    uart1_tx_interrupt();
    INT_SC1FLAG = INT_SCTXFREE;
  }
#endif /* TX_WITH_INTERRUPT */

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*--------------------------------------------------------------------------*/
#endif
void uart_irq_handler() {
	
	ENERGEST_ON(ENERGEST_TYPE_IRQ);
	
	/* Read the status register. */
	uint32_t status = uart_get_status(CONSOLE_UART);

	/* Branch execution depending on receive or transmit interrupt. */
	if ((status & UART_SR_RXRDY) == UART_SR_RXRDY) {
		/* We have received data and the receive register is ready. */
		uart1_rx_interrupt();
	
	} else if ((status & UART_SR_OVRE) == UART_SR_OVRE || 
			   (status & UART_SR_FRAME) == UART_SR_FRAME) {
		/* TODO: error reporting outside ISR.
		 * For the moment we just reset the control register. 
		 */
		CONSOLE_UART->UART_CR |= UART_CR_RSTSTA;
	}
	
	#if TX_WITH_INTERRUPT
	else if ((status & UART_SR_TXRDY) == UART_SR_TXRDY) {
		
		uart1_tx_interrupt();
		INT_SC1FLAG = INT_SCTXFREE;
	}	
	#endif /* TX_WITH_INTERRUPT */

	ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*--------------------------------------------------------------------------*/
#if 0
void
uart1_rx_interrupt(void)
{
  uint8_t c;

  c = SC1_DATA;
  if(uart1_input_handler != NULL) {
    uart1_input_handler(c);
  }
}
#endif
/*---------------------------------------------------------------------------*/
#if TX_WITH_INTERRUPT
void
uart1_tx_interrupt(void)
{
  if(ringbuf_elements(&txbuf) == 0) {
    transmitting = 0;
    INT_SC1CFG &= ~INT_SCTXFREE;
  } else {
    SC1_DATA = ringbuf_get(&txbuf);
  }
}
#endif /* TX_WITH_INTERRUPT */
/** @} */

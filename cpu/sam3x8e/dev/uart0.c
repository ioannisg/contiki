/*
 * uart0.c
 *
 * Created: 2015-01-22 16:37:24
 *  Author: ioannisg
 */ 
#include <stdio.h>
#include <stdint.h>
#include "sam3x8e.h"
#include "uart.h"

#include "platform-conf.h"
#include "sysclk.h"
#include "watchdog.h"
#include "energest.h"
#include "status_codes.h"
#include "uart0.h"
#include "delay.h"
#include "uart_serial.h"
#include "stdio_serial.h"

static long uart0_baudrate = 0;

/* Define and link the Interrupt handler of the UART interface. 
 * The interrupt handler is called when there is activity in the
 * UART interface. We should handle all interrupts, however we'll
 * concentrate on Receive and Error interrupts.
 */
#define uart_irq_handler	UART_Handler
/*---------------------------------------------------------------------------*/

/* This is the receive interrupt handler. */
static int (*uart0_input_handler) (unsigned char c);
/*---------------------------------------------------------------------------*/
/* 
 * This function is called externally, to assign a 
 * handler for the received input interrupt. 
 */
void 
uart0_set_input(int (*input) (unsigned char c))
{
	printf("\rSetting UART0 callback.\n");
	uart0_input_handler = input;
	if (input == NULL ) 
		printf("ERROR setting UART callback.\n");
}
/*---------------------------------------------------------------------------*/
/*
 * Initialize the RS232 port.
 */
void
uart0_init(unsigned long ubr)
{
  uart0_baudrate = ubr;
  sysclk_disable_peripheral_clock(ID_UART);
  pmc_disable_periph_clk(ID_UART);
  delay_ms(100);
  sysclk_enable_peripheral_clock(ID_UART);
  
  const usart_serial_options_t uart_serial_options = {
	  .baudrate = ubr,
	  .paritytype = CONF_UART_PARITY
  };
  
  /* UART initialization. */
  stdio_serial_init(CONF_UART, &uart_serial_options);
  
  /* Enable the peripheral clock in the PMC. */
  pmc_enable_periph_clk(ID_UART);
  
  /* Disable all the interrupts. */
  uart_disable_interrupt(UART, 0xffffffff);

  /* Enable the receiver and transmitter. */
  uart_enable_tx(UART);
  uart_enable_rx(UART);
}
/*---------------------------------------------------------------------------*/
void
uart0_writeb(unsigned char c)
{
  watchdog_periodic();
  
  /* Block until the transmission buffer is available. */
  while(!uart_is_tx_ready(UART)) {
  }
  
  /* Transmit the data. */
  if (uart_write(UART, c)) {
    printf("uart: write-err\n");
  }
}
/*---------------------------------------------------------------------------*/
void
uart0_enable_rx_interrupt(void)
{  
  /* Enable successful & error receive interrupts on the UART port. */
  uart_enable_interrupt(UART,
    US_IER_RXRDY |
    US_IER_OVRE |
    US_IER_FRAME);
  
  /* Enable UART interrupts in IRQ vector [NVIC]. */
  NVIC_EnableIRQ(ID_UART);
  NVIC_SetPriority(ID_UART, UART_IRQ_PRIORITY);
}
/*---------------------------------------------------------------------------*/
static void
uart0_rx_interrupt(void)
{
  /* Store the character that is read from the UART line. */
  uint8_t c;
  /* We must read from the UART register, otherwise the 
   * interrupt will be thrown again and again. Reading
   * clears the interrupt status.  
   */
  while(uart_read(UART, &c) == 0) {
  
    if(uart0_input_handler != NULL) {
      uart0_input_handler((unsigned char)c);
    } else {
      printf("Null UART RX handler!\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
void
uart_irq_handler()
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  /* Read the status register. */
  uint32_t status = uart_get_status(UART);

  /* Branch execution depending on receive or transmit interrupt. */
  if ((status & US_CSR_RXRDY) == US_CSR_RXRDY) {
    /* We have received data and the receive register is ready. */
    uart0_rx_interrupt();

  } else if ((status & US_CSR_OVRE) == US_CSR_OVRE || 
    (status & US_CSR_FRAME) == US_CSR_FRAME) {
		/* TODO: error reporting outside ISR.
		 * For the moment we do reset to default settings.
		 */
      //printf("uart: irq-err %x\n", status);
		uart0_init(uart0_baudrate);
		uart0_enable_rx_interrupt();
	}

	ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
void
uart0_reset(void) {
	uart_reset(UART);
}
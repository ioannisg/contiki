/*
 * usart1.c
 *
 * Created: 2014-12-31 23:32:28
 *  Author: ioannisg
 */ 
#include <stdio.h>
#include <stdint.h>
#include "sam3x8e.h"
#include "usart.h"

#include "platform-conf.h"
#include "sam3x\sysclk.h"
#include "watchdog.h"
#include "energest.h"
#include "status_codes.h"
#include "usart1.h"
#include "delay.h"

static long usart1_baudrate = 0;

/* Define and link the Interrupt handler of the UART interface. 
 * The interrupt handler is called when there is activity in the
 * UART interface. We should handle all interrupts, however we'll
 * concentrate on Receive and Error interrupts.
 */
#define usart_irq_handler	USART0_Handler
/*---------------------------------------------------------------------------*/

/* This is the receive interrupt handler. */
static int (*usart1_input_handler) (unsigned char c);
/*---------------------------------------------------------------------------*/
/* 
 * This function is called externally, to assign a 
 * handler for the received input interrupt. 
 */
void 
usart1_set_input(int (*input) (unsigned char c))
{
	printf("\rSetting USART callback.\n");
	usart1_input_handler = input;
	if (input == NULL ) 
		printf("ERROR setting USART callback.\n");
}
/*---------------------------------------------------------------------------*/
/*
 * Initialize the RS232 port.
 */
void
usart1_init(unsigned long ubr)
{
  usart1_baudrate = ubr;
  sysclk_disable_peripheral_clock(ID_USART0);
  pmc_disable_periph_clk(ID_USART0);
  delay_ms(100);
  sysclk_enable_peripheral_clock(ID_USART0);
  
  const sam_usart_opt_t usart_console_settings = {
    ubr,
    US_MR_CHRL_8_BIT,
    US_MR_PAR_NO,
    US_MR_NBSTOP_1_BIT,
    US_MR_CHMODE_NORMAL,
    /* This field is only used in IrDA mode. */
    0
  };
  
  /* Enable the peripheral clock in the PMC. */
  pmc_enable_periph_clk(ID_USART0);
  
  /* Configure USART in serial mode. */
  if (usart_init_rs232(USART0, &usart_console_settings,
    sysclk_get_cpu_hz())) {
      printf("usart1: init-fail\n");
	 }

  /* Disable all the interrupts. */
  usart_disable_interrupt(USART0, 0xffffffff);

  /* Enable the receiver and transmitter. */
  usart_enable_tx(USART0);
  usart_enable_rx(USART0);
}
/*---------------------------------------------------------------------------*/
void
usart1_writeb(unsigned char c)
{
  watchdog_periodic();
  
  /* Block until the transmission buffer is available. */
  while(!usart_is_tx_ready(USART0)) {
  }
  
  /* Transmit the data. */
  if (usart_putchar(USART0, c)) {
    printf("usart: write-err\n");
  }
}
/*---------------------------------------------------------------------------*/
void
usart1_enable_rx_interrupt(void)
{  
  /* Enable successful & error receive interrupts on the UART port. */
  usart_enable_interrupt(USART0,
    US_IER_RXRDY |
    US_IER_OVRE |
    US_IER_FRAME);
  
  /* Enable USART interrupts in IRQ vector [NVIC]. */
  NVIC_EnableIRQ(ID_USART0);
  NVIC_SetPriority(ID_USART0, USART_IRQ_PRIORITY);
}
/*---------------------------------------------------------------------------*/
static void
usart1_rx_interrupt(void)
{
  /* Store the character that is read from the UART line. */
  uint32_t c;
  /* We must read from the UART register, otherwise the 
   * interrupt will be thrown again and again. Reading
   * clears the interrupt status.  
   */
  while(usart_read(USART0, &c) == 0) {
  
    if(usart1_input_handler != NULL) {
      usart1_input_handler((unsigned char)c);
    } else {
      printf("Null USART RX handler!\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
void
usart_irq_handler()
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  /* Read the status register. */
  uint32_t status = usart_get_status(USART0);

  /* Branch execution depending on receive or transmit interrupt. */
  if ((status & US_CSR_RXRDY) == US_CSR_RXRDY) {
    /* We have received data and the receive register is ready. */
    usart1_rx_interrupt();

  } else if ((status & US_CSR_OVRE) == US_CSR_OVRE || 
    (status & US_CSR_FRAME) == US_CSR_FRAME) {
		/* TODO: error reporting outside ISR.
		 * For the moment we do reset to default settings.
		 */
      //printf("usart: irq-err %x\n", status);
		usart1_init(usart1_baudrate);
		usart1_enable_rx_interrupt();
	}

	ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
void
usart1_reset(void) {
	usart_reset(USART0);
}
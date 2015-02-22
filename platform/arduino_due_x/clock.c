/*---------------------------------------------------------------------------*/
/**
* \file
*			System Clock for SAM3X8E.
* \author
*			Ioannis Glaropoulos <ioannisg@kth.se>
*/
/*---------------------------------------------------------------------------*/

/*
 * File customized for SAM3X8E platform. It uses systick timer to control button
 * status without interrupts, as well as for system clock.
 */
#include "interrupt/interrupt_sam_nvic.h"
#include "sam3x/sysclk.h"
#include "sys/clock.h"
#include "sys/etimer.h"
//#include "dev/button-sensor.h"
//#include "uart1.h"
//#include "dev/leds.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

// The value that will be load in the SysTick value register.
#define RELOAD_VALUE 24000-1   // 1 ms with a 24 MHz clock

static volatile clock_time_t count;
static volatile unsigned long current_seconds = 0;
static unsigned int second_countdown = CLOCK_SECOND;

/*---------------------------------------------------------------------------*/
/**
 *  \brief Handler for System Tick interrupt.
 *
 *  Process System Tick Event
 *  Increments the ticks counter and polls the event timer if required.
 */
void SysTick_Handler(void)
{
	count++;
	
/*	
	if(button_sensor.status(SENSORS_READY)){
		button_sensor.value(0); // sensors_changed is called inside this function.
	}
*/
	if(etimer_pending()) {
		etimer_request_poll();
	}
	
	if (--second_countdown == 0) {
		current_seconds++;
		second_countdown = CLOCK_SECOND;
	}
  
}
/*---------------------------------------------------------------------------*/
void clock_init(void)
{ 
	cpu_irq_enter_critical();
	// Counts the number of ticks.
	count = 0;
  
	/* Configure sys-tick for 1 ms */
	PRINTF("DEBUG: Configure system tick to get 1ms tick period.\n");
	PRINTF("DEBUG: The output of the sysclk_get_cpu_hz() is %d Hz.\n",(int)sysclk_get_cpu_hz());
  
	if (SysTick_Config(sysclk_get_cpu_hz() / 1000)) {
		printf("-F- Systick configuration error\r");
		while (1);
	}
	cpu_irq_leave_critical();
}
/*---------------------------------------------------------------------------*/
clock_time_t clock_time(void)
{
  return count;
}
/*---------------------------------------------------------------------------*/
/**
 * Delay the CPU for a multiple of TODO
 */
void clock_delay(unsigned int i)
{
  for (; i > 0; i--) {		/* Needs fixing XXX */
    unsigned j;
    for (j = 50; j > 0; j--)
      asm ("nop");
  }
}
/*---------------------------------------------------------------------------*/
/**
 * Wait for a multiple of 1 ms.
 *
 */
void clock_wait(clock_time_t i)
{
  clock_time_t start;

  start = clock_time();
  while(clock_time() - start < (clock_time_t)i);
}
/*---------------------------------------------------------------------------*/
unsigned long clock_seconds(void)
{
  return current_seconds;
}
/*---------------------------------------------------------------------------*/
#if 0
void sleep_seconds(int seconds)
{
  uint32_t quarter_seconds = seconds * 4;
  uint8_t radio_on;


  halPowerDown();
  radio_on = stm32w_radio_is_on();
  stm32w_radio_driver.off();

  halSleepForQsWithOptions(&quarter_seconds, 0);


  ATOMIC(

  halPowerUp();

  // Update OS system ticks.
  current_seconds += seconds - quarter_seconds / 4 ; // Passed seconds
  count += seconds * CLOCK_SECOND - quarter_seconds * CLOCK_SECOND / 4 ;

  if(etimer_pending()) {
    etimer_request_poll();
  }

  SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
  SysTick_SetReload(RELOAD_VALUE);
  SysTick_ITConfig(ENABLE);
  SysTick_CounterCmd(SysTick_Counter_Enable);

  )

  stm32w_radio_driver.init();
  if(radio_on){
	  stm32w_radio_driver.on();
  }

  uart1_init(115200);
  leds_init();
  rtimer_init();

  PRINTF("WakeInfo: %04x\r\n", halGetWakeInfo());


}
#endif

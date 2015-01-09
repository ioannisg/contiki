/**
 * \addtogroup arduino-due-ath9170-platform
 *
 * @{
 */



/**
* \file
*			Watchdog Wrapper Implementation
* \author
*			Ioannis Glaropoulos <ioannisg@kth.se>
*/

#include <stdio.h>
#include "dev/watchdog.h"
#include "wdt.h"
#include "arduino_due_x/arduino_due_x.h"
#include "sam3x8e.h"

/* Include platform configuration for watchdog settings. */
#include "platform-conf.h"
#include "rstc.h"

/*---------------------------------------------------------------------------*/
/**
 *  \brief Handler for watchdog interrupt.
 */
void WDT_Handler(void)
{
	puts("Enter watchdog interrupt.\n");

	/* Clear status bit to acknowledge interrupt by dummy read. */
	wdt_get_status(WDT);
	/* Restart the WDT counter. */
	wdt_disable(WDT);
	//puts("The watchdog timer was restarted.\r");
	watchdog_reboot();
}
/*---------------------------------------------------------------------------*/
void
watchdog_init(void)
{
	
}
/*---------------------------------------------------------------------------*/
void
watchdog_start(void)
{
  uint32_t wdt_mode, timeout_value;	
  /* 
   * We setup the watchdog to reset the device after 3000 seconds,
   * unless watchdog_periodic() is called.
   */
  
  /* Get timeout value. */
  timeout_value = wdt_get_timeout_value(WDT_PERIOD * 1000,
  BOARD_FREQ_SLCK_XTAL);
  if (timeout_value == WDT_INVALID_ARGUMENT) {
	  while (1) {
		  /* Invalid timeout value, error. */
	  }
  }
  /* Configure WDT to trigger an interrupt (or reset). */
  wdt_mode = WDT_MR_WDFIEN |  /* Enable WDT fault interrupt. */
			WDT_MR_WDRPROC   |  /* WDT fault resets processor only. */
			WDT_MR_WDDBGHLT  |  /* WDT stops in debug state. */
			WDT_MR_WDIDLEHLT;   /* WDT stops in idle state. */
			
  /* Initialize WDT with the given parameters. */
  wdt_init(WDT, wdt_mode, timeout_value, timeout_value);
  printf("Enable watchdog with %d microseconds period.\n",
  (int)wdt_get_us_timeout_period(WDT, BOARD_FREQ_SLCK_XTAL));

  /* Configure and enable WDT interrupt. */
  NVIC_DisableIRQ(WDT_IRQn);
  NVIC_ClearPendingIRQ(WDT_IRQn);
  NVIC_SetPriority(WDT_IRQn, 0);
  NVIC_EnableIRQ(WDT_IRQn);
  
  /* Enable user reset. */
  rstc_enable_user_reset(RSTC);
}
/*---------------------------------------------------------------------------*/
void
watchdog_periodic(void)
{
  /* This function is called periodically to restart the watchdog timer. */
  wdt_restart(WDT);
}
/*---------------------------------------------------------------------------*/
void
watchdog_stop(void)
{
  //wdt_disable(WDT);
  NVIC_DisableIRQ(WDT_IRQn);
}
/*---------------------------------------------------------------------------*/
void
watchdog_reboot(void)
{
  rstc_start_software_reset(RSTC);
}
/*---------------------------------------------------------------------------*/
/** @} */

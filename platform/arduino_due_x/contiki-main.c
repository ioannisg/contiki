/**
 * \addtogroup arduino-due
 *
 * @{
 */
/*
 * Copyright (c) 2014, Ioannis Glaropoulos.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the Contiki OS
 *
 */
/*---------------------------------------------------------------------------*/
/**
* \file
*          Contiki-main.c for Arduino Due.
* \author
*          Ioannis Glaropoulos <ioannisg@kth.se>
*/
/*---------------------------------------------------------------------------*/
#include <asf.h>
#include <string.h>
#include <stdio.h>

#include "stdio_serial.h"
#include "conf_uart_serial.h"
#include "conf_board.h"
#include "conf_clock.h"

#include "contiki.h"
#include "core/net/netstack_x.h"

#include "dev/serial-line.h"
#include "dev/uart1.h"
#include "dev/watchdog.h"

#if WITH_USB_HOST_SUPPORT
#include "dev/usbstack.h"
#endif

#if WITH_WIFI_SUPPORT
#include "dev/wifistack.h"
#endif

#if WITH_ETHERNET_SUPPORT
#include "dev/ethernetstack.h"
#endif

#include "tilt-sensor.h"

#ifdef WITH_SD_CARD_SUPPORT
#if WITH_SD_CARD_SUPPORT
#include "cfs-coffee-arch.h"
#endif
#endif

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x ",lladdr.u8[0], lladdr.u8[1], lladdr.u8[2], lladdr.u8[3],lladdr.u8[4], lladdr.u8[5], lladdr.u8[6], lladdr.u8[7])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif








SENSORS(&tilt_sensor);
#if ENABLE_OTA_FW_UPGRADE
#include "apps/ota-upgrade/ota-upgrade.h"
PROCINIT(&sensors_process, &ota_upgrade_process);
#else
PROCINIT(&sensors_process);
#endif /* ENABLE_OTA_FW_UPGRADE */

#define STRING_EOL    "\r"
#define STRING_HEADER "-- Getting Started Example --\r\n" \
		"-- "BOARD_NAME" --\r\n" \
		"-- Compiled: "__DATE__" "__TIME__" --"STRING_EOL

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
extern "C" {
#endif
/**INDENT-ON**/
/// @endcond

/*---------------------------------------------------------------------------*/
#if !PROCESS_CONF_NO_PROCESS_NAMES
static void
print_processes(struct process * const processes[])
{
	/*  const struct process * const * p = processes;*/
	PRINTF("Starting");
	while(*processes != NULL) {
		PRINTF(" '%s'", (*processes)->name);
		processes++;
	}
	putchar('\n');
}
#endif /* !PROCESS_CONF_NO_PROCESS_NAMES */
/*--------------------------------------------------------------------------*/
int main(void)
{
	/* Initialize the SAM system */
	sysclk_init();
	irq_initialize_vectors();
	cpu_irq_enable();

	/* Initialize the sleep manager */
	sleepmgr_init();
	
	/* Initialize the SAM board */
	board_init();

	/* Serial line [UART] initialization */
	uart1_init(CONF_UART_BAUDRATE);
	
	#if WITH_SERIAL_LINE_INPUT
	uart1_set_input(serial_line_input_byte);
	serial_line_init();
	#endif
	
	while(!uart_is_tx_ready(CONSOLE_UART));
		
	/* Output example information */
	puts(STRING_HEADER);
	
	/* PRINT Contiki Entry String */
	PRINTF("Starting ");
	PRINTF(CONTIKI_VERSION_STRING);
		
	/* Configure sys-tick for 1 ms */
	clock_init();
		
	/* Initialize Contiki and our processes */
	process_init();
	
	/* rtimer and ctimer should be initialized before radio duty cycling layers*/
	rtimer_init();
	
	/* etimer_process should be initialized before ctimer */
	process_start(&etimer_process, NULL);
	
	/* Initialize the ctimer process */
	ctimer_init();
	
	/* rtimer initialization */
	rtimer_init();
		
	/* Init SD card */
#ifdef WITH_SD_CARD_SUPPORT
	#if WITH_SD_CARD_SUPPORT
	sd_init();
	#endif
#endif
	
	/* Initialize watch-dog process */
	watchdog_start();
	
	/* Start boot processes */
	procinit_init();
	
#if WITH_ETHERNET_SUPPORT
	ethernetstack_init();
	/* Initialize networking [Interface 0] */
	netstack_0_init();
#endif
	
#if WITH_ZIGBEE_SUPPORT
	/* Initialize wireless 802.15.4 networking */
	netstack_init();
#endif

	#if WITH_USB_HOST_SUPPORT
	/* USB must be initialized before Wi-Fi device driver. */
	USB_STACK.init();
	#endif
		
	#if WITH_WIFI_SUPPORT 
	wifistack_init();
	#endif
		
	#if !PROCESS_CONF_NO_PROCESS_NAMES
	print_processes(autostart_processes);
	#endif
	
	/* Start all auto-start processes. */
	autostart_start(autostart_processes);
		
	while(1) {
		/* Reset watchdog. */
		watchdog_periodic();
		/* Contiki Polling System */
		process_run();		
	}
}

void 
HardFault_Handler(void)
{
#if PROCESS_CONF_NO_PROCESS_NAMES
	printf("HardFault\n");
#else
	printf("HardFault on %s\n", process_current->name);
#endif
	while(1);
}
void 
UsageFault_Handler(void)
{
	printf("UsageFault");
	while(1);
}
void 
MemManage_Handler(void)
{
	printf("MemManage");
	while(1);
}
void 
BusFault_Handler(void)
{	
	printf("BusFault");
	while(1);
}
void 
DebugMon_Handler(void)
{
	// TODO - no reboot
}

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
}
#endif
/**INDENT-ON**/
/// @endcond

/**
 * \defgroup arduino-due The arduino due x platform.
 *
 * The ARDUINO_DUE_X platform.
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
*          Platform-conf.h for Arduino Due X.
* \author
*          Ioannis Glaropoulos <ioannisg@kth.se>
*/
/*---------------------------------------------------------------------------*/

#ifndef PLATFORM_CONF_H_
#define PLATFORM_CONF_H_

#include <inttypes.h>
#include <string.h>             // For memcpm().
#include "sam3x8e.h"
#include "compiler.h"
#include "interrupt_sam_nvic.h"


/* Platform-dependent definitions */
#define CC_CONF_REGISTER_ARGS          			0
#define CC_CONF_FUNCTION_POINTER_ARGS  			1
#define CC_CONF_FASTCALL
#define CC_CONF_VA_ARGS                			1
#define CC_CONF_INLINE                 			inline

#define CCIF
#define CLIF

typedef unsigned short uip_stats_t;

/* ---- Serial line configuration ---- */
#define UART1_CONF_TX_WITH_INTERRUPT			0
#define WITH_SERIAL_LINE_INPUT				1


/*
 * Makefile should include the ETH_support
 * macro, should 802.3 be supported.
 */
#if WITH_ETHERNET_SUPPORT == 1
#define ETHERNET_DEV_DRIVER				enc424j600_driver
#define ENC624J600_CONF_IRQ_PIN                         16
#include "enc424J600-conf.h"
#define ETHERNET_MAX_FRAME_LEN			        ENC424J600_MAX_RX_FRAME_LEN
#endif

/*
 * Makefile should include the WiFi_support
 * macro, should 802.11 be supported.
 */
#if WITH_WIFI_SUPPORT == 1
#define WIFI_DEV_DRIVER					ar9170_driver
#endif

/*
 * Makefile should include the ZIGBEE_support
 * macro, should 802.15.4 XBEE be supported.
 */
#if WITH_ZIGBEE_SUPPORT == 1
#define ZIGBEE_RADIO_DRIVER				xbee_driver
#endif

/* Sanity check: WiFi needs USB Host stack */
#if WITH_WIFI_SUPPORT && (!WITH_USB_HOST_SUPPORT)
#error "WiFi needs USB!"
#endif

/*
 * Makefile should include the COAP version
 * support macro, should COAP be supported.
 */
#ifdef WITH_COAP_VERSION_SUPPORT
#define WITH_COAP	WITH_COAP_VERSION_SUPPORT
#else
#define WITH_COAP	0
#endif

/*
 * Make file should indicate whether the over-the-air
 * firmware upgrade will be supported
 */
#ifdef WITH_OTA_UPGRADE
#define ENABLE_OTA_FW_UPGRADE				1
#else
#define ENABLE_OTA_FW_UPGRADE				0
#endif

/* 
 * rtimer_second = 10500000;
 * A tick is ~95nanoseconds 
 */
#define RTIMER_ARCH_CONF_RESOLUTION			1
#define WITH_MULTIPLE_RTIMERS				1
#define RTIMER_PENDING_CONF_LEN				8
#define RTIMER_ARCH_GUARD_TIME				65 // ~6 us
/* A trick to resolve a compilation error with IAR. */
#ifdef __ICCARM__
#define UIP_CONF_DS6_AADDR_NBU				1
#endif

typedef unsigned long clock_time_t;

#define CLOCK_CONF_SECOND 1000

typedef uint64_t rtimer_clock_t;

#define RTIMER_CLOCK_LT(a,b)    ((signed short)((a)-(b)) < 0)

#define UIP_ARCH_ADD32					0//1
#define UIP_ARCH_CHKSUM					0

#define UIP_CONF_BYTE_ORDER				UIP_LITTLE_ENDIAN
#define EEPROM_CONF_SIZE				0

/* ---- Watchdog Settings. ---- */

/** Watchdog period 5000ms */
#define WDT_PERIOD					5000

/* ----- Sensor Settings ------ */
#define TILT_SENSOR_CONF_PIN_NUMBER			17

/* ----- Macros for atomic operations -------*/
#define INTERRUPTS_DISABLE()				cpu_irq_enter_critical()
#define INTERRUPTS_ENABLE()				cpu_irq_leave_critical()
#define INTERRUPT_DISABLE(irq_id)			NVIC_DisableIRQ(irq_id)
#define INTERRUPT_ENABLE(irq_id)			NVIC_EnableIRQ(irq_id)

/* ---- Flash/SD configuration for the COFFEE file system ---- */

/*
 * Makefile should include the COFFEE_support macro
 * should this Contiki OS file system be supported.
 */
#ifdef WITH_COFFEE_SUPPORT /* It is the COFFEE=1 flag in Instant Contiki */
/* Default values for coffee section start unless otherwise stated */
#ifndef WITH_SD_SUPPORT
#define FLASH_BANK0_SIZE				IFLASH0_SIZE
#define FLASH_BANK1_SIZE				IFLASH1_SIZE
#define FLASH_BANK0_START				IFLASH0_ADDR
#define FLASH_BANK1_START				IFLASH1_ADDR
#define BANK1_PAGE_SIZE					IFLASH1_PAGE_SIZE
#define BANK0_PAGE_SIZE					IFLASH0_PAGE_SIZE
#ifndef COFFEE_ADDRESS
/* Notice that this leaves 256KB of flash for the coffee file system
 * allowing only the first 256KB for program image. Be very careful!
 */
#define COFFEE_ADDRESS					0
#endif /* COFFEE_ADDRESS */
#else /* WITH_SD_SUPPORT */
#define WITH_SD_CARD_SUPPORT				1
#ifndef COFFEE_ADDRESS
#define COFFEE_ADDRESS					0
#else
#error "COFFEE_ADDRESS should be 0 if SD is present!"
#endif /* COFFEE_ADDRESS */
#endif /* WITH_SD_CARD_SUPPORT */
#endif /* WITH_COFFEE_SUPPORT */


/*
 * SPI bus configuration for the Arduino Due X.
 * This is only to support Contiki default API.
 */
#define SPI_TXBUF	SPI0->SPI_TDR;
#define SPI_RXBUF	SPI0->SPI_RDR;

/* SPI0 Tx ready? */
#define SPI_WAITFOREOTx() while (!spi_is_tx_empty(SPI0))
/* SPI0 Rx ready? */
#define SPI_WAITFOREORx() while (!spi_is_rx_full(SPI0))
/* SPI0 Tx buffer ready? */
#define SPI_WAITFORTxREADY() while (!spi_is_tx_ready(SPI0))


#endif /* PLATFORM_CONF_H_ */
/** @} */

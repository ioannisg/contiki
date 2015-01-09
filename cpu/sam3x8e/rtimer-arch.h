/**
 * \addtogroup arduino-due-ath9170
 *
 * @{
 */

/**
 * \file
 *         Real-timer header file for SAM3X8E.
 * \author
 *         Ioannis Glaropoulos <ioannisg@kth.net>
 */

#ifndef RTIMER_ARCH_H_
#define RTIMER_ARCH_H_

#include "contiki-conf.h"
#include "sys/clock.h"

#define RTIMER_ARCH_RES_24NS	0
#define	RTIMER_ARCH_RES_95NS	1
#define RTIMER_ARCH_RES_341NS	2
#define RTIMER_ARCH_RES_1_52US	3
#define RTIMER_ARCH_RES_85US	4

#ifdef RTIMER_ARCH_CONF_RESOLUTION
#define RTIMER_ARCH_RESOLUTION	RTIMER_ARCH_CONF_RESOLUTION
#else /* RTIMER_ARCH_CONF_RESOLUTION */
#define RTIMER_ARCH_RESOLUTION	RTIMER_ARCH_RES_95NS
#endif /* RTIMER_ARCH_CONF_RESOLUTION */

/*
 * We use the customized pre-scaling options defined in tc.h
 * and we adjust the declarations of TC ticks per second. We
 * note that the lower the pre-scaler the higher the obtained
 * resolution, however the overhead increases as TC comparison
 * and overflow events become more frequent and the total time
 * available for universal time, stored in one 64-bit integer.
 */

#if RTIMER_ARCH_RESOLUTION == RTIMER_ARCH_RES_85US
/* CK_CNT =  PCLK/2048 = 12 MHz/2048 = 5859.375 Hz */
#define RTIMER_ARCH_PRESCALER					10
/* One tick: 85.33 us. Using this value we will delay about 1.84 sec
   after a day. */
#define RTIMER_ARCH_SECOND						11719
#endif /* RTIMER_ARCH_RESOLUTION == RTIMER_ARCH_RES_85US */

#if RTIMER_ARCH_RESOLUTION == RTIMER_ARCH_RES_24NS
#define	RTIMER_ARCH_PRESCALER					0	// Division with 2
#define	RTIMER_ARCH_SECOND						42000000    // One tick: 23.77 ns. CPU Clock is 84MHz.
#else
#endif /* RT_RESOLUTION == RES_24NS */

#if RTIMER_ARCH_RESOLUTION == RTIMER_ARCH_RES_95NS
#define	RTIMER_ARCH_PRESCALER					1	// Division with 8
#define	RTIMER_ARCH_SECOND						10500000    // One tick: 95.238 ns. CPU Clock is 84MHz.
#else
#endif /* RT_RESOLUTION == RES_95NS */

#if RTIMER_ARCH_RESOLUTION == RTIMER_ARCH_RES_341NS
#define	RTIMER_ARCH_PRESCALER					2	 // Division with 32
#define	RTIMER_ARCH_SECOND						2625000    // One tick: 341.28 ns. CPU Clock is 84MHz.
#else
#endif /* RT_RESOLUTION == RES_341NS */

#if RTIMER_ARCH_RESOLUTION == RTIMER_ARCH_RES_1_52US
#define	RTIMER_ARCH_PRESCALER					3	 // Division with 128
#define	RTIMER_ARCH_SECOND						656250    // One tick: 1.52 us. CPU Clock is 84MHz.
#else
#endif /* RTIMER_ARCH_RES_1_52US */

rtimer_clock_t rtimer_arch_now(void);

enum rtimer_channel {
	RTIMER_PRIMARY = 1,
	RTIMER_ALTERNATIVE,
};

#if WITH_MULTIPLE_RTIMERS

void rtimer_arch_disable_irq(enum rtimer_channel);
#else
void rtimer_arch_disable_irq(int);
#endif

void rtimer_arch_enable_irq(void);

#endif /* RTIMER_ARCH_H_ */
/** @} */

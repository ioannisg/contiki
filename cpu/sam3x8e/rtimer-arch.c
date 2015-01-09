/**
 * \addtogroup arduino-due-ath9170
 *
 * @{
 */

/*
 * Copyright (c) 2010, STMicroelectronics.
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
 */

/**
* \file
*			Real-timer specific implementation for SAM3X8E.
* \author
*			Ioannis Glaropoulos <ioannisg@kth.se>
*/

#include "sys/energest.h"
#include "sys/rtimer.h"
#include "tc.h"
#include "pmc.h"
#include "interrupt/interrupt_sam_nvic.h"
#include <stdio.h>
#include <stdint-gcc.h>

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define RTIMER_ARCH_MAX_32_TIME	0xFFFFFFFFUL
#define RTIMER_ARCH_MAX_GUARD_TIME 2

/* Most significant bits of the current time */
static uint64_t time_msb = 0;

/* Time of the next rtimer event. Initially set to the maximum value. */
static rtimer_clock_t next_rtimer_time = 0;
#if WITH_MULTIPLE_RTIMERS
static rtimer_clock_t next_alter_rtimer_time = 0;
#endif

/************************************************************************/
/* Interrupt handler for the ID_TC0 Interrupt                                                           
 * [Universal time register overflow]                                                                   */
/************************************************************************/
void TC0_Handler(void)
{
	volatile uint32_t ul_dummy, TC_value;
    
	/* Get the current timer counter value from a 32-bit register */
   TC_value = tc_read_cv(TC0, 0);
	      
	/* Clear status bit to acknowledge interrupt */
   ul_dummy = tc_get_status(TC0, 0);
    
	if ((ul_dummy & TC_SR_CPCS) == TC_SR_CPCS) {
                
		PRINTF("rtimer-arch: TC0:%08x [OVF]\n", TC_value);
      /* Increment the most significant part of the universal time. */                
      time_msb++;
		/* Determine the current time */
		rtimer_clock_t now;
		if (unlikely(TC_value == RTIMER_ARCH_MAX_32_TIME)) {
			/* We are still at the 0xffffffff value (!) */
			now = ((rtimer_clock_t)time_msb << 32);
		}
		else
			now = ((rtimer_clock_t)time_msb << 32) | tc_read_cv(TC0,0);
#if WITH_MULTIPLE_RTIMERS			
		uint32_t tc_channel;
		/* Check both timer channels */
		for (tc_channel=RTIMER_PRIMARY; tc_channel<=RTIMER_ALTERNATIVE; tc_channel++) {
			
			rtimer_clock_t clock_to_wait, next_time;
			
			if (tc_channel == RTIMER_PRIMARY) {				
				clock_to_wait = next_rtimer_time - now;
				next_time = next_rtimer_time;
			} else {				
				clock_to_wait = next_alter_rtimer_time - now;
				next_time = next_alter_rtimer_time;				
			}
			
			if (next_time == 0) {
				/* Nothing to schedule */
				continue;
			}
			
			if (unlikely(next_time <= now)) {
				/* We lost race, this event is now in the past. 
				 * We log an error here. However this may occur
				 * as the event target time is critically close
				 * to the overflow target time. So, we check if
				 * this is the case, and schedule the event for
				 * almost NOW.
				 */
				if (likely(next_time >= now - RTIMER_ARCH_MAX_GUARD_TIME)) {
					PRINTF("rtimer-arch: channel (%u) critically-close\n", tc_channel);
					clock_to_wait = 1;
					goto start_aux_timer;
			
				} else {
					printf("rtimer-arch: lost-race for channel (%u)\n", tc_channel);
					continue;
				}
			}
		
			if(clock_to_wait <= RTIMER_ARCH_MAX_32_TIME) { 
				/* We must set now the Timer Compare Register. It is possible
				 * that the event has already been registered so in this case
				 * we register the timer for the event for a second time which
				 * is definitely not optimal but should work without problems.
				 */
start_aux_timer:                        
				/* Set the auxiliary timer (TC0,1) at the write time interrupt */
				tc_write_rc(TC0, tc_channel, (uint32_t)clock_to_wait);
				/* Set and enable interrupt on RC compare */
				tc_enable_interrupt(TC0, tc_channel, TC_IER_CPCS);
				/* Start the auxiliary timer */
				tc_start(TC0, tc_channel);			
				continue;
			}  						
		}
#else
		rtimer_clock_t clock_to_wait = next_rtimer_time - now;
	
		if (next_rtimer_time == 0) {
			/* Nothing to schedule */
			return;
		}
	
		if (unlikely(next_rtimer_time <= now)) {
		
			if (likely(next_rtimer_time >= now - RTIMER_ARCH_MAX_GUARD_TIME)) {
				PRINTF("rtimer-arch: channel critically-close\n");
				clock_to_wait = 1;
				goto start_aux_timer_no_multi;
						
			} else {
				printf("rtimer-arch: lost-race \n");
				return;
			}
		}
	
		if (clock_to_wait <= RTIMER_ARCH_MAX_32_TIME) {
	start_aux_timer_no_multi:
			/* Set the auxiliary timer (TC0,1) at the write time interrupt */
			tc_write_rc(TC0, 1, (uint32_t)clock_to_wait);
			/* Set and enable interrupt on RC compare */
			tc_enable_interrupt(TC0, 1, TC_IER_CPCS);
			/* Start the auxiliary timer */
			tc_start(TC0, 1);
			return;	
		}

#endif	/* WITH_MULTIPLE_TIMERS */

	} else {
		/* 
		 * Normally we have registered (TC0,0) interrupts only
		 * on RC compare so if we reach here we have an error.
		 */
		printf("rtimer-arch: unknown-interrupt-on-(TC0,0)\n");
   }
}

/************************************************************************/
/* Interrupt handler for ID_TC1 interrupt 
   [Auxiliary (real-time event) timer]                                  */
/************************************************************************/
void TC1_Handler(void)
{
	volatile uint32_t ul_dummy;
   /* Clear status bit to acknowledge interrupt */
   ul_dummy = tc_get_status(TC0, 1);
#if DEBUG
	if ((ul_dummy & TC_SR_CPCS) == TC_SR_CPCS) {
#else
	UNUSED(ul_dummy);	
#endif
		tc_stop(TC0, 1); // Crucial, since the execution may take long time [3-4 TC cycles]
      
		PRINTF("rtimer-arch: compare-event: %16x\r\n", ul_dummy);
      // Disable the next compare interrupt XXX XXX [3-4 TC cycles]
      tc_disable_interrupt(TC0, 1, TC_IDR_CPCS);  
      ENERGEST_ON(ENERGEST_TYPE_IRQ);
      rtimer_run_next(); /* Run the posted task. */
		/* next_rtimer_time shall be set to zero */
		next_rtimer_time = 0;
      ENERGEST_OFF(ENERGEST_TYPE_IRQ);                
#if DEBUG
	} else {
		printf("rtimer-arch: unknown-interrupt-on-(TC0,1)\n");
	}
#endif
}

/************************************************************************/
/* Interrupt handler for ID_TC2 interrupt 
   [Auxiliary (real-time event) timer]                                  */
/************************************************************************/
void TC2_Handler(void)
{
	volatile uint32_t ul_dummy;
   /* Clear status bit to acknowledge interrupt */
   ul_dummy = tc_get_status(TC0, 2);
#if DEBUG
	if ((ul_dummy & TC_SR_CPCS) == TC_SR_CPCS) {
#else
		UNUSED(ul_dummy);
#endif
		tc_stop(TC0, 2); // Crucial, since the execution may take long time [3-4 TC cycles]
		PRINTF("rtimer-arch: compare-event: %16x\r\n", ul_dummy);
		// Disable the next compare interrupt XXX XXX [3-4 TC cycles]
		tc_disable_interrupt(TC0, 2, TC_IDR_CPCS);
		ENERGEST_ON(ENERGEST_TYPE_IRQ);
		rtimer_run_alter(); /* Run the posted task. */
		next_alter_rtimer_time = 0;              
		ENERGEST_OFF(ENERGEST_TYPE_IRQ);  
#if DEBUG
	} else {
		printf("rtimer-arch: unknown-interrupt-on-(TC0,2)\n");
   }
#endif
}

/************************************************************************/
/* Initialize the rtimer [TC peripherals]                               */
/************************************************************************/

void rtimer_arch_init(void)
{       
	/*
    * In this implementation we will make use of two Timer Counters (TC).
    * The first (TC0) is used to count the universal time, and triggers 
    * an interrupt on overflow. The timer runs always, and counts the total 
    * time elapsed since device boot. It is used to extract the current time 
    * (now)
    *
    * The second timer (TC1) is used to schedule events in real time.
    */
   
   /* Configure PMC: Enable peripheral clock on component: TC0, channel 0 [see sam3x8e.h] */
   pmc_enable_periph_clk(ID_TC0);
   /* Configure PMC: Enable peripheral clock on component: TC0, channel 1 [see sam3x8e.h] */
   pmc_enable_periph_clk(ID_TC1);
   /* Configure the TC0 for 10500000 Hz and trigger on RC register compare */
   tc_init(TC0, 0, RTIMER_ARCH_PRESCALER | TC_CMR_CPCTRG);
   /* Configure the TC1 for 10500000 Hz and trigger on RC register compare */
   tc_init(TC0, 1, RTIMER_ARCH_PRESCALER | TC_CMR_CPCTRG);
   /* Disable all interrupts on both counters*/
	tc_disable_interrupt(TC0,0,0b11111111);
   tc_disable_interrupt(TC0,1,0b11111111);
        
   /* Configure the main counter to trigger an interrupt on RC compare (overflow) */
   tc_write_rc(TC0, 0, 0xffffffff);
        
   /* Configure interrupt on the selected timer channel, setting the corresponding callback */
   NVIC_EnableIRQ((IRQn_Type) ID_TC0);
   NVIC_SetPriority((IRQn_Type) ID_TC0,1);
   /* Configure interrupt on the selected timer channel, setting the corresponding callback */
   NVIC_EnableIRQ((IRQn_Type) ID_TC1);
   NVIC_SetPriority((IRQn_Type) ID_TC1,1);
   /* Enable TC0 interrupt on RC Compare */
   tc_enable_interrupt(TC0, 0, TC_IER_CPCS);
   
   /* Start the universal time counter */
   tc_start(TC0, 0);
        
#if	WITH_MULTIPLE_RTIMERS	
	/* Configure PMC: Enable peripheral clock on component: TC0, channel 2 [see sam3x8e.h] */
   pmc_enable_periph_clk(ID_TC2);
   /* Configure the TC1 for 10500000 Hz and trigger on RC register compare */
   tc_init(TC0, 2, RTIMER_ARCH_PRESCALER | TC_CMR_CPCTRG);
   /* Disable all interrupts on counter*/
   tc_disable_interrupt(TC0,2,0b11111111);
   /* Configure interrupt on the selected timer channel, setting the corresponding callback */
   NVIC_EnableIRQ((IRQn_Type) ID_TC2);     
   NVIC_SetPriority((IRQn_Type) ID_TC2,1);
#endif
	PRINTF("rtimer-arch: universal real-timer started.\n");  
}
/*---------------------------------------------------------------------------*/
void rtimer_arch_enable_irq(void)
{
	// XXX Not PORTED!
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t rtimer_arch_now(void)
{
	/* Modified, as now time is 64 bit */
	return  ((rtimer_clock_t)time_msb << 32)|(uint64_t)tc_read_cv(TC0,0);
}
/*---------------------------------------------------------------------------*/
#if WITH_MULTIPLE_RTIMERS
uint8_t rtimer_arch_schedule(rtimer_clock_t t, enum rtimer_channel tc_channel)
{
	if (tc_channel > RTIMER_ALTERNATIVE) {
		PRINTF("rtimer_arch.c: not valid TC channel\n");
		return RTIMER_ERR_ALREADY_SCHEDULED;
	}
	
	PRINTF("rtimer-arch: rtimer_arch_schedule time %llu\r\n", 
		/*((uint32_t*)&t)+1,*/(rtimer_clock_t)t);

	if (tc_channel == RTIMER_PRIMARY)
		next_rtimer_time = t;
	else
		next_alter_rtimer_time = t;
		
	cpu_irq_enter_critical();
	rtimer_clock_t now = rtimer_arch_now();
	if (unlikely(t < now)) {
		
		printf("RTIMER-ARCH: event in past; time: %llu, current: %llu\n",t, now);
		/* Signal an error - indicate that the time is not scheduled. */
		cpu_irq_leave_critical();
		return RTIMER_ERR_TIME;
	}
	
	rtimer_clock_t clock_to_wait = t - now;
	
	if(clock_to_wait <= RTIMER_ARCH_MAX_32_TIME){ 
		// We must set now the Timer Compare Register.
		tc_disable_interrupt(TC0, tc_channel, TC_IDR_CPCS);
		tc_stop(TC0, tc_channel);
		// Set the auxiliary/alternative timer (TC0,1) at the write time interrupt
		tc_write_rc(TC0, tc_channel, (uint32_t)(t-rtimer_arch_now()));
		// Set and enable interrupt on RC compare
		tc_enable_interrupt(TC0, tc_channel, TC_IER_CPCS);
		// Start the auxiliary timer
		tc_start(TC0, tc_channel);

		PRINTF("rtimer_arch.c: Timer (%u) started on time %llu.\n",
			tc_channel, rtimer_arch_now());
	}
	/* Otherwise, the compare register will be set at overflow interrupt
	 * closer to the rtimer event.
	 */
	cpu_irq_leave_critical();
	return RTIMER_OK;
}
/*---------------------------------------------------------------------------*/
void rtimer_arch_disable_irq(enum rtimer_channel tc_channel)
{
	rtimer_clock_t *next_time_p;
	
	if (tc_channel > RTIMER_ALTERNATIVE) {
		PRINTF("rtimer_arch.c: not valid TC channel\n");
		return;
	}
	
	tc_stop(TC0, (uint32_t)tc_channel);
	tc_disable_interrupt(TC0, (uint32_t)tc_channel, TC_IDR_CPCS);
	
	if (tc_channel == RTIMER_PRIMARY)
		next_time_p = &next_rtimer_time;
	else
		next_time_p = &next_alter_rtimer_time;
	
	if (unlikely((*next_time_p) == 0))
		PRINTF("rtimer_arch.c: disable timer but next_rtimer_time not 0\n");
	else
		*next_time_p = 0;
}
/*---------------------------------------------------------------------------*/
#else
uint8_t rtimer_arch_schedule(rtimer_clock_t t)
{	
	PRINTF("rtimer-arch: rtimer_arch_schedule time %llu\r\n", 
		/*((uint32_t*)&t)+1,*/(rtimer_clock_t)t);
		
	next_rtimer_time = t;
	
	cpu_irq_enter_critical();
	rtimer_clock_t now = rtimer_arch_now();
	if (unlikely(t < now)) {
		
		printf("RTIMER-ARCH: event in past; time: %llu, current: %llu\n",t, now);
		/* Signal an error - indicate that the time is not scheduled. */
		cpu_irq_leave_critical();
		return RTIMER_ERR_TIME;
	}
	
	rtimer_clock_t clock_to_wait = t - now;
	
	if(clock_to_wait <= RTIMER_ARCH_MAX_32_TIME){ 
		// We must set now the Timer Compare Register.
		tc_disable_interrupt(TC0, 1, TC_IDR_CPCS);
		tc_stop(TC0, 1);
		// Set the auxiliary/alternative timer (TC0,1) at the write time interrupt
		tc_write_rc(TC0, 1, (uint32_t)(t-rtimer_arch_now()));
		// Set and enable interrupt on RC compare
		tc_enable_interrupt(TC0, 1, TC_IER_CPCS);
		// Start the auxiliary timer
		tc_start(TC0, 1);

		PRINTF("rtimer_arch.c: Timer started on time %llu.\n", rtimer_arch_now());
	}
	/* Otherwise, the compare register will be set at overflow interrupt
	 * closer to the rtimer event.
	 */
	cpu_irq_leave_critical();
	return RTIMER_OK;
}
/*---------------------------------------------------------------------------*/
void rtimer_arch_disable_irq(void)
{	
	tc_stop(TC0, 1);
	tc_disable_interrupt(TC0, 1, TC_IDR_CPCS);
	
	if (unlikely(next_rtimer_time == 0))
		PRINTF("rtimer_arch.c: disable timer but next_rtimer_time not 0\n");
	else
		next_rtimer_time = 0;
}
/*---------------------------------------------------------------------------*/
#endif /* WITH_MULTIPLE_RTIMERS */
/** @} */

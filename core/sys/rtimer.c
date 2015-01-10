/**
 * \addtogroup rt
 * @{
 */

/**
 * \file
 *         Implementation of the architecture-agnostic parts of the real-time timer module.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */


/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#include "sys/rtimer.h"
#include "contiki.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

static struct rtimer *next_rtimer;

#if WITH_MULTIPLE_RTIMERS

#include "list.h"
#include "interrupt/interrupt_sam_nvic.h"
#define RTIMER_GUARD_TIME       RTIMER_ARCH_GUARD_TIME
#ifdef RTIMER_PENDING_CONF_LEN
#define RTIMER_PENDING_LEN      RTIMER_PENDING_CONF_LEN
#else   /* RTIMER_PENDING_CONF_LEN */
#define RTIMER_PENDING_LEN      4
#endif /* RTIMER_PENDING_CONF_LEN */
/* Declare a LIST of pending real timers. */
LIST(rtimers_list);
/* Alternative real timer for short events. */
static struct rtimer *alt_rtimer;



/*---------------------------------------------------------------------------*/
void
rtimer_init(void)
{
        next_rtimer = NULL;
        alt_rtimer = NULL;
        rtimer_arch_init();
        /* Initialize the list of pending real timers. */
        list_init(rtimers_list);
}
/*---------------------------------------------------------------------------*/
void
rtimer_stop(struct rtimer *_rtimer)
{
	cpu_irq_enter_critical();
	if (alt_rtimer == _rtimer) {
		rtimer_arch_disable_irq(RTIMER_ALTERNATIVE);
		alt_rtimer = NULL;
		cpu_irq_leave_critical();
		return;
	}
	/*
	 * If the timer is not scheduled just remove from the list
	 * otherwise remove from the list and stop the scheduling,
	 * and schedule the next timer, if any.
	 */
	list_remove(rtimers_list, _rtimer);
	if (next_rtimer != _rtimer) {
		/* This timer was not yet scheduled. */
		cpu_irq_leave_critical();
		return;
	}
	/* Otherwise disable it */
	rtimer_arch_disable_irq(RTIMER_PRIMARY);
	
	/* Point the next_rtimer to the head of the list. */
	next_rtimer = list_head(rtimers_list);                  
	
	/* Attempt to schedule the next real timer if such exists. */
	while(next_rtimer != NULL) {
		if(rtimer_arch_schedule(next_rtimer->time, RTIMER_PRIMARY) != RTIMER_OK) {
			PRINTF("rtimer.c: next timer for: %llu! could not be scheduled\n",
				next_rtimer->time);
			/* Remove real timer. However, the caller of the timer must be 
			 * notified that the interrupt it is waiting for will not occur.
			 * This event is super rare and can only occur, if someone sets
			 * two real timers at the same time in the future.
			 */
			list_remove(rtimers_list, next_rtimer);
			next_rtimer = list_head(rtimers_list);
			continue;
		}
		break;
	}
	cpu_irq_leave_critical();               
}
/*---------------------------------------------------------------------------*/
static void
rtimer_add_to_list(struct rtimer* _rtimer)
{       
        struct rtimer *s = list_head(rtimers_list);
        
        if(_rtimer->time < s->time) {
                /* Add to front. */
                list_push(rtimers_list, _rtimer);
                return;
        }
        /* We add in the middle of the list. */
        for (s=list_head(rtimers_list); s->next!=NULL; s=list_item_next(s)) {
                
                if (_rtimer->time < s->next->time) {
                        list_insert(rtimers_list, s, _rtimer);
                        return;
                }
        }
        /* Add to tail. */
        list_add(rtimers_list, _rtimer);        
}
/*---------------------------------------------------------------------------*/
int
rtimer_set(struct rtimer *rtimer, rtimer_clock_t time,
           rtimer_clock_t duration,
           rtimer_callback_t func, void *ptr)
{
	rtimer->func = func;
	rtimer->ptr = ptr;
	rtimer->next = NULL;
	rtimer->time = time;
	
	/* If the next_rtimer is empty there is no waiting real timer, so we can
	 * proceed as normally; place the incoming real timer in the next_rtimer
	 * location and schedule its execution. BUT we also place the real timer
	 * in the list, just to have a COPY available if reschedule is needed.
	 */
	if(next_rtimer == NULL) {
		if (list_length(rtimers_list) < RTIMER_PENDING_LEN) {
			/* Attempt to push in the list. */
			list_push(rtimers_list, rtimer);
		} else {
			PRINTF("rtimer.c: ERROR; list full but no next_timer scheduled.\n");
			return RTIMER_ERR_FULL;
		}
		
		/* Attempt to schedule the real timer. */
		int result = rtimer_arch_schedule(time, RTIMER_PRIMARY);
		if (result == RTIMER_OK) {
			/* We can directly store this real timer in the next_rtimer place. */
			next_rtimer = rtimer;
		} else {
			list_remove(rtimers_list, rtimer);
			next_rtimer = list_head(rtimers_list);
		}
		return result;
	
	} else {
		/* If this timer is critically close to any scheduled timers inside
		 * the list of pending timers, we use the alternative timer counter
		 */
		struct rtimer *s;
		for (s=list_head(rtimers_list); s!=NULL; s=list_item_next(s)) {
			
			if ((s->time > time && s->time - time < RTIMER_ARCH_GUARD_TIME ) ||
					(s->time <= time && time - s->time < RTIMER_ARCH_GUARD_TIME)) {
						
				if(alt_rtimer == NULL) {
					/* Attempt to schedule timer on the alternative channel. */
					uint8_t result =  rtimer_arch_schedule(time, RTIMER_ALTERNATIVE);
					if (result == RTIMER_OK) {
						alt_rtimer = rtimer;
					}
					return result;
				} else {
					return RTIMER_ERR_FULL;
				}
			}
		} 
		
		/* The next_rtimer is not empty. We first check whether the incoming
		 * real timer needs an execution time that is earlier than the real
		 * timer that is already scheduled. In such a case we have to cancel
		 * the current real timer, schedule this one and push the current one
		 * to the list, given that the list can accommodate it, otherwise we
		 * drop the newly arrived real timer.
		 */
		if (time < next_rtimer->time) {
			
			if (list_length(rtimers_list) < RTIMER_PENDING_LEN) {
				list_push(rtimers_list, rtimer);
				/* Attempt to schedule the new real timer. */
				int result = rtimer_arch_schedule(time, RTIMER_PRIMARY);
				if (result == RTIMER_OK) {
					/* We can directly store this real timer in the next_rtimer place. */
					next_rtimer = rtimer;
				} else {
					list_remove(rtimers_list, rtimer);
					/* We bring back the previously scheduled timer. */
					next_rtimer = list_head(rtimers_list);
					
					if (result == RTIMER_ERR_TIME) {
						/* The previous timer is still scheduled */
					} else {
						if (rtimer_arch_schedule(next_rtimer->time, RTIMER_PRIMARY) != RTIMER_OK) {
							PRINTF("rtimer.c: could not bring back rtimer\n");
							/* This is a serious error. A previously scheduled timer 
						 	 * that was unscheduled, could not be brought back. The
							 * related process, however, gets no notification! FIXME
							 */
						}
					}					
				}
				return result;
				
			} else {
				PRINTF("rtimer.c: List is full\n");
				return RTIMER_ERR_FULL;
			}
		} else {
			/* The incoming real timer needs an execution time that is later 
          * than the one that is currently scheduled, so we place the one
			 * that arrived now directly to the pending list, assuming that
			 * there is space left. 
			 */
			if (list_length(rtimers_list) < RTIMER_PENDING_LEN) {
					cpu_irq_enter_critical();
					rtimer_add_to_list(rtimer);
					cpu_irq_leave_critical();
					return RTIMER_OK;
			} else {
				PRINTF("RTIMER: List is full\n");
				return RTIMER_ERR_FULL;
			}
		}
	}
}
/*---------------------------------------------------------------------------*/
void
rtimer_run_next(void) 
{
	/* If the next_rtimer pointer is NULL we do nothing. 
	 * But this should never occur.
	 */
	if(next_rtimer == NULL) {
		return;
	}
	/* Temporarily store the real timer. */
	struct rtimer *t;
	t = next_rtimer;
	next_rtimer = NULL;
/*              
        if (list_head(rtimers_list) == NULL || ((struct rtimer*)list_head(rtimers_list)) != t) {
                PRINTF("RTIMER ERROR: run_next finds empty list head/erroneous\n");
                return;
        }
*/
	/* Remove the list head. */
	list_pop(rtimers_list);
	
	/* Pass the following pending timer on the next timer pointer. */
	next_rtimer = list_head(rtimers_list);
	
	/* Attempt to schedule the next real timer if it exists. */
	while(next_rtimer != NULL) {
		if(rtimer_arch_schedule(next_rtimer->time, RTIMER_PRIMARY) != RTIMER_OK) {
			PRINTF("rtimer.c: run-next couldn't schedule next timer for: %llu.\n",
					next_rtimer->time);
			/* Remove real timer. However, the caller of the timer must be
			 * notified that the interrupt it is waiting for will not occur.
			 * This event is super rare and can only occur, if someone sets
			 * two real timers at the same time in the future.  FIXME
			 */
			list_remove(rtimers_list, next_rtimer);
			next_rtimer = list_head(rtimers_list);
		}
		break;
	}
	/* We execute the callback function now. */
	t->func(t, t->ptr);
	
	return;
}
/*---------------------------------------------------------------------------*/
void
rtimer_run_alter(void)
{
        if(alt_rtimer == NULL) {
                return;
        }
        /* Temporarily store the real timer. */
        struct rtimer *t;
        t = alt_rtimer;
        alt_rtimer = NULL;

        /* We execute the callback function now. */
        t->func(t, t->ptr);
}
#else /* WITH_MLTIPLE_RTIMERS */
/*---------------------------------------------------------------------------*/
void
rtimer_init(void)
{
  rtimer_arch_init();
}
/*---------------------------------------------------------------------------*/
int
rtimer_set(struct rtimer *rtimer, rtimer_clock_t time,
	   rtimer_clock_t duration,
	   rtimer_callback_t func, void *ptr)
{
  int first = 0;

  PRINTF("rtimer_set time %d\n", time);

  if(next_rtimer == NULL) {
    first = 1;
  }

  rtimer->func = func;
  rtimer->ptr = ptr;

  rtimer->time = time;
  next_rtimer = rtimer;

  if(first == 1) {
    rtimer_arch_schedule(time);
  }
  return RTIMER_OK;
}
/*---------------------------------------------------------------------------*/
void
rtimer_run_next(void)
{
  struct rtimer *t;
  if(next_rtimer == NULL) {
    return;
  }
  t = next_rtimer;
  next_rtimer = NULL;
  t->func(t, t->ptr);
  if(next_rtimer != NULL) {
    rtimer_arch_schedule(next_rtimer->time);
  }
  return;
}
/*---------------------------------------------------------------------------*/
#endif /* WITH_MLTIPLE_RTIMERS */
/** @}*/

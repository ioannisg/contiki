/*
 * pend_SV.c
 *
 * Created: 2/25/2014 10:29:23 AM
 *  Author: Ioannis Glaropoulos
 */ 
#include "sam3x8e.h"
#include "core_cm3.h"
#include "pend-SV.h"
#include "interrupt_sam_nvic.h"
#include "pmc.h"
#include "platform-conf.h"

#ifndef PEND_SV_CONF_PRIORITY
#define PEND_SV_CONF_PRIORITY 7
#endif

/* The low-priority call-back function and assigned pointer */
static void (*pend_sv_handler) (void *ptr) = NULL;
static void *pend_sv_ptr = NULL;
/*---------------------------------------------------------------------------*/
void
PendSV_Handler(void)
{
  INTERRUPTS_DISABLE();
  /* Store current function & pointer */
  pend_sv_callback_t current_handler = pend_sv_handler;
  void *current_ptr = pend_sv_ptr;
  /* Clear data & flag, so a SW-IRQ can be registered again */
  pend_sv_handler = NULL;
  pend_sv_ptr = NULL;
  SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk;
  INTERRUPTS_ENABLE();
  /* Execute SW-IRQ handler without blocking high-priority IRQ sources */
  if (current_handler != NULL)
    current_handler(current_ptr);
}
/*---------------------------------------------------------------------------*/
void
pend_sv_init(void)
{
  /* Assign low priority */
  NVIC_SetPriority(PendSV_IRQn, PEND_SV_CONF_PRIORITY);	
}
/*---------------------------------------------------------------------------*/
uint8_t
pend_sv_register_handler(pend_sv_callback_t callback, void *ptr)
{
  if (pend_sv_handler != NULL) { 
    if (pend_sv_handler != callback || pend_sv_ptr != ptr) {
      return 0;
    }
  } else {
    pend_sv_handler = callback;
    pend_sv_ptr = ptr; 
  }  
  SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
  return 1;
}
/*---------------------------------------------------------------------------*/
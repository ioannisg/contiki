/*
 * pend_SV.c
 *
 * Created: 2/25/2014 10:29:23 AM
 *  Author: Ioannis Glaropoulos
 */ 

#include <stdio.h>
#include "sam3x8e.h"
#include "core_cm3.h"
#include "pend-SV.h"
#include "interrupt\interrupt_sam_nvic.h"
#include "pmc.h"


/* The low-priority call-back */
static void (*pend_sv_handler) (void *ptr) = NULL;
static void *pend_sv_ptr = NULL;

/* The IRQ Handler */
#if 0
void EMAC_Handler(void) {
		
	if (pend_sv_handler != NULL)
		pend_sv_handler(pend_sv_ptr);
		
	NVIC_ClearPendingIRQ(EMAC_IRQn);
}
#endif

/* The IRQ Handler */
void PendSV_Handler(void)
{
	pend_sv_callback_t current_handler = pend_sv_handler;
	pend_sv_handler = NULL;
	
	if (current_handler != NULL)
		current_handler(pend_sv_ptr);
	SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk;
}

void pend_sv_init(void)
{
	/* Assign low priority */
	//pmc_enable_periph_clk(ID_EMAC);
	//NVIC_EnableIRQ((IRQn_Type)EMAC_IRQn);
	//NVIC_SetPriority(EMAC_IRQn, 7);	
	NVIC_SetPriority(PendSV_IRQn, 7);	
}


uint8_t
pend_sv_register_handler(pend_sv_callback_t callback, void *ptr)
{
	if (pend_sv_handler != NULL) {
		return 0;
	}
	pend_sv_handler = callback;
	pend_sv_ptr = ptr;
	//NVIC_SetPendingIRQ(EMAC_IRQn);
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
	return 1;
}





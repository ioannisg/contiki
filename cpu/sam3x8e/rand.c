/**
 * \addtogroup arduino-due-ath9170
 *
 * @{
 */

/**
* \file
*			Random number functions for SAM3X8E.
* \author
*			Ioannis Glaropoulos <ioannisg@kth.se>
*/

#include "lib/random.h"
#include "sam3x8e.h"
#include "pmc.h"
#include "trng.h"

#if (RANDOM_RAND_MAX != 0xffff)
#warning "RANDOM_RAND_MAX is not defined as 65535."
#endif

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
#include "core_cm3.h"


#define SAM3X8E_MAX_RAND	0xffffffff

/* Semaphore indicating that the random generated number is ready. */
static volatile uint8_t trng_is_ready;

static volatile uint8_t trng_is_configured = 0;

/* 4-byte random number */
static uint32_t trng_rand;

/* Handling the interrupts from the random generator. */
void 
TRNG_Handler(void)
{
	PRINTF("Done\n");
	/* Disable the TRNG interrupt. */
	trng_disable_interrupt(TRNG);
	
	uint32_t status;
	
	status = trng_get_interrupt_status(TRNG);
	
	if ((status & TRNG_ISR_DATRDY) == TRNG_ISR_DATRDY) {
		
		trng_rand = trng_read_output_data(TRNG);
		#if DEBUG
		printf("-- TRNG Random Value: %u --\n\r", trng_rand);
		#endif
		
	} else {
		
		printf("TGEN ERROR: TRNG Not ready!\n");
	}
	
	/* Set semaphore ready. Signal an error if already set. */
	if(trng_is_ready != 0) {
		printf("TGEN ERROR: TRNG flag is already set!\n");
	
	} else {
		
		trng_is_ready = 1;
	}
}
/*--------------------------------------------------------------------------*/
/**
 *  Configure the real random generator.
 */
static void
configure_trng(void) 
{
	/* Configure PMC */
	pmc_enable_periph_clk(ID_TRNG);
	
	/* Enable TRNG */
	trng_enable(TRNG);
	
	NVIC_DisableIRQ(TRNG_IRQn);
	NVIC_ClearPendingIRQ(TRNG_IRQn);
	NVIC_SetPriority(TRNG_IRQn, 0);
	NVIC_EnableIRQ(TRNG_IRQn);
	
	/* Set the TRNG ready semaphore. */
	trng_is_ready = 1;
}

/*--------------------------------------------------------------------------*/
/**
 * Generate a uniformly distributed random number in [0..65535]
 * Note that the function can NOT be called within interrupt context.
 */
int
rand(void)
{
	if (!trng_is_configured) {
		
		configure_trng();
		trng_is_configured = 1;
	}
	
	/* Clear the random number. */
	trng_rand = 0;
  
	/* Cleared the completion flag. */
	if (trng_is_ready == 0) {
	  
		printf("TRNG ERROR: Semaphore already cleared!\n");
		return 0;
  
	} else {
	  
		trng_is_ready = 0;
	}

	/* Enable interrupts from the real random generator. */
	trng_enable_interrupt(TRNG);
  
	while(trng_is_ready == 0) {
	  /* Wait [block until the interrupt has set the random number] */	  
	}
  
	/* Now the random number is already stored. Return the 16-bit LSB only. */
	return trng_rand & 0xffff;
}
/*--------------------------------------------------------------------------*/
/*
 *	It does nothing, since the rand already generates true random numbers.
 */
void
srand(unsigned int seed)
{
}
/** @} */

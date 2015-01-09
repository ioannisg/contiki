/*
 * wire_digital.c
 *
 * Created: 1/25/2014 9:55:36 PM
 *  Author: Ioannis Glaropoulos
 */ 

#include "wire_digital.h"
#include "arduino_due.h"
#include "component_pio.h"
#include "pio_handler.h"
#include "pmc.h"
#include "core_cm3.h"
#include "pio.h"
#include <stdio.h>

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

void 
pinMode( uint32_t ulPin, uint32_t ulMode )
{
	if ( arduino_pin_description[ulPin].ulPinType == PIO_NOT_A_PIN ) {
		PRINTF("ERORR: pinMode: Not a pin.\n");
		return;
	}

	switch ( ulMode )
	{
		case INPUT:
			/* Enable peripheral for clocking input */
			pmc_enable_periph_clk(arduino_pin_description[ulPin].ulPeripheralId);
			int result = pio_configure(arduino_pin_description[ulPin].pPort,
				PIO_INPUT,arduino_pin_description[ulPin].ulPin,0);
			if (result == 0) {
				PRINTF("ERROR while configuring input pin.\n");
			}
			break;
		case INPUT_PULLUP:
			/* Enable peripheral for clocking input */
			pmc_enable_periph_clk( arduino_pin_description[ulPin].ulPeripheralId ) ;
			pio_configure(arduino_pin_description[ulPin].pPort,
				PIO_INPUT,arduino_pin_description[ulPin].ulPin,PIO_PULLUP);
			break;
		case OUTPUT:
			pio_configure(arduino_pin_description[ulPin].pPort,PIO_OUTPUT_1,
				arduino_pin_description[ulPin].ulPin,
				arduino_pin_description[ulPin].ulPinConfiguration ) ;

			/* if all pins are output, disable PIO Controller clocking, reduce power consumption */
			if ( arduino_pin_description[ulPin].pPort->PIO_OSR == 0xffffffff )
			{
				PRINTF("INFO: Disabling PIO Controller as all pins are outputs.\n");
				pmc_disable_periph_clk( arduino_pin_description[ulPin].ulPeripheralId ) ;
			}
			break;
		default:
			PRINTF("WARNING: Unrecognized PIN mode.\n");
			break;
	}
}


void 
configure_input_pin_interrupt_enable(uint32_t pin_number, uint32_t pin_attribute,
	uint32_t irq_attr, void (*pin_irq_handler) (uint32_t, uint32_t),
	int irq_priority_level)
{
	/* Derive the PIN Bank of the desired PIN number. */
	Pio* pin_pio_bank = (arduino_pin_description[pin_number]).pPort;
	
	/* Extract the ID of the desired PIN bank. */
	uint32_t pin_pio_id = (arduino_pin_description[pin_number]).ulPeripheralId; 
		
	/* Extract the ID of the desired PIN. */
	uint32_t pin_id = (arduino_pin_description[pin_number]).ulPin;
	
	/* Enable peripheral clocking on the desired port. */
	pmc_enable_periph_clk(pin_pio_id);
	
	/* Configure the desired PIN as input. */
	pio_set_input(pin_pio_bank, pin_id, pin_attribute);
	
	/* Configure the interrupt mode of the desired PIN. */
	pio_handler_set(pin_pio_bank, pin_pio_id, pin_id, irq_attr, pin_irq_handler);
	
	switch(pin_pio_id) {
			
		case ID_PIOA:
			if (irq_priority_level == -1) {
				/* The interrupt on the Cortex-M3 is given highest priority. */
				NVIC_SetPriority(PIOA_IRQn,0);
			} else {
				NVIC_SetPriority(PIOA_IRQn,irq_priority_level);
			}
			/* Register the interrupt on the Cortex-M3. */
			NVIC_EnableIRQ(PIOA_IRQn);
			break;
		case ID_PIOB:
			if (irq_priority_level == -1) {
				/* The interrupt on the Cortex-M3 is given highest priority. */
				NVIC_SetPriority(PIOB_IRQn,0);
			} else {
				NVIC_SetPriority(PIOB_IRQn,irq_priority_level);
			}
			/* Register the interrupt on the Cortex-M3. */
			NVIC_EnableIRQ(PIOB_IRQn);
			break;
		case ID_PIOC:
			if (irq_priority_level == -1) {
				/* The interrupt on the Cortex-M3 is given highest priority. */
				NVIC_SetPriority(PIOC_IRQn,0);
			} else {
				NVIC_SetPriority(PIOC_IRQn,irq_priority_level);
			}
			/* Register the interrupt on the Cortex-M3. */
			NVIC_EnableIRQ(PIOC_IRQn);
			break;
		case ID_PIOD:
			if (irq_priority_level == -1) {
				/* The interrupt on the Cortex-M3 is given highest priority. */
				NVIC_SetPriority(PIOD_IRQn,0);
			} else {
				NVIC_SetPriority(PIOD_IRQn,irq_priority_level);
			}
			/* Register the interrupt on the Cortex-M3. */
			NVIC_EnableIRQ(PIOD_IRQn);
			break;
		default:
			printf("ERROR IRQ Type resolve error!\n");
			return;
	}
		
	/* Enable the interrupt on the desired input PIN. */
	pio_enable_interrupt(pin_pio_bank, pin_id);
}


void 
configure_input_pin_interrupt_disable(uint32_t pin_number)
{
	/* Derive the PIN Bank of the desired PIN number. */
	Pio* pin_pio_bank = (arduino_pin_description[pin_number]).pPort;
	
	/* Extract the ID of the desired PIN bank. */
	uint32_t pin_pio_id = (arduino_pin_description[pin_number]).ulPeripheralId;
	
	/* Extract the ID of the desired PIN. */
	uint32_t pin_id = (arduino_pin_description[pin_number]).ulPin;
	
	/* Enable peripheral clocking on the desired port. */
	pmc_disable_periph_clk(pin_pio_id);
	
	switch(pin_pio_id) {
		
		case ID_PIOA:
		/* Register the interrupt on the Cortex-M3. */
		NVIC_DisableIRQ(PIOA_IRQn);
		break;
		case ID_PIOB:
		/* Register the interrupt on the Cortex-M3. */
		NVIC_DisableIRQ(PIOB_IRQn);
		break;
		case ID_PIOC:
		/* Register the interrupt on the Cortex-M3. */
		NVIC_DisableIRQ(PIOC_IRQn);
		break;
		case ID_PIOD:
		/* Register the interrupt on the Cortex-M3. */
		NVIC_DisableIRQ(PIOD_IRQn);
		break;
		default:
		printf("ERROR IRQ Type resolve error!\n");
		return;
	}
	
	/* Enable the interrupt on the desired input PIN. */
	pio_disable_interrupt(pin_pio_bank, pin_id);
}


void 
configure_output_pin( uint32_t pin_number,	const uint32_t pin_default_level, 
	const uint32_t ul_multidrive_enable, const uint32_t ul_pull_up_enable )
{
	/* Derive the PIN Bank of the desired PIN number. */
	Pio* pin_pio_bank = (arduino_pin_description[pin_number]).pPort;
	
	/* Extract the ID of the desired PIN bank. */
	uint32_t pin_pio_id = (arduino_pin_description[pin_number]).ulPeripheralId;
	
	/* Extract the ID of the desired PIN. */
	uint32_t pin_id = (arduino_pin_description[pin_number]).ulPin;
	
	/* Enable peripheral clocking on the desired port. */
	pmc_enable_periph_clk(pin_pio_id);
	
	/* Configure the desired PIN as output. */
	pio_set_output(pin_pio_bank, pin_id, pin_default_level, 
		ul_multidrive_enable, ul_pull_up_enable);
		
	/* DEBUG */
	if (pin_default_level == LOW)
		PRINTF("Output PIN %u: %lu.\n", pin_number, pio_get(pin_pio_bank, PIO_TYPE_PIO_OUTPUT_0, pin_id));
	else if (pin_default_level == HIGH)
		PRINTF("Output PIN %u: %lu.\n", pin_number, pio_get(pin_pio_bank, PIO_TYPE_PIO_OUTPUT_1, pin_id));
	else
		printf("ERROR: Unrecognized default output pin level.\n");	
}


void 
wire_digital_write( uint32_t pin_number, uint32_t val )
{
	/* Derive the PIN Bank of the desired PIN number. */
	Pio* pin_pio_bank = (arduino_pin_description[pin_number]).pPort;
	
	/* Extract the ID of the desired PIN. */
	uint32_t pin_id = (arduino_pin_description[pin_number]).ulPin;
	
	if (val == LOW) {
		/* Clear the pin. */
		pio_clear(pin_pio_bank, pin_id);	
	
	} else if (val == HIGH) {
		
		/* Set the pin. */
		pio_set(pin_pio_bank, pin_id);	
	}	
}


uint32_t 
wire_digital_read(uint32_t pin_number, const pio_type_t pin_type)
{	
	if ( arduino_pin_description[pin_number].ulPinType == PIO_NOT_A_PIN )
	{
		/* This is not a correct pin type.  */
		return LOW ;
	}	
	/* Derive the PIN Bank of the desired PIN number. */
	Pio* pin_pio_bank = (arduino_pin_description[pin_number]).pPort;
	
	/* Extract the ID of the desired PIN. */
	uint32_t pin_id = (arduino_pin_description[pin_number]).ulPin;
	
	if (pio_get(pin_pio_bank, pin_type, pin_id) != 0)
		return HIGH;
	else
		return LOW;
}

/*
 * wire_digital.h
 *
 * Created: 1/25/2014 9:48:43 PM
 *  Author: Ioannis Glaropoulos
 */ 

#include "sam3x8e.h"
#include "pio.h"

#ifndef WIRE_DIGITAL_H_
#define WIRE_DIGITAL_H_

#define INPUT			0x0
#define OUTPUT			0x1
#define INPUT_PULLUP	0x2

/************************************************************************/
/* Configure an input pin as an interrupt source                        */
/************************************************************************/
void configure_input_pin_interrupt_enable(uint32_t pin_number, 
	uint32_t pin_attribute,
	uint32_t irq_attr, 
	void (*pin_irq_handler) (uint32_t, uint32_t),
	int irq_priority_level);
	
/************************************************************************/
/* Disable an interrupt from a pre-defined input pin.                   */
/************************************************************************/	
void configure_input_pin_interrupt_disable(uint32_t pin_number);

/************************************************************************/
/* Configure an output pin.                                             */
/************************************************************************/
void configure_output_pin( uint32_t pin_number, 
	const uint32_t pin_default_level,
	const uint32_t ul_multidrive_enable, 
	const uint32_t ul_pull_up_enable );

/************************************************************************/
/* Write a binary value on an output pin.                               */
/************************************************************************/
void wire_digital_write(uint32_t pin_number, uint32_t val);

/************************************************************************/
/* Read from an input pin.                                              */
/************************************************************************/
uint32_t wire_digital_read(uint32_t pin_number, const pio_type_t pin_type);
void pinMode( uint32_t ulPin, uint32_t ulMode );

#endif /* WIRE_DIGITAL_H_ */
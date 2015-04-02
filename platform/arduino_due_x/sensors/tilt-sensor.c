/*
 * tilt_sensor.c
 *
 * Created: 10/29/2013 6:32:22 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "platform-conf.h"
#include "tilt-sensor.h"
#include <stdio.h>
//#include "wire_digital.h"

#ifdef TILT_SENSOR_CONF_PIN_NUMBER
#define TILT_SENSOR_PIN_NUMBER TILT_SENSOR_CONF_PIN_NUMBER
#else
#define TILT_SENSOR_PIN_NUMBER	17 // Default PIN for the tilt sensor
#endif
/*---------------------------------------------------------------------------*/
static void 
tilt_handler(void) {
	
	printf("Tilt\n");
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
	/* Configure tilt sensor. */
//	configure_input_pin_interrupt_enable(TILT_SENSOR_PIN_NUMBER,
	//										PIO_PULLUP,
		//									PIO_IT_RISE_EDGE,
			//								tilt_handler);
}
/*---------------------------------------------------------------------------*/
static int
value(int type)
{
	/* Tilt sensor returns no type. */
	return 0;
}
/*---------------------------------------------------------------------------*/
static int
configure(int type, int value)
{
	switch(type){
		case SENSORS_HW_INIT:
			init();
			return 1;
		case SENSORS_ACTIVE:
			return 1;
	}

	return 0;
}
/*---------------------------------------------------------------------------*/
static int
status(int type)
{
	switch(type) {
		
		case SENSORS_READY:
		return 1;
	}
	
	return 0;
}
/*---------------------------------------------------------------------------*/
SENSORS_SENSOR(tilt_sensor, TILT_SENSOR,
value, configure, status);
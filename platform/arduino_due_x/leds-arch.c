/**
* \addtogroup cc2538-smartrf
* @{
*
* \defgroup cc2538dk-leds SmartRF06EB LED driver
*
* LED driver implementation for the TI SmartRF06EB + cc2538EM
* @{
*
* \file
* LED driver implementation for the TI SmartRF06EB + cc2538EM
*/
#include "contiki.h"
#include "dev/leds.h"
#include "arduino_due_x.h"
#include "wire_digital.h"
#define ARDUINO_ORANGE_LED_PIN 13
/*---------------------------------------------------------------------------*/
void
leds_arch_init(void)
{
  /* LED is already initialized in init, but we could move it here. */
  configure_output_pin(13, LOW, DISABLE, DISABLE);
}
/*---------------------------------------------------------------------------*/
unsigned char
leds_arch_get(void)
{
  return 
    (wire_digital_read(ARDUINO_ORANGE_LED_PIN, PIO_TYPE_PIO_OUTPUT_0) == 0) ? 0 : 1;
}
/*---------------------------------------------------------------------------*/
void
leds_arch_set(unsigned char leds)
{
  if(leds == 0) {
    wire_digital_write(ARDUINO_ORANGE_LED_PIN, LOW);
  } else {
    wire_digital_write(ARDUINO_ORANGE_LED_PIN, HIGH);
  } 
}
/*---------------------------------------------------------------------------*/
/**
* @}
* @}
*/

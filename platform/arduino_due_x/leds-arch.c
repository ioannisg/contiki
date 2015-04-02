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
#include "reg.h"
#include "dev/leds.h"
#include "arduino_due_x.h"
#define ARDUINO_ORANGE_LED 1
/*---------------------------------------------------------------------------*/
void
leds_arch_init(void)
{
  /* LED is already initialized in init, but we could move it here. */
}
/*---------------------------------------------------------------------------*/
unsigned char
leds_arch_get(void)
{
  // TODO Read the PIN status and return 1 or 0 as we have a single LED.
  return 0;
}
/*---------------------------------------------------------------------------*/
void
leds_arch_set(unsigned char leds)
{
  // TODO leds is a mask (currently single LED), so we only check the last bit.
}
/*---------------------------------------------------------------------------*/
/**
* @}
* @}
*/

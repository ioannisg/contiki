/*
 * ar9170_led.c
 *
 * Created: 2/7/2014 6:08:36 PM
 *  Author: ioannisg
 */ 
#include "ar9170_hw.h"
#include "ar9170_cmd.h"

int 
ar9170_led_set_state(struct ar9170 *ar, const uint32_t led_state)
{
          return ar9170_write_reg(ar, AR9170_GPIO_REG_PORT_DATA, led_state);
}
  
int 
ar9170_led_init(struct ar9170 *ar)
{
	int err;
  
	/* disable LEDs */
    /* GPIO [0/1 mode: output, 2/3: input] */
    err = ar9170_write_reg(ar, AR9170_GPIO_REG_PORT_TYPE, 3);
    if (err)
		goto out;
  
	/* GPIO 0/1 value: off */
    err = ar9170_led_set_state(ar, 0);
  
out:
    return err;
 }

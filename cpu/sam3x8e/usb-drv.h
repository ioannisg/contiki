/**
 * usb_drv.h
 *
 * A Contiki OS USB Driver 
 *
 * Implements the required outgoing queues for endpoint transfers.
 *
 * Created: 1/27/2014 10:59:06 AM
 *  Author: Ioannis Glaropoulos
 */ 
//#include <stdint-gcc.h>
#include "compiler.h"

#ifndef USB_DRV_H_
#define USB_DRV_H_

/** OUT endpoint time-out values */
#define USB_DRV_EP_OUT_TIMEOUT_MS			5u
#define USB_DRV_CTRL_EP_OUT_TIMEOUT_MS		200u



#include "usb_dev.h"

extern const struct usb_dev_driver usb_driver;

int usb_drv_int_input_handler(void);
int usb_drv_bulk_input_handler(void);
#endif /* USB_DRV_H_ */
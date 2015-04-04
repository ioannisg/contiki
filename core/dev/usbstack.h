/*
 * usbstack.h
 *
 * Created: 1/31/2014 4:32:35 PM
 *  Author: Ioannis Glaropoulos
 */ 


#ifndef USBSTACK_H_
#define USBSTACK_H_


#include "contiki-conf.h"

#ifndef USB_STACK
#ifdef USB_CONF_STACK
#define USB_STACK USB_CONF_STACK
#else /* USB_CONF_STACK */
#define USB_STACK
#endif /* USB_CONF_STACK */
#endif /* USB_STACK */

#include "dev/usb_dev.h"
extern const struct usb_dev_driver			USB_STACK;

#endif /* USBSTACK_H_ */
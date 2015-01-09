/*
 * usb.h
 *
 * A Contiki OS <--> ASF USB Wrapper for SAM3X8E.
 *
 * Created: 1/26/2014 2:02:57 PM
 *  Author: Ioannis Glaropoulos
 */ 
#ifndef USB_H_
#define USB_H_

#include "contiki-conf.h"
#include "usb_protocol.h"
#include "uhd.h"

/* Definitions for USB Endpoint sizes. */
#ifdef USB_CONF_INT_EP_MAX_SIZE
#define USB_INT_EP_MAX_SIZE			USB_CONF_INT_EP_MAX_SIZE
#else
#define USB_INT_EP_MAX_SIZE			64
#endif

#ifdef USB_CONF_CTRL_EP_MAX_SIZE
#define USB_CTRL_EP_MAX_SIZE		USB_CONF_INT_EP_MAX_SIZE
#else
#define USB_CTRL_EP_MAX_SIZE		64
#endif

/* The maximum size of the BULK endpoint is chosen in such a way that
 * all possible cases regarding response sizes are covered. It seems
 * there is a bug in the ASF USB stack, making the program crash, if
 * a response larger than the declared maximum size arrives. 2048 is
 * a workaround, i.e. we have measured the device responses for long
 * times and conclude that this value covers all cases. Note that we
 * can not theoretically compute the maximum response size directly
 * from the maximum 802.11 frame-in-the-air size, as, unfortunately, 
 * the AR9170 may group responses.
 */
#ifdef USB_CONF_BULK_EP_MAX_SIZE
#define USB_BULK_EP_MAX_SIZE		USB_CONF_BULK_EP_MAX_SIZE
#else
#define USB_BULK_EP_MAX_SIZE		2048
#endif

#ifdef USB_CONF_BULK_EP_MAX_OUT_SIZE
#define USB_BULK_EP_MAX_OUT_SIZE	USB_CONF_BULK_EP_MAX_OUT_SIZE
#else
#define USB_BULK_EP_MAX_OUT_SIZE	512
#endif

#define USB_BULK_EP_MAX_WORD_SIZE	(USB_BULK_EP_MAX_SIZE) / 4

/* USB lock semaphore */
typedef volatile uint8_t usb_lock_t;

#define USB_CHECK_GET_LOCK(_lock)	cpu_irq_enter_critical(); \
if ((*_lock) == 1) { \
	printf("ERROR: USB-lock-set;abort\n"); \
	cpu_irq_leave_critical(); \
	return 1; \
} \
(*_lock) = 1; \
cpu_irq_leave_critical()

#define USB_GET_LOCK(_lock) (*_lock) = 1

#define USB_WAIT_FOR_LOCK_CLEAR(_lock) while(*(_lock)) { \
	} \

#define USB_CHECK_RELEASE_LOCK(_lock)	cpu_irq_enter_critical(); \
if ((*_lock) == 0) { \
	printf("ERROR: USB lock clear!\n");	\
} \
else { \
	\
} \
*(_lock) = 0; \
cpu_irq_leave_critical()

#define USB_RELEASE_LOCK(_lock) (*_lock) = 0


/**
 * Artificial limit for the bulk-in transfers
 */
uint8_t usb_bulk_in_transfer_limit;

/* ----- USB Transfer Call-back functions ----- */

void usb_int_in_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, iram_size_t nb_transfered);
void usb_bulk_in_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, iram_size_t nb_transfered);
void usb_ctrl_in_transfer_done(usb_add_t add, uhd_trans_status_t status, uint16_t nb_transfered);

void usb_int_out_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, iram_size_t nb_transfered);
void usb_bulk_out_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, iram_size_t nb_transfered);
void usb_ctrl_out_transfer_done(usb_add_t add, uhd_trans_status_t status, uint16_t nb_transfered);
void usb_async_int_out_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, iram_size_t nb_transfered);
/* ----- USB transfer IN functions ----- */

/** 
 * Register listening on the interrupt (INT) endpoint.
 * A call-back function is invoked, when listening has
 * been completed.
 */
int usb_listen_on_interrupt_in(uhd_callback_trans_t int_in_done_cb);
/** 
 * Register listening on the bulk (BULK) endpoint.
 * A call-back function is invoked, when listening has
 * been completed.
 */
int usb_listen_on_bulk_in(uhd_callback_trans_t bulk_in_done_cb);
/** 
 * Register listening on the control (CTRL) endpoint.
 * A call-back function is invoked, when listening has
 * been completed.
 */
void usb_listen_on_ctrl_in(uhd_callback_setup_end_t ctrl_in_done_cb);


/* ----- USB transfer OUT functions ----- */

/** 
 * Register transfer down to the interrupt (INT) endpoint.
 * A call-back function is invoked when the transfer has
 * been completed. 
 *
 * Returns non-zero on transfer error.
 */
uint8_t usb_send_int_out(uint8_t* cmd, uint16_t cmd_len, uint8_t is_sync,
	uhd_callback_trans_t int_out_transfer_done_cb);
/** 
 * Register transfer down to the bulk (BULK) endpoint.
 * A call-back function is invoked when the transfer has
 * been completed.
 *
 * Returns non-zero on transfer error.
 */
uint8_t usb_send_bulk_out(uint8_t* data, uint16_t data_len, uint8_t is_sync, 
	uhd_callback_trans_t bulk_out_transfer_done_cb);
/** 
 * Register transfer down to the control (CTRL) endpoint.
 * A call-back function is invoked when the transfer has
 * been completed.
 *
 * Returns non-zero on transfer error. 
 */
uint8_t usb_send_ctrl_out(uint8_t * buf, iram_size_t buf_size, 
	uint8_t bRequest, le16_t wValue, uhd_callback_setup_end_t callback);
/**
 * Set the bulk (BULK) input handler.
 */
void usb_set_bulk_input_handler(int (*input) (void));
/**
 * Set the Interrupt (INT) input handler.
 */
void usb_set_int_input_handler(int (*input) (void));
/**
 * Initialize USB device driver.
 */
void usb_init(void);

uint8_t usb_is_int_out_busy(void);

uint8_t usb_is_bulk_out_busy(void);

uint8_t usb_is_ctrl_out_busy(void);

/**
 * Release the INT OUT endpoint transfer lock.
 */
void usb_release_int_out_lock(void);
/**
 * Release the BULK OUT endpoint transfer lock.
 */
void usb_release_bulk_out_lock(void);

void usb_release_ctrl_out_lock(void);

void usb_abort_bulk_in(void);
void usb_abort_int_in(void);
inline void usb_bulk_in_set_transfer_limit(uint8_t limit);
inline void usb_bulk_in_set_transfer_limit(uint8_t limit)
{
	usb_bulk_in_transfer_limit = limit;
}

uint8_t usb_is_int_out_last_tx_not_ok(void);
uint8_t usb_is_bulk_out_last_tx_ok(void);
#endif /* USB_H_ */
/*
 * usb_drv.c
 * A Contiki OS USB driver module interconnected with USBSTACK.
 *
 * Created: 1/27/2014 12:18:41 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "contiki.h"
#include "contiki-net.h"
#include "usb-drv.h"
#include "usb_dev.h"
#include "usb.h"
#include "wifistack.h"
#include "compiler.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#if WITH_USB_HOST_SUPPORT

#ifdef USB_BULK_EP_MAX_WORD_SIZE
#define USB_DRV_BULK_EP_MAX_WORD_SIZE	USB_BULK_EP_MAX_WORD_SIZE
#else
#define USB_DRV_BULK_EP_MAX_WORD_SIZE	512
#endif

#ifdef USB_INT_EP_MAX_SIZE
#define USB_DRV_INT_EP_MAX_SIZE	USB_INT_EP_MAX_SIZE
#else
#define USB_DRV_INT_EP_MAX_SIZE	64
#endif


/* Buffer holding the incoming BULK USB data. */
static COMPILER_WORD_ALIGNED uint32_t usb_drv_bulk_buf[USB_DRV_BULK_EP_MAX_WORD_SIZE];
static uint32_t usb_drv_bulk_buf_len;
static uint32_t* usb_drv_bulk_buf_ptr;

/* Buffer holding the incoming Interrupt Endpoint USB data. */
static COMPILER_WORD_ALIGNED uint8_t usb_drv_int_buf[USB_DRV_INT_EP_MAX_SIZE];
static uint32_t usb_drv_int_buf_len;

/*---------------------------------------------------------------------------*/
static uint8_t*
usb_drv_get_int_ptr(void)
{
	return usb_drv_int_buf;
}
/*---------------------------------------------------------------------------*/
static uint32_t*
usb_drv_get_bulk_ptr(void)
{
	return usb_drv_bulk_buf_ptr;
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_int_buf_clear(void)
{
	memset(usb_drv_int_buf, 0, USB_DRV_INT_EP_MAX_SIZE);
	usb_drv_int_buf_len = 0;
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_bulk_buf_clear(void)
{
	//memset(usb_drv_bulk_buf, 0, USB_BULK_EP_MAX_WORD_SIZE);
	usb_drv_bulk_buf_len = 0;
	usb_drv_bulk_buf_ptr = &usb_drv_bulk_buf[0];
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_bulk_buf_set_len(uint32_t len)
{
	usb_drv_bulk_buf_len = len;
}
/*---------------------------------------------------------------------------*/
static uint32_t
usb_drv_bulk_buf_get_len(void)
{
	return usb_drv_bulk_buf_len;
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_int_buf_set_len(uint32_t len)
{
	usb_drv_int_buf_len = len;
}
/*---------------------------------------------------------------------------*/
static uint32_t
usb_drv_int_buf_get_len(void)
{
	return usb_drv_int_buf_len;
}
/*---------------------------------------------------------------------------*/
int 
usb_drv_bulk_input_handler(void)
{
	/* Point to the beginning of the data */
	usb_drv_bulk_buf_ptr = &usb_drv_bulk_buf[0];
	
#ifdef WITH_USB_UNTIE_RSPS
	/* Save total responses' length*/
	uint32_t bulk_total_length = usb_drv_bulk_buf_len;
	/* Length of individual response */
	uint32_t bulk_len = 0;
	/* Length of aggregate responses */
	uint32_t bulk_aggr_length = 0;
		
	/* Moving pointer */
	uint8_t* bulk_ptr = (uint8_t*)usb_drv_bulk_buf_ptr;
	
	while(1) {
		/* Get length of individual response plus header */
		bulk_len = bulk_ptr[0]+4;
		usb_drv_bulk_buf_len = bulk_len;
		usb_drv_bulk_buf_ptr = (uint32_t*)(&bulk_ptr[0]);
		WIFI_STACK.input();
		
		/* Move the moving data pointer */
		bulk_ptr += bulk_len;
		/* Update aggregate response length */
		bulk_aggr_length += bulk_len;
		/* Check if we are at the end */
		if(bulk_aggr_length >= bulk_total_length) {			
			break;
		}
		PRINTF("USB_DRV: more rsps\n");		
	}
#else
	WIFI_STACK.input();
#endif	
	return 0;
}
/*---------------------------------------------------------------------------*/
int 
usb_drv_int_input_handler(void)
{
	/* INT data available in the buffer. */
	WIFI_STACK.input();
	return 0;
}
/*---------------------------------------------------------------------------*/
static void 
init(void)
{
	/* Initialize USB host stack. */
	usb_init();
	/* Register the input handlers for INT/BULK USB responses. */
	usb_set_bulk_input_handler(usb_drv_bulk_input_handler);
	usb_set_int_input_handler(usb_drv_int_input_handler);
}
/*---------------------------------------------------------------------------*/
static int
usb_drv_init_bulk_in(void)
{
	return usb_listen_on_bulk_in(usb_bulk_in_transfer_done);
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_cancel_bulk_in(void)
{
	usb_abort_bulk_in();
}
/*---------------------------------------------------------------------------*/
static int
usb_drv_init_int_in(void)
{
	return usb_listen_on_interrupt_in(usb_int_in_transfer_done);
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_cancel_int_in(void)
{
	usb_abort_int_in();
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_connect(le16_t vendor_id, le16_t product_id, uint8_t plug)
{
	/* Inform on disconnection. Connection notifications with vendor_change */
	if (!plug) {
		WIFI_STACK.connect(vendor_id, product_id, plug);		
	}
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_vendor_change(le16_t vendor_id, le16_t product_id, uint8_t plug)
{
	/* Inform the connected WiFi driver about this event */
	WIFI_STACK.vendor_change(vendor_id, product_id, plug);
	
}
/*---------------------------------------------------------------------------*/
static int
send_control(uint8_t * buf, iram_size_t buf_size, uint8_t bRequest,	
	le16_t wValue)
{
	PRINTF("USB_DRV: send-control [%u]\n",buf_size);
	if (usb_is_ctrl_out_busy()) {
		PRINTF("AR9170_DRV: ctrl-out busy\n");
		return USB_DRV_TX_ERR;
	}

	if (usb_send_ctrl_out(buf, buf_size, bRequest, wValue, 
		usb_ctrl_out_transfer_done)) {
			PRINTF("USB_DRV ERROR: ctrl-out transfer returned error\n");
			return USB_DRV_TX_ERR;
			
	} else {
		
		/* Wait for the USB lock to be released by the call-back, or timeout. */
		rtimer_clock_t due_time = RTIMER_NOW() + (RTIMER_SECOND/1000)*USB_DRV_CTRL_EP_OUT_TIMEOUT_MS;
		while(usb_is_ctrl_out_busy()) {
			if(RTIMER_NOW() >= due_time) {
				PRINTF("USB_DRV: ctrl-out timeout\n");
				usb_release_ctrl_out_lock();
				return USB_DRV_TX_ERR;
			}
		}
		/* Successfully sent down for transmission and the transfer completed. */
		return USB_DRV_TX_OK;
	}	
}
/*---------------------------------------------------------------------------*/
static int 
send_async_bulk(usb_chunk_t *ptr)
{	
	PRINTF("USB_DRV: send-async-bulk\n");
	if (!usb_is_bulk_out_busy() && !usb_is_int_out_busy()) {
		
		if (unlikely(usb_send_bulk_out(ptr->buf, ptr->len, USB_ASYNC_CMD_FLAG, 
				usb_bulk_out_transfer_done))) {
			PRINTF("USB_DRV: bulk-out-transfer-error\n");
			return USB_DRV_TX_ERR;
		}
		/* Success */		
		return USB_DRV_TX_OK;		
	}
	PRINTF("USB_DRV: async-bulk:int-bulk-out-busy\n");
	return USB_DRV_TX_ERR_BUSY;
}
/*---------------------------------------------------------------------------*/
static int 
send_sync_bulk(usb_chunk_t *ptr)
{
	if (usb_is_bulk_out_busy() || usb_is_int_out_busy()) {
		printf("USB_DRV ERROR: bulk-int endpoint busy\n");
		return USB_DRV_TX_ERR_BUSY;
	}
	if (unlikely(usb_send_bulk_out(ptr->buf, ptr->len, USB_SYNC_CMD_FLAG,
			usb_bulk_out_transfer_done))) {
		printf("USB_DRV: sync-bulk-out-transfer-error\n");
		return USB_DRV_TX_ERR;
		
	} else {
		/* Wait for the USB lock to be released by the call-back, or timeout. */
		rtimer_clock_t due_time = RTIMER_NOW() + 
			(RTIMER_SECOND/1000)*USB_DRV_EP_OUT_TIMEOUT_MS;
		while(usb_is_bulk_out_busy()) {
			
			if(RTIMER_NOW() >= due_time && usb_is_bulk_out_busy()) {
				printf("USB_DRV: bulk-out timeout\n");
				usb_release_bulk_out_lock();
				return USB_DRV_TX_ERR_TIMEOUT;				
			}
		}
		/* Success */
		return USB_DRV_TX_OK;
	}	
}
/*---------------------------------------------------------------------------*/
static int
send_async_cmd(usb_chunk_t *ptr)
{
	PRINTF("USB_DRV: send-async-cmd\n");
	if (!usb_is_int_out_busy() && !usb_is_bulk_out_busy()) {
		/* Send with the ASYNC flag and choose the call-back accordingly. */
		if (unlikely(usb_send_int_out(ptr->buf, ptr->len, USB_ASYNC_CMD_FLAG, 
			usb_async_int_out_transfer_done))) {
			PRINTF("USB_DRV ERROR: async-int-out transfer-error\n");
			return USB_DRV_TX_ERR;
		}
		/* Success */
		return USB_DRV_TX_OK;
	}
	PRINTF("USB_DRV: async-cmd:int-bulk-out-busy\n");
	return USB_DRV_TX_ERR_BUSY;
}
/*---------------------------------------------------------------------------*/
static int
send_sync_cmd(usb_chunk_t *ptr)
{
	if (usb_is_int_out_busy() || usb_is_bulk_out_busy()) {
		printf("USB_DRV ERROR: int-bulk-out-busy\n");
		return USB_DRV_TX_ERR_BUSY;
	}
	/* Send with SYNC flag so USB waits until EP is available */
	if (unlikely(usb_send_int_out(ptr->buf, ptr->len, USB_SYNC_CMD_FLAG, 
			usb_int_out_transfer_done))) {
		printf("USB_DRV: %2x int-out-transfer-error\n",ptr->buf[1]);
		return USB_DRV_TX_ERR;
		
	} else {
		/* Wait for the USB lock to be released by the call-back, or timeout. */
		rtimer_clock_t due_time = RTIMER_NOW() +
			(RTIMER_SECOND/1000)*USB_DRV_EP_OUT_TIMEOUT_MS;
		while(usb_is_int_out_busy()) {
			if(RTIMER_NOW() >= due_time) {
				if (usb_is_int_out_busy()) {
					printf("USB_DRV: int-out-timeout (%02x)\n", ptr->buf[1]);
					usb_release_int_out_lock();
					return USB_DRV_TX_ERR_TIMEOUT;
				}	
			}
		}
		if (unlikely(usb_is_int_out_last_tx_not_ok())) {
			return USB_DRV_TX_ERR_TIMEOUT;
		}
		/* Success */
		return USB_DRV_TX_OK;	
	}
}
/*---------------------------------------------------------------------------*/
static int
usb_drv_bulk_busy(void)
{
	return usb_is_bulk_out_busy();
}
/*---------------------------------------------------------------------------*/
static int
usb_drv_int_busy(void)
{
	return usb_is_int_out_busy();
}
/*---------------------------------------------------------------------------*/
static void
usb_drv_set_bulk_limit(uint8_t limit)
{
	usb_bulk_in_set_transfer_limit(limit);
}
/*---------------------------------------------------------------------------*/
const struct usb_dev_driver usb_driver = {
	"usb-driver",
	init,
	usb_drv_connect,
	usb_drv_vendor_change,
	usb_drv_init_bulk_in,
	usb_drv_init_int_in,
	usb_drv_cancel_bulk_in,
	usb_drv_cancel_int_in,
	send_control,
	send_async_bulk,
	send_async_cmd,
	send_sync_bulk,
	send_sync_cmd,
	usb_drv_bulk_busy,
	usb_drv_int_busy,
	usb_drv_get_int_ptr,
	usb_drv_get_bulk_ptr,
	usb_drv_int_buf_set_len,
	usb_drv_bulk_buf_set_len,
	usb_drv_bulk_buf_get_len,
	usb_drv_int_buf_get_len,
	usb_drv_int_buf_clear,
	usb_drv_bulk_buf_clear,
	usb_drv_set_bulk_limit,	
};
#endif /* WITH_USB_HOST_SUPPORT */
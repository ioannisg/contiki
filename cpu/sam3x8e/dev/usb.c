/*
 * usb.c
 *
 * USB API for Vendor-specific devices.
 * 
 * Usage hint: if you want to initiate down-transfers through the bulk or
 * interrupt endpoints inside an interrupt context, it is not thread-safe
 * to use an asynchronous bulk/int transfer, since such transfer will not
 * block until the transfer has completed, so if a subsequent transfer is 
 * critically close, it will mess up with the one inside IRQ.
 *
 * Created: 1/26/2014 7:08:24 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "contiki-conf.h"
#include "compiler.h"
#include "usb.h"
#include "usbstack.h"
#include "interrupt_sam_nvic.h"
#include "uhc.h"
#include "uhi_vendor.h"


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#if WITH_USB_HOST_SUPPORT

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

/* USB transfer buffers */
static COMPILER_WORD_ALIGNED uint8_t int_in_buffer[USB_INT_EP_MAX_SIZE];
static COMPILER_WORD_ALIGNED uint8_t int_out_buffer[USB_INT_EP_MAX_SIZE];

static COMPILER_WORD_ALIGNED uint32_t bulk_in_buffer[USB_BULK_EP_MAX_WORD_SIZE];
static COMPILER_WORD_ALIGNED uint8_t bulk_out_buffer[USB_BULK_EP_MAX_OUT_SIZE];

static COMPILER_WORD_ALIGNED uint8_t ctrl_in_buffer[USB_CTRL_EP_MAX_SIZE];

/* The asynchronous locks for synchronizing USB endpoint access. */
static usb_lock_t usb_cmd_lock, usb_data_lock, usb_ctrl_lock;

/* Outgoing transfer status for INT/BULK endpoints */
static uint8_t usb_int_out_last_status, usb_bulk_out_last_status;

/* This is the receive bulk interrupt handler. */
static int (*usb_bulk_input_handler) (void);

/* This is the receive bulk interrupt handler. */
static int (*usb_int_input_handler) (void);

/*---------------------------------------------------------------------------*/
uint8_t 
usb_is_int_out_last_tx_not_ok(void)
{
	return usb_int_out_last_status;
}
/*---------------------------------------------------------------------------*/
uint8_t
usb_is_bulk_out_last_tx_ok(void)
{
	return usb_bulk_out_last_status;
}
/*---------------------------------------------------------------------------*/
uint8_t 
usb_is_int_out_busy(void)
{
	uint8_t is_busy = usb_cmd_lock;
	if (is_busy) {
		return 1;		
	}
	return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t 
usb_is_bulk_out_busy(void)
{
	uint8_t is_busy = usb_data_lock;
	if (is_busy)
		return 1;
	return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t 
usb_is_ctrl_out_busy(void)
{
	uint8_t is_busy = usb_ctrl_lock;
	if (is_busy)
		return 1;
	return 0;
}
/*---------------------------------------------------------------------------*/
void
usb_init(void)
{
	/* Initialize ASF USB Host */
	uhc_start();
	
	/* Initialize locks */
	usb_cmd_lock = 0;
	usb_data_lock = 0;
	usb_ctrl_lock = 0;
	/* This clears the transfer limit */
	usb_bulk_in_transfer_limit = 0;
	
	usb_int_out_last_status = 0;
	usb_bulk_out_last_status = 0;
}
/*---------------------------------------------------------------------------*/
void 
usb_set_bulk_input_handler(int (*input) (void))
{
	if (input != NULL) {
		usb_bulk_input_handler = input;
	
	} else {
		PRINTF("USB ERROR: Bulk input handler is null\n");
	}	
}
/*---------------------------------------------------------------------------*/
void
usb_set_int_input_handler(int (*input) (void))
{
	if (input != NULL) {
		usb_int_input_handler = input;
		
		} else {
		PRINTF("USB ERROR: Interrupt input handler is null\n");
	}
}
/*---------------------------------------------------------------------------*/
static void 
usb_memcpy(uint32_t *pDest, uint32_t *pSrc, uint32_t len)
{
	uint32_t i;
	
	/* Manually copy the data from the source to the destination */
	for (i=0; i<len; i++) {
		
		*pDest++ = *pSrc++;
	}
}
/*---------------------------------------------------------------------------*/
uint8_t 
usb_send_bulk_out(uint8_t* data, uint16_t data_len, uint8_t is_sync,
	uhd_callback_trans_t bulk_out_transfer_done_cb) {

	PRINTF("USB: send-data-down (%u)\n",data_len);
	if (is_sync) {
		/* 
		 * Prevents accessing the pipe if the current thread was interrupted
		 * by an asynchronous bulk transfer, so the lock is not yet released.
		 */
		USB_CHECK_GET_LOCK(&usb_data_lock);
	
	} else {		
		/* 
		 * We make it thread-safe by waiting until the lock is released. This
		 * is why we can NOT use this inside interrupt context as the flag is
		 * maybe cleared in thread-mode.
		 */
		USB_WAIT_FOR_LOCK_CLEAR(&usb_data_lock);
		USB_GET_LOCK(&usb_data_lock);
	}	
	/* Wait for the bulk endpoint to become available. */
	if (!uhi_vendor_bulk_is_available()) {
		printf("ERROR: BULK endpoint not available.\n");
		while(!uhi_vendor_bulk_is_available());
	}	
	/* Copy the packet data to the 
	 * interrupt endpoint output buffer.
	 */
	memcpy(bulk_out_buffer, data, data_len);
	
	/* Start transferring OUT on the BULK endpoint. 
	 * Return the status of the transfer upon completion. 
	 */
	unsigned char result = uhi_vendor_bulk_out_run(bulk_out_buffer,
		(iram_size_t) data_len, bulk_out_transfer_done_cb);
	if (result == 0) {
		/* Perhaps this is redundant; if the transfer has not 
		 * started, there is nothing to abort here.
		 */
		uhi_vendor_bulk_out_abort();
		USB_RELEASE_LOCK(&usb_data_lock);
	}
	/* Release the lock if asynchronous, otherwise 
	 * the lock will be released by the callback.
	 */
	if (!is_sync) {
		USB_RELEASE_LOCK(&usb_data_lock);
	}
	return !result; 
}
/*---------------------------------------------------------------------------*/
uint8_t 
usb_send_int_out(uint8_t* cmd, uint16_t cmd_len, uint8_t is_synchronous,
	uhd_callback_trans_t int_out_transfer_done_cb) {
	
	PRINTF("Send command down [%u].\n",cmd_len);
	if (unlikely(cmd_len > USB_INT_EP_MAX_SIZE)) {
		printf("USB: too-large-transfer\n");
		return 1;
	}
	if (is_synchronous) {
		/* Lock asynchronous command transfer. The flag must
		 * be released by the callback function, called upon
		 * transfer completion.
		 *
		 * Prevents from accessing the pipe, if the current thread was 
		 * interrupted by an asynchronous int-out transfer so the lock
		 * is not yet released. Note, however, that the AR9170 driver,
		 * does not allow such thing, but our USB driver should not be
		 * aware of that.
		 */
		USB_CHECK_GET_LOCK(&usb_cmd_lock);
		
	} else {
		/* MUST NOT order asynchronous transfers inside interrupt context */
		USB_WAIT_FOR_LOCK_CLEAR(&usb_cmd_lock);
		USB_GET_LOCK(&usb_cmd_lock);
	}
	
	/* Block execution while the endpoint is not available. */
	if(!uhi_vendor_int_is_available()) {
		printf("ERROR: INT endpoint not available.\n");
		while(!uhi_vendor_int_is_available());
	}	
	/* Copy the command data to the  interrupt endpoint output buffer */
	memcpy(int_out_buffer, cmd, cmd_len);
	
	/* Start transferring OUT on the interrupt endpoint. 
	 * Return the status of the transfer upon completion. 
	 */
	unsigned char result = uhi_vendor_int_out_run(int_out_buffer,
		(iram_size_t) cmd_len, int_out_transfer_done_cb); 	
	if (result == 0) {
		/* Perhaps abort has no effect if not scheduled at all */
		uhi_vendor_int_out_abort();
		/* Update last-transfer-status to BAD */
		usb_int_out_last_status = 1;
		USB_RELEASE_LOCK(&usb_cmd_lock);
	}
	if (!is_synchronous) {
		USB_RELEASE_LOCK(&usb_cmd_lock);
	}
	return !result; 
}
/*---------------------------------------------------------------------------*/
uint8_t 
usb_send_ctrl_out(uint8_t * buf, iram_size_t buf_size, uint8_t bRequest, 
	le16_t wValue, uhd_callback_setup_end_t callback)
{	
	USB_CHECK_GET_LOCK(&usb_ctrl_lock);	
		
	unsigned char result =  uhi_vendor_control_out_run_extended(buf, buf_size, 
		bRequest, wValue, callback);
	
	if (!result) {
		printf("USB CTRL OUT ERROR!\n");
	}
	return !result;	
}
/*---------------------------------------------------------------------------*/
int 
usb_listen_on_interrupt_in(uhd_callback_trans_t int_in_done_cb)
{
	uint8_t read_result;
	
	memset(int_in_buffer, 0, USB_INT_EP_MAX_SIZE);
	
	if (!uhi_vendor_int_is_available()) {
		printf("USB ERROR: Interrupt transfer endpoint is not available.\n");
		return -1;
	}
	
	read_result = uhi_vendor_int_in_run(int_in_buffer, 
		(iram_size_t)(USB_INT_EP_MAX_SIZE), int_in_done_cb);
	
	if (read_result == 0) {
		printf("ERROR: Interrupt IN Listening register completed with errors!\n");
		return -1;
	}
	
	return 0;	
}
/*---------------------------------------------------------------------------*/
void 
usb_listen_on_ctrl_in(uhd_callback_setup_end_t ctrl_in_done_cb)
{
	uint8_t read_result;
	
	memset(ctrl_in_buffer,0,USB_CTRL_EP_MAX_SIZE);
		
	read_result = uhi_vendor_control_in_run(ctrl_in_buffer, 
		(iram_size_t)(USB_CTRL_EP_MAX_SIZE), ctrl_in_done_cb);
	
	if (read_result == 0) {
		printf("ERROR: Control IN Listening register completed with errors!\n");
	}	
}
/*---------------------------------------------------------------------------*/
int 
usb_listen_on_bulk_in(uhd_callback_trans_t bulk_in_done_cb)
{
	/* Signal an error if the bulk endpoint is not available. */
	if (!uhi_vendor_bulk_is_available()) {
		printf("ERROR: Bulk transfer endpoint is not available.\n");
		return -1;
	}
		
	if (!uhi_vendor_bulk_in_run((COMPILER_WORD_ALIGNED uint8_t*)bulk_in_buffer,
		(iram_size_t)(USB_BULK_EP_MAX_SIZE), bulk_in_done_cb)) {
		/* Signal an error. */
		printf("ERROR: BULK IN Listening register completed with errors!\n");
		return -1;
	}
	
	return 0;
}
/*---------------------------------------------------------------------------*/
void 
usb_int_out_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, 
	iram_size_t nb_transfered)
{		
	USB_RELEASE_LOCK(&usb_cmd_lock); // we could use check_release TODO
	
	switch (status) 
		{
		case UHD_TRANS_NOERROR:	
			usb_int_out_last_status = 0;		
			PRINTF("INT OUT Success. Bytes Transfered: %u.\n",
				(unsigned int)nb_transfered);			
			break;
		case UHD_TRANS_TIMEOUT:
			/* Inform the driver that the transmission was not successful */
			usb_int_out_last_status = 1;
			printf("ERROR: INT OUT Timeout.\n");	
			break;
		case UHD_TRANS_DISCONNECT:
			printf("ERROR: INT OUT disconnect error.\n");
			break;
		case UHD_TRANS_CRC:
		case UHD_TRANS_DT_MISMATCH:
		case UHD_TRANS_PIDFAILURE:
			printf("ERROR: INT OUT CRC / MISMATCH / PIDFAILURE error.\n");
			break;
		case UHD_TRANS_STALL:
			printf("ERROR: INT OUT STALL error.\n");
			break;
		case UHD_TRANS_NOTRESPONDING:
			printf("ERROR: INT OUT Not Responding error.\n");		
			break;
		case UHD_TRANS_ABORTED:
			printf("ERROR: INT OUT Aborted.\n");
			break;
		default:
			printf("ERROR: Sending INT OUT unrecognized error: %d.\n",status);
		break;
	}	
}
/*---------------------------------------------------------------------------*/
void
usb_async_int_out_transfer_done(usb_add_t add, usb_ep_t ep, 
	uhd_trans_status_t status, iram_size_t nb_transfered)
{
	USB_RELEASE_LOCK(&usb_cmd_lock);
	
	switch (status)
	{
		case UHD_TRANS_NOERROR:
			usb_int_out_last_status = 0;
			PRINTF("INT OUT Success. Bytes Transfered: %u.\n",
				(unsigned int)nb_transfered);
			break;
		case UHD_TRANS_TIMEOUT:
			/* Inform the driver that the transmission was not successful */
			usb_int_out_last_status = 1;
			printf("ERROR: INT OUT Timeout.\n");
			break;
		case UHD_TRANS_DISCONNECT:
			printf("ERROR: INT OUT disconnect error.\n");
			break;
		case UHD_TRANS_CRC:
		case UHD_TRANS_DT_MISMATCH:
		case UHD_TRANS_PIDFAILURE:
			printf("ERROR: INT OUT CRC / MISMATCH / PIDFAILURE error.\n");
			break;
		case UHD_TRANS_STALL:
			printf("ERROR: INT OUT STALL error.\n");
			break;
		case UHD_TRANS_NOTRESPONDING:
			printf("ERROR: INT OUT Not Responding error.\n");
			break;
		case UHD_TRANS_ABORTED:
			printf("ERROR: INT OUT Aborted.\n");
			break;
		default:
			printf("ERROR: Sending INT OUT unrecognized error: %d.\n",status);
			break;
	}
}
/*---------------------------------------------------------------------------*/
void 
usb_bulk_out_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, 
	iram_size_t nb_transfered)
{		
	/* Release asynchronous TX lock, so further packets can be submitted down.
	 * Does not really matter where exactly we release this flag, since we do
	 * it inside the interrupt context. Note, however, that we may have lost
	 * race to the status response interrupt, which may have already cleared
	 * the flag.
	 */	
	switch (status) {
		
		case UHD_TRANS_NOERROR:			
			USB_RELEASE_LOCK(&usb_data_lock);	
			PRINTF("Bulk OUT Success. Bytes Transfered: %u.\n",(unsigned int)nb_transfered);		
			break;
		case UHD_TRANS_TIMEOUT:			
			printf("ERROR: Bulk OUT Timeout.\n");			
			break;
		case UHD_TRANS_DISCONNECT:
			printf("ERROR: Bulk OUT disconnect error.\n");
			break;
		case UHD_TRANS_CRC:
		case UHD_TRANS_DT_MISMATCH:
		case UHD_TRANS_PIDFAILURE:
			printf("Bulk OUT CRC / MISMATCH / PIDFAILURE error.\n");
			break;
		case UHD_TRANS_STALL:
			printf("ERROR: Bulk OUT STALL error.\n");
			break;
		case UHD_TRANS_ABORTED:
			printf("ERROR: Bulk OUT Aborted.\n");
			break;
		case UHD_TRANS_NOTRESPONDING:
			printf("ERROR: Bulk OUT Not Responding error.\n");
			break;
		default:
			printf("ERROR: Sending Bulk OUT unrecognized error: %d.\n",status);
			break;
	}
}
/*---------------------------------------------------------------------------*/
void
usb_ctrl_out_transfer_done(usb_add_t add, uhd_trans_status_t status, 
	uint16_t payload_trans)
{
	/* Release USB transfer lock. */
	USB_RELEASE_LOCK(&usb_ctrl_lock);

	switch (status)
	{
		case UHD_TRANS_NOERROR:
			PRINTF("USB CTRL OUT Success. Bytes Transfered: %u.\n",(unsigned int)payload_trans);
			break;
		case UHD_TRANS_TIMEOUT:
			printf("CTRL OUT Timeout.\n");
			break;
		case UHD_TRANS_DISCONNECT:
			printf("CTRL OUT disconnect error.\n");
			break;
		case UHD_TRANS_CRC:
		case UHD_TRANS_DT_MISMATCH:
		case UHD_TRANS_PIDFAILURE:
			printf("CTRL OUT CRC / MISMATCH / PIDFAILURE error.\n");
			break;
		case UHD_TRANS_STALL:
			printf("CTRL OUT STALL error.\n");
			break;
		case UHD_TRANS_ABORTED:
			printf("CTRL OUT Aborted.\n");
			break;
		case UHD_TRANS_NOTRESPONDING:
			printf("CTRL OUT Not Responding error.\n");
			break;
		default:
			printf("ERROR: Sending CTRL OUT unrecognized error: %d.\n",status);
			break;
	}
}
/*---------------------------------------------------------------------------*/
void 
usb_int_in_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, 
	iram_size_t nb_transfered )
{		
	switch (status)
	{
		case UHD_TRANS_NOERROR:		
			PRINTF("Interrupt IN success.\n");
			#if DEBUG
			unsigned int i;
			PRINTF("Number of bytes transfered: %u.\n",(unsigned int)nb_transfered);
			for (i=0; i<nb_transfered; i++) {
				PRINTF("%02x ", int_in_buffer[i]);
			}
			PRINTF("\n");
			#endif
			/* Clear the INT USB driver buffer */
			USB_STACK.int_clear();
			/* Copy data */
			memcpy(USB_STACK.int_dataptr(), int_in_buffer, nb_transfered);
			USB_STACK.int_set_len((uint32_t)nb_transfered);
			memset(int_in_buffer, 0, USB_INT_EP_MAX_SIZE);
			/* Re-schedule listening */
			usb_listen_on_interrupt_in(usb_int_in_transfer_done);
			/* Handle command response */
			usb_int_input_handler();
			break;
		case UHD_TRANS_DISCONNECT:
			printf("ERROR: Interrupt IN disconnect error.\n");
			break;
		case UHD_TRANS_CRC:
		case UHD_TRANS_DT_MISMATCH:
		case UHD_TRANS_PIDFAILURE:
			printf("ERROR: Interrupt IN CRC / MISMATCH / PIDFAILURE error.\n");
			break;
		case UHD_TRANS_STALL:
			printf("ERROR: Interrupt IN STALL error.\n");
			break;
		case UHD_TRANS_NOTRESPONDING:
			printf("ERROR: Interrupt IN Not Responding error.\n");
			break;
		case UHD_TRANS_TIMEOUT:			
			PRINTF("Interrupt IN has timed-out. \n");			
			usb_listen_on_interrupt_in(usb_int_in_transfer_done);
			break;
		default:
			printf("ERROR: Unrecognized status: %d.\n",status);
	}	
}
/*---------------------------------------------------------------------------*/
void 
usb_bulk_in_transfer_done(usb_add_t add, usb_ep_t ep, uhd_trans_status_t status, 
	iram_size_t nb_transfered)
{		
	switch (status)
	{ 
		case UHD_TRANS_NOERROR:	
			if (nb_transfered >= (USB_BULK_EP_MAX_SIZE) ||
				 (usb_bulk_in_transfer_limit != 0 &&
					nb_transfered > usb_bulk_in_transfer_limit)) {
				PRINTF("ERROR: Response out of limits (%u)\n",nb_transfered);			
				/* Just reschedule. However, I think here the program will crash, anyway. */
				usb_listen_on_bulk_in(usb_bulk_in_transfer_done);
				return;
			}			
			PRINTF("USB: BULK-in [%u]\n",nb_transfered);
			/* Copy response to the driver buffer */
			USB_STACK.bulk_clear();	
			usb_memcpy(USB_STACK.bulk_dataptr(), bulk_in_buffer, DIV_ROUND_UP(nb_transfered,4));			
			/* Reschedule immediately the listening process on the Bulk IN endpoint. 
			 * Although a Bulk IN interrupt can only occur after this interrupt code
			 * is over, we must schedule this listening immediately, as data might be
			 * lost, if e.g. the re-scheduling is done after the current received data
			 * are processed.
			 */
			usb_listen_on_bulk_in(usb_bulk_in_transfer_done);							
			/* Handle the bulk IN response now. */	
			goto handle_ok;
			
		case UHD_TRANS_DISCONNECT:
			printf("ERROR: Bulk IN disconnect error.\n");
		break;
			case UHD_TRANS_CRC:
			case UHD_TRANS_DT_MISMATCH:
			case UHD_TRANS_PIDFAILURE:
			printf("ERROR: Bulk IN CRC / MISMATCH / PIDFAILURE error.\n");
			break;
		case UHD_TRANS_STALL:
			printf("ERROR: Bulk IN STALL error.\n");
			break;
		case UHD_TRANS_ABORTED:
			printf("ERROR: Bulk IN Aborted.\n");
			break;
		case UHD_TRANS_NOTRESPONDING:
			printf("ERROR: Bulk IN Not Responding error.\n");
			break;
		case UHD_TRANS_TIMEOUT: /* Reschedule immediately. */
			printf("WARNING: Bulk IN has timed-out. Will reschedule.\n");
			usb_listen_on_bulk_in(usb_bulk_in_transfer_done);
			break;
		default:
			printf("ERROR: Unrecognized status response: %d.\n",status);
	}	
	return;
	
handle_ok:	
	PRINTF("BULK IN [%u]: ",(unsigned int)nb_transfered);				
	#if DEBUG
	unsigned int i;	
	for (i=0; i<nb_transfered; i++) {
		PRINTF("%02x ", ((uint8_t*)bulk_in_buffer)[i]);
	}
	PRINTF(" \n");
	#endif			
	/* Handle response. The response is handled within the interrupt
	 * context and can be a reason for delaying critical operations,
	 * so we are going to handle only the [critical] command responses
	 * but not the received packets.
	 */
	USB_STACK.bulk_set_len((uint32_t)nb_transfered);		
	usb_bulk_input_handler();
	return;
}
/*---------------------------------------------------------------------------*/
/** 
 *Interrupt handler for device change, declared in conf_usb_host.h 
 */
void 
usb_vendor_change(uhc_device_t* dev, bool b_plug) {
		
	/* Call the driver with the vendor/product ID as arguments */	
	PRINTF("USB: vendor change\n");
	USB_STACK.vendor_change(dev->dev_desc.idVendor,dev->dev_desc.idProduct, b_plug);
}
/*---------------------------------------------------------------------------*/
void
usb_host_vbus_error(void) 
{
	printf("VBUSS_ERROR\n");
}

/** 
 *Interrupt handler for device connection, declared in conf_usb_host.h 
 */
void 
usb_host_connection_event(uhc_device_t* dev, bool b_plug) {
		
	/* Call the driver with the vendor/product ID as arguments */
	if (b_plug) {
		PRINTF("USB: device connected\n");	
	} else {
		PRINTF("USB: device disconnected\n");	
	}
	USB_STACK.connect(dev->dev_desc.idVendor,dev->dev_desc.idProduct, b_plug);
}

void
usb_release_int_out_lock()
{
	USB_RELEASE_LOCK(&usb_cmd_lock);
}

void
usb_release_bulk_out_lock()
{
	USB_RELEASE_LOCK(&usb_data_lock);
}

void
usb_release_ctrl_out_lock()
{
	USB_RELEASE_LOCK(&usb_ctrl_lock);
}

void 
usb_abort_bulk_in( void )
{
	uhi_vendor_bulk_in_abort();
}

void
usb_abort_int_in( void )
{
	uhi_vendor_int_in_abort();
}
#endif /* WITH_USB_HOST_SUPPORT */

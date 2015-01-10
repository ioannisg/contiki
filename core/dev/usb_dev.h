/**
 * Copyright (c) 2013, Calipso project consortium
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or
 * other materials provided with the distribution.
 * 
 * 3. Neither the name of the Calipso nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific
 * prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/ 

/**
 * usb_dev.h
 *
 * Created: 3/19/2014 2:08:58 PM
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se>
 */ 


#ifndef USB_DEV_H_
#define USB_DEV_H_

#include "compiler.h"

/** A socket-like buffer for USB out transfers */
struct usb_out_chunk {
	struct usb_out_chunk *next;
	uint32_t len;
	COMPILER_WORD_ALIGNED uint8_t *buf;
};
typedef struct usb_out_chunk	usb_chunk_t;

/** USB OUT transfer mode */
enum {
	/** USB transfer out will be marked as asynchronous */
	USB_ASYNC_CMD_FLAG = 0,
	
	/** USB transfer out will be marked as synchronous */
	USB_SYNC_CMD_FLAG,
};

/** Generic USB transfer OUT return status */
enum usb_drv_tx_status {
	
	USB_DRV_TX_OK = 0,
	
	USB_DRV_TX_ERR,
	
	USB_DRV_TX_ERR_BUSY,
	
	USB_DRV_TX_ERR_TIMEOUT,
	
};


/**
 * The structure of a USB device driver in Contiki.
 */
struct usb_dev_driver {
	
	const char *name;
	
	/** Initialize the USB driver */
	void (*init)(void);
	
	/** Connection event */
	void (*connect)(le16_t vendor_id, le16_t product_id, uint8_t plug);
	
	/** Vendor change event */
	void (*vendor_change)(le16_t vendor_id, le16_t product_id, uint8_t plug);
	
	/** Initialize listening on the BULK endpoint */
	int (*init_bulk_in)(void);
	
	/** Initialize listening on the INT endpoint */
	int (*init_int_in)(void);	
	
	/** Cancel listening on the BULK endpoint */
	void (*cancel_bulk_in)(void);
	
	/** Cancel listening on the INT endpoint */
	void (*cancel_int_in)(void);	
	
	/** Transfer down to the control endpoint */
	int (*send_ctrl)(uint8_t * buf, iram_size_t buf_size, 
		uint8_t bRequest,	le16_t wValue);
	
	/** Transfer down asynchronously to the BULK endpoint.
	  * Return immediately after transfer is completed. If
	  * the endpoint is busy, return error. Do not use it
	  * inside interrupt context!
	  */
	int (*send_bulk)(usb_chunk_t *ptr);
	
	/** Transfer down asynchronously to the INT endpoint.
	  * Return immediately after transfer is completed. If
	  * the EP is busy, return error. Do not use it inside
	  * interrupt context!
	  */
	int (*send_async_int)(usb_chunk_t *ptr); 
	
	/** Transfer down synchronously to the BULK endpoint.
	  * Wait for completion or transfer time-out. If the
	  * EP is busy, return error.
	  */
	int (*send_sync_bulk)(usb_chunk_t *ptr);
	
	/** Transfer down synchronously to the INT endpoint.
	  * Wait for completion or transfer timeout. If the 
	  * EP is busy, return error.
	  */
	int (*send_sync_int)(usb_chunk_t *ptr);	
	
	/** Check the availability of the BULK USB endpoint. */	
	int (*is_bulk_busy)(void);
	
	/** Check the availability of the INT USB endpoint. */
	int (* is_int_busy)(void);
	
	/** Return the INT USB endpoint buffer. */
	uint8_t* (*int_dataptr)(void);
	
	/** Return the BULK USB endpoint buffer. */
	uint32_t* (*bulk_dataptr)(void);
	
	/** Set INT buffer length. */
	void (*int_set_len)(uint32_t);
	
	/** Set bulk buffer length. */
	void (*bulk_set_len)(uint32_t);
	
	/** Get bulk buffer length. */
	uint32_t (*bulk_datalen)(void);
	
	/** Get bulk buffer length. */
	uint32_t (*int_datalen)(void);
	
	/** Clear INT buffer. */
	void (*int_clear)(void);
	
	/** Clear bulk buffer. */
	void (*bulk_clear)(void);
	
	/** Set bulk-in transfer limit */
	void (*bulk_in_set_limit)(uint8_t limit);	
};


#endif /* USB_DEV_H_ */
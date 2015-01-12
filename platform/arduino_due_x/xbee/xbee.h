#include "core/net/mac/xbee/xbee-api.h"
#include "compiler.h"
#include "process.h"
#include "contiki-conf.h"
/*
 * xbee.h
 *
 * Created: 11/5/2013 1:52:44 PM
 *  Author: Ioannis Glaropoulos
 */ 

#ifndef XBEE_H_
#define XBEE_H_

PROCESS_NAME(xbee_serial_line_process);
PROCESS_NAME(xbee_driver_process);

#ifdef XBEE_CONF_CMD_TIMEOUT_MS
#define XBEE_CMD_TIMEOUT_MS XBEE_CONF_CMD_TIMEOUT_MS
#else
#define XBEE_CMD_TIMEOUT_MS            20
#endif

#ifdef XBEE_CONF_MAX_PENDING_CMD_LEN
#define XBEE_MAX_PENDING_CMD_LEN XBEE_CONF_MAX_PENDING_CMD_LEN
#else
#define XBEE_MAX_PENDING_CMD_LEN       3
#endif

#ifdef XBEE_CONF_HANDLE_SYNC_OPERATIONS
#define XBEE_HANDLE_SYNC_OPERATIONS XBEE_CONF_HANDLE_SYNC_OPERATIONS
#else
#define XBEE_HANDLE_SYNC_OPERATIONS    1
#endif



typedef enum xbee_cmd_status {

	XBEE_STATUS_OK,
	XBEE_STATUS_ERR,
	XBEE_STATUS_INV_CMD,
	XBEE_STATUS_INV_ARG,
	XBEE_STATUS_NO_RSP
} xbee_cmd_status_t;

/**
 * Status of XBEE operation
 */
typedef enum xbee_status{

  XBEE_TX_PKT_TX_OK = 0,
  XBEE_TX_PKT_TX_ERR_INV_ARG,
  XBEE_TX_PKT_TX_ERR_LEN,
  XBEE_TX_PKT_TX_ERR_NO_ACK,
  XBEE_TX_PKT_TX_ERR_CCA_FAIL,
  XBEE_TX_PKT_TX_ERR_OTHER,  
} xbee_status_t;

/**
 * XBEE AT Command response status values
 */
typedef enum xbee_cmd_rsp_status {
  XBEE_CMD_RSP_STATUS_OK = 0,
  XBEE_CMD_RSP_STATUS_ERR = 1,
  XBEE_CMD_RSP_STATUS_INV_CMD = 2,
  XBEE_CMD_RSP_STATUS_INV_PARAM= 3,
  XBEE_CMD_STATUS_RSP_INVALID_RSP = 0xff,  
} xbee_cmd_rsp_status_t;

/**
 * XBEE TX Frame status response
 */
typedef enum xbee_tx_status_rsp {
  XBEE_TX_STATUS_RSP_OK = 0,
  XBEE_TX_STATUS_RSP_NO_ACK = 1,
  XBEE_TX_STATUS_RSP_CCA_FAIL = 2,
  XBEE_TX_STATUS_RSP_PURGED = 3,
  XBEE_TX_STATUS_RSP_INVALID_RSP = 0xff,
} xbee_tx_status_rsp_t;

/**
 * Event posted when a line of input from the XBEE has been received.
 *
 * This event is posted when an entire line of input has been received
 * from the serial port. A data pointer to the incoming line of input
 * is sent together with the event.
 */
extern process_event_t xbee_serial_line_event_message;

/**
 * Event posted when an illegal serial input has been detected.
 */
extern process_event_t xbee_serial_line_error_event_message;

/**
 * Event posted when a new command needs to be sent down.
 */
extern process_event_t xbee_driver_cmd_event;

/**
 * Event posted when a new device response needs to be handled
 */
extern process_event_t xbee_drv_device_response;

/**
 * Flags indicating that a frame/command has been transmitted and a response is pending
 */
extern volatile uint8_t xbee_frame_is_pending, xbee_sync_cmd_is_pending;

/**
 * The frame ID of a pending synchronous AT Command
 */
extern uint8_t xbee_sync_rsp_pending_id;

xbee_cmd_status_t xbee_drv_handle_device_response(xbee_response_t *rsp);
uint8_t xbee_drv_has_pending_operations(void);
void xbee_drv_check_response(void *data);
uint8_t xbee_send_asynchronous_cmd(const xbee_at_command_t* cmd);
void xbee_send_byte_stream_down(uint8_t* _data, uint8_t len);
void xbee_serial_line_init(unsigned long baudrate);
#endif /* XBEE_H_ */
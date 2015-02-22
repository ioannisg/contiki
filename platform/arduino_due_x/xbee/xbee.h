#include "xbee-api.h"
#include "xbee-public.h"
#include "process.h"
#include "netstack_x.h"
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

#ifdef XBEE_CONF_WITH_DUAL_RADIO
#define XBEE_WITH_DUAL_RADIO XBEE_CONF_WITH_DUAL_RADIO
#else
#define XBEE_WITH_DUAL_RADIO           0
#endif

#ifdef XBEE_SERIAL_LINE_CONF_BUFSIZE
#define XBEE_BUFSIZE XBEE_SERIAL_LINE_CONF_BUFSIZE
#else /* SERIAL_LINE_CONF_BUFSIZE */
#define XBEE_BUFSIZE                   128
#endif /* SERIAL_LINE_CONF_BUFSIZE */

#if (XBEE_BUFSIZE & (XBEE_BUFSIZE - 1)) != 0
#error XBEE_SERIAL_LINE_CONF_BUFSIZE must be a power of two (i.e., 1, 2, 4, 8, 16, 32, 64, ...).
#error "Change XBEE_SERIAL_LINE_CONF_BUFSIZE in contiki-conf.h."
#endif

typedef enum {
  XBEE_SERIAL_DEV_PRIMAL = NETSTACK_802154,
  XBEE_SERIAL_DEV_SECOND = NETSTACK_802154_SEC,
} xbee_dev_num_t;

/** XBEE serial line statistics */
typedef struct xbee_dev_stats {
  uint16_t sfd_err;
  uint16_t msb_err;
  uint16_t lsb_err;
  uint16_t pkt_err;
  uint16_t cmd_err;
  uint16_t ovf_err;
#if XBEE_HANDLE_SYNC_OPERATIONS
  uint16_t ovf_on_err;
#endif
  uint16_t tx_pkt_count;
  uint16_t rx_pkt_count;
  uint8_t rst_count;
} xbee_dev_stats_t;

/** XBEE configuration */
typedef struct xbee_dev_config {
  uint8_t channel;
  uint8_t channel_sec;
  uint16_t panid;
  uint8_t mac_mode;
  uint8_t power;
  uint16_t my_addr;  
  uint8_t api;
} xbee_dev_config_t;

/** The structure of an XBEE serial device */
typedef struct xbee_device {
  uint32_t baudrate;
  struct ringbuf *buf;
#if XBEE_HANDLE_SYNC_OPERATIONS
  struct ringbuf *onbuf;
#endif
  xbee_dev_num_t sdev;
  xbee_dev_stats_t *stats;
  void ( *writeb)(unsigned char c);
  uint8_t ptr;
  uint8_t esc;
  uint8_t rsp[XBEE_BUFSIZE];
} xbee_device_t;

typedef enum xbee_cmd_status {

	XBEE_STATUS_OK,
	XBEE_STATUS_ERR,
	XBEE_STATUS_INV_CMD,
	XBEE_STATUS_INV_ARG,
	XBEE_STATUS_NO_RSP,
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
 * The XBEE device structure
 */
extern xbee_device_t xbee_dev;

#if XBEE_WITH_DUAL_RADIO
/**
 * The secondary XBEE device structure
 */
extern xbee_device_t xbee_dev0;
#endif

/* Internal API */
uint8_t xbee_drv_has_pending_operations(xbee_device_t *dev);
uint8_t xbee_drv_has_pending_cmd(xbee_device_t *dev);
void xbee_drv_check_response(void *data);
uint8_t xbee_send_asynchronous_cmd(xbee_device_t *dev, const xbee_at_command_t* cmd);
void xbee_send_byte_stream_down(xbee_device_t *dev, uint8_t* _data, uint8_t len);
void xbee_serial_line_init(unsigned long baudrate);
void xbee_serial_line_reset(xbee_device_t *device);
void xbee_do_hw_reset(xbee_device_t *device);
#endif /* XBEE_H_ */
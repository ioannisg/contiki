/*
 * ar9170_drv.h
 *
 * A Contiki OS driver module for AR9170 WiFi chip
 *
 * Created: 1/28/2014 9:26:25 AM
 *  Author: Ioannis Glaropoulos
 */ 

#ifndef AR9170_DRV_H_
#define AR9170_DRV_H_

#include "compiler.h"
#include "usb.h"
#include "wifi.h"
#include <stdio.h>
#include "rtimer.h"
#include "ar9170.h"


/* Configuration */
#define AR9170_DRV_TX_PKT_MAX_QUEUE_LEN	6
#define AR9170_DRV_RX_PKT_MAX_QUEUE_LEN	6

/* We do not handle packets larger than 512 bytes. */
#define AR9170_DRV_RX_MAX_PKT_LENGTH	IEEE80211_TX_MAX_FRAME_LENGTH
#define AR9170_DRV_TX_MAX_PKT_LENGTH	IEEE80211_TX_MAX_FRAME_LENGTH

#if (AR9170_DRV_RX_MAX_PKT_LENGTH > USB_BULK_EP_MAX_SIZE)
#error USB library does not handle such large incoming packets! Reduce sizes.
#endif
#if (AR9170_DRV_TX_MAX_PKT_LENGTH > USB_BULK_EP_MAX_OUT_SIZE)
#error USB library does not handle such large outgoing packets! Reduce sizes.
#endif

#define AR9170_DRV_SCANNING_MODE	BIT(1)
#define AR9170_DRV_SNIFFING_MODE	BIT(2)



/* AR9170 driver command object structure*/
#define AR9170_DRV_CMD_MAX_QUEUE_LEN	4

enum ar9170_tx_state {
	AR9170_PKT_UNKNOWN,
	AR9170_PKT_QUEUED,
	AR9170_DRV_PKT_SENT,
};
/* AR9170 driver packet object structure */
struct ar9170_pkt_buffer;
struct ar9170_pkt_buffer {
	struct ar9170_pkt_buffer *next;
	enum ar9170_tx_state pkt_state;
	uint32_t pkt_len;
	uint8_t pkt_data[AR9170_DRV_TX_MAX_PKT_LENGTH];
};
typedef struct ar9170_pkt_buffer ar9170_pkt_buf_t;


/** AR9170 command response timeout value */
#define AR9170_RSP_TIMEOUT_MS				6
#define AR9170_TX_STATUS_RSP_TIMEOUT_MS		25

#define AR9170_GET_LOCK(_lock)	if ((*_lock) == 1) { \
	printf("ERROR: AR9170 lock set!\n"); \
} \
(*_lock) = 1;

#define AR9170_CHECK_RELEASE_LOCK(_lock)	if ((*_lock) == 0) { \
	printf("ERROR: AR91790_DRV lock clear!\n");	\
} \
(*_lock) = 0;

#define AR9170_RELEASE_LOCK(_lock)	(*_lock) = 0;

#define AR9170_WAIT_FOR_LOCK_CLEAR(_lock) while ((*_lock) != 0) { \
} \

#define AR9170_WAIT_FOR_LOCK_CLEAR_TIMEOUT_MS(_lock, _timeout) uint8_t cmd_timeout = 0; \
rtimer_clock_t due_time = (RTIMER_NOW()) + _timeout * (RTIMER_SECOND/1000); \
while (((*_lock) != 0) && (due_time > RTIMER_NOW())) { \
} \
if (due_time <= RTIMER_NOW()) \
	cmd_timeout = 1; \

#define AR9170_IS_RSP_TIMEOUT	cmd_timeout 

enum ar9170_drv_status {
	AR9170_DRV_TX_OK = 0,
	AR9170_DRV_TX_WIFI_ERR,
	AR9170_DRV_TX_ERR_FULL,
	AR9170_DRV_TX_ERR_USB,
	AR9170_DRV_TX_ERR_TIMEOUT,
	AR9170_DRV_ERR_INV_ARG,
	AR9170_DRV_CMD_FULL,
	AR9170_DRV_MEM_ERR,
};

/** Driver action vector */
#define AR9170_DRV_ACTION_FILTER_RX_DATA	BIT(0)
#define AR9170_DRV_ACTION_CANCEL_RX_FILTER	BIT(1)
#define AR9170_DRV_ACTION_CHECK_PSM			BIT(2)
#define AR9170_DRV_ACTION_JOIN_DEFAULT_IBSS BIT(3)
#define AR9170_DRV_ACTION_UPDATE_BEACON		BIT(4)

#define AR9170_DRV_ACTION_GENERATE_ATIMS	BIT(5)
#define AR9170_DRV_ACTION_FLUSH_OUT_ATIMS	BIT(6)

#define AR9170_DRV_INIT_ACTION_FLAG_STATUS	uint8_t is_action_flag_set
#define AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(_flag) cpu_irq_enter_critical(); \
is_action_flag_set = 0; \
if(ar9170_action_vector & _flag) { \
	is_action_flag_set = 1; \
	ar9170_action_vector &= ~_flag; \
} \
cpu_irq_leave_critical(); \

#define AR9170_IS_ACTION_FLAG_SET	is_action_flag_set

typedef uint8_t ar9170_action_t;

extern ar9170_action_t	ar9170_action_vector;

extern const struct wifi_driver ar9170_driver; 


/* Queue getters */
struct memb* ar9170_drv_get_rx_queue(void);
struct memb* ar9170_drv_get_tx_queue(void);

/* Order action */
void ar9170_drv_order_action(struct ar9170* ar, ar9170_action_t actions);
/* Status response */
void ar9170_drv_handle_status_response( ar9170_pkt_buf_t * skb, 
	uint8_t success, unsigned int r, unsigned int t, uint8_t cookie);
#endif /* AR9170-DRV_H_ */
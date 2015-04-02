/*
 * ar9170_drv.c
 *
 * AR9170 Radio Driver 
 *
 * Created: 1/28/2014 12:24:32 PM
 *  Author: Ioannis Glaropoulos
 */
#include "contiki-net.h"
#include "contiki-lib.h"

#include "netstack_x.h"
#include "linkaddrx.h"

#include "ar9170-drv.h"
#include "ar9170.h"
#include "ar9170_fw.h"
#include "ar9170_cmd.h"
#include "ar9170_fwcmd.h"
#include "ar9170_psm.h"

#include "wire_digital.h"

#include "carl9170_v197.h"
#include "carl9170_v199.h"

#include "usbstack.h"
#include "wifi.h"
#include "pend-SV.h"

#include "delay.h"
#include "core_cm3.h"
#include "interrupt\interrupt_sam_nvic.h"
#include "usb_protocol_vendor.h"
#include "compiler.h"

#include "ieee80211\mac80211.h"
#include "ieee80211\ieee80211_ibss.h"
#include "ieee80211_ibss_setup.h"
/* FIXME ar9170 should NOT directly include this for the status handler */
#include "ieee80211_rdc.h" 


#include <stdlib.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
#include "ieee80211.h"

#if WITH_WIFI_SUPPORT

COMPILER_WORD_ALIGNED uint8_t scsi_stop_unit[31] = {
	0x55, 0x53, 0x42, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x1B,	0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

PROCESS(ar9170_driver_proc, "AR9170_driver_proc");

/* Declare the driver queues */
LIST(ar9170_tx_list);
LIST(ar9170_rx_list);
LIST(ar9170_cmd_list);
LIST(ar9170_tx_pending_list);

/* Declare the static memory for the command queue elements */
MEMB(ar9170_cmd_buf_mem, ar9170_cmd_buf_t, AR9170_DRV_CMD_MAX_QUEUE_LEN);
/* Declare the static memory for the tx queue */
MEMB(ar9170_tx_buf_mem, ar9170_pkt_buf_t, AR9170_DRV_TX_PKT_MAX_QUEUE_LEN);
/* Declare the static memory for the rx queue */
MEMB(ar9170_rx_buf_mem, ar9170_pkt_buf_t, AR9170_DRV_RX_PKT_MAX_QUEUE_LEN);
/* Declare the static memory for the tx pending queue */
MEMB(ar9170_tx_pending_buf_mem, ar9170_pkt_buf_t, AR9170_DRV_TX_PKT_MAX_QUEUE_LEN);

/* The global AR9170 device structure */
struct ar9170 ar9170_dev;
/* The driver asynchronous action vector */
ar9170_action_t ar9170_action_vector;

/* Declare the MAC address array of the device */
static linkaddr1_t ar9170_macaddr;

struct memb* ar9170_drv_get_tx_queue() {
	return &ar9170_tx_buf_mem;
}

struct memb* ar9170_drv_get_rx_queue() {
	return &ar9170_rx_buf_mem;
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_dealloc(void)
{
	/* Clear lists */
	
	/* Deallocate memory blocks */
	
	/* Zero the ar9170 memory */
	
	/* TODO */
	
}
/*---------------------------------------------------------------------------*/
void 
ar9170_drv_handle_status_response(ar9170_pkt_buf_t *skb, uint8_t success, 
	unsigned int rate_index, unsigned int transmissions, uint8_t cookie)
{	
	/* The function is called within USB interrupt context */		
	/* 
	 * Release TX waiting lock [it could already be cleared if
	 * if the packet under transmission didn't require a lock]
	 */
	AR9170_RELEASE_LOCK(&ar9170_dev.tx_wait);
	
	/* Inform RDC about the status response */
	ieee80211_rdc_status_rsp_handler(success,rate_index,transmissions,cookie);
	
	/* Free pending packet status and deallocate memory */
	list_remove(ar9170_tx_pending_list, skb);
	memb_free(&ar9170_tx_pending_buf_mem, skb);
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_remove_pending_status_pkts(void)
{
	PRINTF("AR9170_DRV: clear-pending-status-rsps\n");
	/* Remove all pending status responses for sent frames */
	while(list_length(ar9170_tx_pending_list) > 0) {
		ar9170_pkt_buf_t * pending_pkt = list_pop(ar9170_tx_pending_list);
		if (pending_pkt != NULL && pending_pkt->pkt_state == AR9170_DRV_PKT_SENT)
			memb_free(&ar9170_tx_pending_buf_mem, pending_pkt);
	}
}
/*---------------------------------------------------------------------------*/
static ar9170_pkt_buf_t*
ar9170_drv_alloc_pending_status_pkt(struct ieee80211_tx_info* info, 
	enum ar9170_tx_state _state) 
{	
	ar9170_pkt_buf_t *new_pkt;
	/* Allocate memory for status packet. */
	cpu_irq_enter_critical();
	new_pkt = (ar9170_pkt_buf_t*)memb_alloc(&ar9170_tx_pending_buf_mem);
	cpu_irq_leave_critical();
	
	if(new_pkt == NULL) {
		PRINTF("AR9170_DRV: list-full, can't store pending status.\n");
		return NULL;
	}
	/* Copy packet header and state */
	new_pkt->next = NULL;
	new_pkt->pkt_state = _state;
	new_pkt->pkt_len = packetbuf_hdrlen();
	packetbuf_copyto_hdr(new_pkt->pkt_data);
	/* Copy also the tx info */
	memcpy(new_pkt->pkt_data + new_pkt->pkt_len, info, 
		sizeof(struct ieee80211_tx_info));
		
	return new_pkt;
}
/*---------------------------------------------------------------------------*/
static uint8_t
ar9170_drv_update_queued_pkt_status(uint8_t cookie)
{
	ar9170_pkt_buf_t *pending_pkt = list_head(ar9170_tx_pending_list);
	while(pending_pkt != NULL) {
		if (pending_pkt->pkt_state == AR9170_PKT_QUEUED) {
			uint8_t c = ((struct _ar9170_tx_superframe*)(&pending_pkt->pkt_data[0]))->s.cookie;
			if (cookie == c) {
				pending_pkt->pkt_state = AR9170_DRV_PKT_SENT;
				return 1;
			}
		}
		pending_pkt = pending_pkt->next;
	}
	return 0;
}
/*---------------------------------------------------------------------------*/
static int 
ar9170_drv_start_network_interface(struct ar9170* ar) 
{
	/* Start device */
	if(ar9170_op_start(ar)) {
		PRINTF("AR9170_DRV: dev could not be started\n");
		return AR9170_DRV_TX_ERR_USB;
	}
	
	/* Add interface */
	if (ar9170_op_add_interface(ar->hw, ar->vif)) {
		PRINTF("ERROR: Interface could not be added!\n");
		return AR9170_DRV_MEM_ERR;
	}
	
	/* Set up 802.11 networking */
	uint32_t bss_info_changed_flag = 0;
	/* Call bss_info_changed for modifying slot time */
	bss_info_changed_flag |= BSS_CHANGED_ERP_SLOT;
	ar9170_op_bss_info_changed(ar->hw, ar->vif, &ar->vif->bss_conf, bss_info_changed_flag);
	/* TODO continue*/
	uint32_t config_flag_changed = 0;
	int err;
	
	/* Update TX QoS */
	struct ieee80211_tx_queue_params params[4];
	
	CARL9170_FILL_QUEUE(params[3], 7, 31, 1023,	0);
	CARL9170_FILL_QUEUE(params[2], 3, 31, 1023,	0);
	CARL9170_FILL_QUEUE(params[1], 2, 15, 31,	188);
	CARL9170_FILL_QUEUE(params[0], 2, 7,  15,	102);
	
	/* Configure tx queues */
	if (ar9170_op_conf_tx(ar->hw, ar->vif, 0, &params[0]) ||
		ar9170_op_conf_tx(ar->hw, ar->vif, 1, &params[1]) ||
		ar9170_op_conf_tx(ar->hw, ar->vif, 2, &params[2]) ||
		ar9170_op_conf_tx(ar->hw, ar->vif, 3, &params[3])) {
		PRINTF("ERROR: Could not set TX parameters.\n");
		return -AR9170_DRV_TX_ERR_USB;
	}
	
	/* BSS Change notification: change device to idle. */
	config_flag_changed = BSS_CHANGED_IDLE;
	ar9170_op_bss_info_changed(ar->hw, ar->vif, &ar->vif->bss_conf, 
		config_flag_changed);
	
	/* Flush device if necessary. Drain beacon.*/
	ar9170_op_flush(ar->hw, 0);
	
	config_flag_changed = 0;
	config_flag_changed |= IEEE80211_CONF_CHANGE_CHANNEL |
						IEEE80211_CONF_CHANGE_POWER |
						IEEE80211_CONF_CHANGE_LISTEN_INTERVAL;
						// |	IEEE80211_CONF_CHANGE_PS;
	
	/* Configure network hardware parameters */
	err =  __ieee80211_hw_config(ar->hw, config_flag_changed);
	
	if (err) {
		printf("ERROR: IEEE80211 HW configuration returned errors.\n");
		return AR9170_DRV_TX_ERR_USB;
	}
	/* Filter everything */
	ar9170_rx_filter(ar, AR9170_RX_FILTER_EVERYTHING);
	
	/* Initially the driver is in DATA TX state */
	ar9170_dev.psm_mgr.state = AR9170_PSM_DATA;
	
	/* Copy the BSSID to the driver container */
	memcpy(ar->common.curbssid, ieee80211_ibss_vif.bss_conf.bssid, ETH_ALEN);
	
	/* Set the Ethernet address for the ARP code */	
	uip_lladdr_t eth_addr;
	memcpy(eth_addr.addr, ieee80211_ibss_vif.addr, UIP_LLADDR_LEN);
	uip_setethaddr(eth_addr);
	#if DEBUG
	PRINTF("AR9170_DRV: Copying MAC address to UIP ARP: %02x:%02x:%02x:%02x:%02x:%02x.\n",
			uip_lladdr.addr[0],
			uip_lladdr.addr[1],
			uip_lladdr.addr[2],
			uip_lladdr.addr[3],
			uip_lladdr.addr[4],
			uip_lladdr.addr[5]);
	#endif
	/* Everything is fine */
	return AR9170_DRV_TX_OK;
}
/*---------------------------------------------------------------------------*/
static void 
ar9170_drv_send_eject_cmd(void) 
{
	PRINTF("AR9170_DRV: send-eject-cmd\n");
	if(list_length(ar9170_tx_list) > 0) {
		ar9170_pkt_buf_t *eject_cmd = list_pop(ar9170_tx_list);
		if(eject_cmd == NULL) {
			printf("AR9170_DRV: eject cmd null\n");
			return;
		}
		usb_chunk_t eject_bulk_cmd;
		eject_bulk_cmd.buf = eject_cmd->pkt_data;
		eject_bulk_cmd.len = eject_cmd->pkt_len;
		
		if(USB_STACK.send_bulk(&eject_bulk_cmd) == USB_DRV_TX_OK) {
			delay_ms(100);
			PRINTF("eject-sent-ok\n");
			process_poll(&ar9170_driver_proc);
		} else  {
			PRINTF("AR9170_DRV: eject-error\n");
		}
		/* Free packet mem. Restore element */
		cpu_irq_enter_critical();
		memb_free(&ar9170_tx_buf_mem, eject_cmd);
		cpu_irq_leave_critical();
	} else {
		PRINTF("no eject command\n");
	}
}
/*---------------------------------------------------------------------------*/
static uint8_t
ar9170_drv_verify_cmd(void)
{
	if (ar9170_dev.cmd_buffer.cmd_data == NULL || ar9170_dev.cmd_buffer.cmd_len == 0
		|| ar9170_dev.cmd_buffer.cmd_len > USB_INT_EP_MAX_SIZE) {
		PRINTF("AR9170_DRV: null/empty command\n");
		return AR9170_DRV_ERR_INV_ARG;
	}
	return AR9170_DRV_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_rx_filter(wifi_rx_filter_flag_t rx_filter_flag)
{
	switch (rx_filter_flag) {
		
		case WIFI_FILTER_ALL:
			return	ar9170_rx_filter(&ar9170_dev,
			AR9170_RX_FILTER_OTHER_RA |
			AR9170_RX_FILTER_CTL_OTHER |
			AR9170_RX_FILTER_BAD |
			AR9170_RX_FILTER_CTL_BACKR |
			AR9170_RX_FILTER_DATA |
			AR9170_RX_FILTER_MGMT |
			AR9170_RX_FILTER_DECRY_FAIL);
			break;
		case WIFI_FILTER_MGMT:
			return	ar9170_rx_filter(&ar9170_dev,
			AR9170_RX_FILTER_OTHER_RA |
			AR9170_RX_FILTER_CTL_OTHER |
			AR9170_RX_FILTER_BAD |
			AR9170_RX_FILTER_CTL_BACKR |
			AR9170_RX_FILTER_MGMT |
			AR9170_RX_FILTER_DECRY_FAIL);
			break;
		case WIFI_FILTER_DATA:
			return	ar9170_rx_filter(&ar9170_dev,
			AR9170_RX_FILTER_OTHER_RA |
			AR9170_RX_FILTER_CTL_OTHER |
			AR9170_RX_FILTER_BAD |
			AR9170_RX_FILTER_CTL_BACKR |
			AR9170_RX_FILTER_DATA |
			AR9170_RX_FILTER_DECRY_FAIL);
			break;
		case WIFI_NO_FILTER:
		return	ar9170_rx_filter(&ar9170_dev,
			AR9170_RX_FILTER_OTHER_RA |
			AR9170_RX_FILTER_CTL_OTHER |
			AR9170_RX_FILTER_BAD |
			AR9170_RX_FILTER_CTL_BACKR |
			AR9170_RX_FILTER_DECRY_FAIL);
			break;
		default:
			PRINTF("AR9170_DRV: rx-filter-flag-err\n");
			return -1;
	}
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_join(void)
{
	/* State change to DATA TX PERIOD */
	ar9170_dev.psm_mgr.state = AR9170_PSM_DATA;
	/* Stop scanning mode */
	ar9170_dev.rx_filter.mode = 0;
	ar9170_dev.beacon_enabled = 1;
	ar9170_dev.vif->bss_conf.enable_beacon = 1;
	ar9170_dev.vif_info->enable_beacon = 1;
	/* Send BCN ctrl command down, to start beaconing */
	ar9170_update_beacon(&ar9170_dev, 1);
	/* Set state to joined */
	if (ar9170_dev.is_joined == 1)
		printf("AR9170_DRV: join-flag-set!\n");
	ar9170_dev.is_joined = 1;
	/* Clear RX filters if any */
	ar9170_drv_rx_filter(WIFI_NO_FILTER);
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called within thread or low-priority interrupt context.
 */
static void
ar9170_drv_handle_critical_actions(struct ar9170* ar)
{
	/* It is unlikely, but if this function is called inside low-priority
	 * interrupt a BULK or INT chunk could be under transfer, means we do
	 * not transmit and we go for the next best thing: poll the scheduler
	 * so the commands are executed in thread context. Note, however that
	 * if an additional driver is using the USB interface this may create
	 * a serious issue. 
	 */
	//if ((SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0 || __get_IPSR() != 0) {
		if (unlikely(ar->cmd_only_thread ||
				ar->cmd_wait ||
				USB_STACK.is_bulk_busy())) {
			PRINTF("AR9170_DRV: critical-cmd-wait-on\n");
			process_poll(&ar9170_driver_proc);
			return;	
		}				
	//}	
	AR9170_DRV_INIT_ACTION_FLAG_STATUS;
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_JOIN_DEFAULT_IBSS);
	if(AR9170_IS_ACTION_FLAG_SET) {
		ar9170_set_beacon_timers(ar);
		/* Restore USB RX limit */
		USB_STACK.bulk_in_set_limit(0);
		ieee80211_sta_join_ibss();
		ar9170_drv_join();
		process_post(&ieee80211_ibss_setup_process, PROCESS_EVENT_CONTINUE, NULL);
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_FILTER_RX_DATA);	
	if(AR9170_IS_ACTION_FLAG_SET) {
		//ar9170_drv_rx_filter(WIFI_FILTER_DATA);
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_CANCEL_RX_FILTER);	
	if(AR9170_IS_ACTION_FLAG_SET) {
		//ar9170_drv_rx_filter(WIFI_NO_FILTER);
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_UPDATE_BEACON);
	if(AR9170_IS_ACTION_FLAG_SET) {
		//ar9170_update_beacon(ar,1); FIXME
	}
	
	/* Poll process for non-critical actions */
	process_poll(&ar9170_driver_proc);
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_handle_actions_offline(struct ar9170 *ar)
{
	AR9170_DRV_INIT_ACTION_FLAG_STATUS;
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_JOIN_DEFAULT_IBSS);
	if(AR9170_IS_ACTION_FLAG_SET) {
		ar9170_set_beacon_timers(ar);
		/* Restore USB RX limit */
		USB_STACK.bulk_in_set_limit(0);
		ieee80211_sta_join_ibss();
		ar9170_drv_join();
		process_post(&ieee80211_ibss_setup_process, PROCESS_EVENT_CONTINUE, NULL);
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_FILTER_RX_DATA);
	if(AR9170_IS_ACTION_FLAG_SET) {
		//ar9170_drv_rx_filter(WIFI_FILTER_DATA);
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_CANCEL_RX_FILTER);
	if(AR9170_IS_ACTION_FLAG_SET) {
		//ar9170_drv_rx_filter(WIFI_NO_FILTER);
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_UPDATE_BEACON);
	if(AR9170_IS_ACTION_FLAG_SET) {
		//ar9170_update_beacon(ar,1); FIXME
	}
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_GENERATE_ATIMS);
	if(AR9170_IS_ACTION_FLAG_SET) {
		NETSTACK_1_MAC.on();//FIXME
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_FLUSH_OUT_ATIMS);
	if(AR9170_IS_ACTION_FLAG_SET) {
		NETSTACK_1_MAC.off(0);//FIXME
	}
	
	AR9170_DRV_CRITICAL_CHECK_ACTION_FLAG(AR9170_DRV_ACTION_CHECK_PSM);
	if(AR9170_IS_ACTION_FLAG_SET) {
		NETSTACK_1_MAC.off(1);//FIXME
	}
}
/*---------------------------------------------------------------------------*/
/**
 * This function is called within USB/TC interrupt context.
 */
void 
ar9170_drv_order_action(struct ar9170* ar, ar9170_action_t actions)
{	
	if (unlikely(actions & ar9170_action_vector)) {
		printf("AR9170_DRV: error ordering actions (%02x) (%02x)\n",
			actions, ar9170_action_vector);
		return;
	}
	/* Otherwise append new ordered actions */
	ar9170_action_vector |= actions;
		
	/* Order low priority interrupt handling */
	pend_sv_register_handler((pend_sv_callback_t)ar9170_drv_handle_critical_actions, ar);
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_config(struct ieee80211_hw *_hw, uint32_t flag)
{
	return ar9170_op_config(_hw, flag);
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_tx_config(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	uint16_t queue, const struct ieee80211_tx_queue_params *param)
{
	return ar9170_op_conf_tx(hw, vif, queue, param);
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_rx_config(void)
{
	struct ar9170* ar = &ar9170_dev;
	
	// TODO - automate the following
	unsigned int new_flags = 0x1001;
	unsigned int new_flags_e = 0x0001;
	ar->rx_filter_caps = 0xed;
	ar->cur_mc_hash = 1;
/*	
	ar9170_op_configure_filter(hw, 0x80000100, &new_flags, 0x80001001);
	mdelay(2);
	ar9170_op_configure_filter(hw, 0x80000100, &new_flags_e, 0x80000001);
	mdelay(2);
	ar9170_op_configure_filter(hw, 0x80000100, &new_flags, 0x800001001);
	mdelay(2);
	ar9170_op_configure_filter(hw, 0x80000100, &new_flags, 0xc00001001);
*/	
	ar9170_update_multicast_mine(ar, 0x80000000, 0x00001001);
	
	ar9170_update_multicast_mine(ar, 0x80000000, 0x00000001);
	
	ar9170_update_multicast_mine(ar, 0x80000000, 0x00001001);
	
	ar9170_update_multicast_mine(ar, 0xc0000000,0x00001001);
	
}
/*---------------------------------------------------------------------------*/
static void 
ar9170_drv_enable_scan(void)
{
	/* Leave out management only */
	ar9170_drv_rx_filter(WIFI_FILTER_DATA);
	
	/* Inform ar9170 that beacons should be trapped */	
	ar9170_dev.rx_filter.mode = AR9170_DRV_SCANNING_MODE;
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_init(void)
{	
	pend_sv_init();
	configure_output_pin(PRE_TBTT_ACTIVE_PIN, LOW, DISABLE, DISABLE);
	configure_output_pin(PRE_TBTT_PASSIVE_PIN, LOW, DISABLE, DISABLE);
	configure_output_pin(DOZE_ACTIVE_PIN, LOW, DISABLE, DISABLE);
	configure_output_pin(DOZE_PASSIVE_PIN, LOW, DISABLE, DISABLE);
	configure_output_pin(TX_ACTIVE_PIN, LOW, DISABLE, DISABLE);
	configure_output_pin(TX_PASSIVE_PIN, LOW, DISABLE, DISABLE);
	wire_digital_write(PRE_TBTT_ACTIVE_PIN, LOW);
	wire_digital_write(PRE_TBTT_PASSIVE_PIN, LOW);
	wire_digital_write(DOZE_ACTIVE_PIN, LOW);
	wire_digital_write(DOZE_PASSIVE_PIN, LOW);
	wire_digital_write(TX_ACTIVE_PIN, LOW);
	wire_digital_write(TX_PASSIVE_PIN, LOW);
	
	/* Initialize device structure */
	memset(&ar9170_dev, 0, sizeof(struct ar9170));
	
	/* Initialize firmware */
	ar9170_init_fw();
	
	/* Initialize driver queues */
	list_init(ar9170_rx_list);
	list_init(ar9170_tx_list);
	list_init(ar9170_cmd_list);
	list_init(ar9170_tx_pending_list);

	ar9170_dev.ar9170_rx_list = ar9170_rx_list;
	ar9170_dev.ar9170_tx_list = ar9170_tx_list;
	ar9170_dev.ar9170_cmd_list = ar9170_cmd_list;
	ar9170_dev.ar9170_tx_pending_list = ar9170_tx_pending_list;
	
	/* Initialize static  memories */
	memb_init(&ar9170_cmd_buf_mem);
	memb_init(&ar9170_tx_buf_mem);
	memb_init(&ar9170_rx_buf_mem);
	memb_init(&ar9170_tx_pending_buf_mem);	
	
	/* Add memories to ar9170 */
	ar9170_dev.ar9170_cmd_buf_mem = ar9170_cmd_buf_mem;
		
	ar9170_action_vector = 0;
	
	/* Initialize receiving process */
	process_start(&ar9170_driver_proc, NULL);
}
/*---------------------------------------------------------------------------*/
static uint8_t 
ar9170_drv_queue_packet(uint8_t has_priority)
{	
	PRINTF("AR9170_DRV: queue-pkt\n");
	ar9170_pkt_buf_t *new_pkt;	
	/* Allocate memory for packet. */	
	new_pkt = (ar9170_pkt_buf_t*)memb_alloc(&ar9170_tx_buf_mem);	
	
	if(new_pkt == NULL) {
		printf("AR9170_DRV: TX-list-full. Drop queue pkt.\n");
		return WIFI_TX_ERR_FULL;
	}		
	new_pkt->next = NULL;
	new_pkt->pkt_state = AR9170_PKT_QUEUED;
	new_pkt->pkt_len = packetbuf_totlen();
	packetbuf_copyto(new_pkt->pkt_data);
		
	if(has_priority) {
		/* Add new USB chunk to the LIFO queue */
		list_push(ar9170_tx_list, new_pkt);
	} else {
		/* Add new USB chunk to the FIFO queue */
		list_add(ar9170_tx_list, new_pkt);
	}
	PRINTF("AR9170_DRV: Queue-len (%u)\n",list_length(ar9170_tx_list));
	/* Wake-up driver process */
	process_poll(&ar9170_driver_proc);	
	return WIFI_TX_OK;	
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_send_packet(uint8_t is_synchronous)
{
	uint8_t result = 0;	
	PRINTF("AR9170_DRV: send-pkt\n");
	
	if (packetbuf_hdrptr() == NULL || packetbuf_totlen() == 0) {
		PRINTF("AR9170_DRV: TX input error\n");
		return WIFI_TX_ERR_FATAL;
	}	
	/* Prepare to send */
	if (ar9170_op_tx(ar9170_dev.hw, NULL, NULL)) {
		PRINTF("AR9170_DRV: tx-failed\n");
		return WIFI_TX_ERR_FATAL;
	}	
	/* Store tx info */
	struct ieee80211_tx_info *tx_info = NULL;
	if (linkaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),&linkaddr_null)) {
		tx_info = &ieee80211_ibss_bcast_tx_info;
	} else {
		tx_info = &ieee80211_ibss_data_tx_info;
	}
	/* Prepare USB chunk */
	usb_chunk_t usb_pkt;
	usb_pkt.buf = packetbuf_hdrptr();
	usb_pkt.len = packetbuf_totlen();
	
#if DEBUG
	int i;
	PRINTF("HDR[%u]:",packetbuf_hdrlen());
	for (i=0; i<36; i++)
		PRINTF("%02x ",usb_pkt.buf[i]);
	PRINTF("\n");	
#endif	
	if (is_synchronous) {
		/* Wait for the response of the previous packet. 
		 * Then send immediately. 
		 */
		struct _ar9170_tx_superframe *txc = packetbuf_hdrptr();
		if(ar9170_dev.tx_wait != 0)			
			printf("AR9170_DRV: SYNC-PKT-WAIT (%u)\n",txc->s.cookie);
		
		AR9170_WAIT_FOR_LOCK_CLEAR_TIMEOUT_MS(&ar9170_dev.tx_wait, (AR9170_TX_STATUS_RSP_TIMEOUT_MS));
		if(AR9170_IS_RSP_TIMEOUT) {
			printf("AR9170_DRV: pkt-rsp-timeout\n");
			ar9170_drv_remove_pending_status_pkts();
			AR9170_RELEASE_LOCK(&ar9170_dev.tx_wait);
			return WIFI_TX_ERR_TIMEOUT;
		}
		PRINTF("tx-sync (%u)(%u)\n",txc->s.cookie,packetbuf_datalen());
		
		if (USB_STACK.is_bulk_busy()) {
			/* Wait for the bulk endpoint to become idle. TODO check if redundant */
			printf("AR9170_DRV: bulk-busy-wait\n");
			while (USB_STACK.is_bulk_busy());
		}		
		/* GET lock on packet status wait flag */
		AR9170_GET_LOCK(&ar9170_dev.tx_wait);
			
		/* Store packet header on waiting list, along with TX info. */
		ar9170_pkt_buf_t *pending_tx_pkt = 
			ar9170_drv_alloc_pending_status_pkt(tx_info, AR9170_DRV_PKT_SENT);
		if (pending_tx_pkt == NULL) {
			PRINTF("AR9170_DRV: push-pending-tx-err\n");
			AR9170_RELEASE_LOCK(&ar9170_dev.tx_wait);
			return WIFI_TX_ERR_FULL;
		}		
		/* Ready to send. Block until transfer down is completed. */
		result = USB_STACK.send_sync_bulk(&usb_pkt);
		if (unlikely(result != USB_DRV_TX_OK)) {
			printf("AR9170_DRV: USB SYNC TX ERR (%u)\n", result);
			/* Deallocate pending packet as it was not sent */
			memb_free(&ar9170_tx_pending_buf_mem, pending_tx_pkt);
			AR9170_RELEASE_LOCK(&ar9170_dev.tx_wait);
			return WIFI_TX_ERR_USB;
		}
		/* Add to pending list */
		cpu_irq_enter_critical();		
		list_add(ar9170_tx_pending_list, pending_tx_pkt);
		cpu_irq_leave_critical();		
		return WIFI_TX_OK;
		
	} else {
		/* Send immediately if the device is not busy */
		if (!USB_STACK.is_bulk_busy())	{
					
			/* Store packet header on waiting list, along with TX info. */
			ar9170_pkt_buf_t *pending_tx_pkt =
				ar9170_drv_alloc_pending_status_pkt(tx_info, AR9170_DRV_PKT_SENT);
			if (pending_tx_pkt == NULL) {
				PRINTF("AR9170_DRV: push-pending-tx-err\n");
				return WIFI_TX_ERR_FULL;
			}			
			/* Attempt to send the data. Return immediately. */
			result = USB_STACK.send_sync_bulk(&usb_pkt);
			if (result != USB_DRV_TX_OK) {
				printf("AR9170_DRV: USB ASYNC TX ERR (%u)\n",result);
				/* Deallocate pending packet as it was not sent */
				memb_free(&ar9170_tx_pending_buf_mem, pending_tx_pkt);
				return WIFI_TX_ERR_USB;				
			} else {
				/* Add to pending list */
				cpu_irq_enter_critical();	
				list_add(ar9170_tx_pending_list, pending_tx_pkt);
				cpu_irq_leave_critical();
				return WIFI_TX_OK;
			}
			
		} else {
			/* Outgoing link is busy. Queue the packet and its (queued) status. */
			printf("AR9170_DRV: async-pkt-wait\n");
			
			ar9170_pkt_buf_t *pending_tx_pkt =
				ar9170_drv_alloc_pending_status_pkt(tx_info, AR9170_PKT_QUEUED);
			if (pending_tx_pkt == NULL) {
				PRINTF("AR9170_DRV: push-pending-tx-err\n");
				return WIFI_TX_ERR_FULL;
			}
			result = ar9170_drv_queue_packet(0);
			if (result != WIFI_TX_OK) {
				printf("AR9170_DRV: USB ASYNC Queue ERR (%u)\n",result);
				/* Deallocate pending packet as it was not sent */
				memb_free(&ar9170_tx_pending_buf_mem, pending_tx_pkt);
				return result;
			} else {
				/* Add to pending list */
				list_add(ar9170_tx_pending_list, pending_tx_pkt);
			}			
			return WIFI_TX_OK;
		}
	}		
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_queue_cmd(void)
{		
	/* ! Caller must have secured the cmd_wait flag ! */
	uint8_t result;
	/* Verify the content of the command buffer. */
	result = ar9170_drv_verify_cmd();
	if (result) {
		return AR9170_DRV_ERR_INV_ARG;
	}		
	/* Allocate space for new buffered command  */
	ar9170_cmd_buf_t *new_cmd = (ar9170_cmd_buf_t*)memb_alloc(&ar9170_cmd_buf_mem);
			
	if (new_cmd == NULL) {
		printf("AR9170_DRV: queue-cmd-mem-alloc-err\n");
		AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
		process_poll(&ar9170_driver_proc);
		return AR9170_DRV_MEM_ERR;
	}
	
	/* Make a copy of the command to be queued */
	new_cmd->next = NULL;
	new_cmd->cmd_len = ar9170_dev.cmd_buffer.cmd_len;
	memcpy(new_cmd->cmd_data, ar9170_dev.cmd_buffer.cmd_data, 
		ar9170_dev.cmd_buffer.cmd_len);
	
	/* Queue the command */
	list_add(ar9170_dev.ar9170_cmd_list,new_cmd);
	AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
	process_poll(&ar9170_driver_proc);
	return AR9170_DRV_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_send_cmd_in_irq(void)
{
	/* Caller must have secured the cmd_wait flag */
	PRINTF("AR9170_DRV: send cmd in IRQ\n");
	uint8_t result;
	/* Verify the content of the command buffer. */
	result = ar9170_drv_verify_cmd();
	if (result) {
		return result;
	}
	
	/* We don't wait for a clear USB output. */
		
	usb_chunk_t usb_chunk;
	usb_chunk.buf = ar9170_dev.cmd_buffer.cmd_data;
	usb_chunk.len = ar9170_dev.cmd_buffer.cmd_len;
	/* Send the command down asynchronously to the USB driver */
	if (USB_STACK.send_async_int(&usb_chunk) != USB_DRV_TX_OK) {
		PRINTF("AR9170_DRV: USB TX ERROR\n");
		AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
		return AR9170_DRV_TX_ERR_USB;
	}
	AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
	process_poll(&ar9170_driver_proc);
	return AR9170_DRV_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_send_cmd(uint8_t is_synchronous)
{
	/* ! Caller MUST have secured the cmd_wait flag ! */
	uint8_t result;
	/* Verify the content of the command buffer */
	result = ar9170_drv_verify_cmd();
	if (result){
		return AR9170_DRV_ERR_INV_ARG;
	}	
	/* Ready to send */
	usb_chunk_t usb_chunk;
	usb_chunk.buf = ar9170_dev.cmd_buffer.cmd_data;
	usb_chunk.len = ar9170_dev.cmd_buffer.cmd_len;
			
	struct ar9170_cmd_head* cmd_header = 
		(struct ar9170_cmd_head*)ar9170_dev.cmd_buffer.cmd_data;
	
	/* Wait until EPs are free TODO remove */
	if(USB_STACK.is_int_busy() || USB_STACK.is_bulk_busy()) {
		printf("AR9170_DRV: eps-busy-cmd-wait %u\n",is_synchronous);
		while(USB_STACK.is_int_busy() || USB_STACK.is_bulk_busy());
	}
	/* Commands are ALWAYS transfered-down synchronously, i.e.
	 * we wait for the USB INT transfer-out to finalize so we
	 * can return. 
	 */
	result = USB_STACK.send_sync_int(&usb_chunk);
	if (unlikely(result != USB_DRV_TX_OK)) {
		printf("AR9170_DRV: USB TX CMD ERROR (%02x) on cmd (%02x)\n",
			result,cmd_header->cmd);
		/* If USB error, do not attempt again inside IRQ */
		ar9170_dev.cmd_only_thread = 1;	
		AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
		return AR9170_DRV_TX_ERR_USB;
	}
	ar9170_dev.cmd_only_thread = 0;
		
	if (is_synchronous) {
		/* Wait for device response. The interrupt handler
		 * will perform the required actions corresponding
		 * to this synchronous command. For RF_INIT, allow
		 * a higher waiting time.
		 */
		unsigned int timeout = AR9170_RSP_TIMEOUT_MS;
		if (unlikely(cmd_header->cmd == AR9170_CMD_RF_INIT))
			timeout = 100* AR9170_RSP_TIMEOUT_MS;
		AR9170_WAIT_FOR_LOCK_CLEAR_TIMEOUT_MS(&ar9170_dev.cmd_wait, timeout);
		if (AR9170_IS_RSP_TIMEOUT) {
			PRINTF("AR9170_DRV: sync-cmd-rsp-timeout (%02x)\n",usb_chunk.buf[1]);
			AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
			return AR9170_DRV_TX_ERR_TIMEOUT;
		}
	} else {
		AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
	}	
	/* For synchronous commands it means that the response is now available. */
	return AR9170_DRV_TX_OK;
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_usb_rx(void)
{
	/* Response is stored in the incoming buffer. */
	ar9170_rx(&ar9170_dev, ar9170_rx_list, ((uint8_t*)USB_STACK.bulk_dataptr()), 
			USB_STACK.bulk_datalen());
	process_poll(&ar9170_driver_proc);
	
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_reset(void)
{
	/**
	 * Reset USB, Enumerate, Install etc.
	 * Reset Device, meaning, we need to upload the FW again
	 * Reset MAC init
	 * But keep the	queues and the state info
	 */
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_handle_connection_event(le16_t vendor_id, le16_t product_id, 
	uint8_t plug)
{
	PRINTF("AR9170_DRV: connect-event: VID: %02x PID: %02x RSL: %u\n",
		vendor_id, product_id, plug);
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_handle_vendor_change(le16_t vendor_id, le16_t product_id, 
	uint8_t plug)
{
	PRINTF("AR9170_DRV: vendor-change-event: VID: %02x PID: %02x RSL: %u\n",
		vendor_id, product_id, plug);
		
	if (plug) {				
		if (product_id == USB_ATHEROS_PID_MSC) {
			PRINTF("MSC connected, must eject.\n");
			/* Queue the eject command. */
			packetbuf_clear();
			packetbuf_copyfrom(scsi_stop_unit,31);
			packetbuf_set_datalen(31);
			ar9170_drv_queue_packet(1);
			/* Poll process so the packet will be transmitted */
			process_poll(&ar9170_driver_proc);
		
		} else if (product_id == USB_ATHEROS_PID_WLAN) {
		
			if (!IS_INITIALIZED(&ar9170_dev)) {
				PRINTF("AR9170_DRV: dev-connected\n");
				/* Set state to stopped - will trigger firmware upload */
				ar9170_set_state(&ar9170_dev, AR9170_STOPPED);
				/* Poll the driver process to start firmware upload. */
				process_poll(&ar9170_driver_proc);
	
			} else {
				PRINTF("AR9170_DRV: dev just connected but already initialized!\n");
			}
			
		} else {
			PRINTF("AR9170_DRV: unknown dev connected\n");
		} 
			
	} else {
		ar9170_set_state(&ar9170_dev, AR9170_UNKNOWN_STATE);
		PRINTF("AR9170_DRV: device-unplugged\n");
		/* Free add driver resources */
		ar9170_drv_dealloc();
	}
}
/*---------------------------------------------------------------------------*/
static int
ar9170_drv_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif, 
	uint32_t flag)
{
	ar9170_op_bss_info_changed(hw, vif, &vif->bss_conf, flag);
	
	return 0;
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_powersave(void)
{
	if(ar9170_dev.psm_mgr.state != AR9170_PSM_DATA) {
		printf("AR9170_DRV: wrong-state-at-ps-check\n");
		return;
	}
	ar9170_ps_update(&ar9170_dev);	
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_send_one_buffered_packet(void)
{	
	if (USB_STACK.is_bulk_busy() || USB_STACK.is_int_busy()) {
		process_poll(&ar9170_driver_proc);
		return;
	}
	ar9170_pkt_buf_t *buf_pkt = (ar9170_pkt_buf_t*)list_pop(ar9170_tx_list);
	
	if(unlikely(!buf_pkt)) {
		printf("AR9170_DRV: buf-pkt-null\n");
		return;
	}
	uint8_t result = 0;
	printf("AR9170_DRV: send-buf-pkt\n");
	usb_chunk_t buffered_usb_chunk;
	buffered_usb_chunk.buf = &buf_pkt->pkt_data[0];
	buffered_usb_chunk.len = buf_pkt->pkt_len;
	/* Find prepared status struct with the right cookie */
	struct _ar9170_tx_superframe *txc 
		 = (struct _ar9170_tx_superframe*)&buf_pkt->pkt_data[0];
	if(ar9170_drv_update_queued_pkt_status(txc->s.cookie) == 0) {
		printf("AR9170_DRV: queued-cookie-not-found\n");
		goto err_out;
	}
	/* Send the packet without any status response requirement */
	result = USB_STACK.send_sync_bulk(&buffered_usb_chunk);
	if(result == USB_DRV_TX_OK) {
		process_poll(&ar9170_driver_proc);
	} else  {
		printf("AR9170_DRV: tx-buf-pkt-error (%u)\n",result);
	}
err_out:	
	/* Free packet mem. Restore element */
	memb_free(&ar9170_tx_buf_mem, buf_pkt);
}
/*---------------------------------------------------------------------------*/
static void
ar9170_drv_pollhandler(void)
{	
	/* ----------------------- AR9170 driver scheduler ----------------------*/
	if (likely(IS_STARTED(&ar9170_dev))) {
		/* High priority commands */		
		while(ar9170_action_vector) {
			PRINTF("AR9170_offline_handling_of_actions\n");
			ar9170_drv_handle_actions_offline(&ar9170_dev);
		}
		
		/*--- Queued commands ---*/
		while (list_length(ar9170_cmd_list) > 0) {
			cpu_irq_enter_critical();
			if(ar9170_dev.cmd_wait) {
				/* Perhaps some IRQ context is sending a command? */
				process_poll(&ar9170_driver_proc);
				cpu_irq_leave_critical();
				break;
			}			
			AR9170_GET_LOCK(&ar9170_dev.cmd_wait);
			cpu_irq_leave_critical();
			/* Pop a command from the command buffer */
			ar9170_cmd_buf_t *buffered_cmd = list_pop(ar9170_cmd_list);
			if (buffered_cmd != NULL && buffered_cmd->cmd_data != NULL &&
					buffered_cmd->cmd_len != 0) {
				/* Copy to outgoing buffer */
				ar9170_dev.cmd_buffer.cmd_len = buffered_cmd->cmd_len;
				memcpy(ar9170_dev.cmd_buffer.cmd_data, buffered_cmd->cmd_data, 
					buffered_cmd->cmd_len);
				/* Free memory */
				memb_free(&ar9170_cmd_buf_mem, buffered_cmd);
				/* Send asynchronous command */
				if (ar9170_drv_send_cmd(0) != AR9170_DRV_TX_OK) {
					PRINTF("AR9170_DRV: error send-asynch-cmd\n");
					process_poll(&ar9170_driver_proc);
					return;
				}
			} else {
				printf("AR9170_DRV: cmd-handler error; empty cmd\n");
				AR9170_RELEASE_LOCK(&ar9170_dev.cmd_wait);
			}
		}
		/*--- Queued TX packets ---*/
		if (list_length(ar9170_tx_list) > 0) {
			/* RDC is responsible for the content of the queue */
			if((ar9170_dev.psm_mgr.state == AR9170_PSM_DATA && 
				ar9170_dev.ps.state == 0)|| 
				ar9170_dev.psm_mgr.state == AR9170_PSM_ATIM) {
				/* Transmit a packet only in ATIM/DTX Window */
				ar9170_drv_send_one_buffered_packet();
				/* TODO do not send if very close to the end of the ATIM Window */				
			}	
		}
		
		/*  --- Queued RX packets --- */
		if (list_length(ar9170_rx_list) > 0 &&
			ar9170_dev.psm_mgr.state != AR9170_PSM_TBTT) {
			/* Pop a packet from the input buffer - ATOMIC! */
			PRINTF("AR9170_DRV: rx-poll [%u]\n",list_length(ar9170_rx_list));
			cpu_irq_enter_critical();
			ar9170_pkt_buf_t *buffered_pkt = list_pop(ar9170_rx_list);
			cpu_irq_leave_critical();
			if (buffered_pkt != NULL && buffered_pkt->pkt_len != 0 
				&& buffered_pkt->pkt_data != NULL ) {
				PRINTF("AR9170_DRV: pkt,mem-len [%u]\n",buffered_pkt->pkt_len);					
				/* Clear the packet buffer in case of existing trash. */
				packetbuf_clear();
				/* Copy packet content to Contiki packet buffer. */
				if(packetbuf_copyfrom(buffered_pkt->pkt_data,
					(uint16_t)buffered_pkt->pkt_len) != buffered_pkt->pkt_len) {
						PRINTF("AR9170_DRV: packetbuf-copy-error\n");
						goto err_copy;
				}
				packetbuf_set_datalen((uint16_t)buffered_pkt->pkt_len);
				/* DEBUG */
				struct ieee80211_mgmt *mgmt = packetbuf_dataptr();
				if (ieee80211_is_beacon(mgmt->frame_control)) {
					printf("Local :%llu\n", ar9170_op_get_tsf(ar9170_dev.hw, ar9170_dev.vif));
					printf("Remote:%llu\n",mgmt->u.beacon.timestamp);
				}
				
				/* Send the packet to the network stack. */
				NETSTACK_1_RDC.input();				
		err_copy:					
				cpu_irq_enter_critical();	
				memb_free(&ar9170_rx_buf_mem, buffered_pkt);	
				cpu_irq_leave_critical();
			} else {
				printf("AR9170_DRV: rx-handler error; empty packet\n");
			}
			
			if(list_length(ar9170_rx_list) != 0) {
				/* Poll the process for processing next packet */
				PRINTF("AR9170_DRV: list contains more: [%u]\n",list_length(ar9170_rx_list));
				process_poll(&ar9170_driver_proc);			
			}
		}		
		return;
	}

	/* Check if the device is connected. Could be that MSC eject is needed. */
	if (unlikely(!IS_INITIALIZED(&ar9170_dev))) {
		ar9170_drv_send_eject_cmd();
		return;
	}	
	/* Check whether the firmware upload procedure is due. */
	if (unlikely(!IS_ACCEPTING_CMD(&ar9170_dev))) {
		printf("AR9170_DRV: set-up IF is due\n");
		/* Initialize netstack_1 here FIXME */
		netstack_1_init();
		ar9170_set_up_interface(&ar9170_dev, &ar9170_firmware);
		/* Store MAC address into Contiki NETSTACK_1 */
		linkaddr1_copy(&linkaddr1_node_addr, (linkaddr1_t*)&ar9170_dev.vif->addr[0]);
		process_poll(&ar9170_driver_proc);
		return;
	}
	/* Check whether the initialization procedure is due. */
	if (unlikely(!IS_STARTED(&ar9170_dev))) {
		PRINTF("AR9170_DRV: dev not-started\n");
		if (ar9170_drv_start_network_interface(&ar9170_dev) == 0) {
			/* Enable beacon scanning */
			ar9170_drv_enable_scan();
			/* Can now start IBSS setup process */
			process_start(&ieee80211_ibss_setup_process, NULL);
			process_poll(&ar9170_driver_proc);
		}
		else {
			PRINTF("AR9170_DRV: start-iface error\n");
		}
		return;
	}
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ar9170_driver_proc, ev, data)
{
  UNUSED(data);

  /* Register poll-handler */
  PROCESS_POLLHANDLER(ar9170_drv_pollhandler());	

  PROCESS_BEGIN();		

  PRINTF("AR9170_driver process\n");
	
  while(1) {
    PROCESS_PAUSE();
    if (unlikely(ev == PROCESS_EVENT_EXIT)) {
      break;
    }
  }

  PRINTF("AR9170_DRV: exiting\n"); 
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
const struct wifi_driver ar9170_driver = {
	"ar9170-driver",
	ar9170_drv_init,
	ar9170_drv_handle_connection_event,
	ar9170_drv_handle_vendor_change,
	ar9170_drv_send_packet,
	ar9170_drv_queue_cmd,
	ar9170_drv_send_cmd,
	ar9170_drv_usb_rx,
	ar9170_drv_reset,
	ar9170_drv_join,
	ar9170_drv_rx_filter,
	ar9170_drv_bss_info_changed,
	ar9170_drv_config,
	ar9170_drv_tx_config,
	ar9170_drv_rx_config,
	ar9170_drv_powersave,
};
#endif /* WITH_WIFI_SUPPORT */
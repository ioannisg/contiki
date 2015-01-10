/*
 * ieee80211_rdc.c
 *
 * Created: 2/9/2014 3:05:45 PM
 *  Author: Ioannis Glaropoulos
 */ 

#include "ieee80211_rdc.h"
#include "mac-sequence.h"
#include "net/packetbuf.h"
#include "net/queuebuf.h"
#include "net/netstack_x.h"
#include "net/linkaddrx.h"
#include "wifistack.h"
#include "ieee80211.h"
#include "ieee80211_ibss.h"
#include "rtimer.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


#include "contiki-conf.h"
#if WITH_WIFI_SUPPORT


#ifdef IEEE80211_RDC_CONF_ADDRESS_FILTER
#define IEEE80211_RDC_ADDRESS_FILTER IEEE80211_RDC_CONF_ADDRESS_FILTER
#else
#define IEEE80211_RDC_ADDRESS_FILTER 1
#endif /* IEEE80211_RDC_CONF_ADDRESS_FILTER */

#ifndef IEEE80211_RDC_AUTOACK
#ifdef IEEE80211_RDC_CONF_AUTOACK
#define IEEE80211_RDC_AUTOACK IEEE80211_RDC_CONF_AUTOACK
#else
#define IEEE80211_RDC_AUTOACK 0
#endif /* IEEE80211_RDC_CONF_AUTOACK */
#endif /* IEEE80211_RDC_AUTOACK */

#ifndef IEEE80211_RDC_AUTOACK_HW
#ifdef IEEE80211_RDC_CONF_AUTOACK_HW
#define IEEE80211_RDC_AUTOACK_HW IEEE80211_RDC_CONF_AUTOACK_HW
#else
#define IEEE80211_RDC_AUTOACK_HW 0
#endif /* IEEE80211_RDC_CONF_AUTOACK_HW */
#endif /* IEEE80211_RDC_AUTOACK_HW */


#ifdef IEEE80211_RDC_CONF_STATUS_RSP_WAIT_TIME
#define IEEE80211_RDC_STATUS_RSP_WAIT_TIME	IEEE80211_RDC_CONF_STATUS_RSP_WAIT_TIME
#else /* IEEE80211_RDC_CONF_STATUS_RSP_WAIT_TIME */
#define IEEE80211_RDC_STATUS_RSP_WAIT_TIME	(RTIMER_SECOND/25)
#endif /* IEEE80211_RDC_CONF_STATUS_RSP_WAIT_TIME */

#ifdef IEEE80211_RDC_CONF_MAX_PEND_MGMT
#define IEEE80211_RDC_MAX_PEND_MGMT	IEEE80211_RDC_CONF_MAX_PEND_MGMT
#else	/* IEEE80211_RDC_CONF_MAX_PEND_MGMT */
#define IEEE80211_RDC_MAX_PEND_MGMT	4
#endif /* IEEE80211_RDC_CONF_MAX_PEND_MGMT */
struct ieee80211_rdc_pending_pkt {
	le16_t dsn;
	mac_callback_t mac_callback;
	void *ptr;
};
/* Pending status info */
static struct ieee80211_rdc_pending_pkt ieee80211_rdc_pending_tx;
static struct ieee80211_rdc_pending_pkt 
	ieee80211_rdc_pending_mgmt[IEEE80211_RDC_MAX_PEND_MGMT];
/* RDC real timers */
static struct rtimer ieee80211_rdc_rsp_wait_rtimer;
static struct rtimer 
	ieee80211_rdc_rsp_wait_mgmt_rtimer[IEEE80211_RDC_MAX_PEND_MGMT];
	
static rtimer_clock_t last_timeout;	
/*---------------------------------------------------------------------------*/
void
ieee80211_rdc_status_rsp_handler(unsigned int success,
	unsigned int rate, unsigned int transmissions, uint8_t cookie)
{
	/* Determine the actual pending packet based on the cookie info */	
	int i, ret;
	struct ieee80211_rdc_pending_pkt *pending_info;
	
	if (cookie == ieee80211_rdc_pending_tx.dsn) {	
		
		pending_info = &ieee80211_rdc_pending_tx;
		/* Cancel waiting timeout */
		rtimer_stop(&ieee80211_rdc_rsp_wait_rtimer);
		goto rsp_cont;	
	}
	
	for (i=0; i<IEEE80211_RDC_MAX_PEND_MGMT; i++) {
		if (cookie == ieee80211_rdc_pending_mgmt[i].dsn) {	
					
			pending_info = &ieee80211_rdc_pending_mgmt[i];
			/* Cancel waiting timeout */
			rtimer_stop(&ieee80211_rdc_rsp_wait_mgmt_rtimer[i]);
			goto rsp_cont;
		}
	}
	/* Cookie not found */
	printf("IEEE80211_RDC: wrong-cookie/late-rsp (%u) (%llu)\n",cookie,  
		((RTIMER_NOW()-last_timeout)*1000/RTIMER_SECOND));
	ret = MAC_TX_ERR_FATAL;
	goto rsp_error;
	
rsp_cont:
	if (success) {
		ret = MAC_TX_OK;
	} else {
		ret = MAC_TX_ERR;
	}	
	/* Set packet buffer attribute so packet is located at MAC */
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_TX_RSP_SEQNO,cookie);
	/* Inform MAC */
	mac_call_sent_callback(pending_info->mac_callback,
		pending_info->ptr,
		ret,
		transmissions);	
	/* Clear pending status */
	pending_info->dsn = 0;
	pending_info->mac_callback = NULL;
	pending_info->ptr = NULL;
	
	return;

rsp_error:
	/* No call-back for wrong/late cookie */
	return;	
}
/*---------------------------------------------------------------------------*/
static void 
ieee80211_rdc_tx_rsp_timeout(struct rtimer *rt, 
	struct ieee80211_rdc_pending_pkt *ptr)
{	
	if (ptr->dsn == 0 && ptr->mac_callback == NULL) {
		/* We lost race to actual status response interrupt */
		return;
	}
	PRINTF("IEEE80211_RDC: rsp-timeout\n");
	last_timeout = RTIMER_NOW(); //DEBUG only
	int ret = MAC_TX_NOACK;
	/* Set packet buffer attribute so MAC can find the packet */
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_TX_RSP_SEQNO, ptr->dsn);
	/* Maximum retries are reported */
	mac_call_sent_callback(ptr->mac_callback, ptr->ptr, ret, 6);
	
	/* Clear pending status */
	ptr->dsn = 0;
	ptr->mac_callback = NULL;
	ptr->ptr = NULL;	
	
	/* Perhaps there are more packets to be sent. The upper layer will
	 * be informed by the callback function and request again from the
	 * RDC to transmit the list of packets for this neighbor. The same
	 * applies as well to the in-time status response arrival.
	 */
}
/*---------------------------------------------------------------------------*/
static int
send_one_packet(mac_callback_t sent, void *ptr)
{
	PRINTF("IEEE80211_RDC: send-1-pkt\n");
	int ret;
	int i;
	int last_sent_ok = 0;
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, 
		(linkaddr_t*)ieee80211_ibss_vif.bss_conf.bssid);
	
	if(NETSTACK_1_FRAMER.create() < 0) {
		/* Failed to allocate space for headers */
		PRINTF("ieee80211_rdc: send failed, too large header\n");
		ret = MAC_TX_ERR_FATAL;
	} else {

#ifdef NETSTACK_1_ENCRYPT
	NETSTACK_1_ENCRYPT();
#endif /* NETSTACK_1_ENCRYPT */

		int is_mgmt;
		le16_t dsn;
		dsn = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO) & 0xff;
		is_mgmt = 
			(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE)==PACKETBUF_ATTR_PACKET_TYPE_ATIM) ||
			(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE)==PACKETBUF_ATTR_PACKET_TYPE_BCN);		
		/* Management packets are NOT synchronous
		 * Should be sent without waiting for the
		 * status responses of the previous ones.
		 */
		if (is_mgmt) {	
			int pos;				
			/* Search for storing a pending status for MGMT */	
			for (i=0; i<IEEE80211_RDC_MAX_PEND_MGMT; i++) {
				if(ieee80211_rdc_pending_mgmt[i].dsn == 0) {
					/* found */
					ieee80211_rdc_pending_mgmt[i].dsn = dsn;
					ieee80211_rdc_pending_mgmt[i].ptr = ptr;
					ieee80211_rdc_pending_mgmt[i].mac_callback = sent;
					pos = i;
					goto mgmt_send;
				}
			}
			/* TODO What if we can not store the pending status? */
			printf("IEEE80211_RDC: pending-mgmt-full\n");
			ret = MAC_TX_ERR_FATAL;
			goto tx_end;
		mgmt_send:	
			switch(WIFI_STACK.tx_packet(WIFI_TX_ASYNC)) {
			case WIFI_TX_OK:
				cpu_irq_enter_critical();
				/* Timeout for management frame status response */
				if (rtimer_set(&ieee80211_rdc_rsp_wait_mgmt_rtimer[pos],
					RTIMER_NOW() + IEEE80211_RDC_STATUS_RSP_WAIT_TIME,
					NULL,
					(void (*)(struct rtimer *, struct ieee80211_rdc_pending_pkt *))ieee80211_rdc_tx_rsp_timeout,
					&ieee80211_rdc_pending_mgmt[pos])) {
					printf("IEEE80211_RDC: err-rtimer-set\n");
					/* FIXME strictly speaking: the packet was actually sent down!*/
					ret = MAC_TX_ERR_FATAL;
				} else {
					/* It does not mean that the packet was received correctly! */
					ret = MAC_TX_OK;
				}
				cpu_irq_leave_critical();
				break;
			case WIFI_TX_ERR_RADIO:
				printf("IEEE80211_RDC: wifi-pkt-radio-err\n");
				ret = MAC_TX_ERR;
				break;
			case WIFI_TX_ERR_FATAL:
			case WIFI_TX_ERR_TIMEOUT:
			case WIFI_TX_ERR_USB:
			case WIFI_TX_ERR_FULL:
				ret = MAC_TX_ERR_FATAL;
				PRINTF("IEEE80211_RDC: wifi-tx-err\n");
				break;
			default:
				printf("IEEE80211_RDC: unrecognized-status\n");
				ret = MAC_TX_ERR_FATAL;
				break;			
			}	
			if (ret != MAC_TX_OK) {
				/* Remove pending status since packet was not sent down */
				ieee80211_rdc_pending_mgmt[pos].dsn = 0;
				ieee80211_rdc_pending_mgmt[pos].mac_callback = NULL;
				ieee80211_rdc_pending_mgmt[pos].ptr = NULL;
			}		
			
		} else {
			/* DATA frame, broadcast or unicast */
			
			/* Defer, if a packet is pending */
			if (ieee80211_rdc_pending_tx.dsn != 0) {
				PRINTF("IEEE80211_RDC: pending-tx\n");
				ret = MAC_TX_DEFERRED;
				
			} else {				
				/* Store pending TX status */
				ieee80211_rdc_pending_tx.dsn = dsn;
				ieee80211_rdc_pending_tx.mac_callback = sent;
				ieee80211_rdc_pending_tx.ptr = ptr;
				/* It is synchronous and is attempted immediately, or error is thrown */
				switch(WIFI_STACK.tx_packet(WIFI_TX_SYNC)) {
				case WIFI_TX_OK:
					cpu_irq_enter_critical();					
					/* Set status-rsp timeout */
					if (rtimer_set(&ieee80211_rdc_rsp_wait_rtimer,
						RTIMER_NOW()+IEEE80211_RDC_STATUS_RSP_WAIT_TIME,
						NULL,
						(void (*)(struct rtimer *, void *))ieee80211_rdc_tx_rsp_timeout,
						&ieee80211_rdc_pending_tx)) {
							printf("IEEE80211_RDC: err-rtimer-set\n");
							cpu_irq_leave_critical();
							ret = MAC_TX_ERR_FATAL;
					} else {
							/* It does not mean that the packet was received correctly! */
							cpu_irq_leave_critical();
							ret = MAC_TX_OK;
						}
					break;
				case WIFI_TX_ERR_RADIO:
					printf("IEEE80211_RDC: wifi-tx-err-radio\n");
					ret = MAC_TX_ERR;
					break;
				case WIFI_TX_ERR_FATAL:
				case WIFI_TX_ERR_TIMEOUT:
				case WIFI_TX_ERR_USB:
				case WIFI_TX_ERR_FULL:
					ret = MAC_TX_ERR_FATAL;
					printf("IEEE80211_RDC: wifi-tx-err\n");
					break;	
				default:
					printf("IEEE80211_RDC: unrecognized-status\n");
					ret = MAC_TX_ERR_FATAL;
					break;	
				}					
			}			
		}			
	}
tx_end:	
	if(ret == MAC_TX_OK) {
		last_sent_ok = 1;
		/* Status will inform upper layer  */
	} else if(ret == MAC_TX_DEFERRED) {
		/* We did not sent so inform send list to stop */
	} else {
		/* Packet was not sent down so we inform the mac now */
		mac_call_sent_callback(sent, ptr, ret, 0);
	}	
	return last_sent_ok;
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{	
	return 0;
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
	WIFI_STACK.powersave();
	return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
	int i;
	on();
	
	ieee80211_rdc_pending_tx.dsn = 0;
	ieee80211_rdc_pending_tx.mac_callback = NULL;
	ieee80211_rdc_pending_tx.ptr = NULL;
	
	for (i=0; i<IEEE80211_RDC_MAX_PEND_MGMT; i++) {
		ieee80211_rdc_pending_mgmt[i].dsn = 0;
		ieee80211_rdc_pending_mgmt[i].mac_callback = NULL;
		ieee80211_rdc_pending_mgmt[i].ptr = NULL;
	}
}
/*---------------------------------------------------------------------------*/
static void
packet_input(void)
{
	int original_datalen;
	uint8_t *original_dataptr;

	original_datalen = packetbuf_datalen();
	original_dataptr = packetbuf_dataptr();

	if(NETSTACK_1_FRAMER.parse() < 0) {
		PRINTF("ieee80211_rdc: failed to parse %u\n", original_datalen);
#if IEEE80211_RDC_ADDRESS_FILTER
	} else if(!linkaddr1_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		&linkaddr1_node_addr) &&
		!linkaddr1_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER),
		&linkaddr1_null)) {
		PRINTF("ieee80211_rdc: not for us\n");
#endif /* IEEE80211_RDC_ADDRESS_FILTER */
	} else {
		int duplicate = 0;
		if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) !=
			PACKETBUF_ATTR_PACKET_TYPE_BCN) {
			duplicate = mac_sequence_is_duplicate();	
		}		
		if(duplicate) {
			/* Drop the packet. */		
			PRINTF("ieee80211_rdc: drop duplicate link layer packet %u\n",
			packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));
		} else {
			mac_sequence_register_seqno();
		}
		#if DEBUG
		int i;
		PRINTF("PKT[%u]:",original_datalen);
		for (i=0; i<original_datalen; i++)
		PRINTF("%02x ",original_dataptr[i]);
		#endif
		
		if(!duplicate) {
			/* Remove the IEEE80211 FCS */
			packetbuf_set_datalen(packetbuf_datalen()- FCS_LEN);
			NETSTACK_1_MAC.input();
		}
	}	
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void* ptr)
{
	send_one_packet(sent, ptr);
}
/*---------------------------------------------------------------------------*/
static void
send_list(mac_callback_t sent, void *ptr, struct rdc_buf_list *buf_list)
{
	/* ptr is the neighbor queue */
	while(buf_list != NULL) {
		/* We backup the next pointer, as it may be nullified by
		 * mac_call_sent_callback() */
		struct rdc_buf_list *next = buf_list->next;
		int last_sent_ok;

		queuebuf_to_packetbuf(buf_list->buf);
		last_sent_ok = send_one_packet(sent, ptr);

		/* If packet transmission was not successful, we should back off and let
		 * upper layers retransmit, rather than potentially sending out-of-order
		 * packet fragments. */
		if(!last_sent_ok) {
		  return;
		}
		buf_list = next;
  }
}
/*---------------------------------------------------------------------------*/
const struct rdc_driver ieee80211_rdc_driver = {
	
	"ieee80211_rdc_driver",
	init,
	send_packet,
	send_list,
	packet_input,
	on,
	off,
	NULL,
};
#endif /* WITH_WIFI_SUPPORT */
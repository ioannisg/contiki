/*
 * ieee80211_mac.c
 *
 * Created: 2/14/2014 11:32:30 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ieee80211_ibss.h"
#include "rimeaddr.h"
#include "ctimer.h"
#include "lib/list.h"
#include "memb.h"
#include "queuebuf.h"
#include "netstack_x.h"
#include "random.h"
#include "wire_digital.h"
#include "ar9170.h" // only for debug because of pin debug

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else /* DEBUG */
#define PRINTF(...)
#endif /* DEBUG */


#include "contiki-conf.h"
#if WITH_WIFI_SUPPORT


/* The mac layer needs to know the driver state */
enum ieee80211_mac_psm_state {
	IEEE80211_MAC_PSM_TBTT,
	IEEE80211_MAC_PSM_ATIM,
	IEEE80211_MAC_PSM_DATA,
	IEEE80211_MAC_PSM_DOZE
};
static enum ieee80211_mac_psm_state psm_state;

/* Packet metadata */
struct qbuf_metadata {
	mac_callback_t sent;
	void *cptr;
	uint8_t max_transmissions;
};

/* Every neighbor has its own packet queue */
struct neighbor_queue {
	struct neighbor_queue *next;
	rimeaddr_t addr;
	struct ctimer transmit_timer;
	uint8_t transmissions;
	uint8_t collisions, deferrals;
	uint8_t is_awake;
	uint8_t atim_set;
	LIST_STRUCT(queued_packet_list);
};

/* The maximum number of co-existing neighbor queues */
#ifdef IEEE80211_MAC_CONF_MAX_NEIGHBOR_QUEUES
#define IEEE80211_MAC_MAX_NEIGHBOR_QUEUES IEEE80211_MAC_CONF_MAX_NEIGHBOR_QUEUES
#else
#define IEEE80211_MAC_MAX_NEIGHBOR_QUEUES 2
#endif /* IEEE80211_MAC_CONF_MAX_NEIGHBOR_QUEUES */

#define MAX_QUEUED_PACKETS QUEUEBUF_NUM
MEMB(neighbor_memb, struct neighbor_queue, IEEE80211_MAC_MAX_NEIGHBOR_QUEUES);
MEMB(packet_memb, struct rdc_buf_list, MAX_QUEUED_PACKETS);
MEMB(metadata_memb, struct qbuf_metadata, MAX_QUEUED_PACKETS);
LIST(neighbor_list);

static void packet_sent(void *ptr, int status, int num_transmissions);
static void transmit_packet_list(void *ptr);
static void free_packet(struct neighbor_queue *n, struct rdc_buf_list *p);
static void send_packet(mac_callback_t sent, void *ptr);


static uint8_t ieee80211_mac_can_sleep_flag;

static void 
transmit_packet_lists( void ) 
{
	PRINTF("IEEE80211_mac: tx-pkt-lists\n");
	struct neighbor_queue *n = list_head(neighbor_list);
	while(n != NULL) {
		transmit_packet_list(n);
		n = list_item_next(n);
	}
}
/*---------------------------------------------------------------------------*/
static void 
ieee80211_mac_add_wake_neighbor(const rimeaddr_t *addr) 
{
	struct neighbor_queue *n = list_head(neighbor_list);
	
	while(n != NULL) {
		
		if (rimeaddr_cmp(addr, &n->addr)) {
			if (n->is_awake)
				PRINTF("IEEE80211_MAC: neighbor-already-awake\n");
			n->is_awake = 1;
			/* The above cancels a pending ATIM transmission */
			break;
		}
		n = list_item_next(n);		
	}	
}
/*---------------------------------------------------------------------------*/
static void
atim_sent(void *ptr, int status, int txs)
{
	struct neighbor_queue *n = ptr;
	
	if (unlikely(psm_state != IEEE80211_MAC_PSM_ATIM))
		PRINTF("IEEE80211_MAC: atim-status-late (%u)\n",psm_state);
	if(status == 0)
		ieee80211_mac_add_wake_neighbor(&n->addr);
	else
		PRINTF("IEEE80211_mac: ATIM NACK (%u) for (%02x)\n",status,n->addr.u8[0]);		
}
/*---------------------------------------------------------------------------*/
static void 
transmit_atim_list(void) 
{	
	/* Walk through all neighbors and send the ATIM, if necessary */
	struct neighbor_queue *n = list_head(neighbor_list);
	while(n != NULL) {
		if ((!n->is_awake) && (n->atim_set)) {
			/* Means, the first packet is the ATIM */
			struct rdc_buf_list *q = list_head(n->queued_packet_list);
			if(q != NULL) {
				PRINTF("ieee80211mac: preparing atim number %d %p, queue len %d\n", n->transmissions, q,
					list_length(n->queued_packet_list));
				/* Place ATIM in the packet buffer */
				queuebuf_to_packetbuf(q->buf);	
				/* Send Atim in the neighbor's list */
				NETSTACK_1_RDC.send(packet_sent,n);
				/* 
				 * Register ATIM as attempted (atim_set = 0), so clear_atims 
				 * will not free it; it will be freed by the status handler.
				 */				
				if (!n->atim_set)
					printf("IEEE80211_MAC: err-atim-set-already-zero\n");
				n->atim_set = 0;				
			}
		}
		n = list_item_next(n);
	}
}
/*---------------------------------------------------------------------------*/
static void
ieee80211_mac_clear_atims(void)
{	
	struct neighbor_queue *n = list_head(neighbor_list);
	while(n != NULL) {
		struct rdc_buf_list *q = list_head(n->queued_packet_list);
		if (q != NULL) {
			if (n->atim_set) {
				PRINTF("IEEE80211_MAC: clear-a-not-attempted-atim\n");
				free_packet(n,q);
				n->atim_set = 0;
			}
		}
		n = list_item_next(n);
	}
}
/*---------------------------------------------------------------------------*/
static void
ieee80211_mac_clear_wake_neigbors(void)
{
	struct neighbor_queue *n = list_head(neighbor_list);
	while(n != NULL) {
		if (n->is_awake)
			PRINTF("IEEE80211_mac: clear-neighbor %02x\n",n->addr.u8[0]);
		n->is_awake = 0;
		n = list_item_next(n);
	}
}
/*---------------------------------------------------------------------------*/
static uint8_t
ieee80211_mac_generate_atims(void)
{	
	struct neighbor_queue *n = list_head(neighbor_list);
	uint8_t atim_gen = 0;
	
	while(n != NULL) {
		if (list_length(n->queued_packet_list) > 0) {
			/* Neighbor queue non-empty */
			if(n->is_awake == 0) {
				/* Must generate ATIM */
				PRINTF("IEEE80211_MAC: gen-atim for neighbor\n");
				packetbuf_clear();
				packetbuf_clear_hdr();
				packetbuf_attr_clear();
				packetbuf_set_datalen(0);
				packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &n->addr);
				packetbuf_set_addr(PACKETBUF_ADDR_SENDER,
					&ieee80211_ibss_vif.addr);
				packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, 
					&ieee80211_ibss_vif.bss_conf.bssid);
				packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, 
					PACKETBUF_ATTR_PACKET_TYPE_ATIM);
				/* TODO in MH-PSM set the address3 field */
				n->atim_set = 1;
				/* Add to list */
				send_packet(atim_sent, n);
				atim_gen++;
			}
		}
		n = list_item_next(n);
	}
	return atim_gen;
}
/*---------------------------------------------------------------------------*/
static struct neighbor_queue *
neighbor_queue_from_addr(const rimeaddr_t *addr)
{
	struct neighbor_queue *n = list_head(neighbor_list);
	while(n != NULL) {
		if(rimeaddr_cmp(&n->addr, addr)) {
			return n;
		}
		n = list_item_next(n);
	}
	return NULL;
}
/*---------------------------------------------------------------------------*/
static void
transmit_packet_list(void *ptr)
{
	/* If in ATIM Window, send only ATIMs*/
	if (psm_state == IEEE80211_MAC_PSM_ATIM) {
		transmit_atim_list();
		return;
	}
	/* Send only on Data TX state if we are in PSM */
	if (psm_state != IEEE80211_MAC_PSM_DATA) {
		return;
	}
	struct neighbor_queue *n = ptr;
	if(n) {
		/* And only if the neighbor is awake */
		if (!n->is_awake) {
			PRINTF("IEEE80211_MAC: neighbor-not-awake\n");
			return;
		}
		struct rdc_buf_list *q = list_head(n->queued_packet_list);
		if(q != NULL) {
			PRINTF("ieee80211mac: preparing data number %d %p, queue len %d\n", n->transmissions, q,
			list_length(n->queued_packet_list));
			/* Send packets in the neighbor's list */
			NETSTACK_1_RDC.send_list(packet_sent, n, q);
		}
	}
}
/*---------------------------------------------------------------------------*/
static void
free_packet(struct neighbor_queue *n, struct rdc_buf_list *p)
{
	if(p != NULL) {
		/* Remove packet from list and deallocate */
		list_remove(n->queued_packet_list, p);

		queuebuf_free(p->buf);
		memb_free(&metadata_memb, p->ptr);
		memb_free(&packet_memb, p);
		PRINTF("ieee80211_mac: free_queued_packet, queue length %d\n",
			list_length(n->queued_packet_list));
		if(list_head(n->queued_packet_list) != NULL) {
			/* There is a next packet. We reset current tx information */
			n->transmissions = 0;
			n->collisions = 0;
			n->deferrals = 0;
			/* Set a timer for next transmissions */
			ctimer_set(&n->transmit_timer, (CLOCK_SECOND/1000),
				transmit_packet_list, n);
			} else {
				/* This was the last packet in the queue, we free the neighbor */
				PRINTF("IEEE80211_mac: free-neighbor\n");
				ctimer_stop(&n->transmit_timer);
				list_remove(neighbor_list, n);
				memb_free(&neighbor_memb, n);
		}
	}
}
/*---------------------------------------------------------------------------*/
static void
packet_sent(void *ptr, int status, int num_transmissions)
{
	struct neighbor_queue *n;
	struct rdc_buf_list *q;
	struct qbuf_metadata *metadata;
	
	mac_callback_t sent;
	void *cptr;
	int num_tx;
	int backoff_exponent;
	int backoff_transmissions;
	PRINTF("IEEE80211_MAC: pkt-sent %u %u \n", status, num_transmissions);
	n = ptr;
	if(n == NULL) {
		printf("IEEE80211_MAC: err-null-neighbor-queue\n");
		return;
	}
	switch(status) {
	case MAC_TX_OK:
		n->transmissions += num_transmissions;
		break;
	case MAC_TX_ERR:
		n->collisions += num_transmissions;	
		break;
	case MAC_TX_NOACK:
		n->transmissions += num_transmissions;
		break;
	case MAC_TX_COLLISION:
		n->collisions += num_transmissions;
		break;
	case MAC_TX_DEFERRED:
		/* \Note that deferral here means that we just wait for
		 * the previous packet to finish, so it is not a source
		 * of error! However, we do not expect a function call,
		 * in case of deferral!
		 */
		printf("IEEE80211_mac: deferred\n");
		n->deferrals++;
		break;
	}
	/* Locate the exact packet */
	for(q = list_head(n->queued_packet_list);
		q != NULL; q = list_item_next(q)) {
		if(queuebuf_attr(q->buf, PACKETBUF_ATTR_MAC_SEQNO) ==  
			packetbuf_attr(PACKETBUF_ATTR_MAC_TX_RSP_SEQNO)) { // XXX
			break;
		}
	}

	if(q != NULL) {
		metadata = (struct qbuf_metadata *)q->ptr;

		if(metadata != NULL) {
			sent = metadata->sent;
			cptr = metadata->cptr;
			num_tx = n->transmissions;
			if(status == MAC_TX_COLLISION ||
				status == MAC_TX_NOACK ||
				status == MAC_TX_ERR) {
				/* If the transmission was erroneous, because of 
				 * either a driver error, or a collision or even
				 * a never arrived ACK, we register it as a lost
				 * packet because all the retries occurred at the
				 * firmware. So we do not retransmit.
				 */			
				switch(status) {
				case MAC_TX_COLLISION:
				case MAC_TX_ERR:
					PRINTF("ieee80211mac: rexmit/tx collision %d\n", n->transmissions);
					break;
				case MAC_TX_NOACK:
					PRINTF("csma: rexmit noack %d\n", n->transmissions);
					break;
				default:
					PRINTF("csma: rexmit err %d, %d\n", status, n->transmissions);
				}
			} else {
				if(status == MAC_TX_OK) {
					PRINTF("ieee80211mac: rexmit ok %d\n", n->transmissions);
				} else {
					PRINTF("csma: rexmit failed %d: %d\n", n->transmissions, status);
				}
			}
			rimeaddr_t recv_tmp;		
			uint8_t restore_addr = 0;	
			if (unlikely(!rimeaddr_cmp(&n->addr,
				packetbuf_addr(PACKETBUF_ADDR_RECEIVER)))) {
				/* Temporary store the current address in the packet buffer */
				rimeaddr_copy(&recv_tmp,
					packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
				/* Update the receiver address in the packet buffer */
				packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &n->addr);
				restore_addr++;
			}			
			free_packet(n, q);
			mac_call_sent_callback(sent, cptr, status, num_tx);
			/* Restore packet buffer address if required */
			if (restore_addr)
				packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &recv_tmp);
		}
	} 
}
/*---------------------------------------------------------------------------*/
static void
send_packet(mac_callback_t sent, void *ptr)
{
	struct rdc_buf_list *q;
	struct neighbor_queue *n;
	static uint8_t initialized = 0;
	static uint16_t seqno;
	const rimeaddr_t *addr = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

	if(!initialized) {
		initialized = 1;
		/* Increment the sequence number */
		seqno = 1;
	}

	if(seqno == 0) {
		/* PACKETBUF_ATTR_MAC_SEQNO cannot be zero */
		seqno++;
	}
	PRINTF("IEEE80211_MAC: seq-no:%u\n",seqno);
	packetbuf_set_attr(PACKETBUF_ATTR_MAC_SEQNO, seqno++);
	
	/* Look for the neighbor entry */
	n = neighbor_queue_from_addr(addr);
	if(n == NULL) {
		/* Allocate a new neighbor entry */
		n = memb_alloc(&neighbor_memb);
		if(n != NULL) {
			/* Init neighbor entry */
			rimeaddr_copy(&n->addr, addr);
			n->transmissions = 0;
			n->collisions = 0;
			n->deferrals = 0;
			n->is_awake = 0;
			n->atim_set = 0;
			/* Init packet list for this neighbor */
			LIST_STRUCT_INIT(n, queued_packet_list);
			/* Add neighbor to the list */
			list_add(neighbor_list, n);
		}
	}

	if(n != NULL) {
		/* Add packet to the neighbor's queue */
		q = memb_alloc(&packet_memb);
		if(q != NULL) {
			q->ptr = memb_alloc(&metadata_memb);
			if(q->ptr != NULL) {
				q->buf = queuebuf_new_from_packetbuf();
				if(q->buf != NULL) {
					struct qbuf_metadata *metadata = (struct qbuf_metadata *)q->ptr;
					/* Neighbor and packet successfully allocated */
					if(packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS) == 0) {
						/* Use default configuration for max transmissions. For 
						 * 802.11 this is 1 because they are handled in the driver
						 */
						metadata->max_transmissions = 1;
					} else {
						metadata->max_transmissions =
							packetbuf_attr(PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS);
					}
					metadata->sent = sent;
					metadata->cptr = ptr;

					if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
						PACKETBUF_ATTR_PACKET_TYPE_ATIM) {
						list_push(n->queued_packet_list, q);
					} else if(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
						PACKETBUF_ATTR_PACKET_TYPE_DATA) {
						list_add(n->queued_packet_list, q);
					} else {
						PRINTF("IEEE80211_MAC: pkt-not-supported\n");
					}

					/* If q is the first packet in the neighbor's queue, send asap */
					if(list_head(n->queued_packet_list) == q) {
						ctimer_set(&n->transmit_timer, 0, transmit_packet_list, n);
					}
					return;
				}
				memb_free(&metadata_memb, q->ptr);
				printf("ieee80211mac: could not allocate queuebuf, dropping packet\n");
			}
			memb_free(&packet_memb, q);
			printf("ieee80211mac: could not allocate queuebuf, dropping packet\n");
		}
		/* The packet allocation failed. Remove and free neighbor entry if empty. */
		if(list_length(n->queued_packet_list) == 0) {
			list_remove(neighbor_list, n);
			memb_free(&neighbor_memb, n);
		}
		printf("ieee80211mac: could not allocate packet, dropping packet\n");
	} else {
		printf("ieee80211mac: could not allocate neighbor, dropping packet\n");
	}
	mac_call_sent_callback(sent, ptr, MAC_TX_ERR_FATAL, 1);
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
	/* on() signals TBTT, by convention */
	ieee80211_mac_clear_wake_neigbors();
		
	if(ibss_info.ibss_ps_mode == IBSS_NULL_PSM ||
			ieee80211_mac_generate_atims()) {
		ieee80211_mac_can_sleep_flag = 0;
		wire_digital_write(TX_ACTIVE_PIN, HIGH);
	}
	else {
		ieee80211_mac_can_sleep_flag = 1;
		wire_digital_write(TX_ACTIVE_PIN, LOW);
	}
				
	if (psm_state != IEEE80211_MAC_PSM_DATA && 
		psm_state != IEEE80211_MAC_PSM_DOZE) {
		printf("IEEE80211_MAC: wrong-state (%u)\n",psm_state);
		/* Signal error to AR9170? */
	}
	psm_state = IEEE80211_MAC_PSM_TBTT;
	return 0;
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
	if (keep_radio_on) {
		/* Data TX Window */
		if (psm_state != IEEE80211_MAC_PSM_ATIM) {
			printf("IEEE80211_MAC: wrong-psm-state-at-DTX (%u)",psm_state);
		}				
		ieee80211_mac_clear_atims();
		
		/* Transmit the pending packets, if any.
		 * If not check if we can move to sleep.
		 * Question: which neighbor has priority?
		 */
		if (ieee80211_mac_can_sleep_flag) {
			psm_state = IEEE80211_MAC_PSM_DOZE;
			NETSTACK_1_RDC.off(0);
		} else {
			psm_state = IEEE80211_MAC_PSM_DATA;
			transmit_packet_lists();
		}		
		
	} else {
		/* ATIM Window */
		if (psm_state != IEEE80211_MAC_PSM_TBTT) {
			printf("IEEE80211_MAC: wrong-psm-state-at-ATIM (%u)",psm_state);
		}
		psm_state = IEEE80211_MAC_PSM_ATIM;
		transmit_atim_list();
	}
	
	return 0;
}
/*---------------------------------------------------------------------------*/
static void
init(void)
{
	/* Configure 802.11 */
	__ieee80211_sta_init_config();
	
	memb_init(&packet_memb);
	memb_init(&metadata_memb);
	memb_init(&neighbor_memb);
	
	/* Initially in Data */
	psm_state = IEEE80211_MAC_PSM_DATA;
}
/*---------------------------------------------------------------------------*/
static void
input_packet(void)
{
	/* Trap ATIM packets here */
	if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) == 
		PACKETBUF_ATTR_PACKET_TYPE_ATIM) {
		PRINTF("IEEE80211_MAC: atim-recv\n");
		/* Add neighbor in awake list */
		ieee80211_mac_add_wake_neighbor(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
		return;	
	}
	/* Trap BCN packets here */
	if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
	PACKETBUF_ATTR_PACKET_TYPE_BCN) {
		PRINTF("IEEE80211_MAC: bcn-recv\n");
		/* Process beacon TODO */
		return;
	}
	if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) ==
		PACKETBUF_ATTR_PACKET_TYPE_DATA) {
		link_if_in(NETSTACK_80211);
		NETSTACK_1_NETWORK.input();
		link_if_in(NETSTACK_NULL);
		return;
	}
	printf("IEEE80211_MAC: unrecognized-pkt-type\n");
}
/*---------------------------------------------------------------------------*/
const struct mac_driver ieee80211_mac_driver = {
	"80211_MAC",
	init,
	send_packet,
	input_packet,
	on,
	off,
	NULL,
};
/*---------------------------------------------------------------------------*/
#endif /* WITH_WIFI_SUPPORT */
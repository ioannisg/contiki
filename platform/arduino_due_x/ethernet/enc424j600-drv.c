/*
 * enc424j600_drv.c
 *
 * Created: 2014-08-22 20:45:46
 *  Author: Ioannis Glaropoulos <ioannisg@sics.se>
 */ 
#include "contiki-net.h"
#include "contiki-lib.h"
#include "lib/ringmem.h" // TODO - add to contiki-lib.h

#include "netstack_x.h" //TODO - add to contiki-net.h
#include "enc424j600-drv.h"
#include "ethernet.h"

#include <stdlib.h>
#include <stdio.h>
#include "net/ipv4/uip_arp.h"
#include "watchdog.h"
#include "rtimer.h"
#include "platform-conf.h"
#include "clock.h"


#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#if WITH_ETHERNET_SUPPORT

static void enc424j600_drv_reset(void);

PROCESS(enc424j600_drv_proc, "ENC424J600_drv_proc");
/*---------------------------------------------------------------------------*/

#if 0
/* Declare the driver queues */
LIST(enc424j600_rx_list);
/* Declare the static memory for the rx queue */
MEMB(enc424j600_rx_buf_mem, enc424j600_pkt_buf_t, ENC424J600_DRV_RX_PKT_MAX_QUEUE_LEN);
#endif

/* Declare the MAC address array of the device */
static linkaddr0_t enc424j600_macaddr;

RINGMEM(enc424j600_ringmem, 2048);

static enc424j600_drv_status_t enc424j600_status;
/*---------------------------------------------------------------------------*/
static void
enc424j600_drv_irq_handle_rx(void)
{
  uint16_t pkt_len = enc424j600_get_recv_frame_len();
  
  if (pkt_len == 0) {
   PRINTF("enc424j600_drv: zero-rx\n");
	return;
  }
  
  if (pkt_len > ETH_424J600_DRV_RX_MAX_PKT_LEN) {
	  PRINTF("enc424j600_drv: too-large-rx %u\n", pkt_len);
	  return;
  }

  if (!ringmem_alloc(enc424j600_ringmem, enc424j600_get_recv_frame_ptr(), 
    pkt_len)) {
    printf("enc424j600_drv: ringmem-full\n");
	 return;
  }
#if 0
  /* Process received frame */
  eth_pkt_buf_t* new_pkt;
   
  new_pkt = memb_alloc(&enc424j600_rx_buf_mem);
  if (new_pkt == NULL) {
    PRINTF("enc424j600_drv_ mem-alloc-err\n");
    return;
  }
  new_pkt->next = NULL;
  new_pkt->pkt_len = pkt_len;
   
  /* Copy packet data */
  memcpy(&new_pkt->pkt_data[0], (uint8_t*)enc424j600_get_recv_frame_ptr(),
    pkt_len);
  PRINTF("enc424j600_drv: rx-copied\n");
   
  /* Add to list */
  if (unlikely(list_length(enc424j600_rx_list) >=
    ENC424J600_DRV_RX_PKT_MAX_QUEUE_LEN)) {
    printf("enc424j600_drv: rx-list-full\n");
    memb_free(&enc424j600_rx_buf_mem, new_pkt);
    return;
  } else {
    list_add(enc424j600_rx_list, new_pkt);
    PRINTF("enc424j600_drv: rx-pkt-added [%u]\n", (unsigned int)pkt_len);
  }
#endif
  process_poll(&enc424j600_drv_proc);
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_drv_irq_handle_link_updt(uint32_t link_up)
{
  if (unlikely(!enc424j600_status.link_initialized)) {
    PRINTF("enc424j600_drv: got link-up from un-initialized device\n");
	 return;
  }
  
  /* PHY Link status update */
  if (link_up == ENC424J600_IRQ_STATUS_LINK_UP) {
    if (unlikely(enc424j600_status.link_active)) {
      PRINTF("enc424j600_drv: link is already up\n");
    } else {
      PRINTF("enc424j600_drv: cable plugged\n");
      /* Change flag status, so process can initialize net_stack */
      enc424j600_status.link_active = 1;
		enc424j600_status.link_transition = 1;
		/* Display PHY info */
		if (enc424j600_get_speed()) {
		  PRINTF("enc424j600_drv: 100BASE-Tn");
		} else {
		  PRINTF("enc424j600_drv: 10BASE-Tn");
		}
      if (enc424j600_get_dpx_mode()) {
		  PRINTF(", Full-Duplex\n");
		} else {
		  PRINTF(", Half-Duplex\n");	  
		}
    }
  } else if (link_up == ENC424J600_IRQ_STATUS_LINK_DN) {
    if (unlikely(!enc424j600_status.link_active)) {
      PRINTF("enc424j600_drv: link is already down\n");
    } else {
      PRINTF("enc424j600_drv: cable unplugged\n");
      /* Change flag status, to DOWN */
      enc424j600_status.link_active = 0;
		enc424j600_status.link_transition = 1;
    }
  } else {
    PRINTF("unrecognized link update status\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_drv_irq_cback(uint32_t irq_status)
{
  switch (irq_status) {
  case ENC424J600_IRQ_STATUS_RX:
    enc424j600_drv_irq_handle_rx();
    break;
  case ENC424J600_IRQ_STATUS_RX_ABRT:
    break;
  case ENC424J600_IRQ_STATUS_LINK_UP:
  case ENC424J600_IRQ_STATUS_LINK_DN:
    enc424j600_drv_irq_handle_link_updt(irq_status);
    break;
  case ENC424J600_IRQ_STATUS_TX_COMP:
  case ENC424J600_IRQ_STATUS_TX_ABRT:
    if (likely(enc424j600_status.link_busy))
      enc424j600_status.link_busy = 0;
    else
      printf("enc424j600_drv: tx-complete-on-idle-link!\n");
    if (irq_status == ENC424J600_IRQ_STATUS_TX_ABRT) {
      printf("enc424j600_drv: tx-status-err\n");
#if ETHERNET_BLOCK_UNTIL_TX_COMP
		enc424j600_status.link_tx_err = 1;
#endif
	 }
    break;
  default:
		PRINTF("enc424j600_drv: unrecognized-irq-status\n");
	}
}
/*---------------------------------------------------------------------------*/
static int
enc424j600_drv_tx(void)
{
  PRINTF("enc424j600_drv: tx (%u)\n", (unsigned int)packetbuf_totlen());

  if (!enc424j600_status.link_active) {
    PRINTF("enc424j600_drv: offline\n");
	 return ETHERNET_TX_ERR_OFFLINE;
  }
  rtimer_clock_t link_delay = RTIMER_NOW();
  while(enc424j600_status.link_busy) {
    watchdog_periodic();
    if (RTIMER_NOW()-link_delay > (RTIMER_SECOND)/50) {
      PRINTF("enc424j600_drv: link-busy. Resetting...\n");
		enc424j600_drv_reset();
		return ETHERNET_TX_ERR_FATAL;
	 }
  }
 
  enc424j600_status.link_busy = 1;
  
  if (enc424j600_pkt_send(packetbuf_totlen(), packetbuf_hdrptr()) != 
    ENC424J600_STATUS_OK) {
    PRINTF("enc424j600_drv: collision\n");
	 enc424j600_status.link_busy = 0;
	 return ETHERNET_TX_ERR_COLLISION;
  }
#if ETHERNET_BLOCK_UNTIL_TX_COMP
  volatile uint32_t tx_done;
  clock_time_t current_time = clock_time();
  while(1) {
    watchdog_periodic();
	 tx_done = (!enc424j600_status.link_busy);
	 if (tx_done) {
      /* Transmission completed. Check result and break. */
		if (enc424j600_status.link_tx_err) {
			enc424j600_status.link_tx_err = 0;
			return ETHERNET_TX_ERR_COLLISION;
		} else {
        return ETHERNET_TX_OK;
		}
    } else {
      if (clock_time()-current_time > ETHERNET_TX_BLOCK_TIMEOUT_MS) {
        /* Timeout. Remove busy flag. */
		  enc424j600_status.link_busy = 0;
		  return ETHERNET_TX_ERR_TIMEOUT;
		}
	 }
  }
#endif /* ETHERNET_BLOCK_UNTIL_TX_COMP */
  return ETHERNET_TX_OK;	 	
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_drv_init(void)
{
  PRINTF("enc424j600_drv: init\r\n");
  enc424j600_status.link_active = 0;
  enc424j600_status.link_transition = 0;
  enc424j600_status.link_initialized = 0;
  enc424j600_status.link_has_address = 0;
  enc424j600_status.link_busy = 0;
#if ETHERNET_BLOCK_UNTIL_TX_COMP
  enc424j600_status.link_tx_err = 0;
#endif
#if 0  
  /* Initialize driver queues */
  list_init(enc424j600_rx_list);
  
  /* Initialize static memories */
  memb_init(&enc424j600_rx_buf_mem);
#endif  
  
  RINGMEM_INIT(enc424j600_ringmem);
    
  /* Initialize low-level driver */
  if (enc424j600_init() != ENC424J600_STATUS_OK) {
    PRINTF("enc424j600_drv: init-error\n");
    return;
  }
  
  if (enc424j600_get_macaddr(&enc424j600_macaddr.u8[0])) {
    PRINTF("enc424j600_drv: error-getting mac address\n");
	 return;
  }
  PRINTF("enc424j600_drv: ETH-ADDR: %02x:%02x:%02x:%02x:%02x:%02x\n",
    enc424j600_macaddr.u8[0],
    enc424j600_macaddr.u8[1],
    enc424j600_macaddr.u8[2],
    enc424j600_macaddr.u8[3],
    enc424j600_macaddr.u8[4],
    enc424j600_macaddr.u8[5]);
  enc424j600_status.link_has_address = 1;
  /* Set IRQ callback */
  enc424j600_set_irq_callback(enc424j600_drv_irq_cback);
    
  enc424j600_status.link_initialized = 1;
    
  /* Initialize receiving process */
  process_start(&enc424j600_drv_proc, NULL);
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_drv_reset(void)
{
  PRINTF("enc424j600_drv: reset\n");
  enc424j600_status.link_busy = 0;
  if (enc424j600_status.link_active) {
	  enc424j600_status.link_active = 0;
	  enc424j600_status.link_transition = 0;
  }
  enc424j600_status.link_initialized = 0;
    
  /* Initialize low-level driver */
  if (enc424j600_init() != ENC424J600_STATUS_OK) {
    PRINTF("enc424j600_drv: init-error\n");
    return;
  }
  enc424j600_status.link_initialized = 1;
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_drv_set_macaddr(linkaddr0_t* macaddr)
{
  if (enc424j600_set_macaddr(&macaddr->u8[0]) != ENC424J600_STATUS_OK) {
    PRINTF("enc424j600-drv: error-setting-mac-addr\n");
  }
}
/*---------------------------------------------------------------------------*/
static linkaddr0_t*
enc424j600_drv_get_macaddr(void)
{
  linkaddr0_t macaddr;

  if (enc424j600_status.link_initialized)
    return &enc424j600_macaddr;

  if (enc424j600_get_macaddr(&macaddr.u8[0]) != ENC424J600_STATUS_OK)
    return NULL;

  linkaddr0_copy(&enc424j600_macaddr, &macaddr);
  return &enc424j600_macaddr;
}
/*---------------------------------------------------------------------------*/
static void
enc424j600_drv_pollhandler(void)
{
  if(unlikely(!enc424j600_status.link_initialized))
    return;
  
  if(unlikely(!enc424j600_status.link_active))
    return;

  uint16_t copy_result;
  
  INTERRUPT_DISABLE(ENC624J600_IRQ_SOURCE);
  ringmem_block_t *pkt = ringmem_get_next_block_ptr(enc424j600_ringmem);
  INTERRUPT_ENABLE(ENC624J600_IRQ_SOURCE);

  if (pkt == NULL) {
    PRINTF("enc424j600_drv: polled on empty\n");
    return;
  }
  /* Clear the packet buffer in case of existing trash. */
  packetbuf_clear();
  packetbuf_set_datalen(pkt->len);
  copy_result = (packetbuf_copyfrom(&pkt->data_ptr, pkt->len) == pkt->len);
  /* Free driver queue resources */
  INTERRUPT_DISABLE(ENC624J600_IRQ_SOURCE);
  ringmem_free_block(enc424j600_ringmem);
  pkt = ringmem_get_next_block_ptr(enc424j600_ringmem);
  INTERRUPT_ENABLE(ENC624J600_IRQ_SOURCE);
  
  if (copy_result == 0) {
    PRINTF("ENC424J600_DRV: packetbuf-copy-error\n"); 
  } else {
    /* Send the packet to the network stack. */
    NETSTACK_0_MAC.input();
  } 

  if (pkt != NULL) {
    process_poll(&enc424j600_drv_proc);
  }
#if 0
  /* Check for received packets */
  if (list_length(enc424j600_rx_list) > 0) {
    /* Pop a packet from the input buffer - ATOMIC! */
    PRINTF("ENC424J600_DRV: rx-poll [%u]\n",list_length(enc424j600_rx_list));
    INTERRUPT_DISABLE(ENC624J600_IRQ_SOURCE);
    enc424j600_pkt_buf_t *buffered_pkt = list_pop(enc424j600_rx_list);
    INTERRUPT_ENABLE(ENC624J600_IRQ_SOURCE);
    if (buffered_pkt != NULL && buffered_pkt->pkt_len != 0
      && buffered_pkt->pkt_data != NULL ) {
      PRINTF("ENC424J600_DRV: pkt,mem-len [%u]\n",
      (unsigned int)(buffered_pkt->pkt_len));
      /* Clear the packet buffer in case of existing trash. */
      packetbuf_clear();
      /* Copy packet content to Contiki packet buffer. */
      if(packetbuf_copyfrom(buffered_pkt->pkt_data,
        (uint16_t)buffered_pkt->pkt_len) != (int)buffered_pkt->pkt_len) {
        PRINTF("ENC424J600_DRV: packetbuf-copy-error\n");
        goto err_copy;
      }
      packetbuf_set_datalen((uint16_t)buffered_pkt->pkt_len);
      /* Send the packet to the network stack. */
      NETSTACK_0_MAC.input();
err_copy:
      INTERRUPT_DISABLE(ENC624J600_IRQ_SOURCE);
      memb_free(&enc424j600_rx_buf_mem, buffered_pkt);
      INTERRUPT_ENABLE(ENC624J600_IRQ_SOURCE);
    } else {
      printf("ENC424J600_DRV: rx-handler error; empty packet\n");
    }
  }
  
  if(list_length(enc424j600_rx_list) != 0) {
    /* Poll the process for processing next packet */
    PRINTF("ENC424J600_DRV: list contains more: [%u]\n",
    list_length(enc424j600_rx_list));
    process_poll(&enc424j600_drv_proc);
  }
#endif
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_drv_is_initialized(void)
{
  return enc424j600_status.link_initialized;
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_drv_plug(void)
{
  if (enc424j600_status.link_active) {
    return 0;
  }
  enc424j600_status.link_active = 1;
  //TODO bring line up
  return 1;
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_drv_unplug(uint8_t keep_resources)
{
  if (!enc424j600_status.link_active) {
    return 0;
  }
  enc424j600_status.link_active = 0;
  //TODO bring line down
  if (keep_resources) {	  
  } else {	  
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static uint8_t
enc424j600_drv_is_up(void)
{
  return enc424j600_status.link_active;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(enc424j600_drv_proc, ev, data)
{
  UNUSED(data);

  /* Register poll-handler */
  PROCESS_POLLHANDLER(enc424j600_drv_pollhandler());

  PROCESS_BEGIN();

  PRINTF("ENC424J600_driver process\n");

  /* Copy link address in the data structure */
  linkaddr0_copy(&linkaddr0_node_addr, enc424j600_drv_get_macaddr());

#ifdef ETHERNET_BLOCK_UNTIL_LINK_UP
#if ETHERNET_BLOCK_UNTIL_LINK_UP
  clock_time_t t = clock_time();
  while(!enc424j600_drv_is_up()) {
    watchdog_periodic();
    if (clock_time() - t > (CLOCK_SECOND*ETHERNET_INIT_BLOCK_TIMEOUT_S))
      break;
  }
#endif
#endif /* ETHERNET_BLOCK_UNTIL_LINK_UP */

  while(1) {
    PROCESS_PAUSE();
    if (unlikely(ev == PROCESS_EVENT_EXIT)) {
      if (enc424j600_drv_is_up()) {
        NETSTACK_0_MAC.connect_event(0);
		}
      break;
	 }
    if (!enc424j600_status.link_transition)
      continue;
    /* Transition has occurred */
    if (!enc424j600_drv_is_up()) {
      /* Device went down. Inform MAC. */
      NETSTACK_0_MAC.connect_event(0);
    } else {
      NETSTACK_0_MAC.connect_event(1);
    }
    enc424j600_status.link_transition = 0;
  }

  PRINTF("ENC424J600_DRV: exit\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
const struct eth_driver enc424j600_driver = {
  "ENC424J600",
  enc424j600_drv_init,
  enc424j600_drv_reset,
  enc424j600_drv_set_macaddr,
  enc424j600_drv_get_macaddr,
  enc424j600_drv_tx,
  enc424j600_drv_is_initialized,
  enc424j600_drv_is_up,
  enc424j600_drv_plug,
  enc424j600_drv_unplug,
};
/*---------------------------------------------------------------------------*/
#endif /* WITH_ETHERNET_SUPPORT */
/*
 * xbee_framer.c
 *
 * Created: 2014-12-24 22:29:29
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se>
 */ 
#include "xbee-api.h"
#include "linkaddr.h"
#include "framer.h"
#include "packetbuf.h"
#include "random.h"
#if NETSTACK_CONF_WITH_DUAL_RADIO
#include "linkaddrx.h"
#include "netstack_c.h"
#endif

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTADDR(addr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7])
#else
#define PRINTF(...)
#define PRINTADDR(addr)
#endif

#define XBEE_BCAST_64_ADDR {{0, 0, 0, 0, 0, 0, 0xff, 0xff}}

/*---------------------------------------------------------------------------*/
static int
hdr_length(void)
{
  return sizeof(xbee_at_frame_tx_hdr_t);
}
/*---------------------------------------------------------------------------*/
static int
create(void)
{
  uint8_t hdr_len = sizeof(xbee_at_frame_tx_hdr_t);
  if(packetbuf_hdralloc(hdr_len)) {
    /* This is a hack. Since we are using a single packet buffer, with 
     * the maximum packet frame considering all interfaces, we have to
     * manually check whether we have exceeded the acceptable length of
     * an XBEE frame. TODO
     */
    xbee_at_frame_tx_hdr_t *hdr = packetbuf_hdrptr();
    memset(hdr, 0, sizeof(xbee_at_frame_tx_hdr_t));
    /* Fill-in the XBEE header  */
    if (!packetbuf_holds_broadcast()) {
      linkaddr_copy((linkaddr_t*)(hdr->dst_addr),
        (linkaddr_t*)packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
      hdr->option = XBEE_TX_OPT_UNICAST;
    } else {
      linkaddr_t bcast = XBEE_BCAST_64_ADDR;
      linkaddr_copy((linkaddr_t*)(&hdr->dst_addr[0]), &bcast);
      hdr->option = XBEE_TX_OPT_NO_ACK;
      packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);
    }
    hdr->api_id = XBEE_AT_TX_FRAME_ID;
#if XBEE_WITH_FRAME_SEQNO
    hdr->seqno = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);
#endif
    hdr->frame_id = (((uint8_t)(packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO))) != 0) ? 
      (uint8_t)(packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO)) : 1;
#if DEBUG
    int i;
    PRINTF("XBEE-OUT: ");
    PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
    PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
#if XBEE_WITH_FRAME_SEQNO
    PRINTF("SEQNO: %02x\n", hdr->seqno);
#endif
    PRINTF("Frame ID %02x\n", hdr->frame_id);
    PRINTF("Frame Option: %02x\n", hdr->option);
    PRINTF("XBEE_FRAMER: (%u) ", packetbuf_totlen());
    uint8_t *frame_data = packetbuf_hdrptr();
    for (i=0; i<packetbuf_totlen(); i++) {
      PRINTF("%02x ",frame_data[i]);
    }
    PRINTF("\n");
#endif
    return hdr_len;
  }
  return FRAMER_FAILED;
}
/*---------------------------------------------------------------------------*/
static int
parse(void)
{
  if (packetbuf_datalen() < XBEE_RX_RF_HDR_SIZE) {
    PRINTF("XBEE_FRAMER: too-short pkt [%u]\n",packetbuf_datalen());
    return FRAMER_FAILED;
  }
  xbee_at_frame_rsp_t *hdr = (xbee_at_frame_rsp_t *)packetbuf_hdrptr();
  
  /* Register source address */
  linkaddr_t srcaddr;  
  linkaddr_copy(&srcaddr, (linkaddr_t *)&hdr->src_addr[0]);
  packetbuf_set_addr(PACKETBUF_ADDR_SENDER, (linkaddr_t*)&srcaddr);
  
  /* Register destination address */
  if (hdr->option & XBEE_RX_OPT_PAN_BCAST) {
    PRINTF("XBEE_FRAMER: drop-bcast-pan-frame\n");
    return FRAMER_FAILED;
  }
#if NETSTACK_CONF_WITH_DUAL_RADIO
  uint8_t radio_type = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_RADIO_INTERFACE);
#endif
  if (hdr->option & XBEE_RX_OPT_ADDR_BCAST) {
    PRINTF("XBEE_FRAMER: broadcast\n");
#if ! NETSTACK_CONF_WITH_DUAL_RADIO
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr_null);
#else
    if (radio_type == NETSTACK_802154) {
      packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr_null);
    } else if (radio_type == NETSTACK_802154_SEC) {
      packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, (linkaddr_t *)&linkaddr1_null);
    } else {
      return FRAME_FAILED;
    }
#endif /* NETSTACK_CONF_WITH_DUAL_RADIO */
  } else {
#if ! NETSTACK_CONF_WITH_DUAL_RADIO
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr_node_addr);
#else
    if (radio_type == NETSTACK_802154) {
      packetbuf_set_addr(PACKETBUD_ADDR_RECEIVER, &linkaddr_node_addr);
    } else if (radio_type == NETSTACK_802154_SEC) {
      packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, (linkaddr_t *)&linkaddr1_node_addr);
    } else {
      return FRAME_FAILED;
    }
#endif /* NETSTACK_CONF_WITH_DUAL_RADIO */
  }
  /* Register sequence number if option is suported */
#if XBEE_WITH_FRAME_SEQNO 
  packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, hdr->seqno);
#else
  packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, random_rand());
#endif
  packetbuf_set_attr(PACKETBUF_ATTR_FRAME_TYPE, FRAME802154_DATAFRAME);
  PRINTF("IEEE802154-IN: ");
  PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
  PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
  PRINTF("[%u]\n", packetbuf_datalen());

  /* Header parsed & removed */
  if (packetbuf_hdrreduce(XBEE_RX_RF_HDR_SIZE))
    return XBEE_RX_RF_HDR_SIZE;
  
  return FRAMER_FAILED;
}
/*---------------------------------------------------------------------------*/
const struct framer xbee_framer = {
	hdr_length,
	create,
	framer_canonical_create_and_secure,
	parse
};
/*---------------------------------------------------------------------------*/

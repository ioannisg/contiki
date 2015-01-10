/*
 * ieee80211_net.c
 *
 * Created: 2/16/2014 6:34:40 PM
 *  Author: Ioannis Glaropoulos
 */ 

#include "net/linkaddr.h"
#include "net/netstack_x.h"
#include "net/linkaddrx.h"

#include "compiler.h"
#include "mini_ip.h"
#include "packetbuf.h"

#include "net/ip/uip.h"
#include "net/ip/tcpip.h"
#if UIP_MULTI_IFACES
#include "net/ipv6/uip-ds6.h"
#endif

#include "ipv6/uip-ds6-nbr.h"
#include "ipv4/uip_arp.h"

#include "process.h"

#include "netop-conf.h"

#if WITH_WIFI_SUPPORT
#ifdef NETSTACK_CONF_WITH_IPV6

#define UIP_IP_BUF          ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF          ((struct uip_udp_hdr *)&uip_buf[UIP_LLIPH_LEN])
#define UIP_TCP_BUF          ((struct uip_tcp_hdr *)&uip_buf[UIP_LLIPH_LEN])
#define UIP_ICMP_BUF          ((struct uip_icmp_hdr *)&uip_buf[UIP_LLIPH_LEN])


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#if UIP_MULTI_IFACES
static uip_ds6_iface_t *uip_iface = NULL; /** \brief a pointer to the corresponding DS6 interface */
#else
netstack_link_type_t link_if;
#endif
/*---------------------------------------------------------------------------*/
static void 
packet_sent(void* ptr, int status, int retransmissions) {
	
	UNUSED(ptr);

	if(!status) {
		PRINTF("IEEE80211_net: Packet was sent/scheduled successfully.\n");
	}
	else {		
		if (retransmissions == 0) {
			PRINTF("IEEE80211_net: not-attempted\n");			
		} else {
			PRINTF("IEEE80211_net: NOT-successful\n");
		}
	}	
	/* Inform UIP, RPL */
#if ! UIP_MULTI_IFACES
	uip_ds6_link_neighbor_callback(status, retransmissions);
#else
   uip_ds6_link_neighbor_callback(uip_iface, status, retransmissions);
#endif
}

/*--------------------------------------------------------------------*/
/** \brief Process a received 802.11 packet.
 *  \param r The MAC layer
 *
 * The 802.11 packet payload is put in the "packetbuf" by the underlying
 * MAC. It is actually an IP packet. The IP packet must be complete and,
 * thus, it is copied to the "uip_buf" and the IP layer is called. The 
 * module, actually, does nothing important.
 */
static void 
input(void) {		
	
	PRINTF("IEEE80211_NET: recv (%u)\n",packetbuf_datalen());
	
	memcpy((uint8_t *)UIP_IP_BUF, (uint8_t*)(packetbuf_dataptr()), packetbuf_datalen());
	uip_len = packetbuf_datalen();
	
	#if DEBUG
	int i;
	uint8_t *pkt = packetbuf_dataptr();
	for (i=0; i<packetbuf_datalen(); i++)
		PRINTF("%02x ", pkt[i]);
	PRINTF("\n");
	#endif
		
	/* Deliver the packet to the "uip" layer. */
	tcpip_input();		
}
/*--------------------------------------------------------------------*/
/** \brief Take an IP packet and format it to be sent on an 802.11
 *  network using the underlying WiFi implementation.
 *  \param localdest The MAC address of the destination
 *
 *  The IP packet is initially in uip_buf. The resulting packet is 
 *  put in the "packetbuf" and delivered to the 802.11 MAC module.
 */
static uint8_t 
output(const uip_lladdr_t *localdest) {
	
	if (unlikely(uip_len <= 0)) {
		PRINTF("IEEE80211_NET: empty-corrupted UIP packet\n");
		return 0;
	} 
#if DEBUG
	int i;
	PRINTF("UIP [%u]: ",uip_len);
	for(i=0; i<uip_len; i++) {
		PRINTF("%02x ", uip_buf[i]);
	}
	PRINTF("\n");
#endif	
	
	packetbuf_clear();
	
	/* Copy the UIP payload to the packet buffer. */
	if (unlikely(!packetbuf_copyfrom(uip_buf, uip_len) == uip_len)) {
		PRINTF("IEEE80211_NET: too-large-pkt (%u)\n",uip_len);
		return 0;
	}	
	/* Packet length */
	packetbuf_set_datalen(uip_len);
	
	/* Source MAC address */
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, (linkaddr_t*)&linkaddr1_node_addr);
	
	/* Destination MAC address */
	if (localdest == NULL) {
		packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr1_null);
		PRINTF("IEEE80211_NET: tx (%u) to (%02x)\n", uip_len, linkaddr1_null.u8[0]);	
	} else {
		packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, 
			(linkaddr_t*)&localdest->addr[0]);
		PRINTF("IEEE80211_NET: tx (%u) to (%02x)\n", uip_len, localdest->addr[0]);
	}		
		
	/* Maybe redundant since DATA type is zero value */
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, 
		PACKETBUF_ATTR_PACKET_TYPE_DATA);
	
	/* Send the packet down to the MAC. */
	NETSTACK_1_MAC.send(packet_sent, NULL);	
	
	return 1;
}
/*---------------------------------------------------------------------------*/
static void
init(void) {

  /*
   * Register a new network interface at UIP
   * supplying the Link Layer address of the
   * physical interface, the output function
   * for TCP/IP and the type of the physical
   * interface.
   */
#if ! UIP_MULTIPLE_IFACES
  linkaddr_copy(&linkaddr_node_addr, (linkaddr_t*)&linkaddr1_node_addr);
	
  /* This is the only interface so we must set up the
   * link address for UIP4, particularly, for the ARP.
   */
#pragma message("LINK address set by WiFi")
  uip_lladdr_t eth_addr;
  memcpy(eth_addr.addr, &linkaddr1_node_addr.u8[0], UIP_LLADDR_LEN);
  uip_setethaddr(eth_addr);

  /*
   * Set out output function as the function to be called from uIP to
   * send a network packet.
   */
  tcpip_set_outputfunc(output);	
#else
  uip_iface = uip_ds6_register_net_iface(NETSTACK_80211, &linkaddr1_node_addr,
    output);
#endif /* ! UIP_MULTI_IFACES */

  /* Upper-layer networking starts when 802.11 is established */
}
/*---------------------------------------------------------------------------*/
const struct network_driver ieee80211_net_driver = {
	(char*)"ieee80211_net",
	init,
	input,
};

#endif /* UIP_CONF_IPV6 */
#endif /* WITH_WIFI_SUPPORT */
/*
 * ieee80211_ibss_setup.c
 *
 * Created: 2/13/2014 8:43:53 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "contiki.h"
#include "ieee80211_ibss_setup.h"
#include "ieee80211_ibss.h"
#include "uip.h"
#include "resolv.h"
#include "rpl.h"
#include "uip_arp.h"
#include "packetbuf.h"
#include "udp-client.h"
#include "udp-server.h"
#if WITH_SLIP_RADIO
#include "slip-radio.h"
#endif
#include "wifistack.h"

#if WITH_WIFI_SUPPORT

#ifdef IEEE80211_CONF_IBSS_SCAN_PERIOD_S
#define IEEE80211_IBSS_SCAN_PERIOD_S IEEE80211_CONF_IBSS_SCAN_PERIOD_S
#else
#define IEEE80211_IBSS_SCAN_PERIOD_S	5
#endif

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif



/*---------------------------------------------------------------------------*/
PROCESS(ieee80211_ibss_setup_process, "IEEE80211_IBSS_SETUP_PROCESS");

process_event_t wifi_init_complete_event_message;

static struct etimer et;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ieee80211_ibss_setup_process, ev, data)
{
	PROCESS_BEGIN();
	PRINTF("IEEE80211_SETUP_PROC: Setting-up net\n");
	
	wifi_init_complete_event_message = process_alloc_event();
	
	/* The process starts when the low-level WIFI driver has initialized the 
	 * device
	 */	
	etimer_set(&et, IEEE80211_IBSS_SCAN_PERIOD_S*CLOCK_SECOND);	
	
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) || (ev == PROCESS_EVENT_CONTINUE));
	
	if (ev == PROCESS_EVENT_CONTINUE) {	
		/* Must join; TODO change to join */
		PRINTF("IEEE80211_SETUP_PROC: Merged into default ibss...\n");	
	} else {
		/* Must create the IBSS */
		PRINTF("IEEE80211_SETUP_PROC: creating-ibss...\n");
		ieee80211_sta_create_ibss();
		WIFI_STACK.join();
	}
	etimer_set(&et, 2*CLOCK_SECOND);
	
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	
#if WITH_SLIP_RADIO
process_start(&slip_radio_process, NULL);
#else /* WITH_SLIP_RADIO */
#if NETSTACK_CONF_WITH_IPV6
/*	
	uip_lladdr_t eth_addr;
	memcpy(eth_addr.addr, ieee80211_ibss_vif.addr, UIP_LLADDR_LEN);
	linkaddr_set_node_addr(&ieee80211_ibss_vif.addr);
	uip_setethaddr(eth_addr);
	PRINTF("IEE80211_SETUP: Copying MAC address to UIP ARP: %02x:%02x:%02x:%02x:%02x:%02x.\n",
			uip_lladdr.addr[0],
			uip_lladdr.addr[1],
			uip_lladdr.addr[2],
			uip_lladdr.addr[3],
			uip_lladdr.addr[4],
			uip_lladdr.addr[5]);
*/	
	/* Start upper-layer networking */
	PRINTF("IEEE80211_SETUP: Starting-upper-layers\n");
/*	
	process_post(PROCESS_BROADCAST, wifi_init_complete_event_message, NULL);
*/
	if (!process_is_running(&tcpip_process)) {
		process_start(&tcpip_process, NULL);
	}
	
	/* Initialize RPL, if not already started. Normally it should not start,
	 * unless a wireless interface started RPL earlier, which, currently, is
	 * not possible, as we only support one wireless and one wired interface
	 */
#if UIP_CONF_IPV6_RPL
	rpl_init();
#endif
	
#ifdef NET_PROC
	if (!process_is_running(&(NET_PROC))) {
		process_start(&(NET_PROC), NULL);
	} else {
		/* Network process already runs, so simply inform about WiFi start */
		process_post(&(NET_PROC), wifi_init_complete_event_message, NULL);
	}
#endif
		
#endif /* UIP_CONF_IPV6 */
#endif /* WITH_SLIP_RADIO */

	/* TODO while loop where the process checks the status of the WLAN
	 * and takes proper actions.
	 */
	PROCESS_END();
}
#endif /* WITH_WIFI_SUPPORT */
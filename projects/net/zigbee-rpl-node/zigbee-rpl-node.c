/*
 * zigbee_rpl_node.c
 *
 * Created: 2014-12-26 10:57:50
 *  Author: Ioannis Glaropoulos
 */ 
#include "contiki-net.h"
#include "net-monitor.h"

#if ZIGBEE_RPL_NODE

#include "zigbee-rpl-node/zigbee-rpl-node.h"

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

/*---------------------------------------------------------------------------*/
PROCESS(xbee_ipv6_test_process, "XBEE IPv6 node");
//AUTOSTART_PROCESSES(&xbee_ipv6_test_process);
/*---------------------------------------------------------------------------*/
static void
print_net_conf(void)
{
  int i;
  uint8_t state, issued;
  PRINTF("XBEE_IPV6_TEST: IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    issued = uip_ds6_if.addr_list[i].isused;
    if(state == ADDR_TENTATIVE || state == ADDR_PREFERRED) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("Status: %u, issued: %u\n",state, issued);
      PRINTF("\n");
    }
  }
#if UIP_CONF_ROUTER
  PRINTF("XBEE_IPV6_TEST: IPv6 prefixes: ");
  for(i = 0; i < UIP_DS6_PREFIX_NB; i++) {
    PRINT6ADDR(&uip_ds6_prefix_list[i].ipaddr);
    issued = uip_ds6_prefix_list[i].isused;
    PRINTF("Used: %u\n",issued);
  }
#endif
}
/*---------------------------------------------------------------------------*/
static struct etimer et;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(xbee_ipv6_test_process, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("xbee-rpl-node: starts-upper-layer-net\n");
  if (unlikely(!process_is_running(&tcpip_process))) {
    process_start(&tcpip_process, NULL);
  }
  print_net_conf();
  
  /* Start (m)DNS client */
  process_start(&resolv_process, NULL);
  /* Create host name based on MAC address */
  char name[15];
  sprintf(name, "contiki-node-%02x", uip_lladdr.addr[7]);
  resolv_set_hostname(&name[0]);
  PROCESS_PAUSE();
  
  /* Start network configuration monitor */
  process_start(&net_monitor_process, NULL);

  /* Start process loop */
  while (1) {
    static resolv_status_t status;
    static uip_ipaddr_t *resv_addr;
    etimer_set(&et, CLOCK_SECOND*60);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    /* Check the status of the services */
    if ((resolv_lookup("contiki-xbee-router.local", &resv_addr)
      != RESOLV_STATUS_CACHED)) {
      resolv_query("contiki-xbee-router.local");
    } else {
       uip_nameserver_update(resv_addr, UIP_NAMESERVER_INFINITE_LIFETIME);
    }
  }
  PRINTF("xbee-rpl-node: exit\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
#endif /* ZIGBEE_RPL_NODE */

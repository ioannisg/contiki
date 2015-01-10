/*
 * zigbee_rpl_node.c
 *
 * Created: 2014-12-26 10:57:50
 *  Author: ioannisg
 */ 
#include "contiki-net.h"
#include "net-monitor.h"

#if ZIGBEE_RPL_NODE

#include "zigbee-rpl-node/zigbee-rpl-node.h"

#include "compiler.h"

#define DEBUG DEBUG_PRINT
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
PROCESS_THREAD(xbee_ipv6_test_process, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("xbee-rpl-node: start upper-layer-net\n");
  print_net_conf();

  /* Start upper layer networking */
  if (unlikely(!process_is_running(&tcpip_process))) {
    process_start(&tcpip_process, NULL);
  }
  process_start(&resolv_process, NULL);

  /* Start network configuration monitor */
  process_start(&net_monitor_process, NULL);

  PROCESS_PAUSE();
	
  PRINTF("xbee-rpl-node: exit\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
#endif /* ZIGBEE_RPL_NODE */

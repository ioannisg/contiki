/*
 * ethernet_test.c
 *
 * Created: 5/11/2014 1:29:41 AM
 *  Author: Ioannis 
 */ 
#include "contiki.h"
#include "contiki-net.h"
#include "net-monitor.h"

#include "dev/ethernetstack.h"

#if ETH_NODE_SUPPORT

#include "eth-node.h"

#include "compiler.h"

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"


/*---------------------------------------------------------------------------*/
PROCESS(eth_ipv6_test_process, "Ethernet IPv6 node");
//AUTOSTART_PROCESSES(&eth_ipv6_test_process);
/*---------------------------------------------------------------------------*/
static void
print_net_conf(void)
{
        int i;
        uint8_t state, issued;
        PRINTF("ETH_IPV6_TEST: IPv6 addresses: ");
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
        PRINTF("ETH_IPV6_TEST: IPv6 prefixes: ");
        for(i = 0; i < UIP_DS6_PREFIX_NB; i++) {
                PRINT6ADDR(&uip_ds6_prefix_list[i].ipaddr);
                issued = uip_ds6_prefix_list[i].isused;
                PRINTF("Used: %u\n",issued);
        }
#endif  
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(eth_ipv6_test_process, ev, data)
{
	PROCESS_BEGIN();
	
	PRINTF("Ethernet IPv6 test\n");
	// TODO remove the following check
	if (!ETHERNET_STACK.is_up()) {
		PRINTF("ETH_TEST: eth-not-up\n");
		goto _exit_proc;
	}
		
	PRINTF("ETH_TEST: start upper-layer-net\n");
	print_net_conf();

	/* Start upper layer networking */
	if (unlikely(!process_is_running(&tcpip_process))) {
		process_start(&tcpip_process, NULL);
	}
	process_start(&resolv_process, NULL);

	/* Start network configuration monitor */
	process_start(&net_monitor_process, NULL);
	
	PROCESS_PAUSE();
	
_exit_proc:
	PRINTF("ETH_TEST: exit\n");      
	PROCESS_END();  
}
/*---------------------------------------------------------------------------*/
#endif /* ETH_NODE_SUPPORT */

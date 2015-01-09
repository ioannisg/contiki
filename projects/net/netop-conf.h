/*
 * netop_conf.h
 *
 * Created: 2014-06-27 20:39:46
 *  Author: Ioannis Glaropoulos
 */ 


#ifndef NETOP_CONF_H_
#define NETOP_CONF_H_

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++	*/
/*              Network Operation Modes							   */
/* -----------------------------------------------------------	*/

/* Makefile should include the ETH_Node option,
 * should only Ethernet at node mode be supported
 */
#ifdef WITH_ETH_NODE_SUPPORT
#define ETH_NODE_SUPPORT													1
#define PROJECT_NET_CONF_H "eth-node/project-conf.h"
#include "eth-node.h"
#define NET_PROC				eth_ipv6_test_process
#else
#define ETH_NODE_SUPPORT													0
#endif

/* Makefile should include the WIFI_RPL_Node option,
 * should only WiFi at node mode (with RPL) be supported
 */
#ifdef WITH_WIFI_RPL_NODE_SUPPORT
#define WIFI_RPL_NODE_SUPPORT												1
#define PROJECT_NET_CONF_H "wifi-rpl-node/project-conf.h"
#include "wifi-rpl-node/wifi-rpl-node.h"
#define NET_PROC				wifi_rpl_test_process
#else
#define WIFI_RPL_NODE_SUPPORT												0
#endif

/* Makefile should include the ETH-PRIORITY option,
 * should this operational mode be supported. This
 * operation prefers the Ethernet gateway instead
 * of WiFi, if the Ethernet interface is active,
 * and the Ethernet cable is plugged.
 */
#ifdef WITH_WIFI_ETH_PRIORITY_SUPPORT
#define WIFI_ETH_PRIORITY_SUPPORT										1
#define PROJECT_NET_CONF_H "eth-wifi-prior/project-conf.h"
#include "projects/net/eth-wifi-prior/wifi-eth-prior.h"
#define NET_PROC				wifi_rpl_eth_prior_process
#else
#define WIFI_ETH_PRIORITY_SUPPORT										0
#endif

/*
 * Makefile should include the RPL-ETH-ROUTER
 * macro, should the ETH-RPLbridge be supported.
 */
#ifdef WITH_ETH_WIFI_ROUTER_SUPPORT
#define ETH_WIFI_BORDER_ROUTER											1
#define PROJECT_NET_CONF_H "eth-wifi-router/project-conf.h"
#include "eth-wifi-router/eth-wifi-router.h"
#define NET_PROC				eth_wifi_router_process
#else
#define ETH_WIFI_BORDER_ROUTER											0
#endif

/*
 * Makefile should include the WITH_SLIP_RADIO
 * macro, should the slip radio be supported.
 */
#ifdef WITH_SLIP_RADIO
#define WITH_SLIP_RADIO														1
#define PROJECT_NET_CONF_H  "slip-radio/project-conf.h"
#endif

/*
 * Makefile should include the RPL-ETH-ROUTER
 * macro, should the ETH-RPL bridge be supported.
 */
#ifdef WITH_ETH_ZIGBEE_ROUTER_SUPPORT
#define ETH_XBEE_BORDER_ROUTER											1
#define PROJECT_NET_CONF_H "eth-zigbee-router/project-conf.h"
#define NET_PROC_H "eth-zigbee-router/eth-zigbee-router.h"
#else
#define ETH_XBEE_BORDER_ROUTER											0
#endif

/*
 * Makefile should include the ZIGBEE RPL node
 * macro, should the ETH-RPL bridge be supported.
 */
#ifdef WITH_ZIGBEE_RPL_NODE_SUPPORT
#define ZIGBEE_RPL_NODE       											1
#define PROJECT_NET_CONF_H "zigbee-rpl-node/project-conf.h"
#define NET_PROC_H "zigbee-rpl-node/zigbee-rpl-node.h"
#else
#define ZIGBEE_RPL_NODE														0
#endif

#endif /* NETOP-CONF_H_ */
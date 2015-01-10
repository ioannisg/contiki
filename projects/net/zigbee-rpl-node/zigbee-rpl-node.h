/*
 * zigbee_rpl_node.h
 *
 * Created: 2014-12-26 10:57:30
 *  Author: ioannisg
 */
#include "contiki.h"
#include "process.h"


#ifndef ZIGBEE_RPL_NODE_H_
#define ZIGBEE_RPL_NODE_H_


PROCESS_NAME(xbee_ipv6_test_process);


extern process_event_t xbee_node_connect_event;

#define NET_PROC_EVENT xbee_node_connect_event

#define NET_PROC				xbee_ipv6_test_process


#endif /* ZIGBEE_RPL_NODE_H_ */
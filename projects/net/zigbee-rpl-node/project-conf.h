/*
 * project_conf.h
 *
 * Created: 2014-12-03 15:37:23
 *  Author: ioannisg
 */ 


#ifndef PROJECT_NETOP_CONF_H_
#define PROJECT_NETOP_CONF_H_

#if ZIGBEE_RPL_NODE
/* Sanity check; ZIGBEE node needs XBEE support */
#if !WITH_ZIGBEE_SUPPORT
#error "XBEE node is requested without ZIGBEE support"
#endif

/* Buffer size of the XBEE serial device */
#define XBEE_SERIAL_LINE_CONF_BUFSIZE         128

/* Command wait timeout for XBEE nodes - to be placed in platform-conf.h */
#define XBEE_CONF_CMD_TIMEOUT_MS              10
#define XBEE_CONF_MAX_PENDING_CMD_LEN         4

/* XBEE uses null-rdc, however, packets are retried in H/W, so response
 * only reaches the driver after long time
 */
#define XBEE_CONF_TX_STATUS_WAIT_TIME         (RTIMER_SECOND / 20)

/*
 * XBEE uses null-rdc, which retries packets in H/W so AUTO_ACK flag 
 * must be turned off and blocking is supported on the radio driver.
 */
#define NULLRDC_CONF_802154_AUTOACK           0

/*
 * Reduce the packet-buffer size to what the XBEE devices requires
 */
#ifdef PACKETBUF_CONF_SIZE
#undef PACKETBUF_CONF_SIZE
#endif
#define PACKETBUF_CONF_SIZE                   512
#ifdef PACKETBUF_CONF_HDR_SIZE
#undef PACKETBUF_CONF_HDR_SIZE
#endif
#define PACKETBUF_CONF_HDR_SIZE               64

/* For ZigBee nodes the LINKADDR size is set to 8 */
#ifdef LINKADDR_CONF_SIZE
#undef LINKADDR_CONF_SIZE
#endif
#define LINKADDR_CONF_SIZE					       8

/* ZigBee nodes have a single interface */
#ifdef UIP_CONF_MULTI_IFACES
#if UIP_CONF_MULTI_IFACES	!= 0
#undef UIP_CONF_MULTI_IFACES
#define UIP_CONF_MULTI_IFACES				       0
#endif
#else
#define UIP_CONF_MULTI_IFACES				       0
#endif /* UIP_CONF_MULTI_IFACES */

/* ZigBee nodes use RPL to route packets */
#ifdef UIP_CONF_ROUTER
#undef  UIP_CONF_ROUTER
#endif
#define UIP_CONF_ROUTER						       1

#ifdef UIP_CONF_IPV6_RPL
#undef UIP_CONF_IPV6_RPL
#endif
#define UIP_CONF_IPV6_RPL					       1

/* Routing information objects included in RA packets */
#ifdef UIP_CONF_DS6_ROUTE_INFORMATION
#undef UIP_CONF_DS6_ROUTE_INFORMATION
#endif
#define UIP_CONF_DS6_ROUTE_INFORMATION        1

/* We increase the neighbor table size */
#ifdef NBR_TABLE_CONF_MAX_NEIGHBORS
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#endif
#define NBR_TABLE_CONF_MAX_NEIGHBORS          8

/* We increase the size of the CSMA queue */
#ifdef CSMA_CONF_MAX_NEIGHBOR_QUEUES
#undef CSMA_CONF_MAX_NEIGHBOR_QUEUES
#endif
#define CSMA_CONF_MAX_NEIGHBOR_QUEUES         4

/* We just increase the connection limit to 8 */
#undef UIP_CONF_MAX_CONNECTIONS
#define UIP_CONF_MAX_CONNECTIONS              8
#undef UIP_CONF_UDP_CONNS
#define UIP_CONF_UDP_CONNS                    8

/* If the router supports NAT64 we must not filter out IPv4 */
#if WITH_DNS64_SUPPORT
#undef NETSTACK_CONF_WITH_DNS64
#define NETSTACK_CONF_WITH_DNS64              1
#endif

/* REST chunk size is set to 256, double the default size */
#define REST_MAX_CHUNK_SIZE                   256

#ifndef CONTIKI_CONF_DEFAULT_HOSTNAME
#define CONTIKI_CONF_DEFAULT_HOSTNAME         "contiki-xbee-node"
#endif

#endif /* ZIGBEE_RPL_NODE */
#endif /* PROJECT_NETOP_CONF_H_ */

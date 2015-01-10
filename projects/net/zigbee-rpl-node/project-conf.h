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
#define PACKETBUF_CONF_SIZE                   256
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

#endif /* ZIGBEE_RPL_NODE */
#endif /* PROJECT_NETOP_CONF_H_ */
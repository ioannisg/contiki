/**
 * \addtogroup mbxxx-platform
 *
 * @{
 */
/*
 * Copyright (c) 2010, STMicroelectronics.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the Contiki OS
 *
 */
/*---------------------------------------------------------------------------*/
/**
* \file
*          Contiki-conf.h for MBXXX.
* \author
*          Salvatore Pitrulli <salvopitru@users.sourceforge.net>
*          Chi-Anh La <la@imag.fr>
*          Simon Duquennoy <simonduq@sics.se>
*/
/*---------------------------------------------------------------------------*/



#ifndef CONTIKI_CONF_H_
#define CONTIKI_CONF_H_


#ifdef PLATFORM_CONF_H
#include PLATFORM_CONF_H
#else
#include "platform-conf.h"
#endif /* PLATFORM_CONF_H */

#ifdef AUTOSTART_ENABLE
#define AUTOSTART_ENABLE											1
#endif /* AUTOSTART_ENABLE */

/* Add in project-conf for being able to auto-start processes */
#ifndef WITH_COAP
#define WITH_COAP														0
#endif

/* Common NETSTACK configuration for ETHERNET and WIFI */
#if (WITH_ETHERNET_SUPPORT || WITH_WIFI_SUPPORT) && (!WITH_ZIGBEE_SUPPORT)
#define PACKETBUF_CONF_SIZE										512
#define PACKETBUF_CONF_HDR_SIZE									64
#else
#define PACKETBUF_CONF_SIZE                              111
#define PACKETBUF_CONF_HDR_SIZE									64
#endif

/* Link addresses sizes */
#if WITH_ETHERNET_SUPPORT
#define LINKADDR0_CONF_SIZE										6
#endif /* WITH_ETHERNET_SUPPORT */

#if WITH_WIFI_SUPPORT
#define LINKADDR1_CONF_SIZE										6
#endif /* WITH_WIFI_SUPPORT */

#if WITH_ZIGBEE_SUPPORT
#define LINKADDR_CONF_SIZE											8
#endif /* WITH_ZIGBEE_SUPPORT */

/* Netstack_0 & 802.3 config [Interface 0] */
#if WITH_ETHERNET_SUPPORT
#define ETHERNET_CONF_STACK										ETHERNET_DEV_DRIVER
#define NETSTACK_0_CONF_MAC										ieee8023_mac_driver
#define NETSTACK_0_CONF_FRAMER									ieee8023_framer
#endif /* WITH_ETHERNET_SUPPORT */

/* Netstack & 802.11 Config */
#if WITH_WIFI_SUPPORT
#define WIFI_CONF_STACK												WIFI_DEV_DRIVER
#define NETSTACK_1_CONF_RDC										ieee80211_rdc_driver
#define NETSTACK_1_CONF_FRAMER									ieee80211_framer
#define NETSTACK_1_CONF_MAC										ieee80211_mac_driver
#define NETSTACK_CONF_MAC_SEQNO_HISTORY						8
#define IEEE80211_MAC_CONF_MAX_NEIGHBOR_QUEUES				4
#define IEEE80211_CONF_PSM											IBSS_STD_PSM
#define WIFISTACK_CONF_MAX_TX_QUEUES							4 // FIXME
#endif /* WITH_WIFI_SUPPORT */

#if WITH_ZIGBEE_SUPPORT
/* Netstack 802.15.4 config */
#define NETSTACK_CONF_MAC											csma_driver
#define NETSTACK_CONF_RDC											nullrdc_driver
#define NETSTACK_CONF_FRAMER										xbee_framer
#define NETSTACK_CONF_RADIO										ZIGBEE_RADIO_DRIVER
#define NETSTACK_CONF_MAC_SEQNO_HISTORY						8
#endif /* WITH_ZIGBEE_SUPPORT */

/* Radio and 802.15.4 params */
/* 802.15.4 radio channel */
#define RF_CHANNEL													26
/* 802.15.4 PAN ID */
#define IEEE802154_CONF_PANID										0xabcd
/* Use EID 64, enable hardware autoack and address filtering */
#define ST_CONF_RADIO_AUTOACK										1
/* Number of buffers for incoming frames */
#define RADIO_RXBUFS													2
/* Set to 0 for non ethernet links */
#define UIP_CONF_LLH_LEN											0

/* RDC params */
/* TX routine passes the cca/ack result in the return parameter */
#define RDC_CONF_HARDWARE_ACK										1
/* TX routine does automatic cca and optional backoff */
#define RDC_CONF_HARDWARE_CSMA									0
/* RDC debug with LED */
#define RDC_CONF_DEBUG_LED											1
/* Channel check rate (per second) */
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE					8
/* Use ACK for optimization (LPP, XMAC) */
#define WITH_ACK_OPTIMIZATION										0

/* ContikiMAC config */
#define CONTIKIMAC_CONF_COMPOWER									1
#define CONTIKIMAC_CONF_BROADCAST_RATE_LIMIT					0
#define CONTIKIMAC_CONF_ANNOUNCEMENTS							0
#define CONTIKIMAC_CONF_WITH_CONTIKIMAC_HEADER           0

/* CXMAC config */
#define CXMAC_CONF_ANNOUNCEMENTS									0
#define CXMAC_CONF_COMPOWER										1

/* XMAC config */
#define XMAC_CONF_ANNOUNCEMENTS									0
#define XMAC_CONF_COMPOWER											1

/* Other (RAM saving) */
#define ENERGEST_CONF_ON											0
#define QUEUEBUF_CONF_NUM											8//2 john
#define QUEUEBUF_CONF_REF_NUM										0
#define NBR_TABLE_CONF_MAX_NEIGHBORS							4
#define UIP_CONF_DS6_ROUTE_NBU									4
#define RPL_CONF_MAX_PARENTS_PER_DAG							4
#define RPL_CONF_MAX_INSTANCES									1
#define RPL_CONF_MAX_DAG_PER_INSTANCE							1
#define PROCESS_CONF_NUMEVENTS									16
/* The process names are not used to save RAM */
#define PROCESS_CONF_NO_PROCESS_NAMES							1
#define CC_CONF_NO_VA_ARGS											0
#define PACKETBUF_CONF_ATTRS_INLINE								1
#define RIMESTATS_CONF_ENABLED									0
#define CC_CONF_UNSIGNED_CHAR_BUGS								0
#define CC_CONF_DOUBLE_HASH										0
#define CONTIKI_CONF_SETTINGS_MANAGER							0
#define PROCESS_CONF_STATS											0
#define MMEM_CONF_SIZE												256// We do not use this memory

/* Common configuration for IPv6. Set in Makefile for IPv6 support. */
#if NETSTACK_CONF_WITH_IPV6 == 1

#define NETSTACK_CONF_WITH_IP64                                                         0
#define NETSTACK_CONF_WITH_RIPNG                                                        0

/* TCP if CoAP is needed. Set in Makefile for CoAP support. */
#if (WITH_COAP==7) || (WITH_COAP==6) || (WITH_COAP==3)
#define UIP_CONF_TCP													0
#else
#define UIP_CONF_TCP													1
#endif /* WITH_COAP */

#define UIP_CONF_UDP                							1
#define UIP_CONF_ICMP6												1

/* Default is single interface: override on netop-conf.h */
#define UIP_CONF_MULTI_IFACES								0

/* Default is single radio: override on netop-conf.h */
#define NETSTACK_CONF_WITH_DUAL_RADIO                                                   0
/* Default is not sending RIO in RAs */
#define UIP_CONF_DS6_ROUTE_INFORMATION                                                  0

/* Default forwarding and router configuration */
#define UIP_CONF_ROUTER                                                                 1
#define UIP_CONF_ND6_SEND_NA                                                            1
#define UIP_CONF_ND6_SEND_RA                                                            0
#define UIP_RA_ADVERTISE_DEFAULT_ROUTE                                                  0
#define UIP_CONF_PROCESS_RA_ROUTER                                                      0
#define UIP_CONF_DS6_RDNSS_INFORMATION                                                  0
#define UIP_ICMP_SEND_UNREACH_ROUTE                                                     0
#define UIP_CONF_ND6_MAX_PREFIXES                                                       2
#define UIP_CONF_ND6_MAX_DEFROUTERS                                                     1

/* Default IPv6 configuration: override on netop-conf.h */
#define UIP_CONF_IPV6												1
#define UIP_CONF_IPV6_QUEUE_PKT									1//0 We have enough memory
#define UIP_CONF_IPV6_CHECKS										1
#define UIP_CONF_IPV6_REASSEMBLY									0

#define UIP_CONF_DS6_ADDR_NBU										2

#define UIP_CONF_IP_FORWARD										0
#define UIP_CONF_MAX_CONNECTIONS									4
#define UIP_CONF_MAX_LISTENPORTS									8
#define UIP_CONF_UDP_CONNS											4
#define TCPIP_CONF_ANNOTATE_TRANSMISSIONS						0

#if WITH_WIFI_SUPPORT || WITH_ZIGBEE_SUPPORT
#define UIP_CONF_IPV6_RPL											1
#else
#define UIP_CONF_IPV6_RPL                                0
#endif

/* Common IPv6 Configuration for Ethernet and WiFi */
#if (((WITH_WIFI_SUPPORT) || (WITH_ETHERNET_SUPPORT)) && (!(WITH_ZIGBEE_SUPPORT)))
#define UIP_CONF_LL_802154											0
#define UIP_CONF_LL_80211											1
#define UIP_CONF_BUFFER_SIZE										450
#else
#define UIP_CONF_LL_802154                               1
#define UIP_CONF_LL_80211                                0
#define UIP_CONF_BUFFER_SIZE                                                           1280
#endif

/* IPv6 Configuration for each physical interface: Ethernet, WiFi, or ZigBee */
#if WITH_ETHERNET_SUPPORT
#define NETSTACK_0_CONF_NETWORK									ieee8023_net_driver
#endif

#if WITH_WIFI_SUPPORT
#define NETSTACK_1_CONF_NETWORK									ieee80211_net_driver
#endif

#if WITH_ZIGBEE_SUPPORT
#define NETSTACK_CONF_NETWORK										sicslowpan_driver
/* Specify a minimum packet size for 6lowpan compression to be
   enabled. This is needed for ContikiMAC, which needs packets to be
   larger than a specified size, if no ContikiMAC header should be
   used. */
#define SICSLOWPAN_CONF_COMPRESSION_THRESHOLD            63
#define SICSLOWPAN_CONF_MAC_MAX_PAYLOAD                  111
#define SICSLOWPAN_CONF_COMPRESSION_IPV6                 0
#define SICSLOWPAN_CONF_COMPRESSION_HC1                  1
#define SICSLOWPAN_CONF_COMPRESSION_HC01                 2
#define SICSLOWPAN_CONF_COMPRESSION                      SICSLOWPAN_COMPRESSION_HC06
#ifndef SICSLOWPAN_CONF_FRAG
#define SICSLOWPAN_CONF_FRAG                             1
#define SICSLOWPAN_CONF_MAXAGE                           8
#endif /* SICSLOWPAN_CONF_FRAG */
#define SICSLOWPAN_CONF_CONVENTIONAL_MAC	               1
#define SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS                2
#ifndef SICSLOWPAN_CONF_ADDR_CONTEXT_0
#define SICSLOWPAN_CONF_ADDR_CONTEXT_0 { \
	addr_contexts[0].prefix[0] = 0xaa; \
	addr_contexts[0].prefix[1] = 0xaa; \
}
#endif
#ifndef SICSLOWPAN_CONF_MAX_MAC_TRANSMISSIONS
#define SICSLOWPAN_CONF_MAX_MAC_TRANSMISSIONS            5
#endif /* SICSLOWPAN_CONF_MAX_MAC_TRANSMISSIONS */
#endif  /* WITH_ZIGBEE_SUPORT */

#else /* NESTACK_CONF_WITH_UIP6 */
/* Network setup for non-IPv6 (rime). */
#define NETSTACK_CONF_NETWORK										rime_driver

#define UIP_CONF_TCP													0
#define UIP_CONF_UDP													0
#define UIP_CONF_ICMP6                                   0
#define UIP_CONF_IPV6                                    0

#endif /* WITH_IPV6 */

/* ------------------------  Network Operation Mode  ----------------------- */
#include "../../contiki/projects/net/netop-conf.h"

/* If there is particular network configuration, include it */
#ifdef PROJECT_NET_CONF_H
#include PROJECT_NET_CONF_H
#endif

/* USB config */
#if WITH_USB_HOST_SUPPORT
#define USB_CONF_STACK					usb_driver
#else
#define USB_CONF_STACK
#endif /* WITH_USB_HOST_SUPPORT */

/* Include project-specific configuration */
#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif /* PROJECT_CONF_H */

#endif /* CONTIKI-CONF_H_ */
/** @} */

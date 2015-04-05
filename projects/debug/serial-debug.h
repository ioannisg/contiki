/* Copyright (c) 2015, Ioannis Glaropoulos.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "platform-conf.h"

/**
 * MAC debug information
 */
#define MAC_NO_DEBUG          0
#define MAC_ETH_DEBUG         1
#define MAC_WIFI_DEBUG        2
#define MAC_XBEE_DEBUG        4
#define MAC_8023_DEBUG        8
#define MAC_80211_DEBUG       16
#define MAC_6LOWPAN_DEBUG     32

/**
 * IPv6 debug information
 */
#define UIP6_NO_DEBUG         0
#define UIP6_UIP_DEBUG        1
#define UIP6_NBR_DEBUG        2
#define UIP6_ICMP_DEBUG       4
#define UIP6_ND6_DEBUG        8
#define UIP6_DS6_DEBUG        16
#define UIP6_TCPIP_DEBUG      32
#define UIP6_RPL_DEBUG        64

/**
 * PIN Configuration
 */
#ifndef SERIAL_DEBUG_CONF_BASE_PIN
#define SERIAL_DEBUG_BASE_PIN 40
#else
#define SERIAL_DEBUG_BASE_PIN SERIAL_DEBUG_CONF_BASE_PIN
#endif

#define ETH_DEBUG_PIN         (SERIAL_DEBUG_BASE_PIN + 1)
#define WIFI_DEBUG_PIN        (SERIAL_DEBUG_BASE_PIN + 2)
#define XBEE_DEBUG_PIN        (SERIAL_DEBUG_BASE_PIN + 3)
#define IEEE8023_DEBUG_PIN    (SERIAL_DEBUG_BASE_PIN + 4)
#define IEEE80211_DEBUG_PIN   (SERIAL_DEBUG_BASE_PIN + 5)
#define SICSLOWPAN_DEBUG_PIN  (SERIAL_DEBUG_BASE_PIN + 6)
#define UIP_DEBUG_PIN         (SERIAL_DEBUG_BASE_PIN + 7)
#define NBR_DEBUG_PIN         (SERIAL_DEBUG_BASE_PIN + 8)
#define ICMP_DEBUG_PIN        (SERIAL_DEBUG_BASE_PIN + 9)
#define ND6_DEBUG_PIN         (SERIAL_DEBUG_BASE_PIN + 10)
#define DS6_DEBUG_PIN         (SERIAL_DEBUG_BASE_PIN + 11)
#define TCPIP_DEBUG_PIN       (SERIAL_DEBUG_BASE_PIN + 12)
#define RPL_DEBUG_PIN         (SERIAL_DEBUG_BASE_PIN + 13)

/**
 * Serial debugging information
 */
typedef struct {
  uint8_t mac_debug;
  uint8_t ip_debug;
} serial_debug_t;
extern serial_debug_t serial_debug;

/**
 * Initialize pins for on-demand serial debug
 */
void serial_debug_init(void);

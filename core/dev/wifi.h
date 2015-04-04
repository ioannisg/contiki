/**
 * Copyright (c) 2013, Calipso project consortium
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or
 * other materials provided with the distribution.
 * 
 * 3. Neither the name of the Calipso nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific
 * prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/ 

/**
 * \file
 *		WiFi driver header file
 *
 * Created: 1/28/2014 2:31:32 PM
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se> 
 *
 */ 


#ifndef WIFI_H_
#define WIFI_H_

#include "mac80211.h"

/** List of packets to be sent by WiFi driver */
struct wifi_pkt_buf {
	struct wifi_pkt_buf *next;
	uint32_t pkt_len;
	uint8_t	pkt_data[0];
};
typedef struct wifi_pkt_buf wifi_pkt_buf_t;

/** WiFi virtual interface info */
struct wifi_vif_info {
	//struct list_head list;
	uint8_t active;
	unsigned int id;
	struct ieee80211_pkt_buf *beacon;
	uint8_t enable_beacon;
};

/* Generic RX-filter flags */
enum wifi_rx_filter_flag {
	/**< The WiFi driver should allow all frame receptions */
	WIFI_NO_FILTER,
	
	/**< The WiFi-driver should filter-out management frames */
	WIFI_FILTER_MGMT,
	
	/**< The WiFi-driver should filter-out data frames */
	WIFI_FILTER_DATA,
	
	/**< The WiFi-driver should filter-out all frames */
	WIFI_FILTER_ALL,
};
typedef enum wifi_rx_filter_flag wifi_rx_filter_flag_t;

/**
 * The structure of a WiFi (USB WIFI dongle) driver in Contiki.
 */
struct wifi_driver {
	
	const char *name;
	
	/** Initialize the WiFi driver */
	void (*init)(void);
	
	/** Callback function for connect/disconnect events */
	void (*connect)(le16_t vendor_id, le16_t product_id, uint8_t plug);
	
	/** Callback function for vendor-change events */
	void (*vendor_change)(le16_t vendor_id, le16_t product_id, uint8_t plug);
	
	/** Transmit a WiFi packet or add it to the driver outgoing queue */
	int (*tx_packet)(uint8_t is_sync);
	
	/** Add a driver command to the outgoing command queue */
	int (*queue_tx_cmd)(void);
	
	/** Send a command to the WiFi device and block until device response / timeout */
	int (*send_sync_cmd)(uint8_t is_synchronous);
	
	/** Callback for getting a USB response from low-level USB driver */
	void (*input)(void);
	
	/** Reset the WiFi driver */
	void (*reset)(void);
	
	/** Inform driver upon joining a network */
	void (*join)(void);	
	
	/** Request filtering of received WiFi frames */
	int (*rx_filter)(wifi_rx_filter_flag_t rx_filter_flag);
	
	/** Request config change due to BSS changes */
	int (*bss_info_changed)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			uint32_t change_flag);
	
	/** Request config change */
	int (*config)(struct ieee80211_hw* _hw, uint32_t change_flag);
	
	/** Request TX QoS config change */
	int (*config_tx)(struct ieee80211_hw *hw, struct ieee80211_vif *vif, 
		uint16_t queue, const struct ieee80211_tx_queue_params *param);
	
	/** Request RX config change */
	void (*config_rx)(void);	
	
	/** Request power-save */
	void (*powersave)(void);	
};

/* Generic WiFi-driver TX return values */
enum {
	/**< The WiFi-driver frame transmission was OK. */
	WIFI_TX_OK,
	
	/**< The WiFi-driver frame transmission was unsuccessful due to
	  errors in transmission that are not recognized */
	WIFI_TX_ERR_RADIO,
	
	/**< The WiFi-driver frame transmission could not be performed
	 due to a fatal error */
	WIFI_TX_ERR_FATAL,
	
	/**< The WiFi-driver frame transmission could not be performed
	 as the driver timed-out (for synchronous transmissions) */
	WIFI_TX_ERR_TIMEOUT,
	
	/**< The WiFi-driver frame transmission could not be performed
	 due to errors on the underlying USB transmission */
	WIFI_TX_ERR_USB,
	
	/**< The WiFi-driver frame transmission could not be performed
	 due to reaching the maximum number of pending frames */
	WIFI_TX_ERR_FULL,	
};

/* Generic WiFi-driver TX flags */ 
enum {
	/**< A WiFi-driver frame should be send without waiting for
	 pending frames to get (n)acknowledged */
	WIFI_TX_ASYNC,
	
	/**< A WiFi-driver frame should be sent after the earlier
	 frames are (n)acknowledged */
	WIFI_TX_SYNC,
};
#endif /* WIFI_H_ */
/*
 * ieee80211_rdc.h
 *
 * Created: 2/9/2014 2:49:19 PM
 *  Author: Ioannis Glaropoulos
 */ 


#ifndef IEEE80211_RDC_H_
#define IEEE80211_RDC_H_

#include "net/mac/rdc.h"
#include "contiki-conf.h"

#if WITH_WIFI_SUPPORT 

#ifdef IEEE80211_CONF_TX_MAX_FRAME_LENGTH
#define IEEE80211_RDC_MAX_FRAME_LENGTH	IEEE80211_CONF_TX_MAX_FRAME_LENGTH
#else
#define IEEE80211_RDC_MAX_FRAME_LENGTH	512
#endif
/* TODO cleanup */
struct ieee80211_rdc_pkt_buf;
struct ieee80211_rdc_pkt_buf {
	struct ieee80211_rdc_pkt_buf *next;
	uint32_t pkt_len;
	uint8_t pkt_data[IEEE80211_RDC_MAX_FRAME_LENGTH];
	};
typedef struct ieee80211_rdc_pkt_buf ieee80211_rdc_pkt_buf_t;
/* TODO cleanup */
struct ieee80211_rdc_neighbor_info;
struct ieee80211_rdc_neighbor_info {
	struct ieee80211_rdc_neighbor_info *next;
	uint8_t addr[LINKADDR1_CONF_SIZE];
};
typedef struct ieee80211_rdc_neighbor_info ieee80211_rdc_neighbor_info_t;


extern const struct rdc_driver ieee80211_rdc_driver;
void ieee80211_rdc_status_rsp_handler(unsigned int success, 
	unsigned int r, unsigned int t, uint8_t cookie);

#endif /* WITH_WIFI_SUPPORT */
#endif /* IEEE80211_RDC_H_ */
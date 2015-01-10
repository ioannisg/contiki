/*
 * ieee80211_ibss.h
 *
 * Created: 2/9/2014 3:01:47 PM
 *  Author: Ioannis Glaropoulos
 */ 


#ifndef IEEE80211_IBSS_H_
#define IEEE80211_IBSS_H_

#include "ieee80211_psm.h"
#include "mac80211.h"
#include "clock.h"
#include "if_ether.h"
#include "contiki-conf.h"

#ifdef IEEE80211_CONF_TX_MAX_FRAME_LENGTH
#define IEEE80211_TX_MAX_FRAME_LENGTH	IEEE80211_CONF_TX_MAX_FRAME_LENGTH
#else
#define IEEE80211_TX_MAX_FRAME_LENGTH	512
#endif


/* Power-saving mode */
#ifdef IEEE80211_CONF_PSM
#define IEEE80211_PSM	IEEE80211_CONF_PSM
#else
#define IEEE80211_PSM	IBSS_NULL_PSM;
#endif
/* Beacon period */
#ifdef IEEE80211_CONF_BCN_INT	
#define BEACON_INTERVAL IEEE80211_CONF_BCN_INT
#else
#define BEACON_INTERVAL	200
#endif
/* ATIM Window duration */
#ifdef IEEE80211_CONF_ATIM_W
#define ATIM_WINDOW_DURATION IEEE80211_CONF_ATIM_W
#else
#define ATIM_WINDOW_DURATION	50u
#endif
/* Soft Beacon Period */
#ifdef IEEE80211_CONF_SOFT_BCN_W
#define IEEE80211_SOFT_BCN_W	IEEE80211_CONF_SOFT_BCN_W
#else
#define IEEE80211_SOFT_BCN_W	50u
#endif
/* pre-TBTT window */
#ifdef IEEE80211_CONF_PRE_TBTT_W
#define IEEE80211_PRE_TBTT_W	IEEE80211_PRE_TBTT_W
#else
#define IEEE80211_PRE_TBTT_W	8u
#endif
/* Default BSSID which STAs will create or join to. */
#ifdef IEEE80211_CONF_DEFAULT_BSSID
#define IBSS_DEFAULT_BSSID	IEEE80211_CONF_DEFAULT_BSSID
#else
#define IBSS_DEFAULT_BSSID		{0x11, 0x22, 0x33, 0x44, 0x55, 0x66}
#endif
/* Timeout for scanning for the default IBSS network. */
#define IBSS_PROC_SCAN_TIMEOUT	7 * CLOCK_SECOND

#define IBSS_PROC_INIT_DELAY	3 * CLOCK_SECOND

/* ATIM Window Duration as a [%] percentage of beacon period. This one is not used, use the setting below!*/
#define ATIM_WINDOW_PERCENTAGE	20u



/* The ieee80211 packet buffer  */
struct ieee80211_pkt_buf;

struct ieee80211_pkt_buf {
	struct ieee80211_pkt_buf *next;
	uint32_t len;
	uint8_t data[IEEE80211_TX_MAX_FRAME_LENGTH];
};
typedef struct ieee80211_pkt_buf ieee80211_pkt_buf_t;

/** 
 * The unique global virtual interface structure
 */
extern struct ieee80211_vif ieee80211_ibss_vif;

/** 
 * The unique globally visible ieee80211 hardware structure
 */
extern struct ieee80211_hw ieee80211_ibss_hw;

/** The TX parameter set */
extern struct ieee80211_tx_queue_params ibss_tx_params;

/**
 * The unique transmission info structures for beacon and data frames
 */
extern struct ieee80211_tx_info ieee80211_ibss_bcn_tx_info;
extern struct ieee80211_tx_info ieee80211_ibss_data_tx_info;
extern struct ieee80211_tx_info ieee80211_ibss_bcast_tx_info;
extern struct ieee80211_tx_rate ieee80211_ibss_data_tx_rate;

/* Wireless channel type*/
extern uint8_t ibss_channel_type;

/**
 * The IBSS information structure 
 */
struct ieee80211_ibss_info {
	
	uint8_t is_initialized;
	uint8_t is_merged;
	/*
	 * The static array containing the 
	 * name representation of the IBSS
	 * Network. 
	 */
	uint8_t ibss_name[ETH_ALEN];
	/*
	 * The static array containing the 
	 * BSSID of the IBSS Network.
	 */
	uint8_t ibss_bssid[ETH_ALEN];
	/* The unique globally visible
	 * ieee80211 channel struct.
	 */
	struct ieee80211_channel ibss_channel;
	
	/* The unique beacon buffer */
	struct ieee80211_pkt_buf ibss_beacon_buf;

	enum ieee80211_ibss_ps_mode ibss_ps_mode;
};	

/* The unique global IBSS information struct */
extern struct ieee80211_ibss_info ibss_info;

/* The unique global ieee80211 STA information struct */
extern struct ieee80211_sta ibss_station;

/* The unique global AR9170 vif info structure */
extern struct wifi_vif_info ibss_ar9170_cvif;

/** The wireless phy interface */
extern struct wiphy ibss_attached_wiphy;


int __ieee80211_sta_init_hw(void);
int __ieee80211_sta_init_config();
int __ieee80211_hw_config(struct ieee80211_hw* _hw , uint32_t changed_flag);
void ieee80211_sta_create_ibss(void);
struct ieee80211_pkt_buf* ieee80211_beacon_get_tim(struct ieee80211_hw * hw, struct ieee80211_vif* vif, void* tim_offset, void* tim_length);
void ieee80211_sta_join_ibss(void);
#endif /* IEEE80211_IBSS_H_ */
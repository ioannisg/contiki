/*
 * nl80211.h
 *
 * Created: 1/29/2014 10:08:53 PM
 *  Author: ioannisg
 */ 


#ifndef NL80211_H_
#define NL80211_H_



enum nl80211_iftype {
	         NL80211_IFTYPE_UNSPECIFIED,
	         NL80211_IFTYPE_ADHOC,
	         NL80211_IFTYPE_STATION,
	         NL80211_IFTYPE_AP,
	         NL80211_IFTYPE_AP_VLAN,
	         NL80211_IFTYPE_WDS,
	         NL80211_IFTYPE_MONITOR,
	         NL80211_IFTYPE_MESH_POINT,
	         NL80211_IFTYPE_P2P_CLIENT,
	         NL80211_IFTYPE_P2P_GO,
	
	         /* keep last */
	         NUM_NL80211_IFTYPES,
	         NL80211_IFTYPE_MAX = NUM_NL80211_IFTYPES - 1
};

/**
 * enum nl80211_band - Frequency band
 * @NL80211_BAND_2GHZ: 2.4 GHz ISM band
 * @NL80211_BAND_5GHZ: around 5 GHz band (4.9 - 5.7 GHz)
 * @NL80211_BAND_60GHZ: around 60 GHz band (58.32 - 64.80 GHz)
 */
enum nl80211_band {
    NL80211_BAND_2GHZ,
	NL80211_BAND_5GHZ,
	NL80211_BAND_60GHZ,
};

enum nl80211_channel_type {
	NL80211_CHAN_NO_HT,
	NL80211_CHAN_HT20,
	NL80211_CHAN_HT40MINUS,
	NL80211_CHAN_HT40PLUS
};

enum nl80211_dfs_state {
	NL80211_DFS_USABLE,
	NL80211_DFS_UNAVAILABLE,
	NL80211_DFS_AVAILABLE,
};

/**
 * enum nl80211_chan_width - channel width definitions
 *
 * These values are used with the %NL80211_ATTR_CHANNEL_WIDTH
 * attribute.
 *
 * @NL80211_CHAN_WIDTH_20_NOHT: 20 MHz, non-HT channel
 * @NL80211_CHAN_WIDTH_20: 20 MHz HT channel
 * @NL80211_CHAN_WIDTH_40: 40 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *      attribute must be provided as well
 * @NL80211_CHAN_WIDTH_80: 80 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *      attribute must be provided as well
 * @NL80211_CHAN_WIDTH_80P80: 80+80 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *      and %NL80211_ATTR_CENTER_FREQ2 attributes must be provided as well
 * @NL80211_CHAN_WIDTH_160: 160 MHz channel, the %NL80211_ATTR_CENTER_FREQ1
 *      attribute must be provided as well
 * @NL80211_CHAN_WIDTH_5: 5 MHz OFDM channel
 * @NL80211_CHAN_WIDTH_10: 10 MHz OFDM channel
 */
enum nl80211_chan_width {
	         NL80211_CHAN_WIDTH_20_NOHT,
	         NL80211_CHAN_WIDTH_20,
	         NL80211_CHAN_WIDTH_40,
	         NL80211_CHAN_WIDTH_80,
	         NL80211_CHAN_WIDTH_80P80,
	         NL80211_CHAN_WIDTH_160,
	         NL80211_CHAN_WIDTH_5,
	         NL80211_CHAN_WIDTH_10,
};

#endif /* NL80211_H_ */
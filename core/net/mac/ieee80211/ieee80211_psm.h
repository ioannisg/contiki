/*
 * ieee80211_psm.h
 *
 * Created: 2/9/2014 3:01:11 PM
 *  Author: ioannisg
 */ 


#ifndef IEEE80211_PSM_H_
#define IEEE80211_PSM_H_

enum ieee80211_ibss_ps_mode {
	
	/* No PSM */
	IBSS_NULL_PSM,
	/* Standard PSM */
	IBSS_STD_PSM,
	/* Advanced PSM */
	IBSS_MH_PSM,
};



#endif /* IEEE80211_PSM_H_ */
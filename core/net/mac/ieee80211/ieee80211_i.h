#include "include\if_ether.h"
#include "ieee80211.h"
#include "cfg80211.h"
/*
 * ieee80211_i.h
 *
 * Created: 2/23/2014 11:29:51 PM
 *  Author: Ioannis Glaropoulos
 */ 


#ifndef IEEE80211_I_H_
#define IEEE80211_I_H_


/* this struct represents 802.11n's RA/TID combination */
struct ieee80211_ra_tid {
	uint8_t ra[ETH_ALEN];
	uint16_t tid;
};

/* this struct holds the value parsing from channel switch IE  */
struct ieee80211_csa_ie {
	struct cfg80211_chan_def chandef;
	uint8_t mode;
	uint8_t count;
	uint8_t ttl;
	uint16_t pre_value;
};

/* Parsed Information Elements */
struct ieee802_11_elems {
	const uint8_t *ie_start;
	size_t total_len;
	
	/* pointers to IEs */
	const uint8_t *ssid;
	const uint8_t *supp_rates;
	const uint8_t *ds_params;
	const struct ieee80211_tim_ie *tim;
	const uint8_t *ibss_params;
	const uint8_t *challenge;
	const uint8_t *rsn;
	const uint8_t *erp_info;
	const uint8_t *ext_supp_rates;
	const uint8_t *wmm_info;
	const uint8_t *wmm_param;
	const struct ieee80211_ht_cap *ht_cap_elem;
	const struct ieee80211_ht_operation *ht_operation;
	const struct ieee80211_vht_cap *vht_cap_elem;
	const struct ieee80211_vht_operation *vht_operation;
	const struct ieee80211_meshconf_ie *mesh_config;
	const uint8_t *mesh_id;
	const uint8_t *peering;
	const le16_t *awake_window;
	const uint8_t *preq;
	const uint8_t *prep;
	const uint8_t *perr;
	const struct ieee80211_rann_ie *rann;
	const struct ieee80211_channel_sw_ie *ch_switch_ie;
	const struct ieee80211_ext_chansw_ie *ext_chansw_ie;
	const struct ieee80211_wide_bw_chansw_ie *wide_bw_chansw_ie;
	const uint8_t *country_elem;
	const uint8_t *pwr_constr_elem;
	const struct ieee80211_timeout_interval_ie *timeout_int;
	const uint8_t *opmode_notif;
	const struct ieee80211_sec_chan_offs_ie *sec_chan_offs;
	const struct ieee80211_mesh_chansw_params_ie *mesh_chansw_params_ie;
	
	/* length of them, respectively */
	uint8_t ssid_len;
	uint8_t supp_rates_len;
	uint8_t tim_len;
	uint8_t ibss_params_len;
	uint8_t challenge_len;
	uint8_t rsn_len;
	uint8_t ext_supp_rates_len;
	uint8_t wmm_info_len;
	uint8_t wmm_param_len;
	uint8_t mesh_id_len;
	uint8_t peering_len;
	uint8_t preq_len;
	uint8_t prep_len;
	uint8_t perr_len;
	uint8_t country_elem_len;
	
	/* whether a parse error occurred while retrieving these elements */
	bool parse_error;
};

uint32_t ieee802_11_parse_elems_crc(const uint8_t *start, size_t len, bool action,
    struct ieee802_11_elems *elems, uint64_t filter, uint32_t crc);
	
static inline void ieee802_11_parse_elems(const uint8_t *start, size_t len,
                                           bool action,
                                           struct ieee802_11_elems *elems)
{
	ieee802_11_parse_elems_crc(start, len, action, elems, 0, 0);
}

#endif /* IEEE80211_I_H_ */
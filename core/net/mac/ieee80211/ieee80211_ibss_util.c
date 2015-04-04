#include "ieee80211_ibss_util.h"
#include "compiler.h"
#include "ieee80211.h"
#include "..\..\..\..\AVR8 GCC\Native\3.4.2.1002\avr8-gnu-toolchain\avr\include\string.h"
#include "ieee80211_i.h"
/*
 * ieee80211_ibss_util.c
 *
 * Created: 2/23/2014 11:57:42 PM
 *  Author: ioannisg
 */ 
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))

#define BIT(nr)				(1UL << (nr))
#define BITS(_start, _end)	((BIT(_end) - BIT(_start)) + BIT(_end))
#define BITS_PER_BYTE		8
#define BITS_PER_LONG		(BITS_PER_BYTE)*sizeof(long)

#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define BIT_MASK(nr)            (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)            ((nr) / BITS_PER_LONG)
#define DECLARE_BITMAP(name,bits) \
unsigned long name[BITS_TO_LONGS(bits)]

#define small_const_nbits(nbits) \
         ((nbits) && (nbits) <= BITS_PER_LONG)

/**
  7  * __set_bit - Set a bit in memory
  8  * @nr: the bit to set
  9  * @addr: the address to start counting from
 10  *
 11  * Unlike set_bit(), this function is non-atomic and may be reordered.
 12  * If it's called on the same region of memory simultaneously, the effect
 13  * may be that only one operation succeeds.
 14  */
 static inline void 
 __set_bit(int nr, volatile unsigned long *addr)
{
          unsigned long mask = BIT_MASK(nr);
          unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
  
          *p  |= mask;
 }
 
 /**
 99  * test_bit - Determine whether a bit is set
100  * @nr: bit number to test
101  * @addr: Address to start counting from
102  */
static inline int 
test_bit(int nr, const volatile unsigned long *addr)
{
         return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
}
 
static inline void 
bitmap_zero(unsigned long *dst, int nbits)
{
	         if (small_const_nbits(nbits))
	                 *dst = 0UL;
	         else {
		                 int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		                 memset(dst, 0, len);
	         }
}

static const uint8_t *
cfg80211_find_ie(uint8_t eid, const uint8_t *ies, int len)
 {
	         while (len > 2 && ies[0] != eid) {
		                 len -= ies[1] + 2;
		                 ies += ies[1] + 2;
	         }
	         if (len < 2)
	                 return NULL;
	         if (len < 2 + ies[1])
	                 return NULL;
	         return ies;
 }

uint32_t ieee802_11_parse_elems_crc(const uint8_t *start, size_t len, bool action,
                                struct ieee802_11_elems *elems,
                                uint64_t filter, uint32_t crc)
{
	size_t left = len;
	const uint8_t *pos = start;
	bool calc_crc = filter != 0;
	DECLARE_BITMAP(seen_elems, 256);
	const uint8_t *ie;
	
	bitmap_zero(seen_elems, 256);
	memset(elems, 0, sizeof(*elems));
	elems->ie_start = start;
	elems->total_len = len;
	
	while (left >= 2) {
		uint8_t id, elen;
		bool elem_parse_failed;
		
		id = *pos++;
		elen = *pos++;
		left -= 2;
		
		if (elen > left) {
			elems->parse_error = true;
			break;
		}
		
		switch (id) {
			case WLAN_EID_SSID:
			case WLAN_EID_SUPP_RATES:
			case WLAN_EID_FH_PARAMS:
			case WLAN_EID_DS_PARAMS:
			case WLAN_EID_CF_PARAMS:
			case WLAN_EID_TIM:
			case WLAN_EID_IBSS_PARAMS:
			case WLAN_EID_CHALLENGE:
			case WLAN_EID_RSN:
			case WLAN_EID_ERP_INFO:
			case WLAN_EID_EXT_SUPP_RATES:
			case WLAN_EID_HT_CAPABILITY:
			case WLAN_EID_HT_OPERATION:
			case WLAN_EID_VHT_CAPABILITY:
			case WLAN_EID_VHT_OPERATION:
			case WLAN_EID_MESH_ID:
			case WLAN_EID_MESH_CONFIG:
			case WLAN_EID_PEER_MGMT:
			case WLAN_EID_PREQ:
			case WLAN_EID_PREP:
			case WLAN_EID_PERR:
			case WLAN_EID_RANN:
			case WLAN_EID_CHANNEL_SWITCH:
			case WLAN_EID_EXT_CHANSWITCH_ANN:
			case WLAN_EID_COUNTRY:
			case WLAN_EID_PWR_CONSTRAINT:
			case WLAN_EID_TIMEOUT_INTERVAL:
			case WLAN_EID_SECONDARY_CHANNEL_OFFSET:
			case WLAN_EID_WIDE_BW_CHANNEL_SWITCH:
			case WLAN_EID_CHAN_SWITCH_PARAM:
			/*
			 * not listing WLAN_EID_CHANNEL_SWITCH_WRAPPER -- it seems possible
			 * that if the content gets bigger it might be needed more than once
			 */
			if (test_bit(id, seen_elems)) {
				elems->parse_error = true;
				left -= elen;
			    pos += elen;
			    continue;
			}
			break;
		}
		
		if (calc_crc && id < 64 && (filter & (1ULL << id)))
			// XXXcrc = crc32_be(crc, pos - 2, elen + 2);
		elem_parse_failed = false;
		
		switch (id) {
		case WLAN_EID_SSID:
			elems->ssid = pos;
			elems->ssid_len = elen;
			break;
		case WLAN_EID_SUPP_RATES:
			elems->supp_rates = pos;
			elems->supp_rates_len = elen;
			break;
		case WLAN_EID_DS_PARAMS:
			if (elen >= 1)
			    elems->ds_params = pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_TIM:
			if (elen >= sizeof(struct ieee80211_tim_ie)) {
				elems->tim = (void *)pos;
				elems->tim_len = elen;
			} else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_IBSS_PARAMS:
			if (elen >=1)	{
				elems->ibss_params = pos;
				elems->ibss_params_len = elen;				
			} else
				elem_parse_failed = true;
			break;
		case WLAN_EID_CHALLENGE:
			elems->challenge = pos;
			elems->challenge_len = elen;
			break;
		case WLAN_EID_VENDOR_SPECIFIC:
			if (elen >= 4 && pos[0] == 0x00 && pos[1] == 0x50 &&
			          pos[2] == 0xf2) {
				/* Microsoft OUI (00:50:F2) */
				
				if (calc_crc)
				   // XXX crc = crc32_be(crc, pos - 2, elen + 2);
				
				if (elen >= 5 && pos[3] == 2) {
				/* OUI Type 2 - WMM IE */
					if (pos[4] == 0) {
						elems->wmm_info = pos;
					    elems->wmm_info_len = elen;
					} else if (pos[4] == 1) {
					    elems->wmm_param = pos;
					    elems->wmm_param_len = elen;
				     }
				}
			}
			break;
		case WLAN_EID_RSN:
			elems->rsn = pos;
			elems->rsn_len = elen;
			break;
		case WLAN_EID_ERP_INFO:
			if (elen >= 1)
			    elems->erp_info = pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_EXT_SUPP_RATES:
			elems->ext_supp_rates = pos;
			elems->ext_supp_rates_len = elen;
			break;
		case WLAN_EID_HT_CAPABILITY:
			if (elen >= sizeof(struct ieee80211_ht_cap))
			    elems->ht_cap_elem = (void *)pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_HT_OPERATION:
			if (elen >= sizeof(struct ieee80211_ht_operation))
			     elems->ht_operation = (void *)pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_VHT_CAPABILITY:
			if (elen >= sizeof(struct ieee80211_vht_cap))
			   elems->vht_cap_elem = (void *)pos;
			else
			   elem_parse_failed = true;
			break;
		case WLAN_EID_VHT_OPERATION:
			if (elen >= sizeof(struct ieee80211_vht_operation))
			    elems->vht_operation = (void *)pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_OPMODE_NOTIF:
			if (elen > 0)
			    elems->opmode_notif = pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_MESH_ID:
			elems->mesh_id = pos;
			elems->mesh_id_len = elen;
			break;
		case WLAN_EID_MESH_CONFIG:
			if (elen >= sizeof(struct ieee80211_meshconf_ie))
			     elems->mesh_config = (void *)pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_PEER_MGMT:
			elems->peering = pos;
			elems->peering_len = elen;
			break;
		case WLAN_EID_MESH_AWAKE_WINDOW:
			if (elen >= 2)
			    elems->awake_window = (void *)pos;
			break;
		case WLAN_EID_PREQ:
			elems->preq = pos;
			elems->preq_len = elen;
			break;
		case WLAN_EID_PREP:
			elems->prep = pos;
			elems->prep_len = elen;
			break;
		case WLAN_EID_PERR:
			elems->perr = pos;
			elems->perr_len = elen;
			break;
		case WLAN_EID_RANN:
			if (elen >= sizeof(struct ieee80211_rann_ie))
				elems->rann = (void *)pos;
			else
			    elem_parse_failed = true;
			break;
		case WLAN_EID_CHANNEL_SWITCH:
			if (elen != sizeof(struct ieee80211_channel_sw_ie)) {
				elem_parse_failed = true;
			    break;
			}
			elems->ch_switch_ie = (void *)pos;
			break;
		case WLAN_EID_EXT_CHANSWITCH_ANN:
			if (elen != sizeof(struct ieee80211_ext_chansw_ie)) {
			    elem_parse_failed = true;
				break;
			}
			elems->ext_chansw_ie = (void *)pos;
				break;
		case WLAN_EID_SECONDARY_CHANNEL_OFFSET:
			if (elen != sizeof(struct ieee80211_sec_chan_offs_ie)) {
				 elem_parse_failed = true;
				 break;
			}
			elems->sec_chan_offs = (void *)pos;
			     break;
		case WLAN_EID_CHAN_SWITCH_PARAM:
			 if (elen !=
			     sizeof(*elems->mesh_chansw_params_ie)) {
				 elem_parse_failed = true;
				 break;
			 }
			 elems->mesh_chansw_params_ie = (void *)pos;
			 break;
		case WLAN_EID_WIDE_BW_CHANNEL_SWITCH:
		    if (!action ||
		        elen != sizeof(*elems->wide_bw_chansw_ie)) {
			    elem_parse_failed = true;
			    break;
			}
			elems->wide_bw_chansw_ie = (void *)pos;
			break;
		case WLAN_EID_CHANNEL_SWITCH_WRAPPER:
			if (action) {
				elem_parse_failed = true;
			    break;
			}
			/*
			 * This is a bit tricky, but as we only care about
			 * the wide bandwidth channel switch element, so
			 * just parse it out manually.
			 */
			ie = cfg80211_find_ie(WLAN_EID_WIDE_BW_CHANNEL_SWITCH, pos, elen);
			if (ie) {
				if (ie[1] == sizeof(*elems->wide_bw_chansw_ie))
					elems->wide_bw_chansw_ie = (void *)(ie + 2);
				else
				    elem_parse_failed = true;
			}
            break;
		case WLAN_EID_COUNTRY:
			elems->country_elem = pos;
			elems->country_elem_len = elen;
			break;
		case WLAN_EID_PWR_CONSTRAINT:
			if (elen != 1) {
				elem_parse_failed = true;
				break;
			}
			elems->pwr_constr_elem = pos;
			break;
		case WLAN_EID_TIMEOUT_INTERVAL:
			if (elen >= sizeof(struct ieee80211_timeout_interval_ie))
			    elems->timeout_int = (void *)pos;
			else
				elem_parse_failed = true;
			break;
		default:
		     break;
		}
		
		if (elem_parse_failed)
		    elems->parse_error = true;
		else
			__set_bit(id, seen_elems);
		left -= elen;
		pos += elen;
	}
	
	if (left != 0)
	     elems->parse_error = true;
	
	return crc;
}






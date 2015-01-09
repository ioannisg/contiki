/*
 * ar9170_mac.c
 *
 * Created: 2/8/2014 1:49:18 PM
 *  Author: Ioannis Glaropoulos
 */ 

#include "ieee80211\mac80211.h"
#include "compiler.h"
#include "ieee80211\cfg80211.h"
#include "ar9170_wlan.h"
#include "ieee80211\nl80211.h"
#include "ar9170_cmd.h"
#include "ar9170_hw.h"
#include "ar9170.h"
#include <sys\errno.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


static le32_t 
get_unaligned_le32( const uint8_t* param )
{
	le32_t return_value;
	memcpy(&return_value,param,4);
	return return_value;
}

static le32_t 
get_unaligned_le16( const uint8_t* param )
{
	le32_t return_value;
	memset(&return_value,0,4);
	memcpy(&return_value,param,2);
	return return_value;
}
int 
ar9170_set_dyn_sifs_ack(struct ar9170 *ar)
{
	uint32_t val;
	
	if (conf_is_ht40(&ar->hw->conf))
		val = 0x010a;
	else {
		if (ar->hw->conf.chandef.chan->band == IEEE80211_BAND_2GHZ)
			val = 0x105;
		else
			val = 0x104;
	}
	
	return ar9170_write_reg(ar, AR9170_MAC_REG_DYNAMIC_SIFS_ACK, val);
}

int 
ar9170_set_rts_cts_rate(struct ar9170 *ar)
{
	uint32_t rts_rate, cts_rate;
	
	if (conf_is_ht(&ar->hw->conf)) {
		/* 12 mbit OFDM */
		rts_rate = 0x1da;
		cts_rate = 0x10a;
	} else {
		if (ar->hw->conf.chandef.chan->band == IEEE80211_BAND_2GHZ) {
			/* 11 mbit CCK */
			rts_rate = 033;
			cts_rate = 003;
		} else {
			/* 6 mbit OFDM */
			rts_rate = 0x1bb;
			cts_rate = 0x10b;
		}
	}
	
	return ar9170_write_reg(ar, AR9170_MAC_REG_RTS_CTS_RATE,
				rts_rate | (cts_rate) << 16);
}

int 
ar9170_set_slot_time(struct ar9170 *ar)
{
	struct ieee80211_vif *vif;
	uint32_t slottime = 20;
	
	cpu_irq_enter_critical();
	vif = ar9170_get_main_vif(ar);
	if (!vif) {
		cpu_irq_leave_critical();
		return 0;
	}
	
	if ((ar->hw->conf.chandef.chan->band == IEEE80211_BAND_5GHZ) ||
		vif->bss_conf.use_short_slot)
	    slottime = 9;
	
	    cpu_irq_leave_critical();
	
		return ar9170_write_reg(ar, AR9170_MAC_REG_SLOT_TIME,
					slottime << 10);
}

int 
ar9170_set_mac_rates(struct ar9170 *ar)
{
	struct ieee80211_vif *vif;
	uint32_t basic, mandatory;
	
	cpu_irq_enter_critical();
	vif = ar9170_get_main_vif(ar);
	
	if (!vif) {
		cpu_irq_leave_critical();
		return 0;
	}
	
	basic = (vif->bss_conf.basic_rates & 0xf);
	basic |= (vif->bss_conf.basic_rates & 0xff0) << 4;
	cpu_irq_leave_critical();
	
	if (ar->hw->conf.chandef.chan->band == IEEE80211_BAND_5GHZ)
		mandatory = 0xff00; /* OFDM 6/9/12/18/24/36/48/54 */
	else
		mandatory = 0xff0f; /* OFDM (6/9../54) + CCK (1/2/5.5/11) */
	
	ar9170_regwrite_begin(ar);
	ar9170_regwrite(AR9170_MAC_REG_BASIC_RATE, basic);
	ar9170_regwrite(AR9170_MAC_REG_MANDATORY_RATE, mandatory);
	ar9170_regwrite_finish();
	
	return ar9170_regwrite_result();
}

int 
ar9170_set_qos(struct ar9170 *ar)
{
	ar9170_regwrite_begin(ar);
	
	ar9170_regwrite(AR9170_MAC_REG_AC0_CW, ar->edcf[0].cw_min |
	      (ar->edcf[0].cw_max << 16));
	ar9170_regwrite(AR9170_MAC_REG_AC1_CW, ar->edcf[1].cw_min |
	      (ar->edcf[1].cw_max << 16));
	ar9170_regwrite(AR9170_MAC_REG_AC2_CW, ar->edcf[2].cw_min |
	      (ar->edcf[2].cw_max << 16));
	ar9170_regwrite(AR9170_MAC_REG_AC3_CW, ar->edcf[3].cw_min |
	     (ar->edcf[3].cw_max << 16));
	ar9170_regwrite(AR9170_MAC_REG_AC4_CW, ar->edcf[4].cw_min |
	     (ar->edcf[4].cw_max << 16));
	
	ar9170_regwrite(AR9170_MAC_REG_AC2_AC1_AC0_AIFS,
	    ((ar->edcf[0].aifs * 9 + 10)) |
	    ((ar->edcf[1].aifs * 9 + 10) << 12) |
		((ar->edcf[2].aifs * 9 + 10) << 24));
	ar9170_regwrite(AR9170_MAC_REG_AC4_AC3_AC2_AIFS,
	    ((ar->edcf[2].aifs * 9 + 10) >> 8) |
	    ((ar->edcf[3].aifs * 9 + 10) << 4) |
		((ar->edcf[4].aifs * 9 + 10) << 16));
	
	ar9170_regwrite(AR9170_MAC_REG_AC1_AC0_TXOP,
		ar->edcf[0].txop | ar->edcf[1].txop << 16);
	ar9170_regwrite(AR9170_MAC_REG_AC3_AC2_TXOP,
		ar->edcf[2].txop | ar->edcf[3].txop << 16 |
		ar->edcf[4].txop << 24);
	
	ar9170_regwrite_finish();
	
	return ar9170_regwrite_result();
}

 
int 
ar9170_init_mac(struct ar9170 *ar)
{
	PRINTF("AR9170_MAC: init-mac\n");
	ar9170_regwrite_begin(ar);
	
	/* switch MAC to OTUS interface */
	ar9170_regwrite(0x1c3600, 0x3);
	
	ar9170_regwrite(AR9170_MAC_REG_ACK_EXTENSION, 0x40);
	
	ar9170_regwrite(AR9170_MAC_REG_RETRY_MAX, 0x0);
	
	ar9170_regwrite(AR9170_MAC_REG_FRAMETYPE_FILTER, AR9170_MAC_FTF_MONITOR);
	
	/* enable MMIC */
	ar9170_regwrite(AR9170_MAC_REG_SNIFFER, AR9170_MAC_SNIFFER_DEFAULTS);
	
	ar9170_regwrite(AR9170_MAC_REG_RX_THRESHOLD, 0xc1f80);
	
	ar9170_regwrite(AR9170_MAC_REG_RX_PE_DELAY, 0x70);
	ar9170_regwrite(AR9170_MAC_REG_EIFS_AND_SIFS, 0xa144000);
	ar9170_regwrite(AR9170_MAC_REG_SLOT_TIME, 9 << 10);
	
	/* CF-END & CF-ACK rate => 24M OFDM */
	ar9170_regwrite(AR9170_MAC_REG_TID_CFACK_CFEND_RATE, 0x59900000);
	
	/* NAV protects ACK only (in TXOP) */
	ar9170_regwrite(AR9170_MAC_REG_TXOP_DURATION, 0x201);
	
	/* Set Beacon PHY CTRL's TPC to 0x7, TA1=1 */
	/* OTUS set AM to 0x1 */
	ar9170_regwrite(AR9170_MAC_REG_BCN_HT1, 0x8000170);
	
	ar9170_regwrite(AR9170_MAC_REG_BACKOFF_PROTECT, 0x105);
	
	/* Aggregation MAX number and timeout */
	ar9170_regwrite(AR9170_MAC_REG_AMPDU_FACTOR, 0x8000a);
	ar9170_regwrite(AR9170_MAC_REG_AMPDU_DENSITY, 0x140a07);
	
	ar9170_regwrite(AR9170_MAC_REG_FRAMETYPE_FILTER,
	                           AR9170_MAC_FTF_DEFAULTS);
	
	ar9170_regwrite(AR9170_MAC_REG_RX_CONTROL,
	    AR9170_MAC_RX_CTRL_DEAGG |
		AR9170_MAC_RX_CTRL_SHORT_FILTER);
	
	/* rate sets */
	ar9170_regwrite(AR9170_MAC_REG_BASIC_RATE, 0x150f);
	ar9170_regwrite(AR9170_MAC_REG_MANDATORY_RATE, 0x150f);
	ar9170_regwrite(AR9170_MAC_REG_RTS_CTS_RATE, 0x0030033);
	
	/* MIMO response control */
	ar9170_regwrite(AR9170_MAC_REG_ACK_TPC, 0x4003c1e);
	
	ar9170_regwrite(AR9170_MAC_REG_AMPDU_RX_THRESH, 0xffff);
	
	/* set PHY register read timeout (??) */
	ar9170_regwrite(AR9170_MAC_REG_MISC_680, 0xf00008);
	
	/* Disable Rx TimeOut, workaround for BB. */
	ar9170_regwrite(AR9170_MAC_REG_RX_TIMEOUT, 0x0);
	
	/* Set WLAN DMA interrupt mode: generate int per packet */
	ar9170_regwrite(AR9170_MAC_REG_TXRX_MPI, 0x110011);
	
	ar9170_regwrite(AR9170_MAC_REG_FCS_SELECT,
	                         AR9170_MAC_FCS_FIFO_PROT);
	
	/* Disables the CF_END frame, undocumented register */
	ar9170_regwrite(AR9170_MAC_REG_TXOP_NOT_ENOUGH_IND,0x141e0f48);
	
	/* reset group hash table */
	ar9170_regwrite(AR9170_MAC_REG_GROUP_HASH_TBL_L, 0xffffffff);
	ar9170_regwrite(AR9170_MAC_REG_GROUP_HASH_TBL_H, 0xffffffff);
	
	/* disable PRETBTT interrupt */
	ar9170_regwrite(AR9170_MAC_REG_PRETBTT, 0x0);
	ar9170_regwrite(AR9170_MAC_REG_BCN_PERIOD, 0x0);
	
	ar9170_regwrite_finish();
	
	PRINTF("AR9170_MAC: init-mac end\n");
	
	return ar9170_regwrite_result();
}

static int 
ar9170_set_mac_reg(struct ar9170 *ar, const uint32_t reg, const uint8_t *mac)
{
	static const uint8_t zero[ETH_ALEN] = { 0 };
	
	if (!mac)
		mac = zero;
	
	ar9170_regwrite_begin(ar);
	
	ar9170_regwrite(reg, get_unaligned_le32(mac));
	ar9170_regwrite(reg + 4, get_unaligned_le16(mac + 4));
	
	ar9170_regwrite_finish();
	
	return ar9170_regwrite_result();
}

int 
ar9170_mod_virtual_mac(struct ar9170 *ar, const unsigned int id,
	const uint8_t *mac)
{
	if (id >= ar->fw.vif_num) {
		PRINTF("AR9170_MAC: vif id larger than allowed\n");
		return -EINVAL;
	}
	
	return ar9170_set_mac_reg(ar, AR9170_MAC_REG_ACK_TABLE + (id - 1) * 8, mac);
}

int 
ar9170_update_multicast(struct ar9170 *ar, const uint64_t mc_hash)
{
	int err;
	
	ar9170_regwrite_begin(ar);
	ar9170_regwrite(AR9170_MAC_REG_GROUP_HASH_TBL_H, mc_hash >> 32);
	ar9170_regwrite(AR9170_MAC_REG_GROUP_HASH_TBL_L, mc_hash);
	ar9170_regwrite_finish();
	err = ar9170_regwrite_result();
	if (err)
		return err;
	
	ar->cur_mc_hash = mc_hash;
	return 0;
}

int 
ar9170_update_multicast_mine(struct ar9170 *ar, const uint32_t mc_hash_high, const U32 mc_hash_low)
{
	int err;
	
	ar9170_regwrite_begin(ar);
	ar9170_regwrite(AR9170_MAC_REG_GROUP_HASH_TBL_H, mc_hash_high);
	ar9170_regwrite(AR9170_MAC_REG_GROUP_HASH_TBL_L, mc_hash_low);
	ar9170_regwrite_finish();
	err = ar9170_regwrite_result();
	if (err)
	return err;

	ar->cur_mc_hash = 0xc00001001;
	
	return 0;
}

int 
ar9170_set_operating_mode(struct ar9170 *ar)
{
	PRINTF("AR9170_MAC: set-operating-mode\n");
	struct ieee80211_vif *vif;
	struct ath_common *common = &ar->common;
	uint8_t *mac_addr, *bssid;
	uint32_t cam_mode = AR9170_MAC_CAM_DEFAULTS;
	uint32_t enc_mode = AR9170_MAC_ENCRYPTION_DEFAULTS |
	                 AR9170_MAC_ENCRYPTION_MGMT_RX_SOFTWARE;
	uint32_t rx_ctrl = AR9170_MAC_RX_CTRL_DEAGG |
	                 AR9170_MAC_RX_CTRL_SHORT_FILTER;
	uint32_t sniffer = AR9170_MAC_SNIFFER_DEFAULTS;
	int err = 0;
	
	cpu_irq_enter_critical();
	vif = ar9170_get_main_vif(ar);
	
	if (vif) {
		mac_addr = common->macaddr;
		bssid = common->curbssid;

		switch (vif->type) {
		case NL80211_IFTYPE_ADHOC:
			cam_mode |= AR9170_MAC_CAM_IBSS;
			PRINTF("AR9170_MAC: dev is IBSS\n");
			break;
		case NL80211_IFTYPE_MESH_POINT:
		case NL80211_IFTYPE_AP:
			cam_mode |= AR9170_MAC_CAM_AP;
			
			/* iwlagn 802.11n STA Workaround */
			rx_ctrl |= AR9170_MAC_RX_CTRL_PASS_TO_HOST;
			break;
		case NL80211_IFTYPE_WDS:
			cam_mode |= AR9170_MAC_CAM_AP_WDS;
			rx_ctrl |= AR9170_MAC_RX_CTRL_PASS_TO_HOST;
			break;
		case NL80211_IFTYPE_STATION:
			cam_mode |= AR9170_MAC_CAM_STA;
			rx_ctrl |= AR9170_MAC_RX_CTRL_PASS_TO_HOST;
			break;
		default:
			PRINTF(1, "Unsupported operation mode %x\n", vif->type);
			err = -EOPNOTSUPP;
			break;
		}
	} else {
			PRINTF("AR9170_MAC: set-operating-mode null vif\n");
		/*
		 * Enable monitor mode
		 *
		 * rx_ctrl |= AR9170_MAC_RX_CTRL_ACK_IN_SNIFFER;
		 * sniffer |= AR9170_MAC_SNIFFER_ENABLE_PROMISC;
		 *
		 * When the hardware is in SNIFFER_PROMISC mode,
		 * it generates spurious ACKs for every incoming
		 * frame. This confuses every peer in the
		 * vicinity and the network throughput will suffer
		 * badly.
		 *
		 * Hence, the hardware will be put into station
		 * mode and just the rx filters are disabled.
		 */
		cam_mode |= AR9170_MAC_CAM_STA;
		rx_ctrl |= AR9170_MAC_RX_CTRL_PASS_TO_HOST;
		mac_addr = common->macaddr;
		bssid = NULL;
	}
	cpu_irq_leave_critical();
	
	if (err)
		return err;
	
	if (ar->rx_software_decryption)
		enc_mode |= AR9170_MAC_ENCRYPTION_RX_SOFTWARE;
	
	if (ar->sniffer_enabled) {
		enc_mode |= AR9170_MAC_ENCRYPTION_RX_SOFTWARE;
	}
	
	err = ar9170_set_mac_reg(ar, AR9170_MAC_REG_MAC_ADDR_L, mac_addr);
	if (err)
		return err;
	
	err = ar9170_set_mac_reg(ar, AR9170_MAC_REG_BSSID_L, bssid);
	if (err)
		return err;
	
	ar9170_regwrite_begin(ar);
	ar9170_regwrite(AR9170_MAC_REG_SNIFFER, sniffer);
	ar9170_regwrite(AR9170_MAC_REG_CAM_MODE, cam_mode);
	ar9170_regwrite(AR9170_MAC_REG_ENCRYPTION, enc_mode);
	ar9170_regwrite(AR9170_MAC_REG_RX_CONTROL, rx_ctrl);
	ar9170_regwrite_finish();

	return ar9170_regwrite_result();
}

int 
ar9170_set_hwretry_limit(struct ar9170 *ar, const unsigned int max_retry)
{
	uint32_t tmp = min(0x33333, max_retry * 0x11111);
	
	return ar9170_write_reg(ar, AR9170_MAC_REG_RETRY_MAX, tmp);
}

int 
ar9170_set_beacon_timers(struct ar9170 *ar)
{
	PRINTF("AR9170_MAC: set-bcn-timers\n");
	
	struct ieee80211_vif *vif;
	uint32_t v = 0;
	uint32_t pretbtt = 0;
	
	cpu_irq_enter_critical();
	vif = ar9170_get_main_vif(ar);
	
	if (vif) {
		struct ar9170_vif_info *mvif;
		mvif = ar->vif_info; // XXX (void *) vif->drv_priv;
		mvif->enable_beacon = 1; // XXX 
		ar->beacon_enabled = 1; // XXX XXX should move to config	
		if (!ar->beacon_enabled) 
			PRINTF("AR9170_MAC: beacon not enabled\n");
		
		if (mvif->enable_beacon && (ar->beacon_enabled)) {
			ar->global_beacon_int = vif->bss_conf.beacon_int /
				ar->beacon_enabled;
			
			SET_VAL(AR9170_MAC_BCN_DTIM, v, vif->bss_conf.dtim_period);
			
			switch (vif->type) {
				case NL80211_IFTYPE_MESH_POINT:
				case NL80211_IFTYPE_ADHOC:
					v |= AR9170_MAC_BCN_IBSS_MODE;
					PRINTF("AR9170_MAC: ibss-mode\n");
				    break;
				case NL80211_IFTYPE_AP:
				    v |= AR9170_MAC_BCN_AP_MODE;
				    break;
				default:
				    PRINTF("AR9170_MAC: default statement reached\n");
				    break;
			}
		} else if (vif->type == NL80211_IFTYPE_STATION) {
			ar->global_beacon_int = vif->bss_conf.beacon_int;
			
			SET_VAL(AR9170_MAC_BCN_DTIM, v,
			        ar->hw->conf.ps_dtim_period);
			
			v |= AR9170_MAC_BCN_STA_PS | AR9170_MAC_BCN_PWR_MGT;
		}
		
		if (ar->global_beacon_int) {
		    if (ar->global_beacon_int < 15) {
				cpu_irq_leave_critical();
				return -ERANGE;
			}
			
			ar->global_pretbtt = ar->global_beacon_int - (AR9170_PRETBTT_KUS);
		} else {
			ar->global_pretbtt = 0;
		}
	} else {
		ar->global_beacon_int = 0;
		ar->global_pretbtt = 0;
	}
	
	cpu_irq_leave_critical();
	/* Set the ATIM window interval as well */
	//uint32_t a = 0;
	//SET_VAL(AR9170_MAC_ATIM_PERIOD, a, vif->bss_conf.atim_window);
	
	SET_VAL(AR9170_MAC_BCN_PERIOD, v, ar->global_beacon_int);
	SET_VAL(AR9170_MAC_PRETBTT, pretbtt, ar->global_pretbtt);
	SET_VAL(AR9170_MAC_PRETBTT2, pretbtt, ar->global_pretbtt);
	
	ar9170_regwrite_begin(ar);
	ar9170_regwrite(AR9170_MAC_REG_PRETBTT, pretbtt);
	ar9170_regwrite(AR9170_MAC_REG_BCN_PERIOD, v);
	//ar9170_regwrite(AR9170_MAC_REG_ATIM_WINDOW, a);
	ar9170_regwrite_finish();
	return ar9170_regwrite_result();
}

int 
ar9170_upload_key(struct ar9170 *ar, const uint8_t id, const uint8_t *mac,
                         const uint8_t ktype, const uint8_t keyidx, const uint8_t *keydata,
                         const int keylen)
{
	struct ar9170_set_key_cmd key = { };
	static const uint8_t bcast[ETH_ALEN] = {
	     0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	
	mac = mac ? : bcast;
	
	key.user = cpu_to_le16(id);
	key.keyId = cpu_to_le16(keyidx);
	key.type = cpu_to_le16(ktype);
	memcpy(&key.macAddr, mac, ETH_ALEN);
	if (keydata)
	   memcpy(&key.key, keydata, keylen);
	
	return ar9170_exec_cmd(ar, AR9170_CMD_EKEY,
	    sizeof(key), (uint8_t *)&key, 0, NULL);
}


int 
ar9170_disable_key(struct ar9170 *ar, const uint8_t id)
{
	struct ar9170_disable_key_cmd key = { };
	
	key.user = cpu_to_le16(id);
	
	return ar9170_exec_cmd(ar, AR9170_CMD_DKEY,
	   sizeof(key), (uint8_t *)&key, 0, NULL);
}

int 
ar9170_set_mac_tpc(struct ar9170 *ar, struct ieee80211_channel *channel)
{
	unsigned int power, chains;
	 
	 if (ar->eeprom.tx_mask != 1)
	    chains = AR9170_TX_PHY_TXCHAIN_2;
	 else
		chains = AR9170_TX_PHY_TXCHAIN_1;
	 
	 switch (channel->band) {
		 case IEEE80211_BAND_2GHZ:
		    power = ar->power_2G_ofdm[0] & 0x3f;
		    break;
		 case IEEE80211_BAND_5GHZ:
		    power = ar->power_5G_leg[0] & 0x3f;
		    break;
		default:
			PRINTF("AR9170_MAC: set-power; default statement reached\n");
	 }
	 
	 power = min(power, ar->hw->conf.power_level * 2);
	 
	 ar9170_regwrite_begin(ar);
	 ar9170_regwrite(AR9170_MAC_REG_ACK_TPC, 0x3c1e | power << 20 | chains << 26);
	 ar9170_regwrite(AR9170_MAC_REG_RTS_CTS_TPC, 
		power << 5 | chains << 11 |
	    power << 21 | chains << 27);
	 ar9170_regwrite(AR9170_MAC_REG_CFEND_QOSNULL_TPC,
		power << 5 | chains << 11 |
		power << 21 | chains << 27);
	 ar9170_regwrite_finish();
	 return ar9170_regwrite_result();
 }

 
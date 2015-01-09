/*
 * ar9170_main.c
 *
 * Created: 2/1/2014 6:56:15 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ar9170.h"
#include "ar9170-drv.h"
#include "ar9170_eeprom.h"
#include "ar9170_hw.h"
#include "ar9170_fwcmd.h"
#include "ar9170_cmd.h"
#include "ath.h"

#include "ieee80211\cfg80211.h"
#include "ieee80211\ieee80211_ibss.h"
#include "ieee80211\mac80211.h"

#include <math.h>
#include <sys\errno.h>

#include "process.h"
#include "rtimer.h"
#include "list.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#include "platform-conf.h"
#if WITH_WIFI_SUPPORT

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define hweight8(w)             \
	( (!!((w) & (1ULL << 0))) +       \
    (!!((w) & (1ULL << 1))) +       \
    (!!((w) & (1ULL << 2))) +       \
    (!!((w) & (1ULL << 3))) +       \
    (!!((w) & (1ULL << 4))) +       \
    (!!((w) & (1ULL << 5))) +       \
    (!!((w) & (1ULL << 6))) +       \
    (!!((w) & (1ULL << 7))) )
	
#define RATE(_bitrate, _hw_rate, _txpidx, _flags) {     \
	        .bitrate        = (_bitrate),                   \
	        .flags          = (_flags),                     \
	        .hw_value       = (_hw_rate) | (_txpidx) << 4,  \
}

struct ieee80211_rate __carl9170_ratetable[] = {
	         RATE(10, 0, 0, 0),
	         RATE(20, 1, 1, IEEE80211_RATE_SHORT_PREAMBLE),
	         RATE(55, 2, 2, IEEE80211_RATE_SHORT_PREAMBLE),
	         RATE(110, 3, 3, IEEE80211_RATE_SHORT_PREAMBLE),
	         RATE(60, 0xb, 0, 0),
	         RATE(90, 0xf, 0, 0),
	         RATE(120, 0xa, 0, 0),
	         RATE(180, 0xe, 0, 0),
	         RATE(240, 0x9, 0, 0),
	         RATE(360, 0xd, 1, 0),
	         RATE(480, 0x8, 2, 0),
	         RATE(540, 0xc, 3, 0),
};
#undef RATE

#define carl9170_g_ratetable    (__carl9170_ratetable + 0)
#define carl9170_g_ratetable_size       12
#define carl9170_a_ratetable    (__carl9170_ratetable + 4)
#define carl9170_a_ratetable_size		8


/*
 * NB: The hw_value is used as an index into the carl9170_phy_freq_params
 *     array in phy.c so that we don't have to do frequency lookups!
 */
#define CHAN(_freq, _idx) {             \
     .center_freq    = (_freq),      \
     .hw_value       = (_idx),       \
     .max_power      = 18, /* XXX */ \
}
 
static struct ieee80211_channel carl9170_2ghz_chantable[] = {
	CHAN(2412,  0),
	CHAN(2417,  1),
    CHAN(2422,  2),
    CHAN(2427,  3),
    CHAN(2432,  4),
    CHAN(2437,  5),
    CHAN(2442,  6),
    CHAN(2447,  7),
    CHAN(2452,  8),
    CHAN(2457,  9),
    CHAN(2462, 10),
    CHAN(2467, 11),
    CHAN(2472, 12),
    CHAN(2484, 13),
};

static struct ieee80211_channel carl9170_5ghz_chantable[] = {
    CHAN(4920, 14),
    CHAN(4940, 15),
    CHAN(4960, 16),
    CHAN(4980, 17),
    CHAN(5040, 18),
    CHAN(5060, 19),
    CHAN(5080, 20),
    CHAN(5180, 21),
    CHAN(5200, 22),
    CHAN(5220, 23),
    CHAN(5240, 24),
    CHAN(5260, 25),
    CHAN(5280, 26),
    CHAN(5300, 27),
    CHAN(5320, 28),
    CHAN(5500, 29),
    CHAN(5520, 30),
    CHAN(5540, 31),
    CHAN(5560, 32),
    CHAN(5580, 33),
    CHAN(5600, 34),
    CHAN(5620, 35),
    CHAN(5640, 36),
    CHAN(5660, 37),
    CHAN(5680, 38),
    CHAN(5700, 39),
    CHAN(5745, 40),
    CHAN(5765, 41),
    CHAN(5785, 42),
    CHAN(5805, 43),
    CHAN(5825, 44),
    CHAN(5170, 45),
    CHAN(5190, 46),
    CHAN(5210, 47),
    CHAN(5230, 48),
};
#undef CHAN

#define CARL9170_HT_CAP                                                 \
{                                                                       \
	         .ht_supported   = 1,                                         \
	         .cap            = IEEE80211_HT_CAP_MAX_AMSDU |                  \
	                           IEEE80211_HT_CAP_SUP_WIDTH_20_40 |            \
	                           IEEE80211_HT_CAP_SGI_40 |                     \
	                           IEEE80211_HT_CAP_DSSSCCK40 |                  \
	                           IEEE80211_HT_CAP_SM_PS,                       \
	         .ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,                   \
	         .ampdu_density  = IEEE80211_HT_MPDU_DENSITY_8,                  \
	         .mcs            = {                                             \
		                 .rx_mask = { 0xff, 0xff, 0, 0, 0x1, 0, 0, 0, 0, 0, },   \
		                 .rx_highest = cpu_to_le16(300),                         \
		                 .tx_params = IEEE80211_HT_MCS_TX_DEFINED,               \
	        },                                                              \
}


static struct ieee80211_supported_band carl9170_band_2GHz = {
	         .channels       = carl9170_2ghz_chantable,
	         .n_channels     = ARRAY_SIZE(carl9170_2ghz_chantable),
	         .bitrates       = carl9170_g_ratetable,
	         .n_bitrates     = carl9170_g_ratetable_size,
	         .ht_cap         = CARL9170_HT_CAP,
};

static struct ieee80211_supported_band carl9170_band_5GHz = {
	         .channels       = carl9170_5ghz_chantable,
	         .n_channels     = ARRAY_SIZE(carl9170_5ghz_chantable),
	         .bitrates       = carl9170_a_ratetable,
	         .n_bitrates     = carl9170_a_ratetable_size,
	         .ht_cap         = CARL9170_HT_CAP,
};




void 
ar9170_op_bss_info_changed(struct ieee80211_hw *hw,
                           struct ieee80211_vif *vif,
                           struct ieee80211_bss_conf *bss_conf,
                           uint32_t changed)
{
	struct ar9170 *ar = hw->priv;
	struct ath_common *common = &ar->common;
	int err = 0;
	struct ar9170_vif_info *vif_priv;
	struct ieee80211_vif *main_vif;
	/* Pass the private vif info structure from AR9170 */
	vif_priv = ar->vif_info;
	main_vif = ar9170_get_main_vif(ar);
	if (!main_vif) {
		PRINTF("AR9170_MAIN: op-bss-info-changed vif is null\n");
		goto out;
	}
	if (changed & BSS_CHANGED_BEACON_ENABLED) {
		PRINTF("AR9170_MAIN: bss-changed-bcn-enabled\n");
		vif_priv->enable_beacon = bss_conf->enable_beacon;
		
		ar->beacon_enabled = 1;
	}	
	
	if (changed & BSS_CHANGED_BEACON) {
		err = ar9170_update_beacon(ar, false);
		if (err)
			goto out;
	}
	
	if (changed & (BSS_CHANGED_BEACON_ENABLED | 
					//BSS_CHANGED_BEACON | XXX we do this so the beacon can still be updated at join IBSS
					BSS_CHANGED_BEACON_INT)) {
		
		if (main_vif != vif) {
			bss_conf->beacon_int = main_vif->bss_conf.beacon_int;
			bss_conf->dtim_period = main_vif->bss_conf.dtim_period;
		}
		
		/*
		 * Therefore a hard limit for the broadcast traffic should
		 * prevent false alarms.
		 */
		if (vif->type != NL80211_IFTYPE_STATION &&
			(bss_conf->beacon_int * bss_conf->dtim_period >=
			(AR9170_QUEUE_STUCK_TIMEOUT / 2))) {
			err = -EINVAL;
			goto out;
		}
		
		err = ar9170_set_beacon_timers(ar);
		if (err)
			goto out;
	}
	
	if (changed & BSS_CHANGED_HT) {
		/* TODO */
		err = 0;
		if (err)
		    goto out;
	}
	
	if (main_vif != vif)
		goto out;
	
	/*
	 * The following settings can only be changed by the
	 * master interface.
	 */
	
	if (changed & BSS_CHANGED_BSSID) {
		memcpy(common->curbssid, bss_conf->bssid, ETH_ALEN);
		err = ar9170_set_operating_mode(ar);
		if (err)
			goto out;
	}
	
	if (changed & BSS_CHANGED_ASSOC) {
		ar->common.curaid = bss_conf->aid;
		err = ar9170_set_beacon_timers(ar);
		if (err)
			goto out;
	}
	
	if (changed & BSS_CHANGED_ERP_SLOT) {
		err = ar9170_set_slot_time(ar);
		if (err)
			goto out;
	}
	
	if (changed & BSS_CHANGED_BASIC_RATES) {
		err = ar9170_set_mac_rates(ar);
		if (err)
			goto out;
	}
	
out:
	if(err && IS_STARTED(ar))
		PRINTF("AR9170_MAIN: error on a started dev\n");
	
}

static uint8_t 
ar9170_tx_frames_pending(struct ieee80211_hw *hw)
{
	struct ar9170 *ar = hw->priv;
	uint8_t result;
	cpu_irq_enter_critical();
	result = list_length(ar->ar9170_tx_list);
	cpu_irq_leave_critical();
	return result;
}

uint64_t 
ar9170_op_get_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct ar9170 *ar = hw->priv;
	struct ar9170_tsf_rsp tsf;
	int err;
	
	err = ar9170_exec_cmd(ar, AR9170_CMD_READ_TSF, 0, NULL, sizeof(tsf), &tsf);
	
	if (err) {
		PRINTF("AR9170_MAIN: tsf-error\n");
		return 0;
	}
	return tsf.tsf_64;
}

static int 
ar9170_read_eeprom(struct ar9170 *ar)
{
	#define RW      8       /* number of words to read at once */
	#define RB      (sizeof(uint32_t) * RW)
	uint8_t *eeprom = (void *)&ar->eeprom;
	le32_t offsets[RW];
	int i, j, err;
	
	PRINTF("AR9170_MAIN: EEPROM size error %u\n",sizeof(ar->eeprom) & 3);
	
	PRINTF("AR9170_MAIN: RM size error %u\n",RB > AR9170_MAX_CMD_LEN - 4);
	#ifndef __CHECKER__
	/* don't want to handle trailing remains */
	PRINTF("AR9170_MAIN: EEPROM size check error %u\n",(sizeof(ar->eeprom) % RB));
	#endif
	
	for (i = 0; i < sizeof(ar->eeprom) / RB; i++) {
		for (j = 0; j < RW; j++)
			offsets[j] = cpu_to_le32(AR9170_EEPROM_START + RB * i + 4 * j);
		
		err = ar9170_exec_cmd(ar, AR9170_CMD_RREG, RB, (uint8_t *) &offsets,RB, eeprom + RB * i);
		if (err)
			return err;
	}
	
	#undef RW
	#undef RB
	return 0;
}

static int 
ar9170_parse_eeprom(struct ar9170 *ar)
{
	struct ath_regulatory *regulatory = &ar->common.regulatory;
	unsigned int rx_streams, tx_streams, tx_params = 0;
	int bands = 0;
	int chans = 0;
	
	if (ar->eeprom.length == cpu_to_le16(0xffff))
		return -ENODATA;
	
	rx_streams = hweight8(ar->eeprom.rx_mask);
	tx_streams = hweight8(ar->eeprom.tx_mask);
	
	if (rx_streams != tx_streams) {
		tx_params = IEEE80211_HT_MCS_TX_RX_DIFF;
		
		if(!(tx_streams >= 1 && tx_streams <=
			IEEE80211_HT_MCS_TX_MAX_STREAMS)) {
				PRINTF("AR9170_MAIN: tx-stream out of bounds\n");
		}
		
		tx_params = (tx_streams - 1) <<
			IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT;
		
		carl9170_band_2GHz.ht_cap.mcs.tx_params |= tx_params;
		carl9170_band_5GHz.ht_cap.mcs.tx_params |= tx_params;
	}
	
	if (ar->eeprom.operating_flags & AR9170_OPFLAG_2GHZ) {
		ar->hw->wiphy->bands[IEEE80211_BAND_2GHZ] =
			&carl9170_band_2GHz;
        chans += carl9170_band_2GHz.n_channels;
		bands++;
	}
	if (ar->eeprom.operating_flags & AR9170_OPFLAG_5GHZ) {
			ar->hw->wiphy->bands[IEEE80211_BAND_5GHZ] =
			&carl9170_band_5GHz;
		chans += carl9170_band_5GHz.n_channels;
		bands++;
	}
	
	if (!bands)
		return -EINVAL;
		
	cpu_irq_enter_critical();
	ar->survey = malloc(sizeof(struct survey_info) * chans);
	cpu_irq_leave_critical();
	if (!ar->survey)
	    return -ENOMEM;
	ar->num_channels = chans;
	
	         /*
	          * I measured this, a bandswitch takes roughly
	          * 135 ms and a frequency switch about 80.
	          *
	          * FIXME: measure these values again once EEPROM settings
	          *        are used, that will influence them!
	          */
	if (bands == 2)
	        ar->hw->channel_change_time = 135 * 1000;
	else
	        ar->hw->channel_change_time = 80 * 1000;
	
	regulatory->current_rd = le16_to_cpu(ar->eeprom.reg_domain[0]);
	
	/* second part of wiphy init */
	//SET_IEEE80211_PERM_ADDR(ar->hw, ar->eeprom.mac_address);
	memcpy(&ar->vif->addr[0], ar->eeprom.mac_address, ETH_ALEN);
	PRINTF("AR9170_MAIN: MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
		ar->eeprom.mac_address[0],
		ar->eeprom.mac_address[1],
		ar->eeprom.mac_address[2],
		ar->eeprom.mac_address[3],
		ar->eeprom.mac_address[4],
		ar->eeprom.mac_address[5]);
	return 0;
}

static int
ar9170_init_interface(struct ar9170 *ar, struct ieee80211_vif *vif)
{
	struct ath_common *common = &ar->common;
	int err;

	PRINTF("AR9170_MAIN: init-iface\n");
	
    if (!vif) {
		if(IS_STARTED(ar)) {
			PRINTF("AR9170_MAIN: interface zero but device is started\n");
		}
        return 0;
	}

	memcpy(common->macaddr, vif->addr, ETH_ALEN);

	/* We have to fall back to software crypto, whenever
     * the user choose to participates in an IBSS. HW
     * offload for IBSS RSN is not supported by this driver.
     *
     * NOTE: If the previous main interface has already
     * disabled hw crypto offload, we have to keep this
     * previous disable_offload setting as it was.
     * Although ideally, we should notify mac80211 and tell
     * it to forget about any HW crypto offload for now.
	 */
	ar->disable_offload |= ((vif->type != NL80211_IFTYPE_STATION) &&
		(vif->type != NL80211_IFTYPE_AP));
 
   /* While the driver supports HW offload in a single
    * P2P client configuration, it doesn't support HW
    * offload in the favourite, concurrent P2P GO+CLIENT
    * configuration. Hence, HW offload will always be
    * disabled for P2P.
    */
    ar->disable_offload |= vif->p2p;

    ar->rx_software_decryption = ar->disable_offload;
 
    err = ar9170_set_operating_mode(ar);
	PRINTF("AR9170_MAIN: iface initialized\n");
    return err;
}


#if 0
static void 
ar9170_zap_queues(struct ar9170 *ar)
{
	struct ar9170_vif_info *cvif;
	unsigned int i;
	
	//carl9170_ampdu_gc(ar);
	
	//carl9170_flush_ba(ar);
	//carl9170_flush(ar, true);
	
	for (i = 0; i < ar->hw->queues; i++) {
		
		while (!skb_queue_empty(&ar->tx_status[i])) {
			struct sk_buff *skb;
			
			skb = skb_peek(&ar->tx_status[i]);
			carl9170_tx_get_skb(skb);
			
			carl9170_tx_drop(ar, skb);
			
			carl9170_tx_put_skb(skb);
		}
		
	}
	
	if(AR9170_NUM_TX_LIMIT_SOFT < 1) {
		PRINTF("AR9170_MAIN: SOFT limit less than 1\n");
	}
	if(AR9170_NUM_TX_LIMIT_HARD < AR9170_NUM_TX_LIMIT_SOFT) {
		PRINTF("AR9170_MAIN: HARD less than soft\n");
	}
	if(AR9170_NUM_TX_LIMIT_HARD >= AR9170_BAW_BITS) {
		PRINTF("AR9170_MAIN: HADR more than baw bits\n");
	}
	
	/* reinitialize queues statistics */
	memset(&ar->tx_stats, 0, sizeof(ar->tx_stats));
	for (i = 0; i < ar->hw->queues; i++)
		ar->tx_stats[i].limit = AR9170_NUM_TX_LIMIT_HARD;
	
	for (i = 0; i < DIV_ROUND_UP(ar->fw.mem_blocks, BITS_PER_LONG); i++)
		ar->mem_bitmap[i] = 0;
	
	cpu_irq_enter_critical();
	
	list_for_each_entry_rcu(cvif, &ar->vif_list, list) {
		pin_lock_bh(&ar->beacon_lock);
		dev_kfree_skb_any(cvif->beacon);
		cvif->beacon = NULL;
	    spin_unlock_bh(&ar->beacon_lock);
	}
	
	
	ar->tx_ampdu_upload = 0;
	ar->tx_ampdu_scheduler = 0;
	ar->tx_total_pending= 0;
	ar->tx_total_queued = 0;
	ar->mem_free_blocks = ar->fw.mem_blocks;
	cpu_irq_leave_critical();
}
#endif


#define CARL9170_FILL_QUEUE(queue, ai_fs, cwmin, cwmax, _txop)          \
do {                                                                    \
	queue.aifs = ai_fs;                                             \
	queue.cw_min = cwmin;                                           \
	queue.cw_max = cwmax;                                           \
	queue.txop = _txop;                                             \
} while (0)


struct ieee80211_vif*
ar9170_get_main_vif(struct ar9170 *ar)
{
	return ar->vif;
}

struct ieee80211_hw*
ar9170_get_main_iee80211_hw(struct ar9170 *ar)
{
	return ar->hw;
}


static uint8_t 
ar9170_alloc(struct ar9170* ar)
{	
	/* The structures are already initialized in the IBSS init process
	 * So we pass the IEEE802.11 structure references to the ar9170 device 
	 */ 	
	ar->hw = &ieee80211_ibss_hw;
	ar->hw->wiphy = &ibss_attached_wiphy;	
	ar->vif = &ieee80211_ibss_vif; 
	ar->vif_info = &ibss_ar9170_cvif;
	/* Also the hw has a reference to the driver structure */
	ar->hw->priv = ar;
	
	return 0;
}

int 
ar9170_register(struct ar9170 *ar)
{

	//struct ath_regulatory *regulatory = &ar->common.regulatory;
	int err = 0, i;
	
	if (ar->mem_bitmap) {
		PRINTF("AR9170_MAIN: mem_bitmap non-null\n");
		return -EINVAL;
	}
	cpu_irq_enter_critical();
	ar->mem_bitmap = malloc(DIV_ROUND_UP(ar->fw.mem_blocks, BITS_PER_LONG) * sizeof(unsigned long));
	cpu_irq_leave_critical();
	if (!ar->mem_bitmap)
		return -ENOMEM;
	
	/* try to read EEPROM, init MAC addr */
	err = ar9170_read_eeprom(ar);
	if (err)
		return err;
	PRINTF("AR9170_MAIN: EEPROM read\n");	

	err = ar9170_parse_eeprom(ar);
	if (err)
		return err;
#if 0		
	err = ath_regd_init(regulatory, ar->hw->wiphy,
		carl9170_reg_notifier);
	if (err)
		return err;
	
	if (modparam_noht) {
		carl9170_band_2GHz.ht_cap.ht_supported = false;
		carl9170_band_5GHz.ht_cap.ht_supported = false;
	}
	
	for (i = 0; i < ar->fw.vif_num; i++) {
		ar->vif_priv[i].id = i;
		ar->vif_priv[i].vif = NULL;
	}
	
	err = ieee80211_register_hw(ar->hw);
	if (err)
		return err;
#endif	
	/* mac80211 interface is now registered */
	ar->registered = 1;
#if 0	
	2045         if (!ath_is_world_regd(regulatory))
	2046                 regulatory_hint(ar->hw->wiphy, regulatory->alpha2);
#endif 
#ifdef CONFIG_CARL9170_DEBUGFS
	ar9170_debugfs_register(ar);
#endif /* CONFIG_CARL9170_DEBUGFS */

	err = ar9170_led_init(ar);
	if (err)
		goto err_unreg;

#ifdef CONFIG_CARL9170_LEDS
	err = carl9170_led_register(ar);
	if (err)
		goto err_unreg;
#endif /* CONFIG_CARL9170_LEDS */
	
#ifdef CONFIG_CARL9170_WPC
	err = carl9170_register_wps_button(ar);
	if (err)
		goto err_unreg;
#endif /* CONFIG_CARL9170_WPC */
	
#ifdef CONFIG_CARL9170_HWRNG
	err = carl9170_register_hwrng(ar);
	if (err)
		goto err_unreg;
#endif /* CONFIG_CARL9170_HWRNG */
	
	PRINTF("Atheros AR9170 is registered as '%s'\n", "AR9170");
	return 0;

err_unreg:
	// TODO carl9170_unregister(ar);
	return err;
}

void
ar9170_set_up_interface(struct ar9170* ar, const struct firmware *fw)
{
	if(ar9170_alloc(ar)) {
		PRINTF("AR9170_MAIN: alloc-error\n");
		return;
	}
	
	/* Check the integrity of the firmware */
	if (fw == NULL) {
		PRINTF("AR9170_MAIN: Firmware broken\n");
		return;
	}
	/* Pass the firmware reference to the AR9170 device */
	ar->fw.fw = fw;
	
	/* Continue */
	ar9170_usb_firmware_finish(ar);
	
	/* If the device is installed, move to initialization */
	if(!ar->registered) {
		PRINTF("AR9170_MAIN: device could not be registered\n");
		return;		
	}
	
	ar->common.regulatory.country_code = 752 /* CTRY_SWEDEN */;
	ar->common.regulatory.regpair.reg_2ghz_ctl = 48;
	
	/* Initially the PSM state and the OFF override flags are clear */
	ar->ps.state = 0;
	ar->ps.off_override = 0;
	
	/* Start device. */
}

int
ar9170_op_start( struct ar9170* ar )
{
	struct ieee80211_hw *hw = ar->hw;
	
	int err, i;
	
	PRINTF("AR9170_MAIN: op-start\n");
	//ar9170_zap_queues(ar);
	
	/* reset QoS defaults */
	CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_VO], 2, 31, 1023,  0);
	CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_VI], 2, 31, 1023,  0);
	CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_BE], 2, 31, 1023,  0);
	CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_BK], 2, 31, 1023,  0);
	CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_SPECIAL], 2, 3, 31, 0);
	
	err = ar9170_usb_open(ar);
	if (err)
		goto out;
	
    err = ar9170_init_mac(ar);
	if (err)
	   goto out;
	
	err = ar9170_set_qos(ar);
	if (err)
	   goto out;
	
	if (ar->fw.rx_filter) {
		err = ar9170_rx_filter(ar, AR9170_RX_FILTER_OTHER_RA |
		       AR9170_RX_FILTER_CTL_OTHER | AR9170_RX_FILTER_BAD | 
			   AR9170_RX_FILTER_CTL_PSPOLL | AR9170_RX_FILTER_DECRY_FAIL |
			   AR9170_RX_FILTER_DATA | AR9170_RX_FILTER_MGMT);
		if (err)
			goto out;
	}
	
	err = ar9170_write_reg(ar, AR9170_MAC_REG_DMA_TRIGGER,
	                                  AR9170_DMA_TRIGGER_RXQ);
	if (err)
		goto out;
		
	/* Clear key-cache */
	for (i = 0; i < AR9170_CAM_MAX_USER + 4; i++) {
		err = ar9170_upload_key(ar, i, NULL, AR9170_ENC_ALG_NONE, 0, NULL, 0);
		if (err)
			goto out;
		
		err = ar9170_upload_key(ar, i, NULL, AR9170_ENC_ALG_NONE, 1, NULL, 0);
		if (err)
			goto out;
		
		if (i < AR9170_CAM_MAX_USER) {
			err = ar9170_disable_key(ar, i);
			if (err)
				goto out;
		}
	}
	/* Set device state to STARTED */
	ar9170_set_state_when(ar, AR9170_IDLE, AR9170_STARTED);
	
	return 0;	
out:	
	return err;
}

uint8_t 
ar9170_op_add_interface(struct ieee80211_hw * hw, struct ieee80211_vif * vif)
{
	PRINTF("AR9170_MAIN: add-iface\n");
	/* The main VIF */
	struct ieee80211_vif *main_vif = NULL;
	/* The HW structure contains the AR9170 reference */
	struct ar9170* ar = (struct ar9170*)(hw->priv);
	/* This should be non-NULL */
	struct ar9170_vif_info *vif_priv = ar->vif_info;
	
	int vif_id = -1, err = 0;

	/* Get the VIF from the AR9170 */
	main_vif = ar9170_get_main_vif(ar);
	
	if (vif_priv == NULL) {
		PRINTF("AR9170_MAIN: private vif info null\n");
		return -EINVAL;
	}
	
	if(main_vif != vif) {
		PRINTF("AR9170_MAIN: vifs do not match\n");
		return -EINVAL;
	} 
	
	if (main_vif->prepared) {
		PRINTF("AR9170_MAIN: vif already prepared\n");
		return -EINVAL;
	}
	/* Set the flag to prepared */
	main_vif->prepared = 1;

	/* For the moment we only support 1 interface so it has the id of zero */
	vif_id = 0;

	/* Assign the interface ID on the list of virtual interfaces of the AR9170 device */
	if(ar->vif_priv[vif_id].id != vif_id) {
		PRINTF("BUG: The virtual interface ids should match.\n");
		return -EINVAL;
	}
	
	if (vif_priv->active) {
		PRINTF("AR9170_MAIN: vif already active\n");
		return -EBUSY;
	} 
	
	/* Assign the interface ID on the list of virtual interfaces of the AR9170 device */
	ar->vif_priv[vif_id].vif = vif;

	/* VIF is now active*/
	vif_priv->active = 1;
	vif_priv->id = vif_id;
	vif_priv->enable_beacon = 0;
	/* Increment the number of VIFS registered */
	ar->vifs++;

	goto init;
	
#if 0 // XXX XXX XXX	
	if (!vif_priv) {
		/* */
		vif_priv = vif;
		vif_priv->active = 0;
	
	} else {
		/* */
		vif_priv->active = 0;
	}
	/* We should not normally enter here */
	if (vif_priv->active == 1) {
		PRINTF("DEBUG: Virtual interface info already active.\n");
		/*
		 * Skip the interface structure initialization,
		 * if the vif survived the _restart call.
		 */
		vif_id = vif_priv->id;
		vif_priv->enable_beacon = 0;

		if (vif_priv->beacon) {
			if(vif_priv->beacon->pkt_data) {
				free(vif_priv->beacon->pkt_data);
			}
			free(vif_priv->beacon);	
		}		
		vif_priv->beacon = NULL;

		goto init;
	}
	
	/* Get the VIF from the AR9170 */
	main_vif = ar9170_get_main_vif(ar);
	
	/* It should not enter here either */
	if (main_vif && main_vif->prepared) { 
		// Altered by John, to add the prepared flag
		switch (main_vif->type) {
		case NL80211_IFTYPE_STATION:
			PRINTF("AR9170_MAIN: is STA\n");
			if (vif->type == NL80211_IFTYPE_STATION)
				break;
			err = -EBUSY;
			PRINTF("AR9170_MAIN: error on vif infos for STA\n");
			goto unlock;

		case NL80211_IFTYPE_AP:
			if ((vif->type == NL80211_IFTYPE_STATION) ||
				(vif->type == NL80211_IFTYPE_WDS) ||
				(vif->type == NL80211_IFTYPE_AP))
				break;
			PRINTF("AR9170_MAIN: error on vif infos for AP\n");	
			err = -EBUSY;
			goto unlock;

		default:
			goto unlock;
		}
	}

	/* For the moment we only support 1 interface so it has the id of zero */
	vif_id = 0;
	
	/* Assign the interface ID on the list of virtual interfaces of the AR9170 device */
	if(ar->vif_priv[vif_id].id != vif_id)
		PRINTF("BUG: The virtual interface ids should match.\n");

	/* VIF is now active*/
	vif_priv->active = 1;
	
	vif_priv->id = vif_id;
	vif_priv->enable_beacon = 0;
	/* Increment the number of VIFS registered */
	ar->vifs++;
	
	//list_add_tail_rcu(&vif_priv->list, &ar->vif_list); Instead of this we do:
	//ar->vif_list.next = (struct list_head*)&vif_priv; // This is dangerous XXX
	
	/* Assign the interface ID on the list of virtual interfaces of the AR9170 device */
	ar->vif_priv[vif_id].vif = vif;
#endif
init:
	if (ar9170_get_main_vif(ar) == vif) {
		
		ar->beacon_iter = vif_priv;

		PRINTF("AR9170_MAIN: Will initialize interface.\n");
	
		err = ar9170_init_interface(ar, vif);
		if (err) {
			PRINTF("ERROR: ar9170_init_interface returned errors.\n");
			goto unlock;
		}
		
	} else {
		
		PRINTF("ERROR: Could not find the virtual interface.\n");
		//err = carl9170_mod_virtual_mac(ar, vif_id, vif->addr); TODO- add support for this, although, probably not urgent
		if (err)
			goto unlock;
	}

	if (ar->fw.tx_seq_table) {		
		PRINTF("DEBUG: Writing on the tx_seq_table: %d.\n",vif_id);
		err = ar9170_write_reg(ar, ar->fw.tx_seq_table + vif_id * 4, 0);
		if (err)
			goto unlock;
	}		
	return 0;

unlock:
	return err;
}


int 
ar9170_ps_update(struct ar9170 *ar)
{
	uint8_t ps = 0;
	int err = 0;
	rtimer_clock_t curr_time;
	
	if (!ar->ps.off_override)
		ps = (ar->hw->conf.flags & IEEE80211_CONF_PS);
	
	if (ps != ar->ps.state) {
		err = ar9170_powersave(ar, ps);
		if (err)
		   return err;
		
		curr_time = RTIMER_NOW();
		if (ar->ps.state && !ps) {
			ar->ps.sleep_ms = (1000*((curr_time - ar->ps.last_action)))/(RTIMER_SECOND);
		}
		
		if (ps)
		   ar->ps.last_slept = curr_time;
		
		ar->ps.last_action = curr_time;
		ar->ps.state = ps;
	}
	
	return 0;
}

static int 
ar9170_update_survey(struct ar9170 *ar, bool flush, bool noise)
{
	int err;
	
	if (noise) {
		err = ar9170_get_noisefloor(ar);
		if (err)
		    return err;
	}
	
	if (ar->fw.hw_counters) {
		err = ar9170_collect_tally(ar);
		if (err)
			return err;
	}
	
	if (flush)
	   memset(&ar->tally, 0, sizeof(ar->tally));
	
	return 0;
}

int 
ar9170_op_config(struct ieee80211_hw *hw, uint32_t changed)
{
	struct ar9170 *ar = hw->priv;
	int err = 0;
	
	/* John: set power constants */
	ar->power_2G_ofdm[0] = 0x22;
	ar->power_5G_leg[0] = 0x20;
	 
	if (changed & IEEE80211_CONF_CHANGE_LISTEN_INTERVAL) {
		/* TODO */
		err = 0;
	}
	
	if (changed & IEEE80211_CONF_CHANGE_PS) {
		err = ar9170_ps_update(ar);
			if (err)
			    goto out;
	}
	
	if (changed & IEEE80211_CONF_CHANGE_SMPS) {
		/* TODO */
		err = 0;
	}
	
	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		enum nl80211_channel_type channel_type =
				cfg80211_get_chandef_type(&hw->conf.chandef);
		
		/* adjust slot time for 5 GHz */
		err = ar9170_set_slot_time(ar);
		if (err)
			goto out;
		
		err = ar9170_update_survey(ar, true, false);
		if (err)
		    goto out;
		
		err = ar9170_set_channel(ar, hw->conf.chandef.chan, channel_type);
		if (err)
			goto out;
		
        err = ar9170_update_survey(ar, false, true);
		if (err)
			goto out;
		
		err = ar9170_set_dyn_sifs_ack(ar);
		if (err)
		   goto out;
		
		err = ar9170_set_rts_cts_rate(ar);
		if (err)
		    goto out;
	}
	
	if (changed & IEEE80211_CONF_CHANGE_POWER) {
		err = ar9170_set_mac_tpc(ar, ar->hw->conf.chandef.chan);
		if (err)
		   goto out;
	}
	PRINTF("AR9170_MAIN: dev configured\n");
out:
	
	return err;
}

int 
ar9170_op_conf_tx(struct ieee80211_hw *hw,
	struct ieee80211_vif *vif, uint16_t queue,
	const struct ieee80211_tx_queue_params *param)
{
	struct ar9170 *ar = hw->priv;
	int ret;
	
	if (queue < ar->hw->queues) {
		memcpy(&ar->edcf[ar9170_qmap[queue]], param, sizeof(*param));
		ret = ar9170_set_qos(ar);
	} else {
		ret = -EINVAL;
	}
	
	return ret;
}

static void 
ar9170_op_configure_filter(struct ieee80211_hw *hw, unsigned int changed_flags, 
		unsigned int *new_flags, uint64_t multicast)
{
	struct ar9170* ar = hw->priv;
	
	/* mask supported flags */
	*new_flags &= FIF_ALLMULTI | ar->rx_filter_caps;
	
	if (!IS_ACCEPTING_CMD(ar))
		return;
	
	ar->filter_state = *new_flags;
	/*
	 * We can support more by setting the sniffer bit and
	 * then checking the error flags, later.
	 */
	
	if (*new_flags & FIF_ALLMULTI)
		multicast = ~0ULL;
	
	if (multicast != ar->cur_mc_hash)
		if(ar9170_update_multicast(ar, multicast)) {
			PRINTF("AR9170_MAIN: Warning on update_multi\n");
		}
	
	if (changed_flags & (FIF_OTHER_BSS | FIF_PROMISC_IN_BSS)) {
		ar->sniffer_enabled = !!(*new_flags &
		     (FIF_OTHER_BSS | FIF_PROMISC_IN_BSS));
		
		if(ar9170_set_operating_mode(ar)) {
			PRINTF("AR9170_MAIN: set-operating-mode err\n");
		}
		
		
	}
	
	if (ar->fw.rx_filter && changed_flags & ar->rx_filter_caps) {
		uint32_t rx_filter = 0;
		
		if (!ar->fw.ba_filter)
			rx_filter |= AR9170_RX_FILTER_CTL_OTHER;
		
		if (!(*new_flags & (FIF_FCSFAIL | FIF_PLCPFAIL)))
		   rx_filter |= AR9170_RX_FILTER_BAD;
		
		if (!(*new_flags & FIF_CONTROL))
			rx_filter |= AR9170_RX_FILTER_CTL_OTHER;
		
		if (!(*new_flags & FIF_PSPOLL))
			rx_filter |= AR9170_RX_FILTER_CTL_PSPOLL;
		
		if (!(*new_flags & (FIF_OTHER_BSS | FIF_PROMISC_IN_BSS))) {
			rx_filter |= AR9170_RX_FILTER_OTHER_RA;
			rx_filter |= AR9170_RX_FILTER_DECRY_FAIL;
		}
		
		if(ar9170_rx_filter(ar, rx_filter)) {
			PRINTF("AR9170_MAIN: rx-filter-warning\n");
			
		}
	}
}

void 
ar9170_op_flush(struct ieee80211_hw *hw, bool drop)
{
	PRINTF("AR9170_MAIN: ar9170_op_flush.\n");
	
	struct ar9170 *athr = hw->priv;
	unsigned int vid;
	
	vid = 0;
	ar9170_flush_cab(athr, vid);

	/* TODO Drop all packets in the TX queue */
	int i;
	cpu_irq_enter_critical();
	for (i=0; i<list_length(athr->ar9170_tx_list); i++) {
		ar9170_pkt_buf_t* pkt = list_pop(athr->ar9170_tx_list);
		if(pkt != NULL) {
			memb_free(ar9170_drv_get_tx_queue(), pkt);
		}
	}
	cpu_irq_leave_critical();
}
#endif /* WITH_WIFI_SUPPORT */
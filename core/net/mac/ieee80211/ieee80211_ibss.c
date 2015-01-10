/*
 * ieee80211_ibss.c
 *
 * Created: 2/9/2014 3:07:59 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "cfg80211.h"
#include "if_ether.h"
#include "ieee80211_psm.h"
#include "ieee80211_ibss.h"
#include "mac80211.h"
#include "nl80211.h"
#include "ieee80211.h"
#include "platform-conf.h"
#include "wifistack.h"

#if WITH_WIFI_SUPPORT

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define IEEE80211_FILL_TX_QUEUE(queue, ai_fs, cwmin, cwmax, _txop)          \
do {                                                                    \
	queue.aifs = ai_fs;                                             \
	queue.cw_min = cwmin;                                           \
	queue.cw_max = cwmax;                                           \
	queue.txop = _txop;                                             \
} while (0)

typedef uint64_t le64_t;

/** Global structures for the IBSS mgmt **/


struct ieee80211_vif ieee80211_ibss_vif;
struct ieee80211_tx_queue_params ibss_tx_params;
struct ieee80211_hw ieee80211_ibss_hw;
uint8_t ibss_channel_type;
struct ieee80211_ibss_info ibss_info;
struct ieee80211_tx_info ieee80211_ibss_bcn_tx_info;
struct ieee80211_tx_info ieee80211_ibss_data_tx_info;
struct ieee80211_tx_info ieee80211_ibss_bcast_tx_info;


struct ieee80211_sta ibss_station;
struct wifi_vif_info ibss_ar9170_cvif;
struct wiphy ibss_attached_wiphy;

//static uint8_t ieee80211_bcn_buf[AR9170_MAC_BCN_LENGTH_MAX];
//static uint8_t ieee80211_bcn_len;

/** Indicator flag for IBSS join/create */
static volatile uint8_t ieee80211_is_ibss_joined_flag;

/** Indicator flag for IBSS operating */
static volatile uint8_t ieee80211_has_ibss_started_beaconing_flag;


int __ieee80211_sta_init_config(void)
{
	/* Initialize flags */
	ieee80211_is_ibss_joined_flag = 0;
	ieee80211_has_ibss_started_beaconing_flag = 0;
	
	/* Zeroing*/
	memset(&ibss_station, 0, sizeof(struct ieee80211_sta));
	memset(&ibss_info, 0, sizeof(struct ieee80211_ibss_info)),
	
	/* Set to non-initialized */
	ibss_info.is_initialized = 0;
	ibss_info.is_merged = 0;
	
	/** Choose a BSSID for the network. SSID and Name **/
	
	// Set the BSSID manually
	const uint8_t default_bssid[ETH_ALEN] = IBSS_DEFAULT_BSSID;
	
	ibss_info.ibss_bssid[0] = default_bssid[0];
	ibss_info.ibss_bssid[1] = default_bssid[1];
	ibss_info.ibss_bssid[2] = default_bssid[2];
	ibss_info.ibss_bssid[3] = default_bssid[3];
	ibss_info.ibss_bssid[4] = default_bssid[4];
	ibss_info.ibss_bssid[5] = default_bssid[5];
	
	/* The network name is Disney :) */
	ibss_info.ibss_name[0] = 0x44;
	ibss_info.ibss_name[1] = 0x69;
	ibss_info.ibss_name[2] = 0x73;
	ibss_info.ibss_name[3] = 0x6e;
	ibss_info.ibss_name[4] = 0x65;
	ibss_info.ibss_name[5] = 0x79;
	
	/* The PSM mode */
	ibss_info.ibss_ps_mode = IEEE80211_PSM;
		
	/** Initialize the unique virtual interface structure **/
	
	/* set operating mode [ad-hoc]*/
	ieee80211_ibss_vif.type = NL80211_IFTYPE_ADHOC;
	
	/* zeroing the bss_conf memory */
	memset(&ieee80211_ibss_vif.bss_conf,0,sizeof(struct ieee80211_bss_conf));
	
	ieee80211_ibss_vif.bss_conf.use_short_slot = false; // TODO - check if this needs to be changed at any point.
	
	/* Set the bssid on the configuration structure */
	ieee80211_ibss_vif.bss_conf.bssid = ibss_info.ibss_bssid;
	
	/* Beacon period */
	ieee80211_ibss_vif.bss_conf.beacon_int = BEACON_INTERVAL;
	
	/* ATIM Window Duration */
	ieee80211_ibss_vif.bss_conf.atim_window = ATIM_WINDOW_DURATION;
	
	/* Soft Beacon Period. */
	ieee80211_ibss_vif.bss_conf.soft_beacon_int = IEEE80211_SOFT_BCN_W;
	
	/* 29 units of beacon interval IBSS DTIM period [DEBUG]. */
	ieee80211_ibss_vif.bss_conf.bcn_ctrl_period = 29;
	
	/* IBSS has not DTIM period */
	ieee80211_ibss_vif.bss_conf.dtim_period = 0;
	
	/* enable beaconing [default] */
	ieee80211_ibss_vif.bss_conf.enable_beacon = true;
	
	/* set the NO_HT channel type [default] */
	ieee80211_ibss_vif.bss_conf.channel_type = NL80211_CHAN_NO_HT;
	
	/* Basic rates */
	ieee80211_ibss_vif.bss_conf.basic_rates = 1;
	
	/* Short preamble */
	ieee80211_ibss_vif.bss_conf.use_short_preamble = false;
	
	/* set to unprepared to force initialization */
	ieee80211_ibss_vif.prepared = 0;
	
	ibss_ar9170_cvif.active = 0;
	ibss_ar9170_cvif.beacon = NULL;
	ibss_ar9170_cvif.id = 0;
		
	return(__ieee80211_sta_init_hw());
	/* TODO - perhaps more data should be initialized? */
}

int
__ieee80211_sta_init_hw(void)
{	
	ibss_info.ibss_channel.band = IEEE80211_BAND_2GHZ;
	ibss_channel_type = NL80211_CHAN_NO_HT;
	/* Uses channel 1 */
	ibss_info.ibss_channel.center_freq = 2412;
	ibss_info.ibss_channel.hw_value = 0;
	ibss_info.ibss_channel.max_power = 18;
			
	/* Set PS mode on startup.  */
	if (ibss_info.ibss_ps_mode != IBSS_NULL_PSM) {
		printf("IEEE80211_ibss: IBSS operates in Power-Save Mode.\n");
		ieee80211_ibss_hw.conf.flags = IEEE80211_CONF_PS;
	} else {
		printf("IEEE80211_ibss: IBSS operates in non-PSM\n");
	}
	/* We use a single TX QoS queue, but define all of them. */
	ieee80211_ibss_hw.queues = WIFISTACK_CONF_MAX_TX_QUEUES;
	
	/* Store the channel information on the hw structure */
	ieee80211_ibss_hw.conf.channel = &ibss_info.ibss_channel;
	ieee80211_ibss_hw.conf.channel_type = NL80211_CHAN_NO_HT; // TODO - check if this is correct to do it here
	/* Redundant */
	ieee80211_ibss_hw.conf.chandef.chan = &ibss_info.ibss_channel;
	
	ieee80211_ibss_hw.conf.power_level = 18;
	
	ieee80211_ibss_hw.conf.chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
	
	/* Store the reference to the AR9170 device if this is initialized */
/* XXX moved to driver allocation routine!	
	struct ar9170* ar = ar9170_get_device();
	
	if (ar == NULL) {
		printf("ERROR: AR9170 device structure is not allocated / initialized.\n");
		return -ENOMEM;
	}
	// otherwise, this is how the hw structure can access the ar9170 structure
	hw->priv = ar;
*/	
	/* Attach the wireless interface structure */
	ieee80211_ibss_hw.wiphy = &ibss_attached_wiphy;	
	
	/* Initialize the tx parameter set manually. These parameters can change later. */
	ibss_tx_params.aifs = 2;
	ibss_tx_params.cw_max = 1023;
	ibss_tx_params.cw_min = 31;
	ibss_tx_params.txop = 0;
	ibss_tx_params.uapsd = 0;	
	
	/* Static transmission information for outgoing data packets */
	ieee80211_ibss_data_tx_info.band = 0;
	
	ieee80211_ibss_data_tx_info.flags = 0;
	/* We never allow fragmentation */
	ieee80211_ibss_data_tx_info.flags |= IEEE80211_TX_CTL_DONTFRAG;
	/* And the frame is always the first fragment */
	ieee80211_ibss_data_tx_info.flags |= IEEE80211_TX_CTL_FIRST_FRAGMENT;
	/* We do not support encryption of transmitted frames */
	ieee80211_ibss_data_tx_info.flags |= IEEE80211_TX_INTFL_DONT_ENCRYPT;
	/* We always assign a sequence number although not really necessary */
	//ieee80211_ibss_data_tx_info.flags |= IEEE80211_TX_CTL_ASSIGN_SEQ;
	
	/* The driver must configure the queue and the rates instead! */
	
	/* HW queue is 1, unless we have a broadcast frame, e.g. beacon */
	ieee80211_ibss_data_tx_info.hw_queue = 1;	
	
	/* Also, broadcast frames have no rate info and no ack! */
	
	/* But unicast frames do.*/
	
	/* Pointers to IEEE80211 structures */
	ieee80211_ibss_data_tx_info.control.vif = &ieee80211_ibss_vif;
	ieee80211_ibss_data_tx_info.control.sta = &ibss_station;
	
	int i;
	for (i=0; i<IEEE80211_TX_MAX_RATES; i++) {
		
		if (i==0) {
			/* 3 times with the basic rates */
			ieee80211_ibss_data_tx_info.control.rates[i].count = 1;
			/* The rate ID is something large */
			ieee80211_ibss_data_tx_info.control.rates[i].idx = 3;
			/* We only set the TX_RC_MCS flag */
			ieee80211_ibss_data_tx_info.control.rates[i].flags = 0;
		}
		if (i==1) {
			ieee80211_ibss_data_tx_info.control.rates[i].count = 2;
			ieee80211_ibss_data_tx_info.control.rates[i].idx = 0;
			/* We use also RTS CTS here */
			ieee80211_ibss_data_tx_info.control.rates[i].flags = 0;
		}
		if (i==2) {
			ieee80211_ibss_data_tx_info.control.rates[i].count = 1;
			ieee80211_ibss_data_tx_info.control.rates[i].idx = 3;
			/* We use also RTS CTS here */
			ieee80211_ibss_data_tx_info.control.rates[i].flags = 0;
		}
		if (i==3) {
			/* Last rate is perhaps dummy... */
			ieee80211_ibss_data_tx_info.control.rates[i].count = 3;
			ieee80211_ibss_data_tx_info.control.rates[i].idx = 2;
			/* We use also RTS CTS here */
			ieee80211_ibss_data_tx_info.control.rates[i].flags = 0;
		}
	}
	/* Static transmission information for outgoing broadcast packets */
	ieee80211_ibss_bcast_tx_info.band = 0;
	
	ieee80211_ibss_bcast_tx_info.flags = 0;
	/* We never allow fragmentation */
	ieee80211_ibss_bcast_tx_info.flags |= IEEE80211_TX_CTL_DONTFRAG;
	/* And the frame is always the first fragment */
	ieee80211_ibss_bcast_tx_info.flags |= IEEE80211_TX_CTL_FIRST_FRAGMENT;
	/* We do not support encryption of transmitted frames */
	ieee80211_ibss_bcast_tx_info.flags |= IEEE80211_TX_INTFL_DONT_ENCRYPT;
	/* We always assign a sequence number although not really necessary */
	ieee80211_ibss_bcast_tx_info.flags |= IEEE80211_TX_CTL_ASSIGN_SEQ;
	
	/* The driver must configure the queue and the rates instead! */
	
	/* HW queue is 1, unless we have a broadcast frame, e.g. beacon */
	ieee80211_ibss_bcast_tx_info.hw_queue = 1;
	
	/* Also, broadcast frames have no rate info and no ack! */
	ieee80211_ibss_bcast_tx_info.flags |= IEEE80211_TX_CTL_NO_ACK;
	
	/* Pointers to IEEE80211 structures */
	ieee80211_ibss_bcast_tx_info.control.vif = &ieee80211_ibss_vif;
	ieee80211_ibss_bcast_tx_info.control.sta = &ibss_station;
	
	for (i=0; i<IEEE80211_TX_MAX_RATES; i++) {
		
		if (i==0) {
			/* 1 time with the basic rates */
			ieee80211_ibss_bcast_tx_info.control.rates[i].count = 1;
			/* The rate ID is something large */
			ieee80211_ibss_bcast_tx_info.control.rates[i].idx = 0;
			/* We only set the TX_RC_MCS flag */
			ieee80211_ibss_bcast_tx_info.control.rates[i].flags = 0;
		}
		if (i==1) {
			ieee80211_ibss_bcast_tx_info.control.rates[i].count = 0;
			ieee80211_ibss_bcast_tx_info.control.rates[i].idx = -1;
			ieee80211_ibss_bcast_tx_info.control.rates[i].flags = 0;
		}
		if (i==2) {
			ieee80211_ibss_bcast_tx_info.control.rates[i].count = 0;
			ieee80211_ibss_bcast_tx_info.control.rates[i].idx = -1;
			ieee80211_ibss_bcast_tx_info.control.rates[i].flags = 0;
		}
		if (i==3) {
			ieee80211_ibss_bcast_tx_info.control.rates[i].count = 0;
			ieee80211_ibss_bcast_tx_info.control.rates[i].idx = -1;
			ieee80211_ibss_bcast_tx_info.control.rates[i].flags = 0;
		}
	}	
	return 0;
}


int 
__ieee80211_hw_config(struct ieee80211_hw* _hw , uint32_t changed_flag)
{
	/* Configure device */
	int err = WIFI_STACK.config(_hw, changed_flag);
	
	if(err) {
		printf("ERROR: configuring AR9170 returned errors.\n");
	}
	return err;
}

static void 
_ieee80211_sta_update_info(void) {
	memcpy(&ibss_station.addr, ieee80211_ibss_vif.addr, ETH_ALEN);
}

static void ieee80211_ibss_build_probe_resp(ieee80211_pkt_buf_t* bcn_buf, uint32_t basic_rates, 
		uint16_t capability, uint64_t tsf) 
{
	int i, kk;
	uint8_t* test;
	UNUSED(test);
	
	uint16_t supp_rates[] = {10, 20, 55, 110, 60, 90, 120, 180, 240, 360, 480, 540};
		
	PRINTF("IEEE80211_IBSS: build-presp\n");
	
	struct ieee80211_mgmt* mgmt = (struct ieee80211_mgmt*)(bcn_buf->data);
	
	/* Update length */
	bcn_buf->len += 36;
	
	/* The frame control for the beacon frame */
	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP);
	
	/* Address 1: Destination MAC address is set to broadcast */
	memset(mgmt->da, 0xff, ETH_ALEN);
	/* Address 2: Source MAC address is the local address */
	memcpy(mgmt->sa, ieee80211_ibss_vif.addr, ETH_ALEN);
	/* Address 3: Contains the BSSID of the network */
	memcpy(mgmt->bssid, ibss_info.ibss_bssid, ETH_ALEN);
	/* The beacon interval duration */
	mgmt->u.beacon.beacon_int = cpu_to_le16(ieee80211_ibss_vif.bss_conf.beacon_int);
	/* Time stamp */
	mgmt->u.beacon.timestamp = 0;// FIXME cpu_to_le64(tsf);
	/* Privacy capability flag */
	mgmt->u.beacon.capab_info = cpu_to_le16(capability);
	
	/* Add the name of the network */
	bcn_buf->data[bcn_buf->len] = WLAN_EID_SSID;
	bcn_buf->len += 1;
	bcn_buf->data[bcn_buf->len] = ETH_ALEN;
	bcn_buf->len += 1;
	memcpy(&(bcn_buf->data[bcn_buf->len]), ibss_info.ibss_name, ETH_ALEN);
	bcn_buf->len += ETH_ALEN;
	
	/* Add supported rates. */
	// Build supported rates array - FIXME automate this!
	
	for (i = 0; i < 12; i++) {
		int rate = supp_rates[i];
		uint8_t basic = 0;
		if (basic_rates & BIT(i))
			basic = 0x80;
		supp_rates[i] = basic | (uint8_t) (rate / 5);
	}
	
	bcn_buf->data[bcn_buf->len] = WLAN_EID_SUPP_RATES;
	bcn_buf->len += 1;
	bcn_buf->data[bcn_buf->len] = 8; // FIXME - could be even less...
	bcn_buf->len += 1;
	for (i=0; i<8; i++) {
		bcn_buf->data[bcn_buf->len] = (uint8_t)supp_rates[i];
		bcn_buf->len += 1;
	}
	
	/* Add DS parameters */
	if (ibss_channel_type == IEEE80211_BAND_2GHZ) {
		bcn_buf->data[bcn_buf->len] = WLAN_EID_DS_PARAMS;
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 1;
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = ibss_info.ibss_channel.hw_value + 1; // FIXME
		bcn_buf->len += 1;
	}
	
	
	/* Add IBSS parameter set */
	bcn_buf->data[bcn_buf->len] = WLAN_EID_IBSS_PARAMS;
	bcn_buf->len += 1;
	bcn_buf->data[bcn_buf->len] = 2;
	bcn_buf->len += 1;
	bcn_buf->data[bcn_buf->len] =
	(uint8_t)(ieee80211_ibss_vif.bss_conf.atim_window);
	bcn_buf->len += 1;
	bcn_buf->data[bcn_buf->len] = 0;
	bcn_buf->len += 1;
	
	
	/* Add the extra rates */
	bcn_buf->data[bcn_buf->len] = WLAN_EID_EXT_SUPP_RATES;
	bcn_buf->len += 1;
	bcn_buf->data[bcn_buf->len] = 4;
	bcn_buf->len += 1;
	for(i=0; i<4; i++) {
		bcn_buf->data[bcn_buf->len] = (uint8_t)supp_rates[8+i];
		bcn_buf->len += 1;
	}
	
	
	/* Add Vendor Specific data */
	if (ieee80211_ibss_hw.queues >= IEEE80211_NUM_ACS) {
		bcn_buf->data[bcn_buf->len] = WLAN_EID_VENDOR_SPECIFIC;
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 7; /* len */
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 0x00; /* Microsoft OUI 00:50:F2 */
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 0x50;
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 0xf2;
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 2; /* WME */
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 0; /* WME info */
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 1; /* WME ver */
		bcn_buf->len += 1;
		bcn_buf->data[bcn_buf->len] = 0; /* U-APSD no in use */
		bcn_buf->len += 1;
	}
	
	#if DEBUG
	test = (uint8_t*)(bcn_buf->data);
	PRINTF("Current BCN [%d]: ",(unsigned int)(bcn_buf->len));
	for (kk=0; kk<bcn_buf->len; kk++)
		PRINTF("%02x ",test[kk]);
	PRINTF("\n");
	#endif	
}

static void 
__driver_conf_tx() 
{
	
	PRINTF("IEEE80211_IBSS: __driver_conf_tx.\n");
	
	struct ieee80211_tx_queue_params _params[4];
	
	
	IEEE80211_FILL_TX_QUEUE(_params[0], 7, 15, 1023, 0);
	IEEE80211_FILL_TX_QUEUE(_params[1], 3, 15, 1023, 0);
	IEEE80211_FILL_TX_QUEUE(_params[2], 2, 7, 15, 94);
	IEEE80211_FILL_TX_QUEUE(_params[3], 2, 7, 15,  47);
	
	/* This is a workaround for MAC TX QoS. We set 
	 * all TX queue QoS to the maximum possible values
	 * for the minimum and maximum contention window.
	 * This will add a small delay in the transmission
	 * of packets when the network load is moderate, but
	 * will enable fast packet flooding when traffic
	 * conditions are heavy.
	 */
/*	
	CARL9170_FILL_QUEUE(_params[3], 7, 15, 127, 0);
	CARL9170_FILL_QUEUE(_params[2], 7, 15, 63, 0);
	CARL9170_FILL_QUEUE(_params[1], 2, 7, 31, 188);
	CARL9170_FILL_QUEUE(_params[0], 2, 7, 15, 102);
*/	
/*
		CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_VO], 2, 3,     7, 47);
        CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_VI], 2, 7,    15, 94);
        CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_BE], 3, 15, 1023,  0);
        CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_BK], 7, 15, 1023,  0);
        CARL9170_FILL_QUEUE(ar->edcf[AR9170_TXQ_SPECIAL], 2, 3, 7, 0);
*/	
	
	/* Configure tx queues */
	if (WIFI_STACK.config_tx(&ieee80211_ibss_hw, &ieee80211_ibss_vif, 0, &_params[0]) ||
	    WIFI_STACK.config_tx(&ieee80211_ibss_hw, &ieee80211_ibss_vif, 1, &_params[1]) ||
		WIFI_STACK.config_tx(&ieee80211_ibss_hw, &ieee80211_ibss_vif, 2, &_params[2]) ||
		WIFI_STACK.config_tx(&ieee80211_ibss_hw, &ieee80211_ibss_vif, 3, &_params[3])) {
		printf("ERROR: Could not set TX parameters.\n");				
	}	
}

static void 
__driver_conf_rx_filter(void) 
{	
	PRINTF("IEEE80211_IBSS: drv-conf-rx-filter\n");
	
	WIFI_STACK.config_rx();	
}

static void
__ieee80211_bss_change_notify(uint32_t flag_changed) {
	
	PRINTF("IEEE80211_IBSS: bss-info-change\n");	
	WIFI_STACK.bss_info_changed(&ieee80211_ibss_hw, &ieee80211_ibss_vif, flag_changed);
}

static void
__ieee80211_sta_join_ibss(uint32_t basic_rates, uint16_t capability, le64_t tsf)
{	
	int i, err;
	uint32_t bss_change = 0;	
		
	UNUSED(err);
	PRINTF("IEEE80211_IBSS: Attempt to join the IBSS...\n");
			
	/* Until the default IBSS network has been created, and beaconing
	 * has started, we filter-out management frames, as they are not
	 * needed. The following command actually filters out everything.
	 */
	WIFI_STACK.rx_filter(WIFI_FILTER_ALL);	
										  
	/*
	 * Call the configuration function for the underlying
	 * AR9170 device.
	 */
	if(__ieee80211_hw_config(&ieee80211_ibss_hw, IEEE80211_CONF_CHANGE_CHANNEL))
		return;				
		
	/* One of the basic responsibilities of this function is
	 * to create the structure for the beacon that the 
	 * device will periodically transmit.
	 */
	ibss_info.ibss_beacon_buf.len = 0;
	memset(ibss_info.ibss_beacon_buf.data, 0, IEEE80211_TX_MAX_FRAME_LENGTH);

	/* Build the IBSS probe response */
	ieee80211_ibss_build_probe_resp(&ibss_info.ibss_beacon_buf, basic_rates, capability, tsf);
			
	/* Run TX configuration */
	__driver_conf_tx();
	
	if (!ibss_info.is_merged) {
		
		/* Notify the WiFi Driver */
		bss_change |= BSS_CHANGED_BEACON_INT;
		//bss_change |= BSS_CHANGED_BSSID;
		bss_change |= BSS_CHANGED_BEACON;
		bss_change |= BSS_CHANGED_BEACON_ENABLED;
		bss_change |= BSS_CHANGED_BASIC_RATES;
		bss_change |= BSS_CHANGED_HT;
		bss_change |= BSS_CHANGED_IBSS;
		bss_change |= BSS_CHANGED_ERP_SLOT;
	} else {
		/* Notify the WiFi Driver */
		//bss_change |= BSS_CHANGED_BEACON_INT;
		//bss_change |= BSS_CHANGED_BSSID;
		bss_change |= BSS_CHANGED_BEACON;
		//bss_change |= BSS_CHANGED_BEACON_ENABLED;
		bss_change |= BSS_CHANGED_BASIC_RATES;
		bss_change |= BSS_CHANGED_HT;
		bss_change |= BSS_CHANGED_IBSS;
		bss_change |= BSS_CHANGED_ERP_SLOT;
	}
	PRINTF("IEEE80211_IBSS: notify-drv\n");
	
	/* Notify the driver for the network changes */
	__ieee80211_bss_change_notify(bss_change);
	
	/* Run RX filter configuration */
	__driver_conf_rx_filter();
	
	PRINTF("_sta_join_end\n");
}

void ieee80211_sta_join_ibss(void)
{
	PRINTF("IEEE80211_IBSSS: Merging to the default network...\n");
	
	ibss_info.is_merged = 1;
	
	ieee80211_sta_create_ibss();
}
void ieee80211_sta_create_ibss(void)
{
	PRINTF("IEEE80211_IBSS: Creating default network...\n");
	
	_ieee80211_sta_update_info();
	
	uint16_t capability;
	
	#if DEBUG
	int i;
	PRINTF("IEEE80211_IBSS: Network name is 'Disney'. \n");
	PRINTF("IEEE80211_IBSS: BSSID: ");
	for(i=0; i<ETH_ALEN; i++)
		PRINTF("%02x:",ibss_info.ibss_bssid[i]);
		PRINTF("\r\n");
	#endif
	
	/* Privacy capabilities */
	capability = WLAN_CAPABILITY_IBSS | WLAN_CAPABILITY_PRIVACY ;
	
	/* Join this IBSS */
	__ieee80211_sta_join_ibss(1, capability, 0);
	
	/* The IBSS is now initialized */
	ibss_info.is_initialized = 1;
}

struct ieee80211_pkt_buf* 
ieee80211_beacon_get_tim(struct ieee80211_hw * hw, struct ieee80211_vif* vif, 
	void* tim_offset, void* tim_length)
{	
	int kk;
	uint8_t* test;
	struct ieee80211_tx_info *info;
	UNUSED(kk);
	UNUSED(test);
	
	PRINTF("IEEE80211_IBSS: bcn-get-tim\n");
	
	struct ieee80211_pkt_buf *presp = &ibss_info.ibss_beacon_buf;
	
	if (vif->type == NL80211_IFTYPE_AP) {
		/*
		 *  TODO - just ignore for now
		 *
		 */
	} else if (vif->type == NL80211_IFTYPE_ADHOC) {
		
		PRINTF("IEEE80211_IBSS: Beacon will be tweaked assuming AD-HOC mode.\n");
				
		if (!presp) {	
			printf("IEEE80211_IBSS: Beacon data could not be retrieved.\n");
			goto out;
		}
		
		/* Beacon frame control is now changed. */
		le16_t frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
		
		memcpy(presp->data, &frame_control,sizeof(le16_t));
	
	} else {		
		PRINTF("WARNING: Neither ad-hoc nor access point.\n");
	}
	info = &ieee80211_ibss_bcn_tx_info; //John
	
	info->flags |= IEEE80211_TX_INTFL_DONT_ENCRYPT;
	info->flags |= IEEE80211_TX_CTL_NO_ACK;
	info->band = ibss_info.ibss_channel.band;//ieee80211_ibss_vif.chanctx_conf->def.chan->band; //XXX XXX XXX
	//info->control.vif = vif;
	
	// John - XXX 
	info->control.rates[0].count = 1;
	info->control.rates[0].flags = IEEE80211_TX_RC_SHORT_GI;
	info->control.rates[0].idx = 0; 
	
	info->control.rates[1].count = 0;
	info->control.rates[1].idx = -1;
	info->control.rates[1].flags = 0;
	
	info->flags |= IEEE80211_TX_CTL_CLEAR_PS_FILT |
                   IEEE80211_TX_CTL_ASSIGN_SEQ |
                   IEEE80211_TX_CTL_FIRST_FRAGMENT;
		
out:	
	return &ibss_info.ibss_beacon_buf;
}

#endif /* WITH_WIFI_SUPPORT */
/*
 * ieee80211.h
 *
 * Created: 1/29/2014 10:09:14 AM
 *  Author: Ioannis Glaropoulos
 */ 

#ifndef IEEE80211_H_
#define IEEE80211_H_
#include "include/if_ether.h"
//#include <stdint-gcc.h>
#include "compiler.h"

/*
 * IEEE 802.11 defines
 *
 * Copyright (c) 2001-2002, SSH Communications Security Corp and Jouni Malinen
 * <jkmaline@cc.hut.fi>
 * Copyright (c) 2002-2003, Jouni Malinen <jkmaline@cc.hut.fi>
 * Copyright (c) 2005, Devicescape Software, Inc.
 * Copyright (c) 2006, Michael Wu <flamingice@sourmilk.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define IEEE80211_MAX_DATA_LEN          2304
   /* 30 byte 4 addr hdr, 2 byte QoS, 2304 byte MSDU, 12 byte crypt, 4 byte FCS */
#define IEEE80211_MAX_FRAME_LEN         2352
   
#define IEEE80211_MAX_SSID_LEN          32

/* Notice of Absence attribute - described in P2P spec 4.1.14 */
/* Typical max value used here */
#define IEEE80211_P2P_NOA_DESC_MAX      4

#define IEEE80211_MAX_MESH_ID_LEN	32

#define IEEE80211_QOS_CTL_LEN		2
/* 1d tag mask */
#define IEEE80211_QOS_CTL_TAG1D_MASK		0x0007
/* TID mask */
#define IEEE80211_QOS_CTL_TID_MASK		0x000f
/* EOSP */
#define IEEE80211_QOS_CTL_EOSP			0x0010
/* ACK policy */
#define IEEE80211_QOS_CTL_ACK_POLICY_NORMAL	0x0000
#define IEEE80211_QOS_CTL_ACK_POLICY_NOACK	0x0020
#define IEEE80211_QOS_CTL_ACK_POLICY_NO_EXPL	0x0040
#define IEEE80211_QOS_CTL_ACK_POLICY_BLOCKACK	0x0060
#define IEEE80211_QOS_CTL_ACK_POLICY_MASK	0x0060
/* A-MSDU 802.11n */
#define IEEE80211_QOS_CTL_A_MSDU_PRESENT	0x0080
/* Mesh Control 802.11s */
#define IEEE80211_QOS_CTL_MESH_CONTROL_PRESENT  0x0100

/* U-APSD queue for WMM IEs sent by AP */
#define IEEE80211_WMM_IE_AP_QOSINFO_UAPSD	(1<<7)
#define IEEE80211_WMM_IE_AP_QOSINFO_PARAM_SET_CNT_MASK	0x0f

/* U-APSD queues for WMM IEs sent by STA */
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_VO	(1<<0)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_VI	(1<<1)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_BK	(1<<2)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_BE	(1<<3)
#define IEEE80211_WMM_IE_STA_QOSINFO_AC_MASK	0x0f

/* U-APSD max SP length for WMM IEs sent by STA */
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL	0x00
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_2	0x01
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_4	0x02
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_6	0x03
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_MASK	0x03
#define IEEE80211_WMM_IE_STA_QOSINFO_SP_SHIFT	5

#define IEEE80211_HT_CTL_LEN		4


 

struct ieee80211_p2p_noa_desc {
	uint8_t count;
	le32_t duration;
	le32_t interval;
	le32_t start_time;
} __attribute__((packed));

struct ieee80211_p2p_noa_attr {
	 uint8_t index;
	 uint8_t oppps_ctwindow;
	 struct ieee80211_p2p_noa_desc desc[IEEE80211_P2P_NOA_DESC_MAX];
} __attribute__((packed));
 
/*
 * DS bit usage
 *
 * TA = transmitter address
 * RA = receiver address
 * DA = destination address
 * SA = source address
 *
 * ToDS    FromDS  A1(RA)  A2(TA)  A3      A4      Use
 * -----------------------------------------------------------------
 *  0       0       DA      SA      BSSID   -       IBSS/DLS
 *  0       1       DA      BSSID   SA      -       AP -> STA
 *  1       0       BSSID   SA      DA      -       AP <- STA
 *  1       1       RA      TA      DA      SA      unspecified (WDS)
 */
 
#define FCS_LEN 4

#define IEEE80211_FCTL_VERS             0x0003
#define IEEE80211_FCTL_FTYPE            0x000c
#define IEEE80211_FCTL_STYPE            0x00f0
#define IEEE80211_FCTL_TODS             0x0100
#define IEEE80211_FCTL_FROMDS           0x0200
#define IEEE80211_FCTL_MOREFRAGS        0x0400
#define IEEE80211_FCTL_RETRY            0x0800
#define IEEE80211_FCTL_PM               0x1000
#define IEEE80211_FCTL_MOREDATA         0x2000
#define IEEE80211_FCTL_PROTECTED        0x4000
#define IEEE80211_FCTL_ORDER            0x8000
#define IEEE80211_FCTL_CTL_EXT          0x0f00

#define IEEE80211_SCTL_FRAG             0x000F
#define IEEE80211_SCTL_SEQ              0xFFF0

#define IEEE80211_FTYPE_MGMT            0x0000
#define IEEE80211_FTYPE_CTL             0x0004
#define IEEE80211_FTYPE_DATA            0x0008
#define IEEE80211_FTYPE_EXT             0x000c

/* management */
#define IEEE80211_STYPE_ASSOC_REQ       0x0000
#define IEEE80211_STYPE_ASSOC_RESP      0x0010
#define IEEE80211_STYPE_REASSOC_REQ     0x0020
#define IEEE80211_STYPE_REASSOC_RESP    0x0030
#define IEEE80211_STYPE_PROBE_REQ       0x0040
#define IEEE80211_STYPE_PROBE_RESP      0x0050
#define IEEE80211_STYPE_BEACON          0x0080
#define IEEE80211_STYPE_ATIM            0x0090
#define IEEE80211_STYPE_DISASSOC        0x00A0
#define IEEE80211_STYPE_AUTH            0x00B0
#define IEEE80211_STYPE_DEAUTH          0x00C0
#define IEEE80211_STYPE_ACTION          0x00D0

/* control */
#define IEEE80211_STYPE_CTL_EXT         0x0060
#define IEEE80211_STYPE_BACK_REQ        0x0080
#define IEEE80211_STYPE_BACK            0x0090
#define IEEE80211_STYPE_PSPOLL          0x00A0
#define IEEE80211_STYPE_RTS             0x00B0
#define IEEE80211_STYPE_CTS             0x00C0
#define IEEE80211_STYPE_ACK             0x00D0
#define IEEE80211_STYPE_CFEND           0x00E0
#define IEEE80211_STYPE_CFENDACK        0x00F0

/* data */
#define IEEE80211_STYPE_DATA                    0x0000
#define IEEE80211_STYPE_DATA_CFACK              0x0010
#define IEEE80211_STYPE_DATA_CFPOLL             0x0020
#define IEEE80211_STYPE_DATA_CFACKPOLL          0x0030
#define IEEE80211_STYPE_NULLFUNC                0x0040
#define IEEE80211_STYPE_CFACK                   0x0050
#define IEEE80211_STYPE_CFPOLL                  0x0060
#define IEEE80211_STYPE_CFACKPOLL               0x0070
#define IEEE80211_STYPE_QOS_DATA                0x0080
#define IEEE80211_STYPE_QOS_DATA_CFACK          0x0090
#define IEEE80211_STYPE_QOS_DATA_CFPOLL         0x00A0
#define IEEE80211_STYPE_QOS_DATA_CFACKPOLL      0x00B0
#define IEEE80211_STYPE_QOS_NULLFUNC            0x00C0
#define IEEE80211_STYPE_QOS_CFACK               0x00D0
#define IEEE80211_STYPE_QOS_CFPOLL              0x00E0
#define IEEE80211_STYPE_QOS_CFACKPOLL           0x00F0

/* extension, added by 802.11ad */
#define IEEE80211_STYPE_DMG_BEACON              0x0000

/* control extension - for IEEE80211_FTYPE_CTL | IEEE80211_STYPE_CTL_EXT */
#define IEEE80211_CTL_EXT_POLL          0x2000
#define IEEE80211_CTL_EXT_SPR           0x3000
#define IEEE80211_CTL_EXT_GRANT 0x4000
#define IEEE80211_CTL_EXT_DMG_CTS       0x5000
#define IEEE80211_CTL_EXT_DMG_DTS       0x6000
#define IEEE80211_CTL_EXT_SSW           0x8000
#define IEEE80211_CTL_EXT_SSW_FBACK     0x9000
#define IEEE80211_CTL_EXT_SSW_ACK       0xa000


#define IEEE80211_SN_MASK               ((IEEE80211_SCTL_SEQ) >> 4)
#define IEEE80211_MAX_SN                IEEE80211_SN_MASK
#define IEEE80211_SN_MODULO             (IEEE80211_MAX_SN + 1)


/* 802.11n HT capability MSC set */
#define IEEE80211_HT_MCS_RX_HIGHEST_MASK        0x3ff
#define IEEE80211_HT_MCS_TX_DEFINED             0x01
#define IEEE80211_HT_MCS_TX_RX_DIFF             0x02
/* value 0 == 1 stream etc */
#define IEEE80211_HT_MCS_TX_MAX_STREAMS_MASK    0x0C
#define IEEE80211_HT_MCS_TX_MAX_STREAMS_SHIFT   2
#define         IEEE80211_HT_MCS_TX_MAX_STREAMS 4
#define IEEE80211_HT_MCS_TX_UNEQUAL_MODULATION  0x10


/**
667  * struct ieee80211_channel_sw_ie
668  *
669  * This structure refers to "Channel Switch Announcement information element"
670  */
struct ieee80211_channel_sw_ie {
	uint8_t mode;
	uint8_t new_ch_num;
	uint8_t count;
} __attribute__((packed));

/**
678  * struct ieee80211_ext_chansw_ie
679  *
680  * This structure represents the "Extended Channel Switch Announcement element"
681  */
struct ieee80211_ext_chansw_ie {
	uint8_t mode;
	uint8_t new_operating_class;
	uint8_t new_ch_num;
	uint8_t count;
} __attribute__((packed));

/**
690  * struct ieee80211_sec_chan_offs_ie - secondary channel offset IE
691  * @sec_chan_offs: secondary channel offset, uses IEEE80211_HT_PARAM_CHA_SEC_*
692  *      values here
693  * This structure represents the "Secondary Channel Offset element"
694  */
struct ieee80211_sec_chan_offs_ie {
	uint8_t sec_chan_offs;
} __attribute__((packed));

/**
700  * struct ieee80211_mesh_chansw_params_ie - mesh channel switch parameters IE
701  *
702  * This structure represents the "Mesh Channel Switch Paramters element"
703  */
struct ieee80211_mesh_chansw_params_ie {
	uint8_t mesh_ttl;
	uint8_t mesh_flags;
	le16_t mesh_reason;
	le16_t mesh_pre_value;
} __attribute__((packed));


/**
712  * struct ieee80211_wide_bw_chansw_ie - wide bandwidth channel switch IE
713  */
struct ieee80211_wide_bw_chansw_ie {
	uint8_t new_channel_width;
	uint8_t new_center_freq_seg0, new_center_freq_seg1;
} __attribute__((packed));

/**
 * struct ieee80211_tim
 *
 * This structure refers to "Traffic Indication Map information element"
 */
struct ieee80211_tim_ie {
	uint8_t dtim_count;
	uint8_t dtim_period;
	uint8_t bitmap_ctrl;
	/* variable size: 1 - 251 bytes */
	uint8_t virtual_map[1];
} __attribute__((packed));

/**
733  * struct ieee80211_meshconf_ie
734  *
735  * This structure refers to "Mesh Configuration information element"
736  */
struct ieee80211_meshconf_ie {
	uint8_t meshconf_psel;
	uint8_t meshconf_pmetric;
	uint8_t meshconf_congest;
	uint8_t meshconf_synch;
	uint8_t meshconf_auth;
	uint8_t meshconf_form;
	uint8_t meshconf_cap;
} __attribute__((packed));

/**
774  * struct ieee80211_rann_ie
775  *
776  * This structure refers to "Root Announcement information element"
777  */
struct ieee80211_rann_ie {
	uint8_t rann_flags;
	uint8_t rann_hopcount;
	uint8_t rann_ttl;
	uint8_t rann_addr[ETH_ALEN];
	le32_t rann_seq;
	le32_t rann_interval;
	le32_t rann_metric;
} __attribute__((packed));





/**
1106  * struct ieee80211_bar - HT Block Ack Request
1107  *
1108  * This structure refers to "HT BlockAckReq" as
1109  * described in 802.11n draft section 7.2.1.7.1
1110  */
struct ieee80211_bar {
	         le16_t frame_control;
	         le16_t duration;
	         uint8_t ra[ETH_ALEN];
	         uint8_t ta[ETH_ALEN];
	         le16_t control;
	         le16_t start_seq_num;
} __attribute__((packed));

/* 802.11 BAR control masks */
#define IEEE80211_BAR_CTRL_ACK_POLICY_NORMAL    0x0000
#define IEEE80211_BAR_CTRL_MULTI_TID            0x0002
#define IEEE80211_BAR_CTRL_CBMTID_COMPRESSED_BA 0x0004
#define IEEE80211_BAR_CTRL_TID_INFO_MASK        0xf000
#define IEEE80211_BAR_CTRL_TID_INFO_SHIFT       12

#define IEEE80211_HT_MCS_MASK_LEN               10

/**
1130  * struct ieee80211_mcs_info - MCS information
1131  * @rx_mask: RX mask
1132  * @rx_highest: highest supported RX rate. If set represents
1133  *      the highest supported RX data rate in units of 1 Mbps.
1134  *      If this field is 0 this value should not be used to
1135  *      consider the highest RX data rate supported.
1136  * @tx_params: TX parameters
1137  */
struct ieee80211_mcs_info {
	         uint8_t rx_mask[IEEE80211_HT_MCS_MASK_LEN];
	         le16_t rx_highest;
	         uint8_t tx_params;
	         uint8_t reserved[3];
} __attribute__((packed));

/**
 * struct ieee80211_ht_cap - HT capabilities
 *
 * This structure is the "HT capabilities element" as
 * described in 802.11n D5.0 7.3.2.57
 */
struct ieee80211_ht_cap {
	         le16_t cap_info;
	         uint8_t ampdu_params_info;
	
	         /* 16 bytes MCS information */
	         struct ieee80211_mcs_info mcs;
	
	         le16_t extended_ht_cap_info;
	         le32_t tx_BF_cap_info;
	         uint8_t antenna_selection_info;
} __attribute__((packed));

/**
  * struct ieee80211_ht_operation - HT operation IE
1243  *
1244  * This structure is the "HT operation element" as
1245  * described in 802.11n-2009 7.3.2.57
1246  */
struct ieee80211_ht_operation {
	uint8_t primary_chan;
	uint8_t ht_param;
	le16_t operation_mode;
	le16_t stbc_param;
	uint8_t basic_set[16];
} __attribute__((packed));

/**
1308  * struct ieee80211_vht_mcs_info - VHT MCS information
1309  * @rx_mcs_map: RX MCS map 2 bits for each stream, total 8 streams
1310  * @rx_highest: Indicates highest long GI VHT PPDU data rate
1311  *      STA can receive. Rate expressed in units of 1 Mbps.
1312  *      If this field is 0 this value should not be used to
1313  *      consider the highest RX data rate supported.
1314  *      The top 3 bits of this field are reserved.
1315  * @tx_mcs_map: TX MCS map 2 bits for each stream, total 8 streams
1316  * @tx_highest: Indicates highest long GI VHT PPDU data rate
1317  *      STA can transmit. Rate expressed in units of 1 Mbps.
1318  *      If this field is 0 this value should not be used to
1319  *      consider the highest TX data rate supported.
1320  *      The top 3 bits of this field are reserved.
1321  */
struct ieee80211_vht_mcs_info {
	le16_t rx_mcs_map;
	le16_t rx_highest;
	le16_t tx_mcs_map;
	le16_t tx_highest;
} __attribute__((packed));



/**
1351  * struct ieee80211_vht_cap - VHT capabilities
1352  *
1353  * This structure is the "VHT capabilities element" as
1354  * described in 802.11ac D3.0 8.4.2.160
1355  * @vht_cap_info: VHT capability info
1356  * @supp_mcs: VHT MCS supported rates
1357  */
struct ieee80211_vht_cap {
	le32_t vht_cap_info;
	struct ieee80211_vht_mcs_info supp_mcs;
} __attribute__((packed));

/**
1379  * struct ieee80211_vht_operation - VHT operation IE
1380  *
1381  * This structure is the "VHT operation element" as
1382  * described in 802.11ac D3.0 8.4.2.161
1383  * @chan_width: Operating channel width
1384  * @center_freq_seg1_idx: center freq segment 1 index
1385  * @center_freq_seg2_idx: center freq segment 2 index
1386  * @basic_mcs_set: VHT Basic MCS rate set
1387  */
struct ieee80211_vht_operation {
	uint8_t chan_width;
	uint8_t center_freq_seg1_idx;
	uint8_t center_freq_seg2_idx;
	le16_t basic_mcs_set;
} __attribute__((packed));

/* 802.11n HT capabilities masks (for cap_info) */
#define IEEE80211_HT_CAP_LDPC_CODING            0x0001
#define IEEE80211_HT_CAP_SUP_WIDTH_20_40        0x0002
#define IEEE80211_HT_CAP_SM_PS                  0x000C
#define         IEEE80211_HT_CAP_SM_PS_SHIFT    2
#define IEEE80211_HT_CAP_GRN_FLD                0x0010
#define IEEE80211_HT_CAP_SGI_20                 0x0020
#define IEEE80211_HT_CAP_SGI_40                 0x0040
#define IEEE80211_HT_CAP_TX_STBC                0x0080
#define IEEE80211_HT_CAP_RX_STBC                0x0300
#define         IEEE80211_HT_CAP_RX_STBC_SHIFT  8
#define IEEE80211_HT_CAP_DELAY_BA               0x0400
#define IEEE80211_HT_CAP_MAX_AMSDU              0x0800
#define IEEE80211_HT_CAP_DSSSCCK40              0x1000
#define IEEE80211_HT_CAP_RESERVED               0x2000
#define IEEE80211_HT_CAP_40MHZ_INTOLERANT       0x4000
#define IEEE80211_HT_CAP_LSIG_TXOP_PROT         0x8000

 /* 802.11n HT extended capabilities masks (for extended_ht_cap_info) */
#define IEEE80211_HT_EXT_CAP_PCO                0x0001
#define IEEE80211_HT_EXT_CAP_PCO_TIME           0x0006
#define         IEEE80211_HT_EXT_CAP_PCO_TIME_SHIFT     1
#define IEEE80211_HT_EXT_CAP_MCS_FB             0x0300
#define         IEEE80211_HT_EXT_CAP_MCS_FB_SHIFT       8
#define IEEE80211_HT_EXT_CAP_HTC_SUP            0x0400
#define IEEE80211_HT_EXT_CAP_RD_RESPONDER       0x0800

 /* 802.11n HT capability AMPDU settings (for ampdu_params_info) */
#define IEEE80211_HT_AMPDU_PARM_FACTOR          0x03
#define IEEE80211_HT_AMPDU_PARM_DENSITY         0x1C
#define         IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT   2

/*
1217  * Maximum length of AMPDU that the STA can receive.
1218  * Length = 2 ^ (13 + max_ampdu_length_exp) - 1 (octets)
1219  */
enum ieee80211_max_ampdu_length_exp {
	         IEEE80211_HT_MAX_AMPDU_8K = 0,
	         IEEE80211_HT_MAX_AMPDU_16K = 1,
	         IEEE80211_HT_MAX_AMPDU_32K = 2,
	         IEEE80211_HT_MAX_AMPDU_64K = 3
};


#define IEEE80211_HT_MAX_AMPDU_FACTOR 13

/* Minimum MPDU start spacing */
enum ieee80211_min_mpdu_spacing {
	         IEEE80211_HT_MPDU_DENSITY_NONE = 0,     /* No restriction */
	         IEEE80211_HT_MPDU_DENSITY_0_25 = 1,     /* 1/4 usec */
	         IEEE80211_HT_MPDU_DENSITY_0_5 = 2,      /* 1/2 usec */
	         IEEE80211_HT_MPDU_DENSITY_1 = 3,        /* 1 usec */
	         IEEE80211_HT_MPDU_DENSITY_2 = 4,        /* 2 usec */
	         IEEE80211_HT_MPDU_DENSITY_4 = 5,        /* 4 usec */
	         IEEE80211_HT_MPDU_DENSITY_8 = 6,        /* 8 usec */
	         IEEE80211_HT_MPDU_DENSITY_16 = 7        /* 16 usec */
};


/* Information Element IDs */
enum ieee80211_eid {
	WLAN_EID_SSID = 0,
	WLAN_EID_SUPP_RATES = 1,
	WLAN_EID_FH_PARAMS = 2,
	WLAN_EID_DS_PARAMS = 3,
	WLAN_EID_CF_PARAMS = 4,
	WLAN_EID_TIM = 5,
	WLAN_EID_IBSS_PARAMS = 6,
	WLAN_EID_CHALLENGE = 16,

	WLAN_EID_COUNTRY = 7,
	WLAN_EID_HP_PARAMS = 8,
	WLAN_EID_HP_TABLE = 9,
	WLAN_EID_REQUEST = 10,

	WLAN_EID_QBSS_LOAD = 11,
	WLAN_EID_EDCA_PARAM_SET = 12,
	WLAN_EID_TSPEC = 13,
	WLAN_EID_TCLAS = 14,
	WLAN_EID_SCHEDULE = 15,
	WLAN_EID_TS_DELAY = 43,
	WLAN_EID_TCLAS_PROCESSING = 44,
	WLAN_EID_QOS_CAPA = 46,
	/* 802.11z */
	WLAN_EID_LINK_ID = 101,
	/* 802.11s */
	WLAN_EID_MESH_CONFIG = 113,
	WLAN_EID_MESH_ID = 114,
	WLAN_EID_LINK_METRIC_REPORT = 115,
	WLAN_EID_CONGESTION_NOTIFICATION = 116,
	WLAN_EID_PEER_MGMT = 117,
	WLAN_EID_CHAN_SWITCH_PARAM = 118,
	WLAN_EID_MESH_AWAKE_WINDOW = 119,
	WLAN_EID_BEACON_TIMING = 120,
	WLAN_EID_MCCAOP_SETUP_REQ = 121,
	WLAN_EID_MCCAOP_SETUP_RESP = 122,
	WLAN_EID_MCCAOP_ADVERT = 123,
	WLAN_EID_MCCAOP_TEARDOWN = 124,
	WLAN_EID_GANN = 125,
	WLAN_EID_RANN = 126,
	WLAN_EID_PREQ = 130,
	WLAN_EID_PREP = 131,
	WLAN_EID_PERR = 132,
	WLAN_EID_PXU = 137,
	WLAN_EID_PXUC = 138,
	WLAN_EID_AUTH_MESH_PEER_EXCH = 139,
	WLAN_EID_MIC = 140,

	WLAN_EID_PWR_CONSTRAINT = 32,
	WLAN_EID_PWR_CAPABILITY = 33,
	WLAN_EID_TPC_REQUEST = 34,
	WLAN_EID_TPC_REPORT = 35,
	WLAN_EID_SUPPORTED_CHANNELS = 36,
	WLAN_EID_CHANNEL_SWITCH = 37,
	WLAN_EID_MEASURE_REQUEST = 38,
	WLAN_EID_MEASURE_REPORT = 39,
	WLAN_EID_QUIET = 40,
	WLAN_EID_IBSS_DFS = 41,

	WLAN_EID_ERP_INFO = 42,
	WLAN_EID_EXT_SUPP_RATES = 50,

	WLAN_EID_HT_CAPABILITY = 45,
	WLAN_EID_HT_OPERATION = 61,

	WLAN_EID_RSN = 48,
	WLAN_EID_MMIE = 76,
	WLAN_EID_WPA = 221,
	WLAN_EID_GENERIC = 221,
	WLAN_EID_VENDOR_SPECIFIC = 221,
	WLAN_EID_QOS_PARAMETER = 222,

	WLAN_EID_AP_CHAN_REPORT = 51,
	WLAN_EID_NEIGHBOR_REPORT = 52,
	WLAN_EID_RCPI = 53,
	WLAN_EID_BSS_AVG_ACCESS_DELAY = 63,
	WLAN_EID_ANTENNA_INFO = 64,
	WLAN_EID_RSNI = 65,
	WLAN_EID_MEASUREMENT_PILOT_TX_INFO = 66,
	WLAN_EID_BSS_AVAILABLE_CAPACITY = 67,
	WLAN_EID_BSS_AC_ACCESS_DELAY = 68,
	WLAN_EID_RRM_ENABLED_CAPABILITIES = 70,
	WLAN_EID_MULTIPLE_BSSID = 71,
	WLAN_EID_BSS_COEX_2040 = 72,
	WLAN_EID_OVERLAP_BSS_SCAN_PARAM = 74,
	WLAN_EID_EXT_CAPABILITY = 127,

	WLAN_EID_MOBILITY_DOMAIN = 54,
	WLAN_EID_FAST_BSS_TRANSITION = 55,
	WLAN_EID_TIMEOUT_INTERVAL = 56,
	WLAN_EID_RIC_DATA = 57,
	WLAN_EID_RIC_DESCRIPTOR = 75,

	WLAN_EID_DSE_REGISTERED_LOCATION = 58,
	WLAN_EID_SUPPORTED_REGULATORY_CLASSES = 59,
	WLAN_EID_EXT_CHANSWITCH_ANN = 60,

	WLAN_EID_VHT_CAPABILITY = 191,
	WLAN_EID_VHT_OPERATION = 192,

	/* 802.11ad */
	WLAN_EID_NON_TX_BSSID_CAP =  83,
	WLAN_EID_WAKEUP_SCHEDULE = 143,
	WLAN_EID_EXT_SCHEDULE = 144,
	WLAN_EID_STA_AVAILABILITY = 145,
	WLAN_EID_DMG_TSPEC = 146,
	WLAN_EID_DMG_AT = 147,
	WLAN_EID_DMG_CAP = 148,
	WLAN_EID_DMG_OPERATION = 151,
	WLAN_EID_DMG_BSS_PARAM_CHANGE = 152,
	WLAN_EID_DMG_BEAM_REFINEMENT = 153,
	WLAN_EID_CHANNEL_MEASURE_FEEDBACK = 154,
	WLAN_EID_AWAKE_WINDOW = 157,
	WLAN_EID_MULTI_BAND = 158,
	WLAN_EID_ADDBA_EXT = 159,
	WLAN_EID_NEXT_PCP_LIST = 160,
	WLAN_EID_PCP_HANDOVER = 161,
	WLAN_EID_DMG_LINK_MARGIN = 162,
	WLAN_EID_SWITCHING_STREAM = 163,
	WLAN_EID_SESSION_TRANSITION = 164,
	WLAN_EID_DYN_TONE_PAIRING_REPORT = 165,
	WLAN_EID_CLUSTER_REPORT = 166,
	WLAN_EID_RELAY_CAP = 167,
	WLAN_EID_RELAY_XFER_PARAM_SET = 168,
	WLAN_EID_BEAM_LINK_MAINT = 169,
	WLAN_EID_MULTIPLE_MAC_ADDR = 170,
	WLAN_EID_U_PID = 171,
	WLAN_EID_DMG_LINK_ADAPT_ACK = 172,
	WLAN_EID_QUIET_PERIOD_REQ = 175,
	WLAN_EID_QUIET_PERIOD_RESP = 177,
	WLAN_EID_EPAC_POLICY = 182,
	WLAN_EID_CLISTER_TIME_OFF = 183,
	WLAN_EID_ANTENNA_SECTOR_ID_PATTERN = 190,
};

#define WLAN_SA_QUERY_TR_ID_LEN 2


#define WLAN_CAPABILITY_ESS		(1<<0)
#define WLAN_CAPABILITY_IBSS		(1<<1)

/*
 * A mesh STA sets the ESS and IBSS capability bits to zero.
 * however, this holds true for p2p probe responses (in the p2p_find
 * phase) as well.
 */
#define WLAN_CAPABILITY_IS_STA_BSS(cap)	\
	(!((cap) & (WLAN_CAPABILITY_ESS | WLAN_CAPABILITY_IBSS)))

#define WLAN_CAPABILITY_CF_POLLABLE	(1<<2)
#define WLAN_CAPABILITY_CF_POLL_REQUEST	(1<<3)
#define WLAN_CAPABILITY_PRIVACY		(1<<4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE	(1<<5)
#define WLAN_CAPABILITY_PBCC		(1<<6)
#define WLAN_CAPABILITY_CHANNEL_AGILITY	(1<<7)


#define WLAN_EID_SECONDARY_CHANNEL_OFFSET 62

#define WLAN_EID_OPMODE_NOTIF  199

#define WLAN_EID_WIDE_BW_CHANNEL_SWITCH  194



#define WLAN_EID_CHANNEL_SWITCH_WRAPPER  196


















 




 /* Control frames */
struct ieee80211_rts {
	le16_t frame_control;
	le16_t duration;
	uint8_t ra[ETH_ALEN];
	uint8_t ta[ETH_ALEN];
} __attribute__((packed,aligned(2)));

struct ieee80211_cts {
	le16_t frame_control;
	le16_t duration;
	uint8_t ra[ETH_ALEN];
} __attribute__((packed,aligned(2)));




/**
 * struct ieee80211_msrment_ie
 *
 * This structure refers to "Measurement Request/Report information element"
 */
struct ieee80211_msrment_ie {
	U8 token;
	U8 mode;
	U8 type;
	U8 request[0];
} __attribute__ ((packed));




struct ieee80211_mgmt {
	le16_t frame_control;
	le16_t duration;
	uint8_t da[6];
	uint8_t sa[6];
	uint8_t bssid[6];
	le16_t seq_ctrl;
	union {
		struct {
			le16_t auth_alg;
			le16_t auth_transaction;
			le16_t status_code;
			/* possibly followed by Challenge text */
			uint8_t variable[0];
		} __attribute__ ((packed)) auth;
		struct {
			le16_t reason_code;
		} __attribute__ ((packed)) deauth;
		struct {
			le16_t capab_info;
			le16_t listen_interval;
			/* followed by SSID and Supported rates */
			U8 variable[0];
		} __attribute__ ((packed)) assoc_req;
		struct {
			le16_t capab_info;
			le16_t status_code;
			le16_t aid;
			/* followed by Supported rates */
			U8 variable[0];
		} __attribute__ ((packed)) assoc_resp, reassoc_resp;
		struct {
			le16_t capab_info;
			le16_t listen_interval;
			U8 current_ap[6];
			/* followed by SSID and Supported rates */
			U8 variable[0];
		} __attribute__ ((packed)) reassoc_req;
		struct {
			le16_t reason_code;
		} __attribute__ ((packed)) disassoc;
		struct {
			uint64_t timestamp;
			le16_t beacon_int;
			le16_t capab_info;
			/* followed by some of SSID, Supported rates,
			 * FH Params, DS Params, CF Params, IBSS Params, TIM */
			U8 variable[0];
		} __attribute__ ((packed)) beacon;
		struct {
			/* only variable items: SSID, Supported rates */
			U8 variable[0];
		} __attribute__ ((packed)) probe_req;
		struct {
			uint64_t timestamp;
			le16_t beacon_int;
			le16_t capab_info;
			/* followed by some of SSID, Supported rates,
			 * FH Params, DS Params, CF Params, IBSS Params */
			U8 variable[0];
		} __attribute__ ((packed)) probe_resp;
		struct {
			U8 category;
			union {
				struct {
					U8 action_code;
					U8 dialog_token;
					U8 status_code;
					U8 variable[0];
				} __attribute__ ((packed)) wme_action;
				struct{
					U8 action_code;
					U8 element_id;
					U8 length;
					struct ieee80211_channel_sw_ie sw_elem;
				} __attribute__((packed)) chan_switch;
				struct{
					U8 action_code;
					U8 dialog_token;
					U8 element_id;
					U8 length;
					struct ieee80211_msrment_ie msr_elem;
				} __attribute__((packed)) measurement;
				struct{
					U8 action_code;
					U8 dialog_token;
					le16_t capab;
					le16_t timeout;
					le16_t start_seq_num;
				} __attribute__((packed)) addba_req;
				struct{
					U8 action_code;
					U8 dialog_token;
					le16_t status;
					le16_t capab;
					le16_t timeout;
				} __attribute__((packed)) addba_resp;
				struct{
					U8 action_code;
					le16_t params;
					le16_t reason_code;
				} __attribute__((packed)) delba;
				struct {
					U8 action_code;
					U8 variable[0];
				} __attribute__((packed)) self_prot;
				struct{
					U8 action_code;
					U8 variable[0];
				} __attribute__((packed)) mesh_action;
				struct {
					U8 action;
					U8 trans_id[WLAN_SA_QUERY_TR_ID_LEN];
				} __attribute__ ((packed)) sa_query;
				struct {
					U8 action;
					U8 smps_control;
				} __attribute__ ((packed)) ht_smps;
				struct {
					U8 action_code;
					U8 dialog_token;
					le16_t capability;
					U8 variable[0];
				} __attribute__ ((packed)) tdls_discover_resp;
			} u;
		} __attribute__ ((packed)) action;
	} u;
} __attribute__ ((packed));


struct ieee80211_hdr {
	le16_t frame_control;
	le16_t duration_id;
	uint8_t addr1[ETH_ALEN];
	uint8_t addr2[ETH_ALEN];
	uint8_t addr3[ETH_ALEN];
	le16_t seq_ctrl;
	uint8_t addr4[ETH_ALEN];
} __attribute__((packed,aligned(2)));

struct ieee80211_hdr_3addr {
	         le16_t frame_control;
	         le16_t duration_id;
	         uint8_t addr1[ETH_ALEN];
	         uint8_t addr2[ETH_ALEN];
	         uint8_t addr3[ETH_ALEN];
	         le16_t seq_ctrl;
} __attribute__((packed,aligned(2)));

struct ieee80211_qos_hdr {
	         le16_t frame_control;
	         le16_t duration_id;
	         uint8_t addr1[ETH_ALEN];
	         uint8_t addr2[ETH_ALEN];
	         uint8_t addr3[ETH_ALEN];
	         le16_t seq_ctrl;
	         le16_t qos_ctrl;
} __attribute__((packed,aligned(2)));




/**
 * ieee80211_has_tods - check if IEEE80211_FCTL_TODS is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_tods(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_TODS)) != 0;
}

/**
 * ieee80211_has_fromds - check if IEEE80211_FCTL_FROMDS is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_fromds(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FROMDS)) != 0;
}

/**
 * ieee80211_has_a4 - check if IEEE80211_FCTL_TODS and IEEE80211_FCTL_FROMDS are set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_a4(le16_t fc)
{
	le16_t tmp = cpu_to_le16(IEEE80211_FCTL_TODS | IEEE80211_FCTL_FROMDS);
	return (fc & tmp) == tmp;
}

/**
 * ieee80211_has_morefrags - check if IEEE80211_FCTL_MOREFRAGS is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_morefrags(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_MOREFRAGS)) != 0;
}

/**
 * ieee80211_has_retry - check if IEEE80211_FCTL_RETRY is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_retry(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_RETRY)) != 0;
}

/**
 * ieee80211_has_pm - check if IEEE80211_FCTL_PM is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_pm(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_PM)) != 0;
}

/**
 * ieee80211_has_moredata - check if IEEE80211_FCTL_MOREDATA is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_moredata(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_MOREDATA)) != 0;
}

/**
 * ieee80211_has_protected - check if IEEE80211_FCTL_PROTECTED is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_protected(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_PROTECTED)) != 0;
}

/**
 * ieee80211_has_order - check if IEEE80211_FCTL_ORDER is set
 * @fc: frame control bytes in little-endian byteorder
 */
inline int ieee80211_has_order(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_ORDER)) != 0;
}

/**
 * ieee80211_is_data - check if type is IEEE80211_FTYPE_DATA
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_data(le16_t fc)
{
	        return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	               cpu_to_le16(IEEE80211_FTYPE_DATA);
}

/**
 * ieee80211_is_data_qos - check if type is IEEE80211_FTYPE_DATA and IEEE80211_STYPE_QOS_DATA is set
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int 
ieee80211_is_data_qos(le16_t fc)
{
	/*
	 * mask with QOS_DATA rather than IEEE80211_FCTL_STYPE as we just need
	 * to check the one bit
	 */
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_STYPE_QOS_DATA)) ==
	           cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_QOS_DATA);
}

/**
 * ieee80211_is_data_present - check if type is IEEE80211_FTYPE_DATA and has data
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int 
ieee80211_is_data_present(le16_t fc)
{
	/*
	 * mask with 0x40 and test that that bit is clear to only return true
	 * for the data-containing substypes.
	 */
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | 0x40)) ==
	  cpu_to_le16(IEEE80211_FTYPE_DATA);
}

/**
 * ieee80211_is_ack - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_ACK
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int 
ieee80211_is_ack(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	          cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_ACK);
}

/**
 * ieee80211_is_action - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_ACTION
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int 
ieee80211_is_action(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	             cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ACTION);
}

/**
 * ieee80211_is_back_req - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_BACK_REQ
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int 
ieee80211_is_back_req(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	               cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_BACK_REQ);
}

/**
 * ieee80211_is_back - check if IEEE80211_FTYPE_CTL && IEEE80211_STYPE_BACK
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int 
ieee80211_is_back(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	               cpu_to_le16(IEEE80211_FTYPE_CTL | IEEE80211_STYPE_BACK);
}

static inline int 
ieee80211_is_beacon(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
				cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
}

/**
 * ieee80211_is_atim - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_ATIM
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int 
ieee80211_is_atim(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
				cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_ATIM);
}

/**
 * ieee80211_is_ctl - check if type is IEEE80211_FTYPE_CTL
 * @fc: frame control bytes in little-endian byteorder
 */
static inline int ieee80211_is_ctl(le16_t fc)
{
	return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE)) ==
	   cpu_to_le16(IEEE80211_FTYPE_CTL);
}

/**
430  * ieee80211_is_probe_resp - check if IEEE80211_FTYPE_MGMT && IEEE80211_STYPE_PROBE_RESP
431  * @fc: frame control bytes in little-endian byteorder
432  */
static inline int ieee80211_is_probe_resp(le16_t fc)
{
	         return (fc & cpu_to_le16(IEEE80211_FCTL_FTYPE | IEEE80211_FCTL_STYPE)) ==
	                cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP);
}


/**
 * ieee80211_get_qos_ctl - get pointer to qos control bytes
 * @hdr: the frame
 *
 * The qos ctrl bytes come after the frame_control, duration, seq_num
 * and 3 or 4 addresses of length ETH_ALEN.
 * 3 addr: 2 + 2 + 2 + 3*6 = 24
 * 4 addr: 2 + 2 + 2 + 4*6 = 30
 */
inline U8 *ieee80211_get_qos_ctl(struct ieee80211_hdr *hdr)
{
	if (ieee80211_has_a4(hdr->frame_control))
		return (U8 *)hdr + 30;
	else
		return (U8 *)hdr + 24;
}

/**
2036  * struct ieee80211_timeout_interval_ie - Timeout Interval element
2037  * @type: type, see &enum ieee80211_timeout_interval_type
2038  * @value: timeout interval value
2039  */
struct ieee80211_timeout_interval_ie {
	uint8_t type;
	le32_t value;
} __attribute__((packed));


#endif /* IEEE80211_H_ */
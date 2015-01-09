/*
 * ar9170_eeprom.h
 *
 * Created: 2/7/2014 9:44:29 AM
 *  Author: ioannisg
 */ 


#ifndef AR9170_EEPROM_H_
#define AR9170_EEPROM_H_


#define AR9170_EEPROM_START             0x1600
#define AR5416_MAX_CHAINS               2
#define AR5416_MODAL_SPURS              5


struct ar9170_eeprom_modal {
	le32_t  antCtrlChain[AR5416_MAX_CHAINS];
	le32_t  antCtrlCommon;
	int8_t  antennaGainCh[AR5416_MAX_CHAINS];
	uint8_t switchSettling;
	uint8_t txRxAttenCh[AR5416_MAX_CHAINS];
	uint8_t rxTxMarginCh[AR5416_MAX_CHAINS];
	int8_t  adcDesiredSize;
	int8_t  pgaDesiredSize;
	uint8_t xlnaGainCh[AR5416_MAX_CHAINS];
	uint8_t txEndToXpaOff;
	uint8_t txEndToRxOn;
	uint8_t txFrameToXpaOn;
	uint8_t thresh62;
	int8_t  noiseFloorThreshCh[AR5416_MAX_CHAINS];
	uint8_t xpdGain;
	uint8_t xpd;
	int8_t iqCalICh[AR5416_MAX_CHAINS];
	int8_t iqCalQCh[AR5416_MAX_CHAINS];
	uint8_t pdGainOverlap;
	uint8_t ob;
	uint8_t db;
	uint8_t xpaBiasLvl;
	uint8_t pwrDecreaseFor2Chain;
	uint8_t pwrDecreaseFor3Chain;
	uint8_t txFrameToDataStart;
	uint8_t txFrameToPaOn;
	uint8_t ht40PowerIncForPdadc;
	uint8_t bswAtten[AR5416_MAX_CHAINS];
	uint8_t bswMargin[AR5416_MAX_CHAINS];
	uint8_t swSettleHt40;
	uint8_t reserved[22];
	struct spur_channel {
		le16_t spurChan;
		uint8_t spurRangeLow;
		uint8_t spurRangeHigh;
	} __attribute__((packed)) spur_channels[AR5416_MODAL_SPURS];
} __attribute__((packed));

#define AR5416_NUM_PD_GAINS             4
#define AR5416_PD_GAIN_ICEPTS           5

struct ar9170_calibration_data_per_freq {
	uint8_t pwr_pdg[AR5416_NUM_PD_GAINS][AR5416_PD_GAIN_ICEPTS];
	uint8_t vpd_pdg[AR5416_NUM_PD_GAINS][AR5416_PD_GAIN_ICEPTS];
} __attribute__((packed));

#define AR5416_NUM_5G_CAL_PIERS         8
#define AR5416_NUM_2G_CAL_PIERS         4

#define AR5416_NUM_5G_TARGET_PWRS       8
#define AR5416_NUM_2G_CCK_TARGET_PWRS   3
#define AR5416_NUM_2G_OFDM_TARGET_PWRS  4
#define AR5416_MAX_NUM_TGT_PWRS         8

struct ar9170_calibration_target_power_legacy {
	uint8_t      freq;
	uint8_t      power[4];
} __attribute__((packed));

struct ar9170_calibration_target_power_ht {
	uint8_t      freq;
	uint8_t      power[8];
} __attribute__((packed));

#define AR5416_NUM_CTLS                 24

struct ar9170_calctl_edges {
	uint8_t      channel;
	#define AR9170_CALCTL_EDGE_FLAGS        0xC0
	uint8_t      power_flags;
} __attribute__((packed));

#define AR5416_NUM_BAND_EDGES           8

struct ar9170_calctl_data {
	struct ar9170_calctl_edges
	control_edges[AR5416_MAX_CHAINS][AR5416_NUM_BAND_EDGES];
} __attribute__((packed));


struct ar9170_eeprom {
	le16_t  length;
	le16_t  checksum;
	le16_t  version;
	uint8_t operating_flags;
	#define AR9170_OPFLAG_5GHZ              1
	#define AR9170_OPFLAG_2GHZ              2
	uint8_t misc;
	le16_t  reg_domain[2];
	uint8_t mac_address[6];
	uint8_t rx_mask;
	uint8_t tx_mask;
	le16_t rf_silent;
	le16_t bluetooth_options;
	le16_t device_capabilities;
	le32_t build_number;
	uint8_t deviceType;
	uint8_t reserved[33];
	
	uint8_t customer_data[64];
	
	struct ar9170_eeprom_modal modal_header[2];
	
	uint8_t cal_freq_pier_5G[AR5416_NUM_5G_CAL_PIERS];
	uint8_t cal_freq_pier_2G[AR5416_NUM_2G_CAL_PIERS];
	
	struct ar9170_calibration_data_per_freq
		cal_pier_data_5G[AR5416_MAX_CHAINS][AR5416_NUM_5G_CAL_PIERS],
		cal_pier_data_2G[AR5416_MAX_CHAINS][AR5416_NUM_2G_CAL_PIERS];
	
	/* power calibration data */
	struct ar9170_calibration_target_power_legacy
		cal_tgt_pwr_5G[AR5416_NUM_5G_TARGET_PWRS];
	struct ar9170_calibration_target_power_ht
		cal_tgt_pwr_5G_ht20[AR5416_NUM_5G_TARGET_PWRS],
		cal_tgt_pwr_5G_ht40[AR5416_NUM_5G_TARGET_PWRS];
	
	struct ar9170_calibration_target_power_legacy
		cal_tgt_pwr_2G_cck[AR5416_NUM_2G_CCK_TARGET_PWRS],
		cal_tgt_pwr_2G_ofdm[AR5416_NUM_2G_OFDM_TARGET_PWRS];
	struct ar9170_calibration_target_power_ht
		cal_tgt_pwr_2G_ht20[AR5416_NUM_2G_OFDM_TARGET_PWRS],
		cal_tgt_pwr_2G_ht40[AR5416_NUM_2G_OFDM_TARGET_PWRS];
	
	/* conformance testing limits */
	uint8_t ctl_index[AR5416_NUM_CTLS];
	struct ar9170_calctl_data ctl_data[AR5416_NUM_CTLS];
	
	uint8_t pad;
	le16_t  subsystem_id;
} __attribute__((packed));

#define AR9170_LED_MODE_POWER_ON                0x0001
#define AR9170_LED_MODE_RESERVED                0x0002
#define AR9170_LED_MODE_DISABLE_STATE           0x0004
#define AR9170_LED_MODE_OFF_IN_PSM              0x0008

/* AR9170_LED_MODE BIT is set */
#define AR9170_LED_MODE_FREQUENCY_S             4
#define AR9170_LED_MODE_FREQUENCY               0x0030
#define AR9170_LED_MODE_FREQUENCY_1HZ           0x0000
#define AR9170_LED_MODE_FREQUENCY_0_5HZ         0x0010
#define AR9170_LED_MODE_FREQUENCY_0_25HZ        0x0020
#define AR9170_LED_MODE_FREQUENCY_0_125HZ       0x0030

/* AR9170_LED_MODE BIT is not set */
#define AR9170_LED_MODE_CONN_STATE_S            4
#define AR9170_LED_MODE_CONN_STATE              0x0030
#define AR9170_LED_MODE_CONN_STATE_FORCE_OFF    0x0000
#define AR9170_LED_MODE_CONN_STATE_FORCE_ON     0x0010
/* Idle off / Active on */
#define AR9170_LED_MODE_CONN_STATE_IOFF_AON     0x0020
/* Idle on / Active off */
#define AR9170_LED_MODE_CONN_STATE_ION_AOFF     0x0010

#define AR9170_LED_MODE_MODE                    0x0040
#define AR9170_LED_MODE_RESERVED2               0x0080

#define AR9170_LED_MODE_TON_SCAN_S              8
#define AR9170_LED_MODE_TON_SCAN                0x0f00

#define AR9170_LED_MODE_TOFF_SCAN_S             12
#define AR9170_LED_MODE_TOFF_SCAN               0xf000

struct ar9170_led_mode {
	le16_t led;
};

#endif /* AR9170_EEPROM_H_ */
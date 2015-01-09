/*
 * ath.h
 *
 * Created: 1/30/2014 1:40:47 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ieee80211\include\if_ether.h"

#ifndef ATH_H_
#define ATH_H_

struct reg_dmn_pair_mapping {
	uint16_t regDmnEnum;
	uint16_t reg_5ghz_ctl;
	uint16_t reg_2ghz_ctl;
};

struct ath_regulatory {
	char alpha2[2];
	uint16_t country_code;
	uint16_t max_power_level;
	uint16_t current_rd;
	int16_t power_limit;
	struct reg_dmn_pair_mapping regpair; // XXX removed pointer
};

struct ath_common {

	uint16_t curaid;
	uint8_t macaddr[ETH_ALEN];
	uint8_t curbssid[ETH_ALEN];	
	
	struct ath_regulatory regulatory;
};

enum ctl_group {
	CTL_FCC = 0x10,
	CTL_MKK = 0x40,
	CTL_ETSI = 0x30,
};

#define NO_CTL					0xff
#define SD_NO_CTL               0xE0
#define NO_CTL                  0xff
#define CTL_11A                 0
#define CTL_11B                 1
#define CTL_11G                 2
#define CTL_2GHT20              5
#define CTL_5GHT20              6
#define CTL_2GHT40              7
#define CTL_5GHT40              8

#define CTRY_DEBUG 0x1ff
#define CTRY_DEFAULT 0

#define COUNTRY_ERD_FLAG        0x8000 

#define WORLD  0x0199

#define WORLD_SKU_MASK          0x00F0
#define WORLD_SKU_PREFIX        0x0060

#endif /* ATH_H_ */
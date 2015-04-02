/*
 * ar9170_psm.h
 *
 * Created: 2/12/2014 10:40:08 AM
 *  Author: Ioannis Glaropoulos
 */ 
#ifndef AR9170_PSM_H_
#define AR9170_PSM_H_

#include "compiler.h"
#include "rtimer.h"

/** AR9170 PSM states */
enum ar9170_psm_state {
	AR9170_PSM_NO,
	AR9170_PSM_TBTT,
	AR9170_PSM_ATIM,
	AR9170_PSM_DATA,
};

/** AR9170 PSM Action to be performed */
enum ar9170_psm_action {
	AR9170_PSM_NO_ACTION,
	AR9170_PSM_TRANSIT_WAKE,
	AR9170_PSM_TRANSIT_DOZE,
	AR9170_PSM_TRANSIT_SOFT,	
};


struct psm_manager {
	
	enum ar9170_psm_state state;
	enum ar9170_psm_action next_action;
};
	
/** Exported PSM API */
static rtimer_clock_t ar9170_psm_get_next_tbtt_time(void);

#include "ar9170.h"

void ar9170_psm_handle_tbtt(struct ar9170* ar);
void ar9170_psm_handle_bcn_config(struct ar9170* ar, uint32_t bcns_sent);
void ar9170_psm_update_stats(struct ar9170* ar);
#endif /* AR9170_PSM_H_ */

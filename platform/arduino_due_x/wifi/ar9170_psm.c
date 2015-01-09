/*
 * ar9170_psm.c
 *
 * Interrupt handlers for AR9170 PSM duty-cycle events
 *
 * Created: 2/12/2014 10:32:06 AM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ar9170_psm.h"
#include "ar9170_hw.h"
#include "ar9170-drv.h"

#include "rtimer.h"
#include "wire_digital.h"

#include <stdio.h>
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#include "platform-conf.h"
#if WITH_WIFI_SUPPORT

static struct rtimer ar9170_psm_rtimer;
static struct rtimer ar9170_psm_atim_rtimer;



/**
 * This function is called within TC interrupt context.
 */
static void
ar9170_psm_handle_atim_w_end(struct rtimer *t, void *ptr)
{
	struct ar9170* ar = (struct ar9170*)ptr;
	
	PRINTF("AE\n");
	/* Set device state to DATA TX Window */
	if (unlikely(ar->psm_mgr.state != AR9170_PSM_ATIM)) {
		printf("AR9170_PSM: wrong-state-ATIM-End (%u)\n",ar->psm_mgr.state);
	}
	ar->psm_mgr.state = AR9170_PSM_DATA;
	
	/* Cancel filtering of DATA packets and order PSM check */
	ar9170_action_t actions = AR9170_DRV_ACTION_CANCEL_RX_FILTER |
		AR9170_DRV_ACTION_CHECK_PSM;
		
	/* Collect duty-cycling statistics */
	
	/* Order the actions to the driver scheduler */
	ar9170_drv_order_action(ar, actions);
}

/**
 * This function is called within TC interrupt context.
 */
static void
ar9170_psm_handle_atim_w_start(struct rtimer *t, void *ptr)
{	
	PRINTF("AS\n");
	struct ar9170* ar = (struct ar9170*)ptr;
	
	/* Schedule ATIM Window End */
	if(rtimer_set(&ar9170_psm_atim_rtimer, 
		(RTIMER_NOW()+(RTIMER_SECOND/1000)*(ar->vif->bss_conf.atim_window)),
		NULL, ar9170_psm_handle_atim_w_end, ar)) {
		printf("AR9170_PSM: err-setting-atim-end-rtimer\n");
		return;
	}
	/* Set device state to ATIM Window */
	if (unlikely(ar->psm_mgr.state != AR9170_PSM_TBTT)) {
		printf("AR9170_PSM: wrong state at ATIM Start (%u)\n",ar->psm_mgr.state);
	}
	ar->psm_mgr.state = AR9170_PSM_ATIM;
	
	if (list_length(ar->ar9170_tx_list)) {
		printf("AR9170_PSM: tx-list-still-non-empty\n");
		return;
	}
	/* Order ATIM flush */
	ar9170_action_t actions = AR9170_DRV_ACTION_FLUSH_OUT_ATIMS;
	
	/* Collect duty-cycling statistics */
	
	/* Order the actions to the driver scheduler */
	ar9170_drv_order_action(ar, actions);
}

/**
 * This function is called within USB interrupt context.
 */
static rtimer_clock_t next_target_tbtt_time;

void
ar9170_psm_handle_tbtt(struct ar9170* ar)
{
	PRINTF("T\n");	
	rtimer_clock_t current_time = RTIMER_NOW();
	if (unlikely(!ar->is_joined)) 
		return;

	/* Schedule ATIM Window Start */
	if(rtimer_set(&ar9170_psm_rtimer, 
		(current_time + ((RTIMER_SECOND)/1000)*AR9170_PRETBTT_KUS), 
		NULL, ar9170_psm_handle_atim_w_start, ar)) {
		printf("AR9170_PSM: err-setting-atim-start-rtimer\n");
		return;
	}	
	/* Register next target pre-TBTT interrupt time */
	next_target_tbtt_time = current_time + 
		((RTIMER_SECOND)/1000)*(ar->vif->bss_conf.beacon_int);
	
	/* Set device state to TBTT */
	if (unlikely(ar->psm_mgr.state != AR9170_PSM_DATA)) {
		printf("AR9170_PSM: wrong state at TBTT (%u)\n",ar->psm_mgr.state);
	}
	ar->psm_mgr.state = AR9170_PSM_TBTT;
	/* Cancel pending outgoing Data transmissions if in PSM */
	uint8_t tx_len = list_length(ar->ar9170_tx_list); 
	if (unlikely(tx_len > 0)) {
		printf("AR9170_PSM: clear-tx-at-tbtt (%u)\n",tx_len);
		int i;
		for (i=0; i<tx_len; i++) {
			ar9170_pkt_buf_t* pkt = list_pop(ar->ar9170_tx_list);
			if(pkt != NULL) {
				memb_free(ar->ar9170_tx_list, pkt);
			}
		}		
	}	
	/* Schedule filtering-out the incoming packets
	 * if in PSM, order ATIM generation and beacon
	 * update.
	 */
	ar9170_action_t actions = AR9170_DRV_ACTION_GENERATE_ATIMS |
		AR9170_DRV_ACTION_FILTER_RX_DATA |
		AR9170_DRV_ACTION_UPDATE_BEACON;
		
	/* Collect duty-cycling statistics */
	if (wire_digital_read(PRE_TBTT_ACTIVE_PIN, PIO_TYPE_PIO_OUTPUT_0) == HIGH)
		wire_digital_write(PRE_TBTT_ACTIVE_PIN, LOW);
	else
		wire_digital_write(PRE_TBTT_ACTIVE_PIN, HIGH);
	wire_digital_write(DOZE_ACTIVE_PIN, LOW);
	
	/* Order the actions to the driver scheduler */
	ar9170_drv_order_action(ar, actions);
}

/**
 * This function is called within USB interrupt context.
 */
void
ar9170_psm_handle_bcn_config(struct ar9170* ar, uint32_t bcns_sent)
{
	PRINTF("B\n");
	if (ar->psm_mgr.state != AR9170_PSM_ATIM)
		PRINTF("AR9170_PSM: wrong-state-at-bcn (%u)\n",ar->psm_mgr.state);
	ar->ps.off_override |= PS_OFF_BCN; 
}

void
ar9170_psm_update_stats(struct ar9170* ar)
{
	
}

static rtimer_clock_t
ar9170_psm_get_next_tbtt_time(void)
{
	return next_target_tbtt_time;
}
#endif /* WITH_SIFI_SUPPORT */
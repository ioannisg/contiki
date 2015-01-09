/*
 * ar9170_rx.c
 *
 * RX handling functions for AR9170 driver
 *
 * Created: 1/28/2014 10:16:41 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include <sys\errno.h>

#include "ar9170.h"
#include "ar9170_fwcmd.h"
#include "ar9170_wlan.h"
#include "ar9170_psm.h"
#include "ieee80211\mac80211.h"
#include "ieee80211\ieee80211.h"
#include "list.h"
#include "ieee80211\include\etherdevice.h"
#include "ar9170-drv.h"
#include "ieee80211_i.h"

#include "compiler.h"
#include "rtimer.h"
#include "pend-SV.h"
#include "usbstack.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#include "platform-conf.h"
#if WITH_WIFI_SUPPORT

/**
 * clamp - return a value clamped to a given range with strict typechecking
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * This macro does strict typechecking of min/max to make sure they are of the
 * same type as val.  See the unnecessary pointer comparisons.
 */
#define clamp(val, min, max) ({                 \
	typeof(val) __val = (val);              \
	typeof(min) __min = (min);              \
	typeof(max) __max = (max);              \
	(void) (&__val == &__min);              \
	(void) (&__val == &__max);              \
	__val = __val < __min ? __min: __val;   \
	__val > __max ? __max: __val; })

#define AR9170_MAGIC_RSP_HDR_LENGTH		15
#define AR9170_MAGIC_RSP_0XFF_HDR_LEN	12
#define AR9170_MAGIC_RSP_NON_FF_LEN		((AR9170_MAGIC_RSP_HDR_LENGTH) - (AR9170_MAGIC_RSP_0XFF_HDR_LEN))
#define AR9170_MIN_HDR_LEN				4

const uint8_t magic_command_header[AR9170_MAGIC_RSP_HDR_LENGTH] = {0x00, 0x00, 0x4e, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

/**
 * This function is called within USB interrupt context.
 */
static void
ar9170_trap_beacon(struct ar9170* ar, void *data, unsigned int len)
{		
	/* If this is the intended beacon, meaning that both the
	 * BSSID and the SSID match request the AR9170 driver to
	 * set the beacon timers and complete the IBSS register
	 * process off line. We also cancel the scanning here. 
	 * TODO - strictly speaking, the scanning cancellation
	 * must be ordered from above, as the IEEE80211 manager
	 * should decide whether to accept the arrived beacon or
	 * not. FIXME fixed.
	 */
	struct ieee80211_mgmt *mgmt = data;
		
	/* check if this really is a beacon */
	if (!ieee80211_is_beacon(mgmt->frame_control))
		return;
	
	/* min. beacon length + FCS_LEN */
	if (len <= 40 + FCS_LEN)
		return;
	
	/* and only beacons from the associated BSSID, please */
	if (!ether_addr_equal(mgmt->bssid, ar->common.curbssid))
		return;
	
	/* Beacon trapped */
	USB_STACK.bulk_in_set_limit(32);
	
	/* Register beacon interval */	
	ieee80211_ibss_vif.bss_conf.beacon_int = le16_to_cpu(mgmt->u.beacon.beacon_int);
			
	ar9170_action_t action;
	
	/* On-line processing - beacon and Atim timers */
	PRINTF("AR9170_RX: recv-bcn-int(%04x)\n",le16_to_cpu(mgmt->u.beacon.beacon_int));
#if 0		
	struct ieee802_11_elems _elems;
	
	/* Process beacon from the current [default] IBSS */	
	size_t baselen = (uint8_t *) mgmt->u.beacon.variable - (uint8_t *) mgmt;	
	if (baselen > len) {
		printf("WARNING: Received beacon too small.\n");
		return;
	}		
	/* Parse the elements sent with the currently received beacon JUST for the ATIM! */
	ieee802_11_parse_elems(mgmt->u.beacon.variable, len - baselen - FCS_LEN, 0, &_elems);
	uint8_t *ssid = _elems.ssid;
	
	PRINTF("SSID: %02x:%02x:%02x:%02x:%02x:%02x\n",ssid[0],ssid[1],ssid[2],ssid[3],ssid[4],ssid[5]);
	#if DEBUG
	int i;
	PRINTF("DEBUG: IBSS; IBSS Params: ");
	for (i=0; i<_elems.ibss_params_len; i++) {
		PRINTF("%02x ", _elems.ibss_params[i]);
	}
	PRINTF("\n");
	#endif	
	ieee80211_ibss_vif.bss_conf.atim_window = *((le16_t*)_elems.ibss_params);	
#endif	
	action = AR9170_DRV_ACTION_JOIN_DEFAULT_IBSS;
	/* Order low-priority interrupt handling */
	ar9170_drv_order_action(ar, action);
}
/**
 * This function is called within USB interrupt context.
 */
static ar9170_pkt_buf_t*
ar9170_rx_copy_data(uint8_t *buf, int len)
{
	ar9170_pkt_buf_t *new_pkt;
	
	struct memb *pkt_memb;
	pkt_memb = ar9170_drv_get_rx_queue();
	if (pkt_memb == NULL) {
		printf("AR9170_RX: no available mem block\n");
		return NULL;
	}
	
	new_pkt =  memb_alloc(pkt_memb);
	if (new_pkt == NULL) {
		printf("AR9170_RX: no available mem\n");
		return NULL;
	}
	
	new_pkt->next = NULL;
	new_pkt->pkt_len = len;	
	/* Copy packet data */
	memcpy(new_pkt->pkt_data, buf, len);
	PRINTF("AR9170_RX: rx-copied\n");
	return new_pkt;
}

static int 
ar9170_rx_mac_status(struct ar9170 *ar,
	struct ar9170_rx_head *head, struct ar9170_rx_macstatus *mac, struct ieee80211_rx_status *status)
{
	struct ieee80211_channel *chan;
	uint8_t error, decrypt;
	
	if(sizeof(struct ar9170_rx_head) != 12)
		PRINTF("AR9170_RX: rx_head not of size 12\n");
	if(sizeof(struct ar9170_rx_macstatus) != 4)
		PRINTF("AR9170_RX: rx_macstatus not of size 4\n");
	
	error = mac->error;
	
	if (error & AR9170_RX_ERROR_WRONG_RA) {
		if (!ar->sniffer_enabled)
			return -EINVAL;
	}
		
	if (error & AR9170_RX_ERROR_PLCP) {
		if (!(ar->filter_state & FIF_PLCPFAIL))
			return -EINVAL;
		
		status->flag |= RX_FLAG_FAILED_PLCP_CRC;
	}
		
	if (error & AR9170_RX_ERROR_FCS) {
		ar->tx_fcs_errors++;
		
		if (!(ar->filter_state & FIF_FCSFAIL))
			return -EINVAL;
		
		status->flag |= RX_FLAG_FAILED_FCS_CRC;
	}
		
	decrypt = ar9170_get_decrypt_type(mac);
	if (!(decrypt & AR9170_RX_ENC_SOFTWARE) && decrypt != AR9170_ENC_ALG_NONE) {
		if ((decrypt == AR9170_ENC_ALG_TKIP) && (error & AR9170_RX_ERROR_MMIC))
			status->flag |= RX_FLAG_MMIC_ERROR;
		
		status->flag |= RX_FLAG_DECRYPTED;
	}
	
	if (error & AR9170_RX_ERROR_DECRYPT && !ar->sniffer_enabled)
		return -ENODATA;
		
	error &= ~(AR9170_RX_ERROR_MMIC |
	            AR9170_RX_ERROR_FCS |
	            AR9170_RX_ERROR_WRONG_RA |
	            AR9170_RX_ERROR_DECRYPT |
	            AR9170_RX_ERROR_PLCP);
	/* drop any other error frames */
	if (unlikely(error)) {
		/* TODO: update netdevice's RX dropped/errors statistics */
	/*			
		if (net_ratelimit())
			wiphy_dbg(ar->hw->wiphy, "received frame with "
			      "suspicious error code (%#x).\n", error);
	*/		
		return -EINVAL;
	}
	
	chan = ar->channel;
	if (chan) {
		status->band = chan->band;
	    status->freq = chan->center_freq;
	}
	
	switch (mac->status & AR9170_RX_STATUS_MODULATION) {
		case AR9170_RX_STATUS_MODULATION_CCK:
			if (mac->status & AR9170_RX_STATUS_SHORT_PREAMBLE)
				status->flag |= RX_FLAG_SHORTPRE;
			switch (head->plcp[0]) {
				case AR9170_RX_PHY_RATE_CCK_1M:
					status->rate_idx = 0;
					break;
				case AR9170_RX_PHY_RATE_CCK_2M:
					status->rate_idx = 1;
					break;
				case AR9170_RX_PHY_RATE_CCK_5M:
					status->rate_idx = 2;
					break;
				case AR9170_RX_PHY_RATE_CCK_11M:
					status->rate_idx = 3;
					break;
				default:
				/*	
					if (net_ratelimit()) {
						wiphy_err(ar->hw->wiphy, "invalid plcp cck "
							"rate (%x).\n", head->plcp[0]);
					}
				*/
					return -EINVAL;
			}
			break;
			
		case AR9170_RX_STATUS_MODULATION_DUPOFDM:
		case AR9170_RX_STATUS_MODULATION_OFDM:
			switch (head->plcp[0] & 0xf) {
				case AR9170_TXRX_PHY_RATE_OFDM_6M:
					status->rate_idx = 0;
					break;
				case AR9170_TXRX_PHY_RATE_OFDM_9M:
					status->rate_idx = 1;
					break;
				case AR9170_TXRX_PHY_RATE_OFDM_12M:
					status->rate_idx = 2;
					break;
				case AR9170_TXRX_PHY_RATE_OFDM_18M:
					status->rate_idx = 3;
					break;
				case AR9170_TXRX_PHY_RATE_OFDM_24M:
					status->rate_idx = 4;
					break;
				case AR9170_TXRX_PHY_RATE_OFDM_36M:
					status->rate_idx = 5;
					break;
				case AR9170_TXRX_PHY_RATE_OFDM_48M:
					status->rate_idx = 6;
					break;
				case AR9170_TXRX_PHY_RATE_OFDM_54M:
					status->rate_idx = 7;
					break;
				default:
				/*	
					if (net_ratelimit()) {
								wiphy_err(ar->hw->wiphy, "invalid plcp ofdm "
			                                         "rate (%x).\n", head->plcp[0]);
					}
				*/
					return -EINVAL;
			}
			if (status->band == IEEE80211_BAND_2GHZ)
				status->rate_idx += 4;
			break;
		case AR9170_RX_STATUS_MODULATION_HT:
			if (head->plcp[3] & 0x80)
				status->flag |= RX_FLAG_40MHZ;
			if (head->plcp[6] & 0x80)
					status->flag |= RX_FLAG_SHORT_GI;
		
			status->rate_idx = clamp(0, 75, head->plcp[3] & 0x7f);
			status->flag |= RX_FLAG_HT;
			break;
		
		default:
			PRINTF("AR9170_RX: MAC_STATUS enterd default switch case\n");
		    return -ENOSYS;
	}
	
	return 0;
}


static void 
ar9170_rx_phy_status(struct ar9170 *ar,
         struct ar9170_rx_phystatus *phy, struct ieee80211_rx_status *status)
{
	int i;
	
	if(sizeof(struct ar9170_rx_phystatus) != 20) {
		PRINTF("AR9170_RX: ar9170_rx_phystatus not 20\n");
	}
	
	for (i = 0; i < 3; i++)
		if (phy->rssi[i] != 0x80)
			status->antenna |= BIT(i);
			
			/* post-process RSSI */
			for (i = 0; i < 7; i++)
				if (phy->rssi[i] & 0x80)
					phy->rssi[i] = ((phy->rssi[i] & 0x7f) + 1) & 0x7f;
				
				/* TODO: we could do something with phy_errors */
				status->signal = ar->noise[0] + phy->rssi_combined;
}

static void ar9170_ps_atim( struct ar9170 * ar, uint8_t * buf, int len )
{
	struct ieee80211_hdr *hdr = buf;
	
	if (likely(!(ar->hw->conf.flags & IEEE80211_CONF_PS)))
		return;
	
	/* check if this really is an ATIM */
	if (!ieee80211_is_atim(hdr->frame_control))
		return;
		
	ar->ps.off_override |= PS_OFF_ATIM;	
}

/**
 * NOTE:
 *
 * The firmware is in charge of waking up the device just before
 * the AP is expected to transmit the next beacon.
 *
 * This leaves the driver with the important task of deciding when
 * to set the PHY back to bed again.
 *
 * However, depending on the state of the IBSS, we process beacons
 * ONLINE, in order to speed up the process of synchronizing with
 * an IBSS.
 * 
 * This function is called within USB interrupt context.
 *
 */
static void 
ar9170_ps_beacon(struct ar9170 *ar, void *data, unsigned int len)
{
	struct ieee80211_hdr *hdr = data;
	//struct ieee80211_tim_ie *tim_ie;
	//u8 *tim;
	//u8 tim_len;
	uint8_t cam = 0;
	
	if (likely(!(ar->hw->conf.flags & IEEE80211_CONF_PS)))
		return;
	
	/* check if this really is a beacon */
	if (!ieee80211_is_beacon(hdr->frame_control))
		return;
	
	/* min. beacon length + FCS_LEN */
	if (len <= 40 + FCS_LEN)
		return;
	
	/* and only beacons from the associated BSSID, please */
	if (!ether_addr_equal(hdr->addr3, ar->common.curbssid) || !ar->common.curaid)
		return;
	
	/* if in scan mode, use this beacon to join the IBSS */
	if (unlikely(ar->rx_filter.mode) == AR9170_IBSS_SCAN_MODE) {
		// FIXME TODO ieee80211_process_beacon(ar, data, len);
	}
	
	//ar->ps.last_beacon = jiffies;
	
	//tim = carl9170_find_ie(data, len - FCS_LEN, WLAN_EID_TIM);
	//if (!tim)
		//return;
	
	//if (tim[1] < sizeof(*tim_ie))
		//return;
	
	//tim_len = tim[1];
	//tim_ie = (struct ieee80211_tim_ie *) &tim[2];
	
	//if (!WARN_ON_ONCE(!ar->hw->conf.ps_dtim_period))
		//ar->ps.dtim_counter = (tim_ie->dtim_count - 1) %
			//ar->hw->conf.ps_dtim_period;
	
	/* Check whenever the PHY can be turned off again. */
	
	/* 1. What about buffered unicast traffic for our AID? */
	//cam = ieee80211_check_tim(tim_ie, tim_len, ar->common.curaid);
	
	/* 2. Maybe the AP wants to send multicast/broadcast data? */
	//cam |= !!(tim_ie->bitmap_ctrl & 0x01);
	
	//if (!cam) {
		/* back to low-power land. */
	//	ar->ps.off_override &= ~PS_OFF_BCN;
		//carl9170_ps_check(ar);
	//} else {
		/* force CAM */
		//ar->ps.off_override |= PS_OFF_BCN;
	//}
}


static uint8_t 
ar9170_ampdu_check(struct ar9170 *ar, uint8_t *buf, uint8_t ms,
	struct ieee80211_rx_status *rx_status)
{
	le16_t fc;
	
	if ((ms & AR9170_RX_STATUS_MPDU) == AR9170_RX_STATUS_MPDU_SINGLE) {
		/*
		 * This frame is not part of an aMPDU.
		 * Therefore it is not subjected to any
		 * of the following content restrictions.
		 */
		return 1;
	}
	
	rx_status->flag |= RX_FLAG_AMPDU_DETAILS | RX_FLAG_AMPDU_LAST_KNOWN;
	rx_status->ampdu_reference = ar->ampdu_ref;
	
	/*
	 * "802.11n - 7.4a.3 A-MPDU contents" describes in which contexts
	 * certain frame types can be part of an aMPDU.
	 *
	 * In order to keep the processing cost down, I opted for a
	 * stateless filter solely based on the frame control field.
	 */
	
	fc = ((struct ieee80211_hdr *)buf)->frame_control;
	if (ieee80211_is_data_qos(fc) && ieee80211_is_data_present(fc))
		return 1;
	
	if (ieee80211_is_ack(fc) || ieee80211_is_back(fc) ||
		ieee80211_is_back_req(fc))
		return 1;
	
	if (ieee80211_is_action(fc))
		return 1;
	
	return 0;
}
/**
 * This function is called within USB interrupt context.
 */
static int 
ar9170_handle_mpdu(struct ar9170 *ar, list_t rx_list, uint8_t *buf, int len, 
	struct ieee80211_rx_status *status)
{	
	PRINTF("AR9170_RX: handle-mpdu\n");
	/* (driver) frame trap handler
	 *
	 * Because power-saving mode handing has to be implemented by
	 * the driver/firmware. We have to check each incoming beacon
	 * from the associated AP, if there's new data for us (either
	 * broadcast/multicast or unicast) we have to react quickly.
	 *
	 * So, if you have you want to add additional frame trap
	 * handlers, this would be the perfect place!
	 */	
	if(unlikely(ar->rx_filter.mode == AR9170_DRV_SCANNING_MODE)) {
		ar9170_trap_beacon(ar, buf, len);
		/* We are only looking for beacons now. */
		return 0;
	}
	
	ar9170_pkt_buf_t *pkt = NULL;
	
	if (len > AR9170_DRV_RX_MAX_PKT_LENGTH) {
		PRINTF("AR9170_RX: received-oversized pkt\n");
		return -EMSGSIZE;
	}
	
	if(unlikely(ar->rx_filter.mode == AR9170_DRV_SNIFFING_MODE)) {
		goto copy_pkt;
	}
	
	/* Except for ATIM frames, only from the BSSID */
	struct ieee80211_hdr_3addr  *hdr = (struct ieee80211_hdr_3addr*)buf;
	if(!ieee80211_is_atim(hdr->frame_control))
		//if(!ether_addr_equal(hdr->addr3, ar->common.curbssid))
			//return 0;
copy_pkt:	
	ar9170_ps_atim(ar, buf, len);	
/*
	ar9170_ps_beacon(ar, buf, len);
	
	carl9170_ba_check(ar, buf, len);
*/	
	pkt = ar9170_rx_copy_data(buf, len);

	if (!pkt) {
		printf("AR9170_RX: memory-error\n");
		return -ENOMEM;
	}
		
	/* Check if list can accommodate a new packet */
	if (list_length(rx_list) >= AR9170_DRV_RX_PKT_MAX_QUEUE_LEN) {
		printf("AR9170_RX: rx-linked-list-full\n");
		/* free packet TODO */		
		return -ENOMEM;
	}
	/* Add packet to the list. */
	list_add(rx_list, pkt);
	PRINTF("AR9170_RX: rx-added [%u]\n",pkt->pkt_len);
	//memcpy(IEEE80211_SKB_RXCB(skb), status, sizeof(*status));
	//ieee80211_rx(ar->hw, skb);
	return 0;
}
/*
 * This function is called within USB interrupt context.
 */
static int 
ar9170_check_sequence(struct ar9170 * ar, uint32_t seq) 
{
	if (ar->cmd_seq < -1)
		return 0;
	
	/*
	 * Initialize Counter
	 */
	if (ar->cmd_seq < 0)
		ar->cmd_seq = seq;
	
	/*
	 * The sequence is strictly monotonic increasing and it never skips!
	 *
	 * Therefore we can safely assume that whenever we received an
	 * unexpected sequence we have lost some valuable data.
	 */
	if (seq != ar->cmd_seq) {
		printf("ERROR: Lost command responses/traps! "
			"Received: %d while local sequence counter is: %d.\n", (int)seq, ar->cmd_seq);
		
		//ar9170_restart(ar, AR9170_RR_LOST_RSP); TODO the restarting of the AR9170 devcice
		/* FIXME: for the moment, we consider the loss not so important,
		 * so we update the sequence counter as if there was no problem
		 */
		ar->cmd_seq = (seq + 1) % ar->fw.cmd_bufs;
		return -EIO;
	}
	/* Everything is fine. Update sequence counter. */
	ar->cmd_seq = (ar->cmd_seq + 1) % ar->fw.cmd_bufs;
		
	return 0;
}
/*
 * This function is called within USB interrupt context.
 */
static void 
ar9170_rx_untie_cmds(struct ar9170* ar, const uint8_t* buffer, const uint32_t len) 
{	
	PRINTF("AR9170_RX: untie commands; total len: %u\n", (unsigned int)len);
		
	struct ar9170_rsp *cmd;
	int i = 0;
	
	while (i < len ) {
		/* This is a workaround for checking the format of the 
		 * device command responses. We signal an "error", in
		 * case the response contains more than one commands,
		 * which is actually not an error, strictly speaking. 
		 */
		if (unlikely(i>0)) {
			PRINTF("AR9170_RX: Got response with more than one commands!\n");
		}
		/* Assign next command to the command response pointer */
		cmd = (void*) &buffer[i];
		/* Update next pointer position based on the length of the current command */
		i += cmd->hdr.len + AR9170_CMD_HDR_LEN;
		if (unlikely(i > len)) {
			printf("ERROR: Got response with problems.\n");
			int j;
			for (j=0; j<len; j++)
				printf("%02x ", buffer[j]);
			printf(" \n");
			break;
		}
		if (ar9170_check_sequence(ar, cmd->hdr.seq)) {
			PRINTF("AR9170_RX: untie-commands returns error!\n");
			break;
		}
		/* Otherwise handle the command response inside interrupt context. */
		ar9170_handle_command_response(ar, cmd, cmd->hdr.len + AR9170_CMD_HDR_LEN);		 
	}
	
	if(unlikely(i != len)) {
		printf("ERROR: Got response with problems. Not equal in the end.\n");
		
	}	
}

/**
 * This function is called within USB interrupt context.
 */
static void 
ar9170_rx_untie_data(struct ar9170 *ar, list_t rx_list, uint8_t *buf, int len) 
{
	/* XXX XXX extra filtering */
	if(len > 256)
		goto drop;
		
	PRINTF("AR9170_RX: untie-data [%u]\n",len);
	/* Before processing the received data, we need to check 
	 * whether the response from the AR9170 device contains
	 * 2 added padding bytes. If it does, we need to remove
	 * them, as they may be a source of an error. Usually, 
	 * these bytes are either 00 00 or ff ff; in the later, 
	 * they cause the MPDU to be considered as MIDDLE, and
	 * this cause an error - so the packet is dropped. 
	 *
	 * If the padding is 00 00, the packet is surprisingly
	 * processed correctly. This probably occurs because of
	 * the upper layer header that contains the length, so
	 * Contiki does not process more bytes that it needs.
	 * 
	 * Finally, we must check whether there exist even more
	 * padded bytes. For that we rely on the 3f 3f pattern,
	 * which we believe that exists always and is used as
	 * our reference. Actually, it indicates the decryption
	 * field; so if we do not use encryption, this field is
	 * safe to use.	 Additionally, we could check the last
	 * 13 bytes of the PHY control header; should be all 0.
	 */
	
	/* Start from the back and try to locate the 3f 3f pattern.
	 * Once done check whether there are 2 or 4 bytes after it;
	 * remove the last 2 in the later case.
	 */
	
	unsigned int i, match;
	unsigned int counter = len - sizeof(struct ar9170_rx_head);
	unsigned int forward_counter = 0;
	
	/* The constant patterns. */
	const uint8_t _mac_status_pattern[2] = {0x3f, 0x3f};
	const uint8_t _mac_phy_status_pattern[13] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};				
	/* Flag indicating the search result. */
	uint8_t pattern_found = false;
	
	while (counter > 0) {
		/* Compare the short pattern with the tail of the response. */
		match = memcmp(&_mac_status_pattern[0], 
			&buf[counter+sizeof(struct ar9170_rx_head)-sizeof(struct ar9170_rx_macstatus)],2);
			
		if (match == 0) {
			PRINTF("AR9170_RX; pad-len %u\n",forward_counter);
			/* Short pattern found. Proceed with phy comparison. 
			 * If the phy comparison fails, we signal a warning,
			 * but do not drop the packet.
			 */
			pattern_found = 1;
			unsigned int _index = counter - 13;
			match = memcmp(&_mac_phy_status_pattern[0], &buf[_index+sizeof(struct ar9170_rx_head)-sizeof(struct ar9170_rx_macstatus)], 13);
			if (match != 0) {				
				PRINTF("WARNING: Short pattern matched but long pattern not.\n");
			}
			/* Remove the padded bytes, by adjusting the response length. */
			len -=  forward_counter;
			/* Break the loop*/
			break;			
			
		} else {
			/* Short pattern not found. Shift to the left. */
			counter--;
			forward_counter++;
		}			
	}
	
	/* If the short pattern was not found, we could ignore the packet, right? */
	if (pattern_found == false) {
		if (buf[22] == 0xbc)
			printf("AR9170_RX: mac-status-pattern-not-found\n");
		else { 
			PRINTF("AR9170_RX: mac-status-pattern-not-found\n");	
		}
	}		
		
	struct ar9170_rx_head *head;
	struct ar9170_rx_macstatus *mac;
	struct ar9170_rx_phystatus *phy = NULL;
	struct ieee80211_rx_status status;
	int mpdu_len;
	uint8_t mac_status;
	
	if (!IS_STARTED(ar))
		return;
	
	if (unlikely(len < sizeof(*mac)))
		goto drop;
	
	memset(&status, 0, sizeof(status));
	
	mpdu_len = len - sizeof(*mac);
	
	mac = (void *)(buf + mpdu_len);
	mac_status = mac->status;
	
	switch (mac_status & AR9170_RX_STATUS_MPDU) {
	
		case AR9170_RX_STATUS_MPDU_FIRST:
			goto drop; // XXX
			ar->ampdu_ref++;
		    /* Aggregated MPDUs start with an PLCP header */
			if (likely(mpdu_len >= sizeof(struct ar9170_rx_head))) {
				head = (void *) buf;
			
			    /*
			     * The PLCP header needs to be cached for the
			     * following MIDDLE + LAST A-MPDU packets.
			     *
			     * So, if you are wondering why all frames seem
			     * to share a common RX status information,
			     * then you have the answer right here...
			     */
			     memcpy(&ar->rx_plcp, (void *) buf,
			           sizeof(struct ar9170_rx_head));
			     mpdu_len -= sizeof(struct ar9170_rx_head);
			     buf += sizeof(struct ar9170_rx_head);
			
			     ar->rx_has_plcp = 1;
				 
			} else {
				/*
			     if (net_ratelimit()) {
					wiphy_err(ar->hw->wiphy, "plcp info is clipped.\n");
			     }
				*/
			     goto drop;
			}
		    break;
		
		case AR9170_RX_STATUS_MPDU_LAST:
			goto drop; // XXX
			status.flag |= RX_FLAG_AMPDU_IS_LAST;
		
		    /*
		     * The last frame of an A-MPDU has an extra tail
		     * which does contain the phy status of the whole
		     * aggregate.
		     */
		    if (likely(mpdu_len >= sizeof(struct ar9170_rx_phystatus))) {
				mpdu_len -= sizeof(struct ar9170_rx_phystatus);
			    phy = (void *)(buf + mpdu_len);
			} else {
				/*
			    if (net_ratelimit()) {
					wiphy_err(ar->hw->wiphy, "frame tail is clipped.\n");
			     }
				*/
			     goto drop;
			}
		case AR9170_RX_STATUS_MPDU_MIDDLE:
			goto drop; // XXX
			/*  These are just data + mac status */
		    if (unlikely(!ar->rx_has_plcp)) {
				/*
				if (!net_ratelimit())
					return;
			
			     wiphy_err(ar->hw->wiphy, "rx stream does not start "
			          "with a first_mpdu frame tag.\n");
				*/
			     goto drop;
			}
		
		    head = &ar->rx_plcp;
			break;
		
		case AR9170_RX_STATUS_MPDU_SINGLE:
			/* single mpdu has both: plcp (head) and phy status (tail) */
			head = (void *) buf;
			mpdu_len -= sizeof(struct ar9170_rx_head);
			mpdu_len -= sizeof(struct ar9170_rx_phystatus);
			
			buf += sizeof(struct ar9170_rx_head);
			phy = (void *)(buf + mpdu_len);
			break;
		
		default:
			printf("AR9170_RX: bad RX status\n");
			break;
	}
		
	/* FC + DU + RA + FCS */
	if (unlikely(mpdu_len < (2 + 2 + ETH_ALEN + FCS_LEN)))
		goto drop;
	
	if (unlikely(ar9170_rx_mac_status(ar, head, mac, &status)))
		goto drop;
	
	if (!ar9170_ampdu_check(ar, buf, mac_status, &status))
		goto drop;
	
	if (phy)
		ar9170_rx_phy_status(ar, phy, &status);
	else
		status.flag |= RX_FLAG_NO_SIGNAL_VAL;

	if (ar9170_handle_mpdu(ar, rx_list, buf, mpdu_len, &status))
		goto drop;
	
	return;
drop:
	PRINTF("AR9170_RX: drop\n");
	ar->rx_dropped++;		
}

/**
 * This function is called within USB interrupt context.
 */
void 
ar9170_rx(struct ar9170 *ar, list_t rx_list, uint8_t *buffer, uint32_t len)
{
	/*
	 * We would have liked this function to be as simple as the one
	 * implemented on the Linux driver. However, for some reasons the
	 * device is grouping the responses and we must separate them, by
	 * writing a response wrapper here.

	 * Luckily the device is not mixing the responses; it just merges them
	 * one after the other, which makes our life easier. Or, at least, this
	 * is what we think.

	 * This parser only unties the command response from the bulky data, and
	 * leaves the untie of the bulky data to possibly more than one MPDUs for
	 * a later stage.

	 * So, finally, the way we solve our problem is to go through the response
	 * looking for the pattern: LL 00 00 4e ffff ffff ffff ffff ffff ffff, as
	 * this indicates the beginning of the command response. The response 
	 * follows later.
	 */
		
	/* Initially the remaining length is the received length minus the very first byte,
	 * which is the pipe received length and we do not need it.
	 */
	int32_t _remaining_len = len - 4;
	/* Command length is initially zero */
	
	/* Skip the first byte that contains the [virtual] response length */
	uint8_t* _command_buffer = buffer + 4;
	
	/* Initialize the match flag to -1. */
	unsigned int match = -1;	
	
	/* The command has a minimum size of 15 [prefix] and 4 [CMD header - no payload] */
	while(_remaining_len >= AR9170_MIN_HDR_LEN) {		
	
		/* Check for the 0xff pattern. */
		if (*(_command_buffer) != 0xff) {
			
			/* If it was not found, do not bother to check until after 12 bytes. */
			_remaining_len -= AR9170_MAGIC_RSP_0XFF_HDR_LEN;
			_command_buffer += AR9170_MAGIC_RSP_0XFF_HDR_LEN;
			continue;			
		
		} else {
			
			/* We have found a 0xff which is a candidate to be inside the pattern. So,
			 * we must check in the range of this buffer position for pattern matching
			 */
			uint8_t index_counter = 0;
			uint8_t pattern_found = 0;
			
			while (index_counter < AR9170_MAGIC_RSP_0XFF_HDR_LEN) {
				
				/* Compare with the sequence before and after. */
				match = memcmp(&magic_command_header, _command_buffer-AR9170_MAGIC_RSP_NON_FF_LEN-index_counter, AR9170_MAGIC_RSP_HDR_LENGTH);
				
				if (match != 0) {
					/* The pattern has not been found. Increment reverse index counter. */
					index_counter++;
					
				} else {
					/* The pattern was found! */
					pattern_found = 1;
					break;
				}				
			}
			if (pattern_found == 0) {
				
				/* If it was not found, do not bother to check until after 12 bytes. */
				_remaining_len -= AR9170_MAGIC_RSP_0XFF_HDR_LEN;
				_command_buffer += AR9170_MAGIC_RSP_0XFF_HDR_LEN;
				continue;				
			
			} else {

				/* It seems that we have found the command starting pattern, so
				 * whatever is BEFORE this point, is a data packet and shall be 
				 * scheduled for MPDU processing, AFTER the command is handled.
				 */
				_command_buffer = _command_buffer-AR9170_MAGIC_RSP_NON_FF_LEN-index_counter;
				_remaining_len = _remaining_len + AR9170_MAGIC_RSP_NON_FF_LEN + index_counter;
				
				/*
				 * Whatever FOLLOWS the pattern is the actual response that starts
				 * with the response length _excluding_ the header! However, there
				 * may be more data after the command(s) response which shall not
				 * be send to the command response routine. For this reason, we
				 * in fact, do some hacking here: We check already the length of 
				 * the command, considering that the device does NOT buffer 
				 * command responses. [WE HAVE DONE SOME CHECKING ABOUT THIS :)]
				 */			
				struct ar9170_rsp *cmd = (struct ar9170_rsp*)(_command_buffer+AR9170_MAGIC_RSP_HDR_LENGTH); 
				uint32_t cmd_len = cmd->hdr.len + AR9170_CMD_HDR_LEN;
			
				if (cmd_len + AR9170_MAGIC_RSP_HDR_LENGTH > _remaining_len) {
					printf("ERROR: Command declares a size that brings us out of bounds.\n");
				}
			
				/* Handle the command inside the interrupt context. This needs to be fast. */
				ar9170_rx_untie_cmds(ar, _command_buffer+AR9170_MAGIC_RSP_HDR_LENGTH, cmd_len);
			
						
				if (_command_buffer != (buffer + 1)) {
					/* There is a bulk response before the command response, so we
					 * schedule the packet processing, so it is executed after we 
					 * exit the interrupt context.
					 */
					if (len - _remaining_len - 1 <= 4) {
						printf("ERROR: Received insufficient bulky data before command response: %u .\n",len-_remaining_len-1);
				
					} else {						
						PRINTF("DEBUG: Response is mixed.\n");						
						ar9170_rx_untie_data(ar, rx_list, buffer+4, len-_remaining_len - 4 - 1);
					} 
				}
			
				if (unlikely(cmd_len != (_remaining_len-AR9170_MAGIC_RSP_HDR_LENGTH))) {
					/* The remaining length of the buffer exceeds the length of the 
					 * command, considering, of course, that there is only one. So
					 * either the device has actually grouped multiple commands in
					 * a single USB response, or there is a packet coming after the
					 * command. The second is not a problem, while the first is, as
					 * we do not handle it. We will consider here that grouping the
					 * commands never occurs, and handle the remaining bytes as an 
					 * MPDU.
					 */
					if (cmd_len < (_remaining_len-AR9170_MAGIC_RSP_HDR_LENGTH)) {
						#if DEBUG
						printf("WARNING: There is something after the command response!"
							" Trash, packet or grouped responses? %d, %d.\n",
							cmd_len, _remaining_len-AR9170_MAGIC_RSP_HDR_LENGTH);
							int k;
							uint8_t* _extra_bytes = _command_buffer+AR9170_MAGIC_RSP_HDR_LENGTH+cmd_len;
							for (k=0; k<(_remaining_len-AR9170_MAGIC_RSP_HDR_LENGTH-cmd_len); k++)
								printf("%02x ",_extra_bytes[k]);
							printf(" \n");
						#endif
						/* What if this is an entirely new command response? We need 
						 * to check against the pattern; if not, this is handled as a
						 * packet, otherwise we execute this command within interrupt
						 * context. We do not expect to have a third command response
						 * in the same USB response, so we just do a simple check. We
						 * should normally handle the issue in an exhaustive while loop
						 * until we run out of remaining bytes. TODO
						 */
						uint8_t* new_command = _command_buffer + AR9170_MAGIC_RSP_HDR_LENGTH + cmd_len + 1;
						if (memcmp(&magic_command_header, new_command,
							AR9170_MAGIC_RSP_HDR_LENGTH) == 0) {
								/* This is a second command response attached to the same USB response. */
								struct ar9170_rsp* new_cmd = (struct ar9170_rsp*)(new_command+AR9170_MAGIC_RSP_HDR_LENGTH);
							
								uint32_t new_cmd_len = new_cmd->hdr.len + AR9170_CMD_HDR_LEN;

								/* Check if we go out of bounds. */
								if ((_remaining_len - new_cmd_len - cmd_len - 1 - AR9170_MAGIC_RSP_HDR_LENGTH) <= 4 ) {
									printf("ERROR: Attached command has brought us out of bounds.\n");
								}

								#if DEBUG
								PRINTF("ATTACHED CMD\n");
								#endif

								/* Handle the command inside the interrupt context. This needs to be fast. */
								ar9170_rx_untie_cmds(ar,(uint8_t*)new_cmd, new_cmd_len);
					
						} else {
							/* This is a packet. */
							PRINTF("ATTACHED PKT\n");							
							ar9170_rx_untie_data(ar, rx_list, _command_buffer+AR9170_MAGIC_RSP_HDR_LENGTH+cmd_len,
								(_remaining_len-AR9170_MAGIC_RSP_HDR_LENGTH-cmd_len));
						}
					
				
					} else {
						/* The command length is larger than the actual received buffer length. 
						 * This is a serious error and will be detected later by the handling 
						 * of the command response.
						 */
					}				
				}
				/* After the command pattern is found, we do not need to search for more. */			
				break;
			}
			
		}
	
	}
	/* We have now exited the while loop. If the pattern was not found, the received 
	 * USB response is data and we handle the whole response as an MPDU. In the very
	 * unlikely scenario that a single USB response corresponds to multiple packets,
	 * we have a problem, because such case is not handled.
	 */
	if (match != 0) {
		PRINTF("AR9170_RX: only-pkt [%u]\n",len);
		/* Skip the first four bytes [USB delimiter and extra bytes]. */
		ar9170_rx_untie_data(ar, rx_list, buffer+4, len-4);
	}	
}	


/**
 * This function is called within USB interrupt context.
 */
static void 
ar9170_cmd_callback(struct ar9170 *ar, uint32_t len, void *buffer)
{
	/*
	 * Some commands may have a variable response length
	 * and we cannot predict the correct length in advance.
	 * So we only check if we provided enough space for the data.
	 */
	if (unlikely(ar->readlen != (len - 4))) {
		PRINTF("received invalid command response: got %d, instead of %d\n", 
			len - 4, ar->readlen);
		/*
		 * Do not complete. The command times out,
		 * and we get a stack trace from there.
		 */
		//ar9170_restart(ar, AR9170_RR_INVALID_RSP);
	}
	
	//spin_lock(&ar->cmd_lock);
	if (ar->readbuf) {
		if (len >= 4)
			memcpy(ar->readbuf, buffer + 4, len - 4);
		
		ar->readbuf = NULL;
	}
	AR9170_RELEASE_LOCK(&ar->cmd_wait);
	//spin_unlock(&ar->cmd_lock);
}


/**
 * This function is called within USB interrupt context.
 */
void 
ar9170_handle_command_response(struct ar9170 *ar, void *buf, uint32_t len) 
{	
	struct ar9170_rsp *cmd = buf;
	struct ieee80211_vif *vif;
	
	if ((cmd->hdr.cmd & AR9170_RSP_FLAG) != AR9170_RSP_FLAG) {
		if (!(cmd->hdr.cmd & AR9170_CMD_ASYNC_FLAG))
			ar9170_cmd_callback(ar, len, buf);
		
		return;
	}
	
	if (unlikely(cmd->hdr.len != (len - 4))) {
		
		PRINTF("FW: received over-/undersized event %x (%d, but should be %d).\n",
		        cmd->hdr.cmd, cmd->hdr.len, len - 4);
			
		//print_hex_dump_bytes("dump:", DUMP_PREFIX_NONE, buf, len);
	}
	
	/* hardware event handlers */
	switch (cmd->hdr.cmd) {	
	case AR9170_RSP_PRETBTT:
		/* pre-TBTT event */
		if (unlikely((*((uint32_t*)cmd->data)) != 0))
			printf("AR9170_RX: TBTT in PSM\n");
		ar->ps.state = 0;
		ar->ps.off_override = 0;
		ar9170_psm_handle_tbtt(ar);	
		break;
	case AR9170_RSP_TXCOMP:
	/* TX status notification */	
		ar9170_tx_process_status(ar, cmd);
		break;
	case AR9170_RSP_BEACON_CONFIG:
		/*
		 * (IBSS) beacon send notification
		 * bytes: 04 c2 XX YY B4 B3 B2 B1
		 *
		 * XX always 80
		 * YY always 00
		 * B1-B4 "should" be the number of send out beacons.
		 */
		ar9170_psm_handle_bcn_config(ar,(*((uint32_t*)cmd->data)));
		break;
	case AR9170_RSP_ATIM:
		printf("a\n");
	/* End of Atim Window */
		break;	
	case AR9170_RSP_WATCHDOG:
	/* Watchdog Interrupt */
		break;
	case AR9170_RSP_TEXT:
	/* firmware debug */	
		break;
	case AR9170_RSP_HEXDUMP:
		break;
	case AR9170_RSP_RADAR:
		break;
	case AR9170_RSP_GPIO:
		break;	
	case AR9170_RSP_BOOT:
		PRINTF("AR9170_RX: BOOT\n");
		AR9170_RELEASE_LOCK(&ar->fw_boot_wait);
		break;	
	default:
		printf("AR9170-rx: received unhandled event\n %02x\n",cmd->hdr.cmd);			
	}
		
}
#endif /* WITH_WIFI_SUPPORT */


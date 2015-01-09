#include "ar9170-drv.h"
#include "ar9170.h"
#include "ar9170_cmd.h"
#include "compiler.h"
#include "mac80211.h"
#include "ieee80211_ibss.h"
#include <sys\errno.h>
#include "ar9170_wlan.h"
#include "ieee80211.h"
#include "rimeaddr.h"
#include "ar9170_fwcmd.h"
#include "packetbuf.h"

/*
 * ar9170_tx.c
 *
 * Created: 2/9/2014 7:23:36 PM
 *  Author: Ioannis Glaropoulos
 */ 

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#include "platform-conf.h"
#if WITH_WIFI_SUPPORT

#define roundup(x, y) (                                 \
{                                                       \
	         const typeof(y) __y = y;                        \
	         (((x) + (__y - 1)) / __y) * __y;                \
}                                                       \
)
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))


struct ar9170_tx_info {
	unsigned long timeout;
	struct ar9170 *ar;
	//struct kref ref;
};

static void
ar9170_tx_fill_rateinfo(struct ar9170 *ar, unsigned int rix,
unsigned int tries, struct ieee80211_tx_info *txinfo)
{
	unsigned int i;
	
	for (i = 0; i < IEEE80211_TX_MAX_RATES; i++) {
		if (txinfo->status.rates[i].idx < 0)
		break;
		
		if (i == rix) {
			txinfo->status.rates[i].count = tries;
			i++;
			break;
		}
	}
	
	for (; i < IEEE80211_TX_MAX_RATES; i++) {
		txinfo->status.rates[i].idx = -1;
		txinfo->status.rates[i].count = 0;
	}
}


void 
__ar9170_tx_status(struct ar9170 *ar, ar9170_pkt_buf_t *skb,
    const bool success)
{
	struct ieee80211_tx_info *txinfo;
#if 0	
	carl9170_tx_accounting_free(ar, skb);

	txinfo = IEEE80211_SKB_CB(skb);
	
	carl9170_tx_bar_status(ar, skb, txinfo);
#endif	

	txinfo = (struct ieee80211_tx_info*)(((uint8_t*)skb)+skb->pkt_len);//IEEE80211_SKB_CB(skb);
	
	if (success)
	    txinfo->flags |= IEEE80211_TX_STAT_ACK;
	else
	    ar->tx_ack_failures++;
#if 0	
	if (txinfo->flags & IEEE80211_TX_CTL_AMPDU)
	    carl9170_tx_status_process_ampdu(ar, skb, txinfo);
	
	carl9170_tx_ps_unblock(ar, skb);
	carl9170_tx_put_skb(skb);
#endif	
}

static ar9170_pkt_buf_t*
ar9170_get_queued_skb(struct ar9170 *ar, uint8_t cookie)
{	
	ar9170_pkt_buf_t *pkt = list_head(ar->ar9170_tx_pending_list); 
	
	while(pkt != NULL && pkt->pkt_state == AR9170_DRV_PKT_SENT) {
		
		struct _ar9170_tx_superframe *txc = (void *) pkt->pkt_data;
		
		if (txc->s.cookie != cookie) {
			
			pkt = list_item_next(pkt);
			continue;
		}
		return pkt;			
	}
	printf("AR9170_TX: pending-cookie (%u) not found\n",cookie);
	return NULL;
}


static void 
__ar9170_tx_process_status(struct ar9170 *ar,
         const uint8_t cookie, const uint8_t info)
{	
	ar9170_pkt_buf_t *skb;
	struct ieee80211_tx_info *txinfo;
	unsigned int r, t, q;
	bool success = true;
	
	//q = ar9170_qmap[info & AR9170_TX_STATUS_QUEUE];
	
	skb = ar9170_get_queued_skb(ar, cookie);//, &ar->tx_status[q]);
	
	if (!skb) {
		/*
		 * We have lost the race to another thread.
		 */
		PRINTF("AR9170_TX: pending-frame-(%u)-not-found\n", cookie);
		return ;
	}
	/* Get the tx info from the buffered packet */
	txinfo = (struct ieee80211_tx_info*)(((uint8_t*)skb)+skb->pkt_len);//IEEE80211_SKB_CB(skb);
	
	if (!(info & AR9170_TX_STATUS_SUCCESS))
		success = 0;
		
	/* Register the incoming retransmissions info */
	r = (info & AR9170_TX_STATUS_RIX) >> AR9170_TX_STATUS_RIX_S;
	t = (info & AR9170_TX_STATUS_TRIES) >> AR9170_TX_STATUS_TRIES_S;
	
	if (!success)
		printf("AR9170_TX: status %u %u, %u\n", success, r, t);
	
	ar9170_tx_fill_rateinfo(ar, r, t, txinfo);
	__ar9170_tx_status(ar, skb, success);
	
	/* Inform driver, which should also delete the pending status entry. */
	ar9170_drv_handle_status_response(skb, success, r, t, cookie);
}

void 
ar9170_tx_process_status(struct ar9170 *ar,  const struct ar9170_rsp *cmd)
{
	unsigned int i;
	
	for (i = 0;  i < cmd->hdr.ext; i++) {
		if (i > ((cmd->hdr.len / 2) + 1)) {			
			PRINTF("AR9170_TX: inv-status-rsp\n");
			int j;
			uint8_t *cmd_data = cmd;
			for (j=0; i<cmd->hdr.len+4; j++)
				printf("%02x ",cmd_data[i]);
			printf("\n");	
			break;
		}
		
		__ar9170_tx_process_status(ar, cmd->_tx_status[i].cookie, 
			cmd->_tx_status[i].info);
	}
}

static void
ar9170_tx_rate_tpc_chains(struct ar9170 *ar,
struct ieee80211_tx_info *info, struct ieee80211_tx_rate *txrate,
unsigned int *phyrate, unsigned int *tpc, unsigned int *chains)
{
	struct ieee80211_rate *rate = NULL;
	uint8_t *txpower;
	unsigned int idx;
	
	idx = txrate->idx;
	*tpc = 0;
	*phyrate = 0;
	
	if (txrate->flags & IEEE80211_TX_RC_MCS) {
		if (txrate->flags & IEEE80211_TX_RC_40_MHZ_WIDTH) {
			/* +1 dBm for HT40 */
			*tpc += 2;
			
			if (info->band == IEEE80211_BAND_2GHZ)
			txpower = ar->power_2G_ht40;
			else
			txpower = ar->power_5G_ht40;
			} else {
			if (info->band == IEEE80211_BAND_2GHZ)
			txpower = ar->power_2G_ht20;
			else
			txpower = ar->power_5G_ht20;
		}
		
		*phyrate = txrate->idx;
		*tpc += txpower[idx & 7];
		} else {
		if (info->band == IEEE80211_BAND_2GHZ) {
			if (idx < 4)
			txpower = ar->power_2G_cck;
			else
			txpower = ar->power_2G_ofdm;
			} else {
			txpower = ar->power_5G_leg;
			idx += 4;
		}
		
		rate = &__carl9170_ratetable[idx];
		*tpc += txpower[(rate->hw_value & 0x30) >> 4];
		*phyrate = rate->hw_value & 0xf;
	}
	
	if (ar->eeprom.tx_mask == 1) {
		*chains = AR9170_TX_PHY_TXCHAIN_1;
		} else {
		if (!(txrate->flags & IEEE80211_TX_RC_MCS) &&
		rate && rate->bitrate >= 360)
		*chains = AR9170_TX_PHY_TXCHAIN_1;
		else
		*chains = AR9170_TX_PHY_TXCHAIN_2;
	}
	
	*tpc = min(*tpc, ar->hw->conf.power_level * 2);
}


static uint8_t 
ar9170_tx_rts_check(struct ar9170 *ar, struct ieee80211_tx_rate *rate,
	bool ampdu, bool multi)
{
	switch (ar->erp_mode) {
		case AR9170_ERP_AUTO:
			if (ampdu)
		         break;
		
		case AR9170_ERP_MAC80211:
		    if (!(rate->flags & IEEE80211_TX_RC_USE_RTS_CTS))
		         break;
		
		case AR9170_ERP_RTS:
		     if (likely(!multi))
		         return 1;
		
		default:
		     break;
	}
	
	return 0;
}

static uint8_t 
ar9170_tx_cts_check(struct ar9170 *ar, struct ieee80211_tx_rate *rate)
{
	switch (ar->erp_mode) {
		case AR9170_ERP_AUTO:
		case AR9170_ERP_MAC80211:
			if (!(rate->flags & IEEE80211_TX_RC_USE_CTS_PROTECT))
				break;
		
		case AR9170_ERP_CTS:
			return 1;
		
		default:
		     break;
	}
	
	return 0;
}

static le32_t 
ar9170_tx_physet(struct ar9170 *ar,
	struct ieee80211_tx_info *info, struct ieee80211_tx_rate *txrate)
{
	unsigned int power = 0, chains = 0, phyrate = 0;
	le32_t tmp;
	
	tmp = cpu_to_le32(0);
	
	if (txrate->flags & IEEE80211_TX_RC_40_MHZ_WIDTH)
		tmp |= cpu_to_le32(AR9170_TX_PHY_BW_40MHZ << AR9170_TX_PHY_BW_S);
	         /* this works because 40 MHz is 2 and dup is 3 */
	if (txrate->flags & IEEE80211_TX_RC_DUP_DATA)
		tmp |= cpu_to_le32(AR9170_TX_PHY_BW_40MHZ_DUP << AR9170_TX_PHY_BW_S);
	
	if (txrate->flags & IEEE80211_TX_RC_SHORT_GI)
		tmp |= cpu_to_le32(AR9170_TX_PHY_SHORT_GI);
	
	if (txrate->flags & IEEE80211_TX_RC_MCS) {
		SET_VAL(AR9170_TX_PHY_MCS, phyrate, txrate->idx);
		
		/* heavy clip control */
		tmp |= cpu_to_le32((txrate->idx & 0x7) <<
		     AR9170_TX_PHY_TX_HEAVY_CLIP_S);
		
		tmp |= cpu_to_le32(AR9170_TX_PHY_MOD_HT);
		
		/*
		 * green field preamble does not work.
		 *
		 * if (txrate->flags & IEEE80211_TX_RC_GREEN_FIELD)
		 * tmp |= cpu_to_le32(AR9170_TX_PHY_GREENFIELD);
		 */
	} else {
		if (info->band == IEEE80211_BAND_2GHZ) {
			if (txrate->idx <= AR9170_TX_PHY_RATE_CCK_11M)
				tmp |= cpu_to_le32(AR9170_TX_PHY_MOD_CCK);
			else
				tmp |= cpu_to_le32(AR9170_TX_PHY_MOD_OFDM);
		} else {
			tmp |= cpu_to_le32(AR9170_TX_PHY_MOD_OFDM);
		}
		
		/*
		 * short preamble seems to be broken too.
		 *
		 * if (txrate->flags & IEEE80211_TX_RC_USE_SHORT_PREAMBLE)
		 *      tmp |= cpu_to_le32(AR9170_TX_PHY_SHORT_PREAMBLE);
		 */
	}
	ar9170_tx_rate_tpc_chains(ar, info, txrate, &phyrate, &power, &chains);
	
	PRINTF("AR9170_TX: tmp:%08x phyrate:%04x power:%04x chains:%04x\n",
		tmp, phyrate, power, chains);
	
	tmp |= cpu_to_le32(SET_CONSTVAL(AR9170_TX_PHY_MCS, phyrate));
	tmp |= cpu_to_le32(SET_CONSTVAL(AR9170_TX_PHY_TX_PWR, power));
	tmp |= cpu_to_le32(SET_CONSTVAL(AR9170_TX_PHY_TXCHAIN, chains));
	return tmp;
}

static void 
ar9170_tx_apply_rateset(struct ar9170 *ar, struct ieee80211_tx_info *sinfo,
		struct ieee80211_pkt_buf *skb)
{
	struct ieee80211_tx_rate *txrate;
	struct ieee80211_tx_info *info;
	struct _ar9170_tx_superframe *txc = packetbuf_hdrptr();//(void *) skb->data;
	int i;
	bool ampdu;
	bool no_ack;
	
	info = sinfo;//IEEE80211_SKB_CB(skb);
	
	ampdu = !!(info->flags & IEEE80211_TX_CTL_AMPDU);
	no_ack = !!(info->flags & IEEE80211_TX_CTL_NO_ACK);
	
	/* Set the rate control probe flag for all (sub-) frames.
	 * This is because the TX_STATS_AMPDU flag is only set on
	 * the last frame, so it has to be inherited.
	 */
	info->flags |= (sinfo->flags & IEEE80211_TX_CTL_RATE_CTRL_PROBE);
	
	/* NOTE: For the first rate, the ERP & AMPDU flags are directly
	 * taken from mac_control. For all fallback rate, the firmware
	 * updates the mac_control flags from the rate info field.
	 */
	for (i = 0; i < AR9170_TX_MAX_RATES; i++) {
		le32_t phy_set;
		
		txrate = &sinfo->control.rates[i];
		if (txrate->idx < 0)
			break;
		
		phy_set = ar9170_tx_physet(ar, info, txrate);
		if (i == 0) {
			le16_t mac_tmp = cpu_to_le16(0);
			
			/* first rate - part of the hw's frame header */
			txc->f.phy_control = phy_set;
			
			if (ampdu && txrate->flags & IEEE80211_TX_RC_MCS)
			     mac_tmp |= cpu_to_le16(AR9170_TX_MAC_AGGR);
			
			if (ar9170_tx_rts_check(ar, txrate, ampdu, no_ack))
			     mac_tmp |= cpu_to_le16(AR9170_TX_MAC_PROT_RTS);
			else if (ar9170_tx_cts_check(ar, txrate))
			     mac_tmp |= cpu_to_le16(AR9170_TX_MAC_PROT_CTS);
			
			txc->f.mac_control |= mac_tmp;
		} else {
			/* fallback rates are stored in the firmware's
			 * retry rate set array.
			 */
			 txc->s.rr[i - 1] = phy_set;
		}
		
		SET_VAL(AR9170_TX_SUPER_RI_TRIES, txc->s.ri[i], txrate->count);
		
		if (ar9170_tx_rts_check(ar, txrate, ampdu, no_ack))
			txc->s.ri[i] |= (AR9170_TX_MAC_PROT_RTS << AR9170_TX_SUPER_RI_ERP_PROT_S);
		else if (ar9170_tx_cts_check(ar, txrate))
		    txc->s.ri[i] |= (AR9170_TX_MAC_PROT_CTS << AR9170_TX_SUPER_RI_ERP_PROT_S);
		
		if (ampdu && (txrate->flags & IEEE80211_TX_RC_MCS))
		    txc->s.ri[i] |= AR9170_TX_SUPER_RI_AMPDU;
	}
}

static int 
ar9170_tx_prepare(struct ar9170 *ar, struct ieee80211_sta *sta,
		struct ieee80211_pkt_buf *skb)
{
	struct ieee80211_hdr *hdr;
	struct _ar9170_tx_superframe *txc;
	struct ar9170_vif_info *cvif;
	struct ieee80211_tx_info *info;
	struct ar9170_tx_info *arinfo;
	unsigned int hw_queue;
	le16_t mac_tmp;
	uint16_t len;
/*	
	-- We do not currently check for all these possible bugs 
	
	BUILD_BUG_ON(sizeof(*arinfo) > sizeof(info->rate_driver_data));
	BUILD_BUG_ON(sizeof(struct _carl9170_tx_superdesc) !=
	                      AR9170_TX_SUPERDESC_LEN);
	
	BUILD_BUG_ON(sizeof(struct _ar9170_tx_hwdesc) !=
	                     AR9170_TX_HWDESC_LEN);
	
	BUILD_BUG_ON(AR9170_MAX_VIRTUAL_MAC >
	                 ((CARL9170_TX_SUPER_MISC_VIF_ID >>
	                 CARL9170_TX_SUPER_MISC_VIF_ID_S) + 1));
*/	
	hdr = (void *)packetbuf_hdrptr();//skb->data;
	/* Must analyze the tx_info at the point of PHY preparation TODO */
	
	if (packetbuf_attr(PACKETBUF_ATTR_MAC_ACK)) {
		/* is broadcast */
		info = &ieee80211_ibss_bcast_tx_info;//IEEE80211_SKB_CB(skb);
	}
	else {
		info = &ieee80211_ibss_data_tx_info;//IEEE80211_SKB_CB(skb);
	}
	
	len = packetbuf_totlen();//skb->len;
	
	/* Must check which queue is typically returned */
	hw_queue = info->hw_queue;//ar9170_qmap[carl9170_get_queue(ar, skb)];
	
	/*
	 * Note: If the frame was sent through a monitor interface,
	 * the ieee80211_vif pointer can be NULL.
	 */
	if (likely(info->control.vif))
		cvif = (void *) info->control.vif->drv_priv;
	else
		cvif = NULL;
	
	/* We just set the CVIF based on the ar container 
	 * There is only one, in any case
	 */
	cvif = ar->vif_info;
	
	/* Allocate heap space for the total frame size. Since 
	 * there is enough space in the packet buffer we could
	 * just do it there.
	 */
	packetbuf_hdralloc(sizeof(*txc));
	txc = (void *)(packetbuf_hdrptr());
			
	//txc = (void *)skb_push(skb, sizeof(*txc));
	memset(txc, 0, sizeof(*txc));
	
	SET_VAL(AR9170_TX_SUPER_MISC_QUEUE, txc->s.misc, hw_queue);
	
	if (likely(cvif))
	    SET_VAL(AR9170_TX_SUPER_MISC_VIF_ID, txc->s.misc, cvif->id);
	
	if (unlikely(info->flags & IEEE80211_TX_CTL_SEND_AFTER_DTIM))
	    txc->s.misc |= AR9170_TX_SUPER_MISC_CAB;
	
	if (unlikely(info->flags & IEEE80211_TX_CTL_ASSIGN_SEQ))
	    txc->s.misc |= AR9170_TX_SUPER_MISC_ASSIGN_SEQ;
	
	if (unlikely(ieee80211_is_probe_resp(hdr->frame_control)))
	    txc->s.misc |= AR9170_TX_SUPER_MISC_FILL_IN_TSF;
	
	mac_tmp = cpu_to_le16(AR9170_TX_MAC_HW_DURATION |
	    AR9170_TX_MAC_BACKOFF);
	
	mac_tmp |= cpu_to_le16((hw_queue << AR9170_TX_MAC_QOS_S) &
	    AR9170_TX_MAC_QOS);
	
	/* If the packet is broadcast, the TX info should be updated
	 * so the no-ACK flag is set.
	 */
	if (unlikely(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
		PRINTF("AR9170_TX: bcast (%u)\n",packetbuf_datalen());
	    mac_tmp |= cpu_to_le16(AR9170_TX_MAC_NO_ACK);
	} else {
		PRINTF("AR9170_TX: ucast (%u)\n",packetbuf_datalen());
	}
	PRINTF("mac_ctrl:%04x\n",mac_tmp);
/*	
	if (info->control.hw_key) {
		len += info->control.hw_key->icv_len;
		
		switch (info->control.hw_key->cipher) {
			case WLAN_CIPHER_SUITE_WEP40:
			case WLAN_CIPHER_SUITE_WEP104:
			case WLAN_CIPHER_SUITE_TKIP:
			mac_tmp |= cpu_to_le16(AR9170_TX_MAC_ENCR_RC4);
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			mac_tmp |= cpu_to_le16(AR9170_TX_MAC_ENCR_AES);
			break;
		default:
			PRINTF("WARN\n");
			goto err_out;
		}
	}
*/	

	if (info->flags & IEEE80211_TX_CTL_AMPDU) {
		unsigned int density, factor;
		
		if (unlikely(!sta || !cvif))
		    goto err_out;
		
		factor = min(1u, sta->ht_cap.ampdu_factor);
		density = sta->ht_cap.ampdu_density;
		
		if (density) {
			/*
			 * Watch out!
			 *
			 * Otus uses slightly different density values than
			 * those from the 802.11n spec.
			 */
			
			density = max(density + 1, 7u);
		}
		
		SET_VAL(AR9170_TX_SUPER_AMPDU_DENSITY,
		    txc->s.ampdu_settings, density);
		
		SET_VAL(AR9170_TX_SUPER_AMPDU_FACTOR,
		    txc->s.ampdu_settings, factor);
	}
	
	txc->s.len = cpu_to_le16(packetbuf_totlen());
	txc->f.length = cpu_to_le16(len + FCS_LEN);
	txc->f.mac_control = mac_tmp;
	uint8_t *temp = &txc->f.mac_control;
	
	/* Enter the cookie information for status response */
	txc->s.cookie = (uint8_t) packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);
	
	//arinfo = (void *)info->rate_driver_data;
	//arinfo->timeout = jiffies;
	//arinfo->ar = ar;
	//kref_init(&arinfo->ref);
	return 0;
	
err_out:
	//skb_pull(skb, sizeof(*txc));
	return -EINVAL;
}

int 
ar9170_op_tx(struct ieee80211_hw *hw, struct ieee80211_tx_control *control,
     struct ieee80211_pkt_buf *skb)
{
	struct ar9170 *ar = hw->priv;
	struct ieee80211_tx_info *info;
	struct ieee80211_sta *sta = &ibss_station;//control->sta;
	struct ieee80211_vif *vif;
	bool run;
	
	if (unlikely(!IS_STARTED(ar)))
	     goto err_free;
	
	vif = ar->vif; //info->control.vif;
	
	if (packetbuf_attr(PACKETBUF_ATTR_MAC_ACK)) {
		/* is broadcast */
		info = &ieee80211_ibss_bcast_tx_info;//IEEE80211_SKB_CB(skb);
	}
	else {
		info = &ieee80211_ibss_data_tx_info;//IEEE80211_SKB_CB(skb);
	}
	
	if (unlikely(ar9170_tx_prepare(ar, sta, skb)))
	    goto err_free;
		
#if 0 // From now on everything is queue management, not frame preparation	
	carl9170_tx_accounting(ar, skb);
	/*
	 * from now on, one has to use carl9170_tx_status to free
	 * all resources which are associated with the frame.
	 */
	
	if (sta) {
		struct carl9170_sta_info *stai = (void *) sta->drv_priv;
			atomic_inc(&stai->pending_frames);
	}
#endif
#if 0 // We do not expect AMPDUs	
	if (info->flags & IEEE80211_TX_CTL_AMPDU) {
		/* to static code analyzers and reviewers:
		 * mac80211 guarantees that a valid "sta"
		 * reference is present, if a frame is to
		 * be part of an ampdu. Hence any extra
		 * sta == NULL checks are redundant in this
		 * special case.
		 */
		1506                 run = carl9170_tx_ampdu_queue(ar, sta, skb, info);
		1507                 if (run)
		1508                         carl9170_tx_ampdu(ar);
		1509
	} else {
		unsigned int queue = skb_get_queue_mapping(skb);
		
		carl9170_tx_get_rates(ar, vif, sta, skb);
#endif		
		/* At this point we must check the rate info, 
		 * as the above call may have already changed
		 * it
		 */
		ar9170_tx_apply_rateset(ar, info, skb);
#if 0		
		skb_queue_tail(&ar->tx_pending[queue], skb);
	}
#endif
	/* We should find a way to inform the driver that the 
	 * frame is ready to be transmitted
	 */	
	//carl9170_tx(ar);
	return 0;
	
err_free:
	ar->tx_dropped++;
	//ieee80211_free_txskb(ar->hw, skb);
	return -1;
}

static struct ar9170_vif_info* 
ar9170_pick_beaconing_vif( struct ar9170 * ar ) 
{
	if (ar)
		return ar->vif_info;
	return NULL;
}

static uint8_t 
ar9170_tx_beacon_physet(struct ar9170 *ar, struct ieee80211_pkt_buf *skb,
	uint32_t *ht1, uint32_t *plcp)
{
	struct ieee80211_tx_info *txinfo;
	struct ieee80211_tx_rate *rate;
	unsigned int power, chains;
	bool ht_rate;
	
	//txinfo = IEEE80211_SKB_CB(skb); XXX john - get the tx_info for the beacon 
	txinfo = &ieee80211_ibss_bcn_tx_info;
	rate = &txinfo->control.rates[0];
	ht_rate = !!(txinfo->control.rates[0].flags & IEEE80211_TX_RC_MCS);
	ar9170_tx_rate_tpc_chains(ar, txinfo, rate, plcp, &power, &chains);
	
	*ht1 = AR9170_MAC_BCN_HT1_TX_ANT0;
	if (chains == AR9170_TX_PHY_TXCHAIN_2)
		*ht1 |= AR9170_MAC_BCN_HT1_TX_ANT1;
	SET_VAL(AR9170_MAC_BCN_HT1_PWR_CTRL, *ht1, 7);
	SET_VAL(AR9170_MAC_BCN_HT1_TPC, *ht1, power);
	SET_VAL(AR9170_MAC_BCN_HT1_CHAIN_MASK, *ht1, chains);
	
	if (ht_rate) {
		*ht1 |= AR9170_MAC_BCN_HT1_HT_EN;
		if (rate->flags & IEEE80211_TX_RC_SHORT_GI)
			*plcp |= AR9170_MAC_BCN_HT2_SGI;
		
		if (rate->flags & IEEE80211_TX_RC_40_MHZ_WIDTH) {
			*ht1 |= AR9170_MAC_BCN_HT1_BWC_40M_SHARED;
			*plcp |= AR9170_MAC_BCN_HT2_BW40;
		} else if (rate->flags & IEEE80211_TX_RC_DUP_DATA) {
			*ht1 |= AR9170_MAC_BCN_HT1_BWC_40M_DUP;
			*plcp |= AR9170_MAC_BCN_HT2_BW40;
		}
		
		SET_VAL(AR9170_MAC_BCN_HT2_LEN, *plcp, skb->len + FCS_LEN);
	} else {
		if (*plcp <= AR9170_TX_PHY_RATE_CCK_11M)
		     *plcp |= ((skb->len + FCS_LEN) << (3 + 16)) + 0x0400;
		else
		     *plcp |= ((skb->len + FCS_LEN) << 16) + 0x0010;
	}
	
	return ht_rate;
}

int 
ar9170_update_beacon( struct ar9170 * ar, uint8_t submit)
{	
	struct ieee80211_pkt_buf *skb = NULL;
	struct ar9170_vif_info *cvif;
	le32_t *data, *old = NULL;
	uint32_t word, ht1, plcp, off, addr, len;
	int i = 0, err = 0;
	bool ht_rate;
	
	cvif = ar9170_pick_beaconing_vif(ar);
	if (!cvif)
		goto out_unlock;
	
	skb = ieee80211_beacon_get_tim(ar->hw, ar->vif, NULL, NULL);
	
	if (!skb) {
		err = -ENOMEM;
		goto err_free;
	}
	
	for (i=0; i<skb->len; i++) 
		PRINTF("%02x ",skb->data[i]);
	PRINTF("\n");
		
	data = (le32_t *)skb->data;
	if (cvif->beacon)
		old = (le32_t *)cvif->beacon->data;
	
	off = cvif->id * AR9170_MAC_BCN_LENGTH_MAX;
	addr = ar->fw.beacon_addr + off;
	len = roundup(skb->len + FCS_LEN, 4);
	
	if ((off + len) > ar->fw.beacon_max_len) {
		if (1) {
			PRINTF("beacon does not fit into device memory!\n");
		}
		err = -EINVAL;
		goto err_unlock;
	}
	
	if (len > AR9170_MAC_BCN_LENGTH_MAX) {
		if (1) {
			printf("no support for beacons bigger than %d (yours:%d).\n",
				AR9170_MAC_BCN_LENGTH_MAX, len);
		}
		
		err = -EMSGSIZE;
		goto err_unlock;
	}
	
	ht_rate = ar9170_tx_beacon_physet(ar, skb, &ht1, &plcp);
	PRINTF("ht_rate: %u ht1: %08x plcp: %08x\n",ht_rate,ht1,plcp);
	ar9170_async_regwrite_begin(ar);
	ar9170_async_regwrite(AR9170_MAC_REG_BCN_HT1, ht1);
	if (ht_rate)
	    ar9170_async_regwrite(AR9170_MAC_REG_BCN_HT2, plcp);
	else
		ar9170_async_regwrite(AR9170_MAC_REG_BCN_PLCP, plcp);
	
	for (i = 0; i < DIV_ROUND_UP(skb->len, 4); i++) {
		/*
		 * XXX: This accesses beyond skb data for up
		 *  to the last 3 bytes!!
		 */
		
		if (old && (data[i] == old[i]))
			continue;
		
		word = le32_to_cpu(data[i]);
		ar9170_async_regwrite(addr + 4 * i, word);
	}
	ar9170_async_regwrite_finish();
	
	cvif->beacon = NULL;
	
	err = ar9170_async_regwrite_result();
	if (!err)
	    cvif->beacon = skb;
	
	if (err)
		goto err_free;
	
	if (submit) {
		err = ar9170_bcn_ctrl(ar, cvif->id,AR9170_BCN_CTRL_CAB_TRIGGER,
		      addr, skb->len + FCS_LEN);
		
		if (err)
		    goto err_free;
	}
out_unlock:
	return 0;
err_unlock:
	return err;	
err_free:
	return err;
}
#endif /* WITH_WIFI_SUPPORT */
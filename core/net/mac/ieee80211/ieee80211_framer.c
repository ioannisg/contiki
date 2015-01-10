/*
 * ieee80211_framer.c
 *
 * Created: 2/12/2014 7:11:12 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "ieee80211.h"
#include "cfg80211.h"
#include "include/bitops.h"
#include "ieee80211_framer.h"
#include "net/packetbuf.h"
#include "ieee80211_ibss.h"

#include "linkaddrx.h"

#define DEBUG 0

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINTADDR(addr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5])
#else
#define PRINTF(...)
#define PRINTADDR(addr)
#endif

#define QOS_LEN		2
#define ENCAPS_LEN	6

#include "contiki-conf.h"
#if WITH_WIFI_SUPPORT

#define IEEE80211_BCAST_ADDR { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } }

static int 
ieee80211_frame_duration(enum ieee80211_band band, size_t len,
	int rate, int erp, int short_preamble)
{
	int dur;

	/* calculate duration (in microseconds, rounded up to next higher
	 * integer if it includes a fractional microsecond) to send frame of
	 * len bytes (does not include FCS) at the given rate. Duration will
	 * also include SIFS.
	 *
	 * rate is in 100 kbps, so divident is multiplied by 10 in the
	 * DIV_ROUND_UP() operations.
	 */

	if (band == IEEE80211_BAND_5GHZ || erp) {
		/*
		 * OFDM:
		 *
		 * N_DBPS = DATARATE x 4
		 * N_SYM = Ceiling((16+8xLENGTH+6) / N_DBPS)
		 *	(16 = SIGNAL time, 6 = tail bits)
		 * TXTIME = T_PREAMBLE + T_SIGNAL + T_SYM x N_SYM + Signal Ext
		 *
		 * T_SYM = 4 usec
		 * 802.11a - 17.5.2: aSIFSTime = 16 usec
		 * 802.11g - 19.8.4: aSIFSTime = 10 usec +
		 *	signal ext = 6 usec
		 */
		dur = 16; /* SIFS + signal ext */
		dur += 16; /* 17.3.2.3: T_PREAMBLE = 16 usec */
		dur += 4; /* 17.3.2.3: T_SIGNAL = 4 usec */
		dur += 4 * DIV_ROUND_UP((16 + 8 * (len + 4) + 6) * 10,
					4 * rate); /* T_SYM x N_SYM */
		
		
	} else {
		/*
		 * 802.11b or 802.11g with 802.11b compatibility:
		 * 18.3.4: TXTIME = PreambleLength + PLCPHeaderTime +
		 * Ceiling(((LENGTH+PBCC)x8)/DATARATE). PBCC=0.
		 *
		 * 802.11 (DS): 15.3.3, 802.11b: 18.3.4
		 * aSIFSTime = 10 usec
		 * aPreambleLength = 144 usec or 72 usec with short preamble
		 * aPLCPHeaderLength = 48 usec or 24 usec with short preamble
		 */
		dur = 10; /* aSIFSTime = 10 usec */
		dur += short_preamble ? (72 + 24) : (144 + 48);

		dur += DIV_ROUND_UP(8 * (len + 4) * 10, rate);
		// XXX extra		
		dur += 50;
	}

	return dur;
}
/*---------------------------------------------------------------------------*/
static le16_t 
ieee80211_duration(struct ieee80211_hdr* hdr, int group_addr)
{
	int dur, rate, mrate, erp;
	UNUSED(mrate);
	
	if (ieee80211_is_ctl(hdr->frame_control)) {
		PRINTF("WARNING: Ctrl frame. No duration calculation.\n");
		return 0;
	}
	
	if (group_addr) {
		PRINTF("WARNING: Multi-cast frame. No ACK --> duration calculation.\n");
		return 0;
	}
	
	/* Manually setting the rate FIXME - automate it TODO */
	rate = 10;
	erp = 0;
	
	/* Don't calculate ACKs for QoS Frames with NoAck Policy set */
	if (ieee80211_is_data_qos(hdr->frame_control) &&
		*(ieee80211_get_qos_ctl(hdr)) & IEEE80211_QOS_CTL_ACK_POLICY_NOACK) {
		PRINTF("WARNING: No ACKs for QoS frames with NoAck Policy.\n");
		dur = 0;
	} else {
		dur = ieee80211_frame_duration(ibss_info.ibss_channel.band, 10, rate, 
			erp, ieee80211_ibss_vif.bss_conf.use_short_preamble);
	}
	
	PRINTF("IEEE80211_RDC: Duration id: %04x.\n", cpu_to_le16(dur));
	/* TODO - for fragmented packets we should update the duration.
	 * But we do not support it for now. 
	 */
	return cpu_to_le16(dur);
}
/*---------------------------------------------------------------------------*/
static int
parse(void)
{
	if (packetbuf_datalen() < sizeof(struct ieee80211_hdr_3addr)) {
		PRINTF("IEEE80211_FRAMER: too-short pkt [%u]\n",packetbuf_datalen());
		return FRAMER_FAILED;
	}
	
	struct ieee80211_hdr_3addr *hdr = packetbuf_hdrptr();
	/* Frame control parsing */
	le16_t fc = hdr->frame_control;
	if (ieee80211_has_fromds(fc) || ieee80211_has_tods(fc)) {
		PRINTF("IEEE80211_FRAMER: inv-ibss-from/to\n");
		return FRAMER_FAILED;
	}	
	/* FC: packet type */
	if (ieee80211_is_data(fc)) {
		packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, 
			PACKETBUF_ATTR_PACKET_TYPE_DATA);
		goto proceed;
	}
	if (ieee80211_is_beacon(fc)) {
		packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, 
			PACKETBUF_ATTR_PACKET_TYPE_BCN);
		goto proceed;
	}
	if (ieee80211_is_atim(fc)) {
		packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, 
			PACKETBUF_ATTR_PACKET_TYPE_ATIM);
		goto proceed;
	}
	return FRAMER_FAILED;
proceed:
	if (ieee80211_has_pm(fc))
		packetbuf_set_attr(PACKETBUF_ATTR_STA_PSM, 1);
	if (ieee80211_has_retry(fc))
		packetbuf_set_attr(PACKETBUF_ATTR_REXMIT, 1);
	/* Register addresses */
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &hdr->addr2);
	
	const linkaddr1_t bcast_addr = IEEE80211_BCAST_ADDR;
	if (linkaddr1_cmp((linkaddr1_t*)&hdr->addr1, &bcast_addr))
		packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, (linkaddr_t*)&linkaddr1_null);
	else
		packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &hdr->addr1);
		
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &hdr->addr3);
	/* Register attributes */
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, le16_to_cpu(hdr->seq_ctrl));
	
	PRINTF("IEEE80211-IN: ");
	PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
	PRINTADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	PRINTF("%u (%u)\n", packetbuf_datalen(), hdr->seq_ctrl);
	/* Header parsed & removed */
	if (packetbuf_hdrreduce(QOS_LEN+ENCAPS_LEN+sizeof(struct ieee80211_hdr_3addr)))
		return sizeof(struct ieee80211_hdr_3addr);
	
	return FRAMER_FAILED;	
}
/*---------------------------------------------------------------------------*/
static int
create(void)
{
	uint8_t hdr_len = 
		sizeof(struct ieee80211_hdr_3addr) + QOS_LEN + ENCAPS_LEN;
	
	if(packetbuf_hdralloc(hdr_len)) {
		struct ieee80211_hdr_3addr *hdr = packetbuf_hdrptr();
		memset(hdr, 0, sizeof(struct ieee80211_hdr_3addr));
		/* Fill-in the 802.11 header  */
		hdr->seq_ctrl = packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO);		
		switch(packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE)) {
		case PACKETBUF_ATTR_PACKET_TYPE_DATA:
			hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA | 
				IEEE80211_STYPE_DATA);			
			break;
		case PACKETBUF_ATTR_PACKET_TYPE_ATIM:
			hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT | 
				IEEE80211_STYPE_ATIM);			
			break;
		default:
			PRINTF("IEEE80211_FRAMER: err-type\n");
			return FRAMER_FAILED;
		}
		
		if (linkaddr1_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), 
			&linkaddr1_null)) {
			const linkaddr1_t bcast_addr = IEEE80211_BCAST_ADDR;
			linkaddr1_copy((linkaddr1_t*)(hdr->addr1),
				(const linkaddr_t*)&bcast_addr);
		} else {
			linkaddr1_copy((linkaddr1_t*)(hdr->addr1),
				packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
		}		
		linkaddr1_copy((linkaddr1_t*)(hdr->addr2),
			packetbuf_addr(PACKETBUF_ADDR_SENDER));
		linkaddr1_copy((linkaddr1_t*)(hdr->addr3),
			packetbuf_addr(PACKETBUF_ADDR_ERECEIVER));
			
		hdr->frame_control |= cpu_to_le16(IEEE80211_STYPE_QOS_DATA);
		
		//hdr->frame_control |= IEEE80211_FCTL_PROTECTED;
		
		if (packetbuf_attr(PACKETBUF_ATTR_STA_PSM))
			hdr->frame_control |= IEEE80211_FCTL_PM;
			
		if (linkaddr1_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &linkaddr1_null))	
			packetbuf_set_attr(PACKETBUF_ATTR_MAC_ACK, 1);	
		
		/* Set-up encapsulation field info [TODO check if required] */
		uint8_t encaps_data[ENCAPS_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};		
		memcpy(((uint8_t*)packetbuf_dataptr())-ENCAPS_LEN, 
			encaps_data, ENCAPS_LEN);
		/* Zero QoS bytes */
		memset(((uint8_t*)packetbuf_hdrptr())+
			sizeof(struct ieee80211_hdr_3addr),0,2);
			
		hdr->duration_id = ieee80211_duration(hdr, 
			ieee80211_ibss_vif.bss_conf.use_short_preamble);		
		
		#if DEBUG
		int i;
		PRINTF("IEEE80211_FRAMER: (%u) ",packetbuf_totlen());
		uint8_t *frame_data = packetbuf_hdrptr();
		for (i=0; i<packetbuf_totlen(); i++)
			PRINTF("%02x ",frame_data[i]);
		PRINTF("\n");	
		#endif
		return hdr_len;
	} else {
		PRINTF("802.11-OUT: too large header: %u\n", hdr_len);
		return FRAMER_FAILED;
	}
}
/*---------------------------------------------------------------------------*/
const struct framer ieee80211_framer = {
	create, parse
};
#endif /* WITH_WIFI_SUPPORT */
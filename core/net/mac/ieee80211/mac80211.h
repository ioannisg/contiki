/*
 * mac80211.h
 *
 * Created: 1/29/2014 3:29:29 PM
 *  Author: Ioannis Glaropoulos
 */ 


#ifndef MAC80211_H_
#define MAC80211_H_

#include "cfg80211.h"
#include "netdev_features.h"
#include "etherdevice.h"
#include "bitops.h"
#include "ieee80211.h"
#include "nl80211.h"
#include <stdio.h>
#include "string.h"


/**
341  *      struct sk_buff - socket buffer
342  *      @next: Next buffer in list
343  *      @prev: Previous buffer in list
344  *      @tstamp: Time we arrived
345  *      @sk: Socket we are owned by
346  *      @dev: Device we arrived on/are leaving by
347  *      @cb: Control buffer. Free for use by every layer. Put private vars here
348  *      @_skb_refdst: destination entry (with norefcount bit)
349  *      @sp: the security path, used for xfrm
350  *      @len: Length of actual data
351  *      @data_len: Data length
352  *      @mac_len: Length of link layer header
353  *      @hdr_len: writable header length of cloned skb
354  *      @csum: Checksum (must include start/offset pair)
355  *      @csum_start: Offset from skb->head where checksumming should start
356  *      @csum_offset: Offset from csum_start where checksum should be stored
357  *      @priority: Packet queueing priority
358  *      @local_df: allow local fragmentation
359  *      @cloned: Head may be cloned (check refcnt to be sure)
360  *      @ip_summed: Driver fed us an IP checksum
361  *      @nohdr: Payload reference only, must not modify header
362  *      @nfctinfo: Relationship of this skb to the connection
363  *      @pkt_type: Packet class
364  *      @fclone: skbuff clone status
365  *      @ipvs_property: skbuff is owned by ipvs
366  *      @peeked: this packet has been seen already, so stats have been
367  *              done for it, don't do them again
368  *      @nf_trace: netfilter packet trace flag
369  *      @protocol: Packet protocol from driver
370  *      @destructor: Destruct function
371  *      @nfct: Associated connection, if any
372  *      @nf_bridge: Saved data about a bridged frame - see br_netfilter.c
373  *      @skb_iif: ifindex of device we arrived on
374  *      @tc_index: Traffic control index
375  *      @tc_verd: traffic control verdict
376  *      @rxhash: the packet hash computed on receive
377  *      @queue_mapping: Queue mapping for multiqueue devices
378  *      @ndisc_nodetype: router type (from link layer)
379  *      @ooo_okay: allow the mapping of a socket to a queue to be changed
380  *      @l4_rxhash: indicate rxhash is a canonical 4-tuple hash over transport
381  *              ports.
382  *      @wifi_acked_valid: wifi_acked was set
383  *      @wifi_acked: whether frame was acked on wifi or not
384  *      @no_fcs:  Request NIC to treat last 4 bytes as Ethernet FCS
385  *      @dma_cookie: a cookie to one of several possible DMA operations
386  *              done by skb DMA functions
387   *     @napi_id: id of the NAPI struct this skb came from
388  *      @secmark: security marking
389  *      @mark: Generic packet mark
390  *      @dropcount: total number of sk_receive_queue overflows
391  *      @vlan_proto: vlan encapsulation protocol
392  *      @vlan_tci: vlan tag control information
393  *      @inner_protocol: Protocol (encapsulation)
394  *      @inner_transport_header: Inner transport layer header (encapsulation)
395  *      @inner_network_header: Network layer header (encapsulation)
396  *      @inner_mac_header: Link layer header (encapsulation)
397  *      @transport_header: Transport layer header
398  *      @network_header: Network layer header
399  *      @mac_header: Link layer header
400  *      @tail: Tail pointer
401  *      @end: End pointer
402  *      @head: Head of buffer
403  *      @data: Data head pointer
404  *      @truesize: Buffer size
405  *      @users: User count - see {datagram,tcp}.c
406  */

struct sk_buff;
struct sk_buff {
	         /* These two members must be first. */
	struct sk_buff          *next;
	struct sk_buff          *prev;
	
	uint64_t                 tstamp;
	
	struct sock             *sk;
	struct net_device       *dev;
	
	/*
	419          * This is the control buffer. It is free to use for every
	420          * layer. Please put your private variables there. If you
	421          * want to keep them across layers you have to do a skb_clone()
	422          * first. This is owned by whoever has the skb queued ATM.
	423          */
	char                    cb[48] __attribute__((aligned(8)));
	
	unsigned long           _skb_refdst;
	#ifdef CONFIG_XFRM
	struct  sec_path        *sp;
	#endif
	unsigned int            len,
	data_len;
	uint16_t                   mac_len,
	    hdr_len;
	union {
		//csum;
		struct {
			uint16_t   csum_start;
			uint16_t   csum_offset;
		};
	};
	uint32_t                   priority;
	//kmemcheck_bitfield_begin(flags1);
	uint8_t                    local_df:1,
	                                 cloned:1,
	                                 ip_summed:2,
	                                 nohdr:1,
	                                 nfctinfo:3;
	 uint8_t                    pkt_type:3,
	                                 fclone:2,
	                                 ipvs_property:1,
	                                 peeked:1,
	                                 nf_trace:1;
	//kmemcheck_bitfield_end(flags1);
	be16_t                  protocol;
	
	void                    (*destructor)(struct sk_buff *skb);
	#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
	458         struct nf_conntrack     *nfct;
	#endif
	#ifdef CONFIG_BRIDGE_NETFILTER
	461         struct nf_bridge_info   *nf_bridge;
	#endif
	
	         int                     skb_iif;
	
	         uint32_t                   rxhash;
	
	         be16_t                  vlan_proto;
	         uint16_t                   vlan_tci;
	
	#ifdef CONFIG_NET_SCHED
	uint16_t                   tc_index;       /* traffic control index */
	#ifdef CONFIG_NET_CLS_ACT
	uint16_t                   tc_verd;        /* traffic control verdict */
	#endif
	#endif
	
	uint16_t                   queue_mapping;
	//kmemcheck_bitfield_begin(flags2);
	#ifdef CONFIG_IPV6_NDISC_NODETYPE
	         uint8_t                    ndisc_nodetype:2;
	#endif
	         uint8_t                     pfmemalloc:1;
	         uint8_t                    ooo_okay:1;
	         uint8_t                    l4_rxhash:1;
	         uint8_t                    wifi_acked_valid:1;
	         uint8_t                    wifi_acked:1;
	         uint8_t                    no_fcs:1;
	         uint8_t                    head_frag:1;
	         /* Encapsulation protocol and NIC drivers should use
	491          * this flag to indicate to each other if the skb contains
	492          * encapsulated packet or not and maybe use the inner packet
	493          * headers if needed
	494          */
	uint8_t    encapsulation:1;
	/* 6/8 bit hole (depending on ndisc_nodetype presence) */
	//kmemcheck_bitfield_end(flags2);
	
	#if defined CONFIG_NET_DMA || defined CONFIG_NET_RX_BUSY_POLL
	         union {
		                 unsigned int    napi_id;
		                 dma_cookie_t    dma_cookie;
	};
	#endif
	#ifdef CONFIG_NETWORK_SECMARK
	uint32_t               secmark;
	#endif
	union {
		uint32_t           mark;
		uint32_t           dropcount;
		uint32_t           reserved_tailroom;
	};
	
	be16_t                inner_protocol;
	uint16_t              inner_transport_header;
	uint16_t              inner_network_header;
	uint16_t                   inner_mac_header;
	uint16_t                   transport_header;
	uint16_t                   network_header;
	uint16_t                   mac_header;
	/* These elements must be at the end, see alloc_skb() for details.  */
	//sk_buff_data_t          tail;
	//sk_buff_data_t          end;
	unsigned char           *head,  *data;
	unsigned int            truesize;
	uint8_t                users;
};








/**
 * enum ieee80211_max_queues - maximum number of queues
 *
 * @IEEE80211_MAX_QUEUES: Maximum number of regular device queues.
 * @IEEE80211_MAX_QUEUE_MAP: bitmap with maximum queues set
 */
 enum ieee80211_max_queues {
    IEEE80211_MAX_QUEUES = 16,
	IEEE80211_MAX_QUEUE_MAP = BIT(IEEE80211_MAX_QUEUES) - 1,
};

#define IEEE80211_INVAL_HW_QUEUE        0xff

/**
 * enum ieee80211_ac_numbers - AC numbers as used in mac80211
 * @IEEE80211_AC_VO: voice
 * @IEEE80211_AC_VI: video
 * @IEEE80211_AC_BE: best effort
 * @IEEE80211_AC_BK: background
 */
enum ieee80211_ac_numbers {
	IEEE80211_AC_VO         = 0,
	IEEE80211_AC_VI         = 1,
	IEEE80211_AC_BE         = 2,
	IEEE80211_AC_BK         = 3,
};
#define IEEE80211_NUM_ACS       4

/**
 * struct ieee80211_tx_queue_params - transmit queue configuration
 *
 * The information provided in this structure is required for QoS
 * transmit queue configuration. Cf. IEEE 802.11 7.3.2.29.
 *
 * @aifs: arbitration interframe space [0..255]
 * @cw_min: minimum contention window [a value of the form
 *      2^n-1 in the range 1..32767]
 * @cw_max: maximum contention window [like @cw_min]
 * @txop: maximum burst time in units of 32 usecs, 0 meaning disabled
 * @acm: is mandatory admission control required for the access category
 * @uapsd: is U-APSD mode enabled for the queue
 */
struct ieee80211_tx_queue_params {
	uint16_t txop;
	uint16_t cw_min;
	uint16_t cw_max;
	uint8_t aifs;
	bool acm;
	bool uapsd;
};

struct ieee80211_low_level_stats {
	         unsigned int dot11ACKFailureCount;
	         unsigned int dot11RTSFailureCount;
	         unsigned int dot11FCSErrorCount;
	         unsigned int dot11RTSSuccessCount;
};


/**
 * enum ieee80211_chanctx_change - change flag for channel context
 * @IEEE80211_CHANCTX_CHANGE_WIDTH: The channel width changed
 * @IEEE80211_CHANCTX_CHANGE_RX_CHAINS: The number of RX chains changed
 * @IEEE80211_CHANCTX_CHANGE_RADAR: radar detection flag changed
 * @IEEE80211_CHANCTX_CHANGE_CHANNEL: switched to another operating channel,
 *      this is used only with channel switching with CSA
 */
enum ieee80211_chanctx_change {
	         IEEE80211_CHANCTX_CHANGE_WIDTH          = BIT(0),
	         IEEE80211_CHANCTX_CHANGE_RX_CHAINS      = BIT(1),
	         IEEE80211_CHANCTX_CHANGE_RADAR          = BIT(2),
	         IEEE80211_CHANCTX_CHANGE_CHANNEL        = BIT(3),
};


/* maximum number of rate stages */
#define IEEE80211_TX_MAX_RATES	4

/**
166  * struct ieee80211_chanctx_conf - channel context that vifs may be tuned to
167  *
168  * This is the driver-visible part. The ieee80211_chanctx
169  * that contains it is visible in mac80211 only.
170  *
171  * @def: the channel definition
172  * @rx_chains_static: The number of RX chains that must always be
173  *      active on the channel to receive MIMO transmissions
174  * @rx_chains_dynamic: The number of RX chains that must be enabled
175  *      after RTS/CTS handshake to receive SMPS MIMO transmissions;
176  *      this will always be >= @rx_chains_static.
177  * @radar_enabled: whether radar detection is enabled on this channel.
178  * @drv_priv: data area for driver use, will always be aligned to
179  *      sizeof(void *), size is determined in hw information.
180  */
struct ieee80211_chanctx_conf {
	struct cfg80211_chan_def def;
	
	uint8_t rx_chains_static, rx_chains_dynamic;
	
	bool radar_enabled;
	
	uint8_t drv_priv[0] __attribute__((aligned(sizeof(void *))));
};

/**
 * enum ieee80211_bss_change - BSS change notification flags
 *
 * These flags are used with the bss_info_changed() callback
 * to indicate which BSS parameter changed.
 *
 * @BSS_CHANGED_ASSOC: association status changed (associated/disassociated),
 *      also implies a change in the AID.
 * @BSS_CHANGED_ERP_CTS_PROT: CTS protection changed
 * @BSS_CHANGED_ERP_PREAMBLE: preamble changed
 * @BSS_CHANGED_ERP_SLOT: slot timing changed
 * @BSS_CHANGED_HT: 802.11n parameters changed
 * @BSS_CHANGED_BASIC_RATES: Basic rateset changed
 * @BSS_CHANGED_BEACON_INT: Beacon interval changed
 * @BSS_CHANGED_BSSID: BSSID changed, for whatever
 *      reason (IBSS and managed mode)
 * @BSS_CHANGED_BEACON: Beacon data changed, retrieve
 *      new beacon (beaconing modes)
 * @BSS_CHANGED_BEACON_ENABLED: Beaconing should be
 *      enabled/disabled (beaconing modes)
 * @BSS_CHANGED_CQM: Connection quality monitor config changed
 * @BSS_CHANGED_IBSS: IBSS join status changed
 * @BSS_CHANGED_ARP_FILTER: Hardware ARP filter address list or state changed.
 * @BSS_CHANGED_QOS: QoS for this association was enabled/disabled. Note
 *      that it is only ever disabled for station mode.
 * @BSS_CHANGED_IDLE: Idle changed for this BSS/interface.
 * @BSS_CHANGED_SSID: SSID changed for this BSS (AP and IBSS mode)
 * @BSS_CHANGED_AP_PROBE_RESP: Probe Response changed for this BSS (AP mode)
 * @BSS_CHANGED_PS: PS changed for this BSS (STA mode)
 * @BSS_CHANGED_TXPOWER: TX power setting changed for this interface
 * @BSS_CHANGED_P2P_PS: P2P powersave settings (CTWindow, opportunistic PS)
 *      changed (currently only in P2P client mode, GO mode will be later)
 * @BSS_CHANGED_BEACON_INFO: Data from the AP's beacon became available:
 *      currently dtim_period only is under consideration.
 * @BSS_CHANGED_BANDWIDTH: The bandwidth used by this interface changed,
 *      note that this is only called when it changes after the channel
 *      context had been assigned.
 */
enum ieee80211_bss_change {
	         BSS_CHANGED_ASSOC               = 1<<0,
	         BSS_CHANGED_ERP_CTS_PROT        = 1<<1,
	         BSS_CHANGED_ERP_PREAMBLE        = 1<<2,
	         BSS_CHANGED_ERP_SLOT            = 1<<3,
	         BSS_CHANGED_HT                  = 1<<4,
	         BSS_CHANGED_BASIC_RATES         = 1<<5,
	         BSS_CHANGED_BEACON_INT          = 1<<6,
	         BSS_CHANGED_BSSID               = 1<<7,
	         BSS_CHANGED_BEACON              = 1<<8,
	         BSS_CHANGED_BEACON_ENABLED      = 1<<9,
	         BSS_CHANGED_CQM                 = 1<<10,
	         BSS_CHANGED_IBSS                = 1<<11,
	         BSS_CHANGED_ARP_FILTER          = 1<<12,
	         BSS_CHANGED_QOS                 = 1<<13,
	         BSS_CHANGED_IDLE                = 1<<14,
	         BSS_CHANGED_SSID                = 1<<15,
	         BSS_CHANGED_AP_PROBE_RESP       = 1<<16,
	         BSS_CHANGED_PS                  = 1<<17,
	         BSS_CHANGED_TXPOWER             = 1<<18,
	         BSS_CHANGED_P2P_PS              = 1<<19,
	         BSS_CHANGED_BEACON_INFO         = 1<<20,
	         BSS_CHANGED_BANDWIDTH           = 1<<21,
	
	         /* when adding here, make sure to change ieee80211_reconfig */
};


/*
 * The maximum number of IPv4 addresses listed for ARP filtering. If the number
 * of addresses for an interface increase beyond this value, hardware ARP
 * filtering will be disabled.
 */
#define IEEE80211_BSS_ARP_ADDR_LIST_LEN 4

/**
 * enum ieee80211_rssi_event - RSSI threshold event
 * An indicator for when RSSI goes below/above a certain threshold.
 * @RSSI_EVENT_HIGH: AP's rssi crossed the high threshold set by the driver.
 * @RSSI_EVENT_LOW: AP's rssi crossed the low threshold set by the driver.
 */
enum ieee80211_rssi_event {
	         RSSI_EVENT_HIGH,
	         RSSI_EVENT_LOW,
};

/**
 * struct ieee80211_bss_conf - holds the BSS's changing parameters
 *
 * This structure keeps information about a BSS (and an association
 * to that BSS) that can change during the lifetime of the BSS.
 *
 * @assoc: association status
 * @ibss_joined: indicates whether this station is part of an IBSS
 *      or not
 * @ibss_creator: indicates if a new IBSS network is being created
 * @aid: association ID number, valid only when @assoc is true
 * @use_cts_prot: use CTS protection
 * @use_short_preamble: use 802.11b short preamble;
 *      if the hardware cannot handle this it must set the
 *      IEEE80211_HW_2GHZ_SHORT_PREAMBLE_INCAPABLE hardware flag
 * @use_short_slot: use short slot time (only relevant for ERP);
 *      if the hardware cannot handle this it must set the
 *      IEEE80211_HW_2GHZ_SHORT_SLOT_INCAPABLE hardware flag
 * @dtim_period: num of beacons before the next DTIM, for beaconing,
 *      valid in station mode only if after the driver was notified
294  *      with the %BSS_CHANGED_BEACON_INFO flag, will be non-zero then.
295  * @sync_tsf: last beacon's/probe response's TSF timestamp (could be old
296  *      as it may have been received during scanning long ago). If the
297  *      HW flag %IEEE80211_HW_TIMING_BEACON_ONLY is set, then this can
298  *      only come from a beacon, but might not become valid until after
299  *      association when a beacon is received (which is notified with the
300  *      %BSS_CHANGED_DTIM flag.)
301  * @sync_device_ts: the device timestamp corresponding to the sync_tsf,
302  *      the driver/device can use this to calculate synchronisation
303  *      (see @sync_tsf)
304  * @sync_dtim_count: Only valid when %IEEE80211_HW_TIMING_BEACON_ONLY
305  *      is requested, see @sync_tsf/@sync_device_ts.
306  * @beacon_int: beacon interval
307  * @assoc_capability: capabilities taken from assoc resp
308  * @basic_rates: bitmap of basic rates, each bit stands for an
309  *      index into the rate table configured by the driver in
310  *      the current band.
311  * @beacon_rate: associated AP's beacon TX rate
312  * @mcast_rate: per-band multicast rate index + 1 (0: disabled)
313  * @bssid: The BSSID for this BSS
314  * @enable_beacon: whether beaconing should be enabled or not
315  * @chandef: Channel definition for this BSS -- the hardware might be
316  *      configured a higher bandwidth than this BSS uses, for example.
317  * @ht_operation_mode: HT operation mode like in &struct ieee80211_ht_operation.
318  *      This field is only valid when the channel type is one of the HT types.
319  * @cqm_rssi_thold: Connection quality monitor RSSI threshold, a zero value
320  *      implies disabled
321  * @cqm_rssi_hyst: Connection quality monitor RSSI hysteresis
322  * @arp_addr_list: List of IPv4 addresses for hardware ARP filtering. The
323  *      may filter ARP queries targeted for other addresses than listed here.
324  *      The driver must allow ARP queries targeted for all address listed here
325  *      to pass through. An empty list implies no ARP queries need to pass.
326  * @arp_addr_cnt: Number of addresses currently on the list. Note that this
327  *      may be larger than %IEEE80211_BSS_ARP_ADDR_LIST_LEN (the arp_addr_list
328  *      array size), it's up to the driver what to do in that case.
329  * @qos: This is a QoS-enabled BSS.
330  * @idle: This interface is idle. There's also a global idle flag in the
331  *      hardware config which may be more appropriate depending on what
332  *      your driver/device needs to do.
333  * @ps: power-save mode (STA only). This flag is NOT affected by
334  *      offchannel/dynamic_ps operations.
335  * @ssid: The SSID of the current vif. Valid in AP and IBSS mode.
336  * @ssid_len: Length of SSID given in @ssid.
337  * @hidden_ssid: The SSID of the current vif is hidden. Only valid in AP-mode.
338  * @txpower: TX power in dBm
339  * @p2p_noa_attr: P2P NoA attribute for P2P powersave
340  */
struct ieee80211_bss_conf {
	const uint8_t *bssid;
	/* association related data */
	bool assoc, ibss_joined;
	bool ibss_creator;
	uint16_t aid;
	/* erp related data */
	bool use_cts_prot;
	bool use_short_preamble;
	bool use_short_slot;
	bool enable_beacon;
	uint8_t dtim_period;
	uint16_t beacon_int;
	uint16_t assoc_capability;
	uint64_t sync_tsf;
	uint32_t sync_device_ts;
	uint8_t sync_dtim_count;
	uint32_t basic_rates;
	struct ieee80211_rate *beacon_rate;
	int mcast_rate[IEEE80211_NUM_BANDS];
	uint16_t ht_operation_mode;
	int32_t cqm_rssi_thold;
	uint32_t cqm_rssi_hyst;
	struct cfg80211_chan_def chandef;
	be32_t arp_addr_list[IEEE80211_BSS_ARP_ADDR_LIST_LEN];
	int arp_addr_cnt;
	bool qos;
	bool idle;
	bool ps;
	uint8_t ssid[IEEE80211_MAX_SSID_LEN];
	size_t ssid_len;
	bool hidden_ssid;
	int txpower;
	struct ieee80211_p2p_noa_attr p2p_noa_attr;
	/* Added by John */
	enum nl80211_channel_type channel_type;
	uint8_t bcn_ctrl_period;
	uint16_t atim_window;
	uint16_t soft_beacon_int;
};


/**
378  * enum mac80211_tx_info_flags - flags to describe transmission information/status
379  *
380  * These flags are used with the @flags member of &ieee80211_tx_info.
381  *
382  * @IEEE80211_TX_CTL_REQ_TX_STATUS: require TX status callback for this frame.
383  * @IEEE80211_TX_CTL_ASSIGN_SEQ: The driver has to assign a sequence
384  *      number to this frame, taking care of not overwriting the fragment
385  *      number and increasing the sequence number only when the
386  *      IEEE80211_TX_CTL_FIRST_FRAGMENT flag is set. mac80211 will properly
387  *      assign sequence numbers to QoS-data frames but cannot do so correctly
388  *      for non-QoS-data and management frames because beacons need them from
389  *      that counter as well and mac80211 cannot guarantee proper sequencing.
390  *      If this flag is set, the driver should instruct the hardware to
391  *      assign a sequence number to the frame or assign one itself. Cf. IEEE
392  *      802.11-2007 7.1.3.4.1 paragraph 3. This flag will always be set for
393  *      beacons and always be clear for frames without a sequence number field.
394  * @IEEE80211_TX_CTL_NO_ACK: tell the low level not to wait for an ack
395  * @IEEE80211_TX_CTL_CLEAR_PS_FILT: clear powersave filter for destination
396  *      station
397  * @IEEE80211_TX_CTL_FIRST_FRAGMENT: this is a first fragment of the frame
398  * @IEEE80211_TX_CTL_SEND_AFTER_DTIM: send this frame after DTIM beacon
399  * @IEEE80211_TX_CTL_AMPDU: this frame should be sent as part of an A-MPDU
400  * @IEEE80211_TX_CTL_INJECTED: Frame was injected, internal to mac80211.
401  * @IEEE80211_TX_STAT_TX_FILTERED: The frame was not transmitted
402  *      because the destination STA was in powersave mode. Note that to
403  *      avoid race conditions, the filter must be set by the hardware or
404  *      firmware upon receiving a frame that indicates that the station
405  *      went to sleep (must be done on device to filter frames already on
406  *      the queue) and may only be unset after mac80211 gives the OK for
407  *      that by setting the IEEE80211_TX_CTL_CLEAR_PS_FILT (see above),
408  *      since only then is it guaranteed that no more frames are in the
409  *      hardware queue.
410  * @IEEE80211_TX_STAT_ACK: Frame was acknowledged
411  * @IEEE80211_TX_STAT_AMPDU: The frame was aggregated, so status
412  *      is for the whole aggregation.
413  * @IEEE80211_TX_STAT_AMPDU_NO_BACK: no block ack was returned,
414  *      so consider using block ack request (BAR).
415  * @IEEE80211_TX_CTL_RATE_CTRL_PROBE: internal to mac80211, can be
416  *      set by rate control algorithms to indicate probe rate, will
417  *      be cleared for fragmented frames (except on the last fragment)
418  * @IEEE80211_TX_INTFL_OFFCHAN_TX_OK: Internal to mac80211. Used to indicate
419  *      that a frame can be transmitted while the queues are stopped for
420  *      off-channel operation.
421  * @IEEE80211_TX_INTFL_NEED_TXPROCESSING: completely internal to mac80211,
422  *      used to indicate that a pending frame requires TX processing before
423  *      it can be sent out.
424  * @IEEE80211_TX_INTFL_RETRIED: completely internal to mac80211,
425  *      used to indicate that a frame was already retried due to PS
426  * @IEEE80211_TX_INTFL_DONT_ENCRYPT: completely internal to mac80211,
427  *      used to indicate frame should not be encrypted
428  * @IEEE80211_TX_CTL_NO_PS_BUFFER: This frame is a response to a poll
429  *      frame (PS-Poll or uAPSD) or a non-bufferable MMPDU and must
430  *      be sent although the station is in powersave mode.
431  * @IEEE80211_TX_CTL_MORE_FRAMES: More frames will be passed to the
432  *      transmit function after the current frame, this can be used
433  *      by drivers to kick the DMA queue only if unset or when the
434  *      queue gets full.
435  * @IEEE80211_TX_INTFL_RETRANSMISSION: This frame is being retransmitted
436  *      after TX status because the destination was asleep, it must not
437  *      be modified again (no seqno assignment, crypto, etc.)
438  * @IEEE80211_TX_INTFL_MLME_CONN_TX: This frame was transmitted by the MLME
439  *      code for connection establishment, this indicates that its status
440  *      should kick the MLME state machine.
441  * @IEEE80211_TX_INTFL_NL80211_FRAME_TX: Frame was requested through nl80211
442  *      MLME command (internal to mac80211 to figure out whether to send TX
443  *      status to user space)
444  * @IEEE80211_TX_CTL_LDPC: tells the driver to use LDPC for this frame
445  * @IEEE80211_TX_CTL_STBC: Enables Space-Time Block Coding (STBC) for this
446  *      frame and selects the maximum number of streams that it can use.
447  * @IEEE80211_TX_CTL_TX_OFFCHAN: Marks this packet to be transmitted on
448  *      the off-channel channel when a remain-on-channel offload is done
449  *      in hardware -- normal packets still flow and are expected to be
450  *      handled properly by the device.
451  * @IEEE80211_TX_INTFL_TKIP_MIC_FAILURE: Marks this packet to be used for TKIP
452  *      testing. It will be sent out with incorrect Michael MIC key to allow
453  *      TKIP countermeasures to be tested.
454  * @IEEE80211_TX_CTL_NO_CCK_RATE: This frame will be sent at non CCK rate.
455  *      This flag is actually used for management frame especially for P2P
456  *      frames not being sent at CCK rate in 2GHz band.
457  * @IEEE80211_TX_STATUS_EOSP: This packet marks the end of service period,
458  *      when its status is reported the service period ends. For frames in
459  *      an SP that mac80211 transmits, it is already set; for driver frames
460  *      the driver may set this flag. It is also used to do the same for
461  *      PS-Poll responses.
462  * @IEEE80211_TX_CTL_USE_MINRATE: This frame will be sent at lowest rate.
463  *      This flag is used to send nullfunc frame at minimum rate when
464  *      the nullfunc is used for connection monitoring purpose.
465  * @IEEE80211_TX_CTL_DONTFRAG: Don't fragment this packet even if it
466  *      would be fragmented by size (this is optional, only used for
467  *      monitor injection).
468  * @IEEE80211_TX_CTL_PS_RESPONSE: This frame is a response to a poll
469  *      frame (PS-Poll or uAPSD).
470  *
471  * Note: If you have to add new flags to the enumeration, then don't
472  *       forget to update %IEEE80211_TX_TEMPORARY_FLAGS when necessary.
473  */
enum mac80211_tx_info_flags {
	         IEEE80211_TX_CTL_REQ_TX_STATUS          = BIT(0),
	         IEEE80211_TX_CTL_ASSIGN_SEQ             = BIT(1),
	         IEEE80211_TX_CTL_NO_ACK                 = BIT(2),
	         IEEE80211_TX_CTL_CLEAR_PS_FILT          = BIT(3),
	         IEEE80211_TX_CTL_FIRST_FRAGMENT         = BIT(4),
	         IEEE80211_TX_CTL_SEND_AFTER_DTIM        = BIT(5),
	         IEEE80211_TX_CTL_AMPDU                  = BIT(6),
	         IEEE80211_TX_CTL_INJECTED               = BIT(7),
	         IEEE80211_TX_STAT_TX_FILTERED           = BIT(8),
	         IEEE80211_TX_STAT_ACK                   = BIT(9),
	         IEEE80211_TX_STAT_AMPDU                 = BIT(10),
	         IEEE80211_TX_STAT_AMPDU_NO_BACK         = BIT(11),
	         IEEE80211_TX_CTL_RATE_CTRL_PROBE        = BIT(12),
	         IEEE80211_TX_INTFL_OFFCHAN_TX_OK        = BIT(13),
	         IEEE80211_TX_INTFL_NEED_TXPROCESSING    = BIT(14),
	         IEEE80211_TX_INTFL_RETRIED              = BIT(15),
	         IEEE80211_TX_INTFL_DONT_ENCRYPT         = BIT(16),
	         IEEE80211_TX_CTL_NO_PS_BUFFER           = BIT(17),
	         IEEE80211_TX_CTL_MORE_FRAMES            = BIT(18),
	         IEEE80211_TX_INTFL_RETRANSMISSION       = BIT(19),
	         IEEE80211_TX_INTFL_MLME_CONN_TX         = BIT(20),
	         IEEE80211_TX_INTFL_NL80211_FRAME_TX     = BIT(21),
	         IEEE80211_TX_CTL_LDPC                   = BIT(22),
	         IEEE80211_TX_CTL_STBC                   = BIT(23) | BIT(24),
	         IEEE80211_TX_CTL_TX_OFFCHAN             = BIT(25),
	         IEEE80211_TX_INTFL_TKIP_MIC_FAILURE     = BIT(26),
	         IEEE80211_TX_CTL_NO_CCK_RATE            = BIT(27),
	         IEEE80211_TX_STATUS_EOSP                = BIT(28),
	         IEEE80211_TX_CTL_USE_MINRATE            = BIT(29),
	         IEEE80211_TX_CTL_DONTFRAG               = BIT(30),
	         IEEE80211_TX_CTL_PS_RESPONSE            = BIT(31),
};

#define IEEE80211_TX_CTL_STBC_SHIFT             23

/**
511  * enum mac80211_tx_control_flags - flags to describe transmit control
512  *
513  * @IEEE80211_TX_CTRL_PORT_CTRL_PROTO: this frame is a port control
514  *      protocol frame (e.g. EAP)
515  *
516  * These flags are used in tx_info->control.flags.
517  */
enum mac80211_tx_control_flags {
	         IEEE80211_TX_CTRL_PORT_CTRL_PROTO       = BIT(0),
};

/*
523  * This definition is used as a mask to clear all temporary flags, which are
524  * set by the tx handlers for each transmission attempt by the mac80211 stack.
525  */
#define IEEE80211_TX_TEMPORARY_FLAGS (IEEE80211_TX_CTL_NO_ACK |               \
         IEEE80211_TX_CTL_CLEAR_PS_FILT | IEEE80211_TX_CTL_FIRST_FRAGMENT |    \
         IEEE80211_TX_CTL_SEND_AFTER_DTIM | IEEE80211_TX_CTL_AMPDU |           \
         IEEE80211_TX_STAT_TX_FILTERED | IEEE80211_TX_STAT_ACK |               \
         IEEE80211_TX_STAT_AMPDU | IEEE80211_TX_STAT_AMPDU_NO_BACK |           \
         IEEE80211_TX_CTL_RATE_CTRL_PROBE | IEEE80211_TX_CTL_NO_PS_BUFFER |    \
         IEEE80211_TX_CTL_MORE_FRAMES | IEEE80211_TX_CTL_LDPC |                \
         IEEE80211_TX_CTL_STBC | IEEE80211_TX_STATUS_EOSP)

/**
536  * enum mac80211_rate_control_flags - per-rate flags set by the
537  *      Rate Control algorithm.
538  *
539  * These flags are set by the Rate control algorithm for each rate during tx,
540  * in the @flags member of struct ieee80211_tx_rate.
541  *
542  * @IEEE80211_TX_RC_USE_RTS_CTS: Use RTS/CTS exchange for this rate.
543  * @IEEE80211_TX_RC_USE_CTS_PROTECT: CTS-to-self protection is required.
544  *      This is set if the current BSS requires ERP protection.
545  * @IEEE80211_TX_RC_USE_SHORT_PREAMBLE: Use short preamble.
546  * @IEEE80211_TX_RC_MCS: HT rate.
547  * @IEEE80211_TX_RC_VHT_MCS: VHT MCS rate, in this case the idx field is split
548  *      into a higher 4 bits (Nss) and lower 4 bits (MCS number)
549  * @IEEE80211_TX_RC_GREEN_FIELD: Indicates whether this rate should be used in
550  *      Greenfield mode.
551  * @IEEE80211_TX_RC_40_MHZ_WIDTH: Indicates if the Channel Width should be 40 MHz.
552  * @IEEE80211_TX_RC_80_MHZ_WIDTH: Indicates 80 MHz transmission
553  * @IEEE80211_TX_RC_160_MHZ_WIDTH: Indicates 160 MHz transmission
554  *      (80+80 isn't supported yet)
555  * @IEEE80211_TX_RC_DUP_DATA: The frame should be transmitted on both of the
556  *      adjacent 20 MHz channels, if the current channel type is
557  *      NL80211_CHAN_HT40MINUS or NL80211_CHAN_HT40PLUS.
558  * @IEEE80211_TX_RC_SHORT_GI: Short Guard interval should be used for this rate.
559  */
enum mac80211_rate_control_flags {
	         IEEE80211_TX_RC_USE_RTS_CTS             = BIT(0),
	         IEEE80211_TX_RC_USE_CTS_PROTECT         = BIT(1),
	         IEEE80211_TX_RC_USE_SHORT_PREAMBLE      = BIT(2),
	
	         /* rate index is an HT/VHT MCS instead of an index */
	         IEEE80211_TX_RC_MCS                     = BIT(3),
	         IEEE80211_TX_RC_GREEN_FIELD             = BIT(4),
	         IEEE80211_TX_RC_40_MHZ_WIDTH            = BIT(5),
	         IEEE80211_TX_RC_DUP_DATA                = BIT(6),
	         IEEE80211_TX_RC_SHORT_GI                = BIT(7),
	         IEEE80211_TX_RC_VHT_MCS                 = BIT(8),
	         IEEE80211_TX_RC_80_MHZ_WIDTH            = BIT(9),
	         IEEE80211_TX_RC_160_MHZ_WIDTH           = BIT(10),
};


/* there are 40 bytes if you don't need the rateset to be kept */
#define IEEE80211_TX_INFO_DRIVER_DATA_SIZE 40

/* if you do need the rateset, then you have less space */
 #define IEEE80211_TX_INFO_RATE_DRIVER_DATA_SIZE 24

/* maximum number of rate stages */
#define IEEE80211_TX_MAX_RATES  4

/* maximum number of rate table entries */
#define IEEE80211_TX_RATE_TABLE_SIZE    4

/**
 * struct ieee80211_tx_rate - rate selection/status
 *
 * @idx: rate index to attempt to send with
 * @flags: rate control flags (&enum mac80211_rate_control_flags)
 * @count: number of tries in this rate before going to the next rate
 *
 * A value of -1 for @idx indicates an invalid rate and, if used
 * in an array of retry rates, that no more rates should be tried.
 *
 * When used for transmit status reporting, the driver should
 * always report the rate along with the flags it used.
 *
 * &struct ieee80211_tx_info contains an array of these structs
 * in the control information, and it will be filled by the rate
 * control algorithm according to what should be sent. For example,
 * if this array contains, in the format { <idx>, <count> } the
 * information
 *    { 3, 2 }, { 2, 2 }, { 1, 4 }, { -1, 0 }, { -1, 0 }
 * then this means that the frame should be transmitted
 * up to twice at rate 3, up to twice at rate 2, and up to four
 * times at rate 1 if it doesn't get acknowledged. Say it gets
 * acknowledged by the peer after the fifth attempt, the status
 * information should then contain
 *   { 3, 2 }, { 2, 2 }, { 1, 1 }, { -1, 0 } ...
 * since it was transmitted twice at rate 3, twice at rate 2
 * and once at rate 1 after which we received an acknowledgement.
 */
struct ieee80211_tx_rate {
	int8_t idx;
	uint8_t count;
	uint8_t flags;
} __attribute__((packed));

#define IEEE80211_MAX_TX_RETRY          31

static inline void ieee80211_rate_set_vht(struct ieee80211_tx_rate *rate,
       uint8_t mcs, uint8_t nss)
{
	         if(mcs & ~0xF)
				printf("mcs error\n");
	         if((nss - 1) & ~0x7)
				printf("nss error\n");
	         rate->idx = ((nss - 1) << 4) | mcs;
}

static inline uint8_t
ieee80211_rate_get_vht_mcs(const struct ieee80211_tx_rate *rate)
{
	return rate->idx & 0xF;
}

static inline uint8_t
ieee80211_rate_get_vht_nss(const struct ieee80211_tx_rate *rate)
{
	         return (rate->idx >> 4) + 1;
}

/**
646  * struct ieee80211_tx_info - skb transmit information
647  *
648  * This structure is placed in skb->cb for three uses:
649  *  (1) mac80211 TX control - mac80211 tells the driver what to do
650  *  (2) driver internal use (if applicable)
651  *  (3) TX status information - driver tells mac80211 what happened
652  *
653  * @flags: transmit info flags, defined above
654  * @band: the band to transmit on (use for checking for races)
655  * @hw_queue: HW queue to put the frame on, skb_get_queue_mapping() gives the AC
656  * @ack_frame_id: internal frame ID for TX status, used internally
657  * @control: union for control data
658  * @status: union for status data
659  * @driver_data: array of driver_data pointers
660  * @ampdu_ack_len: number of acked aggregated frames.
661  *      relevant only if IEEE80211_TX_STAT_AMPDU was set.
662  * @ampdu_len: number of aggregated frames.
663  *      relevant only if IEEE80211_TX_STAT_AMPDU was set.
664  * @ack_signal: signal strength of the ACK frame
665  */
struct ieee80211_tx_info {
	
	uint32_t flags;
	uint8_t band;
	
	uint8_t hw_queue;
	
	uint16_t ack_frame_id;
	
	union {
		struct {
			union {
				/* rate control */
				struct {
					struct ieee80211_tx_rate rates[IEEE80211_TX_MAX_RATES];
					int8_t rts_cts_rate_idx;
				};
				/* only needed before rate control */
				unsigned long jiffies;
			};
			/* NB: vif can be NULL for injected frames */
			
			struct ieee80211_vif *vif;
			struct ieee80211_key_conf *hw_key;
			struct ieee80211_sta *sta;
		
		} control;
		struct {
		struct ieee80211_tx_rate rates[IEEE80211_TX_MAX_RATES];
		int ack_signal;
		uint8_t ampdu_ack_len;
		uint8_t ampdu_len;
		uint8_t antenna;
		/* 21 bytes free */
		} status;	
	};		
};

/**
720  * struct ieee80211_sched_scan_ies - scheduled scan IEs
721  *
722  * This structure is used to pass the appropriate IEs to be used in scheduled
723  * scans for all bands.  It contains both the IEs passed from the userspace
724  * and the ones generated by mac80211.
725  *
726  * @ie: array with the IEs for each supported band
727  * @len: array with the total length of the IEs for each band
728  */
struct ieee80211_sched_scan_ies {
	         uint8_t *ie[IEEE80211_NUM_BANDS];
	         size_t len[IEEE80211_NUM_BANDS];
};

static inline struct ieee80211_tx_info *IEEE80211_SKB_CB(struct sk_buff *skb)
{
	         return (struct ieee80211_tx_info *)skb->cb;
}

static inline struct ieee80211_rx_status *IEEE80211_SKB_RXCB(struct sk_buff *skb)
{
	         return (struct ieee80211_rx_status *)skb->cb;
}

/**
781  * enum mac80211_rx_flags - receive flags
782  *
783  * These flags are used with the @flag member of &struct ieee80211_rx_status.
784  * @RX_FLAG_MMIC_ERROR: Michael MIC error was reported on this frame.
785  *      Use together with %RX_FLAG_MMIC_STRIPPED.
786  * @RX_FLAG_DECRYPTED: This frame was decrypted in hardware.
787  * @RX_FLAG_MMIC_STRIPPED: the Michael MIC is stripped off this frame,
788  *      verification has been done by the hardware.
789  * @RX_FLAG_IV_STRIPPED: The IV/ICV are stripped from this frame.
790  *      If this flag is set, the stack cannot do any replay detection
791  *      hence the driver or hardware will have to do that.
792  * @RX_FLAG_FAILED_FCS_CRC: Set this flag if the FCS check failed on
793  *      the frame.
794  * @RX_FLAG_FAILED_PLCP_CRC: Set this flag if the PCLP check failed on
795  *      the frame.
796  * @RX_FLAG_MACTIME_START: The timestamp passed in the RX status (@mactime
797  *      field) is valid and contains the time the first symbol of the MPDU
798  *      was received. This is useful in monitor mode and for proper IBSS
799  *      merging.
800  * @RX_FLAG_MACTIME_END: The timestamp passed in the RX status (@mactime
801  *      field) is valid and contains the time the last symbol of the MPDU
802  *      (including FCS) was received.
803  * @RX_FLAG_SHORTPRE: Short preamble was used for this frame
804  * @RX_FLAG_HT: HT MCS was used and rate_idx is MCS index
805  * @RX_FLAG_VHT: VHT MCS was used and rate_index is MCS index
806  * @RX_FLAG_40MHZ: HT40 (40 MHz) was used
807  * @RX_FLAG_80MHZ: 80 MHz was used
808  * @RX_FLAG_80P80MHZ: 80+80 MHz was used
809  * @RX_FLAG_160MHZ: 160 MHz was used
810  * @RX_FLAG_SHORT_GI: Short guard interval was used
811  * @RX_FLAG_NO_SIGNAL_VAL: The signal strength value is not present.
812  *      Valid only for data frames (mainly A-MPDU)
813  * @RX_FLAG_HT_GF: This frame was received in a HT-greenfield transmission, if
814  *      the driver fills this value it should add %IEEE80211_RADIOTAP_MCS_HAVE_FMT
815  *      to hw.radiotap_mcs_details to advertise that fact
816  * @RX_FLAG_AMPDU_DETAILS: A-MPDU details are known, in particular the reference
817  *      number (@ampdu_reference) must be populated and be a distinct number for
818  *      each A-MPDU
819  * @RX_FLAG_AMPDU_REPORT_ZEROLEN: driver reports 0-length subframes
820  * @RX_FLAG_AMPDU_IS_ZEROLEN: This is a zero-length subframe, for
821  *      monitoring purposes only
822  * @RX_FLAG_AMPDU_LAST_KNOWN: last subframe is known, should be set on all
823  *      subframes of a single A-MPDU
824  * @RX_FLAG_AMPDU_IS_LAST: this subframe is the last subframe of the A-MPDU
825  * @RX_FLAG_AMPDU_DELIM_CRC_ERROR: A delimiter CRC error has been detected
826  *      on this subframe
827  * @RX_FLAG_AMPDU_DELIM_CRC_KNOWN: The delimiter CRC field is known (the CRC
828  *      is stored in the @ampdu_delimiter_crc field)
829  * @RX_FLAG_STBC_MASK: STBC 2 bit bitmask. 1 - Nss=1, 2 - Nss=2, 3 - Nss=3
830  * @RX_FLAG_10MHZ: 10 MHz (half channel) was used
831  * @RX_FLAG_5MHZ: 5 MHz (quarter channel) was used
832  * @RX_FLAG_AMSDU_MORE: Some drivers may prefer to report separate A-MSDU
833  *      subframes instead of a one huge frame for performance reasons.
834  *      All, but the last MSDU from an A-MSDU should have this flag set. E.g.
835  *      if an A-MSDU has 3 frames, the first 2 must have the flag set, while
836  *      the 3rd (last) one must not have this flag set. The flag is used to
837  *      deal with retransmission/duplication recovery properly since A-MSDU
838  *      subframes share the same sequence number. Reported subframes can be
839  *      either regular MSDU or singly A-MSDUs. Subframes must not be
840  *      interleaved with other frames.
841  */
enum mac80211_rx_flags {
         RX_FLAG_MMIC_ERROR              = BIT(0),
         RX_FLAG_DECRYPTED               = BIT(1),
         RX_FLAG_MMIC_STRIPPED           = BIT(3),
         RX_FLAG_IV_STRIPPED             = BIT(4),
         RX_FLAG_FAILED_FCS_CRC          = BIT(5),
         RX_FLAG_FAILED_PLCP_CRC         = BIT(6),
         RX_FLAG_MACTIME_START           = BIT(7),
         RX_FLAG_SHORTPRE                = BIT(8),
         RX_FLAG_HT                      = BIT(9),
         RX_FLAG_40MHZ                   = BIT(10),
         RX_FLAG_SHORT_GI                = BIT(11),
         RX_FLAG_NO_SIGNAL_VAL           = BIT(12),
         RX_FLAG_HT_GF                   = BIT(13),
         RX_FLAG_AMPDU_DETAILS           = BIT(14),
         RX_FLAG_AMPDU_REPORT_ZEROLEN    = BIT(15),
		 RX_FLAG_AMPDU_IS_ZEROLEN        = BIT(16),
	     RX_FLAG_AMPDU_LAST_KNOWN        = BIT(17),
	     RX_FLAG_AMPDU_IS_LAST           = BIT(18),
	     RX_FLAG_AMPDU_DELIM_CRC_ERROR   = BIT(19),
	     RX_FLAG_AMPDU_DELIM_CRC_KNOWN   = BIT(20),
	     RX_FLAG_MACTIME_END             = BIT(21),
	     RX_FLAG_VHT                     = BIT(22),
	     RX_FLAG_80MHZ                   = BIT(23),
	     RX_FLAG_80P80MHZ                = BIT(24),
	     RX_FLAG_160MHZ                  = BIT(25),
	     RX_FLAG_STBC_MASK               = BIT(26) | BIT(27),
	     RX_FLAG_10MHZ                   = BIT(28),
	     RX_FLAG_5MHZ                    = BIT(29),
	     RX_FLAG_AMSDU_MORE              = BIT(30),
};

#define RX_FLAG_STBC_SHIFT              26

/**
 * struct ieee80211_rx_status - receive status
 *
 * The low-level driver should provide this information (the subset
 * supported by hardware) to the 802.11 code with each received
 * frame, in the skb's control buffer (cb).
 *
 * @mactime: value in microseconds of the 64-bit Time Synchronization Function
 *      (TSF) timer when the first data symbol (MPDU) arrived at the hardware.
 * @device_timestamp: arbitrary timestamp for the device, mac80211 doesn't use
 *      it but can store it and pass it back to the driver for synchronisation
 * @band: the active band when this frame was received
 * @freq: frequency the radio was tuned to when receiving this frame, in MHz
 * @signal: signal strength when receiving this frame, either in dBm, in dB or
 *      unspecified depending on the hardware capabilities flags
 *      @IEEE80211_HW_SIGNAL_*
 * @chains: bitmask of receive chains for which separate signal strength
 *      values were filled.
 * @chain_signal: per-chain signal strength, in dBm (unlike @signal, doesn't
 *      support dB or unspecified units)
 * @antenna: antenna used
 * @rate_idx: index of data rate into band's supported rates or MCS index if
 *      HT or VHT is used (%RX_FLAG_HT/%RX_FLAG_VHT)
 * @vht_nss: number of streams (VHT only)
 * @flag: %RX_FLAG_*
 * @rx_flags: internal RX flags for mac80211
 * @ampdu_reference: A-MPDU reference number, must be a different value for
 *      each A-MPDU but the same for each subframe within one A-MPDU
 * @ampdu_delimiter_crc: A-MPDU delimiter CRC
 * @vendor_radiotap_bitmap: radiotap vendor namespace presence bitmap
 * @vendor_radiotap_len: radiotap vendor namespace length
 * @vendor_radiotap_align: radiotap vendor namespace alignment. Note
 *      that the actual data must be at the start of the SKB data
 *      already.
 * @vendor_radiotap_oui: radiotap vendor namespace OUI
 * @vendor_radiotap_subns: radiotap vendor sub namespace
 */
struct ieee80211_rx_status {
	uint64_t mactime;
	uint32_t device_timestamp;
	uint32_t ampdu_reference;
	uint32_t flag;
	uint32_t vendor_radiotap_bitmap;
	uint16_t vendor_radiotap_len;
	uint16_t freq;
	uint8_t rate_idx;
	uint8_t vht_nss;
	uint8_t rx_flags;
	uint8_t band;
	uint8_t antenna;
	int8_t signal;
	uint8_t chains;
	int8_t chain_signal[IEEE80211_MAX_CHAINS];
	uint8_t ampdu_delimiter_crc;
	uint8_t vendor_radiotap_align;
	uint8_t vendor_radiotap_oui[3];
	uint8_t vendor_radiotap_subns;
};

/**
 * enum ieee80211_conf_flags - configuration flags
 *
 * Flags to define PHY configuration options
 *
 * @IEEE80211_CONF_MONITOR: there's a monitor interface present -- use this
 *      to determine for example whether to calculate timestamps for packets
 *      or not, do not use instead of filter flags!
 * @IEEE80211_CONF_PS: Enable 802.11 power save mode (managed mode only).
 *      This is the power save mode defined by IEEE 802.11-2007 section 11.2,
 *      meaning that the hardware still wakes up for beacons, is able to
 *      transmit frames and receive the possible acknowledgment frames.
 *      Not to be confused with hardware specific wakeup/sleep states,
 *      driver is responsible for that. See the section "Powersave support"
 *      for more.
 * @IEEE80211_CONF_IDLE: The device is running, but idle; if the flag is set
 *      the driver should be prepared to handle configuration requests but
 *      may turn the device off as much as possible. Typically, this flag will
 *      be set when an interface is set UP but not associated or scanning, but
 *      it can also be unset in that case when monitor interfaces are active.
 * @IEEE80211_CONF_OFFCHANNEL: The device is currently not on its main
 *      operating channel.
 */
enum ieee80211_conf_flags {
	IEEE80211_CONF_MONITOR          = (1<<0),
	IEEE80211_CONF_PS               = (1<<1),
	IEEE80211_CONF_IDLE             = (1<<2),
	IEEE80211_CONF_OFFCHANNEL       = (1<<3),
};

/**
* enum ieee80211_conf_changed - denotes which configuration changed
*
* @IEEE80211_CONF_CHANGE_LISTEN_INTERVAL: the listen interval changed
* @IEEE80211_CONF_CHANGE_MONITOR: the monitor flag changed
* @IEEE80211_CONF_CHANGE_PS: the PS flag or dynamic PS timeout changed
* @IEEE80211_CONF_CHANGE_POWER: the TX power changed
* @IEEE80211_CONF_CHANGE_CHANNEL: the channel/channel_type changed
* @IEEE80211_CONF_CHANGE_RETRY_LIMITS: retry limits changed
* @IEEE80211_CONF_CHANGE_IDLE: Idle flag changed
* @IEEE80211_CONF_CHANGE_SMPS: Spatial multiplexing powersave mode changed
*/
enum ieee80211_conf_changed {
	IEEE80211_CONF_CHANGE_SMPS				= BIT(1),
	IEEE80211_CONF_CHANGE_LISTEN_INTERVAL	= BIT(2),
	IEEE80211_CONF_CHANGE_MONITOR			= BIT(3),
	IEEE80211_CONF_CHANGE_PS				= BIT(4),
	IEEE80211_CONF_CHANGE_POWER				= BIT(5),
	IEEE80211_CONF_CHANGE_CHANNEL			= BIT(6),
	IEEE80211_CONF_CHANGE_RETRY_LIMITS		= BIT(7),
	IEEE80211_CONF_CHANGE_IDLE				= BIT(8),
};

/**
 * enum ieee80211_smps_mode - spatial multiplexing power save mode
 *
 * @IEEE80211_SMPS_AUTOMATIC: automatic
 * @IEEE80211_SMPS_OFF: off
 * @IEEE80211_SMPS_STATIC: static
 * @IEEE80211_SMPS_DYNAMIC: dynamic
 * @IEEE80211_SMPS_NUM_MODES: internal, don't use
 */
enum ieee80211_smps_mode {
	         IEEE80211_SMPS_AUTOMATIC,
	         IEEE80211_SMPS_OFF,
	         IEEE80211_SMPS_STATIC,
	         IEEE80211_SMPS_DYNAMIC,
	
	         /* keep last */
	         IEEE80211_SMPS_NUM_MODES,
};

/**
 1011  * struct ieee80211_conf - configuration of the device
 1012  *
 1013  * This struct indicates how the driver shall configure the hardware.
 1014  *
 1015  * @flags: configuration flags defined above
 1016  *
 1017  * @listen_interval: listen interval in units of beacon interval
 1018  * @max_sleep_period: the maximum number of beacon intervals to sleep for
 1019  *      before checking the beacon for a TIM bit (managed mode only); this
 1020  *      value will be only achievable between DTIM frames, the hardware
 1021  *      needs to check for the multicast traffic bit in DTIM beacons.
 1022  *      This variable is valid only when the CONF_PS flag is set.
 1023  * @ps_dtim_period: The DTIM period of the AP we're connected to, for use
 1024  *      in power saving. Power saving will not be enabled until a beacon
 1025  *      has been received and the DTIM period is known.
 1026  * @dynamic_ps_timeout: The dynamic powersave timeout (in ms), see the
 1027  *      powersave documentation below. This variable is valid only when
 1028  *      the CONF_PS flag is set.
 1029  *
 1030  * @power_level: requested transmit power (in dBm), backward compatibility
 1031  *      value only that is set to the minimum of all interfaces
 1032  *
 1033  * @chandef: the channel definition to tune to
 1034  * @radar_enabled: whether radar detection is enabled
 1035  *
 1036  * @long_frame_max_tx_count: Maximum number of transmissions for a "long" frame
 1037  *      (a frame not RTS protected), called "dot11LongRetryLimit" in 802.11,
 1038  *      but actually means the number of transmissions not the number of retries
 1039  * @short_frame_max_tx_count: Maximum number of transmissions for a "short"
 1040  *      frame, called "dot11ShortRetryLimit" in 802.11, but actually means the
 1041  *      number of transmissions not the number of retries
 1042  *
 1043  * @smps_mode: spatial multiplexing powersave mode; note that
 1044  *      %IEEE80211_SMPS_STATIC is used when the device is not
 1045  *      configured for an HT channel.
 1046  *      Note that this is only valid if channel contexts are not used,
 1047  *      otherwise each channel context has the number of chains listed.
 1048  */
struct ieee80211_conf {
	uint32_t flags;
	int power_level, dynamic_ps_timeout;
	int max_sleep_period;
	 
	uint16_t listen_interval;
	uint8_t ps_dtim_period;
	 
	uint8_t long_frame_max_tx_count, short_frame_max_tx_count;
	struct ieee80211_channel *channel;
	enum nl80211_channel_type channel_type;
	
	struct cfg80211_chan_def chandef;
	uint8_t radar_enabled;
	enum ieee80211_smps_mode smps_mode;
};

/**
1065  * struct ieee80211_channel_switch - holds the channel switch data
1066  *
1067  * The information provided in this structure is required for channel switch
1068  * operation.
1069  *
1070  * @timestamp: value in microseconds of the 64-bit Time Synchronization
1071  *      Function (TSF) timer when the frame containing the channel switch
1072  *      announcement was received. This is simply the rx.mactime parameter
1073  *      the driver passed into mac80211.
1074  * @block_tx: Indicates whether transmission must be blocked before the
1075  *      scheduled channel switch, as indicated by the AP.
1076  * @chandef: the new channel to switch to
1077  * @count: the number of TBTT's until the channel switch event
1078  */
 struct ieee80211_channel_switch {
	         uint64_t timestamp;
	         bool block_tx;
	         struct cfg80211_chan_def chandef;
	         uint8_t count;
};

/**
1087  * enum ieee80211_vif_flags - virtual interface flags
1088  *
1089  * @IEEE80211_VIF_BEACON_FILTER: the device performs beacon filtering
1090  *      on this virtual interface to avoid unnecessary CPU wakeups
1091  * @IEEE80211_VIF_SUPPORTS_CQM_RSSI: the device can do connection quality
1092  *      monitoring on this virtual interface -- i.e. it can monitor
1093  *      connection quality related parameters, such as the RSSI level and
1094  *      provide notifications if configured trigger levels are reached.
1095  */
 enum ieee80211_vif_flags {
         IEEE80211_VIF_BEACON_FILTER             = BIT(0),
         IEEE80211_VIF_SUPPORTS_CQM_RSSI         = BIT(1),
};

/**
1102  * struct ieee80211_vif - per-interface data
1103  *
1104  * Data in this structure is continually present for driver
1105  * use during the life of a virtual interface.
1106  *
1107  * @type: type of this virtual interface
1108  * @bss_conf: BSS configuration for this interface, either our own
1109  *      or the BSS we're associated to
1110  * @addr: address of this interface
1111  * @p2p: indicates whether this AP or STA interface is a p2p
1112  *      interface, i.e. a GO or p2p-sta respectively
1113  * @csa_active: marks whether a channel switch is going on
1114  * @driver_flags: flags/capabilities the driver has for this interface,
1115  *      these need to be set (or cleared) when the interface is added
1116  *      or, if supported by the driver, the interface type is changed
1117  *      at runtime, mac80211 will never touch this field
1118  * @hw_queue: hardware queue for each AC
1119  * @cab_queue: content-after-beacon (DTIM beacon really) queue, AP mode only
1120  * @chanctx_conf: The channel context this interface is assigned to, or %NULL
1121  *      when it is not assigned. This pointer is RCU-protected due to the TX
1122  *      path needing to access it; even though the netdev carrier will always
1123  *      be off when it is %NULL there can still be races and packets could be
1124  *      processed after it switches back to %NULL.
1125  * @debugfs_dir: debugfs dentry, can be used by drivers to create own per
1126  *      interface debug files. Note that it will be NULL for the virtual
1127  *      monitor interface (if that is requested.)
1128  * @drv_priv: data area for driver use, will always be aligned to
1129  *      sizeof(void *).
1130  */
struct ieee80211_vif {
	enum nl80211_iftype type;
	struct ieee80211_bss_conf bss_conf;
	uint8_t addr[ETH_ALEN];
	bool p2p;
	bool csa_active;

	uint8_t cab_queue;
	uint8_t hw_queue[IEEE80211_NUM_ACS];

	struct ieee80211_chanctx_conf  *chanctx_conf;

	uint32_t driver_flags;

	#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *debugfs_dir;
	#endif
	
	uint8_t prepared; // Added by John


	/* must be last */
	uint8_t drv_priv[0] __attribute__((aligned(sizeof(void *))));
};

 /**
 1162  * enum ieee80211_key_flags - key flags
 1163  *
 1164  * These flags are used for communication about keys between the driver
 1165  * and mac80211, with the @flags parameter of &struct ieee80211_key_conf.
 1166  *
 1167  * @IEEE80211_KEY_FLAG_GENERATE_IV: This flag should be set by the
 1168  *      driver to indicate that it requires IV generation for this
 1169  *      particular key.
 1170  * @IEEE80211_KEY_FLAG_GENERATE_MMIC: This flag should be set by
 1171  *      the driver for a TKIP key if it requires Michael MIC
 1172  *      generation in software.
 1173  * @IEEE80211_KEY_FLAG_PAIRWISE: Set by mac80211, this flag indicates
 1174  *      that the key is pairwise rather then a shared key.
 1175  * @IEEE80211_KEY_FLAG_SW_MGMT_TX: This flag should be set by the driver for a
 1176  *      CCMP key if it requires CCMP encryption of management frames (MFP) to
 1177  *      be done in software.
 1178  * @IEEE80211_KEY_FLAG_PUT_IV_SPACE: This flag should be set by the driver
 1179  *      if space should be prepared for the IV, but the IV
 1180  *      itself should not be generated. Do not set together with
 1181  *      @IEEE80211_KEY_FLAG_GENERATE_IV on the same key.
 1182  * @IEEE80211_KEY_FLAG_RX_MGMT: This key will be used to decrypt received
 1183  *      management frames. The flag can help drivers that have a hardware
 1184  *      crypto implementation that doesn't deal with management frames
 1185  *      properly by allowing them to not upload the keys to hardware and
 1186  *      fall back to software crypto. Note that this flag deals only with
 1187  *      RX, if your crypto engine can't deal with TX you can also set the
 1188  *      %IEEE80211_KEY_FLAG_SW_MGMT_TX flag to encrypt such frames in SW.
 1189  */
enum ieee80211_key_flags {
	         IEEE80211_KEY_FLAG_GENERATE_IV  = 1<<1,
	         IEEE80211_KEY_FLAG_GENERATE_MMIC= 1<<2,
	         IEEE80211_KEY_FLAG_PAIRWISE     = 1<<3,
	         IEEE80211_KEY_FLAG_SW_MGMT_TX   = 1<<4,
	         IEEE80211_KEY_FLAG_PUT_IV_SPACE = 1<<5,
	         IEEE80211_KEY_FLAG_RX_MGMT      = 1<<6,
};

/**
1200  * struct ieee80211_key_conf - key information
1201  *
1202  * This key information is given by mac80211 to the driver by
1203  * the set_key() callback in &struct ieee80211_ops.
1204  *
1205  * @hw_key_idx: To be set by the driver, this is the key index the driver
1206  *      wants to be given when a frame is transmitted and needs to be
1207  *      encrypted in hardware.
1208  * @cipher: The key's cipher suite selector.
1209  * @flags: key flags, see &enum ieee80211_key_flags.
1210  * @keyidx: the key index (0-3)
1211  * @keylen: key material length
1212  * @key: key material. For ALG_TKIP the key is encoded as a 256-bit (32 byte)
1213  *      data block:
1214  *      - Temporal Encryption Key (128 bits)
1215  *      - Temporal Authenticator Tx MIC Key (64 bits)
1216  *      - Temporal Authenticator Rx MIC Key (64 bits)
1217  * @icv_len: The ICV length for this key type
1218  * @iv_len: The IV length for this key type
1219  */
struct ieee80211_key_conf {
	        uint32_t cipher;
	        uint8_t icv_len;
	        uint8_t iv_len;
	        uint8_t hw_key_idx;
	        uint8_t flags;
	        int8_t keyidx;
	        uint8_t keylen;
	        uint8_t key[0];
};

/**
1232  * enum set_key_cmd - key command
1233  *
1234  * Used with the set_key() callback in &struct ieee80211_ops, this
1235  * indicates whether a key is being removed or added.
1236  *
1237  * @SET_KEY: a key is set
1238  * @DISABLE_KEY: a key must be disabled
1239  */
enum set_key_cmd {
         SET_KEY, DISABLE_KEY,
};

/**
1245  * enum ieee80211_sta_state - station state
1246  *
1247  * @IEEE80211_STA_NOTEXIST: station doesn't exist at all,
1248  *      this is a special state for add/remove transitions
1249  * @IEEE80211_STA_NONE: station exists without special state
1250  * @IEEE80211_STA_AUTH: station is authenticated
1251  * @IEEE80211_STA_ASSOC: station is associated
1252  * @IEEE80211_STA_AUTHORIZED: station is authorized (802.1X)
1253  */
enum ieee80211_sta_state {
         /* NOTE: These need to be ordered correctly! */
         IEEE80211_STA_NOTEXIST,
         IEEE80211_STA_NONE,
         IEEE80211_STA_AUTH,
         IEEE80211_STA_ASSOC,
         IEEE80211_STA_AUTHORIZED,
};

/**
1264  * enum ieee80211_sta_rx_bandwidth - station RX bandwidth
1265  * @IEEE80211_STA_RX_BW_20: station can only receive 20 MHz
1266  * @IEEE80211_STA_RX_BW_40: station can receive up to 40 MHz
1267  * @IEEE80211_STA_RX_BW_80: station can receive up to 80 MHz
1268  * @IEEE80211_STA_RX_BW_160: station can receive up to 160 MHz
1269  *      (including 80+80 MHz)
1270  *
1271  * Implementation note: 20 must be zero to be initialized
1272  *      correctly, the values must be sorted.
1273  */
enum ieee80211_sta_rx_bandwidth {
	         IEEE80211_STA_RX_BW_20 = 0,
	         IEEE80211_STA_RX_BW_40,
	         IEEE80211_STA_RX_BW_80,
	         IEEE80211_STA_RX_BW_160,
};

/**
1282  * struct ieee80211_sta_rates - station rate selection table
1283  *
1284  * @rcu_head: RCU head used for freeing the table on update
1285  * @rate: transmit rates/flags to be used by default.
1286  *      Overriding entries per-packet is possible by using cb tx control.
1287  */
struct ieee80211_sta_rates {
	    //struct rcu_head rcu_head;
	    struct {
		                int8_t idx;
		                uint8_t count;
		                uint8_t count_cts;
		                uint8_t count_rts;
		                uint16_t flags;
        } rate[IEEE80211_TX_RATE_TABLE_SIZE];
};

/**
1300  * struct ieee80211_sta - station table entry
1301  *
1302  * A station table entry represents a station we are possibly
1303  * communicating with. Since stations are RCU-managed in
1304  * mac80211, any ieee80211_sta pointer you get access to must
1305  * either be protected by rcu_read_lock() explicitly or implicitly,
1306  * or you must take good care to not use such a pointer after a
1307  * call to your sta_remove callback that removed it.
1308  *
1309  * @addr: MAC address
1310  * @aid: AID we assigned to the station if we're an AP
1311  * @supp_rates: Bitmap of supported rates (per band)
1312  * @ht_cap: HT capabilities of this STA; restricted to our own capabilities
1313  * @vht_cap: VHT capabilities of this STA; restricted to our own capabilities
1314  * @wme: indicates whether the STA supports WME. Only valid during AP-mode.
1315  * @drv_priv: data area for driver use, will always be aligned to
1316  *      sizeof(void *), size is determined in hw information.
1317  * @uapsd_queues: bitmap of queues configured for uapsd. Only valid
1318  *      if wme is supported.
1319  * @max_sp: max Service Period. Only valid if wme is supported.
1320  * @bandwidth: current bandwidth the station can receive with
1321  * @rx_nss: in HT/VHT, the maximum number of spatial streams the
1322  *      station can receive at the moment, changed by operating mode
1323  *      notifications and capabilities. The value is only valid after
1324  *      the station moves to associated state.
1325  * @smps_mode: current SMPS mode (off, static or dynamic)
1326  * @rates: rate control selection table
1327  */
struct ieee80211_sta {
	         uint32_t supp_rates[IEEE80211_NUM_BANDS];
	         uint8_t addr[ETH_ALEN];
	         uint16_t aid;
	         struct ieee80211_sta_ht_cap ht_cap;
	         struct ieee80211_sta_vht_cap vht_cap;
	         bool wme;
	         uint8_t uapsd_queues;
	         uint8_t max_sp;
	         uint8_t rx_nss;
	         enum ieee80211_sta_rx_bandwidth bandwidth;
	         enum ieee80211_smps_mode smps_mode;
	         struct ieee80211_sta_rates *rates;
	
	         /* must be last */
	         uint8_t drv_priv[0] __attribute__((__aligned__(sizeof(void *))));
};

/**
1347  * enum sta_notify_cmd - sta notify command
1348  *
1349  * Used with the sta_notify() callback in &struct ieee80211_ops, this
1350  * indicates if an associated station made a power state transition.
1351  *
1352  * @STA_NOTIFY_SLEEP: a station is now sleeping
1353  * @STA_NOTIFY_AWAKE: a sleeping station woke up
1354  */
enum sta_notify_cmd {
	         STA_NOTIFY_SLEEP, STA_NOTIFY_AWAKE,
};

/**
1360  * struct ieee80211_tx_control - TX control data
1361  *
1362  * @sta: station table entry, this sta pointer may be NULL and
1363  *      it is not allowed to copy the pointer, due to RCU.
1364  */
struct ieee80211_tx_control {
	         struct ieee80211_sta *sta;
};

/**
1370  * enum ieee80211_hw_flags - hardware flags
1371  *
1372  * These flags are used to indicate hardware capabilities to
1373  * the stack. Generally, flags here should have their meaning
1374  * done in a way that the simplest hardware doesn't need setting
1375  * any particular flags. There are some exceptions to this rule,
1376  * however, so you are advised to review these flags carefully.
1377  *
1378  * @IEEE80211_HW_HAS_RATE_CONTROL:
1379  *      The hardware or firmware includes rate control, and cannot be
1380  *      controlled by the stack. As such, no rate control algorithm
1381  *      should be instantiated, and the TX rate reported to userspace
1382  *      will be taken from the TX status instead of the rate control
1383  *      algorithm.
1384  *      Note that this requires that the driver implement a number of
1385  *      callbacks so it has the correct information, it needs to have
1386  *      the @set_rts_threshold callback and must look at the BSS config
1387  *      @use_cts_prot for G/N protection, @use_short_slot for slot
1388  *      timing in 2.4 GHz and @use_short_preamble for preambles for
1389  *      CCK frames.
1390  *
1391  * @IEEE80211_HW_RX_INCLUDES_FCS:
1392  *      Indicates that received frames passed to the stack include
1393  *      the FCS at the end.
1394  *
1395  * @IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING:
1396  *      Some wireless LAN chipsets buffer broadcast/multicast frames
1397  *      for power saving stations in the hardware/firmware and others
1398  *      rely on the host system for such buffering. This option is used
1399  *      to configure the IEEE 802.11 upper layer to buffer broadcast and
1400  *      multicast frames when there are power saving stations so that
1401  *      the driver can fetch them with ieee80211_get_buffered_bc().
1402  *
1403  * @IEEE80211_HW_2GHZ_SHORT_SLOT_INCAPABLE:
1404  *      Hardware is not capable of short slot operation on the 2.4 GHz band.
1405  *
1406  * @IEEE80211_HW_2GHZ_SHORT_PREAMBLE_INCAPABLE:
1407  *      Hardware is not capable of receiving frames with short preamble on
1408  *      the 2.4 GHz band.
1409  *
1410  * @IEEE80211_HW_SIGNAL_UNSPEC:
1411  *      Hardware can provide signal values but we don't know its units. We
1412  *      expect values between 0 and @max_signal.
1413  *      If possible please provide dB or dBm instead.
1414  *
1415  * @IEEE80211_HW_SIGNAL_DBM:
1416  *      Hardware gives signal values in dBm, decibel difference from
1417  *      one milliwatt. This is the preferred method since it is standardized
1418  *      between different devices. @max_signal does not need to be set.
1419  *
1420  * @IEEE80211_HW_SPECTRUM_MGMT:
1421  *      Hardware supports spectrum management defined in 802.11h
1422  *      Measurement, Channel Switch, Quieting, TPC
1423  *
1424  * @IEEE80211_HW_AMPDU_AGGREGATION:
1425  *      Hardware supports 11n A-MPDU aggregation.
1426  *
1427  * @IEEE80211_HW_SUPPORTS_PS:
1428  *      Hardware has power save support (i.e. can go to sleep).
1429  *
1430  * @IEEE80211_HW_PS_NULLFUNC_STACK:
1431  *      Hardware requires nullfunc frame handling in stack, implies
1432  *      stack support for dynamic PS.
1433  *
1434  * @IEEE80211_HW_SUPPORTS_DYNAMIC_PS:
1435  *      Hardware has support for dynamic PS.
1436  *
1437  * @IEEE80211_HW_MFP_CAPABLE:
1438  *      Hardware supports management frame protection (MFP, IEEE 802.11w).
1439  *
1440  * @IEEE80211_HW_SUPPORTS_STATIC_SMPS:
1441  *      Hardware supports static spatial multiplexing powersave,
1442  *      ie. can turn off all but one chain even on HT connections
1443  *      that should be using more chains.
1444  *
1445  * @IEEE80211_HW_SUPPORTS_DYNAMIC_SMPS:
1446  *      Hardware supports dynamic spatial multiplexing powersave,
1447  *      ie. can turn off all but one chain and then wake the rest
1448  *      up as required after, for example, rts/cts handshake.
1449  *
1450  * @IEEE80211_HW_SUPPORTS_UAPSD:
1451  *      Hardware supports Unscheduled Automatic Power Save Delivery
1452  *      (U-APSD) in managed mode. The mode is configured with
1453  *      conf_tx() operation.
1454  *
1455  * @IEEE80211_HW_REPORTS_TX_ACK_STATUS:
1456  *      Hardware can provide ack status reports of Tx frames to
1457  *      the stack.
1458  *
1459  * @IEEE80211_HW_CONNECTION_MONITOR:
1460  *      The hardware performs its own connection monitoring, including
1461  *      periodic keep-alives to the AP and probing the AP on beacon loss.
1462  *      When this flag is set, signaling beacon-loss will cause an immediate
1463  *      change to disassociated state.
1464  *
1465  * @IEEE80211_HW_NEED_DTIM_BEFORE_ASSOC:
1466  *      This device needs to get data from beacon before association (i.e.
1467  *      dtim_period).
1468  *
1469  * @IEEE80211_HW_SUPPORTS_PER_STA_GTK: The device's crypto engine supports
1470  *      per-station GTKs as used by IBSS RSN or during fast transition. If
1471  *      the device doesn't support per-station GTKs, but can be asked not
1472  *      to decrypt group addressed frames, then IBSS RSN support is still
1473  *      possible but software crypto will be used. Advertise the wiphy flag
1474  *      only in that case.
1475  *
1476  * @IEEE80211_HW_AP_LINK_PS: When operating in AP mode the device
1477  *      autonomously manages the PS status of connected stations. When
1478  *      this flag is set mac80211 will not trigger PS mode for connected
1479  *      stations based on the PM bit of incoming frames.
1480  *      Use ieee80211_start_ps()/ieee8021_end_ps() to manually configure
1481  *      the PS mode of connected stations.
1482  *
1483  * @IEEE80211_HW_TX_AMPDU_SETUP_IN_HW: The device handles TX A-MPDU session
1484  *      setup strictly in HW. mac80211 should not attempt to do this in
1485  *      software.
1486  *
1487  * @IEEE80211_HW_WANT_MONITOR_VIF: The driver would like to be informed of
1488  *      a virtual monitor interface when monitor interfaces are the only
1489  *      active interfaces.
1490  *
1491  * @IEEE80211_HW_QUEUE_CONTROL: The driver wants to control per-interface
1492  *      queue mapping in order to use different queues (not just one per AC)
1493  *      for different virtual interfaces. See the doc section on HW queue
1494  *      control for more details.
1495  *
1496  * @IEEE80211_HW_SUPPORTS_RC_TABLE: The driver supports using a rate
1497  *      selection table provided by the rate control algorithm.
1498  *
1499  * @IEEE80211_HW_P2P_DEV_ADDR_FOR_INTF: Use the P2P Device address for any
1500  *      P2P Interface. This will be honoured even if more than one interface
1501  *      is supported.
1502  *
1503  * @IEEE80211_HW_TIMING_BEACON_ONLY: Use sync timing from beacon frames
1504  *      only, to allow getting TBTT of a DTIM beacon.
1505  *
1506  * @IEEE80211_HW_SUPPORTS_HT_CCK_RATES: Hardware supports mixing HT/CCK rates
1507  *      and can cope with CCK rates in an aggregation session (e.g. by not
1508  *      using aggregation for such frames.)
1509  *
1510  * @IEEE80211_HW_CHANCTX_STA_CSA: Support 802.11h based channel-switch (CSA)
1511  *      for a single active channel while using channel contexts. When support
1512  *      is not enabled the default action is to disconnect when getting the
1513  *      CSA frame.
1514  */
enum ieee80211_hw_flags {
	         IEEE80211_HW_HAS_RATE_CONTROL                   = 1<<0,
	         IEEE80211_HW_RX_INCLUDES_FCS                    = 1<<1,
	         IEEE80211_HW_HOST_BROADCAST_PS_BUFFERING        = 1<<2,
	         IEEE80211_HW_2GHZ_SHORT_SLOT_INCAPABLE          = 1<<3,
	         IEEE80211_HW_2GHZ_SHORT_PREAMBLE_INCAPABLE      = 1<<4,
	         IEEE80211_HW_SIGNAL_UNSPEC                      = 1<<5,
	         IEEE80211_HW_SIGNAL_DBM                         = 1<<6,
	         IEEE80211_HW_NEED_DTIM_BEFORE_ASSOC             = 1<<7,
	         IEEE80211_HW_SPECTRUM_MGMT                      = 1<<8,
	         IEEE80211_HW_AMPDU_AGGREGATION                  = 1<<9,
	         IEEE80211_HW_SUPPORTS_PS                        = 1<<10,
	         IEEE80211_HW_PS_NULLFUNC_STACK                  = 1<<11,
	         IEEE80211_HW_SUPPORTS_DYNAMIC_PS                = 1<<12,
	         IEEE80211_HW_MFP_CAPABLE                        = 1<<13,
	         IEEE80211_HW_WANT_MONITOR_VIF                   = 1<<14,
	         IEEE80211_HW_SUPPORTS_STATIC_SMPS               = 1<<15,
	         IEEE80211_HW_SUPPORTS_DYNAMIC_SMPS              = 1<<16,
	         IEEE80211_HW_SUPPORTS_UAPSD                     = 1<<17,
	         IEEE80211_HW_REPORTS_TX_ACK_STATUS              = 1<<18,
	         IEEE80211_HW_CONNECTION_MONITOR                 = 1<<19,
	         IEEE80211_HW_QUEUE_CONTROL                      = 1<<20,
	         IEEE80211_HW_SUPPORTS_PER_STA_GTK               = 1<<21,
	         IEEE80211_HW_AP_LINK_PS                         = 1<<22,
	         IEEE80211_HW_TX_AMPDU_SETUP_IN_HW               = 1<<23,
	         IEEE80211_HW_SUPPORTS_RC_TABLE                  = 1<<24,
	         IEEE80211_HW_P2P_DEV_ADDR_FOR_INTF              = 1<<25,
	         IEEE80211_HW_TIMING_BEACON_ONLY                 = 1<<26,
	         IEEE80211_HW_SUPPORTS_HT_CCK_RATES              = 1<<27,
	         IEEE80211_HW_CHANCTX_STA_CSA                    = 1<<28,
};

/**
1548  * struct ieee80211_hw - hardware information and state
1549  *
1550  * This structure contains the configuration and hardware
1551  * information for an 802.11 PHY.
1552  *
1553  * @wiphy: This points to the &struct wiphy allocated for this
1554  *      802.11 PHY. You must fill in the @perm_addr and @dev
1555  *      members of this structure using SET_IEEE80211_DEV()
1556  *      and SET_IEEE80211_PERM_ADDR(). Additionally, all supported
1557  *      bands (with channels, bitrates) are registered here.
1558  *
1559  * @conf: &struct ieee80211_conf, device configuration, don't use.
1560  *
1561  * @priv: pointer to private area that was allocated for driver use
1562  *      along with this structure.
1563  *
1564  * @flags: hardware flags, see &enum ieee80211_hw_flags.
1565  *
1566  * @extra_tx_headroom: headroom to reserve in each transmit skb
1567  *      for use by the driver (e.g. for transmit headers.)
1568  *
1569  * @channel_change_time: time (in microseconds) it takes to change channels.
1570  *
1571  * @max_signal: Maximum value for signal (rssi) in RX information, used
1572  *      only when @IEEE80211_HW_SIGNAL_UNSPEC or @IEEE80211_HW_SIGNAL_DB
1573  *
1574  * @max_listen_interval: max listen interval in units of beacon interval
1575  *      that HW supports
1576  *
1577  * @queues: number of available hardware transmit queues for
1578  *      data packets. WMM/QoS requires at least four, these
1579  *      queues need to have configurable access parameters.
1580  *
1581  * @rate_control_algorithm: rate control algorithm for this hardware.
1582  *      If unset (NULL), the default algorithm will be used. Must be
1583  *      set before calling ieee80211_register_hw().
1584  *
1585  * @vif_data_size: size (in bytes) of the drv_priv data area
1586  *      within &struct ieee80211_vif.
1587  * @sta_data_size: size (in bytes) of the drv_priv data area
1588  *      within &struct ieee80211_sta.
1589  * @chanctx_data_size: size (in bytes) of the drv_priv data area
1590  *      within &struct ieee80211_chanctx_conf.
1591  *
1592  * @max_rates: maximum number of alternate rate retry stages the hw
1593  *      can handle.
1594  * @max_report_rates: maximum number of alternate rate retry stages
1595  *      the hw can report back.
1596  * @max_rate_tries: maximum number of tries for each stage
1597  *
1598  * @napi_weight: weight used for NAPI polling.  You must specify an
1599  *      appropriate value here if a napi_poll operation is provided
1600  *      by your driver.
1601  *
1602  * @max_rx_aggregation_subframes: maximum buffer size (number of
1603  *      sub-frames) to be used for A-MPDU block ack receiver
1604  *      aggregation.
1605  *      This is only relevant if the device has restrictions on the
1606  *      number of subframes, if it relies on mac80211 to do reordering
1607  *      it shouldn't be set.
1608  *
1609  * @max_tx_aggregation_subframes: maximum number of subframes in an
1610  *      aggregate an HT driver will transmit, used by the peer as a
1611  *      hint to size its reorder buffer.
1612  *
1613  * @offchannel_tx_hw_queue: HW queue ID to use for offchannel TX
1614  *      (if %IEEE80211_HW_QUEUE_CONTROL is set)
1615  *
1616  * @radiotap_mcs_details: lists which MCS information can the HW
1617  *      reports, by default it is set to _MCS, _GI and _BW but doesn't
1618  *      include _FMT. Use %IEEE80211_RADIOTAP_MCS_HAVE_* values, only
1619  *      adding _BW is supported today.
1620  *
1621  * @radiotap_vht_details: lists which VHT MCS information the HW reports,
1622  *      the default is _GI | _BANDWIDTH.
1623  *      Use the %IEEE80211_RADIOTAP_VHT_KNOWN_* values.
1624  *
1625  * @netdev_features: netdev features to be set in each netdev created
1626  *      from this HW. Note only HW checksum features are currently
1627  *      compatible with mac80211. Other feature bits will be rejected.
1628  *
1629  * @uapsd_queues: This bitmap is included in (re)association frame to indicate
1630  *      for each access category if it is uAPSD trigger-enabled and delivery-
1631  *      enabled. Use IEEE80211_WMM_IE_STA_QOSINFO_AC_* to set this bitmap.
1632  *      Each bit corresponds to different AC. Value '1' in specific bit means
1633  *      that corresponding AC is both trigger- and delivery-enabled. '' means
1634  *      neither enabled.
1635  *
1636  * @uapsd_max_sp_len: maximum number of total buffered frames the WMM AP may
1637  *      deliver to a WMM STA during any Service Period triggered by the WMM STA.
1638  *      Use IEEE80211_WMM_IE_STA_QOSINFO_SP_* for correct values.
1639  */
struct ieee80211_hw {
	struct ieee80211_conf conf;
	struct wiphy *wiphy;
	const char *rate_control_algorithm;
	void *priv;
	uint32_t flags;
	unsigned int extra_tx_headroom;
	int channel_change_time;
	int vif_data_size;
	int sta_data_size;
	int chanctx_data_size;
	int napi_weight;
	uint16_t queues;
	uint16_t max_listen_interval;
	int8_t max_signal;
	uint8_t max_rates;
	uint8_t max_report_rates;
	uint8_t max_rate_tries;
	uint8_t max_rx_aggregation_subframes;
	uint8_t max_tx_aggregation_subframes;
	uint8_t offchannel_tx_hw_queue;
	uint8_t radiotap_mcs_details;
	uint16_t radiotap_vht_details;
	netdev_features_t netdev_features;
	uint8_t uapsd_queues;
	uint8_t uapsd_max_sp_len;
};

/**
1669  * wiphy_to_ieee80211_hw - return a mac80211 driver hw struct from a wiphy
1670  *
1671  * @wiphy: the &struct wiphy which we want to query
1672  *
1673  * mac80211 drivers can use this to get to their respective
1674  * &struct ieee80211_hw. Drivers wishing to get to their own private
1675  * structure can then access it via hw->priv. Note that mac802111 drivers should
1676  * not use wiphy_priv() to try to get their private driver structure as this
1677  * is already used internally by mac80211.
1678  *
1679  * Return: The mac80211 driver hw struct of @wiphy.
1680  */
struct ieee80211_hw *wiphy_to_ieee80211_hw(struct wiphy *wiphy);

/**
1684  * SET_IEEE80211_DEV - set device for 802.11 hardware
1685  *
1686  * @hw: the &struct ieee80211_hw to set the device for
1687  * @dev: the &struct device of this 802.11 device
1688  */
static inline void SET_IEEE80211_DEV(struct ieee80211_hw *hw, struct device *dev)
{
	         //set_wiphy_dev(hw->wiphy, dev);
}

/**
1695  * SET_IEEE80211_PERM_ADDR - set the permanent MAC address for 802.11 hardware
1696  *
1697  * @hw: the &struct ieee80211_hw to set the MAC address for
1698  * @addr: the address to set
1699  */
static inline void SET_IEEE80211_PERM_ADDR(struct ieee80211_hw *hw, uint8_t *addr)
{
	         memcpy(hw->wiphy->perm_addr, addr, ETH_ALEN);
}

static inline struct ieee80211_rate *
ieee80211_get_tx_rate(const struct ieee80211_hw *hw,
                       const struct ieee80211_tx_info *c)
{
	         if (c->control.rates[0].idx < 0) {
				printf("index of control rate negative\n");
			    return NULL;
			 }
	         return &hw->wiphy->bands[c->band]->bitrates[c->control.rates[0].idx];
}

static inline struct ieee80211_rate *
ieee80211_get_rts_cts_rate(const struct ieee80211_hw *hw,
                            const struct ieee80211_tx_info *c)
{
	         if (c->control.rts_cts_rate_idx < 0)
	                 return NULL;
	         return &hw->wiphy->bands[c->band]->bitrates[c->control.rts_cts_rate_idx];
}

static inline struct ieee80211_rate *
ieee80211_get_alt_retry_rate(const struct ieee80211_hw *hw,
                              const struct ieee80211_tx_info *c, int idx)
{
	         if (c->control.rates[idx + 1].idx < 0)
	                 return NULL;
	         return &hw->wiphy->bands[c->band]->bitrates[c->control.rates[idx + 1].idx];
}

/**
1733  * ieee80211_free_txskb - free TX skb
1734  * @hw: the hardware
1735  * @skb: the skb
1736  *
1737  * Free a transmit skb. Use this funtion when some failure
1738  * to transmit happened and thus status cannot be reported.
1739  */
void ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb);

/**
1743  * DOC: Hardware crypto acceleration
1744  *
1745  * mac80211 is capable of taking advantage of many hardware
1746  * acceleration designs for encryption and decryption operations.
1747  *
1748  * The set_key() callback in the &struct ieee80211_ops for a given
1749  * device is called to enable hardware acceleration of encryption and
1750  * decryption. The callback takes a @sta parameter that will be NULL
1751  * for default keys or keys used for transmission only, or point to
1752  * the station information for the peer for individual keys.
1753  * Multiple transmission keys with the same key index may be used when
1754  * VLANs are configured for an access point.
1755  *
1756  * When transmitting, the TX control data will use the @hw_key_idx
1757  * selected by the driver by modifying the &struct ieee80211_key_conf
1758  * pointed to by the @key parameter to the set_key() function.
1759  *
1760  * The set_key() call for the %SET_KEY command should return 0 if
1761  * the key is now in use, -%EOPNOTSUPP or -%ENOSPC if it couldn't be
1762  * added; if you return 0 then hw_key_idx must be assigned to the
1763  * hardware key index, you are free to use the full u8 range.
1764  *
1765  * When the cmd is %DISABLE_KEY then it must succeed.
1766  *
1767  * Note that it is permissible to not decrypt a frame even if a key
1768  * for it has been uploaded to hardware, the stack will not make any
1769  * decision based on whether a key has been uploaded or not but rather
1770  * based on the receive flags.
1771  *
1772  * The &struct ieee80211_key_conf structure pointed to by the @key
1773  * parameter is guaranteed to be valid until another call to set_key()
1774  * removes it, but it can only be used as a cookie to differentiate
1775  * keys.
1776  *
1777  * In TKIP some HW need to be provided a phase 1 key, for RX decryption
1778  * acceleration (i.e. iwlwifi). Those drivers should provide update_tkip_key
1779  * handler.
1780  * The update_tkip_key() call updates the driver with the new phase 1 key.
1781  * This happens every time the iv16 wraps around (every 65536 packets). The
1782  * set_key() call will happen only once for each key (unless the AP did
1783  * rekeying), it will not include a valid phase 1 key. The valid phase 1 key is
1784  * provided by update_tkip_key only. The trigger that makes mac80211 call this
1785  * handler is software decryption with wrap around of iv16.
1786  *
1787  * The set_default_unicast_key() call updates the default WEP key index
1788  * configured to the hardware for WEP encryption type. This is required
1789  * for devices that support offload of data packets (e.g. ARP responses).
1790  */

/**
1793  * DOC: Powersave support
1794  *
1795  * mac80211 has support for various powersave implementations.
1796  *
1797  * First, it can support hardware that handles all powersaving by itself,
1798  * such hardware should simply set the %IEEE80211_HW_SUPPORTS_PS hardware
1799  * flag. In that case, it will be told about the desired powersave mode
1800  * with the %IEEE80211_CONF_PS flag depending on the association status.
1801  * The hardware must take care of sending nullfunc frames when necessary,
1802  * i.e. when entering and leaving powersave mode. The hardware is required
1803  * to look at the AID in beacons and signal to the AP that it woke up when
1804  * it finds traffic directed to it.
1805  *
1806  * %IEEE80211_CONF_PS flag enabled means that the powersave mode defined in
1807  * IEEE 802.11-2007 section 11.2 is enabled. This is not to be confused
1808  * with hardware wakeup and sleep states. Driver is responsible for waking
1809  * up the hardware before issuing commands to the hardware and putting it
1810  * back to sleep at appropriate times.
1811  *
1812  * When PS is enabled, hardware needs to wakeup for beacons and receive the
1813  * buffered multicast/broadcast frames after the beacon. Also it must be
1814  * possible to send frames and receive the acknowledment frame.
1815  *
1816  * Other hardware designs cannot send nullfunc frames by themselves and also
1817  * need software support for parsing the TIM bitmap. This is also supported
1818  * by mac80211 by combining the %IEEE80211_HW_SUPPORTS_PS and
1819  * %IEEE80211_HW_PS_NULLFUNC_STACK flags. The hardware is of course still
1820  * required to pass up beacons. The hardware is still required to handle
1821  * waking up for multicast traffic; if it cannot the driver must handle that
1822  * as best as it can, mac80211 is too slow to do that.
1823  *
1824  * Dynamic powersave is an extension to normal powersave in which the
1825  * hardware stays awake for a user-specified period of time after sending a
1826  * frame so that reply frames need not be buffered and therefore delayed to
1827  * the next wakeup. It's compromise of getting good enough latency when
1828  * there's data traffic and still saving significantly power in idle
1829  * periods.
1830  *
1831  * Dynamic powersave is simply supported by mac80211 enabling and disabling
1832  * PS based on traffic. Driver needs to only set %IEEE80211_HW_SUPPORTS_PS
1833  * flag and mac80211 will handle everything automatically. Additionally,
1834  * hardware having support for the dynamic PS feature may set the
1835  * %IEEE80211_HW_SUPPORTS_DYNAMIC_PS flag to indicate that it can support
1836  * dynamic PS mode itself. The driver needs to look at the
1837  * @dynamic_ps_timeout hardware configuration value and use it that value
1838  * whenever %IEEE80211_CONF_PS is set. In this case mac80211 will disable
1839  * dynamic PS feature in stack and will just keep %IEEE80211_CONF_PS
1840  * enabled whenever user has enabled powersave.
1841  *
1842  * Driver informs U-APSD client support by enabling
1843  * %IEEE80211_HW_SUPPORTS_UAPSD flag. The mode is configured through the
1844  * uapsd paramater in conf_tx() operation. Hardware needs to send the QoS
1845  * Nullfunc frames and stay awake until the service period has ended. To
1846  * utilize U-APSD, dynamic powersave is disabled for voip AC and all frames
1847  * from that AC are transmitted with powersave enabled.
1848  *
1849  * Note: U-APSD client mode is not yet supported with
1850  * %IEEE80211_HW_PS_NULLFUNC_STACK.
1851  */

/**
1854  * DOC: Beacon filter support
1855  *
1856  * Some hardware have beacon filter support to reduce host cpu wakeups
1857  * which will reduce system power consumption. It usually works so that
1858  * the firmware creates a checksum of the beacon but omits all constantly
1859  * changing elements (TSF, TIM etc). Whenever the checksum changes the
1860  * beacon is forwarded to the host, otherwise it will be just dropped. That
1861  * way the host will only receive beacons where some relevant information
1862  * (for example ERP protection or WMM settings) have changed.
1863  *
1864  * Beacon filter support is advertised with the %IEEE80211_VIF_BEACON_FILTER
1865  * interface capability. The driver needs to enable beacon filter support
1866  * whenever power save is enabled, that is %IEEE80211_CONF_PS is set. When
1867  * power save is enabled, the stack will not check for beacon loss and the
1868  * driver needs to notify about loss of beacons with ieee80211_beacon_loss().
1869  *
1870  * The time (or number of beacons missed) until the firmware notifies the
1871  * driver of a beacon loss event (which in turn causes the driver to call
1872  * ieee80211_beacon_loss()) should be configurable and will be controlled
1873  * by mac80211 and the roaming algorithm in the future.
1874  *
1875  * Since there may be constantly changing information elements that nothing
1876  * in the software stack cares about, we will, in the future, have mac80211
1877  * tell the driver which information elements are interesting in the sense
1878  * that we want to see changes in them. This will include
1879  *  - a list of information element IDs
1880  *  - a list of OUIs for the vendor information element
1881  *
1882  * Ideally, the hardware would filter out any beacons without changes in the
1883  * requested elements, but if it cannot support that it may, at the expense
1884  * of some efficiency, filter out only a subset. For example, if the device
1885  * doesn't support checking for OUIs it should pass up all changes in all
1886  * vendor information elements.
1887  *
1888  * Note that change, for the sake of simplification, also includes information
1889  * elements appearing or disappearing from the beacon.
1890  *
1891  * Some hardware supports an "ignore list" instead, just make sure nothing
1892  * that was requested is on the ignore list, and include commonly changing
1893  * information element IDs in the ignore list, for example 11 (BSS load) and
1894  * the various vendor-assigned IEs with unknown contents (128, 129, 133-136,
1895  * 149, 150, 155, 156, 173, 176, 178, 179, 219); for forward compatibility
1896  * it could also include some currently unused IDs.
1897  *
1898  *
1899  * In addition to these capabilities, hardware should support notifying the
1900  * host of changes in the beacon RSSI. This is relevant to implement roaming
1901  * when no traffic is flowing (when traffic is flowing we see the RSSI of
1902  * the received data packets). This can consist in notifying the host when
1903  * the RSSI changes significantly or when it drops below or rises above
1904  * configurable thresholds. In the future these thresholds will also be
1905  * configured by mac80211 (which gets them from userspace) to implement
1906  * them as the roaming algorithm requires.
1907  *
1908  * If the hardware cannot implement this, the driver should ask it to
1909  * periodically pass beacon frames to the host so that software can do the
1910  * signal strength threshold checking.
1911  */

/**
1914  * DOC: Spatial multiplexing power save
1915  *
1916  * SMPS (Spatial multiplexing power save) is a mechanism to conserve
1917  * power in an 802.11n implementation. For details on the mechanism
1918  * and rationale, please refer to 802.11 (as amended by 802.11n-2009)
1919  * "11.2.3 SM power save".
1920  *
1921  * The mac80211 implementation is capable of sending action frames
1922  * to update the AP about the station's SMPS mode, and will instruct
1923  * the driver to enter the specific mode. It will also announce the
1924  * requested SMPS mode during the association handshake. Hardware
1925  * support for this feature is required, and can be indicated by
1926  * hardware flags.
1927  *
1928  * The default mode will be "automatic", which nl80211/cfg80211
1929  * defines to be dynamic SMPS in (regular) powersave, and SMPS
1930  * turned off otherwise.
1931  *
1932  * To support this feature, the driver must set the appropriate
1933  * hardware support flags, and handle the SMPS flag to the config()
1934  * operation. It will then with this mechanism be instructed to
1935  * enter the requested SMPS mode while associated to an HT AP.
1936  */

/**
1939  * DOC: Frame filtering
1940  *
1941  * mac80211 requires to see many management frames for proper
1942  * operation, and users may want to see many more frames when
1943  * in monitor mode. However, for best CPU usage and power consumption,
1944  * having as few frames as possible percolate through the stack is
1945  * desirable. Hence, the hardware should filter as much as possible.
1946  *
1947  * To achieve this, mac80211 uses filter flags (see below) to tell
1948  * the driver's configure_filter() function which frames should be
1949  * passed to mac80211 and which should be filtered out.
1950  *
1951  * Before configure_filter() is invoked, the prepare_multicast()
1952  * callback is invoked with the parameters @mc_count and @mc_list
1953  * for the combined multicast address list of all virtual interfaces.
1954  * It's use is optional, and it returns a u64 that is passed to
1955  * configure_filter(). Additionally, configure_filter() has the
1956  * arguments @changed_flags telling which flags were changed and
1957  * @total_flags with the new flag states.
1958  *
1959  * If your device has no multicast address filters your driver will
1960  * need to check both the %FIF_ALLMULTI flag and the @mc_count
1961  * parameter to see whether multicast frames should be accepted
1962  * or dropped.
1963  *
1964  * All unsupported flags in @total_flags must be cleared.
1965  * Hardware does not support a flag if it is incapable of _passing_
1966  * the frame to the stack. Otherwise the driver must ignore
1967  * the flag, but not clear it.
1968  * You must _only_ clear the flag (announce no support for the
1969  * flag to mac80211) if you are not able to pass the packet type
1970  * to the stack (so the hardware always filters it).
1971  * So for example, you should clear @FIF_CONTROL, if your hardware
1972  * always filters control frames. If your hardware always passes
1973  * control frames to the kernel and is incapable of filtering them,
1974  * you do _not_ clear the @FIF_CONTROL flag.
1975  * This rule applies to all other FIF flags as well.
1976  */

/**
1979  * DOC: AP support for powersaving clients
1980  *
1981  * In order to implement AP and P2P GO modes, mac80211 has support for
1982  * client powersaving, both "legacy" PS (PS-Poll/null data) and uAPSD.
1983  * There currently is no support for sAPSD.
1984  *
1985  * There is one assumption that mac80211 makes, namely that a client
1986  * will not poll with PS-Poll and trigger with uAPSD at the same time.
1987  * Both are supported, and both can be used by the same client, but
1988  * they can't be used concurrently by the same client. This simplifies
1989  * the driver code.
1990  *
1991  * The first thing to keep in mind is that there is a flag for complete
1992  * driver implementation: %IEEE80211_HW_AP_LINK_PS. If this flag is set,
1993  * mac80211 expects the driver to handle most of the state machine for
1994  * powersaving clients and will ignore the PM bit in incoming frames.
1995  * Drivers then use ieee80211_sta_ps_transition() to inform mac80211 of
1996  * stations' powersave transitions. In this mode, mac80211 also doesn't
1997  * handle PS-Poll/uAPSD.
1998  *
1999  * In the mode without %IEEE80211_HW_AP_LINK_PS, mac80211 will check the
2000  * PM bit in incoming frames for client powersave transitions. When a
2001  * station goes to sleep, we will stop transmitting to it. There is,
2002  * however, a race condition: a station might go to sleep while there is
2003  * data buffered on hardware queues. If the device has support for this
2004  * it will reject frames, and the driver should give the frames back to
2005  * mac80211 with the %IEEE80211_TX_STAT_TX_FILTERED flag set which will
2006  * cause mac80211 to retry the frame when the station wakes up. The
2007  * driver is also notified of powersave transitions by calling its
2008  * @sta_notify callback.
2009  *
2010  * When the station is asleep, it has three choices: it can wake up,
2011  * it can PS-Poll, or it can possibly start a uAPSD service period.
2012  * Waking up is implemented by simply transmitting all buffered (and
2013  * filtered) frames to the station. This is the easiest case. When
2014  * the station sends a PS-Poll or a uAPSD trigger frame, mac80211
2015  * will inform the driver of this with the @allow_buffered_frames
2016  * callback; this callback is optional. mac80211 will then transmit
2017  * the frames as usual and set the %IEEE80211_TX_CTL_NO_PS_BUFFER
2018  * on each frame. The last frame in the service period (or the only
2019  * response to a PS-Poll) also has %IEEE80211_TX_STATUS_EOSP set to
2020  * indicate that it ends the service period; as this frame must have
2021  * TX status report it also sets %IEEE80211_TX_CTL_REQ_TX_STATUS.
2022  * When TX status is reported for this frame, the service period is
2023  * marked has having ended and a new one can be started by the peer.
2024  *
2025  * Additionally, non-bufferable MMPDUs can also be transmitted by
2026  * mac80211 with the %IEEE80211_TX_CTL_NO_PS_BUFFER set in them.
2027  *
2028  * Another race condition can happen on some devices like iwlwifi
2029  * when there are frames queued for the station and it wakes up
2030  * or polls; the frames that are already queued could end up being
2031  * transmitted first instead, causing reordering and/or wrong
2032  * processing of the EOSP. The cause is that allowing frames to be
2033  * transmitted to a certain station is out-of-band communication to
2034  * the device. To allow this problem to be solved, the driver can
2035  * call ieee80211_sta_block_awake() if frames are buffered when it
2036  * is notified that the station went to sleep. When all these frames
2037  * have been filtered (see above), it must call the function again
2038  * to indicate that the station is no longer blocked.
2039  *
2040  * If the driver buffers frames in the driver for aggregation in any
2041  * way, it must use the ieee80211_sta_set_buffered() call when it is
2042  * notified of the station going to sleep to inform mac80211 of any
2043  * TIDs that have frames buffered. Note that when a station wakes up
2044  * this information is reset (hence the requirement to call it when
2045  * informed of the station going to sleep). Then, when a service
2046  * period starts for any reason, @release_buffered_frames is called
2047  * with the number of frames to be released and which TIDs they are
2048  * to come from. In this case, the driver is responsible for setting
2049  * the EOSP (for uAPSD) and MORE_DATA bits in the released frames,
2050  * to help the @more_data paramter is passed to tell the driver if
2051  * there is more data on other TIDs -- the TIDs to release frames
2052  * from are ignored since mac80211 doesn't know how many frames the
2053  * buffers for those TIDs contain.
2054  *
2055  * If the driver also implement GO mode, where absence periods may
2056  * shorten service periods (or abort PS-Poll responses), it must
2057  * filter those response frames except in the case of frames that
2058  * are buffered in the driver -- those must remain buffered to avoid
2059  * reordering. Because it is possible that no frames are released
2060  * in this case, the driver must call ieee80211_sta_eosp()
2061  * to indicate to mac80211 that the service period ended anyway.
2062  *
2063  * Finally, if frames from multiple TIDs are released from mac80211
2064  * but the driver might reorder them, it must clear & set the flags
2065  * appropriately (only the last frame may have %IEEE80211_TX_STATUS_EOSP)
2066  * and also take care of the EOSP and MORE_DATA bits in the frame.
2067  * The driver may also use ieee80211_sta_eosp() in this case.
2068  */

/**
2071  * DOC: HW queue control
2072  *
2073  * Before HW queue control was introduced, mac80211 only had a single static
2074  * assignment of per-interface AC software queues to hardware queues. This
2075  * was problematic for a few reasons:
2076  * 1) off-channel transmissions might get stuck behind other frames
2077  * 2) multiple virtual interfaces couldn't be handled correctly
2078  * 3) after-DTIM frames could get stuck behind other frames
2079  *
2080  * To solve this, hardware typically uses multiple different queues for all
2081  * the different usages, and this needs to be propagated into mac80211 so it
2082  * won't have the same problem with the software queues.
2083  *
2084  * Therefore, mac80211 now offers the %IEEE80211_HW_QUEUE_CONTROL capability
2085  * flag that tells it that the driver implements its own queue control. To do
2086  * so, the driver will set up the various queues in each &struct ieee80211_vif
2087  * and the offchannel queue in &struct ieee80211_hw. In response, mac80211 will
2088  * use those queue IDs in the hw_queue field of &struct ieee80211_tx_info and
2089  * if necessary will queue the frame on the right software queue that mirrors
2090  * the hardware queue.
2091  * Additionally, the driver has to then use these HW queue IDs for the queue
2092  * management functions (ieee80211_stop_queue() et al.)
2093  *
2094  * The driver is free to set up the queue mappings as needed, multiple virtual
2095  * interfaces may map to the same hardware queues if needed. The setup has to
2096  * happen during add_interface or change_interface callbacks. For example, a
2097  * driver supporting station+station and station+AP modes might decide to have
2098  * 10 hardware queues to handle different scenarios:
2099  *
2100  * 4 AC HW queues for 1st vif: 0, 1, 2, 3
2101  * 4 AC HW queues for 2nd vif: 4, 5, 6, 7
2102  * after-DTIM queue for AP:   8
2103  * off-channel queue:         9
2104  *
2105  * It would then set up the hardware like this:
2106  *   hw.offchannel_tx_hw_queue = 9
2107  *
2108  * and the first virtual interface that is added as follows:
2109  *   vif.hw_queue[IEEE80211_AC_VO] = 0
2110  *   vif.hw_queue[IEEE80211_AC_VI] = 1
2111  *   vif.hw_queue[IEEE80211_AC_BE] = 2
2112  *   vif.hw_queue[IEEE80211_AC_BK] = 3
2113  *   vif.cab_queue = 8 // if AP mode, otherwise %IEEE80211_INVAL_HW_QUEUE
2114  * and the second virtual interface with 4-7.
2115  *
2116  * If queue 6 gets full, for example, mac80211 would only stop the second
2117  * virtual interface's BE queue since virtual interface queues are per AC.
2118  *
2119  * Note that the vif.cab_queue value should be set to %IEEE80211_INVAL_HW_QUEUE
2120  * whenever the queue is not used (i.e. the interface is not in AP mode) if the
2121  * queue could potentially be shared since mac80211 will look at cab_queue when
2122  * a queue is stopped/woken even if the interface is not in AP mode.
2123  */

/**
2126  * enum ieee80211_filter_flags - hardware filter flags
2127  *
2128  * These flags determine what the filter in hardware should be
2129  * programmed to let through and what should not be passed to the
2130  * stack. It is always safe to pass more frames than requested,
2131  * but this has negative impact on power consumption.
2132  *
2133  * @FIF_PROMISC_IN_BSS: promiscuous mode within your BSS,
2134  *      think of the BSS as your network segment and then this corresponds
2135  *      to the regular ethernet device promiscuous mode.
2136  *
2137  * @FIF_ALLMULTI: pass all multicast frames, this is used if requested
2138  *      by the user or if the hardware is not capable of filtering by
2139  *      multicast address.
2140  *
2141  * @FIF_FCSFAIL: pass frames with failed FCS (but you need to set the
2142  *      %RX_FLAG_FAILED_FCS_CRC for them)
2143  *
2144  * @FIF_PLCPFAIL: pass frames with failed PLCP CRC (but you need to set
2145  *      the %RX_FLAG_FAILED_PLCP_CRC for them
2146  *
2147  * @FIF_BCN_PRBRESP_PROMISC: This flag is set during scanning to indicate
2148  *      to the hardware that it should not filter beacons or probe responses
2149  *      by BSSID. Filtering them can greatly reduce the amount of processing
2150  *      mac80211 needs to do and the amount of CPU wakeups, so you should
2151  *      honour this flag if possible.
2152  *
2153  * @FIF_CONTROL: pass control frames (except for PS Poll), if PROMISC_IN_BSS
2154  *      is not set then only those addressed to this station.
2155  *
2156  * @FIF_OTHER_BSS: pass frames destined to other BSSes
2157  *
2158  * @FIF_PSPOLL: pass PS Poll frames, if PROMISC_IN_BSS is not set then only
2159  *      those addressed to this station.
2160  *
2161  * @FIF_PROBE_REQ: pass probe request frames
2162  */
enum ieee80211_filter_flags {
	         FIF_PROMISC_IN_BSS      = 1<<0,
	         FIF_ALLMULTI            = 1<<1,
	         FIF_FCSFAIL             = 1<<2,
	         FIF_PLCPFAIL            = 1<<3,
	         FIF_BCN_PRBRESP_PROMISC = 1<<4,
	         FIF_CONTROL             = 1<<5,
	         FIF_OTHER_BSS           = 1<<6,
	         FIF_PSPOLL              = 1<<7,
	         FIF_PROBE_REQ           = 1<<8,
};

/**
2176  * enum ieee80211_ampdu_mlme_action - A-MPDU actions
2177  *
2178  * These flags are used with the ampdu_action() callback in
2179  * &struct ieee80211_ops to indicate which action is needed.
2180  *
2181  * Note that drivers MUST be able to deal with a TX aggregation
2182  * session being stopped even before they OK'ed starting it by
2183  * calling ieee80211_start_tx_ba_cb_irqsafe, because the peer
2184  * might receive the addBA frame and send a delBA right away!
2185  *
2186  * @IEEE80211_AMPDU_RX_START: start RX aggregation
2187  * @IEEE80211_AMPDU_RX_STOP: stop RX aggregation
2188  * @IEEE80211_AMPDU_TX_START: start TX aggregation
2189  * @IEEE80211_AMPDU_TX_OPERATIONAL: TX aggregation has become operational
2190  * @IEEE80211_AMPDU_TX_STOP_CONT: stop TX aggregation but continue transmitting
2191  *      queued packets, now unaggregated. After all packets are transmitted the
2192  *      driver has to call ieee80211_stop_tx_ba_cb_irqsafe().
2193  * @IEEE80211_AMPDU_TX_STOP_FLUSH: stop TX aggregation and flush all packets,
2194  *      called when the station is removed. There's no need or reason to call
2195  *      ieee80211_stop_tx_ba_cb_irqsafe() in this case as mac80211 assumes the
2196  *      session is gone and removes the station.
2197  * @IEEE80211_AMPDU_TX_STOP_FLUSH_CONT: called when TX aggregation is stopped
2198  *      but the driver hasn't called ieee80211_stop_tx_ba_cb_irqsafe() yet and
2199  *      now the connection is dropped and the station will be removed. Drivers
2200  *      should clean up and drop remaining packets when this is called.
2201  */
 enum ieee80211_ampdu_mlme_action {
	         IEEE80211_AMPDU_RX_START,
	         IEEE80211_AMPDU_RX_STOP,
	         IEEE80211_AMPDU_TX_START,
	         IEEE80211_AMPDU_TX_STOP_CONT,
	         IEEE80211_AMPDU_TX_STOP_FLUSH,
	         IEEE80211_AMPDU_TX_STOP_FLUSH_CONT,
	         IEEE80211_AMPDU_TX_OPERATIONAL,
};

/**
2213  * enum ieee80211_frame_release_type - frame release reason
2214  * @IEEE80211_FRAME_RELEASE_PSPOLL: frame released for PS-Poll
2215  * @IEEE80211_FRAME_RELEASE_UAPSD: frame(s) released due to
2216  *      frame received on trigger-enabled AC
2217  */
enum ieee80211_frame_release_type {
	         IEEE80211_FRAME_RELEASE_PSPOLL,
	         IEEE80211_FRAME_RELEASE_UAPSD,
};

/**
2224  * enum ieee80211_rate_control_changed - flags to indicate what changed
2225  *
2226  * @IEEE80211_RC_BW_CHANGED: The bandwidth that can be used to transmit
2227  *      to this station changed. The actual bandwidth is in the station
2228  *      information -- for HT20/40 the IEEE80211_HT_CAP_SUP_WIDTH_20_40
2229  *      flag changes, for HT and VHT the bandwidth field changes.
2230  * @IEEE80211_RC_SMPS_CHANGED: The SMPS state of the station changed.
2231  * @IEEE80211_RC_SUPP_RATES_CHANGED: The supported rate set of this peer
2232  *      changed (in IBSS mode) due to discovering more information about
2233  *      the peer.
2234  * @IEEE80211_RC_NSS_CHANGED: N_SS (number of spatial streams) was changed
2235  *      by the peer
2236  */
enum ieee80211_rate_control_changed {
	         IEEE80211_RC_BW_CHANGED         = BIT(0),
	         IEEE80211_RC_SMPS_CHANGED       = BIT(1),
	         IEEE80211_RC_SUPP_RATES_CHANGED = BIT(2),
	         IEEE80211_RC_NSS_CHANGED        = BIT(3),
};

/**
2245  * enum ieee80211_roc_type - remain on channel type
2246  *
2247  * With the support for multi channel contexts and multi channel operations,
2248  * remain on channel operations might be limited/deferred/aborted by other
2249  * flows/operations which have higher priority (and vise versa).
2250  * Specifying the ROC type can be used by devices to prioritize the ROC
2251  * operations compared to other operations/flows.
2252  *
2253  * @IEEE80211_ROC_TYPE_NORMAL: There are no special requirements for this ROC.
2254  * @IEEE80211_ROC_TYPE_MGMT_TX: The remain on channel request is required
2255  *      for sending managment frames offchannel.
2256  */
enum ieee80211_roc_type {
	         IEEE80211_ROC_TYPE_NORMAL = 0,
	         IEEE80211_ROC_TYPE_MGMT_TX,
};

/**
2263  * struct ieee80211_ops - callbacks from mac80211 to the driver
2264  *
2265  * This structure contains various callbacks that the driver may
2266  * handle or, in some cases, must handle, for example to configure
2267  * the hardware to a new channel or to transmit a frame.
2268  *
2269  * @tx: Handler that 802.11 module calls for each transmitted frame.
2270  *      skb contains the buffer starting from the IEEE 802.11 header.
2271  *      The low-level driver should send the frame out based on
2272  *      configuration in the TX control data. This handler should,
2273  *      preferably, never fail and stop queues appropriately.
2274  *      Must be atomic.
2275  *
2276  * @start: Called before the first netdevice attached to the hardware
2277  *      is enabled. This should turn on the hardware and must turn on
2278  *      frame reception (for possibly enabled monitor interfaces.)
2279  *      Returns negative error codes, these may be seen in userspace,
2280  *      or zero.
2281  *      When the device is started it should not have a MAC address
2282  *      to avoid acknowledging frames before a non-monitor device
2283  *      is added.
2284  *      Must be implemented and can sleep.
2285  *
2286  * @stop: Called after last netdevice attached to the hardware
2287  *      is disabled. This should turn off the hardware (at least
2288  *      it must turn off frame reception.)
2289  *      May be called right after add_interface if that rejects
2290  *      an interface. If you added any work onto the mac80211 workqueue
2291  *      you should ensure to cancel it on this callback.
2292  *      Must be implemented and can sleep.
2293  *
2294  * @suspend: Suspend the device; mac80211 itself will quiesce before and
2295  *      stop transmitting and doing any other configuration, and then
2296  *      ask the device to suspend. This is only invoked when WoWLAN is
2297  *      configured, otherwise the device is deconfigured completely and
2298  *      reconfigured at resume time.
2299  *      The driver may also impose special conditions under which it
2300  *      wants to use the "normal" suspend (deconfigure), say if it only
2301  *      supports WoWLAN when the device is associated. In this case, it
2302  *      must return 1 from this function.
2303  *
2304  * @resume: If WoWLAN was configured, this indicates that mac80211 is
2305  *      now resuming its operation, after this the device must be fully
2306  *      functional again. If this returns an error, the only way out is
2307  *      to also unregister the device. If it returns 1, then mac80211
2308  *      will also go through the regular complete restart on resume.
2309  *
2310  * @set_wakeup: Enable or disable wakeup when WoWLAN configuration is
2311  *      modified. The reason is that device_set_wakeup_enable() is
2312  *      supposed to be called when the configuration changes, not only
2313  *      in suspend().
2314  *
2315  * @add_interface: Called when a netdevice attached to the hardware is
2316  *      enabled. Because it is not called for monitor mode devices, @start
2317  *      and @stop must be implemented.
2318  *      The driver should perform any initialization it needs before
2319  *      the device can be enabled. The initial configuration for the
2320  *      interface is given in the conf parameter.
2321  *      The callback may refuse to add an interface by returning a
2322  *      negative error code (which will be seen in userspace.)
2323  *      Must be implemented and can sleep.
2324  *
2325  * @change_interface: Called when a netdevice changes type. This callback
2326  *      is optional, but only if it is supported can interface types be
2327  *      switched while the interface is UP. The callback may sleep.
2328  *      Note that while an interface is being switched, it will not be
2329  *      found by the interface iteration callbacks.
2330  *
2331  * @remove_interface: Notifies a driver that an interface is going down.
2332  *      The @stop callback is called after this if it is the last interface
2333  *      and no monitor interfaces are present.
2334  *      When all interfaces are removed, the MAC address in the hardware
2335  *      must be cleared so the device no longer acknowledges packets,
2336  *      the mac_addr member of the conf structure is, however, set to the
2337  *      MAC address of the device going away.
2338  *      Hence, this callback must be implemented. It can sleep.
2339  *
2340  * @config: Handler for configuration requests. IEEE 802.11 code calls this
2341  *      function to change hardware configuration, e.g., channel.
2342  *      This function should never fail but returns a negative error code
2343  *      if it does. The callback can sleep.
2344  *
2345  * @bss_info_changed: Handler for configuration requests related to BSS
2346  *      parameters that may vary during BSS's lifespan, and may affect low
2347  *      level driver (e.g. assoc/disassoc status, erp parameters).
2348  *      This function should not be used if no BSS has been set, unless
2349  *      for association indication. The @changed parameter indicates which
2350  *      of the bss parameters has changed when a call is made. The callback
2351  *      can sleep.
2352  *
2353  * @prepare_multicast: Prepare for multicast filter configuration.
2354  *      This callback is optional, and its return value is passed
2355  *      to configure_filter(). This callback must be atomic.
2356  *
2357  * @configure_filter: Configure the device's RX filter.
2358  *      See the section "Frame filtering" for more information.
2359  *      This callback must be implemented and can sleep.
2360  *
2361  * @set_multicast_list: Configure the device's interface specific RX multicast
2362  *      filter. This callback is optional. This callback must be atomic.
2363  *
2364  * @set_tim: Set TIM bit. mac80211 calls this function when a TIM bit
2365  *      must be set or cleared for a given STA. Must be atomic.
2366  *
2367  * @set_key: See the section "Hardware crypto acceleration"
2368  *      This callback is only called between add_interface and
2369  *      remove_interface calls, i.e. while the given virtual interface
2370  *      is enabled.
2371  *      Returns a negative error code if the key can't be added.
2372  *      The callback can sleep.
2373  *
2374  * @update_tkip_key: See the section "Hardware crypto acceleration"
2375  *      This callback will be called in the context of Rx. Called for drivers
2376  *      which set IEEE80211_KEY_FLAG_TKIP_REQ_RX_P1_KEY.
2377  *      The callback must be atomic.
2378  *
2379  * @set_rekey_data: If the device supports GTK rekeying, for example while the
2380  *      host is suspended, it can assign this callback to retrieve the data
2381  *      necessary to do GTK rekeying, this is the KEK, KCK and replay counter.
2382  *      After rekeying was done it should (for example during resume) notify
2383  *      userspace of the new replay counter using ieee80211_gtk_rekey_notify().
2384  *
2385  * @set_default_unicast_key: Set the default (unicast) key index, useful for
2386  *      WEP when the device sends data packets autonomously, e.g. for ARP
2387  *      offloading. The index can be 0-3, or -1 for unsetting it.
2388  *
2389  * @hw_scan: Ask the hardware to service the scan request, no need to start
2390  *      the scan state machine in stack. The scan must honour the channel
2391  *      configuration done by the regulatory agent in the wiphy's
2392  *      registered bands. The hardware (or the driver) needs to make sure
2393  *      that power save is disabled.
2394  *      The @req ie/ie_len members are rewritten by mac80211 to contain the
2395  *      entire IEs after the SSID, so that drivers need not look at these
2396  *      at all but just send them after the SSID -- mac80211 includes the
2397  *      (extended) supported rates and HT information (where applicable).
2398  *      When the scan finishes, ieee80211_scan_completed() must be called;
2399  *      note that it also must be called when the scan cannot finish due to
2400  *      any error unless this callback returned a negative error code.
2401  *      The callback can sleep.
2402  *
2403  * @cancel_hw_scan: Ask the low-level tp cancel the active hw scan.
2404  *      The driver should ask the hardware to cancel the scan (if possible),
2405  *      but the scan will be completed only after the driver will call
2406  *      ieee80211_scan_completed().
2407  *      This callback is needed for wowlan, to prevent enqueueing a new
2408  *      scan_work after the low-level driver was already suspended.
2409  *      The callback can sleep.
2410  *
2411  * @sched_scan_start: Ask the hardware to start scanning repeatedly at
2412  *      specific intervals.  The driver must call the
2413  *      ieee80211_sched_scan_results() function whenever it finds results.
2414  *      This process will continue until sched_scan_stop is called.
2415  *
2416  * @sched_scan_stop: Tell the hardware to stop an ongoing scheduled scan.
2417  *
2418  * @sw_scan_start: Notifier function that is called just before a software scan
2419  *      is started. Can be NULL, if the driver doesn't need this notification.
2420  *      The callback can sleep.
2421  *
2422  * @sw_scan_complete: Notifier function that is called just after a
2423  *      software scan finished. Can be NULL, if the driver doesn't need
2424  *      this notification.
2425  *      The callback can sleep.
2426  *
2427  * @get_stats: Return low-level statistics.
2428  *      Returns zero if statistics are available.
2429  *      The callback can sleep.
2430  *
2431  * @get_tkip_seq: If your device implements TKIP encryption in hardware this
2432  *      callback should be provided to read the TKIP transmit IVs (both IV32
2433  *      and IV16) for the given key from hardware.
2434  *      The callback must be atomic.
2435  *
2436  * @set_frag_threshold: Configuration of fragmentation threshold. Assign this
2437  *      if the device does fragmentation by itself; if this callback is
2438  *      implemented then the stack will not do fragmentation.
2439  *      The callback can sleep.
2440  *
2441  * @set_rts_threshold: Configuration of RTS threshold (if device needs it)
2442  *      The callback can sleep.
2443  *
2444  * @sta_add: Notifies low level driver about addition of an associated station,
2445  *      AP, IBSS/WDS/mesh peer etc. This callback can sleep.
2446  *
2447  * @sta_remove: Notifies low level driver about removal of an associated
2448  *      station, AP, IBSS/WDS/mesh peer etc. This callback can sleep.
2449  *
2450  * @sta_add_debugfs: Drivers can use this callback to add debugfs files
2451  *      when a station is added to mac80211's station list. This callback
2452  *      and @sta_remove_debugfs should be within a CONFIG_MAC80211_DEBUGFS
2453  *      conditional. This callback can sleep.
2454  *
2455  * @sta_remove_debugfs: Remove the debugfs files which were added using
2456  *      @sta_add_debugfs. This callback can sleep.
2457  *
2458  * @sta_notify: Notifies low level driver about power state transition of an
2459  *      associated station, AP,  IBSS/WDS/mesh peer etc. For a VIF operating
2460  *      in AP mode, this callback will not be called when the flag
2461  *      %IEEE80211_HW_AP_LINK_PS is set. Must be atomic.
2462  *
2463  * @sta_state: Notifies low level driver about state transition of a
2464  *      station (which can be the AP, a client, IBSS/WDS/mesh peer etc.)
2465  *      This callback is mutually exclusive with @sta_add/@sta_remove.
2466  *      It must not fail for down transitions but may fail for transitions
2467  *      up the list of states.
2468  *      The callback can sleep.
2469  *
2470  * @sta_rc_update: Notifies the driver of changes to the bitrates that can be
2471  *      used to transmit to the station. The changes are advertised with bits
2472  *      from &enum ieee80211_rate_control_changed and the values are reflected
2473  *      in the station data. This callback should only be used when the driver
2474  *      uses hardware rate control (%IEEE80211_HW_HAS_RATE_CONTROL) since
2475  *      otherwise the rate control algorithm is notified directly.
2476  *      Must be atomic.
2477  *
2478  * @conf_tx: Configure TX queue parameters (EDCF (aifs, cw_min, cw_max),
2479  *      bursting) for a hardware TX queue.
2480  *      Returns a negative error code on failure.
2481  *      The callback can sleep.
2482  *
2483  * @get_tsf: Get the current TSF timer value from firmware/hardware. Currently,
2484  *      this is only used for IBSS mode BSSID merging and debugging. Is not a
2485  *      required function.
2486  *      The callback can sleep.
2487  *
2488  * @set_tsf: Set the TSF timer to the specified value in the firmware/hardware.
2489  *      Currently, this is only used for IBSS mode debugging. Is not a
2490  *      required function.
2491  *      The callback can sleep.
2492  *
2493  * @reset_tsf: Reset the TSF timer and allow firmware/hardware to synchronize
2494  *      with other STAs in the IBSS. This is only used in IBSS mode. This
2495  *      function is optional if the firmware/hardware takes full care of
2496  *      TSF synchronization.
2497  *      The callback can sleep.
2498  *
2499  * @tx_last_beacon: Determine whether the last IBSS beacon was sent by us.
2500  *      This is needed only for IBSS mode and the result of this function is
2501  *      used to determine whether to reply to Probe Requests.
2502  *      Returns non-zero if this device sent the last beacon.
2503  *      The callback can sleep.
2504  *
2505  * @ampdu_action: Perform a certain A-MPDU action
2506  *      The RA/TID combination determines the destination and TID we want
2507  *      the ampdu action to be performed for. The action is defined through
2508  *      ieee80211_ampdu_mlme_action. Starting sequence number (@ssn)
2509  *      is the first frame we expect to perform the action on. Notice
2510  *      that TX/RX_STOP can pass NULL for this parameter.
2511  *      The @buf_size parameter is only valid when the action is set to
2512  *      %IEEE80211_AMPDU_TX_OPERATIONAL and indicates the peer's reorder
2513  *      buffer size (number of subframes) for this session -- the driver
2514  *      may neither send aggregates containing more subframes than this
2515  *      nor send aggregates in a way that lost frames would exceed the
2516  *      buffer size. If just limiting the aggregate size, this would be
2517  *      possible with a buf_size of 8:
2518  *       - TX: 1.....7
2519  *       - RX:  2....7 (lost frame #1)
2520  *       - TX:        8..1...
2521  *      which is invalid since #1 was now re-transmitted well past the
2522  *      buffer size of 8. Correct ways to retransmit #1 would be:
2523  *       - TX:       1 or 18 or 81
2524  *      Even "189" would be wrong since 1 could be lost again.
2525  *
2526  *      Returns a negative error code on failure.
2527  *      The callback can sleep.
2528  *
2529  * @get_survey: Return per-channel survey information
2530  *
2531  * @rfkill_poll: Poll rfkill hardware state. If you need this, you also
2532  *      need to set wiphy->rfkill_poll to %true before registration,
2533  *      and need to call wiphy_rfkill_set_hw_state() in the callback.
2534  *      The callback can sleep.
2535  *
2536  * @set_coverage_class: Set slot time for given coverage class as specified
2537  *      in IEEE 802.11-2007 section 17.3.8.6 and modify ACK timeout
2538  *      accordingly. This callback is not required and may sleep.
2539  *
2540  * @testmode_cmd: Implement a cfg80211 test mode command. The passed @vif may
2541  *      be %NULL. The callback can sleep.
2542  * @testmode_dump: Implement a cfg80211 test mode dump. The callback can sleep.
2543  *
2544  * @flush: Flush all pending frames from the hardware queue, making sure
2545  *      that the hardware queues are empty. The @queues parameter is a bitmap
2546  *      of queues to flush, which is useful if different virtual interfaces
2547  *      use different hardware queues; it may also indicate all queues.
2548  *      If the parameter @drop is set to %true, pending frames may be dropped.
2549  *      The callback can sleep.
2550  *
2551  * @channel_switch: Drivers that need (or want) to offload the channel
2552  *      switch operation for CSAs received from the AP may implement this
2553  *      callback. They must then call ieee80211_chswitch_done() to indicate
2554  *      completion of the channel switch.
2555  *
2556  * @napi_poll: Poll Rx queue for incoming data frames.
2557  *
2558  * @set_antenna: Set antenna configuration (tx_ant, rx_ant) on the device.
2559  *      Parameters are bitmaps of allowed antennas to use for TX/RX. Drivers may
2560  *      reject TX/RX mask combinations they cannot support by returning -EINVAL
2561  *      (also see nl80211.h @NL80211_ATTR_WIPHY_ANTENNA_TX).
2562  *
2563  * @get_antenna: Get current antenna configuration from device (tx_ant, rx_ant).
2564  *
2565  * @remain_on_channel: Starts an off-channel period on the given channel, must
2566  *      call back to ieee80211_ready_on_channel() when on that channel. Note
2567  *      that normal channel traffic is not stopped as this is intended for hw
2568  *      offload. Frames to transmit on the off-channel channel are transmitted
2569  *      normally except for the %IEEE80211_TX_CTL_TX_OFFCHAN flag. When the
2570  *      duration (which will always be non-zero) expires, the driver must call
2571  *      ieee80211_remain_on_channel_expired().
2572  *      Note that this callback may be called while the device is in IDLE and
2573  *      must be accepted in this case.
2574  *      This callback may sleep.
2575  * @cancel_remain_on_channel: Requests that an ongoing off-channel period is
2576  *      aborted before it expires. This callback may sleep.
2577  *
2578  * @set_ringparam: Set tx and rx ring sizes.
2579  *
2580  * @get_ringparam: Get tx and rx ring current and maximum sizes.
2581  *
2582  * @tx_frames_pending: Check if there is any pending frame in the hardware
2583  *      queues before entering power save.
2584  *
2585  * @set_bitrate_mask: Set a mask of rates to be used for rate control selection
2586  *      when transmitting a frame. Currently only legacy rates are handled.
2587  *      The callback can sleep.
2588  * @rssi_callback: Notify driver when the average RSSI goes above/below
2589  *      thresholds that were registered previously. The callback can sleep.
2590  *
2591  * @release_buffered_frames: Release buffered frames according to the given
2592  *      parameters. In the case where the driver buffers some frames for
2593  *      sleeping stations mac80211 will use this callback to tell the driver
2594  *      to release some frames, either for PS-poll or uAPSD.
2595  *      Note that if the @more_data paramter is %false the driver must check
2596  *      if there are more frames on the given TIDs, and if there are more than
2597  *      the frames being released then it must still set the more-data bit in
2598  *      the frame. If the @more_data parameter is %true, then of course the
2599  *      more-data bit must always be set.
2600  *      The @tids parameter tells the driver which TIDs to release frames
2601  *      from, for PS-poll it will always have only a single bit set.
2602  *      In the case this is used for a PS-poll initiated release, the
2603  *      @num_frames parameter will always be 1 so code can be shared. In
2604  *      this case the driver must also set %IEEE80211_TX_STATUS_EOSP flag
2605  *      on the TX status (and must report TX status) so that the PS-poll
2606  *      period is properly ended. This is used to avoid sending multiple
2607  *      responses for a retried PS-poll frame.
2608  *      In the case this is used for uAPSD, the @num_frames parameter may be
2609  *      bigger than one, but the driver may send fewer frames (it must send
2610  *      at least one, however). In this case it is also responsible for
2611  *      setting the EOSP flag in the QoS header of the frames. Also, when the
2612  *      service period ends, the driver must set %IEEE80211_TX_STATUS_EOSP
2613  *      on the last frame in the SP. Alternatively, it may call the function
2614  *      ieee80211_sta_eosp() to inform mac80211 of the end of the SP.
2615  *      This callback must be atomic.
2616  * @allow_buffered_frames: Prepare device to allow the given number of frames
2617  *      to go out to the given station. The frames will be sent by mac80211
2618  *      via the usual TX path after this call. The TX information for frames
2619  *      released will also have the %IEEE80211_TX_CTL_NO_PS_BUFFER flag set
2620  *      and the last one will also have %IEEE80211_TX_STATUS_EOSP set. In case
2621  *      frames from multiple TIDs are released and the driver might reorder
2622  *      them between the TIDs, it must set the %IEEE80211_TX_STATUS_EOSP flag
2623  *      on the last frame and clear it on all others and also handle the EOSP
2624  *      bit in the QoS header correctly. Alternatively, it can also call the
2625  *      ieee80211_sta_eosp() function.
2626  *      The @tids parameter is a bitmap and tells the driver which TIDs the
2627  *      frames will be on; it will at most have two bits set.
2628  *      This callback must be atomic.
2629  *
2630  * @get_et_sset_count:  Ethtool API to get string-set count.
2631  *
2632  * @get_et_stats:  Ethtool API to get a set of u64 stats.
2633  *
2634  * @get_et_strings:  Ethtool API to get a set of strings to describe stats
2635  *      and perhaps other supported types of ethtool data-sets.
2636  *
2637  * @get_rssi: Get current signal strength in dBm, the function is optional
2638  *      and can sleep.
2639  *
2640  * @mgd_prepare_tx: Prepare for transmitting a management frame for association
2641  *      before associated. In multi-channel scenarios, a virtual interface is
2642  *      bound to a channel before it is associated, but as it isn't associated
2643  *      yet it need not necessarily be given airtime, in particular since any
2644  *      transmission to a P2P GO needs to be synchronized against the GO's
2645  *      powersave state. mac80211 will call this function before transmitting a
2646  *      management frame prior to having successfully associated to allow the
2647  *      driver to give it channel time for the transmission, to get a response
2648  *      and to be able to synchronize with the GO.
2649  *      The callback will be called before each transmission and upon return
2650  *      mac80211 will transmit the frame right away.
2651  *      The callback is optional and can (should!) sleep.
2652  *
2653  * @add_chanctx: Notifies device driver about new channel context creation.
2654  * @remove_chanctx: Notifies device driver about channel context destruction.
2655  * @change_chanctx: Notifies device driver about channel context changes that
2656  *      may happen when combining different virtual interfaces on the same
2657  *      channel context with different settings
2658  * @assign_vif_chanctx: Notifies device driver about channel context being bound
2659  *      to vif. Possible use is for hw queue remapping.
2660  * @unassign_vif_chanctx: Notifies device driver about channel context being
2661  *      unbound from vif.
2662  * @start_ap: Start operation on the AP interface, this is called after all the
2663  *      information in bss_conf is set and beacon can be retrieved. A channel
2664  *      context is bound before this is called. Note that if the driver uses
2665  *      software scan or ROC, this (and @stop_ap) isn't called when the AP is
2666  *      just "paused" for scanning/ROC, which is indicated by the beacon being
2667  *      disabled/enabled via @bss_info_changed.
2668  * @stop_ap: Stop operation on the AP interface.
2669  *
2670  * @restart_complete: Called after a call to ieee80211_restart_hw(), when the
2671  *      reconfiguration has completed. This can help the driver implement the
2672  *      reconfiguration step. Also called when reconfiguring because the
2673  *      driver's resume function returned 1, as this is just like an "inline"
2674  *      hardware restart. This callback may sleep.
2675  *
2676  * @ipv6_addr_change: IPv6 address assignment on the given interface changed.
2677  *      Currently, this is only called for managed or P2P client interfaces.
2678  *      This callback is optional; it must not sleep.
2679  *
2680  * @channel_switch_beacon: Starts a channel switch to a new channel.
2681  *      Beacons are modified to include CSA or ECSA IEs before calling this
2682  *      function. The corresponding count fields in these IEs must be
2683  *      decremented, and when they reach zero the driver must call
2684  *      ieee80211_csa_finish(). Drivers which use ieee80211_beacon_get()
2685  *      get the csa counter decremented by mac80211, but must check if it is
2686  *      zero using ieee80211_csa_is_complete() after the beacon has been
2687  *      transmitted and then call ieee80211_csa_finish().
2688  *
2689  * @join_ibss: Join an IBSS (on an IBSS interface); this is called after all
2690  *      information in bss_conf is set up and the beacon can be retrieved. A
2691  *      channel context is bound before this is called.
2692  * @leave_ibss: Leave the IBSS again.
2693  */
struct ieee80211_ops {
	         void (*tx)(struct ieee80211_hw *hw,
	                    struct ieee80211_tx_control *control,
	                    struct sk_buff *skb);
	         int (*start)(struct ieee80211_hw *hw);
	         void (*stop)(struct ieee80211_hw *hw);
	#ifdef CONFIG_PM
	         int (*suspend)(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan);
	         int (*resume)(struct ieee80211_hw *hw);
	         void (*set_wakeup)(struct ieee80211_hw *hw, bool enabled);
	#endif
	         int (*add_interface)(struct ieee80211_hw *hw,
	                              struct ieee80211_vif *vif);
	         int (*change_interface)(struct ieee80211_hw *hw,
	                                 struct ieee80211_vif *vif,
	                                 enum nl80211_iftype new_type, bool p2p);
	         void (*remove_interface)(struct ieee80211_hw *hw,
	                                  struct ieee80211_vif *vif);
	         int (*config)(struct ieee80211_hw *hw, uint32_t changed);
	         void (*bss_info_changed)(struct ieee80211_hw *hw,
	                                  struct ieee80211_vif *vif,
	                                  struct ieee80211_bss_conf *info,
	                                  uint32_t changed);
	
	         int (*start_ap)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	         void (*stop_ap)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	
	         uint64_t (*prepare_multicast)(struct ieee80211_hw *hw,
	                                  struct netdev_hw_addr_list *mc_list);
	         void (*configure_filter)(struct ieee80211_hw *hw,
	                                  unsigned int changed_flags,
	                                  unsigned int *total_flags,
	                                  uint64_t multicast);
	         void (*set_multicast_list)(struct ieee80211_hw *hw,
	                                    struct ieee80211_vif *vif, bool allmulti,
	                                    struct netdev_hw_addr_list *mc_list);
	
	         int (*set_tim)(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
	                        bool set);
	         int (*set_key)(struct ieee80211_hw *hw, enum set_key_cmd cmd,
	                        struct ieee80211_vif *vif, struct ieee80211_sta *sta,
	                        struct ieee80211_key_conf *key);
	         void (*update_tkip_key)(struct ieee80211_hw *hw,
	                                 struct ieee80211_vif *vif,
	                                 struct ieee80211_key_conf *conf,
	                                 struct ieee80211_sta *sta,
	                                 uint32_t iv32, uint16_t *phase1key);
	         void (*set_rekey_data)(struct ieee80211_hw *hw,
	                                struct ieee80211_vif *vif,
	                                struct cfg80211_gtk_rekey_data *data);
	         void (*set_default_unicast_key)(struct ieee80211_hw *hw,
	                                         struct ieee80211_vif *vif, int idx);
	         int (*hw_scan)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                        struct cfg80211_scan_request *req);
	         void (*cancel_hw_scan)(struct ieee80211_hw *hw,
	                                struct ieee80211_vif *vif);
	         int (*sched_scan_start)(struct ieee80211_hw *hw,
	                                 struct ieee80211_vif *vif,
	                                 struct cfg80211_sched_scan_request *req,
	                                 struct ieee80211_sched_scan_ies *ies);
	         void (*sched_scan_stop)(struct ieee80211_hw *hw,
	                                struct ieee80211_vif *vif);
	         void (*sw_scan_start)(struct ieee80211_hw *hw);
	         void (*sw_scan_complete)(struct ieee80211_hw *hw);
	         int (*get_stats)(struct ieee80211_hw *hw,
	                          struct ieee80211_low_level_stats *stats);
	         void (*get_tkip_seq)(struct ieee80211_hw *hw, uint8_t hw_key_idx,
	                              uint32_t *iv32, uint16_t *iv16);
	         int (*set_frag_threshold)(struct ieee80211_hw *hw, uint32_t value);
	         int (*set_rts_threshold)(struct ieee80211_hw *hw, uint32_t value);
	         int (*sta_add)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                        struct ieee80211_sta *sta);
	         int (*sta_remove)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                           struct ieee80211_sta *sta);
	 #ifdef CONFIG_MAC80211_DEBUGFS
	         void (*sta_add_debugfs)(struct ieee80211_hw *hw,
	                                 struct ieee80211_vif *vif,
	                                 struct ieee80211_sta *sta,
	                                 struct dentry *dir);
	         void (*sta_remove_debugfs)(struct ieee80211_hw *hw,
	                                    struct ieee80211_vif *vif,
	                                    struct ieee80211_sta *sta,
	                                    struct dentry *dir);
	 #endif
	         void (*sta_notify)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                         enum sta_notify_cmd, struct ieee80211_sta *sta);
	         int (*sta_state)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                          struct ieee80211_sta *sta,
	                          enum ieee80211_sta_state old_state,
	                          enum ieee80211_sta_state new_state);
	         void (*sta_rc_update)(struct ieee80211_hw *hw,
	                               struct ieee80211_vif *vif,
	                               struct ieee80211_sta *sta,
	                               uint32_t changed);
	         int (*conf_tx)(struct ieee80211_hw *hw,
	                        struct ieee80211_vif *vif, uint16_t ac,
	                        const struct ieee80211_tx_queue_params *params);
	         uint64_t (*get_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	         void (*set_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                         uint64_t tsf);
	         void (*reset_tsf)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	         int (*tx_last_beacon)(struct ieee80211_hw *hw);
	         int (*ampdu_action)(struct ieee80211_hw *hw,
	                             struct ieee80211_vif *vif,
	                             enum ieee80211_ampdu_mlme_action action,
	                             struct ieee80211_sta *sta, uint16_t tid, uint16_t *ssn,
	                             uint8_t buf_size);
	         int (*get_survey)(struct ieee80211_hw *hw, int idx,
	                 struct survey_info *survey);
	         void (*rfkill_poll)(struct ieee80211_hw *hw);
	         void (*set_coverage_class)(struct ieee80211_hw *hw, uint8_t coverage_class);
	 #ifdef CONFIG_NL80211_TESTMODE
	         int (*testmode_cmd)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                             void *data, int len);
	         int (*testmode_dump)(struct ieee80211_hw *hw, struct sk_buff *skb,
	                              struct netlink_callback *cb,
	                              void *data, int len);
	 #endif
	         void (*flush)(struct ieee80211_hw *hw, uint32_t queues, bool drop);
	         void (*channel_switch)(struct ieee80211_hw *hw,
	                                struct ieee80211_channel_switch *ch_switch);
	         int (*napi_poll)(struct ieee80211_hw *hw, int budget);
	         int (*set_antenna)(struct ieee80211_hw *hw, uint32_t tx_ant, uint32_t rx_ant);
	         int (*get_antenna)(struct ieee80211_hw *hw, uint32_t *tx_ant, uint32_t *rx_ant);
	
	         int (*remain_on_channel)(struct ieee80211_hw *hw,
	                                  struct ieee80211_vif *vif,
	                                  struct ieee80211_channel *chan,
	                                  int duration,
	                                  enum ieee80211_roc_type type);
	         int (*cancel_remain_on_channel)(struct ieee80211_hw *hw);
	         int (*set_ringparam)(struct ieee80211_hw *hw, uint32_t tx, uint32_t rx);
	         void (*get_ringparam)(struct ieee80211_hw *hw,
	                               uint32_t *tx, uint32_t *tx_max, uint32_t *rx, uint32_t *rx_max);
	         bool (*tx_frames_pending)(struct ieee80211_hw *hw);
	         int (*set_bitrate_mask)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                                 const struct cfg80211_bitrate_mask *mask);
	         void (*rssi_callback)(struct ieee80211_hw *hw,
	                               struct ieee80211_vif *vif,
	                               enum ieee80211_rssi_event rssi_event);
	
	         void (*allow_buffered_frames)(struct ieee80211_hw *hw,
	                                       struct ieee80211_sta *sta,
	                                       uint16_t tids, int num_frames,
	                                       enum ieee80211_frame_release_type reason,
	                                       bool more_data);
	         void (*release_buffered_frames)(struct ieee80211_hw *hw,
	                                         struct ieee80211_sta *sta,
	                                         uint16_t tids, int num_frames,
	                                         enum ieee80211_frame_release_type reason,
	                                         bool more_data);
	
	         int     (*get_et_sset_count)(struct ieee80211_hw *hw,
	                                      struct ieee80211_vif *vif, int sset);
	         void    (*get_et_stats)(struct ieee80211_hw *hw,
	                                 struct ieee80211_vif *vif,
	                                 struct ethtool_stats *stats, uint64_t *data);
	         void    (*get_et_strings)(struct ieee80211_hw *hw,
	                                   struct ieee80211_vif *vif,
	                                   uint32_t sset, uint8_t *data);
	         int     (*get_rssi)(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	                             struct ieee80211_sta *sta, int8_t *rssi_dbm);
	
	         void    (*mgd_prepare_tx)(struct ieee80211_hw *hw,
	                                   struct ieee80211_vif *vif);
	
	         int (*add_chanctx)(struct ieee80211_hw *hw,
	                            struct ieee80211_chanctx_conf *ctx);
	         void (*remove_chanctx)(struct ieee80211_hw *hw,
	                                struct ieee80211_chanctx_conf *ctx);
	         void (*change_chanctx)(struct ieee80211_hw *hw,
	                                struct ieee80211_chanctx_conf *ctx,
	                                uint32_t changed);
	         int (*assign_vif_chanctx)(struct ieee80211_hw *hw,
	                                   struct ieee80211_vif *vif,
	                                   struct ieee80211_chanctx_conf *ctx);
	         void (*unassign_vif_chanctx)(struct ieee80211_hw *hw,
	                                      struct ieee80211_vif *vif,
	                                      struct ieee80211_chanctx_conf *ctx);
	
	         void (*restart_complete)(struct ieee80211_hw *hw);
	
	 #if 0//IS_ENABLED (CONFIG_IPV6)
	         void (*ipv6_addr_change)(struct ieee80211_hw *hw,
	                                  struct ieee80211_vif *vif,
	                                  struct inet6_dev *idev);
	 #endif
	         void (*channel_switch_beacon)(struct ieee80211_hw *hw,
	                                       struct ieee80211_vif *vif,
	                                       struct cfg80211_chan_def *chandef);
	
	         int (*join_ibss)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
	         void (*leave_ibss)(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
};

/**
2890  * ieee80211_alloc_hw -  Allocate a new hardware device
2891  *
2892  * This must be called once for each hardware device. The returned pointer
2893  * must be used to refer to this device when calling other functions.
2894  * mac80211 allocates a private data area for the driver pointed to by
2895  * @priv in &struct ieee80211_hw, the size of this area is given as
2896  * @priv_data_len.
2897  *
2898  * @priv_data_len: length of private data
2899  * @ops: callbacks for this device
2900  *
2901  * Return: A pointer to the new hardware device, or %NULL on error.
2902  */
struct ieee80211_hw *ieee80211_alloc_hw(size_t priv_data_len,
                                         const struct ieee80211_ops *ops);
										 
										 
/**
2907  * ieee80211_register_hw - Register hardware device
2908  *
2909  * You must call this function before any other functions in
2910  * mac80211. Note that before a hardware can be registered, you
2911  * need to fill the contained wiphy's information.
2912  *
2913  * @hw: the device to register as returned by ieee80211_alloc_hw()
2914  *
2915  * Return: 0 on success. An error code otherwise.
2916  */
int ieee80211_register_hw(struct ieee80211_hw *hw);										 
										 
/**
2920  * struct ieee80211_tpt_blink - throughput blink description
2921  * @throughput: throughput in Kbit/sec
2922  * @blink_time: blink time in milliseconds
2923  *      (full cycle, ie. one off + one on period)
2924  */
struct ieee80211_tpt_blink {
	         int throughput;
	         int blink_time;
};

/**
2931  * enum ieee80211_tpt_led_trigger_flags - throughput trigger flags
2932  * @IEEE80211_TPT_LEDTRIG_FL_RADIO: enable blinking with radio
2933  * @IEEE80211_TPT_LEDTRIG_FL_WORK: enable blinking when working
2934  * @IEEE80211_TPT_LEDTRIG_FL_CONNECTED: enable blinking when at least one
2935  *      interface is connected in some way, including being an AP
2936  */
enum ieee80211_tpt_led_trigger_flags {
         IEEE80211_TPT_LEDTRIG_FL_RADIO          = BIT(0),
         IEEE80211_TPT_LEDTRIG_FL_WORK           = BIT(1),
         IEEE80211_TPT_LEDTRIG_FL_CONNECTED      = BIT(2),
};

#ifdef CONFIG_MAC80211_LEDS
2944 char *__ieee80211_get_tx_led_name(struct ieee80211_hw *hw);
2945 char *__ieee80211_get_rx_led_name(struct ieee80211_hw *hw);
2946 char *__ieee80211_get_assoc_led_name(struct ieee80211_hw *hw);
2947 char *__ieee80211_get_radio_led_name(struct ieee80211_hw *hw);
2948 char *__ieee80211_create_tpt_led_trigger(struct ieee80211_hw *hw,
2949                                          unsigned int flags,
2950                                          const struct ieee80211_tpt_blink *blink_table,
2951                                          unsigned int blink_table_len);
#endif

/**
2954  * ieee80211_get_tx_led_name - get name of TX LED
2955  *
2956  * mac80211 creates a transmit LED trigger for each wireless hardware
2957  * that can be used to drive LEDs if your driver registers a LED device.
2958  * This function returns the name (or %NULL if not configured for LEDs)
2959  * of the trigger so you can automatically link the LED device.
2960  *
2961  * @hw: the hardware to get the LED trigger name for
2962  *
2963  * Return: The name of the LED trigger. %NULL if not configured for LEDs.
2964  */
static inline char *ieee80211_get_tx_led_name(struct ieee80211_hw *hw)
{
	#ifdef CONFIG_MAC80211_LEDS
	         return __ieee80211_get_tx_led_name(hw);
	#else
	         return NULL;
	#endif
}

/**
2975  * ieee80211_get_rx_led_name - get name of RX LED
2976  *
2977  * mac80211 creates a receive LED trigger for each wireless hardware
2978  * that can be used to drive LEDs if your driver registers a LED device.
2979  * This function returns the name (or %NULL if not configured for LEDs)
2980  * of the trigger so you can automatically link the LED device.
2981  *
2982  * @hw: the hardware to get the LED trigger name for
2983  *
2984  * Return: The name of the LED trigger. %NULL if not configured for LEDs.
2985  */
static inline char *ieee80211_get_rx_led_name(struct ieee80211_hw *hw)
{
	#ifdef CONFIG_MAC80211_LEDS
	         return __ieee80211_get_rx_led_name(hw);
	#else
	         return NULL;
	#endif
}

/**
2996  * ieee80211_get_assoc_led_name - get name of association LED
2997  *
2998  * mac80211 creates a association LED trigger for each wireless hardware
2999  * that can be used to drive LEDs if your driver registers a LED device.
3000  * This function returns the name (or %NULL if not configured for LEDs)
3001  * of the trigger so you can automatically link the LED device.
3002  *
3003  * @hw: the hardware to get the LED trigger name for
3004  *
3005  * Return: The name of the LED trigger. %NULL if not configured for LEDs.
3006  */
static inline char *ieee80211_get_assoc_led_name(struct ieee80211_hw *hw)
{
	#ifdef CONFIG_MAC80211_LEDS
	         return __ieee80211_get_assoc_led_name(hw);
	#else
	         return NULL;
	#endif
}

/**
3017  * ieee80211_get_radio_led_name - get name of radio LED
3018  *
3019  * mac80211 creates a radio change LED trigger for each wireless hardware
3020  * that can be used to drive LEDs if your driver registers a LED device.
3021  * This function returns the name (or %NULL if not configured for LEDs)
3022  * of the trigger so you can automatically link the LED device.
3023  *
3024  * @hw: the hardware to get the LED trigger name for
3025  *
3026  * Return: The name of the LED trigger. %NULL if not configured for LEDs.
3027  */
static inline char *ieee80211_get_radio_led_name(struct ieee80211_hw *hw)
{
	#ifdef CONFIG_MAC80211_LEDS
	         return __ieee80211_get_radio_led_name(hw);
	#else
	         return NULL;
	#endif
}

/**
3038  * ieee80211_create_tpt_led_trigger - create throughput LED trigger
3039  * @hw: the hardware to create the trigger for
3040  * @flags: trigger flags, see &enum ieee80211_tpt_led_trigger_flags
3041  * @blink_table: the blink table -- needs to be ordered by throughput
3042  * @blink_table_len: size of the blink table
3043  *
3044  * Return: %NULL (in case of error, or if no LED triggers are
3045  * configured) or the name of the new trigger.
3046  *
3047  * Note: This function must be called before ieee80211_register_hw().
3048  */
static inline char *
ieee80211_create_tpt_led_trigger(struct ieee80211_hw *hw, unsigned int flags,
                                  const struct ieee80211_tpt_blink *blink_table,
                                  unsigned int blink_table_len)
{
	#ifdef CONFIG_MAC80211_LEDS
	         return __ieee80211_create_tpt_led_trigger(hw, flags, blink_table,
	                                                   blink_table_len);
	#else
	         return NULL;
	#endif
}

/**
3063  * ieee80211_unregister_hw - Unregister a hardware device
3064  *
3065  * This function instructs mac80211 to free allocated resources
3066  * and unregister netdevices from the networking subsystem.
3067  *
3068  * @hw: the hardware to unregister
3069  */
void ieee80211_unregister_hw(struct ieee80211_hw *hw);

/**
3073  * ieee80211_free_hw - free hardware descriptor
3074  *
3075  * This function frees everything that was allocated, including the
3076  * private data for the driver. You must call ieee80211_unregister_hw()
3077  * before calling this function.
3078  *
3079  * @hw: the hardware to free
3080  */
void ieee80211_free_hw(struct ieee80211_hw *hw);

/**
3084  * ieee80211_restart_hw - restart hardware completely
3085  *
3086  * Call this function when the hardware was restarted for some reason
3087  * (hardware error, ...) and the driver is unable to restore its state
3088  * by itself. mac80211 assumes that at this point the driver/hardware
3089  * is completely uninitialised and stopped, it starts the process by
3090  * calling the ->start() operation. The driver will need to reset all
3091  * internal state that it has prior to calling this function.
3092  *
3093  * @hw: the hardware to restart
3094  */
void ieee80211_restart_hw(struct ieee80211_hw *hw);

/** ieee80211_napi_schedule - schedule NAPI poll
3098  *
3099  * Use this function to schedule NAPI polling on a device.
3100  *
3101  * @hw: the hardware to start polling
3102  */
void ieee80211_napi_schedule(struct ieee80211_hw *hw);

/** ieee80211_napi_complete - complete NAPI polling
3106  *
3107  * Use this function to finish NAPI polling on a device.
3108  *
3109  * @hw: the hardware to stop polling
3110  */
void ieee80211_napi_complete(struct ieee80211_hw *hw);


/**
3114  * ieee80211_rx - receive frame
3115  *
3116  * Use this function to hand received frames to mac80211. The receive
3117  * buffer in @skb must start with an IEEE 802.11 header. In case of a
3118  * paged @skb is used, the driver is recommended to put the ieee80211
3119  * header of the frame on the linear part of the @skb to avoid memory
3120  * allocation and/or memcpy by the stack.
3121  *
3122  * This function may not be called in IRQ context. Calls to this function
3123  * for a single hardware must be synchronized against each other. Calls to
3124  * this function, ieee80211_rx_ni() and ieee80211_rx_irqsafe() may not be
3125  * mixed for a single hardware. Must not run concurrently with
3126  * ieee80211_tx_status() or ieee80211_tx_status_ni().
3127  *
3128  * In process context use instead ieee80211_rx_ni().
3129  *
3130  * @hw: the hardware this frame came in on
3131  * @skb: the buffer to receive, owned by mac80211 after this call
3132  */
void ieee80211_rx(struct ieee80211_hw *hw, struct sk_buff *skb);

/**
3136  * ieee80211_rx_irqsafe - receive frame
3137  *
3138  * Like ieee80211_rx() but can be called in IRQ context
3139  * (internally defers to a tasklet.)
3140  *
3141  * Calls to this function, ieee80211_rx() or ieee80211_rx_ni() may not
3142  * be mixed for a single hardware.Must not run concurrently with
3143  * ieee80211_tx_status() or ieee80211_tx_status_ni().
3144  *
3145  * @hw: the hardware this frame came in on
3146  * @skb: the buffer to receive, owned by mac80211 after this call
3147  */
void ieee80211_rx_irqsafe(struct ieee80211_hw *hw, struct sk_buff *skb);

/**
3151  * ieee80211_rx_ni - receive frame (in process context)
3152  *
3153  * Like ieee80211_rx() but can be called in process context
3154  * (internally disables bottom halves).
3155  *
3156  * Calls to this function, ieee80211_rx() and ieee80211_rx_irqsafe() may
3157  * not be mixed for a single hardware. Must not run concurrently with
3158  * ieee80211_tx_status() or ieee80211_tx_status_ni().
3159  *
3160  * @hw: the hardware this frame came in on
3161  * @skb: the buffer to receive, owned by mac80211 after this call
3162  */
static inline void ieee80211_rx_ni(struct ieee80211_hw *hw,
                                    struct sk_buff *skb)
{
	         //local_bh_disable();
	         ieee80211_rx(hw, skb);
	         //local_bh_enable();
}

/**
3172  * ieee80211_sta_ps_transition - PS transition for connected sta
3173  *
3174  * When operating in AP mode with the %IEEE80211_HW_AP_LINK_PS
3175  * flag set, use this function to inform mac80211 about a connected station
3176  * entering/leaving PS mode.
3177  *
3178  * This function may not be called in IRQ context or with softirqs enabled.
3179  *
3180  * Calls to this function for a single hardware must be synchronized against
3181  * each other.
3182  *
3183  * @sta: currently connected sta
3184  * @start: start or stop PS
3185  *
3186  * Return: 0 on success. -EINVAL when the requested PS mode is already set.
3187  */
int ieee80211_sta_ps_transition(struct ieee80211_sta *sta, bool start);

/**
3191  * ieee80211_sta_ps_transition_ni - PS transition for connected sta
3192  *                                  (in process context)
3193  *
3194  * Like ieee80211_sta_ps_transition() but can be called in process context
3195  * (internally disables bottom halves). Concurrent call restriction still
3196  * applies.
3197  *
3198  * @sta: currently connected sta
3199  * @start: start or stop PS
3200  *
3201  * Return: Like ieee80211_sta_ps_transition().
3202  */
static inline int ieee80211_sta_ps_transition_ni(struct ieee80211_sta *sta,
                                                   bool start)
{
	         int ret;
	
	         //local_bh_disable();
	         ret = ieee80211_sta_ps_transition(sta, start);
	         //local_bh_enable();
	
	         return ret;
}

/*
3216  * The TX headroom reserved by mac80211 for its own tx_status functions.
3217  * This is enough for the radiotap header.
3218  */
#define IEEE80211_TX_STATUS_HEADROOM    14

/**
3222  * ieee80211_sta_set_buffered - inform mac80211 about driver-buffered frames
3223  * @sta: &struct ieee80211_sta pointer for the sleeping station
3224  * @tid: the TID that has buffered frames
3225  * @buffered: indicates whether or not frames are buffered for this TID
3226  *
3227  * If a driver buffers frames for a powersave station instead of passing
3228  * them back to mac80211 for retransmission, the station may still need
3229  * to be told that there are buffered frames via the TIM bit.
3230  *
3231  * This function informs mac80211 whether or not there are frames that are
3232  * buffered in the driver for a given TID; mac80211 can then use this data
3233  * to set the TIM bit (NOTE: This may call back into the driver's set_tim
3234  * call! Beware of the locking!)
3235  *
3236  * If all frames are released to the station (due to PS-poll or uAPSD)
3237  * then the driver needs to inform mac80211 that there no longer are
3238  * frames buffered. However, when the station wakes up mac80211 assumes
3239  * that all buffered frames will be transmitted and clears this data,
3240  * drivers need to make sure they inform mac80211 about all buffered
3241  * frames on the sleep transition (sta_notify() with %STA_NOTIFY_SLEEP).
3242  *
3243  * Note that technically mac80211 only needs to know this per AC, not per
3244  * TID, but since driver buffering will inevitably happen per TID (since
3245  * it is related to aggregation) it is easier to make mac80211 map the
3246  * TID to the AC as required instead of keeping track in all drivers that
3247  * use this API.
3248  */
void ieee80211_sta_set_buffered(struct ieee80211_sta *sta,
                                 uint8_t tid, bool buffered);

/**
3253  * ieee80211_get_tx_rates - get the selected transmit rates for a packet
3254  *
3255  * Call this function in a driver with per-packet rate selection support
3256  * to combine the rate info in the packet tx info with the most recent
3257  * rate selection table for the station entry.
3258  *
3259  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3260  * @sta: the receiver station to which this packet is sent.
3261  * @skb: the frame to be transmitted.
3262  * @dest: buffer for extracted rate/retry information
3263  * @max_rates: maximum number of rates to fetch
3264  */
void ieee80211_get_tx_rates(struct ieee80211_vif *vif,
                             struct ieee80211_sta *sta,
                             struct sk_buff *skb,
                             struct ieee80211_tx_rate *dest,
                             int max_rates);

/**
3272  * ieee80211_tx_status - transmit status callback
3273  *
3274  * Call this function for all transmitted frames after they have been
3275  * transmitted. It is permissible to not call this function for
3276  * multicast frames but this can affect statistics.
3277  *
3278  * This function may not be called in IRQ context. Calls to this function
3279  * for a single hardware must be synchronized against each other. Calls
3280  * to this function, ieee80211_tx_status_ni() and ieee80211_tx_status_irqsafe()
3281  * may not be mixed for a single hardware. Must not run concurrently with
3282  * ieee80211_rx() or ieee80211_rx_ni().
3283  *
3284  * @hw: the hardware the frame was transmitted by
3285  * @skb: the frame that was transmitted, owned by mac80211 after this call
3286  */
void ieee80211_tx_status(struct ieee80211_hw *hw,
                          struct sk_buff *skb);

/**
3291  * ieee80211_tx_status_ni - transmit status callback (in process context)
3292  *
3293  * Like ieee80211_tx_status() but can be called in process context.
3294  *
3295  * Calls to this function, ieee80211_tx_status() and
3296  * ieee80211_tx_status_irqsafe() may not be mixed
3297  * for a single hardware.
3298  *
3299  * @hw: the hardware the frame was transmitted by
3300  * @skb: the frame that was transmitted, owned by mac80211 after this call
3301  */
static inline void ieee80211_tx_status_ni(struct ieee80211_hw *hw,
                                           struct sk_buff *skb)
{
	         //local_bh_disable();
	         ieee80211_tx_status(hw, skb);
	         //local_bh_enable();
}

/**
3311  * ieee80211_tx_status_irqsafe - IRQ-safe transmit status callback
3312  *
3313  * Like ieee80211_tx_status() but can be called in IRQ context
3314  * (internally defers to a tasklet.)
3315  *
3316  * Calls to this function, ieee80211_tx_status() and
3317  * ieee80211_tx_status_ni() may not be mixed for a single hardware.
3318  *
3319  * @hw: the hardware the frame was transmitted by
3320  * @skb: the frame that was transmitted, owned by mac80211 after this call
3321  */
void ieee80211_tx_status_irqsafe(struct ieee80211_hw *hw,
                                  struct sk_buff *skb);

/**
3326  * ieee80211_report_low_ack - report non-responding station
3327  *
3328  * When operating in AP-mode, call this function to report a non-responding
3329  * connected STA.
3330  *
3331  * @sta: the non-responding connected sta
3332  * @num_packets: number of packets sent to @sta without a response
3333  */
void ieee80211_report_low_ack(struct ieee80211_sta *sta, uint32_t num_packets);

/**
3337  * ieee80211_beacon_get_tim - beacon generation function
3338  * @hw: pointer obtained from ieee80211_alloc_hw().
3339  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3340  * @tim_offset: pointer to variable that will receive the TIM IE offset.
3341  *      Set to 0 if invalid (in non-AP modes).
3342  * @tim_length: pointer to variable that will receive the TIM IE length,
3343  *      (including the ID and length bytes!).
3344  *      Set to 0 if invalid (in non-AP modes).
3345  *
3346  * If the driver implements beaconing modes, it must use this function to
3347  * obtain the beacon frame/template.
3348  *
3349  * If the beacon frames are generated by the host system (i.e., not in
3350  * hardware/firmware), the driver uses this function to get each beacon
3351  * frame from mac80211 -- it is responsible for calling this function
3352  * before the beacon is needed (e.g. based on hardware interrupt).
3353  *
3354  * If the beacon frames are generated by the device, then the driver
3355  * must use the returned beacon as the template and change the TIM IE
3356  * according to the current DTIM parameters/TIM bitmap.
3357  *
3358  * The driver is responsible for freeing the returned skb.
3359  *
3360  * Return: The beacon template. %NULL on error.
3361  */
struct sk_buff *_ieee80211_beacon_get_tim(struct ieee80211_hw *hw,
                                          struct ieee80211_vif *vif,
                                          uint16_t *tim_offset, 
										  uint16_t *tim_length);

/**
3367  * ieee80211_beacon_get - beacon generation function
3368  * @hw: pointer obtained from ieee80211_alloc_hw().
3369  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3370  *
3371  * See ieee80211_beacon_get_tim().
3372  *
3373  * Return: See ieee80211_beacon_get_tim().
3374  */
static inline struct sk_buff *ieee80211_beacon_get(struct ieee80211_hw *hw,
                                                    struct ieee80211_vif *vif)
{
	         return _ieee80211_beacon_get_tim(hw, vif, NULL, NULL);
}

/**
3382  * ieee80211_csa_finish - notify mac80211 about channel switch
3383  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3384  *
3385  * After a channel switch announcement was scheduled and the counter in this
3386  * announcement hit zero, this function must be called by the driver to
3387  * notify mac80211 that the channel can be changed.
3388  */
void ieee80211_csa_finish(struct ieee80211_vif *vif);

/**
3392  * ieee80211_csa_is_complete - find out if counters reached zero
3393  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3394  *
3395  * This function returns whether the channel switch counters reached zero.
3396  */
bool ieee80211_csa_is_complete(struct ieee80211_vif *vif);


/**
3401  * ieee80211_proberesp_get - retrieve a Probe Response template
3402  * @hw: pointer obtained from ieee80211_alloc_hw().
3403  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3404  *
3405  * Creates a Probe Response template which can, for example, be uploaded to
3406  * hardware. The destination address should be set by the caller.
3407  *
3408  * Can only be called in AP mode.
3409  *
3410  * Return: The Probe Response template. %NULL on error.
3411  */
struct sk_buff *ieee80211_proberesp_get(struct ieee80211_hw *hw,
                                         struct ieee80211_vif *vif);

/**
3416  * ieee80211_pspoll_get - retrieve a PS Poll template
3417  * @hw: pointer obtained from ieee80211_alloc_hw().
3418  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3419  *
3420  * Creates a PS Poll a template which can, for example, uploaded to
3421  * hardware. The template must be updated after association so that correct
3422  * AID, BSSID and MAC address is used.
3423  *
3424  * Note: Caller (or hardware) is responsible for setting the
3425  * &IEEE80211_FCTL_PM bit.
3426  *
3427  * Return: The PS Poll template. %NULL on error.
3428  */
struct sk_buff *ieee80211_pspoll_get(struct ieee80211_hw *hw,
                                      struct ieee80211_vif *vif);

/**
3433  * ieee80211_nullfunc_get - retrieve a nullfunc template
3434  * @hw: pointer obtained from ieee80211_alloc_hw().
3435  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3436  *
3437  * Creates a Nullfunc template which can, for example, uploaded to
3438  * hardware. The template must be updated after association so that correct
3439  * BSSID and address is used.
3440  *
3441  * Note: Caller (or hardware) is responsible for setting the
3442  * &IEEE80211_FCTL_PM bit as well as Duration and Sequence Control fields.
3443  *
3444  * Return: The nullfunc template. %NULL on error.
3445  */
struct sk_buff *ieee80211_nullfunc_get(struct ieee80211_hw *hw,
                                        struct ieee80211_vif *vif);

/**
3450  * ieee80211_probereq_get - retrieve a Probe Request template
3451  * @hw: pointer obtained from ieee80211_alloc_hw().
3452  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3453  * @ssid: SSID buffer
3454  * @ssid_len: length of SSID
3455  * @tailroom: tailroom to reserve at end of SKB for IEs
3456  *
3457  * Creates a Probe Request template which can, for example, be uploaded to
3458  * hardware.
3459  *
3460  * Return: The Probe Request template. %NULL on error.
3461  */
struct sk_buff *ieee80211_probereq_get(struct ieee80211_hw *hw,
                                        struct ieee80211_vif *vif,
                                        const uint8_t *ssid, size_t ssid_len,
                                        size_t tailroom);
						
/**
3468  * ieee80211_rts_get - RTS frame generation function
3469  * @hw: pointer obtained from ieee80211_alloc_hw().
3470  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3471  * @frame: pointer to the frame that is going to be protected by the RTS.
3472  * @frame_len: the frame length (in octets).
3473  * @frame_txctl: &struct ieee80211_tx_info of the frame.
3474  * @rts: The buffer where to store the RTS frame.
3475  *
3476  * If the RTS frames are generated by the host system (i.e., not in
3477  * hardware/firmware), the low-level driver uses this function to receive
3478  * the next RTS frame from the 802.11 code. The low-level is responsible
3479  * for calling this function before and RTS frame is needed.
3480  */
void ieee80211_rts_get(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
                        const void *frame, size_t frame_len,
                        const struct ieee80211_tx_info *frame_txctl,
                        struct ieee80211_rts *rts);

/**
3487  * ieee80211_rts_duration - Get the duration field for an RTS frame
3488  * @hw: pointer obtained from ieee80211_alloc_hw().
3489  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3490  * @frame_len: the length of the frame that is going to be protected by the RTS.
3491  * @frame_txctl: &struct ieee80211_tx_info of the frame.
3492  *
3493  * If the RTS is generated in firmware, but the host system must provide
3494  * the duration field, the low-level driver uses this function to receive
3495  * the duration field value in little-endian byteorder.
3496  *
3497  * Return: The duration.
3498  */
le16_t ieee80211_rts_duration(struct ieee80211_hw *hw,
                               struct ieee80211_vif *vif, size_t frame_len,
                               const struct ieee80211_tx_info *frame_txctl);
/**
3504  * ieee80211_ctstoself_get - CTS-to-self frame generation function
3505  * @hw: pointer obtained from ieee80211_alloc_hw().
3506  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3507  * @frame: pointer to the frame that is going to be protected by the CTS-to-self.
3508  * @frame_len: the frame length (in octets).
3509  * @frame_txctl: &struct ieee80211_tx_info of the frame.
3510  * @cts: The buffer where to store the CTS-to-self frame.
3511  *
3512  * If the CTS-to-self frames are generated by the host system (i.e., not in
3513  * hardware/firmware), the low-level driver uses this function to receive
3514  * the next CTS-to-self frame from the 802.11 code. The low-level is responsible
3515  * for calling this function before and CTS-to-self frame is needed.
3516  */
void ieee80211_ctstoself_get(struct ieee80211_hw *hw,
                              struct ieee80211_vif *vif,
                              const void *frame, size_t frame_len,
                              const struct ieee80211_tx_info *frame_txctl,
                              struct ieee80211_cts *cts);

/**
3524  * ieee80211_ctstoself_duration - Get the duration field for a CTS-to-self frame
3525  * @hw: pointer obtained from ieee80211_alloc_hw().
3526  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3527  * @frame_len: the length of the frame that is going to be protected by the CTS-to-self.
3528  * @frame_txctl: &struct ieee80211_tx_info of the frame.
3529  *
3530  * If the CTS-to-self is generated in firmware, but the host system must provide
3531  * the duration field, the low-level driver uses this function to receive
3532  * the duration field value in little-endian byteorder.
3533  *
3534  * Return: The duration.
3535  */
le16_t ieee80211_ctstoself_duration(struct ieee80211_hw *hw,
                                     struct ieee80211_vif *vif,
                                     size_t frame_len,
                                     const struct ieee80211_tx_info *frame_txctl);

/**
3542  * ieee80211_generic_frame_duration - Calculate the duration field for a frame
3543  * @hw: pointer obtained from ieee80211_alloc_hw().
3544  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3545  * @band: the band to calculate the frame duration on
3546  * @frame_len: the length of the frame.
3547  * @rate: the rate at which the frame is going to be transmitted.
3548  *
3549  * Calculate the duration field of some generic frame, given its
3550  * length and transmission rate (in 100kbps).
3551  *
3552  * Return: The duration.
3553  */
le16_t ieee80211_generic_frame_duration(struct ieee80211_hw *hw,
                                         struct ieee80211_vif *vif,
                                         enum ieee80211_band band,
                                         size_t frame_len,
                                         struct ieee80211_rate *rate);

/**
3561  * ieee80211_get_buffered_bc - accessing buffered broadcast and multicast frames
3562  * @hw: pointer as obtained from ieee80211_alloc_hw().
3563  * @vif: &struct ieee80211_vif pointer from the add_interface callback.
3564  *
3565  * Function for accessing buffered broadcast and multicast frames. If
3566  * hardware/firmware does not implement buffering of broadcast/multicast
3567  * frames when power saving is used, 802.11 code buffers them in the host
3568  * memory. The low-level driver uses this function to fetch next buffered
3569  * frame. In most cases, this is used when generating beacon frame.
3570  *
3571  * Return: A pointer to the next buffered skb or NULL if no more buffered
3572  * frames are available.
3573  *
3574  * Note: buffered frames are returned only after DTIM beacon frame was
3575  * generated with ieee80211_beacon_get() and the low-level driver must thus
3576  * call ieee80211_beacon_get() first. ieee80211_get_buffered_bc() returns
3577  * NULL if the previous generated beacon was not DTIM, so the low-level driver
3578  * does not need to check for DTIM beacons separately and should be able to
3579  * use common code for all beacons.
3580  */
struct sk_buff *
ieee80211_get_buffered_bc(struct ieee80211_hw *hw, struct ieee80211_vif *vif);
										
										
										








































#define IEEE80211_NUM_ACS       4







static inline bool
conf_is_ht40(struct ieee80211_conf *conf)
{
	return conf->chandef.width == NL80211_CHAN_WIDTH_40;
}

static inline bool
conf_is_ht(struct ieee80211_conf *conf)
{
	return conf->chandef.width != NL80211_CHAN_WIDTH_20_NOHT;
}


#if 0
struct wiphy {
	         /* assign these fields before you register the wiphy */
	
	         /* permanent MAC address(es) */
	         uint8_t perm_addr[ETH_ALEN];
	         uint8_t addr_mask[ETH_ALEN];
	
	         struct mac_address *addresses;
	
	         const struct ieee80211_txrx_stypes *mgmt_stypes;
	
	         const struct ieee80211_iface_combination *iface_combinations;
	         int n_iface_combinations;
	         uint16_t software_iftypes;
	
	         uint16_t n_addresses;
	
	         /* Supported interface modes, OR together BIT(NL80211_IFTYPE_...) */
	         uint16_t interface_modes;
	
	         uint16_t max_acl_mac_addrs;
	
	         uint32_t flags, features;
	
	         uint32_t ap_sme_capa;
	
	         enum cfg80211_signal_type signal_type;
	
	         int bss_priv_size;
	         uint8_t max_scan_ssids;
	         uint8_t max_sched_scan_ssids;
	         uint8_t max_match_sets;
	         uint16_t max_scan_ie_len;
	         uint16_t max_sched_scan_ie_len;
	
	         int n_cipher_suites;
	         const uint32_t *cipher_suites;
	
	         uint8_t retry_short;
	         uint8_t retry_long;
	         uint32_t frag_threshold;
	         uint32_t rts_threshold;
	         uint8_t coverage_class;
	
	         char fw_version[ETHTOOL_FWVERS_LEN];
	         uint32_t hw_version;
	
	#ifdef CONFIG_PM
	         const struct wiphy_wowlan_support *wowlan;
	         struct cfg80211_wowlan *wowlan_config;
	#endif
	
	         uint16_t max_remain_on_channel_duration;
	
	         uint8_t max_num_pmkids;
	
	         uint32_t available_antennas_tx;
	         uint32_t available_antennas_rx;
	
	         /*
	2850          * Bitmap of supported protocols for probe response offloading
	2851          * see &enum nl80211_probe_resp_offload_support_attr. Only valid
	2852          * when the wiphy flag @WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD is set.
	2853          */
	         uint32_t probe_resp_offload;
	
	         const uint8_t *extended_capabilities, *extended_capabilities_mask;
	         uint8_t extended_capabilities_len;
	
	         /* If multiple wiphys are registered and you're handed e.g.
	2860          * a regular netdev with assigned ieee80211_ptr, you won't
	2861          * know whether it points to a wiphy your driver has registered
	2862          * or not. Assign this to something global to your driver to
	2863          * help determine whether you own this wiphy or not. */
	         const void *privid;
	
	         struct ieee80211_supported_band *bands[IEEE80211_NUM_BANDS];
	
	         /* Lets us get back the wiphy on the callback */
	         void (*reg_notifier)(struct wiphy *wiphy,
	                              struct regulatory_request *request);
	
	         /* fields below are read-only, assigned by cfg80211 */
	
	         // XXX const struct ieee80211_regdomain __rcu *regd;
	
	         /* the item in /sys/class/ieee80211/ points to this,
	2877          * you need use set_wiphy_dev() (see below) */
	         // XXX struct device dev;
	
	         /* protects ->resume, ->suspend sysfs callbacks against unregister hw */
	         bool registered;
	
	         /* dir in debugfs: ieee80211/<wiphyname> */
	         // struct dentry *debugfsdir;
	
	         const struct ieee80211_ht_cap *ht_capa_mod_mask;
	         const struct ieee80211_vht_cap *vht_capa_mod_mask;
	
	 #ifdef CONFIG_NET_NS
	         /* the network namespace this phy lives in currently */
	         struct net *_net;
	#endif
	
	#ifdef CONFIG_CFG80211_WEXT
	         const struct iw_handler_def *wext;
	#endif
	
	const struct wiphy_coalesce_support *coalesce;
	
	char priv[0] __attribute__((aligned(NETDEV_ALIGN)));
};
#endif 



#endif /* MAC80211_H_ */

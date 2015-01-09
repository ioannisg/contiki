/*
 * ar9170.h
 *
 * Created: 1/28/2014 2:48:01 PM
 *  Author: Ioannis Glaropoulos
 */ 
#ifndef AR9170_H_
#define AR9170_H_


#include "list.h"
//#include "ar9170-drv.h"
#include "ar9170_wlan.h"
#include "ath.h"
#include "ar9170_fw.h"
#include "ar9170_fwdesc.h"
#include "ieee80211\cfg80211.h"
#include "ar9170_fwcmd.h"
#include "memb.h"
#include "ar9170_eeprom.h"
#include "ieee80211\mac80211.h"
#include "ar9170_hw.h"
#include "ar9170_psm.h"
#include "ieee80211_ibss.h"
#include "usb.h"

/* Debug pins */
#define PRE_TBTT_ACTIVE_PIN			54
#define PRE_TBTT_PASSIVE_PIN		55
#define DOZE_ACTIVE_PIN				56
#define DOZE_PASSIVE_PIN			57
#define TX_ACTIVE_PIN				58
#define TX_PASSIVE_PIN				59

#define AR9170FW_NAME "carl9170-1.fw"

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))


#define BIT(nr)				(1UL << (nr)) 
#define BITS(_start, _end)	((BIT(_end) - BIT(_start)) + BIT(_end))
#define BITS_PER_BYTE		8
#define BITS_PER_LONG		(BITS_PER_BYTE)*sizeof(long)

 
enum ar9170_dev_state {
	AR9170_UNKNOWN_STATE,
	AR9170_STOPPED,
	AR9170_IDLE,
	AR9170_STARTED,
};

#define CHK_DEV_STATE(a, s)     (((struct ar9170 *)a)->state >= (s))
#define IS_INITIALIZED(a)       (CHK_DEV_STATE(a, AR9170_STOPPED))
#define IS_ACCEPTING_CMD(a)     (CHK_DEV_STATE(a, AR9170_IDLE))
#define IS_STARTED(a)           (CHK_DEV_STATE(a, AR9170_STARTED))



enum ar9170_ps_off_override_reasons {
	PS_OFF_VIF	= BIT(0),
	PS_OFF_BCN	= BIT(1),
	PS_OFF_ATIM = BIT(2),
	PS_OFF_DATA = BIT(3),
};

/* Transfer size settings */
#define AR9170_USB_REG_MAX_BUF_SIZE				64	
#define AR9170_CMD_HDR_LEN						4
#define AR9170_CMD_MAX_PAYLOAD_LEN				(AR9170_USB_REG_MAX_BUF_SIZE) - (AR9170_CMD_HDR_LEN)

/* SCAN Modes */
#define AR9170_IBSS_SCAN_MODE					0x01

#define PAYLOAD_MAX     (AR9170_MAX_CMD_LEN / 4 - 1)

#define WME_BA_BMP_SIZE                 64
#define AR9170_TX_USER_RATE_TRIES     3

#define AR9170_QUEUE_TIMEOUT          256
#define AR9170_BUMP_QUEUE             1000
#define AR9170_TX_TIMEOUT             2500
#define AR9170_JANITOR_DELAY          128
#define AR9170_QUEUE_STUCK_TIMEOUT    5500
#define ARL9170_STAT_WORK             30000

#define AR9170_NUM_TX_AGG_MAX         30

#define AR9170_BAW_BITS (2 * WME_BA_BMP_SIZE)
#define AR9170_BAW_SIZE (BITS_TO_LONGS(AR9170_BAW_BITS))
#define AR9170_BAW_LEN (DIV_ROUND_UP(AR9170_BAW_BITS, BITS_PER_BYTE))

/*
151  * Tradeoff between stability/latency and speed.
152  *
153  * AR9170_TXQ_DEPTH is devised by dividing the amount of available
154  * tx buffers with the size of a full ethernet frame + overhead.
155  *
156  * Naturally: The higher the limit, the faster the device CAN send.
157  * However, even a slight over-commitment at the wrong time and the
158  * hardware is doomed to send all already-queued frames at suboptimal
159  * rates. This in turn leads to an enormous amount of unsuccessful
160  * retries => Latency goes up, whereas the throughput goes down. CRASH!
161  */
#define AR9170_NUM_TX_LIMIT_HARD      ((AR9170_TXQ_DEPTH * 3) / 2)
#define AR9170_NUM_TX_LIMIT_SOFT      (AR9170_TXQ_DEPTH)

static const uint8_t ar9170_qmap[__AR9170_NUM_TXQ] = { 3, 2, 1, 0 };


enum ar9170_erp_modes {
	         AR9170_ERP_INVALID,
	         AR9170_ERP_AUTO,
	         AR9170_ERP_MAC80211,
	         AR9170_ERP_OFF,
	         AR9170_ERP_CTS,
	         AR9170_ERP_RTS,
	         __AR9170_ERP_NUM,
};

#define CARL9170_FILL_QUEUE(queue, ai_fs, cwmin, cwmax, _txop)          \
do {                                                                    \
	queue.aifs = ai_fs;                                             \
	queue.cw_min = cwmin;                                           \
	queue.cw_max = cwmax;                                           \
	queue.txop = _txop;                                             \
} while (0)


struct ar9170_tx_queue_stats {
	unsigned int count;
	unsigned int limit;
	unsigned int len;
};

struct ar9170_vif {
	unsigned int id;
	struct ieee80211_vif *vif;
};

struct ar9170_vif_info {
	//struct list_head list;
	uint8_t active;
	unsigned int id;
	struct ieee80211_pkt_buf *beacon;
	uint8_t enable_beacon;
};

struct ar9170_cmd_buffer;
struct ar9170_cmd_buffer {
	struct ar9170_cmd_buffer *next;
	uint8_t cmd_len;
	COMPILER_WORD_ALIGNED uint8_t cmd_data[USB_INT_EP_MAX_SIZE];
};
typedef struct ar9170_cmd_buffer ar9170_cmd_buf_t;

/** AR9170 lock semaphore */
typedef volatile uint8_t ar9170_lock_t;

struct ar9170 {
	
	struct ath_common common;
	/* pointers to IEEE802.11 structures */
	struct ieee80211_hw *hw;
	struct ieee80211_vif *vif;
	
	struct ar9170_vif_info *vif_info;
	uint8_t registered;
		
	/* interface mode settings */
	//struct _list_head vif_list;
	unsigned long vif_bitmap;
	unsigned int vifs;
	struct ar9170_vif vif_priv[AR9170_MAX_VIRTUAL_MAC];


	/** Device state */
	enum ar9170_dev_state state;
	
	/* Network state */
	uint8_t is_joined;
	
	/** Lock semaphores */
	ar9170_lock_t tx_wait;
	ar9170_lock_t cmd_wait;
	ar9170_lock_t cmd_only_thread;
	ar9170_lock_t boot_wait;
	
	/** Driver queues */
	list_t ar9170_tx_list;
	list_t ar9170_rx_list;
	list_t ar9170_cmd_list;
	list_t ar9170_tx_pending_list;
	
	struct ieee80211_tx_queue_params edcf[5];
	
	/* qos queue settings */
	struct ar9170_tx_queue_stats tx_stats[__AR9170_NUM_TXQ];
	
	/** ---- CMD  ---- */
	struct memb ar9170_cmd_buf_mem;
		
	/* Command Sequence Number*/
	int cmd_seq;	
	int readlen;
	uint8_t *readbuf;
	/* TX Command buffer */
	ar9170_cmd_buf_t cmd_buffer;	
	/* RX Response buffer */
	ar9170_cmd_buf_t rsp_buf;
	
	union {
		le32_t cmd_buf[PAYLOAD_MAX + 1];
		struct ar9170_cmd cmd;
		struct ar9170_rsp rsp;
	};

	/* EEPROM */
	struct ar9170_eeprom eeprom;
	
	/** RX Filtering */
	uint8_t clear_rx_filter;
	
	/** ---- Beaconing ---- */
	unsigned int pretbtt;
	unsigned int bcn_interval;
	unsigned int bcn_enabled;
	unsigned int bcn_ctrl;
	unsigned int bcn_cancel;
	
	unsigned int global_pretbtt;
	unsigned int global_beacon_int;
	struct ar9170_vif_info  *beacon_iter;
	unsigned int beacon_enabled;
	
	/* cryptographic engine */
	uint64_t usedkeys;
	uint8_t rx_software_decryption;
	uint8_t disable_offload;
	
	/* firmware settings */
	ar9170_lock_t fw_load_wait;
	ar9170_lock_t fw_boot_wait;
	struct {
		const struct ar9170fw_desc_head *desc;
		const struct firmware *fw;
		unsigned int offset;
		unsigned int address;
		unsigned int cmd_bufs;
		unsigned int api_version;
		unsigned int vif_num;
		unsigned int err_counter;
		unsigned int bug_counter;
		uint32_t beacon_addr;
		unsigned int beacon_max_len;
		uint8_t rx_stream;
		uint8_t tx_stream;
		uint8_t rx_filter;
		uint8_t hw_counters;
		unsigned int mem_blocks;
		unsigned int mem_block_size;
		unsigned int rx_size;
		unsigned int tx_seq_table;
		uint8_t ba_filter;
		uint8_t disable_offload_fw;
	} fw;
	
	/* interface configuration combinations */
	struct ieee80211_iface_limit if_comb_limits[1];
	struct ieee80211_iface_combination if_combs[1];
			
	/* internal memory mgmt */
	unsigned long *mem_bitmap;
	int mem_free_blocks;
	int mem_allocs;	
	
	/* Statistics */
	unsigned int tx_dropped;
	unsigned int tx_ack_failures;
	unsigned int tx_fcs_errors;
	unsigned int rx_dropped;
	
	/* tx_ampdu */
	int tx_ampdu_upload;
	int tx_ampdu_scheduler;
	int tx_total_pending;
	int tx_total_queued;
	
	/* rstream mpdu merge */
	struct ar9170_rx_head rx_plcp;
	uint8_t rx_has_plcp;
	uint32_t ampdu_ref;
	
	/* RX Filter */
	struct {
		uint8_t mode;
	} rx_filter;
	
	/* PSM */
	struct {
		uint8_t mode;
		unsigned int dtim_counter;
		unsigned long last_beacon;
		uint64_t last_action;
		uint64_t last_slept;
		unsigned int sleep_ms;
		uint8_t off_override;
		uint8_t state;
	} ps;
	
	/* PSM Manager */
	struct psm_manager psm_mgr;
	
	/* MAC */	
	enum ar9170_erp_modes erp_mode;
	
	/* PHY */
	struct ieee80211_channel *channel;
	unsigned int num_channels;
	int noise[4];
	uint8_t heavy_clip;
	uint8_t ht_settings;
	unsigned int chan_fail;
	unsigned int total_chan_fail;
	
	struct {
		uint64_t active;     /* usec */
		uint64_t cca;        /* usec */
		uint64_t tx_time;    /* usec */
		uint64_t rx_total;
		uint64_t rx_overrun;
	} tally;
	
	/* power calibration data */
	uint8_t power_5G_leg[4];
	uint8_t power_2G_cck[4];
	uint8_t power_2G_ofdm[4];
	uint8_t power_5G_ht20[8];
	uint8_t power_5G_ht40[8];
	uint8_t power_2G_ht20[8];
	uint8_t power_2G_ht40[8];
	
	struct survey_info *survey;
	
	/* filtering settings */
	/* filter settings */
	uint64_t cur_mc_hash;
	uint32_t cur_filter;
	unsigned int filter_state;
	unsigned int rx_filter_caps;
	uint8_t sniffer_enabled;
	
};

extern struct ar9170 ar9170_dev;

static inline void 
__ar9170_set_state(struct ar9170 *ar, enum ar9170_dev_state newstate)
{
	ar->state = newstate;
}

static inline void 
ar9170_set_state(struct ar9170 *ar, enum ar9170_dev_state newstate)
{
	cpu_irq_enter_critical();
	__ar9170_set_state(ar, newstate);
	cpu_irq_leave_critical();
}

static inline void 
ar9170_set_state_when(struct ar9170 *ar, enum ar9170_dev_state min,
	enum ar9170_dev_state newstate)
{
	cpu_irq_enter_critical();
	if (CHK_DEV_STATE(ar, min))
		__ar9170_set_state(ar, newstate);
		cpu_irq_leave_critical();
}



/** Exported API **/
void 
ar9170_set_up_interface(struct ar9170* ar9170_dev, const struct firmware *fw);
void 
ar9170_rx(struct ar9170* ar, list_t rx_list, uint8_t* buffer, uint32_t len);
void 
ar9170_handle_command_response(struct ar9170 *ar, void *buf, uint32_t len);
int
ar9170_parse_firmware(struct ar9170 *ar);

int 
__ar9170_exec_cmd(struct ar9170 *ar, struct ar9170_cmd *cmd, const uint8_t free_buf);
int 
___ar9170_exec_cmd(struct ar9170 *ar, struct ar9170_cmd *cmd, const uint8_t free_buf);

int 
ar9170_exec_cmd(struct ar9170 *ar, const enum ar9170_cmd_oids cmd, unsigned int plen, void *payload, unsigned int outlen, void *out);

int 
ar9170_register(struct ar9170 *ar);


int ar9170_led_init(struct ar9170 *ar);
 
void ar9170_usb_firmware_finish(struct ar9170 *ar);
int ar9170_usb_open(struct ar9170 *ar);
 
 
int ar9170_op_start( struct ar9170* ar9170_dev );
uint8_t ar9170_op_add_interface(struct ieee80211_hw * hw, struct ieee80211_vif * vif);
struct ieee80211_vif* ar9170_get_main_vif(struct ar9170 *ar);
void ar9170_op_bss_info_changed(struct ieee80211_hw *hw, struct ieee80211_vif *vif, struct ieee80211_bss_conf *bss_conf, uint32_t changed);
int ar9170_op_config(struct ieee80211_hw *hw, uint32_t changed); 
 
int ar9170_set_mac_rates(struct ar9170 *ar);
int ar9170_set_qos(struct ar9170 *ar);
int ar9170_init_mac(struct ar9170 *ar);
int ar9170_mod_virtual_mac(struct ar9170 *ar, const unsigned int id, const uint8_t *mac);
int ar9170_set_operating_mode(struct ar9170 *ar);
int ar9170_set_hwretry_limit(struct ar9170 *ar, const unsigned int max_retry);
int ar9170_disable_key(struct ar9170 *ar, const uint8_t id);
int ar9170_set_mac_tpc(struct ar9170 *ar, struct ieee80211_channel *channel);
int ar9170_upload_key(struct ar9170 *ar, const uint8_t id, const uint8_t *mac, const uint8_t ktype, const uint8_t keyidx, const uint8_t *keydata, const int keylen);
int ar9170_set_slot_time(struct ar9170 *ar);
int ar9170_set_beacon_timers(struct ar9170 *ar);
int ar9170_set_rts_cts_rate(struct ar9170 *ar);
int ar9170_set_dyn_sifs_ack(struct ar9170 *ar);
int ar9170_update_multicast(struct ar9170 *ar, const uint64_t mc_hash);
int ar9170_update_multicast_mine(struct ar9170 *ar, const uint32_t mc_hash_high, const U32 mc_hash_low);
int ar9170_op_conf_tx(struct ieee80211_hw *hw, struct ieee80211_vif *vif, uint16_t queue, const struct ieee80211_tx_queue_params *param);
void ar9170_op_flush(struct ieee80211_hw *hw, bool drop);
int ar9170_update_beacon(struct ar9170 * ar, uint8_t submit);
int ar9170_set_channel(struct ar9170 *ar, struct ieee80211_channel *channel, enum nl80211_channel_type _bw);
int ar9170_get_noisefloor(struct ar9170 *ar);

int ar9170_ps_update(struct ar9170 *ar);

void ar9170_tx_set_tx_rates(struct ieee80211_tx_rate *rate);
void ar9170_tx_process_status(struct ar9170 *ar, const struct ar9170_rsp *cmd);
int ar9170_op_tx(struct ieee80211_hw *hw, struct ieee80211_tx_control *control, 
	struct ieee80211_pkt_buf *skb);
extern struct ieee80211_rate __carl9170_ratetable[];

uint64_t ar9170_op_get_tsf(struct ieee80211_hw *hw, struct ieee80211_vif *vif);


#endif /* AR9170_H_ */
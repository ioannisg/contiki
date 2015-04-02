#include "ar9170_fwdesc.h"
#include "ar9170.h"
#include "compiler.h"
#include <sys\errno.h>
#include <string.h>
#include "ar9170_hw.h"
#include "ieee80211\mac80211.h"
#include "ieee80211\nl80211.h"
#include "interrupt\interrupt_sam_nvic.h"
#include <math.h>
#include "ar9170_version.h"
/*
 * ar9170_fw.c
 *
 * Created: 2/1/2014 8:48:02 PM
 *  Author: Ioannis Glaropoulos
 */ 
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


static const uint8_t otus_magic[4] = { OTUS_MAGIC };

static const void *ar9170_fw_find_desc(struct ar9170 *ar, const uint8_t descid[4],
        const unsigned int len, const uint8_t compatible_revision)
{
	const struct ar9170fw_desc_head *iter;
	
	carl9170fw_for_each_hdr(iter, ar->fw.desc) {
		if (carl9170fw_desc_cmp(iter, descid, len,compatible_revision))
			return (void *)iter;
		}
	
	/* needed to find the LAST desc */
	if (carl9170fw_desc_cmp(iter, descid, len,compatible_revision))
		return (void *)iter;
	
	return NULL;
}

static int 
ar9170_fw_verify_descs(struct ar9170 *ar,
	const struct ar9170fw_desc_head *head, unsigned int max_len)
{
	const struct ar9170fw_desc_head *pos;
	unsigned long pos_addr, end_addr;
	unsigned int pos_length;
	
	if (max_len < sizeof(*pos))
		return -ENODATA;
	
	max_len = min((unsigned int) CARL9170FW_DESC_MAX_LENGTH, max_len);
	
	pos = head;
	pos_addr = (unsigned long) pos;
	end_addr = pos_addr + max_len;
	
	while (pos_addr < end_addr) {
		if (pos_addr + sizeof(*head) > end_addr)
			return -E2BIG;
		
		pos_length = le16_to_cpu(pos->length);
		
		if (pos_length < sizeof(*head))
			return -EBADMSG;
		
		if (pos_length > max_len)
			return -EOVERFLOW;
		
		if (pos_addr + pos_length > end_addr)
			return -EMSGSIZE;
		
		if (carl9170fw_desc_cmp(pos, LAST_MAGIC,
			CARL9170FW_LAST_DESC_SIZE,
			CARL9170FW_LAST_DESC_CUR_VER))
			return 0;
		
		pos_addr += pos_length;
		pos = (void *)pos_addr;
		max_len -= pos_length;
	}
	return -EINVAL;
}

static void 
ar9170_fw_info(struct ar9170 *ar)
{
	const struct carl9170fw_motd_desc *motd_desc;
	unsigned int str_ver_len;
	uint32_t fw_date;
	
	printf("driver   API: %s 2%03d-%02d-%02d [%d-%d]\n",
		CARL9170FW_VERSION_GIT, CARL9170FW_VERSION_YEAR,
		CARL9170FW_VERSION_MONTH, CARL9170FW_VERSION_DAY,
		AR9170FW_API_MIN_VER, AR9170FW_API_MAX_VER);
	
	motd_desc = ar9170_fw_find_desc(ar, MOTD_MAGIC,
	     sizeof(*motd_desc), CARL9170FW_MOTD_DESC_CUR_VER);
	
	if (motd_desc) {
		str_ver_len = strnlen(motd_desc->release,CARL9170FW_MOTD_RELEASE_LEN);
	
		fw_date = le32_to_cpu(motd_desc->fw_year_month_day);
		
		printf("firmware API: %.*s 2%03d-%02d-%02d\n",
		                          str_ver_len, motd_desc->release,
		                          CARL9170FW_GET_YEAR(fw_date),
		                          CARL9170FW_GET_MONTH(fw_date),
		                          CARL9170FW_GET_DAY(fw_date));
		
		strlcpy(ar->hw->wiphy->fw_version, motd_desc->release,
		                         sizeof(ar->hw->wiphy->fw_version));
							
	}
}

static uint8_t
valid_dma_addr(const uint32_t address)
{
	if (address >= AR9170_SRAM_OFFSET &&
	address < (AR9170_SRAM_OFFSET + AR9170_SRAM_SIZE))
	return 1;
	
	return 0;
}

static uint8_t 
valid_cpu_addr(const uint32_t address)
{
         if (valid_dma_addr(address) || (address >= AR9170_PRAM_OFFSET &&
             address < (AR9170_PRAM_OFFSET + AR9170_PRAM_SIZE)))
                 return 1;

         return 0;
}

static int 
ar9170_fw_checksum(struct ar9170 *ar, const uint8_t *data, size_t len)
{
	return 0;
}

static int
ar9170_fw_tx_sequence(struct ar9170 *ar)
{
	const struct carl9170fw_txsq_desc *txsq_desc;
	
	txsq_desc = ar9170_fw_find_desc(ar, TXSQ_MAGIC, sizeof(*txsq_desc),
	                                           CARL9170FW_TXSQ_DESC_CUR_VER);
	if (txsq_desc) {
		                 ar->fw.tx_seq_table = le32_to_cpu(txsq_desc->seq_table_addr);
		                 if (!valid_cpu_addr(ar->fw.tx_seq_table))
		                         return -EINVAL;
		         } else {
		                 ar->fw.tx_seq_table = 0;
	         }
	
	         return 0;
}

static void 
ar9170_fw_set_if_combinations(struct ar9170 *ar, uint16_t if_comb_types)
{
	         if (ar->fw.vif_num < 2)
	                 return;
	
	         ar->if_comb_limits[0].max = ar->fw.vif_num;
	         ar->if_comb_limits[0].types = if_comb_types;
	
	         ar->if_combs[0].num_different_channels = 1;
	         ar->if_combs[0].max_interfaces = ar->fw.vif_num;
	         ar->if_combs[0].limits = ar->if_comb_limits;
	         ar->if_combs[0].n_limits = ARRAY_SIZE(ar->if_comb_limits);
	
	         ar->hw->wiphy->iface_combinations = ar->if_combs;
	         ar->hw->wiphy->n_iface_combinations = ARRAY_SIZE(ar->if_combs);
	
}

static int 
ar9170_fw(struct ar9170 *ar, const uint8_t *data, size_t len)
{
	const struct carl9170fw_otus_desc *otus_desc;
	int err;
	uint16_t if_comb_types;
	
	err = ar9170_fw_checksum(ar, data, len);
	if (err)
		return err;
	
	otus_desc = ar9170_fw_find_desc(ar, OTUS_MAGIC,
	sizeof(*otus_desc), CARL9170FW_OTUS_DESC_CUR_VER);
	if (!otus_desc) {
		return -ENODATA;
	}
	
	#define SUPP(feat)                                              \
	(ar9170fw_supports(otus_desc->feature_set, feat))
	
	if (!SUPP(CARL9170FW_DUMMY_FEATURE)) {
		printf("invalid firmware descriptor format detected.\n");
		return -EINVAL;
	}
	
	ar->fw.api_version = otus_desc->api_ver;
	
	if (ar->fw.api_version < AR9170FW_API_MIN_VER || ar->fw.api_version > AR9170FW_API_MAX_VER) {
		printf("unsupported firmware api version.\n");
		return -EINVAL;
	}	
	
	if (!SUPP(CARL9170FW_HANDLE_BACK_REQ))
		printf("firmware does not support BAR acknowledgments.\n");
		
	if (!SUPP(CARL9170FW_COMMAND_PHY) || SUPP(CARL9170FW_UNUSABLE) ||
	   !SUPP(CARL9170FW_HANDLE_BACK_REQ)) {
		printf("firmware does not support mandatory features.\n");
		//FIXME XXX XXXreturn -ECANCELED;
	}
	
	if ((log(le32_to_cpu(otus_desc->feature_set)) >= __CARL9170FW_FEATURE_NUM)/log(2)) {
		printf("driver does not support all firmware features.\n");
	}
	
	if (!SUPP(CARL9170FW_COMMAND_CAM)) {
		printf("crypto offloading is disabled by firmware.\n");
		ar->fw.disable_offload_fw = 1;
	}
	
	if (SUPP(CARL9170FW_PSM) && SUPP(CARL9170FW_FIXED_5GHZ_PSM))
	             ar->hw->flags |= IEEE80211_HW_SUPPORTS_PS;
	
	if (!SUPP(CARL9170FW_USB_INIT_FIRMWARE)) {
	    printf("firmware does not provide mandatory interfaces.\n");
		return -EINVAL;
	}
	
	if (SUPP(CARL9170FW_MINIBOOT))
		ar->fw.offset = le16_to_cpu(otus_desc->miniboot_size);
	else
	    ar->fw.offset = 0;
	
	if (SUPP(CARL9170FW_USB_DOWN_STREAM)) {
		ar->hw->extra_tx_headroom += sizeof(struct ar9170_stream);
		ar->fw.tx_stream = 1;
	}
	
	if (SUPP(CARL9170FW_USB_UP_STREAM))
	   ar->fw.rx_stream = 1;
	
	if (SUPP(CARL9170FW_RX_FILTER)) {
		ar->fw.rx_filter = 1;
		ar->rx_filter_caps = FIF_FCSFAIL | FIF_PLCPFAIL |
		      FIF_CONTROL | FIF_PSPOLL | FIF_OTHER_BSS |
		      FIF_PROMISC_IN_BSS;
	}
	
	if (SUPP(CARL9170FW_HW_COUNTERS))
		ar->fw.hw_counters = 1;
	
	if (SUPP(CARL9170FW_WOL))
		// TODO CHECK device_set_wakeup_enable(&ar->udev->dev, true);
	
	if (SUPP(CARL9170FW_RX_BA_FILTER))
		ar->fw.ba_filter = 1;
	
	if_comb_types = BIT(NL80211_IFTYPE_STATION) | BIT(NL80211_IFTYPE_P2P_CLIENT);
	
	ar->fw.vif_num = otus_desc->vif_num;
	ar->fw.cmd_bufs = otus_desc->cmd_bufs;
	ar->fw.address = le32_to_cpu(otus_desc->fw_address);
	ar->fw.rx_size = le16_to_cpu(otus_desc->rx_max_frame_len);
	ar->fw.mem_blocks = min((unsigned int) otus_desc->tx_descs, 0xfe);
	cpu_irq_enter_critical();
	ar->mem_free_blocks = ar->fw.mem_blocks;
	cpu_irq_leave_critical();
	ar->fw.mem_block_size = le16_to_cpu(otus_desc->tx_frag_len);
	
	if (ar->fw.vif_num >= AR9170_MAX_VIRTUAL_MAC || !ar->fw.vif_num ||
	   ar->fw.mem_blocks < 16 || !ar->fw.cmd_bufs ||
	   ar->fw.mem_block_size < 64 || ar->fw.mem_block_size > 512 ||
	   ar->fw.rx_size > 32768 || ar->fw.rx_size < 4096 ||
	   !valid_cpu_addr(ar->fw.address)) {
		printf("firmware shows obvious signs of malicious tampering.\n");
		return -EINVAL;
	}
	
	ar->fw.beacon_addr = le32_to_cpu(otus_desc->bcn_addr);
	ar->fw.beacon_max_len = le16_to_cpu(otus_desc->bcn_len);
	
	if (valid_dma_addr(ar->fw.beacon_addr) && ar->fw.beacon_max_len >=
		AR9170_MAC_BCN_LENGTH_MAX) {
		ar->hw->wiphy->interface_modes |= BIT(NL80211_IFTYPE_ADHOC);
		
		if (SUPP(CARL9170FW_WLANTX_CAB)) {
			if_comb_types |=
			      BIT(NL80211_IFTYPE_AP) |
			      BIT(NL80211_IFTYPE_P2P_GO);
			
			#ifdef CONFIG_MAC80211_MESH
			     if_comb_types |=
				     BIT(NL80211_IFTYPE_MESH_POINT);
			#endif /* CONFIG_MAC80211_MESH */
		}
	}
	
	ar9170_fw_set_if_combinations(ar, if_comb_types);
	
	ar->hw->wiphy->interface_modes |= if_comb_types;
	
	ar->hw->wiphy->flags &= ~WIPHY_FLAG_PS_ON_BY_DEFAULT;
	
	/* As IBSS Encryption is software-based, IBSS RSN is supported. */
	ar->hw->wiphy->flags |= WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
	    WIPHY_FLAG_IBSS_RSN | WIPHY_FLAG_SUPPORTS_TDLS;
	
	#undef SUPPORTED
	return ar9170_fw_tx_sequence(ar);
}

static struct ar9170fw_desc_head *
ar9170_find_fw_desc(struct ar9170 *ar, const uint8_t *fw_data, const size_t len)
{
	int scan = 0, found = 0;
	
	if (!ar9170fw_size_check(len)) {
		printf("firmware size is out of bound [%u].\n",len);
		return NULL;
	}
	
	while (scan < len - sizeof(struct ar9170fw_desc_head)) {
		if (fw_data[scan++] == otus_magic[found])
			found++;
		else
		    found = 0;
		
		if (scan >= len)
		    break;
		
		if (found == sizeof(otus_magic))
		    break;
	}
	
	if (found != sizeof(otus_magic))
		return NULL;
	
	return (void *)&fw_data[scan - found];
}

int 
ar9170_parse_firmware(struct ar9170 *ar)
{
	const struct ar9170fw_desc_head *fw_desc = NULL;
	const struct firmware *fw = ar->fw.fw;
	unsigned long header_offset = 0;
	int err;
	
	if (!fw) {
		printf("ar9170_fw: null\n");
		return -EINVAL;
	}
		
	fw_desc = ar9170_find_fw_desc(ar, fw->fw_data, fw->fw_size);
	
	if (!fw_desc) {
		printf("unsupported firmware.\n");
		return -ENODATA;
	}
	
	header_offset = (unsigned long)fw_desc - (unsigned long)fw->fw_data;
	
	err = ar9170_fw_verify_descs(ar, fw_desc, fw->fw_size - header_offset);
	if (err) {
		printf("damaged firmware (%d).\n", err);
		return err;
	}
	
	ar->fw.desc = fw_desc;
	
	ar9170_fw_info(ar);
	
	err = ar9170_fw(ar, fw->fw_data, fw->fw_size);
	if (err) {
		printf("failed to parse firmware (%d).\n", err);
		return err;
	}
	
	return 0;
}
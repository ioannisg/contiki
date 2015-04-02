#include "compiler.h"
#include "ar9170.h"
/*
 * Shared CARL9170 Header
 *
 * Firmware descriptor format
 *
 * Copyright 2009-2011 Christian Lamparter <chunkeey@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, see
 * http://www.gnu.org/licenses/.
 */
 #ifndef __CARL9170_SHARED_FWDESC_H
 #define __CARL9170_SHARED_FWDESC_H
  
 #define BIT(nr)				(1UL << (nr)) 
  
 /* NOTE: Don't mess with the order of the flags! */
 enum carl9170fw_feature_list {
          /* Always set */
          CARL9170FW_DUMMY_FEATURE,
  
          /*
           * Indicates that this image has special boot block which prevents
           * legacy drivers to drive the firmware.
           */
          CARL9170FW_MINIBOOT,
  
          /* usb registers are initialized by the firmware */
          CARL9170FW_USB_INIT_FIRMWARE,
  
          /* command traps & notifications are send through EP2 */
          CARL9170FW_USB_RESP_EP2,
  
          /* usb download (app -> fw) stream */
          CARL9170FW_USB_DOWN_STREAM,
  
          /* usb upload (fw -> app) stream */
          CARL9170FW_USB_UP_STREAM,
  
          /* unusable - reserved to flag non-functional debug firmwares */
          CARL9170FW_UNUSABLE,
  
          /* AR9170_CMD_RF_INIT, AR9170_CMD_FREQ_START, AR9170_CMD_FREQUENCY */
          CARL9170FW_COMMAND_PHY,
  
          /* AR9170_CMD_EKEY, AR9170_CMD_DKEY */
          CARL9170FW_COMMAND_CAM,
  
          /* Firmware has a software Content After Beacon Queueing mechanism */
          CARL9170FW_WLANTX_CAB,
  
          /* The firmware is capable of responding to incoming BAR frames */
          CARL9170FW_HANDLE_BACK_REQ,
  
          /* GPIO Interrupt | CARL9170_RSP_GPIO */
          CARL9170FW_GPIO_INTERRUPT,
  
          /* Firmware PSM support | CARL9170_CMD_PSM */
          CARL9170FW_PSM,
  
          /* Firmware RX filter | CARL9170_CMD_RX_FILTER */
          CARL9170FW_RX_FILTER,
  
          /* Wake up on WLAN */
          CARL9170FW_WOL,
  
          /* Firmware supports PSM in the 5GHZ Band */
          CARL9170FW_FIXED_5GHZ_PSM,
  
          /* HW (ANI, CCA, MIB) tally counters */
          CARL9170FW_HW_COUNTERS,
  
          /* Firmware will pass BA when BARs are queued */
          CARL9170FW_RX_BA_FILTER,
  
          /* KEEP LAST */
          __CARL9170FW_FEATURE_NUM
};
  
#define OTUS_MAGIC      "OTAR"
#define MOTD_MAGIC      "MOTD"
#define FIX_MAGIC       "FIX\0"
#define DBG_MAGIC       "DBG\0"
#define CHK_MAGIC       "CHK\0"
#define TXSQ_MAGIC      "TXSQ"
#define WOL_MAGIC       "WOL\0"
#define LAST_MAGIC      "LAST"
  
#define CARL9170FW_SET_DAY(d) (((d) - 1) % 31)
#define CARL9170FW_SET_MONTH(m) ((((m) - 1) % 12) * 31)
#define CARL9170FW_SET_YEAR(y) (((y) - 10) * 372)
 
#define CARL9170FW_GET_DAY(d) (((d) % 31) + 1)
#define CARL9170FW_GET_MONTH(m) ((((m) / 31) % 12) + 1)
#define CARL9170FW_GET_YEAR(y) ((y) / 372 + 10)
 
#define CARL9170FW_MAGIC_SIZE                   4
 
struct ar9170fw_desc_head {
	uint8_t      magic[CARL9170FW_MAGIC_SIZE];
	le16_t		length;
	uint8_t		min_ver;
	uint8_t		cur_ver;
} __attribute__((packed));

#define CARL9170FW_DESC_HEAD_SIZE                       \
         (sizeof(struct ar9170fw_desc_head))
 
#define CARL9170FW_OTUS_DESC_MIN_VER            6
#define CARL9170FW_OTUS_DESC_CUR_VER            7
struct carl9170fw_otus_desc {
	struct ar9170fw_desc_head head;
	le32_t feature_set;
	le32_t fw_address;
	le32_t bcn_addr;
	le16_t bcn_len;
	le16_t miniboot_size;
	le16_t tx_frag_len;
	le16_t rx_max_frame_len;
	uint8_t tx_descs;
	uint8_t cmd_bufs;
	uint8_t api_ver;
	uint8_t vif_num;
} __attribute__((packed));

#define CARL9170FW_OTUS_DESC_SIZE                       \
        (sizeof(struct carl9170fw_otus_desc))
 
#define CARL9170FW_MOTD_STRING_LEN                      24
#define CARL9170FW_MOTD_RELEASE_LEN                     20
#define CARL9170FW_MOTD_DESC_MIN_VER                    1
#define CARL9170FW_MOTD_DESC_CUR_VER                    2

struct carl9170fw_motd_desc {
	struct ar9170fw_desc_head head;
    le32_t fw_year_month_day;
	char desc[CARL9170FW_MOTD_STRING_LEN];
	char release[CARL9170FW_MOTD_RELEASE_LEN];
} __attribute__ ((packed));

#define CARL9170FW_MOTD_DESC_SIZE                       \
	(sizeof(struct carl9170fw_motd_desc))
 
#define CARL9170FW_FIX_DESC_MIN_VER                     1
#define CARL9170FW_FIX_DESC_CUR_VER                     2
struct carl9170fw_fix_entry {
	le32_t address;
	le32_t mask;
	le32_t value;
} __attribute__((packed));

struct carl9170fw_fix_desc {
	struct ar9170fw_desc_head head;
    struct carl9170fw_fix_entry data[0];
} __attribute__((packed));

#define CARL9170FW_FIX_DESC_SIZE                        \
         (sizeof(struct carl9170fw_fix_desc))
 
#define CARL9170FW_DBG_DESC_MIN_VER                     1
#define CARL9170FW_DBG_DESC_CUR_VER                     3
struct carl9170fw_dbg_desc {
	struct ar9170fw_desc_head head;
 
	le32_t bogoclock_addr;
	le32_t counter_addr;
	le32_t rx_total_addr;
	le32_t rx_overrun_addr;
	le32_t rx_filter;
	 
	/* Put your debugging definitions here */
} __attribute__((packed));

#define CARL9170FW_DBG_DESC_SIZE                        \
        (sizeof(struct carl9170fw_dbg_desc))

#define CARL9170FW_CHK_DESC_MIN_VER                     1
#define CARL9170FW_CHK_DESC_CUR_VER                     2
struct carl9170fw_chk_desc {
    struct ar9170fw_desc_head head;
    le32_t fw_crc32;
	le32_t hdr_crc32;
} __attribute__((packed));

#define CARL9170FW_CHK_DESC_SIZE                        \
        (sizeof(struct carl9170fw_chk_desc))
 
#define CARL9170FW_TXSQ_DESC_MIN_VER                    1
#define CARL9170FW_TXSQ_DESC_CUR_VER                    1
struct carl9170fw_txsq_desc {
	struct ar9170fw_desc_head head;
 
         le32_t seq_table_addr;
} __attribute__((packed));
#define CARL9170FW_TXSQ_DESC_SIZE                       \
         (sizeof(struct carl9170fw_txsq_desc))
 
#define CARL9170FW_WOL_DESC_MIN_VER                     1
#define CARL9170FW_WOL_DESC_CUR_VER                     1
struct carl9170fw_wol_desc {
         struct ar9170fw_desc_head head;
 
         le32_t supported_triggers;      /* CARL9170_WOL_ */
} __attribute__((packed));
#define CARL9170FW_WOL_DESC_SIZE                        \
         (sizeof(struct carl9170fw_wol_desc))
 
#define CARL9170FW_LAST_DESC_MIN_VER                    1
#define CARL9170FW_LAST_DESC_CUR_VER                    2
struct carl9170fw_last_desc {
         struct ar9170fw_desc_head head;
} __attribute__((packed));
#define CARL9170FW_LAST_DESC_SIZE                       \
         (sizeof(struct carl9170fw_fix_desc))
 
#define CARL9170FW_DESC_MAX_LENGTH                      8192
 
#define CARL9170FW_FILL_DESC(_magic, _length, _min_ver, _cur_ver)       \
         .head = {                                                       \
                 .magic = _magic,                                        \
                 .length = cpu_to_le16(_length),                         \
                 .min_ver = _min_ver,                                    \
                 .cur_ver = _cur_ver,                                    \
         }
 
static inline void carl9170fw_fill_desc(struct ar9170fw_desc_head *head,
                                          uint8_t magic[CARL9170FW_MAGIC_SIZE],
                                          le16_t length, uint8_t min_ver, uint8_t cur_ver)
{
         head->magic[0] = magic[0];
         head->magic[1] = magic[1];
         head->magic[2] = magic[2];
         head->magic[3] = magic[3];
 
         head->length = length;
         head->min_ver = min_ver;
         head->cur_ver = cur_ver;
}
 
#define carl9170fw_for_each_hdr(desc, fw_desc)                          \
         for (desc = fw_desc;                                            \
              memcmp(desc->magic, LAST_MAGIC, CARL9170FW_MAGIC_SIZE) &&  \
              le16_to_cpu(desc->length) >= CARL9170FW_DESC_HEAD_SIZE &&  \
              le16_to_cpu(desc->length) < CARL9170FW_DESC_MAX_LENGTH;    \
              desc = (void *)((unsigned long)desc + le16_to_cpu(desc->length)))
 
#define CHECK_HDR_VERSION(head, _min_ver)                               \
        (((head)->cur_ver < _min_ver) || ((head)->min_ver > _min_ver))  \

static inline uint8_t ar9170fw_supports(le32_t list, uint8_t feature)
{
        return le32_to_cpu(list) & BIT(feature);
}
 
static inline uint8_t carl9170fw_desc_cmp(const struct ar9170fw_desc_head *head,
                                        const uint8_t descid[CARL9170FW_MAGIC_SIZE],
                                        uint16_t min_len, uint8_t compatible_revision)
{
         if (descid[0] == head->magic[0] && descid[1] == head->magic[1] &&
             descid[2] == head->magic[2] && descid[3] == head->magic[3] &&
             !CHECK_HDR_VERSION(head, compatible_revision) &&
             (le16_to_cpu(head->length) >= min_len))
                 return 1;
 
         return 0;
}
 
#define CARL9170FW_MIN_SIZE     32
#define CARL9170FW_MAX_SIZE     16384
 
static inline bool ar9170fw_size_check(unsigned int len)
{
        return (len <= CARL9170FW_MAX_SIZE && len >= CARL9170FW_MIN_SIZE);
}
 
#endif /* __CARL9170_SHARED_FWDESC_H */
 
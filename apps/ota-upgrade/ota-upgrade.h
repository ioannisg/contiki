/*
 * ota_upgrade.h
 *
 * Created: 2014-09-12 14:08:59
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se>
 */ 


#ifndef OTA_UPGRADE_H_
#define OTA_UPGRADE_H_

#include "contiki.h"

#ifdef OTA_UPGRADE_CONF_MAX_FW_LEN
#define OTA_UPGRADE_MAX_FW_LEN OTA_UPGRADE_CONF_MAX_FW_LEN
#else
#define OTA_UPGRADE_MAX_FW_LEN 160000
#endif

enum ota_pkt_type {
  OTA_HDR_START = 0,
  OTA_HDR_DATA,
  OTA_HDR_DATA_ACK,
  OTA_HDR_FIN,
  OTA_HDR_START_ACK,
  OTA_HDR_FIN_ACK,
  OTA_HDR_FIN_NACK,
};

COMPILER_PACK_SET(1);
/** \brief Structure of the application layer header of the OTA upgrade protocol */
typedef struct ota_upgrd_hdr {
  uint8_t type;                /* Frame Control, packet type [START/DATA/ etc.]*/
  uint8_t flash_bank;          /* The flash bank number where FW is to be stored */ 
  uint16_t seqno;              /* The sequence number of the uploaded firmware chunk */
  uint16_t len;                /* Length of payload */
} ota_upgrd_hdr_t;
COMPILER_PACK_RESET();

uint8_t ota_flash_store_chunk(uint8_t * _data, uint16_t data_len);
uint8_t ota_flash_swap(void);
uint8_t ota_flash_get_fw_bank(void);

PROCESS_NAME(ota_upgrade_process);

#endif /* OTA-UPGRADE_H_ */
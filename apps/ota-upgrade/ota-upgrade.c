/*
 * ota_upgrade.c
 *
 * Created: 2014-09-12 14:11:46
 *  Author: Ioannis Glaropoulos <ioannisg@sics.se>
 */ 
#include "apps/ota-upgrade/ota-upgrade.h"
#include "contiki-net.h"
#include "cfs-coffee-arch.h"
#ifdef NET_PROC_H
#include NET_PROC_H
#endif

#define DEBUG 0
#if DEBUG 
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define DEBUG_NETW 0
#if DEBUG_NETW
#include "apps/ota-debug/ota-debug.h"
#define NPRINTF(...) sprintf(ota_debug_buf, __VA_ARGS__); \
otg_debug_init_connection(); \
otg_debug_send_debug_msg()
#else
#define NPRINTF(...)
#endif


#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

/*---------------------------------------------------------------------------*/
PROCESS(ota_upgrade_process, "OTA Upgrade");

#define OTA_UPGRADE_UDP_LOCAL_PORT 1234
#define OTA_UPGRADE_UDP_REMOTE_PORT 4321

typedef enum ota_upgrade_fsm_state {
  OTA_UPGRADE_NULL,
  OTA_UPGRADE_INITIATED,
  OTA_UPGRADE_DOWNLOADING,
  OTA_UPGRADE_FLASHING,
} ota_upgrade_fsm_state_t;

/*---------------------------------------------------------------------------*/
static struct uip_udp_conn *ota_udp_conn;
static ota_upgrade_fsm_state_t ota_fsm_state;
static uint16_t next_seq_num = 0;

static void
ota_upgrade_reset(void)
{
  ota_fsm_state = OTA_UPGRADE_NULL;
  next_seq_num = 0;
}
/*---------------------------------------------------------------------------*/
static uint8_t 
ota_upgrade_fw_check(void)
{
  if (ota_fsm_state != OTA_UPGRADE_DOWNLOADING)
    return 0;
  /* TODO firmware check:
   * Check if version is newer that the current one
	* Do some (required?) security checks
	*/
  return 1;
}
/*---------------------------------------------------------------------------*/
static uint8_t
ota_upgrade_store_segment(uint8_t *_data, uint16_t data_len)
{
  return ota_flash_store_chunk(_data, data_len);
}
/*---------------------------------------------------------------------------*/
static void 
ota_upgrade_process_data(void * _appdata)
{
  /* Get packet payload and process it depending on current state, 
   * followed by state update if required
	*/
  if (uip_datalen() < sizeof(ota_upgrd_hdr_t)) {
    PRINTF("ota_upgrade: err-len\n\r");
    goto err_process;
  }
  ota_upgrd_hdr_t *ota_hdr = (ota_upgrd_hdr_t*)_appdata;
  switch(ota_hdr->type) {
  case OTA_HDR_START:
    if (ota_fsm_state != OTA_UPGRADE_NULL && 
        ota_fsm_state != OTA_UPGRADE_INITIATED) {
      PRINTF("ota_upgrade: start-on-non-null/init %u\n\r",ota_fsm_state);
      goto err_process;
    }
    /* Read the firmware length */
    uint16_t payload_len = ota_hdr->len;
    if (payload_len != sizeof(uint32_t)) {
      PRINTF("ota_upgrade: wrong-payload-len-on-start: %u\n\r", payload_len);
      goto err_process;
    }
    if (uip_datalen() < sizeof(ota_upgrd_hdr_t) + payload_len) {
      PRINTF("ota_upgrade: wrong-start-pkt-len\n\r");
      goto err_process;
    }
	 uint8_t *payload = (((uint8_t *)ota_hdr) + sizeof(ota_upgrd_hdr_t));
	 uint32_t fw_len =  payload[0] + (payload[1] << 8) +
                       (payload[2] << 16) + (payload[3] << 24);

    /* Check for space in flash */
    if (fw_len == 0 || fw_len > OTA_UPGRADE_MAX_FW_LEN) {
      PRINTF("ota_upgrade: wrong-fw-len %u\n\t", (unsigned int)fw_len);
      goto err_process;
    }
    /* Change state to START and reply with START_ACK */
    ota_fsm_state = OTA_UPGRADE_INITIATED;
    ota_hdr->type = OTA_HDR_START_ACK;
	 ota_hdr->flash_bank = ota_flash_get_fw_bank();
    ota_hdr->len = 0;
    ota_hdr->seqno = 0xffff;
    /* Expect zero sequence number */
    next_seq_num = 0;
    uip_datalen() = sizeof(struct ota_upgrd_hdr);
    return;
  case OTA_HDR_DATA:
    if (ota_fsm_state == OTA_UPGRADE_INITIATED) {
      /* Check if sequence number is the expected one */
      if (ota_hdr->seqno != next_seq_num) {
        PRINTF("ota_upgrade: got-wrong-seqno-on-init %u\n\r", ota_hdr->seqno);
        goto err_process;
      }
      ota_fsm_state = OTA_UPGRADE_DOWNLOADING;
    }
    if (ota_fsm_state != OTA_UPGRADE_DOWNLOADING) {
      PRINTF("ota_upgrade: data-pkt-on-non-downl\n\r");
      goto err_process;
	 }
	 /* Check if sequence number is the expected one */
	 if (ota_hdr->seqno != next_seq_num) {
		 PRINTF("ota_upgrade: got-wrong-seqno-on-data %u\n\r", ota_hdr->seqno);
		 /* Re-ack previous */
		 ota_hdr->seqno = next_seq_num-1;
		 goto _send_ack;
	 }
    /* Check length */
    if (ota_hdr->len == 0) {
      PRINTF("ota_upgrade: got-zero-len\n\r");
      goto err_process;
    }
    if (uip_datalen() != sizeof(ota_upgrd_hdr_t) + ota_hdr->len) {
      PRINTF("ota_upgrade: corrupted-data-pkt\n\r");
		goto err_process;
	 }
    /* Save firmware chunk */
	 if(!ota_upgrade_store_segment((((uint8_t *)_appdata) + 
        sizeof(ota_upgrd_hdr_t)),uip_datalen()-sizeof(ota_upgrd_hdr_t))) {
      PRINTF("ota_upgrade: error-storing-fw-chunk\n\r");
      goto err_process;
	 }
    /* Expect next sequence number */
    PRINTF("ota_upgrade: stored %u chunk\n\r",ota_hdr->seqno);
	 next_seq_num++;
_send_ack:
	 /* Prepare reply */
	 uip_datalen() = sizeof(ota_upgrd_hdr_t);
    ota_hdr->type = OTA_HDR_DATA_ACK;
    ota_hdr->len = 0;
    break;
  case OTA_HDR_FIN:
    if (ota_fsm_state != OTA_UPGRADE_DOWNLOADING) {
      PRINTF("ota_upgrade: got-fin-on-err-state\n\r");
      goto err_process; 
	 }
    /* Check status of downloaded firmware */
	 uint8_t result = ota_upgrade_fw_check();
	 if (!result) {
      PRINTF("ota_upgrade: fw-check-err; send-nack\n\t");
      ota_hdr->type = OTA_HDR_FIN_NACK;
    } else {
      ota_fsm_state = OTA_UPGRADE_FLASHING;
      ota_hdr->type = OTA_HDR_FIN_ACK;
    }
	 ota_hdr->len = 0;
	 ota_hdr->seqno = 0;
	 uip_datalen() = sizeof(ota_upgrd_hdr_t);
    break;
  default:
    PRINTF("ota_upgrade: err-type\n\r");
	 goto err_process;
  }
  return;
err_process:
  uip_datalen() = 0;
  return;
}
/*---------------------------------------------------------------------------*/
static uint8_t
ota_upgrade_init(void)
{
  ota_fsm_state = OTA_UPGRADE_NULL;

  /* Open UDP connection for OTG */
  ota_udp_conn = udp_new(NULL, UIP_HTONS(OTA_UPGRADE_UDP_REMOTE_PORT), NULL);
  if (ota_udp_conn == NULL) {
    PRINTF("ota_upgrade_proc: err-open-udp-conn\n");
    return 0;
  } else {
    /* Bind the connection to the local port*/
    udp_bind(ota_udp_conn, UIP_HTONS(OTA_UPGRADE_UDP_LOCAL_PORT));
  }
  /* Initialize flash, if not already */
  uint32_t result = COFFEE_INIT();
  if (result) {
    PRINTF("ota_upgrade: flash-init-err %u\n\r", (unsigned int)result);
    return 0;
  } 
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{  
  if (uip_newdata()) {
	 ota_upgrade_process_data(uip_appdata);
	 
	 /* Reply to client if required */
	 if (uip_datalen()) {
      uip_ipaddr_copy(&ota_udp_conn->ripaddr, &UIP_IP_BUF->srcipaddr);
		uip_udp_packet_send(ota_udp_conn, uip_appdata, uip_datalen());
		uip_create_unspecified(&ota_udp_conn->ripaddr);
	 }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ota_upgrade_process, ev, data)
{
  PROCESS_BEGIN();
  
  UNUSED(data);
  
  while (!process_is_running(&NET_PROC)) {
    PROCESS_PAUSE();
  }

  if(!ota_upgrade_init()) {
    PROCESS_EXIT();
  }
  NPRINTF("ota_upgrade: standby\n");
  while(1) {
    PROCESS_YIELD();
    if (ev == tcpip_event)
      tcpip_handler();
    if (ota_fsm_state == OTA_UPGRADE_FLASHING) {
      NPRINTF("ota_upgrade: reprogramming...\n");
      if (!ota_flash_swap()) {
        PRINTF("ota_upgrade: unsuccessful\n\r");
		}
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
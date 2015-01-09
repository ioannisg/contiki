#include "radio.h"
#include <stdint.h>
#include "xbee.h"
#include "core/lib/list.h"
#include "core/lib/memb.h"
#include "netstack.h"
#include "process.h"
#include "rtimer.h"
#include "xbee-api.h"
#include "watchdog.h"
#include "packetbuf.h"
#include "ringbuf.h"
#include "pend-SV.h"
#include "wire_digital.h"
/*
 * xbee_drv.c
 *
 * Created: 2014-11-28 15:30:23
 *  Author: Ioannis Glaropoulos
 */
PROCESS(xbee_driver_process, "XBEE Driver");

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef XBEE_CONF_RESET_PIN
#define XBEE_RESET_PIN XBEE_CONF_RESET_PIN
#else
#define XBEE_RESET_PIN 14
#endif

#ifndef XBEE_CONF_TX_STATUS_WAIT_TIME
#define XBEE_TX_STATUS_WAIT_TIME XBEE_CONF_TX_STATUS_WAIT_TIME
#else
#define XBEE_TX_STATUS_WAIT_TIME (RTIMER_SECOND / 10)
#endif
#define XBEE_TX_STATUS_WAIT_TIME (RTIMER_SECOND / 10)
/** XBEE event timer for handling command timeout messages */
static struct etimer xbee_etimer;
/** XBEE command send event */
process_event_t xbee_driver_cmd_event, xbee_drv_device_response;
/** XBEE API command sequence number */
static uint8_t xbee_drv_cmd_seqno;
/** XBEE driver flag for pending tx frame / command */
volatile uint8_t xbee_frame_is_pending = 0, xbee_sync_cmd_is_pending = 0;
/** XBEE driver flag for the pending frame ID of a synchronous command */
uint8_t xbee_sync_rsp_pending_id;

/** Outgoing FIFO buffer and length */
static uint8_t tx_pkt_array[XBEE_TX_MAX_FIFO_SIZE];
static uint8_t tx_pkt_array_len = 0;
/** Device status flag */
static uint8_t xbee_dev_is_active = 0;

/** List of pending XBEE commands */
LIST(xbee_cmd_list);
/* MEMB memory block for outgoing XBEE commands */
MEMB(xbee_cmd_mem, xbee_at_command_t, XBEE_MAX_PENDING_CMD_LEN);

static xbee_cmd_status_t
xbee_drv_handle_tx_status_rsp(xbee_at_tx_status_rsp_t *rsp, uint8_t len);
static xbee_cmd_status_t xbee_drv_handle_cmd_rsp(xbee_at_command_rsp_t *rsp,
  uint8_t payload_len);
/*---------------------------------------------------------------------------*/
void
xbee_drv_check_response(void *data)
{
  /* This function is called inside software interrupt context */
  static uint8_t pos = 0;
  static xbee_response_t rsp;
  static uint8_t esc_next = 0;
  
  int c = ringbuf_get((struct ringbuf *)data);
  if (c == -1) {
    PRINTF("xbee-drv: check-rsp-err\n");
    pos = 0;
    return;
  }
  /* A valid character is received */
  switch(pos) {
  case 0:
    /* This should be the SFD. Re-set escape character flag. */
    esc_next = 0;
    if (c != XBEE_SFD_BYTE) {
      pos = 0;
      if (ringbuf_elements((struct ringbuf *)data)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      return;
    }
    pos++;
    break;
  case 1:
    /* This should be zero (MSB) */
    if (c != 0) {
  	   pos = 0;
      PRINTF("xbee-drv: MSB-err:%u\n", c);
      if (ringbuf_elements((struct ringbuf *)data)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      return;
    }
    pos++;
    break;
  case 2:
    /* This should be the LSB of the response length. It could be escaped. */
    if (c == XBEE_ESCAPE_CHAR) {
      if (!esc_next) {
        esc_next = 1;
        if (ringbuf_elements((struct ringbuf *)data)) {
          pend_sv_register_handler(xbee_drv_check_response, data);
        }
        return;
      } else {
        pos = 0;
        esc_next = 0;
        PRINTF("xbee-drv: LSB-err\n");
        if (ringbuf_elements((struct ringbuf *)data)) {
          pend_sv_register_handler(xbee_drv_check_response, data);
        }
        return;
      }
    } else {
      if (!esc_next) {
        rsp.len = c;
      } else {
        esc_next = 0;
        rsp.len = c^XBEE_ESCAPE_XOR;
      }
      pos++;
    }
    break;
  case 3:
    /* This is the API ID. No need to be escaped. */
    if (c == XBEE_AT_CMD_RSP_ID) {
      if (!xbee_sync_cmd_is_pending) {
  	     PRINTF("xbee-drv: no-cmd-is-pending\n");
      } else {
        rsp.api_id = c;
        pos++;
        break;
  	   }
    } else if (c == XBEE_AT_TX_STATUS_RSP_ID) {
  	   if (!xbee_frame_is_pending) {
  	     PRINTF("xbee-drv: no-status-is-pending\n");
      } else {
        pos++;
        rsp.api_id = c;
        break;
      }
    } else if (c == XBEE_AT_RX_FRAME_RSP_ID) {
      /* This is an incoming frame, but we should not handle it inside this
       * software interrupt context.
       */
    } else {
  	   PRINTF("xbee-drv: only-handling-rsp:%02x\n", c);
    }
    pos = 0;
    if (ringbuf_elements((struct ringbuf *)data)) {
      pend_sv_register_handler(xbee_drv_check_response, data);
    }
    return;
  case 4:
    /* This is the frame id and could be escaped. */
    if (c == XBEE_ESCAPE_CHAR) {
      if (!esc_next) {
        esc_next = 1;
        break;
      } else {
        esc_next = 0;
        c = c^XBEE_ESCAPE_XOR;
      }
    } else {
      if (esc_next) {
        c = c^XBEE_ESCAPE_XOR;
      }
    }
    if (xbee_sync_cmd_is_pending && xbee_sync_rsp_pending_id != c) {
  	   PRINTF("xbee-drv: got %u, expecting id:%u\n", c,
        xbee_sync_rsp_pending_id);
      pos = 0;
      if (ringbuf_elements((struct ringbuf *)data)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      return;
    }
    /* Now we have the right response */
    if (rsp.api_id == XBEE_AT_TX_STATUS_RSP_ID) {
      rsp.tx_status.frame_id = c;
	 } else if (rsp.api_id == XBEE_AT_CMD_RSP_ID) {
      rsp.cmd_data.frame_id = c;
    }
    pos++;
    break;
  case 5:
    if (xbee_sync_cmd_is_pending) {
      /* Save the first byte of the Command ID. No escape needed. */
      rsp.cmd_data.cmd_id = c;
	 } else if (xbee_frame_is_pending) {
      /* Save the frame status byte. No escape needed. */
      rsp.tx_status.option = c;
    }
    pos++;
    break;
  case 6:
    if (xbee_sync_cmd_is_pending) {
      /* Save the second byte of the Command ID. No escape needed. */
      rsp.cmd_data.cmd_id += ((uint16_t)c) << 8;
      pos++;
    } else if (xbee_frame_is_pending) {
      /* This is the checksum of the TX status response and could be escaped */
      if (c == XBEE_ESCAPE_CHAR) {
        if (!esc_next) {
          esc_next = 1;
          break;
		  }
      }
      if (xbee_drv_handle_tx_status_rsp(&rsp.tx_status,
        sizeof(xbee_at_tx_status_rsp_t)) == XBEE_STATUS_OK) {
      } else {
        PRINTF("xbee-drv: handle-tx-status-err\n");
      }
      pos = 0;
    }
    break;
  case 7:
    /* Read command response status. No escape needed. */
    if (xbee_sync_cmd_is_pending) {
      rsp.cmd_data.rsp_status = c;
      pos++;
      break;
    } else {
      PRINTF("xbee-drv: too-many-bytes:%u\n", pos);
      pos = 0;
      return;
    }
  default:
    if (!xbee_sync_cmd_is_pending) {
      if (ringbuf_elements((struct ringbuf *)data)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      PRINTF("xbee-drv: too-many-bytes:%u\n", pos);
      pos = 0;
      return;
    }
    if (pos-XBEE_TX_FRAME_HDR_SIZE < rsp.len) {
      /* Escaped characters must be handled */
      if (!esc_next) {
        if (c != XBEE_ESCAPE_CHAR) {
          rsp.cmd_data.value[pos-8] = c;
          pos++; 
        } else {
          esc_next = 1;
		  }
      } else {
        rsp.cmd_data.value[pos-8] = c^XBEE_ESCAPE_XOR;
        esc_next = 0;
        pos++;
      }
    } else {
      /* Done. Drop the checksum. */
      pos = 0;
      /* Process response; return result on the ID flag variable. */
      xbee_sync_rsp_pending_id = 
        xbee_drv_handle_cmd_rsp(&rsp.cmd_data, rsp.len-5);
      break;
    }
  }
  if (ringbuf_elements((struct ringbuf *)data)) {
    pend_sv_register_handler(xbee_drv_check_response, data);
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
xbee_drv_has_pending_operations(void)
{
  return xbee_frame_is_pending | xbee_sync_cmd_is_pending;
}
/*---------------------------------------------------------------------------*/
static uint8_t
compute_checksum(uint8_t* cmd_array, int len)
{
  uint32_t checksum = 0;
  int i;
  for (i=0; i<len; i++) {
    checksum += cmd_array[i];
  }
  uint8_t cksm = (uint8_t)(checksum & 0xff);
  return 0xff - cksm;
}
/*---------------------------------------------------------------------------*/
static int
xbee_init(void)
{
  static uint8_t is_initialized = 0;
  if (!is_initialized) {
    /* Initialize underlying serial driver process */
    xbee_serial_line_init(0);
    /* Initialize command sequence numbering */
    xbee_drv_cmd_seqno = 0;
    /* Initialize outgoing command data structures */
    list_init(xbee_cmd_list);
    memb_init(&xbee_cmd_mem);
    /* Start driver process */
    process_start(&xbee_driver_process, NULL);
    is_initialized++;
  }  
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
xbee_read(void *buf, unsigned short buf_len)
{
  /* Clear the packet buffer in case of existing trash. */
  packetbuf_clear();
  packetbuf_set_datalen(buf_len);
  /* Copy to packet buffer */
  if (packetbuf_copyfrom(buf, buf_len) == 0) {
    PRINTF("xbee-drv: copy-to-packetbuf-err\n");
    return XBEE_STATUS_ERR;
  }
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static int
xbee_prepare(const void *payload, unsigned short payload_len)
{
  if (unlikely(payload == NULL || payload_len == 0)) {
    PRINTF("xbee: null-or-empty-pkt\n");
    return XBEE_TX_PKT_TX_ERR_INV_ARG;
  }
  if (payload_len > XBEE_TX_MAX_FRAME_SIZE) {
    PRINTF("xbee-drv: payload-exceeds-max-frame-size:[%u]\n", payload_len);
    return XBEE_TX_PKT_TX_ERR_LEN;
  }
  /* In API mode we check if payload has bytes that need to be escaped */
  uint8_t pos = 0; 
  uint8_t new_pos = 0;
  uint8_t *pld = (uint8_t *)payload;
  /* SFD does not need to be escaped when it is the first byte in a stream. */
  tx_pkt_array[new_pos++] = XBEE_SFD_BYTE;
  /* MSB of length must be zero */
  tx_pkt_array[new_pos++] = 0;
  /* LSB of length is the payload length */
  if (payload_len == XBEE_RADIO_OFF || payload_len == XBEE_RADIO_ON) {
    tx_pkt_array[new_pos++] = XBEE_ESCAPE_CHAR;
    tx_pkt_array[new_pos++] = XBEE_ESCAPE_XOR^payload_len;
  } else {
    tx_pkt_array[new_pos++] = payload_len;
  }
  /* Frame payload appended and escaped if needed */
  while(pos < payload_len) {
    if (new_pos == (XBEE_TX_MAX_FIFO_SIZE-1)) {
      PRINTF("xbee-drv: overflow-fifo-tx\n");
      return XBEE_TX_PKT_TX_ERR_LEN;
	 }
    if (pld[pos] != XBEE_ESCAPE_CHAR && 
        pld[pos] != XBEE_RADIO_OFF   && 
        pld[pos] != XBEE_RADIO_ON    && 
        pld[pos] != XBEE_SFD_BYTE) {
      tx_pkt_array[new_pos++] = pld[pos++];
    } else {
      tx_pkt_array[new_pos++] = XBEE_ESCAPE_CHAR;
      tx_pkt_array[new_pos++] = pld[pos++]^(XBEE_ESCAPE_XOR);
    }
  }
  /* Compute checksum on the non-escaped sequence */
  uint8_t cs = compute_checksum(pld, payload_len);
  if (cs == XBEE_ESCAPE_CHAR || cs == XBEE_RADIO_ON ||
    cs == XBEE_SFD_BYTE || cs == XBEE_RADIO_OFF) {
    if (new_pos == (XBEE_TX_MAX_FIFO_SIZE-1)) {
      PRINTF("xbee-drv: overflow-fifo-tx\n");
      return XBEE_TX_PKT_TX_ERR_LEN;
    }
    tx_pkt_array[new_pos++] = XBEE_ESCAPE_CHAR;
	 tx_pkt_array[new_pos++] = cs^(XBEE_ESCAPE_XOR);
  } else {
    tx_pkt_array[new_pos++] = cs;
  }
  /* Save length of prepared RD frame */
  tx_pkt_array_len = new_pos;
  /* All fine */
  return XBEE_TX_PKT_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
xbee_transmit(unsigned short len)
{
  UNUSED(len);
  if (xbee_drv_has_pending_operations()) {
    PRINTF("xbee-drv: frame-is-pending\n");
    return RADIO_TX_ERR;
  }
  /* Send the packet in the FIFO buffer down to the XBEE device */
  xbee_send_byte_stream_down(&tx_pkt_array[0], tx_pkt_array_len);
  /* A frame is now pending */
  xbee_frame_is_pending = 1;
  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
xbee_send(const void *payload, unsigned short len)
{
  if (xbee_drv_has_pending_operations()) {
    PRINTF("xbee-drv: frame-already-pending\n");
    return RADIO_TX_ERR;
  }
  if (!xbee_dev_is_active) {
    PRINTF("xbee-drv: inactive\n");
    return RADIO_TX_ERR;
  }
  xbee_status_t status;
  if ((status = xbee_prepare(payload, len)) != XBEE_TX_PKT_TX_OK) {
    PRINTF("xbee-drv: prepare-fail [%u]\n", status);
    return RADIO_TX_ERR;
  }
  if ((status = xbee_transmit(tx_pkt_array_len)) != RADIO_TX_OK) {
    PRINTF("xbee-drv: transmit-fail [%u]\n", status);
    return status;
  }
  /* Block until the status response has been received */
  rtimer_clock_t current_time = RTIMER_NOW();
  watchdog_periodic();
  while (xbee_frame_is_pending) {
    if (RTIMER_NOW() > current_time + XBEE_TX_STATUS_WAIT_TIME) {
      PRINTF("xbee-drv: tx-status-timeout\n");
		xbee_serial_line_init(0);
      xbee_frame_is_pending = 0;
      return RADIO_TX_NOACK;
    }
  }
  /* Pending-id contains the RADIO response flag */
  return xbee_sync_rsp_pending_id;
}
/*---------------------------------------------------------------------------*/
static int
xbee_receiving_packet(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static int
xbee_pending_packet(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_prepare_read_param(xbee_at_command_t *new_cmd, uint16_t cmd_id)
{  
  /* Copy the command ID */
  new_cmd->cmd_id[0] = (uint8_t)(cmd_id & 0xff);
  new_cmd->cmd_id[1] = (uint8_t)((cmd_id >> 8) & 0xff);
  /* Fill in command data corresponding to the parameter that is to be read. */
  switch(cmd_id) {
  case XBEE_API_CHAN_16:
  case XBEE_API_HV_VER_16:
  case XBEE_API_SRC_HI_16:
  case XBEE_API_SRC_LO_16:
  case XBEE_API_MY_16:
  case XBEE_API_PAN_ID_16:
  case XBEE_API_RET_16:
  case XBEE_API_MAC_MODE_16:
  case XBEE_API_PW_LEVEL_16:
  case XBEE_API_API_MODE_16:
  case XBEE_API_BAUD_RATE_16:
    new_cmd->len = 0;
    break;
  default:
    PRINTF("XBEE_DRV: read-cmd-not-handled\n");
    return XBEE_STATUS_ERR;
  }
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_prepare_write_param(xbee_at_command_t *new_cmd, uint16_t cmd_id,
  uint8_t *param)
{
  /* Copy the command ID */
  new_cmd->cmd_id[0] = (uint8_t)(cmd_id & 0xff);
  new_cmd->cmd_id[1] = (uint8_t)((cmd_id >> 8) & 0xff);
  /* Fill in command data corresponding to the parameter that is to be read. */
  switch(cmd_id) {  
  case XBEE_API_CHAN_16:
  case XBEE_API_RET_16:
  case XBEE_API_MAC_MODE_16:
  case XBEE_API_PW_LEVEL_16:
  case XBEE_API_API_MODE_16:
  case XBEE_API_BAUD_RATE_16:
    new_cmd->len = 1;
    new_cmd->data[0] = param[0];
    break;
  case XBEE_API_MY_16:
  case XBEE_API_PAN_ID_16:
    new_cmd->len = 2;
    new_cmd->data[0] = param[0];
    new_cmd->data[1] = param[1];
    break;
  case XBEE_API_HV_VER_16:
  case XBEE_API_SRC_HI_16:
  case XBEE_API_SRC_LO_16:
    PRINTF("xbee-drv: read-only-api-cmd\n");
    return XBEE_STATUS_INV_CMD;
  default:
    PRINTF("XBEE_DRV: write-cmd-not-handled\n");
    return XBEE_STATUS_ERR;
  }
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_schedule_async_cmd(uint16_t cmd_id, uint8_t *param, uint8_t len)
{
  /* Prepare a command to be sent to the XBEE device */
  xbee_at_command_t *new_cmd = (xbee_at_command_t *)memb_alloc(&xbee_cmd_mem);
  if(new_cmd == NULL) {
    return XBEE_STATUS_ERR;
  }
  new_cmd->next = NULL;
  /* New frame ID */
  xbee_drv_cmd_seqno++;
  if (xbee_drv_cmd_seqno == 0) {
    xbee_drv_cmd_seqno++;
  }
  new_cmd->frame_id = xbee_drv_cmd_seqno;
  xbee_cmd_status_t status;

  if (param != NULL || len != 0) {
    /* Write parameter down */
    status = xbee_drv_prepare_write_param(new_cmd, cmd_id, param);
  } else {
    /* Read parameter */
    status = xbee_drv_prepare_read_param(new_cmd, cmd_id);
  }  
  if (status != XBEE_STATUS_OK) {
    PRINTF("xbee-drv: prepare-async-cmd-err\n");
    return status;  
  }
  /* Add command to the list of pending commands */
  list_add(xbee_cmd_list, new_cmd);
  /* Post an event for pending asynchronous command */
  process_post(&xbee_driver_process, xbee_driver_cmd_event, NULL);
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_send_sync_cmd(uint16_t cmd_id, uint8_t *param, uint8_t len)
{
  if (xbee_sync_cmd_is_pending) {
    PRINTF("xbee-drv: sync-cmd-pending\n");
    return XBEE_STATUS_ERR;
  }
  xbee_at_command_t _cmd;
  xbee_cmd_status_t status;
  /* New frame ID */
  xbee_drv_cmd_seqno++;
  if (xbee_drv_cmd_seqno == 0) {
	  xbee_drv_cmd_seqno++;
  }
  _cmd.frame_id = xbee_drv_cmd_seqno;
  
  if (param != NULL || len != 0) {
    status = xbee_drv_prepare_write_param(&_cmd, cmd_id, param);
  } else {
    status = xbee_drv_prepare_read_param(&_cmd, cmd_id);
  }  
  if (status != XBEE_STATUS_OK) {
    return status;
  }
  if (xbee_send_asynchronous_cmd(&_cmd) != XBEE_STATUS_OK) {
    PRINTF("xbee-drv: xbee-send-cmd-err\n");
    return XBEE_STATUS_ERR;
  }
  /* A command is now pending */
  xbee_sync_cmd_is_pending = 1;
  xbee_sync_rsp_pending_id = _cmd.frame_id;
  rtimer_clock_t current_time = RTIMER_NOW();
  /* Block until the command response is received */
  while(xbee_sync_cmd_is_pending) {
    watchdog_periodic();
    if (RTIMER_NOW() > current_time + RTIMER_SECOND/100) {
      PRINTF("xbee-drv: sync-cmd-timeout\n");
		return XBEE_STATUS_ERR;
    }
  }
  return xbee_sync_rsp_pending_id;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_handle_cmd_rsp(xbee_at_command_rsp_t *rsp, uint8_t payload_len)
{
  uint8_t has_param;
  if (xbee_drv_has_pending_operations()) {
    if (xbee_sync_rsp_pending_id != rsp->frame_id) {
      PRINTF("XBEE_DRV: received-cmd-rsp-no-pending-or-err\n");
      return XBEE_STATUS_INV_ARG;
    }
    xbee_sync_rsp_pending_id = 0;
    xbee_sync_cmd_is_pending = 0;
  } else {
    /* Response from asynchronous command or from a command already handled */
    etimer_stop(&xbee_etimer);
    //TODO - handle asynchronous response 
  }

  if (unlikely(rsp->rsp_status != XBEE_CMD_RSP_STATUS_OK)) {
    PRINTF("XBEE_DRV: cmd-rsp-status-err %u\n", rsp->rsp_status);
    return XBEE_STATUS_ERR;
  }
  /* The whole process is stateless, so from the command response 
   * and its length, we must know what to do. 
   */
  if (payload_len == 0) {
    has_param = 0;
  } else {
    has_param = 1;
  }
  switch(rsp->cmd_id) {
  case XBEE_API_BAUD_RATE_16:
    if (has_param)
      PRINTF("xbee-drv: baud-rate: %02x %02x %02x %02x\n", rsp->value[0],
        rsp->value[1], rsp->value[2], rsp->value[3]);
    break;
  case XBEE_API_CHAN_16:
    if (has_param)
      PRINTF("xbee-drv: RF channel:%u\n", rsp->value[0]);
    break;
  case XBEE_API_HV_VER_16:
    if (has_param) {
      PRINTF("xbee-drv: H/W v.:");
		PRINTF("%u%u\n", rsp->value[0], rsp->value[1]);
	 }
    break;
  case XBEE_API_SRC_HI_16:
    memcpy(&linkaddr_node_addr.u8[0], &rsp->value[0], 4);
    break;
  case XBEE_API_SRC_LO_16:
    memcpy(&linkaddr_node_addr.u8[4], &rsp->value[0], 4);
    PRINTF("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
      linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
      linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[3],
      linkaddr_node_addr.u8[4], linkaddr_node_addr.u8[5],
      linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
    break;
  case XBEE_API_PAN_ID_16:
    if (has_param)
      PRINTF("xbee-drv: pan-id: 0x %02x:%02x\n", rsp->value[0], rsp->value[1]);
    break;
  case XBEE_API_RET_16:
    if (has_param)
      PRINTF("xbee-drv: xbee-retries: %02x\n", rsp->value[0]);
    break;
  case XBEE_API_MY_16:
    if (has_param)
      PRINTF("xbee-drv: 16-bit address: %02x:%02x\n", rsp->value[0], rsp->value[1]);
    break;
  case XBEE_API_MAC_MODE_16:
    if (has_param)
      PRINTF("xbee-drv: mac-mode: %u\n", rsp->value[0]);
    break;
  case XBEE_API_PW_LEVEL_16:
    if (has_param)
      PRINTF("xbee-drv: power-level: %u\n", rsp->value[0]);
    break;
  case XBEE_API_API_MODE_16:
    if (has_param)
      PRINTF("xbee-drv: API mode: %u\n", rsp->value[0]);
    break;
  default:
    PRINTF("xbee-drv: unknown-cmd\n");
    return XBEE_STATUS_INV_CMD;
  }
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_handle_tx_status_rsp(xbee_at_tx_status_rsp_t *rsp, uint8_t len)
{
  uint8_t rsp_id = (uint8_t)(0xff & packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
  rsp_id = rsp_id == 0 ? 1 : rsp_id;
  if (rsp->frame_id != rsp_id) {
    PRINTF("xbee-drv: received wrong status rsp %02x %02x\n", rsp->frame_id,
      rsp_id);
    return XBEE_TX_STATUS_RSP_INVALID_RSP;
  }
  if (unlikely(!xbee_frame_is_pending)) {
    PRINTF("xbee-drv: no-frame-pending\n");
    return XBEE_STATUS_ERR;
  }
  xbee_frame_is_pending = 0;
  switch (rsp->option) {
  case XBEE_TX_STATUS_OPT_SUCCESS:
    /* TODO - check the Frame ID normally */
    xbee_sync_rsp_pending_id = RADIO_TX_OK;
    break;
  case XBEE_TX_STATUS_OPT_NO_ACK:
    xbee_sync_rsp_pending_id = RADIO_TX_NOACK;
    break;
  case XBEE_TX_STATUS_OPT_CCA_FAIL:
    xbee_sync_rsp_pending_id = RADIO_TX_COLLISION;
    break;
  case XBEE_TX_STATUS_OPT_PURGED:
  default:
    xbee_sync_rsp_pending_id = RADIO_TX_ERR;
    PRINTF("xbee-drv: unknown tx status response %u\n", rsp->option);
    break;
  }
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
xbee_cmd_status_t
xbee_drv_handle_device_response(xbee_response_t *rsp)
{
  /* Inspect the first byte [API identifier] of the response */
  switch(rsp->api_id) {
  case XBEE_AT_CMD_RSP_ID:
    if (xbee_sync_cmd_is_pending) {
      if (rsp->len < 5) {
        PRINTF("xbee-drv: cmd-rsp-too-short\n");
        return XBEE_STATUS_INV_ARG;
      }
      return xbee_drv_handle_cmd_rsp(&rsp->cmd_data, rsp->len-5);
	 }
    return XBEE_STATUS_OK;
  case XBEE_AT_RX_FRAME_RSP_ID:
    /* Remove length byte and API ID byte, so DECREMENT total length */
    xbee_read(&rsp->frame_data, rsp->len-1);
    /* Send packet to the network stack */
    NETSTACK_RDC.input();
    return XBEE_STATUS_OK;
  case XBEE_AT_TX_STATUS_RSP_ID:
    if (xbee_frame_is_pending)
      return xbee_drv_handle_tx_status_rsp(&rsp->tx_status, rsp->len-2);
    return XBEE_STATUS_OK;
  case XBEE_AT_MODEM_STATUS_RSP:
    /* TODO Perform hard reset */
	 xbee_dev_is_active = 0;
    return XBEE_STATUS_OK;
  default:
    PRINTF("XBEE_DRV: unrecognized-api-id %u\n", rsp->api_id);
    PRINTF("DEBUG(%u): ", rsp->len);
#if DEBUG
    int i;
    uint8_t *test = (uint8_t *)rsp;
    for (i=0; i< rsp->len+1; i++) {
      PRINTF("%02x ", test[i]);
    }
#endif
    return XBEE_STATUS_INV_ARG;
  }
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
eventhandler(process_event_t ev, process_data_t data)
{
  if (ev == xbee_drv_device_response) {
    if (data == NULL) {
      PRINTF("xbee-drv: eventhandler-on-null-data\n");
      return XBEE_STATUS_INV_ARG;
    }
    return xbee_drv_handle_device_response((xbee_response_t *)data);
  } else if (ev == PROCESS_EVENT_EXITED) {
  } else {
    PRINTF("xbee-drv: (eventhandler) un-handled event %02x\n", ev);
  }
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static int
xbee_on(void)
{
  return 1;
}
/*---------------------------------------------------------------------------*/
static int
xbee_off(void)
{
  return 1;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(xbee_driver_process, ev, data)
{
  PROCESS_BEGIN();
  PROCESS_PAUSE();
  static xbee_cmd_status_t status = XBEE_STATUS_OK;
  static xbee_at_command_t *_cmd = NULL;
  
  xbee_driver_cmd_event = process_alloc_event();
  xbee_drv_device_response = process_alloc_event();
  
  /* READ the H/W version of the XBEE driver */
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_HV_VER_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("xbee-drv: tx-sync-cmd-err: %04x\n", XBEE_API_HV_VER_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Set the RF channel */
  uint8_t chan = RF_CHANNEL;
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_CHAN_16, &chan, 1))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_CHAN_16);
    goto exit_init;
  }
  etimer_set(&xbee_etimer, CLOCK_SECOND/10);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&xbee_etimer));
  /* Read the RF channel */
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_CHAN_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_CHAN_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Read the 64-bit MAC address, HIGH and LOW bytes */
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_SRC_HI_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_SRC_HI_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_SRC_LO_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_SRC_LO_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Set the PAN-ID */
  uint16_t panid = IEEE802154_PANID;
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_PAN_ID_16, 
    (uint8_t *)&panid, 2)) != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-set-cmd-err: %04x\n", XBEE_API_PAN_ID_16);
    goto exit_init;
  }
  etimer_set(&xbee_etimer, CLOCK_SECOND/10);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&xbee_etimer));
  /* Read the PAN-ID */
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_PAN_ID_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_PAN_ID_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Read the number of XBEE retries */
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_RET_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_RET_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Disable reception of packets with 16-bit addresses */
  uint16_t my_addr = XBEE_API_MY_DISABLE_SHORT;
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_MY_16, (uint8_t *)&my_addr,
    sizeof(my_addr))) != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_MY_16);
    goto exit_init;
  }
  etimer_set(&xbee_etimer, CLOCK_SECOND/10);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&xbee_etimer));
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_MY_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_MY_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Disable DiGi extra header on 802.15.4 frames */
  uint8_t mmode = XBEE_API_MM_802154_WACKS;
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_MAC_MODE_16, &mmode, sizeof(mmode)))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %04x\n", XBEE_API_MAC_MODE_16);
    goto exit_init;
  }
  etimer_set(&xbee_etimer, CLOCK_SECOND/10);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&xbee_etimer));
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_MAC_MODE_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: prepare-cmd-err: %04x\n", XBEE_API_MAC_MODE_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Set TX Power level */
  uint8_t power = XBEE_API_PL_02_DBm;
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_PW_LEVEL_16, &power, sizeof(power)))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %u\n", XBEE_API_PW_LEVEL_16);
    goto exit_init;
  }
  etimer_set(&xbee_etimer, CLOCK_SECOND/10);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&xbee_etimer));
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_PW_LEVEL_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %u\n", XBEE_API_PW_LEVEL_16);
    goto exit_init;
  }
  PROCESS_PAUSE();
  /* Enable API Mode */
  uint8_t api = XBEE_API_MODE_2_EN_ESC;
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_API_MODE_16, &api, sizeof(api)))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %u\n", XBEE_API_API_MODE_16);
    goto exit_init;
  }
  etimer_set(&xbee_etimer, CLOCK_SECOND/10);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&xbee_etimer));
  if ((status = xbee_drv_send_sync_cmd(XBEE_API_API_MODE_16, NULL, 0))
    != XBEE_STATUS_OK) {
    PRINTF("XBEE_DRV: tx-sync-cmd-err: %u\n", XBEE_API_API_MODE_16);
    goto exit_init;
  }
exit_init:
  /* Inform NETSTACK that radio device is ready */
  if (status == XBEE_STATUS_OK) {
    xbee_dev_is_active = 1;
    NETSTACK_RDC.connect_event(1);  
  } else {
    goto exit_proc;
  }
  /* Configure reset pin for hard reset */
  configure_output_pin(XBEE_RESET_PIN, HIGH, DISABLE, DISABLE);
  while(1) {
    PROCESS_YIELD(); 
    if ((data == &xbee_etimer) && (ev == PROCESS_EVENT_TIMER) && 
      (etimer_expired(&xbee_etimer))) {
      /* Execute periodic operations */
      PRINTF("periodic\n");
    } else if (ev == xbee_driver_cmd_event) {
      /* Send sync command(s) down */
      while (list_head(xbee_cmd_list) != NULL) {
        _cmd = (xbee_at_command_t *)list_pop(xbee_cmd_list);
        status = xbee_send_asynchronous_cmd(_cmd);
        memb_free(&xbee_cmd_mem, _cmd);
		  if (status != XBEE_STATUS_OK) {
			  break;
		  }
        if (ev == PROCESS_EVENT_EXIT) {
          break;
        }
      }
    } else {
      if (eventhandler(ev, data) != XBEE_STATUS_OK) {
        break;
  	   } 
    }
  }
exit_proc:
  PRINTF("xbee-drv: exiting\n");
  PROCESS_END();	
}
/*---------------------------------------------------------------------------*/
const struct radio_driver xbee_driver = {
  xbee_init,
  xbee_prepare,
  xbee_transmit,
  xbee_send,
  xbee_read,
  NULL,
  xbee_receiving_packet,
  xbee_pending_packet,
  xbee_on,
  xbee_off, 
};
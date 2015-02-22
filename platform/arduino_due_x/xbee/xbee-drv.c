#include "radio.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "xbee.h"
#include "core/lib/list.h"
#include "core/lib/memb.h"
#include "netstack.h"
#include "linkaddrx.h"
#include "process.h"
#include "rtimer.h"
#include "xbee-api.h"
#include "watchdog.h"
#include "packetbuf.h"
#include "ringbuf.h"
#include "pend-SV.h"
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

#ifndef XBEE_CONF_TX_STATUS_WAIT_TIME
#define XBEE_TX_STATUS_WAIT_TIME XBEE_CONF_TX_STATUS_WAIT_TIME
#else
#define XBEE_TX_STATUS_WAIT_TIME (RTIMER_SECOND / 10)
#endif

#ifndef XBEE_DRV_DEV_ONLINE_RSP_LEN
#define XBEE_DRV_DEV_ONLINE_RSP_LEN 64
#endif

/* XBEE device driver state */
typedef enum xbee_dev_status {
  XBEE_DRV_DEV_INACTIVE = 0,
  XBEE_DRV_DEV_UNDER_RESET,
  XBEE_DRV_DEV_UNDER_INIT,
  XBEE_DRV_DEV_IDLE,
  XBEE_DRV_DEV_CMD_PENDING,
  XBEE_DRV_DEV_TX_STATUS_PENDING
} xbee_dev_status_t;

/* XBEE state of the initialization sequence */
typedef enum xbee_dev_init_status {
  /* This MUST be first */
  XBEE_DRV_DEV_CONFIG_IDLE = 0,
  
  XBEE_DRV_DEV_CONFIG_GET_HW_VER,
  XBEE_DRV_DEV_CONFIG_SET_CHANNEL,
  XBEE_DRV_DEV_CONFIG_GET_CHANNEL,
  XBEE_DRV_DEV_CONFIG_SET_PANID,
  XBEE_DRV_DEV_CONFIG_GET_PANID,
  XBEE_DRV_DEV_CONFIG_GET_SRC_HI,
  XBEE_DRV_DEV_CONFIG_GET_SRC_LO,
  XBEE_DRV_DEV_CONFIG_GET_RR,
  XBEE_DRV_DEV_CONFIG_SET_MYADDR,
  XBEE_DRV_DEV_CONFIG_GET_MYADDR,
  XBEE_DRV_DEV_CONFIG_SET_MMODE,
  XBEE_DRV_DEV_CONFIG_GET_MMODE,
  XBEE_DRV_DEV_CONFIG_SET_POWER,
  XBEE_DRV_DEV_CONFIG_GET_POWER,
  XBEE_DRV_DEV_CONFIG_SET_API,
  XBEE_DRV_DEV_CONFIG_GET_API,
  /* This MUST be last */
  XBEE_DRV_DEV_CONFIG_DONE,
} xbee_dev_init_status_t;

/** XBEE event timer for handling command timeout messages */
static struct etimer xbee_etimer;
/** XBEE command send event */
process_event_t xbee_driver_cmd_event, xbee_drv_device_response;
static process_event_t xbee_driver_reset_event;

/** Outgoing FIFO buffer and length */
static uint8_t tx_pkt_array[XBEE_TX_MAX_FIFO_SIZE];
static uint8_t tx_pkt_array_len = 0;

/** List of pending XBEE commands */
LIST(xbee_cmd_list);
/* MEMB memory block for outgoing XBEE commands */
MEMB(xbee_cmd_mem, xbee_at_command_t, XBEE_MAX_PENDING_CMD_LEN);

/** The XBEE driver structure */
typedef struct xbee_drv_device {
  xbee_device_t *dev;
  xbee_dev_status_t status;
  xbee_dev_init_status_t init_status;
  struct ctimer xbee_ctimer;
  uint8_t sync_rsp_pending_id;
  uint8_t cmd_seqno;
  uint8_t rdc_connected;
  uint8_t pos;
  uint8_t esc_next;
  xbee_response_t rsp;
} xbee_drv_device_t;

xbee_drv_device_t xbee_drv_dev;
#if XBEE_WITH_DUAL_RADIO
xbee_drv_device_t xbee_drv_dev0;
#endif

static xbee_dev_config_t xbee_dft_conf = {
  .channel = RF_CHANNEL,
  .channel_sec = RF_CHANNEL-10,
  .mac_mode = XBEE_API_MM_802154_WACKS,
  .my_addr = XBEE_API_MY_DISABLE_SHORT,
  .panid = IEEE802154_PANID,
  .power = XBEE_API_PL_02_DBm,
  .api = XBEE_API_MODE_2_EN_ESC,
};
/*---------------------------------------------------------------------------*/
void
xbee_drv_get_log(char *err_log)
{
  sprintf(err_log,
    "DEV:%u,RST:%u,OVF:%u,CMD:%u,SFD:%u,MSB:%u,LSB:%u,PKT:%u,TX:%u,RX:%u\n",
    xbee_drv_dev.dev->sdev,
    xbee_drv_dev.dev->stats->rst_count,
	 xbee_drv_dev.dev->stats->ovf_err,
	 xbee_drv_dev.dev->stats->cmd_err,
    xbee_drv_dev.dev->stats->sfd_err,
    xbee_drv_dev.dev->stats->msb_err,
    xbee_drv_dev.dev->stats->lsb_err,
    xbee_drv_dev.dev->stats->pkt_err,
    xbee_drv_dev.dev->stats->tx_pkt_count,
    xbee_drv_dev.dev->stats->rx_pkt_count);
#if XBEE_WITH_DUAL_RADIO
  sprintf(err_log+strlen(err_log),
    "DEV:%u,RST:%u,OVF:%u,CMD:%u,SFD:%u,MSB:%u,LSB:%u,PKT:%u,TX:%u,RX:%u",
    xbee_drv_dev0.dev->sdev,
    xbee_drv_dev0.dev->stats->rst_count,
    xbee_drv_dev0.dev->stats->ovf_err,
    xbee_drv_dev0.dev->stats->cmd_err,
    xbee_drv_dev0.dev->stats->sfd_err,
    xbee_drv_dev0.dev->stats->msb_err,
    xbee_drv_dev0.dev->stats->lsb_err,
    xbee_drv_dev0.dev->stats->pkt_err,
    xbee_drv_dev0.dev->stats->tx_pkt_count,
    xbee_drv_dev0.dev->stats->rx_pkt_count);
#endif /* XBEE_WITH_DUAL_RADIO */
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_handle_tx_status_rsp(xbee_device_t *dev, xbee_at_tx_status_rsp_t *rsp,
  uint8_t len);
static xbee_cmd_status_t xbee_drv_handle_cmd_rsp(xbee_device_t *dev,
  xbee_at_command_rsp_t *rsp, uint8_t payload_len);
/*---------------------------------------------------------------------------*/
void
xbee_drv_check_response(void *data)
{
  xbee_drv_device_t *drv = NULL;
  xbee_device_t *dev = (xbee_device_t *)data;
  
  if (xbee_drv_dev.dev->sdev == dev->sdev) {
    drv = &xbee_drv_dev;
#if XBEE_WITH_DUAL_RADIO
  } else if (xbee_drv_dev0.dev->sdev == dev->sdev) {
    drv = &xbee_drv_dev0;
#endif
  } else {
    PRINTF("xbee-drv: no-device-found\n");
    return;
  }
  
  int c = ringbuf_get(((xbee_device_t *)data)->onbuf);
  if (c == -1) {
    PRINTF("xbee-drv: check-rsp-err\n");
    drv->pos = 0;
    return;
  }
  /* A valid character is received */
  switch(drv->pos) {
  case 0:
    /* This should be the SFD. Re-set escape character flag. */
    drv->esc_next = 0;
    if (c != XBEE_SFD_BYTE) {
      drv->pos = 0;
      if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      return;
    }
    drv->pos++;
    break;
  case 1:
    /* This should be zero (MSB) */
    if (c != 0) {
  	   drv->pos = 0;
      PRINTF("xbee-drv: (on) MSB-err:%u\n", c);
      if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      return;
    }
    drv->pos++;
    break;
  case 2:
    /* This should be the LSB of the response length. It could be escaped. */
    if (c == XBEE_ESCAPE_CHAR) {
      if (!drv->esc_next) {
        drv->esc_next = 1;
        if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
          pend_sv_register_handler(xbee_drv_check_response, data);
        }
        return;
      } else {
        drv->pos = 0;
        drv->esc_next = 0;
        PRINTF("xbee-drv: (on) LSB-err\n");
        if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
          pend_sv_register_handler(xbee_drv_check_response, data);
        }
        return;
      }
    } else {
      if (!drv->esc_next) {
        drv->rsp.len = c;
      } else {
        drv->esc_next = 0;
        drv->rsp.len = c^XBEE_ESCAPE_XOR;
      }
      drv->pos++;
    }
    break;
  case 3:
    /* This is the API ID. No need to be escaped. */
    if (c == XBEE_AT_CMD_RSP_ID) {
      if (drv->status != XBEE_DRV_DEV_CMD_PENDING) {
  	     PRINTF("xbee-drv: no-cmd-is-pending\n");
      } else {
        drv->rsp.api_id = c;
        drv->pos++;
        break;
  	   }
    } else if (c == XBEE_AT_TX_STATUS_RSP_ID) {
  	   if (drv->status != XBEE_DRV_DEV_TX_STATUS_PENDING) {
  	     PRINTF("xbee-drv: no-status-is-pending\n");
      } else {
        drv->pos++;
        drv->rsp.api_id = c;
        break;
      }
    } else if (c == XBEE_AT_RX_FRAME_RSP_ID) {
      /* This is an incoming frame, but we should not handle it inside this
       * software interrupt context.
       */
    } else {
  	   PRINTF("xbee-drv: only-handling-rsp:%02x\n", c);
    }
    drv->pos = 0;
    if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
      pend_sv_register_handler(xbee_drv_check_response, data);
    }
    return;
  case 4:
    /* This is the frame id and could be escaped. */
    if (c == XBEE_ESCAPE_CHAR) {
      if (!drv->esc_next) {
        drv->esc_next = 1;
        break;
      } else {
        drv->esc_next = 0;
        c = c^XBEE_ESCAPE_XOR;
      }
    } else {
      if (drv->esc_next) {
        c = c^XBEE_ESCAPE_XOR;
      }
    }
    if (drv->status == XBEE_DRV_DEV_CMD_PENDING && drv->sync_rsp_pending_id != c) {
  	   PRINTF("xbee-drv: got %u, expecting id:%u\n", c,
        drv->sync_rsp_pending_id);
      drv->pos = 0;
      if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      return;
    }
    /* Now we have the right response */
    if (drv->rsp.api_id == XBEE_AT_TX_STATUS_RSP_ID) {
      drv->rsp.tx_status.frame_id = c;
	 } else if (drv->rsp.api_id == XBEE_AT_CMD_RSP_ID) {
      drv->rsp.cmd_data.frame_id = c;
    }
    drv->pos++;
    break;
  case 5:
    if (drv->status == XBEE_DRV_DEV_CMD_PENDING) {
      /* Save the first byte of the Command ID. No escape needed. */
      drv->rsp.cmd_data.cmd_id = c;
	 } else if (drv->status == XBEE_DRV_DEV_TX_STATUS_PENDING) {
      /* Save the frame status byte. No escape needed. */
      drv->rsp.tx_status.option = c;
    }
    drv->pos++;
    break;
  case 6:
    if (drv->status == XBEE_DRV_DEV_CMD_PENDING) {
      /* Save the second byte of the Command ID. No escape needed. */
      drv->rsp.cmd_data.cmd_id += ((uint16_t)c) << 8;
      drv->pos++;
    } else if (drv->status == XBEE_DRV_DEV_TX_STATUS_PENDING) {
      /* This is the checksum of the TX status response and could be escaped */
      if (c == XBEE_ESCAPE_CHAR) {
        if (!drv->esc_next) {
          drv->esc_next = 1;
          break;
		  }
      }
      if (xbee_drv_handle_tx_status_rsp((xbee_device_t *)data, &drv->rsp.tx_status,
        sizeof(xbee_at_tx_status_rsp_t)) == XBEE_STATUS_OK) {
      } else {
        PRINTF("xbee-drv: handle-tx-status-err\n");
      }
      drv->pos = 0;
    }
    break;
  case 7:
    /* Read command response status. No escape needed. */
    if (drv->status == XBEE_DRV_DEV_CMD_PENDING) {
      drv->rsp.cmd_data.rsp_status = c;
      drv->pos++;
      break;
    } else {
      PRINTF("xbee-drv: too-many-bytes:%u\n", drv->pos);
      drv->pos = 0;
      return;
    }
  default:
    if (drv->status != XBEE_DRV_DEV_CMD_PENDING) {
      if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
        pend_sv_register_handler(xbee_drv_check_response, data);
      }
      PRINTF("xbee-drv: too-many-bytes:%u\n", drv->pos);
      drv->pos = 0;
      return;
    }
    if (drv->pos-XBEE_TX_FRAME_HDR_SIZE < drv->rsp.len) {
      /* Escaped characters must be handled */
      if (!drv->esc_next) {
        if (c != XBEE_ESCAPE_CHAR) {
          drv->rsp.cmd_data.value[drv->pos-8] = c;
          drv->pos++; 
        } else {
          drv->esc_next = 1;
		  }
      } else {
        drv->rsp.cmd_data.value[drv->pos-8] = c^XBEE_ESCAPE_XOR;
        drv->esc_next = 0;
        drv->pos++;
      }
    } else {
      /* Done. Drop the checksum. */
      drv->pos = 0;
      /* Process response; return result on the ID flag variable. */
      drv->sync_rsp_pending_id = 
        xbee_drv_handle_cmd_rsp((xbee_device_t *)data,
          &drv->rsp.cmd_data, drv->rsp.len-5);
      break;
    }
  }
  if (ringbuf_elements(((xbee_device_t *)data)->onbuf)) {
    pend_sv_register_handler(xbee_drv_check_response, data);
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
xbee_drv_has_pending_operations(xbee_device_t *dev)
{
  if (dev == xbee_drv_dev.dev) {
    return (xbee_drv_dev.status == XBEE_DRV_DEV_CMD_PENDING) || 
           (xbee_drv_dev.status == XBEE_DRV_DEV_TX_STATUS_PENDING);
  }
#if XBEE_WITH_DUAL_RADIO
  if (dev == xbee_drv_dev0.dev) {
	  return (xbee_drv_dev0.status == XBEE_DRV_DEV_CMD_PENDING) ||
	  (xbee_drv_dev0.status == XBEE_DRV_DEV_TX_STATUS_PENDING);
  }
#endif
  return 0;
}
/*---------------------------------------------------------------------------*/
uint8_t
xbee_drv_has_pending_cmd(xbee_device_t *dev)
{
  if (dev == xbee_drv_dev.dev) {
    return xbee_drv_dev.status == XBEE_DRV_DEV_CMD_PENDING;
  }
#if XBEE_WITH_DUAL_RADIO
  if (dev == xbee_drv_dev0.dev) {
    return xbee_drv_dev0.status == XBEE_DRV_DEV_CMD_PENDING;
  }
#endif
  return 0;
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
    /* Initialize underlying serial driver process (default baudrate) */
    xbee_serial_line_init(0);
    /* Zeroing memory. This set device state to INACTIVE. */
    memset(&xbee_drv_dev, 0, sizeof(xbee_drv_device_t));
    xbee_drv_dev.dev = &xbee_dev;
#if XBEE_WITH_DUAL_RADIO
    memset(&xbee_drv_dev0, 0, sizeof(xbee_drv_device_t));
    xbee_drv_dev0.dev = &xbee_dev0;
#endif 
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
  /* Get RADIO interface from packet buffer */
  xbee_drv_device_t *drv = NULL;
  uint8_t radio_iface = packetbuf_attr(PACKETBUF_ATTR_RADIO_INTERFACE);
  if (radio_iface == xbee_drv_dev.dev->sdev) {
    drv = &xbee_drv_dev;
#if XBEE_WITH_DUAL_RADIO
  } else if (radio_iface == xbee_drv_dev0.dev->sdev) {
    drv = &xbee_drv_dev0;
#endif
  } else {
    PRINTF("xbee-drv: no-device-selected\n");
    return RADIO_TX_ERR;
  }
  if (xbee_drv_has_pending_operations(drv->dev)) {
    PRINTF("xbee-drv: frame-is-pending\n");
    return RADIO_TX_ERR;
  }
  drv->dev->stats->tx_pkt_count++;
  /* Send the packet in the FIFO buffer down to the XBEE device */
  xbee_send_byte_stream_down(drv->dev, &tx_pkt_array[0], tx_pkt_array_len);
  /* A frame is now pending */
  drv->status = XBEE_DRV_DEV_TX_STATUS_PENDING;
  return RADIO_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
xbee_send(const void *payload, unsigned short len)
{
  /* Get RADIO interface from packet buffer */
  xbee_drv_device_t *drv = NULL;
#if XBEE_WITH_DUAL_RADIO
  uint8_t radio_iface = packetbuf_attr(PACKETBUF_ATTR_RADIO_INTERFACE);
#else
  uint8_t radio_iface = NETSTACK_802154;
  packetbuf_set_attr(PACKETBUF_ATTR_RADIO_INTERFACE, radio_iface);
#endif
  if (radio_iface == xbee_drv_dev.dev->sdev) {
    drv = &xbee_drv_dev;
#if XBEE_WITH_DUAL_RADIO
  } else if (radio_iface == xbee_drv_dev0.dev->sdev) {
    drv = &xbee_drv_dev0;
#endif
  } else {
    PRINTF("xbee-drv: no-device-selected\n");
    return RADIO_TX_ERR;
  }	
  if (xbee_drv_has_pending_operations(drv->dev)) {
    PRINTF("xbee-drv: frame-already-pending:%u\n", drv->dev->sdev);
    return RADIO_TX_ERR;
  }
  if (drv->status < XBEE_DRV_DEV_IDLE) {
    PRINTF("xbee-drv: inactive:%u:%u\n", drv->dev->sdev, drv->status);
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
  while (drv->status == XBEE_DRV_DEV_TX_STATUS_PENDING) {
    if (RTIMER_NOW() > current_time + XBEE_TX_STATUS_WAIT_TIME) {
      PRINTF("xbee-drv: tx-status-timeout:%u\n", drv->dev->sdev);
      drv->status = XBEE_DRV_DEV_INACTIVE;
      /* Reset reading pointers, as we will start form the beginning */
      drv->pos = 0;
      drv->esc_next = 0;
      drv->dev->ptr = 0;
      drv->dev->esc = 0;
      xbee_do_hw_reset(drv->dev);
      return RADIO_TX_NOACK;
    }
  }
  /* Pending-id contains the RADIO response flag */
  return drv->sync_rsp_pending_id;
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
    new_cmd->data[0] = param[1];
    new_cmd->data[1] = param[0];
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
xbee_drv_schedule_async_cmd(xbee_drv_device_t *drv, uint16_t cmd_id, uint8_t *param,
  uint8_t len)
{
  /* Prepare a command to be sent to the XBEE device */
  xbee_at_command_t *new_cmd = (xbee_at_command_t *)memb_alloc(&xbee_cmd_mem);
  if(new_cmd == NULL) {
    return XBEE_STATUS_ERR;
  }
  new_cmd->next = NULL;
  /* New frame ID */
  drv->cmd_seqno++;
  if (drv->cmd_seqno == 0) {
    drv->cmd_seqno++;
  }
  new_cmd->frame_id = drv->cmd_seqno;
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
  process_post(&xbee_driver_process, xbee_driver_cmd_event, drv->dev);
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_send_sync_cmd(xbee_drv_device_t *drv, uint16_t cmd_id, uint8_t *param,
  uint8_t len)
{
  if (drv->status == XBEE_DRV_DEV_CMD_PENDING) {
    PRINTF("xbee-drv: sync-cmd-pending:%u\n", drv->dev->sdev);
    return XBEE_STATUS_ERR;
  }
  xbee_at_command_t _cmd;
  xbee_cmd_status_t status;
  /* New frame ID */
  drv->cmd_seqno++;
  if (drv->cmd_seqno == 0) {
	  drv->cmd_seqno++;
  }
  _cmd.frame_id = drv->cmd_seqno;
  
  if (param != NULL || len != 0) {
    status = xbee_drv_prepare_write_param(&_cmd, cmd_id, param);
  } else {
    status = xbee_drv_prepare_read_param(&_cmd, cmd_id);
  }  
  if (status != XBEE_STATUS_OK) {
    return status;
  }
  if (xbee_send_asynchronous_cmd(drv->dev, &_cmd) != XBEE_STATUS_OK) {
    PRINTF("xbee-drv: xbee-send-cmd-err\n");
    return XBEE_STATUS_ERR;
  }
  /* A command is now pending */
  drv->status = XBEE_DRV_DEV_CMD_PENDING;
  drv->sync_rsp_pending_id = _cmd.frame_id;
  rtimer_clock_t current_time = RTIMER_NOW();
  /* Block until the command response is received */
  while(drv->status == XBEE_DRV_DEV_CMD_PENDING) {
    watchdog_periodic();
    if (RTIMER_NOW() > current_time + RTIMER_SECOND/10) {
      PRINTF("xbee-drv: sync-cmd-timeout:%u\n", drv->dev->sdev);
		return XBEE_STATUS_ERR;
    }
  }
  return drv->sync_rsp_pending_id;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_handle_cmd_rsp(xbee_device_t *dev, xbee_at_command_rsp_t *rsp,
  uint8_t payload_len)
{
  xbee_drv_device_t *drv = NULL;
  if (xbee_drv_dev.dev->sdev == dev->sdev) {
    drv = &xbee_drv_dev;
#if XBEE_WITH_DUAL_RADIO
  } else if (xbee_drv_dev0.dev->sdev == dev->sdev) {
    drv = &xbee_drv_dev0;
#endif
  } else {
    PRINTF("xbee-drv: no-selected-device\n");
    return XBEE_STATUS_INV_ARG;
  }
  uint8_t has_param;
  if (xbee_drv_has_pending_cmd(dev)) {
    if (drv->sync_rsp_pending_id != rsp->frame_id) {
      PRINTF("XBEE_DRV: received-cmd-rsp-no-pending-or-err\n");
      return XBEE_STATUS_INV_ARG;
    }
    drv->sync_rsp_pending_id = 0;
    if (drv->init_status != XBEE_DRV_DEV_CONFIG_DONE) {
      drv->status = XBEE_DRV_DEV_UNDER_INIT;
    } else {
      drv->status = XBEE_DRV_DEV_IDLE;
    }
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
      PRINTF("xbee-drv: H/W v.(%u):",drv->dev->sdev);
		PRINTF("%u%u\n", rsp->value[0], rsp->value[1]);
	 }
    break;
  case XBEE_API_SRC_HI_16:
    if (drv == &xbee_drv_dev)
      memcpy(&linkaddr_node_addr.u8[0], &rsp->value[0], 4);
#if XBEE_WITH_DUAL_RADIO
    else if (drv == &xbee_drv_dev0)
      memcpy(&linkaddr1_node_addr.u8[0], &rsp->value[0], 4);
#endif
    break;
  case XBEE_API_SRC_LO_16:
    if (drv == &xbee_drv_dev) {
      memcpy(&linkaddr_node_addr.u8[4], &rsp->value[0], 4);
      PRINTF("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
        linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
        linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[3],
        linkaddr_node_addr.u8[4], linkaddr_node_addr.u8[5],
        linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
#if XBEE_WITH_DUAL_RADIO
    } else if (drv == &xbee_drv_dev0) {
      memcpy(&linkaddr1_node_addr.u8[4], &rsp->value[0], 4);
      PRINTF("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
      linkaddr1_node_addr.u8[0], linkaddr1_node_addr.u8[1],
      linkaddr1_node_addr.u8[2], linkaddr1_node_addr.u8[3],
      linkaddr1_node_addr.u8[4], linkaddr1_node_addr.u8[5],
      linkaddr1_node_addr.u8[6], linkaddr1_node_addr.u8[7]);
#endif
    }
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
xbee_drv_handle_tx_status_rsp(xbee_device_t *dev, xbee_at_tx_status_rsp_t *rsp,
  uint8_t len)
{
  UNUSED(len);
  xbee_drv_device_t *drv = NULL;
  if (xbee_drv_dev.dev->sdev == dev->sdev) {
    drv = &xbee_drv_dev;
#if XBEE_WITH_DUAL_RADIO
  } else if (xbee_drv_dev0.dev->sdev == dev->sdev) {
    drv = &xbee_drv_dev0;
#endif
  } else {
    PRINTF("xbee-drv: no-device\n");
    return XBEE_STATUS_INV_ARG;
  }
  uint8_t rsp_id = (uint8_t)(0xff & packetbuf_attr(PACKETBUF_ATTR_MAC_SEQNO));
  rsp_id = rsp_id == 0 ? 1 : rsp_id;
  if (rsp->frame_id != rsp_id) {
    PRINTF("xbee-drv: received wrong status rsp %02x %02x\n", rsp->frame_id,
      rsp_id);
    return XBEE_TX_STATUS_RSP_INVALID_RSP;
  }
  if (unlikely(drv->status != XBEE_DRV_DEV_TX_STATUS_PENDING)) {
    PRINTF("xbee-drv: no-frame-pending\n");
    return XBEE_STATUS_ERR;
  }
  drv->status = XBEE_DRV_DEV_IDLE;
  switch (rsp->option) {
  case XBEE_TX_STATUS_OPT_SUCCESS:
    /* TODO - check the Frame ID normally */
    drv->sync_rsp_pending_id = RADIO_TX_OK;
    break;
  case XBEE_TX_STATUS_OPT_NO_ACK:
    drv->sync_rsp_pending_id = RADIO_TX_NOACK;
    break;
  case XBEE_TX_STATUS_OPT_CCA_FAIL:
    drv->sync_rsp_pending_id = RADIO_TX_COLLISION;
    break;
  case XBEE_TX_STATUS_OPT_PURGED:
  default:
    drv->sync_rsp_pending_id = RADIO_TX_ERR;
    PRINTF("xbee-drv: unknown tx status response %u\n", rsp->option);
    break;
  }
  return XBEE_STATUS_OK;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
xbee_drv_handle_device_response(xbee_device_t *dev)
{
  /* Inspect the first byte [API identifier] of the response */
  xbee_response_t *rsp = (xbee_response_t *)&dev->rsp[0];
  xbee_drv_device_t *drv = NULL;
  if (dev->sdev == xbee_drv_dev.dev->sdev) {
    drv = &xbee_drv_dev;
#if XBEE_WITH_DUAL_RADIO  
  } else if (dev->sdev == xbee_drv_dev0.dev->sdev) {
    drv = &xbee_drv_dev0;
#endif
  } else {
    PRINTF("xbee-drv: invalid device\n");
    return XBEE_STATUS_INV_ARG;
  }
  switch(rsp->api_id) {
  case XBEE_AT_CMD_RSP_ID:
    if (drv->status == XBEE_DRV_DEV_CMD_PENDING) {
      if (rsp->len < 5) {
        PRINTF("xbee-drv: cmd-rsp-too-short\n");
        return XBEE_STATUS_INV_ARG;
      }
      return xbee_drv_handle_cmd_rsp(dev, &rsp->cmd_data, rsp->len-5);
	 }
    return XBEE_STATUS_OK;
  case XBEE_AT_RX_FRAME_RSP_ID:
    /* Remove length byte and API ID byte, so DECREMENT total length */
    xbee_read(&rsp->frame_data, rsp->len-1);
	 /* Set radio signature on packet buffer */
	 packetbuf_set_attr(PACKETBUF_ATTR_RADIO_INTERFACE, drv->dev->sdev);
    drv->dev->stats->rx_pkt_count++;
    /* Send packet to the network stack */
    NETSTACK_RDC.input();
    return XBEE_STATUS_OK;
  case XBEE_AT_TX_STATUS_RSP_ID:
    if (drv->status == XBEE_DRV_DEV_TX_STATUS_PENDING)
      return xbee_drv_handle_tx_status_rsp(dev, &rsp->tx_status, rsp->len-2);
    return XBEE_STATUS_OK;
  case XBEE_AT_MODEM_STATUS_RSP:
	 switch (rsp->modem_status.status) {
    case XBEE_AT_MODEM_RSP_HWRST:
      /* Schedule initialization procedure */
      PRINTF("xbee-drv: hw-rst-rsp:%u\n", dev->sdev);
      process_post(&xbee_driver_process, xbee_driver_reset_event, drv);
      return XBEE_STATUS_OK;
    case XBEE_AT_MODEM_RSP_WDT: 
      /* Perform hard reset if this is WDT  */
      PRINTF("xbee-drv: wdt-rsp:%u\n", dev->sdev);
      drv->status = XBEE_DRV_DEV_INACTIVE;
      /* Reset reading pointers, as we will start form the beginning */
      drv->pos = 0;
      drv->esc_next = 0;
      drv->dev->ptr = 0;
      drv->dev->esc = 0;
      xbee_do_hw_reset(dev);
      return XBEE_STATUS_OK;
    default:
      return XBEE_STATUS_ERR;
	 }
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
static void
xbee_drv_perform_init_sequence(void *ptr)
{
  xbee_drv_device_t *drv = (xbee_drv_device_t *)ptr;
  /* Get next command in line */
  uint16_t cmd_id;
  uint32_t cmd_param;
  uint8_t cmd_len;

  if (drv->init_status == XBEE_DRV_DEV_CONFIG_IDLE &&
    drv->status != XBEE_DRV_DEV_UNDER_RESET) {
    PRINTF("xbee-drv: wrong-status-at-start-of-init:%u:%u\n",
      drv->dev->sdev, drv->status);
    process_post(&xbee_driver_process, PROCESS_EVENT_EXIT, drv);
    return;
  }
  drv->status = XBEE_DRV_DEV_UNDER_INIT;
  drv->init_status++;
  switch (drv->init_status) {
  case XBEE_DRV_DEV_CONFIG_IDLE:
    break;
  case XBEE_DRV_DEV_CONFIG_SET_CHANNEL:
    cmd_id = XBEE_API_CHAN_16;
#if XBEE_WITH_DUAL_RADIO
    cmd_param = 
      (drv->dev->sdev == (xbee_dev_num_t)NETSTACK_802154) ? xbee_dft_conf.channel : xbee_dft_conf.channel_sec;
#else
    cmd_param = xbee_dft_conf.channel;
#endif
    cmd_len = sizeof(xbee_dft_conf.channel);
    break;
  case XBEE_DRV_DEV_CONFIG_GET_CHANNEL:
    cmd_id = XBEE_API_CHAN_16;
	 cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_GET_HW_VER:
    cmd_id = XBEE_API_HV_VER_16;
    cmd_len = 0;
    break;
 case XBEE_DRV_DEV_CONFIG_SET_PANID:
    cmd_id = XBEE_API_PAN_ID_16;
    cmd_len = sizeof(xbee_dft_conf.panid);
	 cmd_param = xbee_dft_conf.panid;
    break;
  case XBEE_DRV_DEV_CONFIG_GET_PANID:
    cmd_id = XBEE_API_PAN_ID_16;
    cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_GET_SRC_HI:
    cmd_id = XBEE_API_SRC_HI_16;
	 cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_GET_SRC_LO:
    cmd_id = XBEE_API_SRC_LO_16;
    cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_GET_RR:
    cmd_id = XBEE_API_RET_16;
    cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_SET_MYADDR:
    cmd_id = XBEE_API_MY_16;
    cmd_len = sizeof(xbee_dft_conf.my_addr);
    cmd_param = xbee_dft_conf.my_addr;
    break;
  case XBEE_DRV_DEV_CONFIG_GET_MYADDR:
    cmd_id = XBEE_API_MY_16;
    cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_SET_MMODE:
    cmd_id = XBEE_API_MAC_MODE_16;
    cmd_param = xbee_dft_conf.mac_mode;
    cmd_len = sizeof(xbee_dft_conf.mac_mode);
    break;
  case XBEE_DRV_DEV_CONFIG_GET_MMODE:
    cmd_id = XBEE_API_MAC_MODE_16;
    cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_SET_POWER:
    cmd_id = XBEE_API_PW_LEVEL_16;
    cmd_param = xbee_dft_conf.power;
    cmd_len = sizeof(xbee_dft_conf.power);
    break;
  case XBEE_DRV_DEV_CONFIG_GET_POWER:
    cmd_id = XBEE_API_PW_LEVEL_16;
    cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_SET_API:
    cmd_id = XBEE_API_API_MODE_16;
    cmd_param = xbee_dft_conf.api;
    cmd_len = sizeof(xbee_dft_conf.api);
    break;
  case XBEE_DRV_DEV_CONFIG_GET_API:
    cmd_id = XBEE_API_API_MODE_16;
    cmd_len = 0;
    break;
  case XBEE_DRV_DEV_CONFIG_DONE:
    PRINTF("xbee-drv: init-done %u\n", drv->dev->sdev);
    drv->status = XBEE_DRV_DEV_IDLE;
    if (!drv->rdc_connected) {
#if XBEE_WITH_DUAL_RADIO
      packetbuf_set_attr(PACKETBUF_ATTR_RADIO_INTERFACE, drv->dev->sdev);
#endif
      NETSTACK_RDC.connect_event(1);
      drv->rdc_connected++;
    }
    return;
  default:
    PRINTF("xbee-drv: unknown-init-status-on-%u\n", drv->dev->sdev);
    goto err_exit;
  }
  /* Send and wait */
  uint8_t *cmd_parm_ptr = (cmd_len == 0) ? NULL : (uint8_t *)&cmd_param;  
  if (xbee_drv_send_sync_cmd(drv, cmd_id, cmd_parm_ptr, cmd_len)
    != XBEE_STATUS_OK) {
    /* Initialization phase cannot continue. Exit driver for the moment. */
    PRINTF("xbee-drv: err-in-init-cmd-for:%u(%04x)\n", drv->dev->sdev, cmd_id);
    goto err_exit;
  }
  /* Schedule a new event for the driver initialization sequence */
  process_post(&xbee_driver_process, xbee_driver_cmd_event, drv);
  return;
err_exit:
  process_post(&xbee_driver_process, PROCESS_EVENT_EXIT, drv);
  return;
}
/*---------------------------------------------------------------------------*/
static xbee_cmd_status_t
eventhandler(process_event_t ev, process_data_t data)
{
  if (data == NULL) {
    PRINTF("xbee-drv: eventhandler-on-null-data\n");
    return XBEE_STATUS_INV_ARG;
  }
  if (ev == xbee_drv_device_response) {
    return xbee_drv_handle_device_response((xbee_device_t *)data);
  } else if (ev == xbee_driver_reset_event) {
    /* Start the initialization phase */
    xbee_drv_device_t *drv = (xbee_drv_device_t *)data;
    if (drv->status != XBEE_DRV_DEV_INACTIVE) {
      PRINTF("xbee-drv: rst-ev-on-non-inactive:%u\n", drv->dev->sdev);
      return XBEE_STATUS_INV_ARG;
    }
    /* Restart initialization command sequence */
    drv->status = XBEE_DRV_DEV_UNDER_RESET;
    drv->init_status = XBEE_DRV_DEV_CONFIG_IDLE;
    ctimer_set(&drv->xbee_ctimer, CLOCK_SECOND/10,
      xbee_drv_perform_init_sequence, drv);
    return XBEE_STATUS_OK;
  } else if (ev == xbee_driver_cmd_event) {
    xbee_drv_device_t *drv = (xbee_drv_device_t *)data;
    if (drv->status == XBEE_DRV_DEV_UNDER_INIT) {
      /* Continue the initialization phase */
      ctimer_set(&drv->xbee_ctimer, CLOCK_SECOND/10,
        xbee_drv_perform_init_sequence, drv);
      return XBEE_STATUS_OK;
    } else if (drv->status == XBEE_DRV_DEV_IDLE) {
      /* TODO Pick a command from a list and send it down */
      return XBEE_STATUS_OK;
    } else {
      PRINTF("xbee-drv: cmd-ev-for-%u-on-err-state:%u\n", drv->dev->sdev,
        drv->status);
      return XBEE_STATUS_INV_ARG;
    }
  } else if (ev == PROCESS_EVENT_EXITED) {
    /* TODO */
  } else if (ev == PROCESS_EVENT_EXIT) {
    /* TODO */
    return XBEE_STATUS_ERR;
  } else {
    //PRINTF("xbee-drv: eventhandler-unknown-ev:%02x\n", ev);
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
  /* Allocate driver events */
  xbee_driver_cmd_event = process_alloc_event();
  xbee_drv_device_response = process_alloc_event();
  xbee_driver_reset_event = process_alloc_event();
  /* Allow other init to take place */
  PROCESS_PAUSE();
  /* Post a starting event for the XBEE devices */
  process_post(&xbee_driver_process, xbee_driver_reset_event, &xbee_drv_dev);
#if XBEE_WITH_DUAL_RADIO
  process_post(&xbee_driver_process, xbee_driver_reset_event, &xbee_drv_dev0);
#endif
  while(1) {
    PROCESS_YIELD();
    if (eventhandler(ev, data) != XBEE_STATUS_OK) {
      PRINTF("xbee-drv: eventhandler-err:%02x\n", ev);
      break;
    }
  }
  PRINTF("xbee-drv: exits\n");
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
  NULL,
  NULL,
  NULL,
  NULL,
};
/*---------------------------------------------------------------------------*/
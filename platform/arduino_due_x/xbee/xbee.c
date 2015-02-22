/*
 * xbee.c
 *
 * Created: 2014-11-28 14:56:26
 *  Author: Ioannis Glaropoulos
 */
#include "contiki.h"
#include "lib/ringbuf.h"
#include "xbee.h"
#include "xbee-api.h"
#include <stdio.h>
#include <string.h>
#include "wire_digital.h"
#include "watchdog.h"
#include "delay.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef XBEE_CONF_RESET_PIN
#define XBEE_RESET_PIN XBEE_CONF_RESET_PIN
#else
#define XBEE_RESET_PIN 8
#endif

#if XBEE_WITH_DUAL_RADIO
#ifdef XBEE_CONF_RESET_PIN_SEC
#define XBEE_RESET_PIN_SEC XBEE_CONF_RESET_PIN_SEC
#else
#define XBEE_RESET_PIN_SEC 9
#endif
#endif

#ifndef XBEE_CONF_BAUDRATE
#define XBEE_BAUDRATE 115200
#else
#define XBEE_BAUDRATE XBEE_CONF_BAUDRATE
#endif

/* Define the Serial Line Communication bus */
#ifndef XBEE_SERIAL_PORT
#define XBEE_SERIAL_PORT usart1
#include "usart1.h"
#endif
/* Set the Serial Line core methods */
#define xbee_serial_init ATPASTE2(XBEE_SERIAL_PORT, _init)
#define xbee_serial_set_input ATPASTE2(XBEE_SERIAL_PORT, _set_input)
#define xbee_serial_enable_rx_interrupt ATPASTE2(XBEE_SERIAL_PORT, _enable_rx_interrupt)
#define xbee_serial_writeb ATPASTE2(XBEE_SERIAL_PORT, _writeb)
#define xbee_serial_reset ATPASTE2(XBEE_SERIAL_PORT, _reset)

#if XBEE_WITH_DUAL_RADIO
#ifndef XBEE_SERIAL_PORT0
#define XBEE_SERIAL_PORT0 usart2
#include "usart2.h"
#endif /* XBEE_SERIAL_PORT0 */
#define xbee_serial0_init ATPASTE2(XBEE_SERIAL_PORT0, _init)
#define xbee_serial0_set_input ATPASTE2(XBEE_SERIAL_PORT0, _set_input)
#define xbee_serial0_enable_rx_interrupt ATPASTE2(XBEE_SERIAL_PORT0, _enable_rx_interrupt)
#define xbee_serial0_writeb ATPASTE2(XBEE_SERIAL_PORT0, _writeb)
#define xbee_serial0_reset ATPASTE2(XBEE_SERIAL_PORT0, _reset)
#endif /* XBEE_WITH_DUAL_RADIO */
static struct ringbuf rxbuf;
static uint8_t rxbuf_data[XBEE_BUFSIZE];

#if XBEE_HANDLE_SYNC_OPERATIONS
#include "pend-SV.h"
static struct ringbuf rx_online_buf;
static uint8_t rx_online_buf_data[128];
#endif /* XBEE_HANDLE_SYNC_OPERATIONS */

/* Statistics collector */
static xbee_dev_stats_t xbee_stats;

/* Global device structure */
xbee_device_t xbee_dev = {
  .sdev = XBEE_SERIAL_DEV_PRIMAL,
  .buf = &rxbuf,
  .onbuf = &rx_online_buf,
  .writeb = xbee_serial_writeb,
  .stats = &xbee_stats,
  /* Initialize read pointer. */
  .ptr = 0,
  .esc = 0,
};

#if XBEE_WITH_DUAL_RADIO
static struct ringbuf rxbuf0;
static uint8_t rxbuf0_data[XBEE_BUFSIZE];
#if XBEE_HANDLE_SYNC_OPERATIONS
static struct ringbuf rx_online_buf0;
static uint8_t rx_online_buf0_data[128];
#endif
static xbee_dev_stats_t xbee_stats0;
/* Global device structure (secondary radio) */
xbee_device_t xbee_dev0 = {
  .sdev = XBEE_SERIAL_DEV_SECOND,
  .buf = &rxbuf0,
  .onbuf = &rx_online_buf0,
  .writeb = xbee_serial0_writeb,
  .stats = &xbee_stats0,
  /* Initialize read pointer. */
  .ptr = 0,
  .esc = 0,
};
#endif /* XBEE_WITH_DUAL_RADIO */

PROCESS(xbee_serial_line_process, "XBEE serial driver");

process_event_t xbee_serial_line_event_message;
process_event_t xbee_serial_line_error_event_message;

static uint8_t xbee_num_of_pend_polls = 0;
/*---------------------------------------------------------------------------*/
void
xbee_send_byte_stream_down(xbee_device_t *dev, uint8_t* _data, uint8_t len)
{
  int i;
  if (dev->writeb == NULL)
    return;

  for (i=0; i<len; i++) {
     dev->writeb(_data[i]);
  }
}
/*---------------------------------------------------------------------------*/
static void
xbee_send_byte(xbee_device_t *dev, uint8_t b, uint8_t escape_if_needed)
{
  if (dev->writeb == NULL)
    return;

  if (escape_if_needed && (b == XBEE_SFD_BYTE || b == XBEE_ESCAPE_CHAR ||
                           b == XBEE_RADIO_ON || b == XBEE_RADIO_OFF)) {
    dev->writeb((unsigned char)XBEE_ESCAPE_CHAR);
    dev->writeb((unsigned char)(b ^ XBEE_ESCAPE_XOR));
  } else {
    dev->writeb((unsigned char)b);
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
xbee_send_asynchronous_cmd(xbee_device_t *dev, const xbee_at_command_t* cmd)
{
  int i, len = 0;
  if (cmd == NULL) {
    dev->stats->cmd_err++;
    PRINTF("xbee: null command.\n");
    return XBEE_TX_PKT_TX_ERR_INV_ARG;
  }
  /* Send the SFD byte, which is not escaped. */
  xbee_send_byte(dev, XBEE_SFD_BYTE, 0);
  /* API ID + Frame ID + AT Command */
  /* Send the length of the request/command in 2 bytes. */
  uint8_t msb_len = ((cmd->len + 4) >> 8) & 0xff;
  uint8_t lsb_len = (cmd->len + 4) & 0xff;
  /* Send the length bytes with escape option set. */
  xbee_send_byte(dev, msb_len, 1);
  xbee_send_byte(dev, lsb_len, 1);
  /* Send the API ID and the Frame ID. */
  xbee_send_byte(dev, XBEE_AT_CMD_REQ_ID, 1);
  xbee_send_byte(dev, cmd->frame_id, 1);
  /* Send the AT command string. */
  xbee_send_byte(dev, cmd->cmd_id[0], 1);
  xbee_send_byte(dev, cmd->cmd_id[1], 1);
  /* Variable will hold the checksum which is the last byte. */
  uint8_t checksum = 0;

  /* Start computing the checksum. Starts at API ID. */
  checksum += XBEE_AT_CMD_REQ_ID;
  checksum += cmd->frame_id;
  checksum += cmd->cmd_id[0];
  checksum += cmd->cmd_id[1];

  /* Perhaps the command has data */
  for (i=0; i<cmd->len; i++) {
    /* Send byte from the frame data. */
    xbee_send_byte(dev, cmd->data[i], 1);
    len++;
    /* And add to the checksum calculation. */
    checksum += cmd->data[i];
  }
  
  /* perform 2s complement */
  checksum = 0xff - checksum;

  /* send checksum */
  xbee_send_byte(dev, checksum, 1);
  return XBEE_TX_PKT_TX_OK;
}
/*---------------------------------------------------------------------------*/
static uint8_t
xbee_poll_process(void)
{
  if (!process_is_running(&xbee_serial_line_process)) {
    return 0;
  }
  if (xbee_num_of_pend_polls < 1) {
    if (process_post(&xbee_serial_line_process,
      xbee_serial_line_event_message, NULL) == PROCESS_ERR_FULL) {
        PRINTF("xbee: event-queue-full\n");
      return 0;
    } else {
      xbee_num_of_pend_polls = 1;
    }
  } else {
    process_poll(&xbee_serial_line_process);
  }
  return 1;
}
/*---------------------------------------------------------------------------*/
static uint8_t
xbee_serial_line_irq_handler(xbee_device_t *dev, unsigned char c)
{
  static uint8_t overflow = 0; /* Buffer overflow: ignore until END */ 
  if(!overflow) {
    /* Add character */
    if(ringbuf_put(dev->buf, c) == 0) {
      /* Buffer overflow */
      overflow = 1;
      dev->stats->ovf_err++;
      PRINTF("xbee:[%u]ovf\n",dev->sdev);
    }
  } else {
    /* Buffer previously overflowed */
    if(ringbuf_put(dev->buf, c) != 0) {
      overflow = 0;
    }
  }
#if XBEE_HANDLE_SYNC_OPERATIONS
  static uint8_t online_overflow = 0; 
  if (xbee_drv_has_pending_operations(dev)) {
    /* A command or status response is pending. However, the driver 
     * may be in the process of receiving a frame. The response 
     * must be handled on-line, but the frame response should not.
     */		
    if (ringbuf_put(dev->onbuf, c) == 0) {
      if (!online_overflow) {
        online_overflow = 1;
        dev->stats->ovf_on_err++;
        PRINTF("xbee:[%u]on-ovf\n", dev->sdev);
      }
    } else {
      if (online_overflow) {
        online_overflow = 0;
      }
      /* Process on lower-priority IRQ context */
      if (!pend_sv_register_handler((pend_sv_callback_t)xbee_drv_check_response,
        dev)) {
        PRINTF("xbee: cannot-reg-pend:%u\n", dev->sdev);
      }
    }
  }
#endif /* XBEE_HANDLE_SYNC_OPERATIONS */

  /* Wake up consumer process */
  return xbee_poll_process();
}
/*---------------------------------------------------------------------------*/
static int
xbee_serial_line_input_byte(unsigned char c)
{
  return xbee_serial_line_irq_handler(&xbee_dev, c);
}
/*---------------------------------------------------------------------------*/
#if XBEE_WITH_DUAL_RADIO
static int
xbee_serial_line0_input_byte(unsigned char c)
{
  return xbee_serial_line_irq_handler(&xbee_dev0, c);
}
#endif /* XBEE_WITH_DUAL_RADIO */
/*---------------------------------------------------------------------------*/
void
xbee_serial_line_init(unsigned long baudrate)
{
  /* Configure reset pin for hard reset */
  configure_output_pin(XBEE_RESET_PIN, HIGH, DISABLE, DISABLE);
  ringbuf_init(&rxbuf, rxbuf_data, sizeof(rxbuf_data));
  memset(&xbee_stats, 0, sizeof(xbee_stats));
#if XBEE_HANDLE_SYNC_OPERATIONS
  ringbuf_init(&rx_online_buf, rx_online_buf_data, sizeof(rx_online_buf_data));
  pend_sv_init();
#endif
#if XBEE_WITH_DUAL_RADIO
  configure_output_pin(XBEE_RESET_PIN_SEC, HIGH, DISABLE, DISABLE);
  ringbuf_init(&rxbuf0, rxbuf0_data, sizeof(rxbuf0_data));
  memset(&xbee_stats0, 0, sizeof(xbee_stats0));
#if XBEE_HANDLE_SYNC_OPERATIONS
  ringbuf_init(&rx_online_buf0, rx_online_buf0_data, sizeof(rx_online_buf0_data));
#endif
#endif /* XBEE_WITH_DUAL_RADIO */
  /* After process has started, enable UART RX IRQs */
  if (!process_is_running(&xbee_serial_line_process))
    process_start(&xbee_serial_line_process, NULL);
  if (baudrate != 0) {
    xbee_dev.baudrate = baudrate;
    xbee_serial_reset();
    xbee_serial_init(baudrate);
#if XBEE_WITH_DUAL_RADIO
    xbee_dev0.baudrate = baudrate;
    xbee_serial0_reset();
    xbee_serial0_init(baudrate);
#endif /* XBEE_WITH_DUAL_RADIO */
  } else {
    xbee_dev.baudrate = XBEE_BAUDRATE;
    xbee_serial_init(XBEE_BAUDRATE);
#if XBEE_WITH_DUAL_RADIO
    xbee_dev0.baudrate = XBEE_BAUDRATE;
    xbee_serial0_init(XBEE_BAUDRATE);
#endif /* XBEE_WITH_DUAL_RADIO */
  }
  xbee_serial_set_input(xbee_serial_line_input_byte);
  xbee_serial_enable_rx_interrupt();
#if XBEE_WITH_DUAL_RADIO
  xbee_serial0_set_input(xbee_serial_line0_input_byte);
  xbee_serial0_enable_rx_interrupt();
#endif /* XBEE_WITH_DUAL_RADIO */
}
/*---------------------------------------------------------------------------*/
void
xbee_serial_line_reset(xbee_device_t *device)
{
  if (device->sdev == XBEE_SERIAL_DEV_PRIMAL) {
    xbee_serial_reset();
    xbee_serial_init(device->baudrate);
    xbee_serial_enable_rx_interrupt();
#if XBEE_WITH_DUAL_RADIO
  } else if (device->sdev == XBEE_SERIAL_DEV_SECOND) {
    xbee_serial0_reset();
    xbee_serial0_init(device->baudrate);
    xbee_serial0_enable_rx_interrupt();
#endif
  }
  if (!process_is_running(&xbee_driver_process)) {
    process_start(&xbee_driver_process, NULL);
  }
}
/*---------------------------------------------------------------------------*/
void
xbee_do_hw_reset(xbee_device_t *device)
{
  PRINTF("do-rst:%u\n", device->sdev);
  device->stats->rst_count++;
  if (device->sdev == XBEE_SERIAL_DEV_PRIMAL) {
    ringbuf_init(&rxbuf, rxbuf_data, sizeof(rxbuf_data));
#if XBEE_HANDLE_SYNC_OPERATIONS
    ringbuf_init(&rx_online_buf, rx_online_buf_data, sizeof(rx_online_buf_data));
#endif
    wire_digital_write(XBEE_RESET_PIN, LOW);
    watchdog_periodic();
    delay_ms(100);
	 wire_digital_write(XBEE_RESET_PIN, HIGH);
#if XBEE_WITH_DUAL_RADIO
  } else if (device->sdev == XBEE_SERIAL_DEV_SECOND) {
    ringbuf_init(&rxbuf0, rxbuf0_data, sizeof(rxbuf0_data));
#if XBEE_HANDLE_SYNC_OPERATIONS
    ringbuf_init(&rx_online_buf0, rx_online_buf0_data, sizeof(rx_online_buf0_data));
#endif
	 wire_digital_write(XBEE_RESET_PIN_SEC, LOW);
	 watchdog_periodic();
	 delay_ms(100);
	 wire_digital_write(XBEE_RESET_PIN_SEC, HIGH);
#endif /* XBEE_WITH_DUAL_RADIO */
  }
}
/*---------------------------------------------------------------------------*/
static void
xbee_process_byte(xbee_device_t *dev, unsigned char c)
{
  /* There are data in the input. We need to check 
   * how much this data is; and whether a response
   * is completed or we should wait for more bytes
   */                     
  if (dev->ptr == 0) {
    /* If the pointer is zero, we have just received
     * the first byte of the response. We check if it
     * has the standard value of the SFD [0x7e].
     */
    if (c != XBEE_SFD_BYTE) {
      /* This is an error. */
      PRINTF("xbee: SFD-err:%02x %02x\n", c, dev->sdev);
      dev->stats->sfd_err++;
    } else {
      /* The first byte was the Start-of-Frame Delimiter. */
      dev->ptr++;
    }
  } else if (dev->ptr == 1) {
    /* If the pointer is one, we have just received 
     * the second byte of the response. We check if
     * it is zero, because it should be the MSB of 
     * the response length and we do not expect any
     * response larger than 255 bytes.
     */
    if (c != 0) {
      /* This is an error. */
      PRINTF("xbee: MSB-err%02x\n", c);
      dev->stats->msb_err++;
      /* Start over the reading. */
      dev->ptr = 0;
    } else {
      /* The second byte was correct. */
      dev->ptr++;
    }
  } else if (dev->ptr == 2) {
    /* This byte contains the actual response length.
     * We must store it, so we know when we need to
     * terminate the reading from the line.
     *
     * Notice, additionally, that in API 2 mode, this
     * does not include the escape characters, which
     * are possibly transmitted, so in the process of
     * receiving the packet bytes we need to manually
     * check for escape bytes.
     */
    if (dev->esc == 0 && c != XBEE_ESCAPE_CHAR) {
      dev->rsp[0] = c;
      dev->ptr++;
    } else if (dev->esc == 0 && c == XBEE_ESCAPE_CHAR) {
      dev->esc = 1;
    } else if (dev->esc == 1 && c != XBEE_ESCAPE_CHAR) {
      dev->rsp[0] = c^XBEE_ESCAPE_XOR;
      dev->esc = 0;
      dev->ptr++;
   } else {
      dev->esc = 0;
      PRINTF("xbee: LSB-err\n");
      dev->stats->lsb_err++;
      /* Post a serial line event error to the driver process. */
      //process_post(&xbee_serial_line_process, 
        //xbee_serial_line_error_event_message, NULL);
      //xbee_num_of_pend_polls++; So the event is not overwritten by IRQ handler
      /* Start over the reading. */
      dev->ptr = 0;
   }
  } else if (dev->ptr > 2) {
    /* 
     * Actual packet/command payload. We must check,
     * whether we received a SFD without the escape 
     * character flag being set. In such a case, we
     * will assume that this is the starting of new
     * packet.
     */
    if (((uint8_t)c) == XBEE_SFD_BYTE && dev->esc == 0) {
      PRINTF("xbee: ERROR: new packet starts before last finishes!\n");
      /* Start over the reading, but from second byte. */
      dev->ptr = 1;
      dev->stats->pkt_err++;
      /* Post an error message to the driver process. */
      //process_post(&xbee_serial_line_process, 
        //xbee_serial_line_error_event_message, NULL);
      //xbee_num_of_pend_polls++; So the event is not overwritten by IRQ handler
    }
    /* Payload, but before the checksum. */
    if (dev->ptr < (dev->rsp[0] + 3)) {
      /* We are receiving the actual response data. 
       * Check, whether we have received an escape
       * character.
       */
      if (((uint8_t)c) == XBEE_ESCAPE_CHAR) {
        if (dev->esc == 1) {
          /* The escape character flag is set,
           * so this is NOT an escape character.
           */
          c = ((uint8_t)c)^XBEE_ESCAPE_XOR;
          dev->rsp[dev->ptr - 2] = (uint8_t)c;
          /* Increment the pointer after storing the data. */
          dev->ptr++;
          /* Clear the escape next character flag. */
          dev->esc = 0;
        } else {
          /* This is an escape character, so we
           * set the escape next character flag. 
           */
          dev->esc = 1;
        }
      } else {
        /* We have received an ordinary character. */
        if (dev->esc == 1) {
          /* Clear flag and compute actual character. */
          dev->esc = 0;
          c = ((uint8_t)c)^XBEE_ESCAPE_XOR;
        }
        /* Store character. */
        dev->rsp[dev->ptr - 2] = (uint8_t)c;
        /* Increment the pointer after storing the data. */
        dev->ptr++;
      }
    } else if (dev->ptr == (dev->rsp[0] + 3)) {
      /* This is the checksum byte. We might want to do
       * something with it. And of course, now we send 
       * the RX post event to the processes that listen
       * for it, and re-set the receiving process. Note
       * that we need to put a stop in the end, so that
       * the remote process knows how much to read from 
       * the buffer.
       */
      if (c == XBEE_ESCAPE_CHAR && dev->esc == 0) {
        /* We will need to wait for one more byte */
        dev->esc = 1;
      } else {
        dev->rsp[dev->ptr - 2] = (uint8_t)(XBEE_STOP_BYTE);
        /* Nullify the pointer and escape flag so a new response can follow up. */
        dev->ptr = 0;
        dev->esc = 0;
        /* Send the packet to the radio driver synchronously */
        process_post_synch(&xbee_driver_process, xbee_drv_device_response,
          dev);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(xbee_serial_line_process, ev, data)
{
  PROCESS_BEGIN();
  /* Allocate an event ID for the XBEE serial line event. */
  xbee_serial_line_event_message = process_alloc_event();
  /* Allocate an event ID for the XBEE serial line error event. */
  xbee_serial_line_error_event_message = process_alloc_event();
  PROCESS_PAUSE();  
  while(1) {
    /* Fill application buffer until frame is completed or timeout */
    int c = ringbuf_get(&rxbuf);
#if XBEE_WITH_DUAL_RADIO
    int c0 = ringbuf_get(&rxbuf0);
    if (c == -1 && c0 == -1) {
#else /* XBEE_WITH_DUAL_RADIO */
    if (c == -1) {
#endif  /* XBEE_WITH_DUAL_RADIO */
      /* Buffer empty, wait for poll */
      PROCESS_WAIT_EVENT();
      xbee_num_of_pend_polls--;
      if (ev == xbee_serial_line_error_event_message) {
        PRINTF("xbee: got-err-msg\n");
        process_post(&xbee_serial_line_process, PROCESS_EVENT_EXIT, NULL);
      } else if (ev == xbee_serial_line_event_message ||
          ev == PROCESS_EVENT_POLL) {
      } else if (ev == PROCESS_EVENT_EXIT) {
        PRINTF("xbee: got-exit-signal\n");
        break;
      } else if (ev == PROCESS_EVENT_EXITED) {
     	  if (data == &xbee_driver_process) {
 	       PRINTF("xbee: xbee-driver has exited. Exiting.\n");
          process_post(&xbee_serial_line_process, PROCESS_EVENT_EXIT, NULL);
        }
      } else {
        PRINTF("xbee: unknown-event:%02x\n", ev);
      }
    } else {
      if (c != -1) {
        xbee_process_byte(&xbee_dev, c);
      }
#if XBEE_WITH_DUAL_RADIO
      if (c0 != -1) {
        xbee_process_byte(&xbee_dev0, c0);
      }
#endif
    }
  }
  /* Let the driver exit */
  if (process_is_running(&xbee_driver_process)) {
    process_post(&xbee_driver_process, PROCESS_EVENT_EXIT, NULL);  
  }
  PRINTF("xbee: exit\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
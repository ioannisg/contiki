/*
 * xbee.c
 *
 * Created: 2014-11-28 14:56:26
 *  Author: Ioannis Glaropoulos
 */ 
#include "contiki.h"
#include "platform/arduino_due_ath9170/xbee/xbee.h"
#include "platform/arduino_due_ath9170/xbee/xbee-api.h"

#include <string.h> /* for memcpy() */
#include <stdio.h>

#include "lib/ringbuf.h"
#include "pend-SV.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef XBEE_SERIAL_LINE_CONF_BUFSIZE
#define BUFSIZE XBEE_SERIAL_LINE_CONF_BUFSIZE
#else /* SERIAL_LINE_CONF_BUFSIZE */
#define BUFSIZE 128
#endif /* SERIAL_LINE_CONF_BUFSIZE */

#if (BUFSIZE & (BUFSIZE - 1)) != 0
#error XBEE_SERIAL_LINE_CONF_BUFSIZE must be a power of two (i.e., 1, 2, 4, 8, 16, 32, 64, ...).
#error "Change XBEE_SERIAL_LINE_CONF_BUFSIZE in contiki-conf.h."
#endif

#ifndef XBEE_CONF_BAUDRATE
#define XBEE_BAUDRATE 115200
#else
#define XBEE_BAUDRATE XBEE_CONF_BAUDRATE
#endif

static struct ringbuf rxbuf;
static uint8_t rxbuf_data[BUFSIZE];

#if XBEE_HANDLE_SYNC_OPERATIONS
static struct ringbuf rx_online_buf;
static uint8_t rx_online_buf_data[128];
#endif

PROCESS(xbee_serial_line_process, "XBEE serial driver");

process_event_t xbee_serial_line_event_message;
process_event_t xbee_serial_line_error_event_message;

static uint8_t xbee_num_of_pend_polls = 0;
  
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
/*---------------------------------------------------------------------------*/
void
xbee_send_byte_stream_down(uint8_t* _data, uint8_t len)
{
  int i;
  for (i=0; i<len; i++) {
    xbee_serial_writeb(_data[i]);
  }
}
/*---------------------------------------------------------------------------*/
static void
xbee_send_byte(uint8_t b, uint8_t escape_if_needed)
{
  if (escape_if_needed && (b == XBEE_SFD_BYTE || b == XBEE_ESCAPE_CHAR ||
                           b == XBEE_RADIO_ON || b == XBEE_RADIO_OFF)) {
    xbee_serial_writeb((unsigned char)XBEE_ESCAPE_CHAR);
    xbee_serial_writeb((unsigned char)(b ^ XBEE_ESCAPE_XOR));
  } else {
    xbee_serial_writeb((unsigned char)b);
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
xbee_send_asynchronous_cmd(const xbee_at_command_t* cmd)
{
  int i, len = 0;
  if (cmd == NULL) {
    PRINTF("xbee: null command.\n");
    return XBEE_TX_PKT_TX_ERR_INV_ARG;
  }
  /* Send the SFD byte, which is not escaped. */
  xbee_send_byte(XBEE_SFD_BYTE, 0);
  /* API ID + Frame ID + AT Command */
  /* Send the length of the request/command in 2 bytes. */
  uint8_t msb_len = ((cmd->len + 4) >> 8) & 0xff;
  uint8_t lsb_len = (cmd->len + 4) & 0xff;
  /* Send the length bytes with escape option set. */
  xbee_send_byte(msb_len, 1);
  xbee_send_byte(lsb_len, 1);
  /* Send the API ID and the Frame ID. */
  xbee_send_byte(XBEE_AT_CMD_REQ_ID, 1);
  xbee_send_byte(cmd->frame_id, 1);
  /* Send the AT command string. */
  xbee_send_byte(cmd->cmd_id[0], 1);
  xbee_send_byte(cmd->cmd_id[1], 1);
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
    xbee_send_byte(cmd->data[i], 1);
    len++;
    /* And add to the checksum calculation. */
    checksum += cmd->data[i];
  }
  
  /* perform 2s complement */
  checksum = 0xff - checksum;

  /* send checksum */
  xbee_send_byte(checksum, 1);
  return XBEE_TX_PKT_TX_OK;
}
/*---------------------------------------------------------------------------*/
static int
xbee_serial_line_input_byte(unsigned char c)
{
  static uint8_t overflow = 0; /* Buffer overflow: ignore until END */
  
  if(!overflow) {
    /* Add character */
    if(ringbuf_put(&rxbuf, c) == 0) {
      /* Buffer overflow */
      overflow = 1;
      printf("xbee: ovf\n");
    }
  } else {
    /* Buffer previously overflowed */
    if(ringbuf_put(&rxbuf, c) != 0) {
      overflow = 0;
    }
  }

#if XBEE_HANDLE_SYNC_OPERATIONS
  static uint8_t online_overflow = 0;
  
  if (xbee_drv_has_pending_operations()) {
    /* A command or status response is pending. However, the driver 
     * may be in the process of receiving a frame. The response 
     * must be handled on-line, but the frame response should not.
     */
    if (ringbuf_put(&rx_online_buf, c) == 0) {
      if (!online_overflow) {
        online_overflow = 1;
        printf("xbee: on-ovf\n");
      }
    } else {
      if (online_overflow) {
        online_overflow = 0;
      }
      pend_sv_register_handler((pend_sv_callback_t)xbee_drv_check_response,
        &rx_online_buf);
    }
  }
#endif /* XBEE_HANDLE_SYNC_OPERATIONS */

  /* Wake up consumer process */
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
void
xbee_serial_line_init(unsigned long baudrate)
{
  ringbuf_init(&rxbuf, rxbuf_data, sizeof(rxbuf_data));
#if XBEE_HANDLE_SYNC_OPERATIONS
  ringbuf_init(&rx_online_buf, rx_online_buf_data, sizeof(rx_online_buf_data));
  pend_sv_init();
#endif
  /* After process has started, enable UART RX IRQs */
  if (!process_is_running(&xbee_serial_line_process))
    process_start(&xbee_serial_line_process, NULL);
  if (baudrate != 0) {
    xbee_serial_reset();
    xbee_serial_init(baudrate);
  }
  else {
    xbee_serial_init(XBEE_BAUDRATE);
  }
  xbee_serial_set_input(xbee_serial_line_input_byte);
  xbee_serial_enable_rx_interrupt();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(xbee_serial_line_process, ev, data)
{
  static char buf[BUFSIZE];
  static uint8_t ptr, escape_next_char;
  /* Variable that holds the XBEE response information. */
  static xbee_response_t *xbee_response;

  PROCESS_BEGIN();
  PROCESS_PAUSE();
  
  /* Allocate an event ID for the XBEE serial line event. */
  xbee_serial_line_event_message = process_alloc_event();
  /* Allocate an event ID for the XBEE serial line error event. */
  xbee_serial_line_error_event_message = process_alloc_event();
  /* Initialize read pointer. */
  ptr = 0;
  escape_next_char = 0;
  /* Attach the incoming buffer data to the XBEE response. */
  xbee_response = (xbee_response_t *)buf;
  
  while(1) {
    /* Fill application buffer until frame is completed or timeout */
    int c = ringbuf_get(&rxbuf);
    if (c == -1) {
      /* Buffer empty, wait for poll */
      PROCESS_WAIT_EVENT();
      xbee_num_of_pend_polls--;
      if (ev == xbee_serial_line_error_event_message) {
        PRINTF("xbee: error message\n");
        process_post(&xbee_serial_line_process, PROCESS_EVENT_EXIT, NULL);
        continue;
      } else if (ev == xbee_serial_line_event_message ||
          ev == PROCESS_EVENT_POLL) {
        continue;
      } else if (ev == PROCESS_EVENT_EXIT) {
        PRINTF("xbee: got-exit-signal\n");
        break;
      } else if (ev == PROCESS_EVENT_EXITED) {
     	  if (data == &xbee_driver_process) {
 	       PRINTF("xbee: xbee-driver has exited. Exiting.\n");
          process_post(&xbee_serial_line_process, PROCESS_EVENT_EXIT, NULL);
        }
		  continue;
      } else {
        PRINTF("xbee: got-unknown/unhandled-event, %02x\n", ev);
        //process_post(&xbee_serial_line_process, PROCESS_EVENT_EXIT, NULL);
        continue;
		}
    } else {
      /* There are data in the input. We need to check 
       * how much this data is; and whether a response
       * is completed or we should wait for more bytes
       */                     
      if (ptr == 0) {
        /* If the pointer is zero, we have just received
         * the first byte of the response. We check if it
         * has the standard value of the SFD [0x7e].
         */
        if (c != XBEE_SFD_BYTE) {
          /* This is an error. */
          PRINTF("xbee: SFD-err:%02x\n", c);
        } else {
          /* The first byte was the Start-of-Frame Delimiter. */
          ptr++;
        }
      } else if (ptr == 1) {
        /* If the pointer is one, we have just received 
         * the second byte of the response. We check if
         * it is zero, because it should be the MSB of 
         * the response length and we do not expect any
         * response larger than 255 bytes.
         */
        if (c != 0) {
          /* This is an error. */
          PRINTF("xbee: MSB-err%02x\n", c);
          /* Start over the reading. */
          ptr = 0;
        } else {
          /* The second byte was correct. */
          ptr++;
        }
      } else if (ptr == 2) {
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
        if (escape_next_char == 0 && c != XBEE_ESCAPE_CHAR) {
          buf[0] = c;
          ptr++;
        } else if (escape_next_char == 0 && c == XBEE_ESCAPE_CHAR) {
          escape_next_char = 1;
        } else if (escape_next_char == 1 && c != XBEE_ESCAPE_CHAR) {
          buf[0] = c^XBEE_ESCAPE_XOR;
          escape_next_char = 0;
          ptr++;
       } else {
          escape_next_char = 0;
          PRINTF("xbee: LSB-err\n");
          /* Post a serial line event error to the driver process. */
          process_post(&xbee_serial_line_process, 
            xbee_serial_line_error_event_message, NULL);
          /* Start over the reading. */
          ptr = 0;
       }
      } else if (ptr > 2) {
        /* 
         * Actual packet/command payload. We must check,
         * whether we received a SFD without the escape 
         * character flag being set. In such a case, we
         * will assume that this is the starting of new
         * packet.
         */
        if (((uint8_t)c) == XBEE_SFD_BYTE && escape_next_char == 0) {
          PRINTF("xbee: ERROR: new packet starts before last finishes!\n");
          /* Start over the reading, but from second byte. */
          ptr = 1;
          /* Post an error message to the driver process. */
          //process_post(&xbee_serial_line_process, 
            //xbee_serial_line_error_event_message, NULL);
          continue;
        }
        /* Payload, but before the checksum. */
        if (ptr < (buf[0] + 3)) {
          /* We are receiving the actual response data. 
           * Check, whether we have received an escape
           * character.
           */
          if (((uint8_t)c) == XBEE_ESCAPE_CHAR) {
            if (escape_next_char == 1) {
              /* The escape character flag is set,
               * so this is NOT an escape character.
               */
              c = ((uint8_t)c)^XBEE_ESCAPE_XOR;
              buf[ptr - 2] = (uint8_t)c;
              /* Increment the pointer after storing the data. */
              ptr++;
              /* Clear the escape next character flag. */
              escape_next_char = 0;
            } else {
              /* This is an escape character, so we
               * set the escape next character flag. 
               */
              escape_next_char = 1;
            }
          } else {
            /* We have received an ordinary character. */
            if (escape_next_char == 1) {
              /* Clear flag and compute actual character. */
              escape_next_char = 0;
              c = ((uint8_t)c)^XBEE_ESCAPE_XOR;
            }
            /* Store character. */
            buf[ptr - 2] = (uint8_t)c;
            /* Increment the pointer after storing the data. */
            ptr++;
          }
        } else if (ptr == (buf[0] + 3)) {
          /* This is the checksum byte. We might want to do
           * something with it. And of course, now we send 
           * the RX post event to the processes that listen
           * for it, and re-set the receiving process. Note
           * that we need to put a stop in the end, so that
           * the remote process knows how much to read from 
           * the buffer.
           */
          if (c == XBEE_ESCAPE_CHAR && escape_next_char == 0) {
            /* We will need to wait for one more byte */
            escape_next_char = 1;
          } else {
            buf[ptr - 2] = (uint8_t)(XBEE_STOP_BYTE);
            /* Nullify the pointer and escape flag so a new response can follow up. */
            ptr = 0;
            escape_next_char = 0;
            /* Send the packet to the radio driver synchronously */
            process_post_synch(&xbee_driver_process, xbee_drv_device_response,
                xbee_response);
			 }
        }
      }
    }
  }
  PRINTF("xbee: exit\n");
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
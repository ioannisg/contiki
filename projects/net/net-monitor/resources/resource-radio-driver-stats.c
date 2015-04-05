/*
 * resource_driver_stats.c
 *
 * Created: 2015-01-19 22:46:38
 *  Author: ioannisg
 */ 
#include "contiki-net.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rest-engine.h"
#include "net-monitor.h"
#include "xbee/xbee-public.h"
#include "sicslowpan.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define SPRINTLLADDR(buf, addr) sprintf(buf, "02x:02x:02x:02x:02x:02x:02x:02x", \
  addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7])
#else
#define PRINTF(...)
#define SPRINT6ADDR(buf, addr)
#endif


/*---------------------------------------------------------------------------*/
static char radio_stats[REST_MAX_CHUNK_SIZE];
static uint16_t radio_stats_rsp_len = 0;
/*---------------------------------------------------------------------------*/
static void radio_stats_get_periodic_handler(void *request, void *response,
  uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void radio_stats_periodic_handler(void);
/*---------------------------------------------------------------------------*/
PERIODIC_RESOURCE(res_radio_stats_periodic,
  "title=\"Periodic demo\";obs",
  radio_stats_get_periodic_handler,
  NULL,
  NULL,
  NULL,
  60 * CLOCK_SECOND,
  radio_stats_periodic_handler);
/*---------------------------------------------------------------------------*/
static void
sprint_radio_stats(uint8_t err_stats)
{
  memset(radio_stats, 0, sizeof(radio_stats));
  sprint_time(radio_stats, uptime_min);
  if(err_stats) {
    radio_stats_rsp_len = 
      NETSTACK_RADIO.print_err_stats(&radio_stats[0], sizeof(radio_stats), 0);
  } else {
    radio_stats_rsp_len = NETSTACK_RADIO.print_traffic_stats(&radio_stats[0], 
      sizeof(radio_stats), 0);
  }
  radio_stats_rsp_len =
    (radio_stats_rsp_len > REST_MAX_CHUNK_SIZE) ? REST_MAX_CHUNK_SIZE : radio_stats_rsp_len;
}
/*---------------------------------------------------------------------------*/
static void
radio_stats_periodic_handler(void)
{
  /* Print RADIO device error log */
  sprint_radio_stats(1);
  REST.notify_subscribers(&res_radio_stats_periodic);
}
/*---------------------------------------------------------------------------*/
static void
radio_stats_get_periodic_handler(void *request, void *response,
  uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_max_age(response, 
    res_radio_stats_periodic.periodic->period / CLOCK_SECOND);
  /* Print whatever is written in the radio stats */
  memcpy(buffer, radio_stats, radio_stats_rsp_len);
  REST.set_response_payload(response, buffer, radio_stats_rsp_len);
}
/*---------------------------------------------------------------------------*/
static void
radio_stats_get_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset)
{
  const char *len = NULL;
  char message[REST_MAX_CHUNK_SIZE];
  memset(message, 0, REST_MAX_CHUNK_SIZE);
  int length = 
    NETSTACK_RADIO.print_err_stats(&message[0], REST_MAX_CHUNK_SIZE, 0);

  if(REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if(length < 0) {
      length = 0;
    }
    if(length > REST_MAX_CHUNK_SIZE) {
      length = REST_MAX_CHUNK_SIZE;
    }
  } else {
    length = (length > REST_MAX_CHUNK_SIZE) ? REST_MAX_CHUNK_SIZE : length;
  }
  memcpy(buffer, message, length);

  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
/*----------------------------------------------------------------------------*/
static void
radio_stats_post_put_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  uint8_t length;
  const char *stats = NULL;
  uint8_t success = 0;
  char message[REST_MAX_CHUNK_SIZE];
  memset(message, 0, sizeof(message));

  if((len = REST.get_post_variable(request, "stats", &stats))) {
    if(strncmp(stats, "error", len) == 0) {
      success = 1;
      sprint_time(&message[0], uptime_min);
      length = 
        NETSTACK_RADIO.print_err_stats(&message[0], REST_MAX_CHUNK_SIZE, 0);
    } else if(strncmp(stats, "traffic", len) == 0) {
      success = 1;
      sprint_time(&message[0], uptime_min);
      length =
        NETSTACK_RADIO.print_traffic_stats(&message[0], REST_MAX_CHUNK_SIZE, 0);
    }
  }
  if(success == 0) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
  } else {
    memcpy(buffer, message, length);
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
/*---------------------------------------------------------------------------*/
RESOURCE(res_radio_stats,
"title=\"RADIO statistics: ?len=0..\";rt=\"Text\"",
  radio_stats_get_handler,
  radio_stats_post_put_handler,
  radio_stats_post_put_handler,
  NULL);
/*---------------------------------------------------------------------------*/

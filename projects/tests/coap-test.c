/*
 * coap_test.c
 *
 * Created: 2015-02-14 11:23:15
 *  Author: ioannisg
 */
#include "contiki-net.h"
#include "er-coap.h"
#include "er-coap-engine.h"

#ifndef COAP_TEST_PERIOD_MIN
#define COAP_TEST_PERIOD_MIN 1
#endif

#define DEBUG DEBUG_NONE
#include "net/ip/uip-debug.h"

#if DEBUG
#include "resolv.h"
#endif

#define LOCAL_PORT      UIP_HTONS(COAP_DEFAULT_PORT + 1)
#define REMOTE_PORT     UIP_HTONS(COAP_DEFAULT_PORT)

/* Example URIs that can be queried. */
#define NUM_OF_URLS 2
/* leading and ending slashes only for demo purposes, 
 * get cropped automatically when setting the Uri-Path
 */
static const char *service_urls[NUM_OF_URLS] = { ".well-known/core",  "time"};
static struct etimer et;
static uint16_t coap_test_valid_rsp = 0;
/*---------------------------------------------------------------------------*/
uint16_t
coap_test_get_rsp_count(void)
{
  return coap_test_valid_rsp;
}
/*---------------------------------------------------------------------------*/
/* This function is passed to COAP_BLOCKING_REQUEST() to handle responses. */
static void
client_chunk_handler(void *response)
{
  const uint8_t *chunk;

  int len = coap_get_payload(response, &chunk);
  if (len > 9) {
    coap_test_valid_rsp++;
  }
#if DEBUG
  PRINTF("coap-client:|%.*s", len, (char *)chunk);
#endif
}
/*---------------------------------------------------------------------------*/
PROCESS(coap_client_test_process, "coap-client-test");
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(coap_client_test_process, ev, data)
{
  PROCESS_BEGIN();
  UNUSED(data);
  /* This way the packet can be treated as pointer as usual. */
  static coap_packet_t request[1];
  /* receives all CoAP messages */
  coap_init_engine();
  /* Prepare request, TID is set by COAP_BLOCKING_REQUEST() */
  coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
  coap_set_header_uri_path(request, service_urls[1]);
  etimer_set(&et, CLOCK_SECOND*60*COAP_TEST_PERIOD_MIN);
  
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    static uip_ipaddr_t *jupiter_addr;
    static resolv_status_t status;
    if ((status = resolv_lookup("jupiter.s3.kth.se", &jupiter_addr))
      == RESOLV_STATUS_CACHED) {
      PRINT6ADDR(jupiter_addr);
      PRINTF("\n");
      const char msg[] = "test";
      coap_set_payload(request, (uint8_t *)msg, sizeof(msg) - 1);
      PRINTF("sending coap req\n");
      COAP_BLOCKING_REQUEST(jupiter_addr, REMOTE_PORT, request, client_chunk_handler);
    } else {
      PRINTF("jupiter status :%u\n", status);
      resolv_query("jupiter.s3.kth.se");
    }
    etimer_set(&et, CLOCK_SECOND*60*COAP_TEST_PERIOD_MIN);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
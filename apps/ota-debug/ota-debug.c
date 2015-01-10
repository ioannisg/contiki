/*
 * ota_debug.c
 *
 * Created: 2014-09-20 01:28:43
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se>
 */ 

#include "contiki-net.h"
#include "apps/ota-debug/ota-debug.h"
#include <stdio.h>
#include "string.h"
#include "memb.h"
#include "list.h"

char ota_debug_buf[128];
  
typedef struct ota_debug_string {
  struct ota_debug_string *next;
  char buf[128];
} ota_debug_string_t;

static ota_debug_string_t *udp_msg;

MEMB(ota_debug_mem, ota_debug_string_t, 4);
LIST(ota_debug_list);

static uint8_t otg_debug_initialized = 0;
static struct uip_udp_conn *otg_client_conn;
static uip_ipaddr_t otg_server_ipaddr;

PROCESS(ota_debug_process, "OTA_DEBUG_PROCESS");
/*---------------------------------------------------------------------------*/

/*
 * Initialize UDP connection to the remote host for sending debug messages.
 */
uint8_t
otg_debug_init_connection(void)
{
  if (otg_debug_initialized)
    return 1;
  
  memb_init(&ota_debug_mem);
  list_init(ota_debug_list);
  
  if (!process_is_running(&tcpip_process))
    return 0;

  /* Set up the remote (server) address */
  uip_ip6addr(&otg_server_ipaddr, 
    OTG_SERVER_ADDR_0,
	 OTG_SERVER_ADDR_1,
	 OTG_SERVER_ADDR_2,
	 OTG_SERVER_ADDR_3,
	 OTG_SERVER_ADDR_4,
	 OTG_SERVER_ADDR_5,
	 OTG_SERVER_ADDR_6,
	 OTG_SERVER_ADDR_7);

  /* New connection with remote host */
  otg_client_conn = udp_new(NULL, UIP_HTONS(OTG_UDP_SERVER_PORT), NULL);
  if(otg_client_conn == NULL) {
    return 0;
  }
  udp_bind(otg_client_conn, UIP_HTONS(OTG_UDP_CLIENT_PORT));

  otg_debug_initialized = 1;
  process_start(&ota_debug_process, NULL);
  return 1;
}
/*---------------------------------------------------------------------------*/

/*
 * Send a debug message to the remote host.
 */
void
otg_debug_send_debug_msg(void)
{
  ota_debug_string_t* new_msg = NULL;
  if (!otg_debug_initialized)
    return;
	 
  new_msg = (ota_debug_string_t*)memb_alloc(&ota_debug_mem);

  if (new_msg == NULL) {
    return;
  }
  memcpy(&new_msg->buf[0], &ota_debug_buf[0], strlen(ota_debug_buf)+1);
  list_add(ota_debug_list, new_msg); 
}
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(ota_debug_process, ev, data)
{
  PROCESS_BEGIN();
  
  UNUSED(data);
  
  while(1) {
	  
    if (list_head(ota_debug_list) != NULL) {
      INTERRUPTS_DISABLE();
      udp_msg = (ota_debug_string_t *)list_pop(ota_debug_list);
		INTERRUPTS_ENABLE();
	   if (udp_msg == NULL)
		  continue;
		uip_udp_packet_sendto(otg_client_conn, &udp_msg->buf[0], 
        strlen(&udp_msg->buf[0])+1, &otg_server_ipaddr, UIP_HTONS(OTG_UDP_SERVER_PORT));
      memb_free(&ota_debug_mem, udp_msg);
	 }
	 PROCESS_PAUSE();
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
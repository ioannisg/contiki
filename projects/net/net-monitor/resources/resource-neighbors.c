/*
 * resource_neighbors.c
 *
 * Created: 2015-01-18 00:47:53
 *  Author: Ioannis Glaropoulos
 */ 

#include "contiki-net.h"
#include <stdlib.h>
#include <string.h>
#include "net-monitor.h"
#include "rest-engine.h"
#include "uip-ds6-nbr.h"

/*---------------------------------------------------------------------------*/
static uint16_t
print_neighbors(char *msg)
{
  sprint_time(msg, uptime_min);
  sprintf(msg+strlen(msg),"N%u\n", uip_ds6_nbr_num());
  uip_ds6_nbr_t *nbr = nbr_table_head(ds6_neighbors);
  while (nbr != NULL) {
    if (strlen(msg) > REST_MAX_CHUNK_SIZE) {
      return strlen(msg);
    }
    sprint_addr6(msg, &nbr->ipaddr);
#if UIP_MULTI_IFACES
    sprintf(msg+strlen(msg), ";%u", nbr->iface->ll_type);
#endif
    sprintf(msg+strlen(msg), "\n");
    nbr = nbr_table_next(ds6_neighbors, nbr);
  }
  return strlen(msg);
}
/*---------------------------------------------------------------------------*/
static uint16_t
sprint_nbr_json(char *msg, uint16_t len, uip_ds6_nbr_t *nbr)
{
  uint16_t offset = 0;
  char nbr_state[12];
  uip_lladdr_t *laddr = NULL;

  memset(msg, 0, len);
  snprintf(&msg[offset], len-offset-1, "{\"ipv6\": \"");
  offset = strlen(msg);
  sprint_addr6(&msg[offset], &nbr->ipaddr);
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, "\",\"link\": \"");
  offset = strlen(msg);

  if(uip_ds6_nbr_lladdr_from_ipaddr(&nbr->ipaddr) != NULL) {
    laddr = uip_ds6_nbr_lladdr_from_ipaddr(&nbr->ipaddr);
#if LINKADDR_SIZE == 8
    snprintf(&msg[offset], len-offset-1, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
      laddr->addr[0], laddr->addr[1], laddr->addr[2], laddr->addr[3],
      laddr->addr[4], laddr->addr[5], laddr->addr[6], laddr->addr[7]);
#elif LINKADDR_SIZE == 6
    snprintf(&msg[offset], len-offset-1, "%02x:%02x:%02x:%02x:%02x:%02x",
      laddr->addr[0], laddr->addr[1], laddr->addr[2],
      laddr->addr[3], laddr->addr[4], laddr->addr[5]);
#endif
  } else {
    snprintf(&msg[offset], len-offset-1, "unknown");
  }
  offset = strlen(msg);
  switch(nbr->state) {
  case NBR_INCOMPLETE:
    sprintf(nbr_state, "INCOMPLETE");
    break;
  case NBR_REACHABLE:
    sprintf(nbr_state, "REACHABLE");
    break;
  case NBR_STALE:
    sprintf(nbr_state, "STALE");
    break;
  case NBR_DELAY:
    sprintf(nbr_state, "DELAY");
    break;
  default:
    sprintf(nbr_state, "unknown");
    break;
  }
  snprintf(&msg[offset], len-offset-1, "\", \"state\": \"");
  offset = strlen(msg);
  strncpy(&msg[offset], nbr_state, len-offset-1);
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, "\"");
  offset = strlen(msg);

#if UIP_MULTI_IFACES
  snprintf(&msg[offset], len-offset-1, ", \"interface\": \"");
  offset = strlen(msg);
  if(nbr->iface != NULL) {
    snprintf(&msg[offset], len-offset-1, "%u\"", nbr->iface->ll_type);
  } else {
    snprintf(&msg[offset], len-offset-1, "0\"");
  }
  offset = strlen(msg);
#endif /* UIP_MULTI_IFACES */
  snprintf(&msg[offset], len-offset-1, "}");
  return strlen(msg);
}
/*---------------------------------------------------------------------------*/
static void res_get_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset);
static void res_put_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset);
/*---------------------------------------------------------------------------*/
/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
RESOURCE(res_neighbors,
         "title=\"IPv6 neighbors: ?len=0..\";rt=\"Text\"",
         res_get_handler,
         NULL,
         res_put_handler,
         NULL);
/*---------------------------------------------------------------------------*/
static void
res_get_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset)
{
  const char *len = NULL;
  /* Some data that has the length up to REST_MAX_CHUNK_SIZE. 
   * For more, see the chunk resource.
   */
  char message[2*REST_MAX_CHUNK_SIZE];
  memset(message, 0, REST_MAX_CHUNK_SIZE);
  int length = print_neighbors(message);
	
  /* The query string can be retrieved by rest_get_query() or parsed for its key-value pairs. */
  if(REST.get_query_variable(request, "len", &len)) {
    length = atoi(len);
    if(length < 0) {
      length = 0;
    }
    if(length > REST_MAX_CHUNK_SIZE) {
      length = REST_MAX_CHUNK_SIZE;
    }
    memcpy(buffer, message, length);
  } else {
    memcpy(buffer, message, length);
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
/*---------------------------------------------------------------------------*/
static void
res_put_handler(void * request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *neighbor = NULL;
  int success = 1;
  int nbr_index = -1;
  uint16_t length = 0;
  char message[256];
  
  if((len = REST.get_post_variable(request, "neighbor", &neighbor))) {
    nbr_index = atoi(neighbor);
    if(nbr_index < 0 || nbr_index >= uip_ds6_nbr_num()) {
      success = 0;
    } else {
      uint8_t index = 0;
      uip_ds6_nbr_t *nbr = nbr_table_head(ds6_neighbors);
      while(index < nbr_index && nbr != NULL) {
        index++;
        nbr = nbr_table_next(ds6_neighbors, nbr);
      }
      if(nbr == NULL) {
        success = 0;
      } else {
        sprint_nbr_json(message, 256, nbr);
        length = strlen(message);
      }
    }
  }
  if(success == 0) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    length = 0;
  } else {
    memcpy(buffer, message, strlen(message));
    length = strlen(message);
  }
  REST.set_header_content_type(response, REST.type.APPLICATION_JSON);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
/*---------------------------------------------------------------------------*/

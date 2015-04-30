/*
 * resource_rpl.c
 *
 * Created: 2015-04-25 10:07:53
 *  Author: Ioannis Glaropoulos
 */ 

#include "contiki-net.h"
#include <stdlib.h>
#include <string.h>
#include "net-monitor.h"
#include "rest-engine.h"
#include "rpl.h"
#include "rpl-private.h"

typedef struct {
  rpl_instance_t *instance;
  struct uip_icmp6_echo_reply_notification ping_handler;
  struct ctimer ping_timer;
  uint8_t requests;
  uint8_t replies;
  uint8_t lost;
  uint8_t pending;
  uip_ipaddr_t host_addr;
#if UIP_MULTI_IFACES
  uip_ds6_iface_t *iface;
#endif
} ping_parent_info_t;
static ping_parent_info_t ping_info;
/*---------------------------------------------------------------------------*/
static uint16_t
print_generic_rpl_dag_info(char *msg, uint16_t len, rpl_dag_t *dag)
{
  uint16_t offset;
  snprintf(msg, len - 1, "DAG-id: ");
  offset = strlen(msg);
  if((offset + 32) < len) {
    sprint_addr6(&msg[offset], &dag->dag_id);
  }
  return strlen(msg);
}
/*---------------------------------------------------------------------------*/
static uint16_t
print_generic_rpl_info(char *msg, uint16_t len)
{
  rpl_instance_t *instance;
  rpl_dag_t *dag;
  uint16_t offset = 0;
  int i;

  sprint_time(msg, uptime_min);
  offset = strlen(msg);
  for(i=0; i<RPL_MAX_INSTANCES; i++) {
    instance = &instance_table[i];
    if(instance->used) {
      snprintf(&msg[offset], len - offset - 1, "Instance:%u,", i);
      offset = strlen(msg);
      dag = instance->current_dag;
      if(dag != NULL) {
        offset += print_generic_rpl_dag_info(&msg[offset], len - offset, dag);
      } else {
        snprintf(&msg[offset], len - offset - 1, "no-dag");
        offset = strlen(msg);
      }
    }
  }
  return strlen(msg);   
}
/*---------------------------------------------------------------------------*/
static uint16_t
sprint_rpl_info_json(char *msg, uint16_t len, rpl_instance_t *instance)
{
  uint16_t offset = 0;
  snprintf(&msg[offset], len-offset-1, "{\"instance-id\": \"");
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, "%u\"", instance->instance_id);
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, ", \"dag-id\": \"");
  offset = strlen(msg);
  sprint_addr6(&msg[offset], &instance->current_dag->dag_id);
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, "\", \"dag-version\": \"");
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, "%u\"", instance->current_dag->version);  
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, ", \"parent\": \"");
  offset = strlen(msg);
  if(instance->current_dag->preferred_parent != NULL) {
    sprint_addr6(&msg[offset],
      rpl_get_parent_ipaddr(instance->current_dag->preferred_parent));
    offset = strlen(msg);
    snprintf(&msg[offset], len-offset-1, "\"");
  } else {
    snprintf(&msg[offset], len-offset-1, "none\"");
  }
  offset = strlen(msg);
  snprintf(&msg[offset], len-offset-1, ", \"rank\": \"%u.%02u\"",
    (instance->current_dag->rank / RPL_DAG_MC_ETX_DIVISOR),
    (100 * (instance->current_dag->rank % RPL_DAG_MC_ETX_DIVISOR)) / RPL_DAG_MC_ETX_DIVISOR);
  offset = strlen(msg);
#if UIP_MULTI_IFACES
  snprintf(&msg[offset], len-offset-1, ", \"interface\": \"");
  offset = strlen(msg);
  if(nbr->iface != NULL) {
    snprintf(&msg[offset], len-offset-1, "%u\"", instance->current_dag->iface->ll_type);
  } else {
    snprintf(&msg[offset], len-offset-1, "0\"");
  }
  offset = strlen(msg);
#endif /* UIP_MULTI_IFACES */
  snprintf(&msg[offset], len-offset-1, "}");
  return strlen(msg);
}
/*---------------------------------------------------------------------------*/
static void
parent_echo_reply_callback(uip_ipaddr_t *addr, uint8_t ttl, uint8_t *data, 
  uint16_t datalen)
{
  /* Increment counter if source matches the parent and a reply is pending */
  if(ping_info.pending && uip_ipaddr_cmp(&ping_info.host_addr, addr)) {
    ping_info.pending = 0;
    ping_info.replies++;
    uip_icmp6_echo_reply_callback_rm(&ping_info.ping_handler);
    ctimer_stop(&ping_info.ping_timer);
  }
}
/*---------------------------------------------------------------------------*/
static void
ping_timeout_handler(void *ptr)
{
  UNUSED(ptr);
  if(ping_info.pending) {
    ping_info.pending = 0;
    ping_info.lost++;
  }
}
/*---------------------------------------------------------------------------*/
static void
send_ping_request(void *ptr)
{
  UNUSED(ptr);
  /* Register echo reply handler */
  uip_icmp6_echo_reply_callback_add(&ping_info.ping_handler,
    parent_echo_reply_callback);
  /* Send echo request */
#if UIP_MULTI_IFACES
  /* The global destination is enough to resolve the outoing interface */
  uip_out_if = ping_info.iface;
#else
  ctimer_set(&ping_info.ping_timer, 3*CLOCK_SECOND, ping_timeout_handler, NULL);
  uip_icmp6_send(&ping_info.host_addr, ICMP6_ECHO_REQUEST, 0, 10);
#endif /* UIP_MULTI_IFACES */
}
/*---------------------------------------------------------------------------*/
static int
schedule_ping(rpl_instance_t *rpl_instance)
{
  ping_info.requests++;
  if(ping_info.pending) {
    return 0;
  }
  ping_info.pending = 1;
  ctimer_set(&ping_info.ping_timer, 0, send_ping_request, NULL);
#if UIP_MULTI_IFACES
  ping_info.iface = rpl_instance->current_dag->iface;
#endif
  uip_ipaddr_t *parent_local_addr =
    rpl_get_parent_ipaddr(rpl_instance->current_dag->preferred_parent);
  /* Get the global address of the parent */
  uip_ipaddr_t *rpl_prefix = &rpl_instance->current_dag->prefix_info.prefix;
  uip_ipaddr_copy(&ping_info.host_addr, rpl_prefix);
  uint8_t length_bytes = rpl_instance->current_dag->prefix_info.length >> 3;
  memcpy((uint8_t *)&ping_info.host_addr + length_bytes,
    (uint8_t *)parent_local_addr + length_bytes, 16-length_bytes);

  return 1;
}
/*---------------------------------------------------------------------------*/
static void res_get_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset);
static void res_put_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset);
static void res_post_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset);
/*---------------------------------------------------------------------------*/
/*
 * A handler function named [resource name]_handler must be implemented for each RESOURCE.
 * A buffer for the response payload is provided through the buffer pointer. Simple resources can ignore
 * preferred_size and offset, but must respect the REST_MAX_CHUNK_SIZE limit for the buffer.
 * If a smaller block size is requested for CoAP, the REST framework automatically splits the data.
 */
RESOURCE(res_rpl,
         "title=\"RPL: put:instance, post:instance,action ?len=0..\";rt=\"Text\"",
         res_get_handler,
         res_post_handler,
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
  char message[REST_MAX_CHUNK_SIZE];
  memset(message, 0, REST_MAX_CHUNK_SIZE);
  int length = print_generic_rpl_info(message, REST_MAX_CHUNK_SIZE);
	
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
  int success = 1;
  const char *instance;
  int rpl_instance_index = -1;
  uint16_t length = 0;
  char message[REST_MAX_CHUNK_SIZE];

  memset(message, 0, REST_MAX_CHUNK_SIZE);
  if((len = REST.get_post_variable(request, "instance", &instance))) {
    rpl_instance_index = atoi(instance);
    if(rpl_instance_index < 0 || rpl_instance_index >= RPL_MAX_INSTANCES) {
      success = 0;
    } else {
      rpl_instance_t *rpl_instance = &instance_table[rpl_instance_index];
      if(!rpl_instance->used) {
        success = 0;
      } else {
        if(rpl_instance->current_dag == NULL) {
          snprintf(message, REST_MAX_CHUNK_SIZE-1, "no dag for instance: %u",
            rpl_instance_index);
        } else {
          sprint_rpl_info_json(message, REST_MAX_CHUNK_SIZE, rpl_instance);
        }
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
static void
res_post_handler(void *request, void *response, uint8_t *buffer,
  uint16_t preferred_size, int32_t *offset)
{
  size_t len = 0;
  const char *instance, *action;
  int success = 1;
  int rpl_instance_index = -1;
  uint16_t length = 0;
  char message[64];
  rpl_instance_t *rpl_instance = NULL;

  memset(message, 0, 64);
  if((len = REST.get_post_variable(request, "instance", &instance))) {
    rpl_instance_index = atoi(instance);
    if(rpl_instance_index < 0 || rpl_instance_index >= RPL_MAX_INSTANCES) {
      success = 0;
    } else {
      rpl_instance = &instance_table[rpl_instance_index];
      if(!rpl_instance->used || rpl_instance->current_dag == NULL ||
        rpl_instance->current_dag->preferred_parent == NULL) {
        snprintf(message, 64-1, "Invalid/Inactive instance");
      } else {
        /* Print aggregate PING statistics or schedule PING transission */
        if((len = REST.get_post_variable(request, "action", &action))) {
          if(strncmp(action, "print", len) == 0) {
            snprintf(message, 64-1, "Parent ");
            sprint_addr6(&message[strlen(message)], &ping_info.host_addr);
            snprintf(&message[strlen(message)], 64-1-strlen(message),
              ".Req:%u, Ans:%u, Lost:%u",
              ping_info.requests, ping_info.replies, ping_info.lost);
            length = strlen(message);
            /* Print number of echo requests and replies */
          } else if(strncmp(action, "clear", len) == 0){
            /* Reset the counters for echo request and replies to RPL parent */
            ping_info.requests = 0;
            ping_info.replies = 0;
            ping_info.lost = 0;
            snprintf(message, 64-1, "Cleared ping statistics");
            length = strlen(message);
          } else if(strncmp(action, "ping", len) == 0) {
            /* Schedule a PING transmission. Note that a handler needs to be registered. */
            if(schedule_ping(rpl_instance)) {
              snprintf(message, 64-1, "Pinging: ");
              sprint_addr6(&message[strlen(message)], &ping_info.host_addr);
            } else {
              snprintf(message, 64-1, "Ping failed");
            }
            length = strlen(message);
          } else {
            success = 0;
          }
        } else {
          success = 0;
        }
      }
    }
  }
  if(success == 0) {
    REST.set_response_status(response, REST.status.BAD_REQUEST);
    length = 0;
  } else {
    memcpy(buffer, message, length );
  }
  REST.set_header_content_type(response, REST.type.TEXT_PLAIN);
  REST.set_header_etag(response, (uint8_t *)&length, 1);
  REST.set_response_payload(response, buffer, length);
}
/*---------------------------------------------------------------------------*/

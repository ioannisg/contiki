/*
 * ota_debug.h
 *
 * Created: 2014-09-20 01:26:52
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se>>
 */ 


#ifndef OTA_DEBUG_H_
#define OTA_DEBUG_H_

#define OTG_UDP_CLIENT_PORT		8765
#define OTG_UDP_SERVER_PORT		5678

#define OTG_SERVER_ADDR_0        0xfe80
#define OTG_SERVER_ADDR_1        0
#define OTG_SERVER_ADDR_2        0
#define OTG_SERVER_ADDR_3        0
#define OTG_SERVER_ADDR_4        0x196
#define OTG_SERVER_ADDR_5        0xf71b
#define OTG_SERVER_ADDR_6        0x6f60
#define OTG_SERVER_ADDR_7        0x2cb

extern char ota_debug_buf[128];

/* Initiate network connection with the remote host.
 * Return TRUE (1) if successful otherwise FALSE (1)
 */
uint8_t otg_debug_init_connection(void);

/** Send debug message over the UDP line */
void otg_debug_send_debug_msg(void);


#endif /* OTG_DEBUG_H_ */

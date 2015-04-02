/*
 * enc424j600_drv.h
 *
 * Created: 2014-08-22 20:35:50
 *  Author: Ioannis Glaropoulos <ioannisg@sicse.se>
 */ 


#ifndef ENC424J600_DRV_H_
#define ENC424J600_DRV_H_

#include "compiler.h"
#include "ethernet/enc424j600.h"


/* Configuration */
#define ENC424J600_DRV_RX_PKT_MAX_QUEUE_LEN					4

/* We do not handle packets larger than 1536 bytes.
 * If additionally, further RAM saving is requested
 * we reduce the space allocated for this.
 */
#ifndef ETH_CONF_NO_RAM_SAVING 
#define ETH_424J600_DRV_RX_MAX_PKT_LEN                   ENC424J600_MAX_RX_FRAME_LEN
#else
#define ETH_424J600_DRV_RX_MAX_PKT_LEN                   512
#endif

#include "ethernet.h"
typedef eth_pkt_buf_t enc424j600_pkt_buf_t;

extern const struct eth_driver enc424j600_driver;

typedef struct enc424j600_drv_status {
	unsigned int link_initialized : 1;
	unsigned int link_active : 1;
	unsigned int link_busy : 1;
	unsigned int link_transition : 1;
	unsigned int link_has_address : 1;
#if ETHERNET_BLOCK_UNTIL_TX_COMP
	unsigned int link_tx_err : 1;
#endif
} enc424j600_drv_status_t;
uint32_t enc424j600_get_overflow_counter(void);
#endif /* ENC424J600-DRV_H_ */
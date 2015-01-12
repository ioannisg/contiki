/*
 * xbee-api.h
 *
 * Created: 11/7/2013 11:21:01 AM
 *  Author: Ioannis Glaropoulos
 */ 
#ifndef XBEE_API_H_
#define XBEE_API_H_

#include "contiki-conf.h"

#ifdef XBEE_CONF_WITH_FRAME_SEQNO
#define XBEE_WITH_FRAME_SEQNO XBEE_CONF_WITH_FRAME_SEQNO
#else
#define XBEE_WITH_FRAME_SEQNO                   0
#endif

#define XBEE_CMD_MAX_PAYLOAD_SIZE               8
#define XBEE_TX_MAX_PAYLOAD_SIZE                (100 - (XBEE_WITH_FRAME_SEQNO)*sizeof(uint16_t)) 
#define XBEE_RX_MAX_PAYLOAD_SIZE                (100 - (XBEE_WITH_FRAME_SEQNO)*sizeof(uint16_t))

#define XBEE_TX_RF_HDR_SIZE                     (10 + (XBEE_WITH_FRAME_SEQNO)*sizeof(uint16_t))
#define XBEE_RX_RF_HDR_SIZE                     (10 + (XBEE_WITH_FRAME_SEQNO)*sizeof(uint16_t))

#define XBEE_TX_FRAME_HDR_SIZE                  3
#define XBEE_TX_API_ID_HDR_SIZE                 1
#define XBEE_TX_CHECKSUM_SIZE                   1

#define XBEE_TX_MAX_FRAME_SIZE (XBEE_TX_MAX_PAYLOAD_SIZE + XBEE_TX_RF_HDR_SIZE + XBEE_TX_API_ID_HDR_SIZE)
#define XBEE_TX_MAX_SIZE (XBEE_TX_MAX_FRAME_SIZE + XBEE_TX_FRAME_HDR_SIZE + XBEE_TX_CHECKSUM_SIZE)

#define XBEE_TX_MAX_FIFO_SIZE                   202

/** XBEE API identifier values */
#define XBEE_AT_CMD_REQ_ID                      0x08
#define XBEE_AT_CMD_RSP_ID                      0x88
#define XBEE_AT_TX_FRAME_ID                     0x00
#define XBEE_AT_TX_FRAME_16_ID                  0x01
#define XBEE_AT_RX_FRAME_RSP_ID                 0x80
#define XBEE_AT_RX_FRAME_RSP_16_ID              0x81
#define XBEE_AT_TX_STATUS_RSP_ID                0x89
#define XBEE_AT_MODEM_STATUS_RSP                0x8a

#define XBEE_DEFAULT_FRAME_ID                   0x01
// XBee will not generate a TX Status Packet if this frame id sent
#define XBEE_NO_RSP_FRAME_ID                    0x00

/** XBEE TX Options */
#define XBEE_TX_OPT_UNICAST                     0x00
#define XBEE_TX_OPT_NO_ACK                      0x01
#define XBEE_TX_OPT_BROADCAST                   0x04

/** XBEE RX Options */
#define XBEE_RX_OPT_ADDR_BCAST                  0x02
#define XBEE_RX_OPT_PAN_BCAST                   0x04

/** XBEE_TX Status Option */
#define XBEE_TX_STATUS_OPT_SUCCESS              0x00
#define XBEE_TX_STATUS_OPT_NO_ACK               0x01
#define XBEE_TX_STATUS_OPT_CCA_FAIL             0x02
#define XBEE_TX_STATUS_OPT_PURGED               0x03

/* XBEE Command API */
#define XBEE_CMD_ND_TIMEOUT                     {'N','T'}
#define XBEE_ND_SEND_CMD                        {'N','D'}
#define XBEE_CMD_MAC_ADDR                       {'M','Y'}
#define XBEE_CMD_API_MODE                       {'A','P'}
#define XBEE_CMD_HV_PARAM                       0x5648// {'H','V'}
#define XBEE_CMD_FW_VER                         {'V','R'}
#define XBEE_CMD_CHANNEL                        0x4843//{'C','H'}
#define XBEE_CMD_WR                             {'W','R'}
#define XBEE_CMD_RETRIES                        {'R','R'}
#define XBEE_CMD_MAC_MODE                       {'M','M'}
#define XBEE_CMD_PAN_ID                         {'I','D'}
#define XBEE_CMD_AES_ENCRYPT                    {'E','E'}
#define XBEE_CMD_RF_PW_LEVEL                    {'P','L'}
#define XBEE_CMD_RF_CCA_THRD                    {'C','A'}

/* Special */
#define XBEE_API_SFTW_RST_16                    0x5246  //      {'F','R'}
#define XBEE_API_RST_DEF_16                     0x4552  //      {'R','E'}       
#define XBEE_API_WR_16                          0x5257  //      {'W','R'}

/* Networking & Security */
#define XBEE_API_CHAN_16                        0x4843  //      {'C','H'}
#define XBEE_API_PAN_ID_16                      0x4449  //      {'I','D'}
#define XBEE_API_DH_16                          0x4844  //      {'D','H'}
#define XBEE_API_DL_16                          0x4c44  //      {'D','L'}
#define XBEE_API_MY_16                          0x594d  //      {'M','Y'}
#define XBEE_API_SRC_HI_16                      0x4853  //      {'S','H'}
#define XBEE_API_SRC_LO_16                      0x4c53  //      {'S','L'}
#define XBEE_API_RET_16                         0x5252  //      {'R','R'}
#define XBEE_API_RND_SLOT_16                    0x4e52  //      {'R','N'}
#define XBEE_API_MAC_MODE_16                    0x4d4d  //      {'M','M'}
#define XBEE_API_NODE_ID_16                     0x494e  //      {'N','I'}
#define XBEE_API_NODE_DIS_16                    0x444e  //      {'N','D'}
#define XBEE_API_ND_TIM_16                      0x544e  //      {'N','T'}
#define XBEE_API_MD_OPT_16                      0x4f4e  //      {'N','O'}
#define XBEE_API_DES_NODE_16                    0x4e44  //      {'D','N'}
#define XBEE_API_COORD_EN_16                    0x4543  //      {'C','E'}
#define XBEE_API_SCAN_CH_16                     0x4353  //      {'S','C'}       
#define XBEE_API_SCAN_DR_16                     0x4453  //      {'S','D'}       
#define XBEE_API_ENRG_SCAN_16                   0x4445  //      {'E','D'}
#define XBEE_API_ENCRYPT_EN_16                  0x4545  //      {'E','E'}
#define XBEE_API_ENCRYPT_KEY_16                 0x594d  //      {'K','Y'}
#define XBEE_API_SOFTWARE_RESET_16              0x5246  //      {'F','R'}
/* RF Interfacing */
#define XBEE_API_PW_LEVEL_16                    0x4c50  //      {'P','L'}
#define XBEE_API_CA_THR_16                      0x4143  //      {'C','A'}
        
/* Serial Interfacing */
#define XBEE_API_BAUD_RATE_16                   0x4442  //      {'B','D'}
#define XBEE_API_PKTN_TIM_16                    0x4f52  //      {'R','O'}       
#define XBEE_API_API_MODE_16                    0x5041  //      {'A','P'}
#define XBEE_API_PARITY_16                      0x424e  //      {'N','B'}
        
/* Diagnostics */
#define XBEE_API_FW_VER_16                      0x5256  //      {'V','R'}
#define XBEE_API_HV_VER_16                      0x5648  //      {'H','V'}
#define XBEE_API_RSSI_16                        0x4244  //      {'D','B'}
#define XBEE_API_CCA_FAIL_16                    0x4345  //      {'E','C'}
#define XBEE_API_ACK_FAIL_16                    0x4145  //      {'E','A'}

        
// Responses

/* HW Versions */
#define XBEE_HV_SERIES_1                        0x17
#define XBEE_HV_SERIES_1_PRO                    0x18
#define XBEE_HV_SERIES_2                        0x19
#define XBEE_HV_SERIES_2_PRO                    0x1a

/* API Modes */
#define XBEE_API_MODE_2_EN_ESC                  0x02
#define XBEE_API_MODE_1_EN                      0x01
#define XBEE_API_MODE_0_DIS                     0x00

/* Power Levels */
#define XBEE_API_PL_10_DBm                      0x04
#define XBEE_API_PL_08_DBm                      0x03
#define XBEE_API_PL_02_DBm                      0x02
#define XBEE_API_PL_00_DBm                      0x01
#define XBEE_API_PL_m3_DBm                      0x00

/* MAC Modes */
#define XBEE_API_MM_DIGI                        0x00
#define XBEE_API_MM_802154_NO_ACKS              0x01
#define XBEE_API_MM_802154_WACKS                0x02
#define XEE_API_MM_DIG_NO_ACKS                  0x03

/* Disabled 16-bit mac addresses */
#define XBEE_API_MY_DISABLE_SHORT               0xffff

/* Selected baud-rates */
#define XBEE_API_BAUD_RATE_9600                 (3 << 24)
#define XBEE_API_BAUD_RATE_57600                (6 << 24)
#define XBEE_API_BAUD_RATE_115200               (7 << 24)

#define XBEE_API_PKT_HDR_LEN                    8
#define XBEE_SFD_BYTE                           0x7e
#define XBEE_STOP_BYTE                          0x7e
#define XBEE_ESCAPE_CHAR                        0x7d
#define XBEE_RADIO_ON                           0x11
#define XBEE_RADIO_OFF                          0x13
#define XBEE_ESCAPE_XOR                         0x20

typedef enum xbee_aes_encrypt_mode {
        
        XBEE_MAC_AES_ENCRYPT_DISABLE,
        XBEE_MAC_AES_ENCRYPT_ENABLE
} xbee_aes_encrypt_mode_t;


typedef struct xbee_at_command xbee_at_command_t;

/** The structure of a transmitted XBEE AT Command. */
#pragma pack(1)
typedef struct xbee_at_command {
	xbee_at_command_t *next;
	uint16_t len;
	uint8_t frame_id;
	uint8_t cmd_id[2];
	uint8_t data[4];
} xbee_at_command_t;

/** The structure of a received XBEE AT Command Response Header. */
typedef struct xbee_at_command_rsp {
	/* Frame ID */
	uint8_t frame_id;
	/* AT Command */
	uint16_t cmd_id;
	/* Status of the response */
	uint8_t rsp_status;
	/* Response values */
	uint8_t value[XBEE_CMD_MAX_PAYLOAD_SIZE];
} xbee_at_command_rsp_t;

/** The structure of a transmitted XBEE Frame Header. */
typedef struct xbee_at_frame_tx_hdr {
	/* API ID */
	uint8_t api_id;
	/* Frame ID */
	uint8_t frame_id;
	/* Source Link Layer Address */
	uint8_t dst_addr[8];
	/* Frame Options */
	uint8_t option;
#if XBEE_WITH_FRAME_SEQNO
	/* Sequence number */
	uint16_t seqno;
#endif
} xbee_at_frame_tx_hdr_t;

/** The structure of a received XBEE Frame Header. */
typedef struct xbee_at_frame_rsp {
	/* Source Link Layer Address */
	uint8_t src_addr[8];
	/* RSSI Value */
	uint8_t rssi;
	/* Frame Options */
	uint8_t option;
	#if XBEE_WITH_FRAME_SEQNO
	/* Sequence number */
	uint16_t seqno;
	#endif
	/* Frame payload */
	uint8_t data[XBEE_RX_MAX_PAYLOAD_SIZE];
} xbee_at_frame_rsp_t;

/** The structure of a received XBEE TX Status response. */
typedef struct xbee_at_tx_status_rsp {
	/* Frame ID */
	uint8_t frame_id;
	/* Frame transmission status */
	uint8_t option;
} xbee_at_tx_status_rsp_t;

/* The general structure of an XBEE response. */
typedef struct xbee_response {
	uint8_t len;
	uint8_t api_id;
	union {
		xbee_at_frame_rsp_t frame_data;
		xbee_at_command_rsp_t cmd_data;
		xbee_at_tx_status_rsp_t tx_status;
	};
} xbee_response_t;
#pragma pack();

#endif /* XBEE-API_H_ */
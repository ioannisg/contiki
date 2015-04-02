/*
 * enc624J600_conf.h
 *
 * Created: 2014-08-18 18:38:16
 *  Author: ioannisg
 */ 
#include "sam3x8e.h"
#include "platform-conf.h"

#ifndef ENC624J600_CONF_H_
#define ENC624J600_CONF_H_

/* PIN number where ENC424J600 interrupts are drived into */
#ifndef ENC624J600_CONF_IRQ_PIN
#define ENC624J600_IRQ_PIN					16
#else
#define ENC624J600_IRQ_PIN             ENC624J600_CONF_IRQ_PIN
#endif

/* NVIC interrupt source for the ENC424J600 interrupt pin */
#define ENC624J600_IRQ_SOURCE          PIOA_IRQn

#define ENC424J600_MAX_RX_FRAME_LEN		1536

// Promiscuous mode, uncomment if you want to receive all packets, even those which are not for you
#define PROMISCUOUS_MODE

/* Ethernet module connected on SPI0 channel */
#define ENC424J600_SPI_CHANNEL			SPI0

#endif /* ENC624J600-CONF_H_ */
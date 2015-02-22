/*
 * ip64_conf.h
 *
 * Created: 2015-01-28 23:51:19
 *  Author: ioannisg
 */ 


#ifndef IP64_CONF_H_
#define IP64_CONF_H_

#include "ip64/ip64-eth-interface.h"
#include "ip64/ip64-null-driver.h"

#define IP64_CONF_UIP_FALLBACK_INTERFACE ip64_eth_interface
#define IP64_CONF_INPUT                  ip64_eth_interface_input

#define IP64_CONF_ETH_DRIVER             ip64_null_driver

#define IP64_CONF_DHCP                   1


#endif /* IP64-CONF_H_ */
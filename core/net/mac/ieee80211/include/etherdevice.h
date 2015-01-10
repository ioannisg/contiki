/*
 * etherdevice.h
 *
 * Created: 1/30/2014 12:45:33 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include "if_ether.h"
#include <stdio.h>

#ifndef ETHERDEVICE_H_
#define ETHERDEVICE_H_

#define ETHTOOL_FWVERS_LEN      32
#define ETHTOOL_BUSINFO_LEN     32

/**
 * compare_ether_addr - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two Ethernet addresses, returns 0 if equal, non-zero otherwise.
 * Unlike memcmp(), it doesn't return a value suitable for sorting.
 */
inline unsigned 
compare_ether_addr(const uint8_t *addr1, const uint8_t *addr2)
{
	const uint16_t *a = (const uint16_t *) addr1;
	const uint16_t *b = (const uint16_t *) addr2;
	
	if(ETH_ALEN != 6) {
		printf("ETH_ALEN not 6\n");
		return -1;
	}
	return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}

/**
 * ether_addr_equal - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two Ethernet addresses, returns true if equal
 */
inline
uint8_t ether_addr_equal(const uint8_t *addr1, const uint8_t *addr2)
{
	return !compare_ether_addr(addr1, addr2);
}


#endif /* ETHERDEVICE_H_ */
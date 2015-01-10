/*
 * nullethernet.c
 *
 * Created: 5/11/2014 12:10:52 AM
 *  Author: ioannisg
 */ 
#include "compiler.h"
#include "nullethernet.h"
#include "linkaddr.h"

static void
init(void)
{
	
}

static void
reset(void)
{
	
}

static void
set_macaddr(linkaddr0_t *macaddr)
{
	UNUSED(macaddr);
	return;
}

static linkaddr0_t* get_macaddr(void)
{
	return NULL;
}

static int
tx_packet(void)
{
	return ETHERNET_TX_OK;
}

static uint8_t
is_initialized(void)
{
	return 0;
}

static uint8_t
is_up(void)
{
	return 0;
}
const struct eth_driver nullethernet_driver = {
	"null-eth-drv",
	init,
	reset,
	set_macaddr,
	get_macaddr,
	tx_packet,
	is_initialized,
	is_up,
};
/*
 * nullwifi.c
 *
 * Created: 4/13/2014 2:22:28 AM
 *  Author: ioannisg
 */

#include "dev/nullwifi.h"


/*---------------------------------------------------------------------------*/
static int
init(void)
{
	return 0;
} 
/*---------------------------------------------------------------------------*/
const struct wifi_driver nullwifi_driver =
{
	"nullwifi",
	init,
};


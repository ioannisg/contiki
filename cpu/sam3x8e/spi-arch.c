/*
 * spi_arch.c
 *
 * \brief SPI cpu-dependend implementation for SAM3X8E
 *
 * Created: 4/15/2014 7:00:07 PM
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se>
 */ 
#include "contiki-conf.h"
#include "core/dev/spi.h"
#include "spi_master.h"


/** Spi H/W ID */
#ifndef SPI_CONF_ARCH_ID
#define SPI_ARCH_ID              ID_SPI0
#else 
#define SPI_ARCH_ID              SPI_CONF_ARCH_ID
#endif

/** SPI base address for SPI master mode*/
#ifndef SPI_CONF_ARCH_MASTER_BASE
#define SPI_ARCH_MASTER_BASE     SPI0
#else
#define SPI_ARCH_MASTER_BASE     SPI_CONF_ARCH_MASTER_BASE
#endif

/** SPI base address for SPI slave mode, (on different board) */
#ifndef SPI_CONF_ARCH_SLAVE_BASE
#define SPI_ARCH_SLAVE_BASE      SPI0
#else
#define SPI_ARCH_SLAVE_BASE      SPI_CONF_ARCH_SLAVE_BASE
#endif

/** SPI chip select value [Default is 0, for PIN 10 in board] */
#ifndef SPI_CONF_ARCH_CHIP_SELECT_ID
#define SPI_ARCH_CHIP_SELECT_ID  0
#else
#define SPI_ARCH_CHIP_SELECT_ID  SPI_CONF_ARCH_CHIP_SELECT_ID
#endif

/** SPI transfer mode, for clock polarity and phase */
#ifndef SPI_CONF_ARCH_SPI_MODE
#define SPI_ARCH_SPI_MODE        SPI_MODE_0
#else
#define SPI_ARCH_SPI_MODE        SPI_CONF_ARCH_SPI_MODE
#endif

/** SPI baud rate */
#ifndef SPI_CONF_ARCH_BAUD_RATE
#define SPI_ARCH_BAUD_RATE_BPS  1000000
#else
#define SPI_ARCH_BAUD_RATE      SPI_CONF_ARCH_BAUD_RATE
#endif

/* Delay before SPCK. */
//#define SPI_DLYBS               0//0x40
/* Delay between consecutive transfers. */
//#define SPI_DLYBCT              0//0x10


/*---------------------------------------------------------------------------*/
static struct spi_device _spi_device = {
  .id = SPI_ARCH_CHIP_SELECT_ID
};
/*---------------------------------------------------------------------------*/
void
spi_init(void)
{
  spi_master_init(SPI_ARCH_MASTER_BASE);
  spi_master_setup_device(SPI_ARCH_MASTER_BASE, &_spi_device, SPI_ARCH_SPI_MODE,
    SPI_ARCH_BAUD_RATE_BPS, 0);
  spi_set_transfer_delay(SPI_ARCH_MASTER_BASE, SPI_ARCH_CHIP_SELECT_ID, 0x20, 0x08);
  spi_enable(SPI_ARCH_MASTER_BASE);
}
/*---------------------------------------------------------------------------*/

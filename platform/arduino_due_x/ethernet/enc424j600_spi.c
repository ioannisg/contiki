/*
 * enc424j600_spi.c
 *
 * Created: 2014-08-28 21:27:46
 *  Author: Ioannis Glaropoulos
 */ 
#include "enc424j600.h"
#include "enc424J600-conf.h"
#include "enc424j600-spi.h"
#include "spi_master.h"
#include <stdio.h>

#include "core/dev/spi.h"

static struct spi_device spi_dev = {
	.id = 0 // Pin 10 for chip select [default]
};

void
enc424j600_spi_init(void)
{
	spi_init();
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_execute_op0(uint8_t op)
{
	uint8_t _data = op;
	uint8_t result = ENC424J600_STATUS_OK;
	
	/* Select SPI slave device */
	spi_select_device(ENC424J600_SPI_CHANNEL, &spi_dev);
	
	if(spi_write_packet(ENC424J600_SPI_CHANNEL, &_data, 1) != 0) {
		result = ENC424J600_STATUS_ERR;
	}
	
	/* Release SPI slave device */
	spi_deselect_device(ENC424J600_SPI_CHANNEL, &spi_dev);
	
	return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_execute_op8(uint8_t op, uint8_t data, uint8_t* ret_val)
{
	/* Select SPI slave device */
	spi_select_device(ENC424J600_SPI_CHANNEL, &spi_dev);
	
	uint8_t _data = op;
	uint8_t result = ENC424J600_STATUS_OK;
	
	/* Write OPERATION value to the SPI bus */
	if(spi_write_packet(ENC424J600_SPI_CHANNEL, &_data, 1) != 0) {
		result = ENC424J600_STATUS_ERR;
		goto _exit;
	}

	/* Write data on the SPI bus */
	_data = data;
	if(spi_transfer_single(ENC424J600_SPI_CHANNEL, &_data) != 0) {
		result = ENC424J600_STATUS_ERR;
		goto _exit;
	}
	
	/* Store the read value */
	*ret_val = _data;
	
	_exit:
	spi_deselect_device(ENC424J600_SPI_CHANNEL, &spi_dev);
	return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_execute_op16(uint8_t op, uint16_t data, uint16_t *ret_val)
{
  uint8_t _data = op;
  uint8_t result = ENC424J600_STATUS_OK;
  uint16_t ret_data = 0;
	
  /* Select SPI slave device */
  spi_select_device(ENC424J600_SPI_CHANNEL, &spi_dev);

  /* Write OPERATION value to the SPI bus */
  if(spi_write_packet(ENC424J600_SPI_CHANNEL, &_data, 1) != 0) {
    result = ENC424J600_STATUS_ERR;
    goto _exit;
  }
  _data = (uint8_t)(data & 0xff);

  if (spi_transfer_single(ENC424J600_SPI_CHANNEL, &_data) != 0) {
    result = ENC424J600_STATUS_ERR;
    goto _exit;
  }
  ret_data = _data;

  _data = (uint8_t)((data >> 8) & 0xff);

  if (spi_transfer_single(ENC424J600_SPI_CHANNEL, &_data) != 0) {
    result = ENC424J600_STATUS_ERR;
    goto _exit;
  }

  uint16_t msb = _data;
  msb = msb << 8;

  ret_data |= msb;

  /* Store the read value */
  *ret_val = ret_data;

_exit:
  spi_deselect_device(ENC424J600_SPI_CHANNEL, &spi_dev);
  return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_execute_op32(uint8_t op, uint32_t data, uint32_t *ret_val)
{
  uint8_t _data = op;
  uint8_t result = ENC424J600_STATUS_OK;
  uint32_t ret_data = 0;
	
  /* Select SPI slave device */
  spi_select_device(ENC424J600_SPI_CHANNEL, &spi_dev);
	
  /* Write OPERATION value to the SPI bus */
  if(spi_write_packet(ENC424J600_SPI_CHANNEL, &_data, 1) != 0) {
    result = ENC424J600_STATUS_ERR;
    goto _exit;
  }
	
  for (int i=0; i<3; i++) {
    _data = ((uint8_t*)&data)[i];
    if (spi_transfer_single(ENC424J600_SPI_CHANNEL, &_data) != 0) {
      result = ENC424J600_STATUS_ERR;
      goto _exit;
  }

    ((uint8_t*)&ret_data)[i] = _data;
  }
	
  /* Store the read value */
  *ret_val = ret_data;
_exit:
  spi_deselect_device(ENC424J600_SPI_CHANNEL, &spi_dev);
  return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_read_n(uint8_t op, uint8_t* data, uint16_t data_len)
{
	/* Select slave device */
	spi_select_device(ENC424J600_SPI_CHANNEL, &spi_dev);

	uint8_t result = ENC424J600_STATUS_OK;
	uint8_t _data = op;
	
	/* Issue read command */
	if(spi_write_packet(ENC424J600_SPI_CHANNEL, &_data, 1) != STATUS_OK) {
		result = ENC424J600_STATUS_ERR;
		goto _exit;
	}
	
	/* Read all data */
	result = spi_read_packet(ENC424J600_SPI_CHANNEL, data, data_len);
	
	_exit:
	/* Release slave device */
	spi_deselect_device(ENC424J600_SPI_CHANNEL, &spi_dev);
	
	return result;
}
/*---------------------------------------------------------------------------*/
uint8_t
enc424j600_write_n(uint8_t op, uint8_t* data, uint16_t data_len)
{
	/* Select slave device */
	spi_select_device(ENC424J600_SPI_CHANNEL, &spi_dev);

	uint8_t result = ENC424J600_STATUS_OK;
	uint8_t _data = op;
	
	/* issue read command */
	if(spi_write_packet(ENC424J600_SPI_CHANNEL, &_data, 1) != STATUS_OK) {
		result = ENC424J600_STATUS_ERR;
		goto _exit;
	}
	
	/* Send all data */
	result = spi_write_packet(ENC424J600_SPI_CHANNEL, data, data_len);
	
	_exit:
	/* Release slave device */
	spi_deselect_device(ENC424J600_SPI_CHANNEL, &spi_dev);
	
	return result;
}
/*---------------------------------------------------------------------------*/
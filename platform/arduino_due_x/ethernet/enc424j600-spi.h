/*
 * enc424j600_spi.h
 *
 * Created: 2014-08-28 21:26:15
 *  Author: ioannisg
 */ 


#ifndef ENC424J600_SPI_H_
#define ENC424J600_SPI_H_

void enc424j600_spi_init(void);

uint8_t enc424j600_execute_op32(uint8_t op, uint32_t data, uint32_t* ret_val);
uint8_t enc424j600_execute_op8(uint8_t op, uint8_t data, uint8_t* ret_val);
uint8_t enc424j600_execute_op0(uint8_t op);
uint8_t enc424j600_execute_op16(uint8_t op, uint16_t data, uint16_t* ret_val);


uint8_t enc424j600_read_n(uint8_t op, uint8_t* data, uint16_t data_len);
uint8_t enc424j600_write_n(uint8_t op, uint8_t* data, uint16_t data_len);


#endif /* ENC424J600_SPI_H_ */
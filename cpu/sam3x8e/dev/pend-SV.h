/*
 * pend_SV.h
 *
 * Created: 2/25/2014 10:22:17 AM
 *  Author: Ioannis Glaropoulos
 */ 


#ifndef PEND_SV_H_
#define PEND_SV_H_


typedef void (* pend_sv_callback_t)(void *ptr);

void pend_sv_init(void);

uint8_t pend_sv_register_handler(pend_sv_callback_t callback, void *ptr);


#endif /* PEND-SV_H_ */
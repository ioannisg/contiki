/*
 * coap_test.h
 *
 * Created: 2015-02-14 11:52:42
 *  Author: ioannisg
 */ 


#ifndef COAP_TEST_H_
#define COAP_TEST_H_


#include "contiki.h"
#include "process.h"

PROCESS_NAME(coap_client_test_process);

uint16_t coap_test_get_rsp_count(void);


#endif /* COAP_TEST_H_ */
/*
 * ar9170_fw.h
 *
 * Created: 2/1/2014 7:10:11 PM
 *  Author: Ioannis Glaropoulos
 */ 
#include <stddef.h>


#ifndef AR9170_FW_H_
#define AR9170_FW_H_

struct firmware {
	size_t fw_size;
	const uint8_t *fw_data;
	//struct page **pages;
	
	/* firmware loader private fields */
	void *priv;
};



#endif /* AR9170_FW_H_ */
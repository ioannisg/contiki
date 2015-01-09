/*
 * ota_flash_arch.c
 *
 * Created: 2014-09-14 15:10:55
 *  Author: Ioannis Glaropoulos <ioannisg@kth.se>
 */ 
#include "apps/ota-upgrade/ota-upgrade.h"
#include "compiler.h"
#include "cfs-coffee-arch.h"
#include "flash_efc.h"


static uint32_t fw_offset;
static uint8_t fw_store_ongoing;
static uint8_t fw_store_bank;

/*---------------------------------------------------------------------------*/
static uint8_t
ota_flash_init_store(void)
{
  if (fw_store_ongoing)
    return 0;

  fw_offset = 0;
  fw_store_ongoing = 1;

  return 1;
}
/*---------------------------------------------------------------------------*/
uint8_t 
ota_flash_store_chunk(uint8_t * _data, uint16_t data_len)
{
  if (unlikely(!data_len))
    return 1;

  if (!fw_store_ongoing)
    ota_flash_init_store();

  uint32_t write_res = COFFEE_WRITE(_data, data_len, fw_offset);

  if (write_res)
    return 0;
	 
  fw_offset += data_len;
  return 1;
}
/*---------------------------------------------------------------------------*/
uint8_t
ota_flash_swap(void)
{
  uint16_t i;
  __disable_irq();
  /* Disable SysTick */
  SysTick->CTRL = 0;
  /* Disable IRQs & clear pending IRQs */
  for (i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }

  /* Change Boot bank for next reset */
  if (fw_store_bank == 0) {
    if (!flash_is_gpnvm_set(1)) {
      flash_set_gpnvm(1);
    }
    if (flash_is_gpnvm_set(2)) {
      flash_clear_gpnvm(2);
    }
  } else if (fw_store_bank == 1) {
    if (!flash_is_gpnvm_set(1)) {
      flash_set_gpnvm(1);
    }
    if (!flash_is_gpnvm_set(2)) {
      flash_set_gpnvm(2);
    }
  } else {
    return 0;
  }
  NVIC_SystemReset();  /* Swap the whole flash bank(s) */
  return 1;
  
 #if 0 
  uint16_t i;
  fw_store_ongoing = 0;
  printf("fw_offset: %u\n\r",(unsigned int)fw_offset);
  
  if (flash_unlock(FLASH_ABSOLUTE_START, 
    (FLASH_ABSOLUTE_START)+(FLASH_BANK0_SIZE)+(COFFEE_SIZE)-1, 0, 0)) {
    /* Abort */
	 return 0;
  }
 /* 
  if (flash_is_locked(FLASH_ABSOLUTE_START, FLASH_PAGE_SIZE))
    return 0;
 */ 
  __disable_irq();
  /* Disable SysTick */
  SysTick->CTRL = 0;
  /* Disable IRQs & clear pending IRQs */
  for (i = 0; i < 8; i++) {
    NVIC->ICER[i] = 0xFFFFFFFF;
    NVIC->ICPR[i] = 0xFFFFFFFF;
  }
  
  for (i=0; i<(fw_offset/COFFEE_PAGE_SIZE)+1; i++) {
    /* Write to swap_mem, the contents of the i-th page of Bank 1 */
    COFFEE_READ(swap_mem, COFFEE_PAGE_SIZE, (COFFEE_PAGE_SIZE)*i);
    /* Overwrite Bank 1 with Bank 0 contents */
    //if(COFFEE_WRITE((void *)(FLASH_ABSOLUTE_START + (COFFEE_PAGE_SIZE)*i),
     // COFFEE_PAGE_SIZE, (COFFEE_PAGE_SIZE)*i)) {
      //ioport_set_pin_level(LED0_GPIO, 0);
      //while(1);
    //}
    /* Write to Bank 0 the new FW chunk stored in swap */
	 uint8_t result = 0;
    if (flash_is_locked(FLASH_ABSOLUTE_START+i*(FLASH_PAGE_SIZE), FLASH_ABSOLUTE_START-1+(i+1)*(FLASH_PAGE_SIZE)))
      result = flash_unlock(FLASH_ABSOLUTE_START+i*(FLASH_PAGE_SIZE), FLASH_ABSOLUTE_START-1+(i+1)*(FLASH_PAGE_SIZE), 0, 0);
    if (result) {
       ioport_set_pin_level(LED0_GPIO, 1);
       while(1);
    }
	 result = COFFEE_WRITE((void *)swap_mem, 
      COFFEE_PAGE_SIZE,  -(FLASH_BANK0_SIZE) + (COFFEE_PAGE_SIZE)*i);
    if (!result) {
      // All fine.
    } else if(result & 2) {
      ioport_set_pin_level(LED0_GPIO, 0);
      while(1);
    } else if (result & 4) {
      ioport_set_pin_level(LED0_GPIO, 0); 
      while(1);
    } else {
      ioport_set_pin_level(LED0_GPIO, 0);
    }
    delay_ms(40);
    ioport_toggle_pin_level(LED0_GPIO);
  }
  i = 0;
  while(i < 5) {
    delay_s(2);
    ioport_toggle_pin_level(LED0_GPIO);
    i++;
  }
  NVIC_SystemReset();  
  return 1;
#endif 
}
/*---------------------------------------------------------------------------*/
uint8_t
ota_flash_get_fw_bank(void)
{  
  /* Look for the backup bank */
  uint32_t vector_table = SCB->VTOR;

  if (vector_table == FLASH_BANK0_START) {
    /* FW runs from Bank 0 */
    //printf("ota_flash_utils: store-fw-at-1\n\r");
    fw_store_bank = 1;
	 return 1;
  } else if (vector_table == FLASH_BANK1_START) {
    //printf("ota_flash_utils: store-fw-at-0\n\r");
    fw_store_bank = 0;
	 return 0;
  } else {
    /* Corrupted? */
    while(1);
	 return 0;
  }
}
/*---------------------------------------------------------------------------*/
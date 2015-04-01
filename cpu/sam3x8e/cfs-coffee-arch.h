/**
 * \addtogroup arduino-due-x-platform
 *
 * @{
 */

/*
 * Copyright (c) 2010, STMicroelectronics.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/**
 * \file
 *  Coffee architecture-dependent header for the SAM3X8E-based Arduino Due X 
 *  platform.
 *  
 *  Arduino Due X has in total 512KB of program flash. Half of it will be used
 *  for COFFEE, if SD is not present, otherwise half of the SD is dedicated to
 *  COFFEE.
 *
 * \author
 *  Ioannis Glaropoulos <ioannisg@kth.se>
 */

#ifndef CFS_COFFEE_ARCH_H
#define CFS_COFFEE_ARCH_H

#include "platform-conf.h"
#include "sam3x8e.h"
#if WITH_SD_CARD_SUPPORT
#include "sd_mmc.h"
#else /* WITH_SD_CARD_SUPPORT */
#include "flash_efc.h"
#endif /* WITH_SD_CARD_SUPPORT */
#include "string.h"
#include "energest.h"

/*
 * Arduino Due X has 512KB of program flash in 2048 pages of 256 bytes each.
 * The smallest erasable unit of data is one page, i.e. 256 bytes, and the
 * smallest writable unit of storage is an aligned 16-bit (half-word). In the
 * following we consider only the second (-1) area of flash.
 */
#if WITH_SD_CARD_SUPPORT
/* COFFEE default settings for SD */
#define FLASH_START               COFFEE_ADDRESS
/* Flash page size as the SD block size */
#define FLASH_PAGE_SIZE           SD_MMC_BLOCK_SIZE
#ifdef SD_CONF_SIZE_BLOCKS
#define FLASH_PAGES               (SD_CONF_SIZE_BLOCKS/2)
#else
#warning "SD size not declared. Use 1 MB for COFFEE."
#define FLASH_PAGES               2*1024*1
#endif
#define COFFEE_PAGE_SIZE		    FLASH_PAGE_SIZE

#else /* WITH_SD_CARD_SUPPORT */

/* Byte page size, starting address on page boundary, and size of file system */
/* Flash starts from COFFEE_ADDRESS if defined, or from Bank 1 if not */
#ifdef COFFEE_ADDRESS
#define FLASH_START               COFFEE_ADDRESS
#else
#define FLASH_START               0//FLASH_BANK1_START
#define COFFEE_ADDRESS            0
#endif /* COFFEE_ADDRESS */

/* Minimum erasable unit. */
#define FLASH_PAGE_SIZE           (BANK1_PAGE_SIZE)
/* Last 3 pages reserved for NVM. */
#define FLASH_PAGES               (IFLASH1_NB_OF_PAGES-3)
/* Minimum reservation unit for Coffee. It can be changed by the user.  */
#define COFFEE_PAGE_SIZE		    (FLASH_PAGE_SIZE)
#endif /* WITH_SD_CARD_SUPPORT */
/*
 * If using IAR, COFFEE_ADDRESS reflects the static value in the linker script
 * iar-cfg-coffee.icf, so it can't be passed as a parameter for Make.
 */
#ifdef __ICCARM__
#define COFFEE_ADDRESS            0x8010c00
#endif
#if (COFFEE_ADDRESS & 0x3FF) != 0
#error "COFFEE_ADDRESS not aligned to a 1024-bytes page boundary."
#endif

#define COFFEE_PAGES              ((FLASH_PAGES*FLASH_PAGE_SIZE-               \
                 (COFFEE_ADDRESS-FLASH_START))/COFFEE_PAGE_SIZE)
#define COFFEE_START              (COFFEE_ADDRESS & ~(COFFEE_PAGE_SIZE-1))
#define COFFEE_SIZE               (COFFEE_PAGES*COFFEE_PAGE_SIZE)
#if COFFEE_SIZE > FLASH_BANK1_SIZE
#error "COFFEE size exceeds flash bank"
#endif
/* These must agree with the parameters passed to makefsdata */
#define COFFEE_SECTOR_SIZE        FLASH_PAGE_SIZE
#define COFFEE_NAME_LENGTH        20

/* These are used internally by the AVR flash read routines */
/* Word reads are faster but take 130 bytes more PROGMEM */
 /* #define FLASH_WORD_READS          1 */
/*
 * 1 = Slower reads, but no page writes after erase and 18 bytes less PROGMEM. 
 * Best for dynamic file system
 */
 /* #define FLASH_COMPLEMENT_DATA     0 */

/* These are used internally by the coffee file system */
#define COFFEE_MAX_OPEN_FILES     4
#define COFFEE_FD_SET_SIZE        8
#define COFFEE_DYN_SIZE           (COFFEE_PAGE_SIZE*1)
/* Micro logs are not needed for single page sectors */
#define COFFEE_MICRO_LOGS         0
#define COFFEE_LOG_TABLE_LIMIT    16    /* It doesnt' matter as */
#define COFFEE_LOG_SIZE           128   /* COFFEE_MICRO_LOGS is 0. */

#if COFFEE_PAGES <= 127
#define coffee_page_t int8_t
#elif COFFEE_PAGES <= 0x7FFF
#define coffee_page_t int16_t
#elif COFFEE_PAGES <= 0x7FFFFFFF
#define coffee_page_t int32_t
#else
#error "two many pages for coffee"
#endif

#if WITH_SD_CARD_SUPPORT
#define COFFEE_WRITE(buf, size, offset) \
sd_write(COFFEE_START + offset, buf, size)

#define COFFEE_READ(buf, size, offset) \
sd_read(COFFEE_START + offset, buf, size)

#define COFFEE_ERASE(sector) \
sd_erase(sector)

void sd_read(uint32_t address, void *data, uint32_t length);
void sd_write(uint32_t address, const void *data, uint32_t length);
void sd_erase(uint32_t sector);

void sd_init(void);

#else /* WITH_SD_CARD_SUPPORT */
#define COFFEE_WRITE(from_buf, size, offset) \
(flash_is_gpnvm_set(2) ? flash_write((COFFEE_START+FLASH_BANK0_START) + offset, from_buf, size, 1) :\
  flash_write((COFFEE_START+FLASH_BANK1_START) + offset, from_buf, size, 1)) 

//#define COFFEE_WRITE(from_buf, size, offset) \
uint32_t write_res; \ 
//if (flash_is_gpnvm_set(2)) \
  write_res = flash_write((COFFEE_START+FLASH_BANK0_START) + offset, from_buf, size, 1); \
else \
  write_res = flash_write((COFFEE_START+FLASH_BANK1_START) + offset, from_buf, size, 1);


#define COFFEE_READ(to_buf, size, offset) \
if (flash_is_gpnvm_set(2)) \
  flash_read((COFFEE_START+FLASH_BANK0_START) + offset, to_buf, size); \
else \
  flash_read((COFFEE_START+FLASH_BANK1_START) + offset, to_buf, size); \

#define COFFEE_ERASE(sector) \
flash_erase(sector)

#define COFFEE_INIT() flash_init(FLASH_ACCESS_MODE_128, 6)

inline void flash_read(uint32_t address, void *data, uint32_t length) {
	uint8_t *pdata = (uint8_t *) address;
	ENERGEST_ON(ENERGEST_TYPE_FLASH_READ);
	memcpy((uint8_t*)data, address, length);
	ENERGEST_OFF(ENERGEST_TYPE_FLASH_READ);
}

inline void flash_erase(uint8_t sector) {
	
  uint32_t data = 0;
  uint32_t addr, end;
  if (flash_is_gpnvm_set(2)) {
    addr = (COFFEE_START+FLASH_BANK0_START) + (sector) * COFFEE_SECTOR_SIZE;
    end = addr + COFFEE_SECTOR_SIZE;
    /* This prevents from accidental write to CIB. */
    if(!(addr >= COFFEE_START && end <= COFFEE_START + COFFEE_SIZE -1)) {
      return;
    }
  } else {
    addr = (COFFEE_START+FLASH_BANK1_START) + (sector) * COFFEE_SECTOR_SIZE;
    end = addr + COFFEE_SECTOR_SIZE;	 
    /* This prevents from accidental write to CIB. */
    if(!(addr >= COFFEE_START && end <= COFFEE_START + COFFEE_SIZE -1)) {
      return;
    }
  }

  for(; addr < end; addr += 4) {
    flash_write(addr, &data, 4, 1);
  }
}

#endif /* WITH_SD_SUPPORT */

int coffee_file_test(void);

#endif /* !COFFEE_ARCH_H */
/** @} */

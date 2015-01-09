/*
 * cfs_coffee_arch.c
 *
 * Created: 4/17/2014 4:11:00 PM
 *  Author: Ioannis Glaropoulos
 */ 

#include "cfs-coffee-arch.h"
#include "wire_digital.h"
#include "sd_mmc.h"

#define SD_CONF_SLOT	0

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#include "cfs/cfs.h"
#include "cfs/cfs-coffee.h"
#include "lib/crc16.h"
#include "lib/random.h"
#include <stdio.h>
#define FAIL(x) PRINTF("FAILED\n");error = (x); goto end;

#define FILE_SIZE 512

static uint32_t cfs_coffee_arch_start_block;
/**
 * \brief Display basic information of the card.
 * \note This function should be called only after the card has been
 *       initialized successfully.
 *
 * \param slot   SD/MMC slot to test
 */
static void 
sd_example_display_info_card(uint8_t slot)
{
	PRINTF("Card information:\n\r");

	PRINTF("    ");
	switch (sd_mmc_get_type(slot)) {
	case CARD_TYPE_SD | CARD_TYPE_HC:
		PRINTF("SDHC");
		break;
	case CARD_TYPE_SD:
		PRINTF("SD");
		break;
	case CARD_TYPE_MMC | CARD_TYPE_HC:
		PRINTF("MMC High Density");
		break;
	case CARD_TYPE_MMC:
		PRINTF("MMC");
		break;
	case CARD_TYPE_SDIO:
		PRINTF("SDIO\n\r");
		return;
	case CARD_TYPE_SD_COMBO:
		PRINTF("SD COMBO");
		break;
	case CARD_TYPE_UNKNOWN:
	default:
		PRINTF("Unknown\n\r");
		return;
	}
	PRINTF("\n\r    %d MB\n\r", (uint16_t)(sd_mmc_get_capacity(slot)/1024));
}
/*---------------------------------------------------------------------------*/
void 
sd_init( void )
{
	// HW chip-select pin should be set to output and high, to disable any SPI
	configure_output_pin(10, HIGH, DISABLE, DISABLE);
	// Initialize SD MMC stack
	sd_mmc_init();
	
	sd_mmc_err_t err;
	
	/* Initialize SD card */
	err = sd_mmc_check(SD_CONF_SLOT);
	if (err == SD_MMC_INIT_ONGOING) {
		err = sd_mmc_check(SD_CONF_SLOT);
		if (err != SD_MMC_OK) {
			PRINTF("SD init error: %u\n",err);
			return;
		}
	} else {
		PRINTF("SD init error: %u\n",err);
		return;
	}
	// Display basic card information
	sd_example_display_info_card(SD_CONF_SLOT);
	// Get card version
	card_version_t sd_version = sd_mmc_get_version(SD_CONF_SLOT);
	PRINTF("card version: %2x\n", sd_version);
	// Determine starting address (sector)
	uint32_t last_blocks_addr = sd_mmc_get_capacity(SD_CONF_SLOT) *
		(1024 / SD_MMC_BLOCK_SIZE);
	PRINTF("SD: last sector address: %u\n",last_blocks_addr);
	cfs_coffee_arch_start_block = last_blocks_addr - (FLASH_PAGES);		
}
/*--------------------------------------------------------------------------*/
void 
sd_read(uint32_t address, void *data, uint32_t length)
{
	PRINTF("COFFEE_READ %u %u\n",address, length);
	uint32_t starting_block, ending_block;
	/* Determine the starting block */
	starting_block = cfs_coffee_arch_start_block +
		(address / COFFEE_SECTOR_SIZE);
	PRINTF("COFFEE_START_READ_B %u\n",starting_block);
	/* Determine offset from block start */
	uint16_t block_offset = address % COFFEE_SECTOR_SIZE;
	PRINTF("COFFEE_BLOCK_OFFSET: %u\n", block_offset);
	/* Determine the final block */
	ending_block = cfs_coffee_arch_start_block + 
		(address + length) / COFFEE_SECTOR_SIZE;
	/* Number of blocks */
	uint16_t nb_blocks = ending_block - starting_block + 1;
	PRINTF("COFFEE_READ blocks: %u\n",nb_blocks);
	/* Init read */
	if (SD_MMC_OK != sd_mmc_init_read_blocks(SD_CONF_SLOT,
		starting_block,	nb_blocks)) {
		PRINTF("init read FAIL\n");
		return;
	}
	uint8_t block_data[COFFEE_SECTOR_SIZE];
	/* Read block by block */
	uint16_t block_index = 0;
	uint32_t pos = 0;
	uint32_t rem = length;
	
	while(nb_blocks > 0) {		
		nb_blocks--;
		
		if (SD_MMC_OK != sd_mmc_start_read_blocks(block_data, 1)) {
			PRINTF("read start [FAIL]\n\r");
			return;
		}
		if (SD_MMC_OK != sd_mmc_wait_end_of_read_blocks()) {
			PRINTF("read end [FAIL]\n\r");
			return;
		}
		/* Copy data */
		uint16_t copying;
		if (block_index == 0) {
			copying = min((COFFEE_SECTOR_SIZE-block_offset), rem);
			memcpy(&data[pos], &block_data[block_offset], copying);
		} else {
			copying = min(COFFEE_SECTOR_SIZE, rem);
			memcpy(&data[pos], block_data, copying);				
		}
		/* increment write index */
		pos += copying;
		/* decrement remaining length */
		rem -= copying;
		/* increment block index */
		block_index++;
	}	
	PRINTF("COFFEE_READ_ENDs\n");
}
/*--------------------------------------------------------------------------*/
void 
sd_erase( uint32_t sector )
{
	uint8_t block_data[COFFEE_SECTOR_SIZE];
	memset(block_data, 0, COFFEE_SECTOR_SIZE);
	
	/* Init write one block */
	if (SD_MMC_OK != sd_mmc_init_write_blocks(SD_CONF_SLOT,
		cfs_coffee_arch_start_block + sector, 1)) {
		printf("write init [FAIL]\n\r");
		return;
	}
	/* write */
	if (SD_MMC_OK != sd_mmc_start_write_blocks(block_data, 1)) {
		printf("start write [FAIL]\n\r");
		return;
	}
	if (SD_MMC_OK != sd_mmc_wait_end_of_write_blocks()) {
		printf("stop write [FAIL]\n\r");
		return;
	}
}
/*--------------------------------------------------------------------------*/
void 
sd_write( uint32_t address, const void *data, uint32_t length )
{
	if (sd_mmc_is_write_protected(SD_CONF_SLOT)) {
		printf("Card is write protected\n");
		return;
	}
	PRINTF("COFFEE_WRITE %u %u\n",address, length);
	int i;
	uint8_t *test = data;
	for (i=0; i<length; i++)
		PRINTF("%02x ",test[i]);
	PRINTF("\n");
	uint32_t starting_block, ending_block;
	/* Determine the starting block */
	starting_block = cfs_coffee_arch_start_block +
	(address / COFFEE_SECTOR_SIZE);
	PRINTF("COFFEE_START_WRITE_B %u\n",starting_block);
	/* Determine offset from block start */
	uint16_t block_offset = address % COFFEE_SECTOR_SIZE;
	PRINTF("COFFEE_BLOCK_OFFSET: %u\n", block_offset);
	/* Determine the final block */
	ending_block = cfs_coffee_arch_start_block +
	(address + length) / COFFEE_SECTOR_SIZE;
	/* Number of blocks */
	uint16_t nb_blocks = ending_block - starting_block + 1;
	PRINTF("COFFEE_WRITE blocks: %u\n",nb_blocks);
	
	uint8_t block_data[COFFEE_SECTOR_SIZE];
	/* Write block by block */
	uint16_t block_index = 0;
	uint32_t pos = 0;
	uint32_t rem = length;
	/* Write block by block */
	while (nb_blocks > 0) {
		nb_blocks--;
		/* Init read one block */
		if (SD_MMC_OK != sd_mmc_init_read_blocks(SD_CONF_SLOT,
			starting_block+block_index,	1)) {
			printf("init read FAIL\n");
			return;
		}
		/* Read block */
		if (SD_MMC_OK != sd_mmc_start_read_blocks(block_data, 1)) {
			printf("read start [FAIL]\n\r");
			return;
		}
		if (SD_MMC_OK != sd_mmc_wait_end_of_read_blocks()) {
			printf("read end [FAIL]\n\r");
			return;
		}
		/* Copy data */
		uint16_t copying;
		if (block_index == 0) {
			copying = min((COFFEE_SECTOR_SIZE-block_offset), rem);
			memcpy(&block_data[block_offset], &data[pos], copying);
		} else {
			copying = min(COFFEE_SECTOR_SIZE, rem);
			memcpy(block_data, &data[pos], copying);
		}
		/* Init write one block */
		for (i=0; i<COFFEE_SECTOR_SIZE; i++)
		PRINTF("%02x ",block_data[i]);
		PRINTF("\n");
		if (SD_MMC_OK != sd_mmc_init_write_blocks(SD_CONF_SLOT,
		starting_block + block_index, 1)) {
			printf("write init [FAIL]\n\r");
			return;
		}
		/* write */
		if (SD_MMC_OK != sd_mmc_start_write_blocks(block_data, 1)) {
			printf("start write [FAIL]\n\r");
			return;
		}
		if (SD_MMC_OK != sd_mmc_wait_end_of_write_blocks()) {
			printf("stop write [FAIL]\n\r");
			return;
		}
		/* increment write index */
		pos += copying;
		/* decrement remaining length */
		rem -= copying;
		/* increment block index */
		block_index++;
	}	
	PRINTF("COFFEE_WRITE_ENDs\n");
}

#define TESTCOFFEE 1
#define DEBUG_CFS 1
#if TESTCOFFEE
#if DEBUG_CFS
#include <stdio.h>
#define PRINTF_CFS(...) printf(__VA_ARGS__)
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF_CFS(...)
#endif



int
coffee_file_test(void)
{
#if WITH_SD_CARD_SUPPORT
//Nothing really
#else
  flash_init(FLASH_ACCESS_MODE_128, 6); // XXX John	
#endif
  int error;
  int wfd, rfd, afd;
  unsigned char buf[256], buf2[11];
  int r, i, j, total_read;
  unsigned offset;
  PRINTF("Starting test\n");
  cfs_remove("T1");
  cfs_remove("T2");
  cfs_remove("T3");
  cfs_remove("T4");
  cfs_remove("T5");
  
  wfd = rfd = afd = -1;

  for(r = 0; r < sizeof(buf); r++) {
    buf[r] = r;
  }

  PRINTF("TEST 1\n");

  /* Test 1: Open for writing. */
  wfd = cfs_open("T1", CFS_WRITE);
  if(wfd < 0) {
    FAIL(-1);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("2\n");

  /* Test 2: Write buffer. */
  r = cfs_write(wfd, buf, sizeof(buf));
  if(r < 0) {
    FAIL(-2);
  } else if(r < sizeof(buf)) {
    FAIL(-3);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("3\n");

  /* Test 3: Deny reading. */
  r = cfs_read(wfd, buf, sizeof(buf));
  if(r >= 0) {
    FAIL(-4);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("4\n");

  /* Test 4: Open for reading. */
  rfd = cfs_open("T1", CFS_READ);
  if(rfd < 0) {
    FAIL(-5);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("5\n");

  /* Test 5: Write to read-only file. */
  r = cfs_write(rfd, buf, sizeof(buf));
  if(r >= 0) {
    FAIL(-6);
  }
  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("7\n");

  /* Test 7: Read the buffer written in Test 2. */
  memset(buf, 0, sizeof(buf));
  r = cfs_read(rfd, buf, sizeof(buf));
  if(r < 0) {
    FAIL(-8);
  } else if(r < sizeof(buf)) {
    PRINTF_CFS("r=%d\n", r);
    FAIL(-9);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("8\n");

  /* Test 8: Verify that the buffer is correct. */
  for(r = 0; r < sizeof(buf); r++) {
    if(buf[r] != r) {
      PRINTF_CFS("r=%d. buf[r]=%d\n", r, buf[r]);
      FAIL(-10);
    }
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("9\n");

  /* Test 9: Seek to beginning. */
  if(cfs_seek(wfd, 0, CFS_SEEK_SET) != 0) {
    FAIL(-11);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("10\n");

  /* Test 10: Write to the log. */
  r = cfs_write(wfd, buf, sizeof(buf));
  if(r < 0) {
    FAIL(-12);
  } else if(r < sizeof(buf)) {
    FAIL(-13);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("11\n");

  /* Test 11: Read the data from the log. */
  cfs_seek(rfd, 0, CFS_SEEK_SET);
  memset(buf, 0, sizeof(buf));
  r = cfs_read(rfd, buf, sizeof(buf));
  if(r < 0) {
    FAIL(-14);
  } else if(r < sizeof(buf)) {
    FAIL(-15);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("12\n");

  /* Test 12: Verify that the data is correct. */
  for(r = 0; r < sizeof(buf); r++) {
    if(buf[r] != r) {
      FAIL(-16);
    }
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("13\n");

  /* Test 13: Write a reversed buffer to the file. */
  for(r = 0; r < sizeof(buf); r++) {
    buf[r] = sizeof(buf) - r - 1;
  }
  if(cfs_seek(wfd, 0, CFS_SEEK_SET) != 0) {
    FAIL(-17);
  }
  r = cfs_write(wfd, buf, sizeof(buf));
  if(r < 0) {
    FAIL(-18);
  } else if(r < sizeof(buf)) {
    FAIL(-19);
  }
  if(cfs_seek(rfd, 0, CFS_SEEK_SET) != 0) {
    FAIL(-20);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("14\n");

  /* Test 14: Read the reversed buffer. */
  cfs_seek(rfd, 0, CFS_SEEK_SET);
  memset(buf, 0, sizeof(buf));
  r = cfs_read(rfd, buf, sizeof(buf));
  if(r < 0) {
    FAIL(-21);
  } else if(r < sizeof(buf)) {
    PRINTF_CFS("r = %d\n", r);
    FAIL(-22);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("15\n");

  /* Test 15: Verify that the data is correct. */
  for(r = 0; r < sizeof(buf); r++) {
    if(buf[r] != sizeof(buf) - r - 1) {
      FAIL(-23);
    }
  }

  cfs_close(rfd);
  cfs_close(wfd);

  if(cfs_coffee_reserve("T2", FILE_SIZE) < 0) {
    FAIL(-24);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("16\n");

  /* Test 16: Test multiple writes at random offset. */
  for(r = 0; r < 100; r++) {
    wfd = cfs_open("T2", CFS_WRITE | CFS_READ);
    if(wfd < 0) {
      FAIL(-25);
    }
    offset = random_rand() % FILE_SIZE;
    for(r = 0; r < sizeof(buf); r++) {
      buf[r] = r;
    }
    if(cfs_seek(wfd, offset, CFS_SEEK_SET) != offset) {
      FAIL(-26);
    }
    if(cfs_write(wfd, buf, sizeof(buf)) != sizeof(buf)) {
      FAIL(-27);
    }
    if(cfs_seek(wfd, offset, CFS_SEEK_SET) != offset) {
      FAIL(-28);
    }
    memset(buf, 0, sizeof(buf));
    if(cfs_read(wfd, buf, sizeof(buf)) != sizeof(buf)) {
      FAIL(-29);
    }
    for(i = 0; i < sizeof(buf); i++) {
      if(buf[i] != i) {
        PRINTF_CFS("buf[%d] != %d\n", i, buf[i]);
        FAIL(-30);
      }
    }
  }
  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("17\n");

  /* Test 17: Append data to the same file many times. */
#define APPEND_BYTES 3000
#define BULK_SIZE 10
  for(i = 0; i < APPEND_BYTES; i += BULK_SIZE) {
    afd = cfs_open("T3", CFS_WRITE | CFS_APPEND);
    if(afd < 0) {
      FAIL(-31);
    }
    for(j = 0; j < BULK_SIZE; j++) {
      buf[j] = 1 + ((i + j) & 0x7f);
    }
    if((r = cfs_write(afd, buf, BULK_SIZE)) != BULK_SIZE) {
      PRINTF_CFS("Count:%d, r=%d\n", i, r);
      FAIL(-32);
    }
    cfs_close(afd);
  }

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("18\n");

  /* Test 18: Read back the data written in Test 17 and verify. */
  afd = cfs_open("T3", CFS_READ);
  if(afd < 0) {
    FAIL(-33);
  }
  total_read = 0;
  while((r = cfs_read(afd, buf2, sizeof(buf2))) > 0) {
    for(j = 0; j < r; j++) {
      if(buf2[j] != 1 + ((total_read + j) & 0x7f)) {
        FAIL(-34);
      }
    }
    total_read += r;
  }
  if(r < 0) {
    PRINTF_CFS("FAIL:-35 r=%d\n", r);
    FAIL(-35);
  }
  if(total_read != APPEND_BYTES) {
    PRINTF_CFS("FAIL:-35 total_read=%d\n", total_read);
    FAIL(-35);
  }
  cfs_close(afd);

  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("19\n");

  /* T4 */
  /* 
   * file T4 and T5 writing forces to use garbage collector in greedy mode
   * this test is designed for 10kb of file system
   */
#define APPEND_BYTES_1 2000
#define BULK_SIZE_1 10
  for(i = 0; i < APPEND_BYTES_1; i += BULK_SIZE_1) {
    afd = cfs_open("T4", CFS_WRITE | CFS_APPEND);
    if(afd < 0) {
      FAIL(-36);
    }
    for(j = 0; j < BULK_SIZE_1; j++) {
      buf[j] = 1 + ((i + j) & 0x7f);
    }

    if((r = cfs_write(afd, buf, BULK_SIZE_1)) != BULK_SIZE_1) {
      PRINTF_CFS("Count:%d, r=%d\n", i, r);
      FAIL(-37);
    }
    cfs_close(afd);
  }

  afd = cfs_open("T4", CFS_READ);
  if(afd < 0) {
    FAIL(-38);
  }

  total_read = 0;
  while((r = cfs_read(afd, buf2, sizeof(buf2))) > 0) {
    for(j = 0; j < r; j++) {
      if(buf2[j] != 1 + ((total_read + j) & 0x7f)) {
        PRINTF_CFS("FAIL:-39, total_read=%d r=%d\n", total_read, r);
        FAIL(-39);
      }
    }
    total_read += r;
  }
  if(r < 0) {
    PRINTF_CFS("FAIL:-40 r=%d\n", r);
    FAIL(-40);
  }
  if(total_read != APPEND_BYTES_1) {
    PRINTF_CFS("FAIL:-41 total_read=%d\n", total_read);
    FAIL(-41);
  }
  cfs_close(afd);

  /* T5 */
  PRINTF("PASSED\n");
  PRINTF("TEST ");
  PRINTF("20\n");
#define APPEND_BYTES_2 1000
#define BULK_SIZE_2 10
  for(i = 0; i < APPEND_BYTES_2; i += BULK_SIZE_2) {
    afd = cfs_open("T5", CFS_WRITE | CFS_APPEND);
    if(afd < 0) {
      FAIL(-42);
    }
    for(j = 0; j < BULK_SIZE_2; j++) {
      buf[j] = 1 + ((i + j) & 0x7f);
    }

    if((r = cfs_write(afd, buf, BULK_SIZE_2)) != BULK_SIZE_2) {
      PRINTF_CFS("Count:%d, r=%d\n", i, r);
      FAIL(-43);
    }

    cfs_close(afd);
  }

  afd = cfs_open("T5", CFS_READ);
  if(afd < 0) {
    FAIL(-44);
  }
  total_read = 0;
  while((r = cfs_read(afd, buf2, sizeof(buf2))) > 0) {
    for(j = 0; j < r; j++) {
      if(buf2[j] != 1 + ((total_read + j) & 0x7f)) {
        PRINTF_CFS("FAIL:-45, total_read=%d r=%d\n", total_read, r);
        FAIL(-45);
      }
    }
    total_read += r;
  }
  if(r < 0) {
    PRINTF_CFS("FAIL:-46 r=%d\n", r);
    FAIL(-46);
  }
  if(total_read != APPEND_BYTES_2) {
    PRINTF_CFS("FAIL:-47 total_read=%d\n", total_read);
    FAIL(-47);
  }
  cfs_close(afd);

  PRINTF("PASSED\n");

  error = 0;
end:
  cfs_close(wfd);
  cfs_close(rfd);
  cfs_close(afd);
  return error;
}





#endif /* TESTCOFFEE */
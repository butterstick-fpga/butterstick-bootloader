/*
 *  Copyright 2021 Gregory Davill <greg.davill@gmail.com>
 */
#ifndef FLASH_H_
#define FLASH_H_

#include <stdint.h>
#include <spiflash.h>

void spi_read_uuid(uint8_t* uuid);
uint32_t spiId(uint8_t*);


int spiflash_write_stream(uint32_t addr, uint8_t *stream, int len);
void spiflash_read_uuid(uint8_t* uuid);
bool spiflash_protection_read(void);
void spiflash_protection_write(bool lock);

#define FLASH_64K_BLOCK_ERASE_SIZE (64*1024)
#define FLASH_4K_BLOCK_ERASE_SIZE (4*1024)

#endif /* FLASH_H_ */

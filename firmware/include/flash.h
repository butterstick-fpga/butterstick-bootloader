/*  Originally from: https://github.com/im-tomu/foboot/blob/master/src/include/spi.h
 *  Apache License Version 2.0
 *	Copyright 2019 Sean 'xobs' Cross <sean@xobs.io>
 *  Copyright 2020 Gregory Davill <greg.davill@gmail.com>
 */

#ifndef FLASH_H_
#define FLASH_H_

#include <stdint.h>

void spiPause(void);
void spiBegin(void);
void spiEnd(void);

int spiRead(uint32_t addr, uint8_t *data, unsigned int count);
int spiIsBusy(void);
int spiBeginErase4(uint32_t erase_addr);
int spiBeginErase32(uint32_t erase_addr);
int spiBeginErase64(uint32_t erase_addr);
int spiBeginWrite(uint32_t addr, const void *data, unsigned int count);
void spiEnableQuad(void);

void spi_read_uuid(uint8_t* uuid);
uint32_t spiId(uint8_t*);

int spiWrite(uint32_t addr, const uint8_t *data, unsigned int count);
uint8_t spiReset(void);
int spiInit(void);

void spiHold(void);
void spiUnhold(void);
void spiSwapTxRx(void);

void spiFree(void);

#define FLASH_64K_BLOCK_ERASE_SIZE (64*1024)
#define FLASH_4K_BLOCK_ERASE_SIZE (4*1024)

#endif /* BB_SPI_H_ */

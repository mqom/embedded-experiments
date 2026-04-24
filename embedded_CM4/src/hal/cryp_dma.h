#ifndef __CRYP_DMA_H__
#define __CRYP_DMA_H__

#include <string.h>
#include <stdint.h>

void cryp_dma_init(void);

void cryp_dma_go(const uint8_t *pt, uint8_t *ct, uint32_t len_in_bytes);

void cryp_dma_wait_finished(void);

#endif

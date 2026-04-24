#if defined(STM32F437)

#include "cryp_dma.h"

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/crypto.h>

void cryp_dma_init(void) {
    // RCC
    rcc_periph_clock_enable(RCC_CRYP);
    rcc_periph_clock_enable(RCC_DMA2);

    // Reset and Configuration CRYP
    CRYP_CR = 0;
    CRYP_CR |= CRYP_CR_FFLUSH; // Flush FIFOs

    // Configuration DMA2
    dma_stream_reset(DMA2, DMA_STREAM6); // IN
    dma_stream_reset(DMA2, DMA_STREAM5); // OUT

    // --- IN (Stream 6 / Channel 2) ---
    dma_set_peripheral_address(DMA2, DMA_STREAM6, (uint32_t)&CRYP_DIN);
    dma_channel_select(DMA2, DMA_STREAM6, DMA_SxCR_CHSEL(2));
    dma_set_transfer_mode(DMA2, DMA_STREAM6, DMA_SxCR_DIR_MEM_TO_PERIPHERAL);
    dma_enable_memory_increment_mode(DMA2, DMA_STREAM6);
    dma_set_peripheral_size(DMA2, DMA_STREAM6, DMA_SxCR_PSIZE_32BIT);
    dma_set_memory_size(DMA2, DMA_STREAM6, DMA_SxCR_MSIZE_32BIT);
    dma_set_priority(DMA2, DMA_STREAM6, DMA_SxCR_PL_VERY_HIGH);

    // --- Out (Stream 5 / Channel 2) ---
    dma_set_peripheral_address(DMA2, DMA_STREAM5, (uint32_t)&CRYP_DOUT);
    dma_channel_select(DMA2, DMA_STREAM5, DMA_SxCR_CHSEL(2));
    dma_set_transfer_mode(DMA2, DMA_STREAM5, DMA_SxCR_DIR_PERIPHERAL_TO_MEM);
    dma_enable_memory_increment_mode(DMA2, DMA_STREAM5);
    dma_set_peripheral_size(DMA2, DMA_STREAM5, DMA_SxCR_PSIZE_32BIT);
    dma_set_memory_size(DMA2, DMA_STREAM5, DMA_SxCR_MSIZE_32BIT);
    dma_set_priority(DMA2, DMA_STREAM5, DMA_SxCR_PL_VERY_HIGH);

    // CRYP <-> DMA
    CRYP_DMACR = CRYP_DMACR_DIEN | CRYP_DMACR_DOEN;
}

void cryp_dma_go(const uint8_t *src, uint8_t *dst, uint32_t len_in_bytes)
{
    uint32_t len_in_words = len_in_bytes / 4;

    if(((uint32_t)src % 4) || ((uint32_t)dst % 4)){
        // Deal with buffer alignments
        uint32_t aligned_src[len_in_words];
        uint32_t aligned_dst[len_in_words];
        memcpy(aligned_src, src, len_in_bytes);
        memcpy(aligned_dst, dst, len_in_bytes);

        dma_set_memory_address(DMA2, DMA_STREAM6, (uint32_t)aligned_src);
        dma_set_number_of_data(DMA2, DMA_STREAM6, len_in_words);
    
        dma_set_memory_address(DMA2, DMA_STREAM5, (uint32_t)aligned_dst);
        dma_set_number_of_data(DMA2, DMA_STREAM5, len_in_words);
    }
    else{
        dma_set_memory_address(DMA2, DMA_STREAM6, (uint32_t)src);
        dma_set_number_of_data(DMA2, DMA_STREAM6, len_in_words);
    
        dma_set_memory_address(DMA2, DMA_STREAM5, (uint32_t)dst);
        dma_set_number_of_data(DMA2, DMA_STREAM5, len_in_words);
    }

    dma_enable_stream(DMA2, DMA_STREAM5);
    dma_enable_stream(DMA2, DMA_STREAM6);

    // Start
    CRYP_CR |= CRYP_CR_CRYPEN;
}

void cryp_dma_wait_finished(void)
{
	while (!dma_get_interrupt_flag(DMA2, DMA_STREAM5, DMA_TCIF));
	while ((CRYP_SR & CRYP_SR_BUSY) != 0);
	dma_clear_interrupt_flags(DMA2, DMA_STREAM5, DMA_TCIF | DMA_HTIF | DMA_TEIF | DMA_DMEIF | DMA_FEIF);
	dma_clear_interrupt_flags(DMA2, DMA_STREAM6, DMA_TCIF | DMA_HTIF | DMA_TEIF | DMA_DMEIF | DMA_FEIF);
    	CRYP_CR &= ~CRYP_CR_CRYPEN;
}

#else
/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif

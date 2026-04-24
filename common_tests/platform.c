#include <benchmark/utils.h>
#include <benchmark.h>
#include <benchmark/timing.h>

/* Redirect platform_get_cycles to the board's HAL get cycles */
extern uint64_t hal_get_cycles(void);
long long platform_get_cycles(void){
        return (long long)hal_get_cycles();
}               

/* Implement our local gettimeofday */
extern uint32_t _clock_freq;
extern uint64_t hal_get_systick_counter(void);
void gettimeofday(struct timeval *t, void *dummy)
{   
        (void)dummy;
        uint64_t ctr = hal_get_systick_counter();
        t->tv_sec  = (ctr / 100);
        t->tv_usec = (ctr - (100 * (ctr / 100))) * 10000;
 
        return;
}               
 
void panic(void){
        printf("Panic!\r\n");
        while(1){}
}

#if defined(BOARD_HAS_HW_AES) && defined(RIJNDAEL_EXTERNAL) && !defined(NO_HW_AES) && defined(RIJNDAEL_TEST)

static void hexdump(const char *prefix, const unsigned char *in, unsigned int sz)
{           
        unsigned int i;
        if(prefix != NULL){
                printf("%s", prefix);
        }
        for(i = 0; i < sz; i++){
                printf("%02x", (unsigned char)in[i]);
        }
        printf("\r\n");
}

// Dedicated test for hardware AES
extern void hal_cryp_aes_128_set_key(const uint8_t key[16]);
extern void hal_cryp_aes_128_set_key_dma(const uint8_t key[16]);
extern void hal_cryp_aes_128_enc(const uint8_t *pt, uint8_t *ct, uint32_t sz);
extern void hal_cryp_aes_128_enc_dma(const uint8_t *pt, uint8_t *ct, uint32_t sz);
extern void cryp_dma_init(void);
__attribute__((constructor)) void test_hw_aes_dma_poll(void)
{
        // HW AES tests
        unsigned int i;
        // Test vectors
        uint8_t key[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0X07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
        uint32_t pt[2048 * 16 / 4] = { 0 };
        uint32_t ct[2048 * 16 / 4] = { 0 };
	uint32_t ct_check[2048 * 16 / 4] = { 0 };
	uint64_t t0, t1;

	for(i = 0; i < (4 * 2048); i+=4){
		pt[i]     = 0x33221100;
		pt[i + 1] = 0x77665544;
		pt[i + 2] = 0xbbaa9988;
		pt[i + 3] = 0xffeeddcc;
		//
		ct_check[i]     = 0xd8e0c469;
		ct_check[i + 1] = 0x30047b6a;
		ct_check[i + 2] = 0x80b7cdd8;
		ct_check[i + 3] = 0x5ac5b470;
	}

        printf(" ==== Hardware AES polling versus DMA benchmarks =====\r\n");
        // Tests for polling
        printf("==== POLLING\r\n");
        for(i = 1; i < 2048; i++){
		memset(ct, 0, sizeof(ct));
                t0 = hal_get_cycles();
                hal_cryp_aes_128_set_key(key);
                hal_cryp_aes_128_enc((uint8_t*)pt, (uint8_t*)ct, 16 * i);
                t1 = hal_get_cycles();
                printf("%lld ", t1-t0);
		if(memcmp(ct, ct_check, 16 * i)){
			printf("Error for HW AES POLLING...\r\n");
			while(1);
		}
        }
	printf(" \r\n\n\n");
        // Tests for DMA
        printf("==== DMA\r\n");
        for(i = 1; i < 2048; i++){
		memset(ct, 0, sizeof(ct));
                t0 = hal_get_cycles();
                hal_cryp_aes_128_set_key_dma(key);
                hal_cryp_aes_128_enc_dma((uint8_t*)pt, (uint8_t*)ct, 16 * i);
                t1 = hal_get_cycles();
                printf("%lld ", t1-t0);
		if(memcmp(ct, ct_check, 16 * i)){
			printf("Error for HW AES DMA...\r\n");
			while(1);
		}
        }
	printf(" \r\n\n\n");
        printf("======================\r\n\n\n");
        // =============================
}
#endif

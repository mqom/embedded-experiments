#include "test_rijndael.h"
#include "rijndael_generated_tests.h"

#ifdef EXTERNAL_HAL_GET_CYCLES
extern uint64_t hal_get_cycles(void);
#else
#ifdef NO_CYCLES_COUNT
uint64_t hal_get_cycles(void)
{
	return 0;
}
#else
uint64_t hal_get_cycles(void)
{
        uint64_t result;

#if defined(__x86_64__)
        uint64_t a, d;
        // Getting cycles on the x86_64 platform
        asm volatile ("rdtsc" : "=a" (a), "=d" (d));
        result = (d << 32) | a;
#elif defined(__i386__)
        // Getting cycles on the x86 32-bit platform
        asm volatile ("rdtsc" : "=A" (result));
#elif defined(__aarch64__)
        // Getting cycles on the ARM aarch64 platform
        asm volatile ("isb \r\n mrs %0, CNTVCT_EL0" : "=r" (result));
#else
#error "Error: unknown local platform for getting CPU cycles (only x86_64, i386 and ARM aarch64 are supported). Please adapt to your platform!"
#endif
        return result;
}
#endif
#endif

void hexdump(const char *prefix, const unsigned char *in, unsigned int sz)
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

#ifndef NUM_TESTS_CYCLES
#define NUM_TESTS_CYCLES 500000
#endif

int main(int argc, char *argv[]){
	uint8_t key[32] = { 0 };
	uint8_t pt[32] = { 0 };
	uint8_t ct[32] = { 0 };
#if !defined(NO_AES128_TESTS)
	rijndael_ctx_aes128 ctx_aes128 = { 0 };
#endif
#if !defined(NO_AES256_TESTS)
	rijndael_ctx_aes256 ctx_aes256 = { 0 };
#endif
#if !defined(NO_RIJNDAEL256_TESTS)
	rijndael_ctx_rijndael256 ctx_rijndael256 = { 0 };
#endif
	int ret = -1;
	unsigned int i, j, k, num_aes128 = 0, num_aes256 = 0, num_rijndael256 = 0;
	const char *implementation =
#if defined(RIJNDAEL_CONSTANT_TIME_REF)
		"Constant time reference Rijndael implementation";
#elif defined(RIJNDAEL_TABLE)
		"Non-constant time table based Rijndael implementation";
#elif defined(RIJNDAEL_AES_NI)
		"Constant time AES-NI based Rijndael implementation";
#elif defined(RIJNDAEL_BITSLICE)
		"Constant time bitslice based Rijndael implementation";
#elif defined(RIJNDAEL_EXTERNAL)
		"Externally provided Rijndael implementation";
#else
#error "Unkown Rijndael implementation ..."
#endif

	(void)argv;
	(void)argc;

	for(i = 0; i < sizeof(all_rijndael_tests) / sizeof(rijndael_test); i++){
		switch (all_rijndael_tests[i].type){
			case AES128:{
#if !defined(NO_AES128_TESTS)
				num_aes128++;
				/* ===== Test AES-128 ===== */
				ret = aes128_setkey_enc(&ctx_aes128, all_rijndael_tests[i].key);
				if(ret){
					printf("[-] Error when testing AES-128 for aes128_setkey_enc @ test %d ...\r\n", i);
					goto err;
				}
				ret = aes128_enc(&ctx_aes128, all_rijndael_tests[i].pt, ct);	
				if(ret){
					printf("[-] Error when testing AES-128 for aes128_enc @ test %d ...\r\n", i);
					goto err;
				}
				if(memcmp(ct, all_rijndael_tests[i].ct, 16)){
					printf("[-] Error: AES-128 ciphertexts differ @ test %d!\r\n", i);
					hexdump("    Got:      ", ct, 16);
					hexdump("    Expected: ", all_rijndael_tests[i].ct, 16);
					goto err;
				}
#endif
				break;
			}
			case AES256:{
#if !defined(NO_AES256_TESTS)
				num_aes256++;
				/* ===== Test AES-256 ===== */
				ret = aes256_setkey_enc(&ctx_aes256, all_rijndael_tests[i].key);
				if(ret){
					printf("[-] Error when testing AES-256 for aes256_setkey_enc @ test %d ...\r\n", i);
					goto err;
				}
				ret = aes256_enc(&ctx_aes256, all_rijndael_tests[i].pt, ct);	
				if(ret){
					printf("[-] Error when testing AES-256 for aes256_enc @ test %d ...\r\n", i);
					goto err;
				}
				if(memcmp(ct, all_rijndael_tests[i].ct, 16)){
					printf("[-] Error: AES-256 ciphertexts differ @ test %d!\r\n", i);
					hexdump("    Got:      ", ct, 16);
					hexdump("    Expected: ", all_rijndael_tests[i].ct, 16);
					goto err;
				}
#endif
				break;
			}
			case RIJNDAEL_256_256:{
#if !defined(NO_RIJNDAEL256_TESTS)
				num_rijndael256++;
				/* ===== Test RIJNDAEL-256 ===== */
				ret = rijndael256_setkey_enc(&ctx_rijndael256, all_rijndael_tests[i].key);
				if(ret){
					printf("[-] Error when testing RIJNDAEL-256 for rijndael256_setkey_enc @ test %d ...\r\n", i);
					goto err;
				}
				ret = rijndael256_enc(&ctx_rijndael256, all_rijndael_tests[i].pt, ct);	
				if(ret){
					printf("[-] Error when testing RIJNDAEL-256 for rijndael256_enc @ test %d ...\r\n", i);
					goto err;
				}
				if(memcmp(ct, all_rijndael_tests[i].ct, 32)){
					printf("[-] Error: RIJNDAEL-256 ciphertexts differ @ test %d!\r\n", i);
					hexdump("    Got:      ", ct, 32);
					hexdump("    Expected: ", all_rijndael_tests[i].ct, 32);
					goto err;
				}
#endif
				break;
			}
			default:{
				printf("[-] Error: unsupported Rijndael type @ test %d\r\n", i);
				goto err;
			}
		}
	}

	printf("[+] All %d tests went OK for '%s'! (%d of AES-128, %d of AES-256, %d of RIJNDAEL-256)\r\n", (int) (sizeof(all_rijndael_tests) / sizeof(rijndael_test)), implementation, num_aes128, num_aes256, num_rijndael256);

	/* Test the x2, x4 and x8 APIS */
#if !defined(NO_AES128_TESTS)
	rijndael_ctx_aes128 ctx_x8_aes128[8] = { 0 };
	rijndael_ctx_aes128_x2 ctx_x2_x2_aes128 = { 0 };
	rijndael_ctx_aes128_x4 ctx_x4_x4_aes128 = { 0 };
	rijndael_ctx_aes128_x8 ctx_x8_x8_aes128 = { 0 };
#endif
#if !defined(NO_AES256_TESTS)
	rijndael_ctx_aes256 ctx_x8_aes256[8] = { 0 };
	rijndael_ctx_aes256_x2 ctx_x2_x2_aes256 = { 0 };
	rijndael_ctx_aes256_x4 ctx_x4_x4_aes256 = { 0 };
	rijndael_ctx_aes256_x8 ctx_x8_x8_aes256 = { 0 };
#endif
#if !defined(NO_RIJNDAEL256_TESTS)
	rijndael_ctx_rijndael256 ctx_x8_rijndael256[8] = { 0 };
	rijndael_ctx_rijndael256_x2 ctx_x2_x2_rijndael256 = { 0 };
	rijndael_ctx_rijndael256_x4 ctx_x4_x4_rijndael256 = { 0 };
	rijndael_ctx_rijndael256_x8 ctx_x8_x8_rijndael256 = { 0 };
#endif
	uint8_t key_x8[8][32];
	uint8_t pt_x8[8][32];
	uint8_t ct_x8[8][32];

#if !defined(NO_AES128_TESTS)
	for(i = 0; i < 32; i++){
		for(j = 0; j < 8; j++){
			key_x8[j][i] = rand();
			pt_x8[j][i] = rand();
		}
	}
	for(j = 0; j < 8; j++){
		ret = aes128_setkey_enc(&ctx_x8_aes128[j], (uint8_t*)&key_x8[j]);
		if(ret){
			printf("[-] Error when testing AES-128 X2/X4/X8 ...\r\n");
			goto err;
		}
	}
	ret = aes128_setkey_enc_x2(&ctx_x2_x2_aes128, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1]);
	if(ret){
		printf("[-] Error when testing AES-128 X2/X4/X8 ...\r\n");
		goto err;
	}
	ret = aes128_setkey_enc_x4(&ctx_x4_x4_aes128, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3]);
	if(ret){
		printf("[-] Error when testing AES-128 X2/X4/X8 ...\r\n");
		goto err;
	}
	ret = aes128_setkey_enc_x8(&ctx_x8_x8_aes128, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3], (uint8_t*)&key_x8[4], (uint8_t*)&key_x8[5], (uint8_t*)&key_x8[6], (uint8_t*)&key_x8[7]);
	if(ret){
		printf("[-] Error when testing AES-128 X2/X4/X8 ...\r\n");
		goto err;
	}

	/* ===== AES-128 */
	ret = aes128_enc_x2(&ctx_x8_aes128[0], &ctx_x8_aes128[1], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);	
	if(ret){
		printf("[-] Error when testing AES-128 X2 ...\r\n");
		goto err;
	}
	printf("=== AES-128 X2 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	ret = aes128_enc_x2_x2(&ctx_x2_x2_aes128, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
	if(ret){
		printf("[-] Error when testing AES-128 X2 ...\r\n");
		goto err;
	}
	printf("=== AES-128 X2 X2 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);

	ret = aes128_enc_x4(&ctx_x8_aes128[0], &ctx_x8_aes128[1], &ctx_x8_aes128[2], &ctx_x8_aes128[3], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
	if(ret){
		printf("[-] Error when testing AES-128 X4 ...\r\n");
		goto err;
	}
	printf("=== AES-128 X4 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);
	ret = aes128_enc_x4_x4(&ctx_x4_x4_aes128, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
	if(ret){
		printf("[-] Error when testing AES-128 X4 ...\r\n");
		goto err;
	}
	printf("=== AES-128 X4 X4 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);


	ret = aes128_enc_x8(&ctx_x8_aes128[0], &ctx_x8_aes128[1], &ctx_x8_aes128[2], &ctx_x8_aes128[3], &ctx_x8_aes128[4], &ctx_x8_aes128[5], &ctx_x8_aes128[6], &ctx_x8_aes128[7], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);	
	if(ret){
		printf("[-] Error when testing AES-128 X8 ...\r\n");
		goto err;
	}
	printf("=== AES-128 X8 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);
	hexdump("ct5: ", ct_x8[4], 16);
	hexdump("ct6: ", ct_x8[5], 16);
	hexdump("ct7: ", ct_x8[6], 16);
	hexdump("ct8: ", ct_x8[7], 16);
	ret = aes128_enc_x8_x8(&ctx_x8_x8_aes128, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);	
	if(ret){
		printf("[-] Error when testing AES-128 X8 ...\r\n");
		goto err;
	}
	printf("=== AES-128 X8 X8 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);
	hexdump("ct5: ", ct_x8[4], 16);
	hexdump("ct6: ", ct_x8[5], 16);
	hexdump("ct7: ", ct_x8[6], 16);
	hexdump("ct8: ", ct_x8[7], 16);
#endif


#if !defined(NO_AES256_TESTS)
	/* ===== AES-256 */
	for(j = 0; j < 8; j++){
		ret = aes256_setkey_enc(&ctx_x8_aes256[j], (uint8_t*)&key_x8[j]);
		if(ret){
			printf("[-] Error when testing AES-256 X2/X4 ...\r\n");
			goto err;
		}
	}
	ret = aes256_setkey_enc_x2(&ctx_x2_x2_aes256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1]);
	if(ret){
		printf("[-] Error when testing AES-256 X2/X4/X8 ...\r\n");
		goto err;
	}
	ret = aes256_setkey_enc_x4(&ctx_x4_x4_aes256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3]);
	if(ret){
		printf("[-] Error when testing AES-256 X2/X4/X8 ...\r\n");
		goto err;
	}
	ret = aes256_setkey_enc_x8(&ctx_x8_x8_aes256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3], (uint8_t*)&key_x8[4], (uint8_t*)&key_x8[5], (uint8_t*)&key_x8[6], (uint8_t*)&key_x8[7]);
	if(ret){
		printf("[-] Error when testing AES-256 X2/X4/X8 ...\r\n");
		goto err;
	}

	ret = aes256_enc_x2(&ctx_x8_aes256[0], &ctx_x8_aes256[1], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);	
	if(ret){
		printf("[-] Error when testing AES-256 X2 ...\r\n");
		goto err;
	}
	printf("=== AES-256 X2 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	ret = aes256_enc_x2_x2(&ctx_x2_x2_aes256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
	if(ret){
		printf("[-] Error when testing AES-256 X2 ...\r\n");
		goto err;
	}
	printf("=== AES-256 X2 X2 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);

	ret = aes256_enc_x4(&ctx_x8_aes256[0], &ctx_x8_aes256[1], &ctx_x8_aes256[2], &ctx_x8_aes256[3], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
	if(ret){
		printf("[-] Error when testing AES-256 X4 ...\r\n");
		goto err;
	}
	printf("=== AES-256 X4 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);
	ret = aes256_enc_x4_x4(&ctx_x4_x4_aes256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
	if(ret){
		printf("[-] Error when testing AES-256 X4 ...\r\n");
		goto err;
	}
	printf("=== AES-256 X4 X4 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);


	ret = aes256_enc_x8(&ctx_x8_aes256[0], &ctx_x8_aes256[1], &ctx_x8_aes256[2], &ctx_x8_aes256[3], &ctx_x8_aes256[4], &ctx_x8_aes256[5], &ctx_x8_aes256[6], &ctx_x8_aes256[7], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
	if(ret){
		printf("[-] Error when testing AES-256 X8 ...\r\n");
		goto err;
	}
	printf("=== AES-256 X8 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);
	hexdump("ct5: ", ct_x8[4], 16);
	hexdump("ct6: ", ct_x8[5], 16);
	hexdump("ct7: ", ct_x8[6], 16);
	hexdump("ct8: ", ct_x8[7], 16);
	ret = aes256_enc_x8_x8(&ctx_x8_x8_aes256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
	if(ret){
		printf("[-] Error when testing AES-256 X8 ...\r\n");
		goto err;
	}
	printf("=== AES-256 X8 X8 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 16);
	hexdump("ct2: ", ct_x8[1], 16);
	hexdump("ct3: ", ct_x8[2], 16);
	hexdump("ct4: ", ct_x8[3], 16);
	hexdump("ct5: ", ct_x8[4], 16);
	hexdump("ct6: ", ct_x8[5], 16);
	hexdump("ct7: ", ct_x8[6], 16);
	hexdump("ct8: ", ct_x8[7], 16);
#endif

#if !defined(NO_RIJNDAEL256_TESTS)
	/* ===== RIJNDAEL-256 */
	for(j = 0; j < 8; j++){
		ret = rijndael256_setkey_enc(&ctx_x8_rijndael256[j], (uint8_t*)&key_x8[j]);
		if(ret){
			printf("[-] Error when testing RIJNDAEL-256 X2/X4 ...\r\n");
			goto err;
		}
	}
	ret = rijndael256_setkey_enc_x2(&ctx_x2_x2_rijndael256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1]);
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X2/X4/X8 ...\r\n");
		goto err;
	}
	ret = rijndael256_setkey_enc_x4(&ctx_x4_x4_rijndael256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3]);
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X2/X4/X8 ...\r\n");
		goto err;
	}
	ret = rijndael256_setkey_enc_x8(&ctx_x8_x8_rijndael256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3], (uint8_t*)&key_x8[4], (uint8_t*)&key_x8[5], (uint8_t*)&key_x8[6], (uint8_t*)&key_x8[7]);
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X2/X4/X8 ...\r\n");
		goto err;
	}

	ret = rijndael256_enc_x2(&ctx_x8_rijndael256[0], &ctx_x8_rijndael256[1], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);	
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X2 ...\r\n");
		goto err;
	}
	printf("=== RIJNDAEL-256 X2 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 32);
	hexdump("ct2: ", ct_x8[1], 32);
	ret = rijndael256_enc_x2_x2(&ctx_x2_x2_rijndael256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X2 ...\r\n");
		goto err;
	}
	printf("=== RIJNDAEL-256 X2 X2 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 32);
	hexdump("ct2: ", ct_x8[1], 32);

	ret = rijndael256_enc_x4(&ctx_x8_rijndael256[0], &ctx_x8_rijndael256[1], &ctx_x8_rijndael256[2], &ctx_x8_rijndael256[3], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X4 ...\r\n");
		goto err;
	}
	printf("=== RIJNDAEL-256 X4 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 32);
	hexdump("ct2: ", ct_x8[1], 32);
	hexdump("ct3: ", ct_x8[2], 32);
	hexdump("ct4: ", ct_x8[3], 32);

	ret = rijndael256_enc_x4_x4(&ctx_x4_x4_rijndael256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X4 ...\r\n");
		goto err;
	}
	printf("=== RIJNDAEL-256 X4 X4 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 32);
	hexdump("ct2: ", ct_x8[1], 32);
	hexdump("ct3: ", ct_x8[2], 32);
	hexdump("ct4: ", ct_x8[3], 32);

	ret = rijndael256_enc_x8(&ctx_x8_rijndael256[0], &ctx_x8_rijndael256[1], &ctx_x8_rijndael256[2], &ctx_x8_rijndael256[3], &ctx_x8_rijndael256[4], &ctx_x8_rijndael256[5], &ctx_x8_rijndael256[6], &ctx_x8_rijndael256[7], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);	
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X8 ...\r\n");
		goto err;
	}
	printf("=== RIJNDAEL-256 X8 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 32);
	hexdump("ct2: ", ct_x8[1], 32);
	hexdump("ct3: ", ct_x8[2], 32);
	hexdump("ct4: ", ct_x8[3], 32);
	hexdump("ct5: ", ct_x8[4], 32);
	hexdump("ct6: ", ct_x8[5], 32);
	hexdump("ct7: ", ct_x8[6], 32);
	hexdump("ct8: ", ct_x8[7], 32);
	ret = rijndael256_enc_x8_x8(&ctx_x8_x8_rijndael256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);	
	if(ret){
		printf("[-] Error when testing RIJNDAEL-256 X8 ...\r\n");
		goto err;
	}
	printf("=== RIJNDAEL-256 X8 X8 ====\r\n");
	hexdump("ct1: ", ct_x8[0], 32);
	hexdump("ct2: ", ct_x8[1], 32);
	hexdump("ct3: ", ct_x8[2], 32);
	hexdump("ct4: ", ct_x8[3], 32);
	hexdump("ct5: ", ct_x8[4], 32);
	hexdump("ct6: ", ct_x8[5], 32);
	hexdump("ct7: ", ct_x8[6], 32);
	hexdump("ct8: ", ct_x8[7], 32);
#endif

	/* Check performance */
	uint64_t t0, t1, t_total_keysched = 0, t_total_enc = 0;
#if !defined(NO_AES128_TESTS)
	// ==== Test cycles counting: AES-128
	for(i = 0; i < NUM_TESTS_CYCLES; i++){
		for(j = 0; j < 32; j++){
			key[j] = rand();
			pt[j] = rand();
		}
		t0 = hal_get_cycles();
		ret = aes128_setkey_enc(&ctx_aes128, key);
		t1 = hal_get_cycles();
		t_total_keysched += (t1 - t0);
		if(ret){
			printf("[-] Error when testing AES-128 for aes128_setkey_enc @ test %d ...\r\n", i);
			goto err;
		}
		t0 = hal_get_cycles();
		ret = aes128_enc(&ctx_aes128, pt, ct);
		t1 = hal_get_cycles();
		t_total_enc += (t1 - t0);
		if(ret){
			printf("[-] Error when testing AES-128 for aes128_enc @ test %d ...\r\n", i);
			goto err;
		}
	}
	printf("[+] AES-128 keysched: %lld cycles, encryption %lld cycles\r\n", t_total_keysched / (uint64_t)NUM_TESTS_CYCLES, t_total_enc / (uint64_t)NUM_TESTS_CYCLES);
#endif

#if !defined(NO_AES256_TESTS)
	t_total_enc = t_total_keysched = 0;
	// ==== Test cycles counting: AES-256
	for(i = 0; i < NUM_TESTS_CYCLES; i++){
		for(j = 0; j < 32; j++){
			key[j] = rand();
			pt[j] = rand();
		}
		t0 = hal_get_cycles();
		ret = aes256_setkey_enc(&ctx_aes256, key);
		t1 = hal_get_cycles();
		t_total_keysched += (t1 - t0);
		if(ret){
			printf("[-] Error when testing AES-256 for aes256_setkey_enc @ test %d ...\r\n", i);
			goto err;
		}
		t0 = hal_get_cycles();
		ret = aes256_enc(&ctx_aes256, pt, ct);
		t1 = hal_get_cycles();
		t_total_enc += (t1 - t0);
		if(ret){
			printf("[-] Error when testing AES-256 for aes256_enc @ test %d ...\r\n", i);
			goto err;
		}
	}
	printf("[+] AES-256 keysched: %lld cycles, encryption %lld cycles\r\n", t_total_keysched / (uint64_t)NUM_TESTS_CYCLES, t_total_enc / (uint64_t)NUM_TESTS_CYCLES);
#endif
#if !defined(NO_RIJNDAEL256_TESTS)
	t_total_enc = t_total_keysched = 0;
	// ==== Test cycles counting: RIJNDAEL-256
	for(i = 0; i < NUM_TESTS_CYCLES; i++){
		for(j = 0; j < 32; j++){
			key[j] = rand();
			pt[j] = rand();
		}
		t0 = hal_get_cycles();
		ret = rijndael256_setkey_enc(&ctx_rijndael256, key);
		t1 = hal_get_cycles();
		t_total_keysched += (t1 - t0);
		if(ret){
			printf("[-] Error when testing RIJNDAEL-256 for rijndael256_setkey_enc @ test %d ...\r\n", i);
			goto err;
		}
		t0 = hal_get_cycles();
		ret = rijndael256_enc(&ctx_rijndael256, pt, ct);
		t1 = hal_get_cycles();
		t_total_enc += (t1 - t0);
		if(ret){
			printf("[-] Error when testing RIJNDAEL-256 for rijndael256_enc @ test %d ...\r\n", i);
			goto err;
		}
	}
	printf("[+] RIJNDAEL-256 keysched: %lld cycles, encryption %lld cycles\r\n", t_total_keysched / (uint64_t)NUM_TESTS_CYCLES, t_total_enc / (uint64_t)NUM_TESTS_CYCLES);
#endif

	uint64_t t0_xx, t1_xx, t_total_aes128_xx2 = 0, t_total_aes128_xx4 = 0, t_total_aes128_xx8 = 0, t_total_aes256_xx2 = 0, t_total_aes256_xx4 = 0, t_total_aes256_xx8 = 0, t_total_rijndael256_xx2 = 0, t_total_rijndael256_xx4 = 0, t_total_rijndael256_xx8 = 0;
#if !defined(NO_AES128_TESTS)
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 4; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 2; j++){
			aes128_setkey_enc(&ctx_x8_aes128[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		ret = aes128_enc_x2(&ctx_x8_aes128[0], &ctx_x8_aes128[1], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
		t1_xx = hal_get_cycles();
		t_total_aes128_xx2 += (t1_xx - t0_xx);
	}
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 4; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 4; j++){
			aes128_setkey_enc(&ctx_x8_aes128[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		aes128_enc_x4(&ctx_x8_aes128[0], &ctx_x8_aes128[1], &ctx_x8_aes128[2], &ctx_x8_aes128[3], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
		t1_xx = hal_get_cycles();
		t_total_aes128_xx4 += (t1_xx - t0_xx);
	}
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 8; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 8; j++){
			aes128_setkey_enc(&ctx_x8_aes128[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		aes128_enc_x8(&ctx_x8_aes128[0], &ctx_x8_aes128[1], &ctx_x8_aes128[2], &ctx_x8_aes128[3], &ctx_x8_aes128[4], &ctx_x8_aes128[5], &ctx_x8_aes128[6], &ctx_x8_aes128[7], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
		t1_xx = hal_get_cycles();
		t_total_aes128_xx8 += (t1_xx - t0_xx);
	}
#endif
#if !defined(NO_AES256_TESTS)
	/**/
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 4; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 2; j++){
			aes256_setkey_enc(&ctx_x8_aes256[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		aes256_enc_x2(&ctx_x8_aes256[0], &ctx_x8_aes256[1], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
		t1_xx = hal_get_cycles();
		t_total_aes256_xx2 += (t1_xx - t0_xx);
	}
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 4; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 4; j++){
			aes256_setkey_enc(&ctx_x8_aes256[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		aes256_enc_x4(&ctx_x8_aes256[0], &ctx_x8_aes256[1], &ctx_x8_aes256[2], &ctx_x8_aes256[3], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
		t1_xx = hal_get_cycles();
		t_total_aes256_xx4 += (t1_xx - t0_xx);
	}
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 8; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 8; j++){
			aes256_setkey_enc(&ctx_x8_aes256[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		aes256_enc_x8(&ctx_x8_aes256[0], &ctx_x8_aes256[1], &ctx_x8_aes256[2], &ctx_x8_aes256[3], &ctx_x8_aes256[4], &ctx_x8_aes256[5], &ctx_x8_aes256[6], &ctx_x8_aes256[7], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
		t1_xx = hal_get_cycles();
		t_total_aes256_xx8 += (t1_xx - t0_xx);
	}
#endif
#if !defined(NO_RIJNDAEL256_TESTS)
	/**/
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 8; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 2; j++){
			rijndael256_setkey_enc(&ctx_x8_rijndael256[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		rijndael256_enc_x2(&ctx_x8_rijndael256[0], &ctx_x8_rijndael256[1], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
		t1_xx = hal_get_cycles();
		t_total_rijndael256_xx2 += (t1_xx - t0_xx);
	}
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 8; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 8; j++){
			rijndael256_setkey_enc(&ctx_x8_rijndael256[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		rijndael256_enc_x4(&ctx_x8_rijndael256[0], &ctx_x8_rijndael256[1], &ctx_x8_rijndael256[2], &ctx_x8_rijndael256[3], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);	
		t1_xx = hal_get_cycles();
		t_total_rijndael256_xx4 += (t1_xx - t0_xx);
	}
	for(k = 0; k < NUM_TESTS_CYCLES; k++){
		for(i = 0; i < 32; i++){
			for(j = 0; j < 8; j++){
				pt_x8[j][i] = rand();
			}
		}
		for(j = 0; j < 8; j++){
			rijndael256_setkey_enc(&ctx_x8_rijndael256[j], (uint8_t*)&key_x8[j]);
		}
		t0_xx = hal_get_cycles();
		rijndael256_enc_x8(&ctx_x8_rijndael256[0], &ctx_x8_rijndael256[1], &ctx_x8_rijndael256[2], &ctx_x8_rijndael256[3], &ctx_x8_rijndael256[4], &ctx_x8_rijndael256[5], &ctx_x8_rijndael256[6], &ctx_x8_rijndael256[7], (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
		t1_xx = hal_get_cycles();
		t_total_rijndael256_xx8 += (t1_xx - t0_xx);
	}
#endif
	printf("[+] AES-128 X2 encyption performance: %lld cycles\r\n", t_total_aes128_xx2 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] AES-128 X4 encyption performance: %lld cycles\r\n", t_total_aes128_xx4 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] AES-128 X8 encyption performance: %lld cycles\r\n", t_total_aes128_xx8 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] AES-256 X2 encyption performance: %lld cycles\r\n", t_total_aes256_xx2 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] AES-256 X4 encyption performance: %lld cycles\r\n", t_total_aes256_xx4 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] AES-256 X8 encyption performance: %lld cycles\r\n", t_total_aes256_xx8 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] RIJNDAEL-256 X2 encyption performance: %lld cycles\r\n", t_total_rijndael256_xx2 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] RIJNDAEL-256 X4 encyption performance: %lld cycles\r\n", t_total_rijndael256_xx4 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] RIJNDAEL-256 X8 encyption performance: %lld cycles\r\n", t_total_rijndael256_xx8 / (uint64_t)NUM_TESTS_CYCLES);

	// Variants X2X2, X4X4 and X8X8
        uint64_t t_total_aes128_keysched_xx2 = 0, t_total_aes128_keysched_xx4 = 0, t_total_aes128_keysched_xx8 = 0;
        t_total_aes128_xx2 = 0, t_total_aes128_xx4 = 0, t_total_aes128_xx8 = 0, t_total_aes256_xx2 = 0, t_total_aes256_xx4 = 0, t_total_aes256_xx8 = 0, t_total_rijndael256_xx2 = 0, t_total_rijndael256_xx4 = 0, t_total_rijndael256_xx8 = 0;
#if !defined(NO_AES128_TESTS)
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 4; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                aes128_setkey_enc_x2(&ctx_x2_x2_aes128, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1]);
                t1_xx = hal_get_cycles();
                t_total_aes128_keysched_xx2 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                aes128_enc_x2_x2(&ctx_x2_x2_aes128, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
                t1_xx = hal_get_cycles();
                t_total_aes128_xx2 += (t1_xx - t0_xx);
        }
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 4; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                aes128_setkey_enc_x4(&ctx_x4_x4_aes128, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3]);
                t1_xx = hal_get_cycles();
                t_total_aes128_keysched_xx4 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                aes128_enc_x4_x4(&ctx_x4_x4_aes128, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);
                t1_xx = hal_get_cycles();
                t_total_aes128_xx4 += (t1_xx - t0_xx);
        }
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 8; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                aes128_setkey_enc_x8(&ctx_x8_x8_aes128, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3], (uint8_t*)&key_x8[4], (uint8_t*)&key_x8[5], (uint8_t*)&key_x8[6], (uint8_t*)&key_x8[7]);
                t1_xx = hal_get_cycles();
                t_total_aes128_keysched_xx8 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                aes128_enc_x8_x8(&ctx_x8_x8_aes128, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
                t1_xx = hal_get_cycles();
                t_total_aes128_xx8 += (t1_xx - t0_xx);
        }
#endif
	//
        uint64_t t_total_aes256_keysched_xx2 = 0, t_total_aes256_keysched_xx4 = 0, t_total_aes256_keysched_xx8 = 0;
        t_total_aes256_xx2 = 0, t_total_aes256_xx4 = 0, t_total_aes256_xx8 = 0, t_total_aes256_xx2 = 0, t_total_aes256_xx4 = 0, t_total_aes256_xx8 = 0, t_total_rijndael256_xx2 = 0, t_total_rijndael256_xx4 = 0, t_total_rijndael256_xx8 = 0;
#if !defined(NO_AES256_TESTS)
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 4; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                aes256_setkey_enc_x2(&ctx_x2_x2_aes256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1]);
                t1_xx = hal_get_cycles();
                t_total_aes256_keysched_xx2 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                aes256_enc_x2_x2(&ctx_x2_x2_aes256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
                t1_xx = hal_get_cycles();
                t_total_aes256_xx2 += (t1_xx - t0_xx);
        }
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 4; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                aes256_setkey_enc_x4(&ctx_x4_x4_aes256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3]);
                t1_xx = hal_get_cycles();
                t_total_aes256_keysched_xx4 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                aes256_enc_x4_x4(&ctx_x4_x4_aes256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);
                t1_xx = hal_get_cycles();
                t_total_aes256_xx4 += (t1_xx - t0_xx);
        }
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 8; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                aes256_setkey_enc_x8(&ctx_x8_x8_aes256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3], (uint8_t*)&key_x8[4], (uint8_t*)&key_x8[5], (uint8_t*)&key_x8[6], (uint8_t*)&key_x8[7]);
                t1_xx = hal_get_cycles();
                t_total_aes256_keysched_xx8 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                aes256_enc_x8_x8(&ctx_x8_x8_aes256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
                t1_xx = hal_get_cycles();
                t_total_aes256_xx8 += (t1_xx - t0_xx);
        }
#endif
	//
        uint64_t t_total_rijndael256_keysched_xx2 = 0, t_total_rijndael256_keysched_xx4 = 0, t_total_rijndael256_keysched_xx8 = 0;
        t_total_rijndael256_xx2 = 0, t_total_rijndael256_xx4 = 0, t_total_rijndael256_xx8 = 0, t_total_rijndael256_xx2 = 0, t_total_rijndael256_xx4 = 0, t_total_rijndael256_xx8 = 0, t_total_rijndael256_xx2 = 0, t_total_rijndael256_xx4 = 0, t_total_rijndael256_xx8 = 0;
#if !defined(NO_RIJNDAEL256_TESTS)
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 4; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                rijndael256_setkey_enc_x2(&ctx_x2_x2_rijndael256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1]);
                t1_xx = hal_get_cycles();
                t_total_rijndael256_keysched_xx2 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                rijndael256_enc_x2_x2(&ctx_x2_x2_rijndael256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1]);
                t1_xx = hal_get_cycles();
                t_total_rijndael256_xx2 += (t1_xx - t0_xx);
        }
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 4; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                rijndael256_setkey_enc_x4(&ctx_x4_x4_rijndael256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3]);
                t1_xx = hal_get_cycles();
                t_total_rijndael256_keysched_xx4 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                rijndael256_enc_x4_x4(&ctx_x4_x4_rijndael256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3]);
                t1_xx = hal_get_cycles();
                t_total_rijndael256_xx4 += (t1_xx - t0_xx);
        }
        for(k = 0; k < NUM_TESTS_CYCLES; k++){
                for(i = 0; i < 32; i++){
                        for(j = 0; j < 8; j++){
                                pt_x8[j][i] = rand();
                        }
                }
                t0_xx = hal_get_cycles();
                rijndael256_setkey_enc_x8(&ctx_x8_x8_rijndael256, (uint8_t*)&key_x8[0], (uint8_t*)&key_x8[1], (uint8_t*)&key_x8[2], (uint8_t*)&key_x8[3], (uint8_t*)&key_x8[4], (uint8_t*)&key_x8[5], (uint8_t*)&key_x8[6], (uint8_t*)&key_x8[7]);
                t1_xx = hal_get_cycles();
                t_total_rijndael256_keysched_xx8 += (t1_xx - t0_xx);
                t0_xx = hal_get_cycles();
                rijndael256_enc_x8_x8(&ctx_x8_x8_rijndael256, (uint8_t*)&pt_x8[0], (uint8_t*)&pt_x8[1], (uint8_t*)&pt_x8[2], (uint8_t*)&pt_x8[3], (uint8_t*)&pt_x8[4], (uint8_t*)&pt_x8[5], (uint8_t*)&pt_x8[6], (uint8_t*)&pt_x8[7], (uint8_t*)&ct_x8[0], (uint8_t*)ct_x8[1], (uint8_t*)&ct_x8[2], (uint8_t*)ct_x8[3], (uint8_t*)&ct_x8[4], (uint8_t*)ct_x8[5], (uint8_t*)&ct_x8[6], (uint8_t*)ct_x8[7]);
                t1_xx = hal_get_cycles();
                t_total_rijndael256_xx8 += (t1_xx - t0_xx);
        }
#endif
	printf("================\r\n");
 	printf("[+] AES-128 X2 X2 key sched performance: %lld cycles\r\n", t_total_aes128_keysched_xx2 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] AES-128 X2 X2 encyption performance: %lld cycles\r\n", t_total_aes128_xx2 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] AES-128 X4 X4 key sched performance: %lld cycles\r\n", t_total_aes128_keysched_xx4 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] AES-128 X4 X4 encyption performance: %lld cycles\r\n", t_total_aes128_xx4 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] AES-128 X8 X8 key sched performance: %lld cycles\r\n", t_total_aes128_keysched_xx8 / (uint64_t)NUM_TESTS_CYCLES);

 	printf("[+] AES-256 X2 X2 key sched performance: %lld cycles\r\n", t_total_aes256_keysched_xx2 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] AES-256 X2 X2 encyption performance: %lld cycles\r\n", t_total_aes256_xx2 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] AES-256 X4 X4 key sched performance: %lld cycles\r\n", t_total_aes256_keysched_xx4 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] AES-256 X4 X4 encyption performance: %lld cycles\r\n", t_total_aes256_xx4 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] AES-256 X8 X8 key sched performance: %lld cycles\r\n", t_total_aes256_keysched_xx8 / (uint64_t)NUM_TESTS_CYCLES);

 	printf("[+] RIJNDAEL-256 X2 X2 key sched performance: %lld cycles\r\n", t_total_rijndael256_keysched_xx2 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] RIJNDAEL-256 X2 X2 encyption performance: %lld cycles\r\n", t_total_rijndael256_xx2 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] RIJNDAEL-256 X4 X4 key sched performance: %lld cycles\r\n", t_total_rijndael256_keysched_xx4 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] RIJNDAEL-256 X4 X4 encyption performance: %lld cycles\r\n", t_total_rijndael256_xx4 / (uint64_t)NUM_TESTS_CYCLES);
 	printf("[+] RIJNDAEL-256 X8 X8 key sched performance: %lld cycles\r\n", t_total_rijndael256_keysched_xx8 / (uint64_t)NUM_TESTS_CYCLES);
	printf("[+] RIJNDAEL-256 X8 X8 encyption performance: %lld cycles\r\n", t_total_rijndael256_xx8 / (uint64_t)NUM_TESTS_CYCLES);

	return 0;
err:
	return -1;
}

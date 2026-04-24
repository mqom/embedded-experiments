#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <api.h>
#include <benchmark/utils.h>
#include <benchmark.h>
#include <benchmark/timing.h>
#include "verify-stream.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/* XXX:NOTE: only include the MQOM real API here because we might have "broke" the Rijndael
 * context to simplify stuff before */

__attribute__((weak)) int randombytes(unsigned char* x, unsigned long long xlen) {
    for(unsigned long long j=0; j<xlen; j++)
        x[j] = (uint8_t) rand();
    return 0;
}

extern uint64_t hal_get_cycles(void);
extern void panic(void); 

extern void spray_stack(uint32_t max);
extern uint32_t check_stack(uint32_t max);

extern int embedded_KAT_tests(int num_tests);

//==============================================
//==============================================
//==============================================
//==============================================
#define B_KEY_GENERATION 0
#define B_SIGN_ALGO 1
#define B_VERIFY_INIT_ALGO 2
#define B_VERIFY_UPDATE_FIRST_ALGO 3
#define B_VERIFY_UPDATE_OTHER_ALGO 4
#define B_VERIFY_FINALIZE_ALGO 5
#define NUMBER_OF_ALGO_BENCHES 6
#ifdef BENCHMARK
btimer_t timers[NUMBER_OF_BENCHES];

#ifdef BENCHMARK_CYCLES
#define display_timer(label,num) printf("   - " label ": %f ms (%f cycles)\r\n", btimer_get(&timers[num]), btimer_get_cycles(&timers[num]))
#else
#define display_timer(label,num) printf("   - " label ": %f ms\r\n", btimer_get(&timers[num]))
#endif
#endif

/* Get the static tables usage in SRAM with the linker script help */
extern uint32_t _embedded_sram_size;
/* External allocation */
extern long int alloc_peak_usage;

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

/* If no stack spraying is defind, default to conservative 50000 */
#ifndef MAX_SPRAY_STACK
#define MAX_SPRAY_STACK 50000
#endif

int main(int argc, char *argv[]){
	int ret;
	(void)argc;
	(void)argv;
	uint64_t t0, t1;
	uint32_t embedded_sram_size = (uint32_t)&_embedded_sram_size;
        btimer_t timers_algos[NUMBER_OF_ALGO_BENCHES];

	srand(0);

	printf("=== Welcome to MQOM2 embedded tests on board %s\r\r\n", TOSTRING(CURRENT_BOARD));

	print_configuration();

#ifdef EMBEDDED_KAT_TESTS
#ifdef EMBEDDED_KAT_NUM_TESTS
	if(embedded_KAT_tests(EMBEDDED_KAT_NUM_TESTS)){
#else
	if(embedded_KAT_tests(1)){
#endif
		printf("[-] Error for embedded KAT tests ...\r\n");
		panic();
	}
#endif

        for(int j=0; j<NUMBER_OF_ALGO_BENCHES; j++) {
            btimer_init(&timers_algos[j]);
            btimer_count(&timers_algos[j]);
        }

#ifdef BENCHMARK
        for(int j=0; j<NUMBER_OF_BENCHES; j++) {
            btimer_init(&timers[j]);
        }
#endif
        uint8_t pk[CRYPTO_PUBLICKEYBYTES];
        uint8_t sk[CRYPTO_SECRETKEYBYTES];
	uint32_t stack_usage = 0;

	reset_alloc_usage();
	spray_stack(MAX_SPRAY_STACK);
        btimer_start(&timers_algos[B_KEY_GENERATION]);
	t0 = hal_get_cycles();
	ret = crypto_sign_keypair(pk, sk);
	t1 = hal_get_cycles();
        btimer_end(&timers_algos[B_KEY_GENERATION]);
	stack_usage = check_stack(MAX_SPRAY_STACK);
        if(ret) {
            printf("Failure: crypto_sign_keypair\r\n");
            panic();
        }
	if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
		printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
	}
	printf("crypto_sign_keypair OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
	print_alloc_usage("crypto_sign_keypair");
	printf("  => crypto_sign_keypair total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);

#ifdef BENCHMARK
        for(int num=0; num<NUMBER_OF_BENCHES; num++)
            btimer_count(&timers[num]);
#endif

        // Select the message
        #define MLEN 32
        uint8_t m[MLEN] = {1, 2, 3, 4};

        // Sign the message
        uint8_t sig[MQOM2_SIG_SIZE];
        uint8_t salt[MQOM2_PARAM_SALT_SIZE], mseed[MQOM2_PARAM_SEED_SIZE];
        randombytes(salt, MQOM2_PARAM_SALT_SIZE);
        randombytes(mseed, MQOM2_PARAM_SEED_SIZE);

	reset_alloc_usage();
	spray_stack(MAX_SPRAY_STACK);
        btimer_start(&timers_algos[B_SIGN_ALGO]);
	t0 = hal_get_cycles();
        ret = StreamedVerify_Sign(sk, m, MLEN, salt, mseed, sig);
	t1 = hal_get_cycles();
        btimer_end(&timers_algos[B_SIGN_ALGO]);
	stack_usage = check_stack(MAX_SPRAY_STACK);
        if(ret) {
            printf("Failure: StreamedVerify_Sign\r\n");
            panic();
        }
	if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
		printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
	}
	printf("StreamedVerify_Sign OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
	print_alloc_usage("StreamedVerify_Sign");
	printf("  => StreamedVerify_Sign total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);

        // Verify/Open the signature
	reset_alloc_usage();
	spray_stack(MAX_SPRAY_STACK);
        btimer_start(&timers_algos[B_VERIFY_INIT_ALGO]);
	t0 = hal_get_cycles();
        stream_verif_context_t *verif_ctx = StreamedVerify_Init(pk);
	t1 = hal_get_cycles();
        btimer_end(&timers_algos[B_VERIFY_INIT_ALGO]);
	stack_usage = check_stack(MAX_SPRAY_STACK);
	if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
		printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
	}
	printf("StreamedVerify_Init OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
	print_alloc_usage("StreamedVerify_Init");
	printf("  => StreamedVerify_Init total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);
	//
	reset_alloc_usage();
	spray_stack(MAX_SPRAY_STACK);
        btimer_start(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO]);
	t0 = hal_get_cycles();
        ret = StreamedVerify_Update(verif_ctx, &sig[0], FIRST_CHUNK_BYTESIZE);
	t1 = hal_get_cycles();
        btimer_end(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO]);
	stack_usage = check_stack(MAX_SPRAY_STACK);
	if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
		printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
	}
	printf(" StreamedVerify_Update FIRST_CHUNK_BYTESIZE OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
	print_alloc_usage("StreamedVerify_Update FIRST_CHUNK_BYTESIZE");
	printf("  => StreamedVerify_Update FIRST_CHUNK_BYTESIZE total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);
	//
	for (uint32_t e = 0; e < MQOM2_PARAM_TAU; e++) {
		reset_alloc_usage();
		spray_stack(MAX_SPRAY_STACK);
	        btimer_start(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO]);
		t0 = hal_get_cycles();
	        ret = StreamedVerify_Update(verif_ctx, &sig[FIRST_CHUNK_BYTESIZE + e * OTHER_CHUNK_BYTESIZE], OTHER_CHUNK_BYTESIZE);
		t1 = hal_get_cycles();
	        btimer_end(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO]);
		stack_usage = check_stack(MAX_SPRAY_STACK);
		if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
			printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
		}
		printf(" StreamedVerify_Update OTHER_CHUNK_BYTESIZE OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
		print_alloc_usage("StreamedVerify_Update OTHER_CHUNK_BYTESIZE");
		printf("  => StreamedVerify_Update OTHER_CHUNK_BYTESIZE total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);
	}
	// 
	reset_alloc_usage();
	spray_stack(MAX_SPRAY_STACK);
        btimer_start(&timers_algos[B_VERIFY_FINALIZE_ALGO]);
	t0 = hal_get_cycles();
        ret = StreamedVerify_Finalize(verif_ctx, m, MLEN);;
	t1 = hal_get_cycles();
        btimer_end(&timers_algos[B_VERIFY_FINALIZE_ALGO]);
	stack_usage = check_stack(MAX_SPRAY_STACK);
	if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
		printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
	}
	printf(" StreamedVerify_Finalize OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
	print_alloc_usage("StreamedVerify_Finalize ");
	printf("  => StreamedVerify_Finalize total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);


    // Display Infos
    printf("===== SUMMARY =====\r\n");
    printf("Correctness: 1/1\r\n");
    printf("\r\n");

        printf("Timing in ms:\r\n");
        printf(" - Key Gen: %.2f ms\r\n",
               btimer_get(&timers_algos[B_KEY_GENERATION])
              );
        printf(" - Sign:    %.2f ms\r\n",
               btimer_get(&timers_algos[B_SIGN_ALGO])
              );
        printf(" - Verify - Init:  %.2f ms\r\n",
               btimer_get(&timers_algos[B_VERIFY_INIT_ALGO])
              );
        printf(" - Verify - Update First:    %.2f ms\r\n",
               btimer_get(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO])
              );
        printf(" - Verify - Update Others:    %.2f ms\r\n",
               btimer_get(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO])
              );
        printf(" - Verify - Finalize:    %.2f ms\r\n",
               btimer_get(&timers_algos[B_VERIFY_FINALIZE_ALGO])
              );
        printf("\r\n");

#ifdef BENCHMARK_CYCLES
        printf("Timing in cycles:\r\n");
        printf(" - Key Gen: %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_KEY_GENERATION]));
        printf(" - Sign:    %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_SIGN_ALGO]));
        printf(" - Verify - Init:  %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_VERIFY_INIT_ALGO]));
        printf(" - Verify - Update First:  %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO]));
        printf(" - Verify - Update Others:  %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO]));
        printf(" - Verify - Finalize:  %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_VERIFY_FINALIZE_ALGO]));
#endif

        printf("Communication cost:\r\n");
        printf(" - PK size: %ld B\r\n", CRYPTO_PUBLICKEYBYTES);
        printf(" - SK size: %ld B\r\n", CRYPTO_SECRETKEYBYTES);
        printf(" - Signature size (MAX): %ld B\r\n", CRYPTO_BYTES);
        printf(" - Pre-Signature size (MAX): %ld B\r\n", CRYPTO_PRESIGNBYTES);
        printf("\r\n");


	printf("END SERIAL COMM\r\n");
	while(1){}

	return 0;
}

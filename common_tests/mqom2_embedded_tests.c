#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <api.h>
#include <benchmark/utils.h>
#include <benchmark.h>
#include <benchmark/timing.h>

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
#define B_VERIFY_ALGO 2
#define NUMBER_OF_ALGO_BENCHES 3
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

static size_t total_allocated = 0;
static size_t max_allocated = 0;
static void *max_ptr = NULL;

void *__real_malloc(size_t);
void *__wrap_malloc(size_t size) {
    void *p = __real_malloc(size);
    if (p) {
        total_allocated += size;
        if (total_allocated > max_allocated)
            max_allocated = total_allocated;
        if ((uint32_t)p + size > (uint32_t)max_ptr)
            max_ptr = (void*)((uint32_t)p + size);
    } else {
        printf("[!] malloc(%lu) FAILED! total=%lu\r\n", 
               (uint32_t)size, (uint32_t)total_allocated);
    }
    return p;
}

void __real_free(void *);
void __wrap_free(void *p) {
    // Can't track exact size freed without extra bookkeeping
    __real_free(p);
}

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
        uint8_t m2[MLEN] = {0};
        unsigned long long m2len = 0;

        // Sign the message
        uint8_t sm[MLEN+CRYPTO_BYTES];
        unsigned long long smlen = 0;
	reset_alloc_usage();
	spray_stack(MAX_SPRAY_STACK);
        btimer_start(&timers_algos[B_SIGN_ALGO]);
	t0 = hal_get_cycles();
        ret = crypto_sign(sm, &smlen, m, MLEN, sk);
	t1 = hal_get_cycles();
        btimer_end(&timers_algos[B_SIGN_ALGO]);
	stack_usage = check_stack(MAX_SPRAY_STACK);
        if(ret) {
            printf("Failure: crypto_sign\r\n");
            panic();
        }
	if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
		printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
	}
	printf("crypto_sign OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
	print_alloc_usage("crypto_sign");
	printf("  => crypto_sign total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);
        uint8_t *_nonce = &sm[smlen-4];
	uint32_t nonce = ((uint32_t)_nonce[0]) | ((uint32_t)_nonce[1] << 8) | ((uint32_t)_nonce[2] << 16) | ((uint32_t)_nonce[3] << 24);
	// Adjust the signature performance depending on the current PoW (average is ~11.8 KiloCycles per iteration)
	int64_t delta_pow = ((int64_t)(1 << MQOM2_PARAM_W) - (int64_t)nonce) * 11800;
	printf("  => crypto_sign PoW adjusted cycles = %lld (PoW is %ld iterations)\r\n", (int64_t)(t1-t0) + delta_pow, nonce);

	reset_alloc_usage();
	spray_stack(MAX_SPRAY_STACK);
        btimer_start(&timers_algos[B_VERIFY_ALGO]);
	t0 = hal_get_cycles();
	ret = crypto_sign_open(m2, &m2len, sm, smlen, pk);
	t1 = hal_get_cycles();
        btimer_end(&timers_algos[B_VERIFY_ALGO]);
	stack_usage = check_stack(MAX_SPRAY_STACK);
        if(ret) {
            printf("Failure: crypto_sign_open\r\n");
            panic();
        }
	if(stack_usage >= (uint32_t)MAX_SPRAY_STACK){
		printf("!!!!!! WARNING: stack usage %ld exceeds MAX_SPRAY_STACK = %ld, meaning measurement is lower bound ...\r\n", stack_usage, (uint32_t)MAX_SPRAY_STACK);
	}
	printf("crypto_sign_open OK! Took %lld cycles, stack usage = %ld bytes, global tables usage = %ld bytes, sum = %ld bytes\r\r\n", t1-t0, stack_usage, embedded_sram_size, stack_usage + embedded_sram_size);
	print_alloc_usage("crypto_sign_open");
	printf("  => crypto_sign_open total mem usage: %ld\r\n", stack_usage + embedded_sram_size + alloc_peak_usage);

        if(m2len != MLEN) {
            printf("Failure: message size does not match\r\r\n");
            panic();
        }
        for(int h=0; h<MLEN; h++){
            if(m[h] != m2[h]) {
                printf("Failure: message does not match (char %d)\r\n", h);
            	panic();
            }
        }
        // Display the 'nonce' to have the number of iterations of the PoW (extract it from the signature)
	printf("[+] PoW number of iterations: %ld\r\n", nonce);

    // Display Infos
    printf("===== SUMMARY =====\r\n");
    printf("Correctness: 1/1\r\n");
    printf("\r\n");

    printf("Timing in ms:\r\n");
    printf(" - Key Gen: %.2f ms (std=%.2f)\r\n",
        btimer_get(&timers_algos[B_KEY_GENERATION]),
        0.00
    );
    printf(" - Sign:    %.2f ms (std=%.2f)\r\n",
        btimer_get(&timers_algos[B_SIGN_ALGO]),
        0.00
    );
    printf(" - Verify:  %.2f ms (std=%.2f)\r\n",
        btimer_get(&timers_algos[B_VERIFY_ALGO]),
        0.00
    );
    printf("\r\n");

    printf("Communication cost:\r\n");
    printf(" - PK size: %ld B\r\n", CRYPTO_PUBLICKEYBYTES);
    printf(" - SK size: %ld B\r\n", CRYPTO_SECRETKEYBYTES);
    printf(" - Signature size (MAX): %ld B\r\n", CRYPTO_BYTES);
    printf(" - Signature size: %lld B (std=%.0f)\r\n", smlen, 0.00);
    printf("\r\n");

    #ifdef BENCHMARK_CYCLES
    printf("Timing in cycles:\r\n");
    printf(" - Key Gen: %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_KEY_GENERATION]));
    printf(" - Sign:    %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_SIGN_ALGO]));
    printf(" - Verify:  %.2f cycles\r\n", btimer_get_cycles(&timers_algos[B_VERIFY_ALGO]));
    printf("\r\n");
    #endif

    #ifdef BENCHMARK
    printf("\r\n===== DETAILED BENCHMARK =====\r\n");
    printf(" - Signing\r\n");
    display_timer("BLC.Commit", BS_BLC_COMMIT);
    display_timer("[BLC.Commit] Expand Trees", BS_BLC_EXPAND_TREE);
    display_timer("[BLC.Commit] Seed Commit", BS_BLC_SEED_COMMIT);
    display_timer("[BLC.Commit] PRG", BS_BLC_PRG);
    display_timer("[BLC.Commit] XOF", BS_BLC_XOF);
    display_timer("[BLC.Commit] Arithm", BS_BLC_ARITH);
    display_timer("PIOP.Compute", BS_PIOP_COMPUTE);
    display_timer("[PIOP.Compute] ExpandMQ", BS_PIOP_EXPAND_MQ);
    display_timer("[PIOP.Compute] Expand Batching Mat", BS_PIOP_EXPAND_BATCHING_MAT);
    display_timer("[PIOP.Compute] Matrix Mul Ext", BS_PIOP_MAT_MUL_EXT);
    display_timer("[PIOP.Compute] Compute t1", BS_PIOP_COMPUTE_T1);
    display_timer("[PIOP.Compute] Compute P_zi", BS_PIOP_COMPUTE_PZI);
    display_timer("[PIOP.Compute] Batch and Mask", BS_PIOP_BATCH_AND_MASK);
    display_timer("Sample Challenge", BS_SAMPLE_CHALLENGE);
    display_timer("BLC.Open", BS_BLC_OPEN);
    printf(" - Others\r\n");
    display_timer("Pin A", B_PIN_A);
    display_timer("Pin B", B_PIN_B);
    display_timer("Pin C", B_PIN_C);
    display_timer("Pin D", B_PIN_D);
    #endif

	printf("END SERIAL COMM\r\n");
	while(1){}

	return 0;
}

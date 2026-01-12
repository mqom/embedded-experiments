#include <stdio.h>
#include <stdlib.h>
#include "piop.h"
#include "keygen.h"
#include "expand_mq.h"
#include "benchmark/timing.h"
#include "api.h"

#include "fields_bitsliced.h"

void transform_vect(const field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)]);
void transform_back_vect(felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)], field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)]);
void bitsliced_vect_mult_hybrid_public(felt_ext_elt_bitsliced_t *res, const field_ext_elt *a, const felt_ext_elt_bitsliced_t* b_bitsliced, uint32_t len);

void transform_composite_vect(const field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)]);
void transform_composite_back_vect(felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)], field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)]);
void bitsliced_composite_vect_mult_hybrid_public(felt_ext_elt_bitsliced_t *res, const field_ext_elt *a, const felt_ext_elt_bitsliced_t* b_bitsliced, uint32_t len);

void transform_cond_vect(const field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)]);
void transform_cond_back_vect(felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)], field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)]);
void bitsliced_cond_vect_mult_hybrid_public(felt_ext_elt_bitsliced_t *res, const field_ext_elt *a, const felt_ext_elt_bitsliced_t* b_bitsliced, uint32_t len);

void transform_composite_cond_vect(const field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)]);
void transform_composite_cond_back_vect(felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)], field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)]);
void bitsliced_composite_cond_vect_mult_hybrid_public(felt_ext_elt_bitsliced_t *res, const field_ext_elt *a, const felt_ext_elt_bitsliced_t* b_bitsliced, uint32_t len);

#define NB_TIMERS 10

__attribute__((weak)) int randombytes(unsigned char* x, unsigned long long xlen) {
    for(unsigned long long j=0; j<xlen; j++)
        x[j] = (uint8_t) rand();
    return 0;
}

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

void print_conf(void)
{
        printf("===== SCHEME CONFIG =====\r\n");
        printf("[API] Algo Name: " CRYPTO_ALGNAME "\r\n");
        printf("[API] Algo Version: " CRYPTO_VERSION "\r\n");
        printf("Non-bitslice fields implementation: %s\r\n", fields_conf);
	printf("=========================\r\n");
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
    uint32_t i, j, e;
    int ret;

    printf("==== Welcome to MAT MULT tests ...\r\n");

    btimer_t timer_matmul[NB_TIMERS];
    for(int k=0; k<NB_TIMERS; k++) {
        btimer_init(&timer_matmul[k]);
    }

#if defined(__unix__) || (defined (__APPLE__) && defined (__MACH__))
    /* On POSIX platforms, initialize our random with time */
    srand((unsigned int) time(NULL));
#endif

    field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
    field_ext_elt t0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
    uint8_t mseed_eq[2 * MQOM2_PARAM_SEED_SIZE];
    ExpandEquations_ctx EEctx;
    field_ext_elt A_hat_row[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
    felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)];
    felt_ext_elt_bitsliced_t t0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)];
    field_ext_elt x0_bis[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
    field_ext_elt t0_bis[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];

    for(uint32_t num_test=0; num_test<1; num_test++) {
        for(int k=0; k<NB_TIMERS; k++) {
            btimer_count(&timer_matmul[k]);
        }

        // Preparing
        randombytes(mseed_eq, 2 * MQOM2_PARAM_SEED_SIZE);
        randombytes((uint8_t*) x0, sizeof(field_ext_elt)*MQOM2_PARAM_TAU*FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N));

        // Matrix multiplication (using default code)
        ret = ExpandEquations_memopt_init(mseed_eq, &EEctx);
        if(ret) {
            printf("Failure ExpandEquations_memopt_init\r\n");
            return 0;
        }
        for(i = 0; i < MQOM2_PARAM_MQ_M/MQOM2_PARAM_MU; i++) {
            for(j = 0; j < MQOM2_PARAM_MQ_N; j++){
                ret = ExpandEquations_memopt_update(&EEctx, A_hat_row);
                if(ret) {
                    printf("Failure ExpandEquations_memopt_update\r\n");
                    return 0;
                }
                /* Compute t0, different for each tau repetition */
                btimer_start(&timer_matmul[0]);
                for(e = 0; e < MQOM2_PARAM_TAU; e++){
                    t0[e][j] = field_ext_vect_mult(A_hat_row, x0[e], j+1); 
                }
                btimer_end(&timer_matmul[0]);
            }
        }
        ExpandEquations_memopt_final(&EEctx);

        // Matrix multiplication (using reference code)
        ret = ExpandEquations_memopt_init(mseed_eq, &EEctx);
        if(ret) {
            printf("Failure ExpandEquations_memopt_init\r\n");
            return 0;
        }
        for(i = 0; i < MQOM2_PARAM_MQ_M/MQOM2_PARAM_MU; i++) {
            for(j = 0; j < MQOM2_PARAM_MQ_N; j++){
                ret = ExpandEquations_memopt_update(&EEctx, A_hat_row);
                if(ret) {
                    printf("Failure ExpandEquations_memopt_update\r\n");
                    return 0;
                }
                /* Compute t0, different for each tau repetition */
                btimer_start(&timer_matmul[1]);
                for(e = 0; e < MQOM2_PARAM_TAU; e++){
#if MQOM2_PARAM_EXT_FIELD == 8
                    t0[e][j] = gf256_vect_mult_ref(A_hat_row, x0[e], j+1);
#else
                    t0[e][j] = gf256to2_vect_mult_ref(A_hat_row, x0[e], j+1);
#endif
                }
                btimer_end(&timer_matmul[1]);
            }
        }
        ExpandEquations_memopt_final(&EEctx);

	// ================================================
        // Bitslice the input vector
        btimer_start(&timer_matmul[2]);
        transform_vect(x0, x0_bitsliced);
        transform_back_vect(x0_bitsliced, x0_bis);
        btimer_end(&timer_matmul[2]);
        if(memcmp(x0, x0_bis, sizeof(x0)) != 0) {
            printf("Error Bitslice transformation\r\n");
            continue;
        }
	// Transform again as transform_composite_back_vect have modified x0_bitsliced
        transform_vect(x0, x0_bitsliced);
        // Matrix multiplication in partial hybrid
        ret = ExpandEquations_memopt_init(mseed_eq, &EEctx);
        if(ret) {
            printf("Failure ExpandEquations_memopt_init\r\n");
            return 0;
        }
        for(i = 0; i < MQOM2_PARAM_MQ_M/MQOM2_PARAM_MU; i++) {
            for(j = 0; j < MQOM2_PARAM_MQ_N; j++){
                ret = ExpandEquations_memopt_update(&EEctx, A_hat_row);
                if(ret) {
                    printf("Failure ExpandEquations_memopt_update\r\n");
                    return 0;
                }
                /* Compute t0, different for each tau repetition */
                btimer_start(&timer_matmul[3]);
                bitsliced_vect_mult_hybrid_public(t0_bitsliced, A_hat_row, x0_bitsliced, j); 
                btimer_end(&timer_matmul[3]);
            }
        }
        ExpandEquations_memopt_final(&EEctx);
        transform_back_vect(t0_bitsliced, t0_bis);
        if(memcmp(t0, t0_bis, sizeof(t0)) != 0) {
            printf("Error Bitslice t0\r\n");
            continue;
        }

	// ================================================
        // Composite bitslice the input vector
        btimer_start(&timer_matmul[4]);
        transform_composite_vect(x0, x0_bitsliced);
        transform_composite_back_vect(x0_bitsliced, x0_bis);
        btimer_end(&timer_matmul[4]);
        if(memcmp(x0, x0_bis, sizeof(x0)) != 0) {
            printf("Error Composite Bitslice transformation\r\n");
            continue;
        }
	// Transform again as transform_composite_back_vect have modified x0_bitsliced
        transform_composite_vect(x0, x0_bitsliced);
        // Matrix multiplication in partial hybrid, using composite field
        ret = ExpandEquations_memopt_init(mseed_eq, &EEctx);
        if(ret) {
            printf("Failure ExpandEquations_memopt_init\r\n");
            return 0;
        }
        for(i = 0; i < MQOM2_PARAM_MQ_M/MQOM2_PARAM_MU; i++) {
            for(j = 0; j < MQOM2_PARAM_MQ_N; j++){
                ret = ExpandEquations_memopt_update(&EEctx, A_hat_row);
                if(ret) {
                    printf("Failure ExpandEquations_memopt_update\r\n");
                    return 0;
                }
                /* Compute t0, different for each tau repetition */
                btimer_start(&timer_matmul[5]);
                bitsliced_composite_vect_mult_hybrid_public(t0_bitsliced, A_hat_row, x0_bitsliced, j); 
                btimer_end(&timer_matmul[5]);
            }
        }
        ExpandEquations_memopt_final(&EEctx);
        transform_composite_back_vect(t0_bitsliced, t0_bis);
        if(memcmp(t0, t0_bis, sizeof(t0)) != 0) {
            printf("Error Composite Bitslice t0\r\n");
            continue;
        }

	// ================================================
        // Bitslice with cond the input vector
        btimer_start(&timer_matmul[6]);
        transform_cond_vect(x0, x0_bitsliced);
        transform_cond_back_vect(x0_bitsliced, x0_bis);
        btimer_end(&timer_matmul[6]);
        if(memcmp(x0, x0_bis, sizeof(x0)) != 0) {
            printf("Error Cond Bitslice transformation\r\n");
            continue;
        }
	// Transform again as transform_composite_back_vect have modified x0_bitsliced
        transform_cond_vect(x0, x0_bitsliced);
        // Matrix multiplication in partial hybrid
        ret = ExpandEquations_memopt_init(mseed_eq, &EEctx);
        if(ret) {
            printf("Failure ExpandEquations_memopt_init\r\n");
            return 0;
        }
        for(i = 0; i < MQOM2_PARAM_MQ_M/MQOM2_PARAM_MU; i++) {
            for(j = 0; j < MQOM2_PARAM_MQ_N; j++){
                ret = ExpandEquations_memopt_update(&EEctx, A_hat_row);
                if(ret) {
                    printf("Failure ExpandEquations_memopt_update\r\n");
                    return 0;
                }
                /* Compute t0, different for each tau repetition */
                btimer_start(&timer_matmul[7]);
                bitsliced_cond_vect_mult_hybrid_public(t0_bitsliced, A_hat_row, x0_bitsliced, j); 
                btimer_end(&timer_matmul[7]);
            }
        }
        ExpandEquations_memopt_final(&EEctx);
        transform_cond_back_vect(t0_bitsliced, t0_bis);
        if(memcmp(t0, t0_bis, sizeof(t0)) != 0) {
            printf("Error Cond Bitslice t0\r\n");
            continue;
        }

	// ================================================
        // Composite cond bitslice the input vector
        btimer_start(&timer_matmul[8]);
        transform_composite_cond_vect(x0, x0_bitsliced);
        transform_composite_cond_back_vect(x0_bitsliced, x0_bis);
        btimer_end(&timer_matmul[8]);
        if(memcmp(x0, x0_bis, sizeof(x0)) != 0) {
            printf("Error Composite Cond Bitslice transformation\r\n");
            continue;
        }
	// Transform again as transform_composite_back_vect have modified x0_bitsliced
        transform_composite_cond_vect(x0, x0_bitsliced);
        // Matrix multiplication in partial hybrid, using composite field
        ret = ExpandEquations_memopt_init(mseed_eq, &EEctx);
        if(ret) {
            printf("Failure ExpandEquations_memopt_init\r\n");
            return 0;
        }
        for(i = 0; i < MQOM2_PARAM_MQ_M/MQOM2_PARAM_MU; i++) {
            for(j = 0; j < MQOM2_PARAM_MQ_N; j++){
                ret = ExpandEquations_memopt_update(&EEctx, A_hat_row);
                if(ret) {
                    printf("Failure ExpandEquations_memopt_update\r\n");
                    return 0;
                }
                /* Compute t0, different for each tau repetition */
                btimer_start(&timer_matmul[9]);
                bitsliced_composite_cond_vect_mult_hybrid_public(t0_bitsliced, A_hat_row, x0_bitsliced, j); 
                btimer_end(&timer_matmul[9]);
            }
        }
        ExpandEquations_memopt_final(&EEctx);
        transform_composite_cond_back_vect(t0_bitsliced, t0_bis);
        if(memcmp(t0, t0_bis, sizeof(t0)) != 0) {
            printf("Error Composite Cond Bitslice t0\r\n");
            continue;
        }

    }
    // To force the compiler to make all the computation
    hexdump("direct) ", (uint8_t*) t0, sizeof(field_ext_elt)*MQOM2_PARAM_TAU*FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N));
    hexdump("bitsli) ", (uint8_t*) t0_bis, sizeof(field_ext_elt)*MQOM2_PARAM_TAU*FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N));


    // Print configuration
    print_conf();
    // Display the benchmark
    printf(" - MatMul(default): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[0]));
    printf(" - MatMul(ref): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[1]));
    printf(" - Transform(bitslice): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[2]));
    printf(" - MatMul(bitslice): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[3]));
    printf(" - Transform(bitslice-composite): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[4]));
    printf(" - MatMul(bitslice-composite): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[5]));
    printf(" - Transform(bitslice-cond): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[6]));
    printf(" - MatMul(bitslice-cond): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[7]));
    printf(" - Transform(bitslice-composite-cond): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[8]));
    printf(" - MatMul(bitslice-composite-cond): %.2f cycles \r\n", btimer_get_cycles(&timer_matmul[9]));

	return 0;
}

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "timing.h"
#include "utils.h"
#include "api.h"
#include "benchmark.h"
#include "verify-stream.h"

#define B_KEY_GENERATION 0
#define B_SIGN_ALGO 1
#define B_VERIFY_INIT_ALGO 2
#define B_VERIFY_UPDATE_FIRST_ALGO 3
#define B_VERIFY_UPDATE_OTHER_ALGO 4
#define B_VERIFY_FINALIZE_ALGO 5
#define NUMBER_OF_ALGO_BENCHES 6

int randombytes(unsigned char* x, unsigned long long xlen) {
	for (unsigned long long j = 0; j < xlen; j++) {
		x[j] = (uint8_t) rand();
	}
	return 0;
}

#ifdef BENCHMARK
btimer_t timers[NUMBER_OF_BENCHES];

#ifdef BENCHMARK_CYCLES
#define display_timer(label,num) printf("   - " label ": %f ms (%f cycles)\n", btimer_get(&timers[num]), btimer_get_cycles(&timers[num]))
#else
#define display_timer(label,num) printf("   - " label ": %f ms\n", btimer_get(&timers[num]))
#endif
#endif

int main(int argc, char *argv[]) {
	srand((unsigned int) time(NULL));

	int nb_tests = get_number_of_tests(argc, argv, 1);
	if (nb_tests < 0) {
		exit(EXIT_FAILURE);
	}

	print_configuration();
	printf("\n");

	btimer_t timers_algos[NUMBER_OF_ALGO_BENCHES];
	double std_timer[NUMBER_OF_ALGO_BENCHES];

	// Initialisation
	double timer_pow2[NUMBER_OF_ALGO_BENCHES];
	for (int j = 0; j < NUMBER_OF_ALGO_BENCHES; j++) {
		btimer_init(&timers_algos[j]);
		timer_pow2[j] = 0;
	}
#ifdef BENCHMARK
	for (int num = 0; num < NUMBER_OF_BENCHES; num++) {
		btimer_init(&timers[num]);
	}
#endif
	double mean_of_sig_size = 0;
	double sig_size_pow2 = 0;

	// Execution
	int score = 0;
	int ret;
	for (int i = 0; i < nb_tests; i++) {
#ifdef BENCHMARK
		for (int num = 0; num < NUMBER_OF_BENCHES; num++) {
			btimer_count(&timers[num]); // For standard signature
			btimer_count(&timers[num]); // For pre-signature
		}
#endif

		// Generate the keys
		uint8_t pk[CRYPTO_PUBLICKEYBYTES];
		uint8_t sk[CRYPTO_SECRETKEYBYTES];
		btimer_start(&timers_algos[B_KEY_GENERATION]);
		ret = crypto_sign_keypair(pk, sk);
		btimer_end(&timers_algos[B_KEY_GENERATION]);
		btimer_count(&timers_algos[B_KEY_GENERATION]);
		timer_pow2[B_KEY_GENERATION] += pow(btimer_diff(&timers_algos[B_KEY_GENERATION]), 2) / nb_tests;
		if (ret) {
			printf("Failure (num %d): crypto_sign_keypair\n", i);
			continue;
		}

		// Select the message
#define MLEN 32
		uint8_t m[MLEN] = {1, 2, 3, 4};

		// Sign the message
		uint8_t sig[MQOM2_SIG_SIZE];
		btimer_start(&timers_algos[B_SIGN_ALGO]);
		uint8_t salt[MQOM2_PARAM_SALT_SIZE], mseed[MQOM2_PARAM_SEED_SIZE];
		randombytes(salt, MQOM2_PARAM_SALT_SIZE);
		randombytes(mseed, MQOM2_PARAM_SEED_SIZE);
		ret = StreamedVerify_Sign(sk, m, MLEN, salt, mseed, sig);
		btimer_end(&timers_algos[B_SIGN_ALGO]);
		btimer_count(&timers_algos[B_SIGN_ALGO]);
		timer_pow2[B_SIGN_ALGO] += pow(btimer_diff(&timers_algos[B_SIGN_ALGO]), 2) / nb_tests;
		// Update statistics
		size_t signature_len = MQOM2_SIG_SIZE;
		mean_of_sig_size += (double) signature_len / nb_tests;
		sig_size_pow2 += pow(signature_len, 2) / nb_tests;
		if (ret) {
			printf("Failure (num %d): StreamedVerify_Sign\n", i);
			continue;
		}

		// Verify/Open the signature
		btimer_start(&timers_algos[B_VERIFY_INIT_ALGO]);
		stream_verif_context_t *verif_ctx = StreamedVerify_Init(pk);
		btimer_end(&timers_algos[B_VERIFY_INIT_ALGO]);
		btimer_count(&timers_algos[B_VERIFY_INIT_ALGO]);
		btimer_start(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO]);
		ret = StreamedVerify_Update(verif_ctx, &sig[0], FIRST_CHUNK_BYTESIZE);
		btimer_end(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO]);
		btimer_count(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO]);
		if (ret) {
			printf("Error (num %d): StreamedVerify_Update (First)\n", i);
		}
		for (uint32_t e = 0; e < MQOM2_PARAM_TAU; e++) {
			btimer_start(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO]);
			ret = StreamedVerify_Update(verif_ctx, &sig[FIRST_CHUNK_BYTESIZE + e * OTHER_CHUNK_BYTESIZE], OTHER_CHUNK_BYTESIZE);
			btimer_end(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO]);
			btimer_count(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO]);
			if (ret) {
				printf("Error (num %d): StreamedVerify_Update (Other)\n", i);
			}
		}
		btimer_start(&timers_algos[B_VERIFY_FINALIZE_ALGO]);
		ret = StreamedVerify_Finalize(verif_ctx, m, MLEN);
		StreamedVerify_Clean(verif_ctx);
		btimer_end(&timers_algos[B_VERIFY_FINALIZE_ALGO]);
		btimer_count(&timers_algos[B_VERIFY_FINALIZE_ALGO]);
		if (ret) {
			printf("Failure (num %d): StreamedVerify\n", i);
			continue;
		}

		score++;
	}

	// Compute some statistics
	std_timer[B_KEY_GENERATION] = sqrt(timer_pow2[B_KEY_GENERATION] - pow(btimer_get(&timers_algos[B_KEY_GENERATION]), 2));
	std_timer[B_SIGN_ALGO] = sqrt(timer_pow2[B_SIGN_ALGO] - pow(btimer_get(&timers_algos[B_SIGN_ALGO]), 2));
	double std_sig_size = sqrt(sig_size_pow2 - pow(mean_of_sig_size, 2));

	// Display Infos
	printf("===== SUMMARY =====\n");
	printf("Correctness: %d/%d\n", score, nb_tests);
	printf("\n");

	print_alloc_usage("keygen+sign+verif");
	printf("\n");

	printf("Timing in ms:\n");
	printf(" - Key Gen: %.2f ms (std=%.2f)\n",
	       btimer_get(&timers_algos[B_KEY_GENERATION]),
	       std_timer[B_KEY_GENERATION]
	      );
	printf(" - Sign:    %.2f ms (std=%.2f)\n",
	       btimer_get(&timers_algos[B_SIGN_ALGO]),
	       std_timer[B_SIGN_ALGO]
	      );
	printf(" - Verify - Init:  %.2f ms\n",
	       btimer_get(&timers_algos[B_VERIFY_INIT_ALGO])
	      );
	printf(" - Verify - Update First:    %.2f ms\n",
	       btimer_get(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO])
	      );
	printf(" - Verify - Update Others:    %.2f ms\n",
	       btimer_get(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO])
	      );
	printf(" - Verify - Finalize:    %.2f ms\n",
	       btimer_get(&timers_algos[B_VERIFY_FINALIZE_ALGO])
	      );
	printf("\n");

#ifdef BENCHMARK_CYCLES
	printf("Timing in cycles:\n");
	printf(" - Key Gen: %.2f cycles\n", btimer_get_cycles(&timers_algos[B_KEY_GENERATION]));
	printf(" - Sign:    %.2f cycles\n", btimer_get_cycles(&timers_algos[B_SIGN_ALGO]));
	printf(" - Verify - Init:  %.2f cycles\n", btimer_get_cycles(&timers_algos[B_VERIFY_INIT_ALGO]));
	printf(" - Verify - Update First:  %.2f cycles\n", btimer_get_cycles(&timers_algos[B_VERIFY_UPDATE_FIRST_ALGO]));
	printf(" - Verify - Update Others:  %.2f cycles\n", btimer_get_cycles(&timers_algos[B_VERIFY_UPDATE_OTHER_ALGO]));
	printf(" - Verify - Finalize:  %.2f cycles\n", btimer_get_cycles(&timers_algos[B_VERIFY_FINALIZE_ALGO]));
#endif

	printf("Communication cost:\n");
	printf(" - PK size: %ld B\n", CRYPTO_PUBLICKEYBYTES);
	printf(" - SK size: %ld B\n", CRYPTO_SECRETKEYBYTES);
	printf(" - Signature size (MAX): %ld B\n", CRYPTO_BYTES);
	printf(" - Signature size: %.0f B (std=%.0f)\n", mean_of_sig_size, std_sig_size);
	printf(" - Pre-Signature size (MAX): %ld B\n", CRYPTO_PRESIGNBYTES);
	printf("\n");

	return 0;
}

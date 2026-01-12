#define __concat(x) #x
#define _concat(file_prefix, x) __concat(file_prefix##x.h)
#define concat(file_prefix, x) _concat(file_prefix, x)

#define _concat2(x, y) x##y
#define concat2(x, y) _concat2(x, y)

#define _concatt(file_prefix, x) __concat(file_prefix##x)
#define concatt(file_prefix, x) _concatt(file_prefix, x)

/* NOTE: we use the preprocessor to include the proper test file */
#include <mqom2_parameters.h>
#include <api.h>
#include concat(KAT_, MQOM2_PARAM_KAT)

#include <rijndael.h>

#include <stdio.h>
#include <stdlib.h>

/* ====================================== */
/* Standalone implementation of the KAT randomness
 * provider */
typedef struct {
    unsigned char   buffer[16];
    int             buffer_pos;
    unsigned long   length_remaining;
    unsigned char   key[32];
    unsigned char   ctr[16];
} AES_XOF_struct;

typedef struct {
    unsigned char   Key[32];
    unsigned char   V[16];
    int             reseed_counter;
} AES256_CTR_DRBG_struct;
static AES256_CTR_DRBG_struct  DRBG_ctx;

#define RNG_SUCCESS      0
#define RNG_BAD_MAXLEN  -1
#define RNG_BAD_OUTBUF  -2
#define RNG_BAD_REQ_LEN -3
#define RNG_INTERNAL_ERROR -4

#if defined(BOARD_HAS_HW_AES) && defined(RIJNDAEL_EXTERNAL) && !defined(NO_HW_AES)
/* In case of HW AES, do not count on MQOM local AES-256, use the HW */
extern void hal_cryp_aes_256_set_key(const uint8_t key[32]);
extern void hal_cryp_aes_256_enc(const uint8_t *pt, uint8_t *ct, uint32_t sz);
static int
AES256_ECB(unsigned char *key, unsigned char *ctr, unsigned char *buffer)
{
	int ret = RNG_SUCCESS;

	hal_cryp_aes_256_set_key(key);
	hal_cryp_aes_256_enc(ctr, buffer, 16);

	return ret;
}
#else
/* Wire AES-256 ECB to our local implementation */
static int
AES256_ECB(unsigned char *key, unsigned char *ctr, unsigned char *buffer)
{
	int ret = RNG_INTERNAL_ERROR;
	rijndael_ctx_aes256 ctx;

	ret = aes256_setkey_enc(&ctx, key);
	if(ret){
		goto err;
	}
	ret = aes256_enc(&ctx, ctr, buffer);
	if(ret){
		goto err;
	}

	ret = RNG_SUCCESS;
err:
	return ret;
}
#endif

static int
AES256_CTR_DRBG_Update(unsigned char *provided_data,
                       unsigned char *Key,
                       unsigned char *V)
{
    unsigned char   temp[48];
    int ret = RNG_INTERNAL_ERROR;

    for (int i=0; i<3; i++) {
        //increment V
        for (int j=15; j>=0; j--) {
            if ( V[j] == 0xff )
                V[j] = 0x00;
            else {
                V[j]++;
                break;
            }
        }

        if(AES256_ECB(Key, V, temp+16*i) != RNG_SUCCESS){
            goto err;
        }
    }
    if ( provided_data != NULL )
        for (int i=0; i<48; i++)
            temp[i] ^= provided_data[i];
    memcpy(Key, temp, 32);
    memcpy(V, temp+32, 16);

    ret = RNG_SUCCESS;
err:
    return ret;
}

static int
randombytes_init(const unsigned char *entropy_input,
                 const unsigned char *personalization_string,
                 int security_strength)
{
    unsigned char   seed_material[48];
    (void) security_strength;
    int ret = RNG_INTERNAL_ERROR;

    memcpy(seed_material, entropy_input, 48);
    if (personalization_string)
        for (int i=0; i<48; i++)
            seed_material[i] ^= personalization_string[i];
    memset(DRBG_ctx.Key, 0x00, 32);
    memset(DRBG_ctx.V, 0x00, 16);
    if(AES256_CTR_DRBG_Update(seed_material, DRBG_ctx.Key, DRBG_ctx.V) != RNG_SUCCESS){
        goto err;
    }
    DRBG_ctx.reseed_counter = 1;

    ret = RNG_SUCCESS;
err:
    return ret;
}

#ifdef EMBEDDED_KAT_TESTS
/* XXX: NOTE: this is conditionally compiled because of the weak symbol
 * for randombytes in main */
int
randombytes(unsigned char *x, unsigned long long xlen)
{
    unsigned char   block[16];
    int             i = 0;
    int ret = RNG_INTERNAL_ERROR;

    while ( xlen > 0 ) {
        //increment V
        for (int j=15; j>=0; j--) {
            if ( DRBG_ctx.V[j] == 0xff )
                DRBG_ctx.V[j] = 0x00;
            else {
                DRBG_ctx.V[j]++;
                break;
            }
        }
        if(AES256_ECB(DRBG_ctx.Key, DRBG_ctx.V, block) != RNG_SUCCESS){
            goto err;
        }
        if ( xlen > 15 ) {
            memcpy(x+i, block, 16);
            i += 16;
            xlen -= 16;
        }
        else {
            memcpy(x+i, block, xlen);
            xlen = 0;
        }
    }
    if(AES256_CTR_DRBG_Update(NULL, DRBG_ctx.Key, DRBG_ctx.V) != RNG_SUCCESS){
        goto err;
    }
    DRBG_ctx.reseed_counter++;

    ret = RNG_SUCCESS;
err:
    return ret;
}
#endif

/* Our embedded tests routine */
int KAT_embedded_test_vectors(int num_tests)
{
	int ret = -1;
	int test_num;
	int max_tests = (int)concat2(MQOM2_PARAM_KAT, _TEST_VECTORS_COUNT);
	num_tests = (num_tests < 0) ? max_tests : num_tests;
	num_tests = (num_tests < max_tests) ? num_tests : max_tests;
	
	for(test_num = 0; test_num < num_tests; test_num++){
		const test_vector_t *test = &concat2(MQOM2_PARAM_KAT, _TEST_VECTORS)[test_num];
		unsigned char pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES], sig[CRYPTO_BYTES];
		unsigned long long siglen = CRYPTO_BYTES;
		/* Set the random seed */
		if(test->seed_len != 48){
			goto err;
		}
		if(randombytes_init(test->seed, NULL, 256) != RNG_SUCCESS){
			goto err;
		}
		/* Generate the keypair */
		ret = crypto_sign_keypair(pk, sk);
		if(ret){
			ret = test_num+1;
			goto err;
		}
		/* Check the keypair */
		if((test->pk_len != CRYPTO_PUBLICKEYBYTES) || (test->sk_len != CRYPTO_SECRETKEYBYTES)){
			ret = test_num+1;
			goto err;	
		}
		if(memcmp(pk, test->pk, CRYPTO_PUBLICKEYBYTES) || memcmp(sk, test->sk, CRYPTO_SECRETKEYBYTES)){

			ret = test_num+1;
			goto err;	
		}
		/* Execute the signature */
		ret = crypto_sign_signature(sig, &siglen, test->msg, test->mlen, test->sk);
		if(ret){
			ret = test_num+1;
			goto err;
		}
		/* Check the signature */
		ret = crypto_sign_verify(sig, siglen, test->msg, test->mlen, test->pk);
		if(ret){
			ret = test_num+1;
			goto err;
		}
		/* Check elements */
		if(siglen != (test->smlen - test->mlen)){
			ret = test_num+1;
			goto err;
		}
		if(memcmp(sig, &test->sm[test->mlen], siglen)){
			ret = test_num+1;
			goto err;
		}
	}

	ret = 0;
err:
	return ret;
}

#ifndef BARE_METAL_KAT_TESTS
int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
#if defined(EMBEDDED_KAT_NUM_TESTS)
	int num_tests = (EMBEDDED_KAT_NUM_TESTS < (int)concat2(MQOM2_PARAM_KAT, _TEST_VECTORS_COUNT)) ? EMBEDDED_KAT_NUM_TESTS : (int)concat2(MQOM2_PARAM_KAT, _TEST_VECTORS_COUNT);
#else
	int num_tests = (int)concat2(MQOM2_PARAM_KAT, _TEST_VECTORS_COUNT);
#endif
#else
int embedded_KAT_tests(int num_tests) {
	num_tests = (num_tests < 0) ? (int)concat2(MQOM2_PARAM_KAT, _TEST_VECTORS_COUNT) : num_tests;
	num_tests = (num_tests < (int)concat2(MQOM2_PARAM_KAT, _TEST_VECTORS_COUNT)) ? num_tests : (int)concat2(MQOM2_PARAM_KAT, _TEST_VECTORS_COUNT);
#endif
	int ret = -1;

	printf("===== Embedded KAT tests for %s (%d tests)\r\n", concatt(, MQOM2_PARAM_KAT), num_tests);
	ret = KAT_embedded_test_vectors(num_tests);
	if(ret){
		printf("[-] Error: test %d / %d for %s failed ...\r\n", ret-1, num_tests, concatt(, MQOM2_PARAM_KAT));
		goto err;
	}
	else{
		printf("[+] OK: all tests passed ... (%d) for %s\r\n", num_tests, concatt(, MQOM2_PARAM_KAT));
	}

	ret = 0;
err:
	return ret;
}

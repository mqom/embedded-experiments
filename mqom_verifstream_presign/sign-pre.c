#ifdef SUPERCOP
#include "crypto_sign.h"
#else
#include "api.h"
#endif

#include <stdlib.h>
#include "common.h"
#include "fields.h"
#include "xof.h"
#include "expand_mq.h"
#include "blc.h"
#include "piop.h"
#include "benchmark.h"
#include "sign.h"

extern int randombytes(unsigned char* x, unsigned long long xlen);

int Sign_Prepare(const uint8_t sk[MQOM2_SK_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], const uint8_t mask_rnd[MQOM2_PRESIGN_RND_SIZE], uint8_t presig[MQOM2_PRESIG_SIZE]) {
	int ret = -1;
	int e;

	uint8_t mseed_eq[2 * MQOM2_PARAM_SEED_SIZE];
	field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	xof_context xof_ctx;

	/* Parse the secret key */
	memcpy(mseed_eq, &sk[0], 2 * MQOM2_PARAM_SEED_SIZE);
	field_base_parse(&sk[(2 * MQOM2_PARAM_SEED_SIZE) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)], MQOM2_PARAM_MQ_N, x);

	/* Prepare the pre-signature */
	unsigned int pos = 0;
	memcpy(&presig[pos], mask_rnd, MQOM2_PRESIGN_RND_SIZE);
	pos += MQOM2_PRESIGN_RND_SIZE;
	memcpy(&presig[pos], salt, MQOM2_PARAM_SALT_SIZE);
	pos += MQOM2_PARAM_SALT_SIZE;
	uint8_t *com1 = &presig[pos];
	pos += MQOM2_PARAM_DIGEST_SIZE;
	uint8_t *com2 = &presig[pos];
	pos += MQOM2_PARAM_DIGEST_SIZE;
	uint8_t *serialized_alpha1 = &presig[pos];
	pos += MQOM2_PARAM_TAU * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
	uint8_t *preopening = &presig[pos];

	/* Commit Lines */
	blc_key_t key;
	field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt u0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt u1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	__BENCHMARK_START__(BS_BLC_COMMIT);
	BLC_Commit(mseed, salt, x, com1, &key, x0, u0, u1);
	__BENCHMARK_STOP__(BS_BLC_COMMIT);

	/* Compute P_alpha */
	field_ext_elt alpha0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt alpha1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	__BENCHMARK_START__(BS_PIOP_COMPUTE);
	ComputePAlpha(com1, x0, u0, u1, x, mseed_eq, alpha0, alpha1);
	__BENCHMARK_STOP__(BS_PIOP_COMPUTE);

	/* Hash P_alpha and compute Fiat-Shamir hash */
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x03", 1);
	ERR(ret, err);
	uint8_t alpha[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU)];
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		field_ext_serialize(alpha0[e], MQOM2_PARAM_ETA, alpha);
		ret = xof_update(&xof_ctx, alpha, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
		ERR(ret, err);
	}
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		uint8_t *buffer = &serialized_alpha1[e * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU)];
		field_ext_serialize(alpha1[e], MQOM2_PARAM_ETA, buffer);
		ret = xof_update(&xof_ctx, buffer, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
		ERR(ret, err);
	}
	ret = xof_squeeze(&xof_ctx, com2, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	/* Export BLC opening key */
	ret = BLC_Serialize_Opening_Key(&key, preopening);
	ERR(ret, err);

	/* Mask the pre-signature */
	uint8_t presign_mask[MQOM2_PRESIG_SIZE-MQOM2_PRESIGN_RND_SIZE];
	ret = xof_init(&xof_ctx);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x09", 1);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, sk, MQOM2_SK_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, mask_rnd, MQOM2_PRESIGN_RND_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, presign_mask, MQOM2_PRESIG_SIZE-MQOM2_PRESIGN_RND_SIZE);
	ERR(ret, err);
	for (uint32_t i=MQOM2_PRESIGN_RND_SIZE; i<MQOM2_PRESIG_SIZE; i++) {
		presig[i] ^= presign_mask[i-MQOM2_PRESIGN_RND_SIZE];
	}
err:
	return ret;
}

int Sign_Finalize(const uint8_t sk[MQOM2_SK_SIZE], const uint8_t *msg, unsigned long long mlen, const uint8_t presig[MQOM2_PRESIG_SIZE], uint8_t sig[MQOM2_SIG_SIZE]) {
	int ret = -1;

	uint8_t msg_hash[MQOM2_PARAM_DIGEST_SIZE], hash[MQOM2_PARAM_DIGEST_SIZE];
	field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	xof_context xof_ctx;

	/* Parse the secret key */
	const uint8_t *pk = &sk[0];
	field_base_parse(&sk[(2 * MQOM2_PARAM_SEED_SIZE) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)], MQOM2_PARAM_MQ_N, x);
	/* Parse the pre-signature */
	uint8_t presig_plain[MQOM2_PRESIG_SIZE-MQOM2_PRESIGN_RND_SIZE];
	unsigned int pos = 0;
	const uint8_t *salt = &presig_plain[pos];
	pos += MQOM2_PARAM_SALT_SIZE;
	const uint8_t *com1 = &presig_plain[pos];
	pos += MQOM2_PARAM_DIGEST_SIZE;
	const uint8_t *com2 = &presig_plain[pos];
	pos += MQOM2_PARAM_DIGEST_SIZE;
	const uint8_t *serialized_alpha1 = &presig_plain[pos];
	pos += MQOM2_PARAM_TAU * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
	const uint8_t *preopening = &presig_plain[pos];
	pos += MQOM2_PARAM_PREOPENING_SIZE;

	/* De-mask the pre-signature */
	const uint8_t *mask_rnd = &presig[0];
	ret = xof_init(&xof_ctx);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x09", 1);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, sk, MQOM2_SK_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, mask_rnd, MQOM2_PRESIGN_RND_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, presig_plain, MQOM2_PRESIG_SIZE-MQOM2_PRESIGN_RND_SIZE);
	ERR(ret, err);
	for (uint32_t i=MQOM2_PRESIGN_RND_SIZE; i<MQOM2_PRESIG_SIZE; i++) {
		presig_plain[i-MQOM2_PRESIGN_RND_SIZE] ^= presig[i];
	}

	/* Prepare the signature */
	pos = 0;
	memcpy(&sig[pos], salt, MQOM2_PARAM_SALT_SIZE);
	pos += MQOM2_PARAM_SALT_SIZE;
	memcpy(&sig[pos], com1, MQOM2_PARAM_DIGEST_SIZE);
	pos += MQOM2_PARAM_DIGEST_SIZE;
	memcpy(&sig[pos], com2, MQOM2_PARAM_DIGEST_SIZE);
	pos += MQOM2_PARAM_DIGEST_SIZE;
	memcpy(&sig[pos], serialized_alpha1, MQOM2_PARAM_TAU * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
	pos += MQOM2_PARAM_TAU * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
	uint8_t *opening = &sig[pos];
	uint8_t *nonce = &sig[MQOM2_SIG_SIZE - 4];

	/* Hash message */
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x02", 1);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, msg, mlen);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, msg_hash, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	blc_key_t key;
	ret = BLC_Deserialize_Opening_Key(preopening, salt, x, &key);
	ERR(ret, err);

	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x04", 1);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, pk, MQOM2_PK_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, com1, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, com2, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, msg_hash, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, hash, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	/* Sample Challenge */
	uint16_t i_star[MQOM2_PARAM_TAU];
	__BENCHMARK_START__(BS_SAMPLE_CHALLENGE);
	ret = SampleChallenge(hash, i_star, nonce);
	__BENCHMARK_STOP__(BS_SAMPLE_CHALLENGE);

	/* Open Line Evaluation */
	__BENCHMARK_START__(BS_BLC_OPEN);
	BLC_Open(&key, i_star, opening);
	__BENCHMARK_STOP__(BS_BLC_OPEN);

	ret = 0;
err:
	return ret;
}

int crypto_sign_prepare(unsigned char *presig, unsigned long long *presiglen, const unsigned char *sk) {
	int ret = -1;

	// Sample mseed
	uint8_t mseed[MQOM2_PARAM_SEED_SIZE];
	ret = randombytes(mseed, MQOM2_PARAM_SEED_SIZE);
	ERR(ret, err);

	// Sample salt
	uint8_t salt[MQOM2_PARAM_SALT_SIZE];
	ret = randombytes(salt, MQOM2_PARAM_SALT_SIZE);
	ERR(ret, err);

	// Sample mask_rnd
	uint8_t mask_rnd[MQOM2_PRESIGN_RND_SIZE];
	ret = randombytes(mask_rnd, MQOM2_PRESIGN_RND_SIZE);
	ERR(ret, err);

	// Build the pre-signature
	ret = Sign_Prepare(sk, salt, mseed, mask_rnd, presig);
	ERR(ret, err);
	if (presiglen != NULL) {
		*presiglen = (unsigned long long) MQOM2_PRESIG_SIZE;
	}

	ret = 0;
err:
	return ret;
}

int crypto_sign_signature_finalize(unsigned char *sig, unsigned long long *siglen,
                                   const unsigned char *m, unsigned long long mlen,
                                   const unsigned char *presig, unsigned long long presiglen,
                                   const unsigned char *sk) {
	int ret = -1;
	if (presiglen != (unsigned long long) MQOM2_PRESIG_SIZE) {
		return -1;
	}

	ret = Sign_Finalize(sk, m, mlen, presig, sig);
	ERR(ret, err);
	if (siglen != NULL) {
		*siglen = (unsigned long long) MQOM2_SIG_SIZE;
	}

	ret = 0;
err:
	return ret;
}


int crypto_sign_finalize(unsigned char* sm, unsigned long long* smlen,
                         const unsigned char *m, unsigned long long mlen,
                         const unsigned char *presig, unsigned long long presiglen,
                         const unsigned char *sk) {
	int ret = -1;

	uint8_t *message = sm;
	memmove(message, m, mlen);
	ret = crypto_sign_signature_finalize(sm + mlen, smlen, message, mlen, presig, presiglen, sk);
	ERR(ret, err);
	if (smlen != NULL) {
		*smlen += mlen;
	}

	ret = 0;
err:
	return ret;
}

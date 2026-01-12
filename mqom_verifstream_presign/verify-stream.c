#include "verify-stream.h"
#include "xof.h"
#include "blc/blc_streaming.h"
#include "piop/piop_streaming.h"
#include "benchmark.h"
#include "sign.h"

int StreamedVerify_Sign(const uint8_t sk[MQOM2_SK_SIZE], const uint8_t *msg, unsigned long long mlen, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], uint8_t sig[MQOM2_SIG_SIZE]) {
	int ret = -1;
	int e;
	uint8_t mseed_eq[2 * MQOM2_PARAM_SEED_SIZE];
	uint8_t msg_hash[MQOM2_PARAM_DIGEST_SIZE], com2[MQOM2_PARAM_DIGEST_SIZE];
	field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	xof_context xof_ctx;

	/* Parse the secret key */
	memcpy(mseed_eq, &sk[0], 2 * MQOM2_PARAM_SEED_SIZE);
	const uint8_t *pk = &sk[0];
	field_base_parse(&sk[(2 * MQOM2_PARAM_SEED_SIZE) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)], MQOM2_PARAM_MQ_N, x);

	/* Hash message */
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x02", 1);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, msg, mlen);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, msg_hash, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	/* Prepare the signature */
	unsigned int pos = 0;
	uint8_t *hash = &sig[pos];
	pos += MQOM2_PARAM_DIGEST_SIZE;
	memcpy(&sig[pos], salt, MQOM2_PARAM_SALT_SIZE);
	pos += MQOM2_PARAM_SALT_SIZE;
	uint8_t *com1 = &sig[pos];
	pos += MQOM2_PARAM_DIGEST_SIZE;
	uint8_t *nonce = &sig[pos];
	pos += 4;

	/* Commit Lines */
	blc_key_streaming_t key;
	field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt u0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt u1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	__BENCHMARK_START__(BS_BLC_COMMIT);
	BLC_Commit_streaming(mseed, salt, x, com1, &key, x0, u0, u1);
	__BENCHMARK_STOP__(BS_BLC_COMMIT);

	/* Compute P_alpha */
	field_ext_elt alpha0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt alpha1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	__BENCHMARK_START__(BS_PIOP_COMPUTE);
	ComputePAlpha_streaming(com1, x0, u0, u1, x, mseed_eq, alpha0, alpha1);
	__BENCHMARK_STOP__(BS_PIOP_COMPUTE);

	/* Hash P_alpha and compute Fiat-Shamir hash */
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x03", 1);
	ERR(ret, err);
	uint8_t alpha_ser[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU)];
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		field_ext_serialize(alpha0[e], MQOM2_PARAM_ETA, alpha_ser);
		ret = xof_update(&xof_ctx, alpha_ser, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
		ERR(ret, err);

		field_ext_serialize(alpha1[e], MQOM2_PARAM_ETA, alpha_ser);
		ret = xof_update(&xof_ctx, alpha_ser, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
		ERR(ret, err);
	}
	ret = xof_squeeze(&xof_ctx, com2, MQOM2_PARAM_DIGEST_SIZE);
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
	uint8_t opening_all[MQOM2_PARAM_OPENING_SIZE];
	BLC_Open_streaming(&key, i_star, opening_all);
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		memcpy(&sig[pos], &opening_all[e * MQOM2_PARAM_OPENING_X1_SIZE], MQOM2_PARAM_OPENING_X1_SIZE);
		pos += MQOM2_PARAM_OPENING_X1_SIZE;
		field_ext_serialize(alpha1[e], MQOM2_PARAM_ETA, alpha_ser);
		memcpy(&sig[pos], alpha_ser, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
		pos += BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
	}
	__BENCHMARK_STOP__(BS_BLC_OPEN);

	ret = 0;
err:
	return ret;
}

stream_verif_context_t *StreamedVerify_Init(const uint8_t pk[MQOM2_PK_SIZE]) {
	stream_verif_context_t *ctx = mqom_malloc(sizeof(stream_verif_context_t));
	if (ctx == NULL) {
		return NULL;
	}

	memcpy(ctx->pk, pk, MQOM2_PK_SIZE);
	ctx->piop_data = NULL;
	ctx->num_current_chunk = 0;
	ctx->pos = 0;

	/* Prepare PIOP public data */
	const uint8_t *mseed_eq = &pk[0];
	field_ext_elt y[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)];
	field_ext_parse(&pk[2 * MQOM2_PARAM_SEED_SIZE], MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU, y);
	ctx->piop_data = PreparePIOPPublicData(mseed_eq, y);

	return ctx;
}

int StreamedVerify_Update(stream_verif_context_t* ctx, const uint8_t* sigpart, unsigned long long sigpartlen) {
	if (ctx == NULL) {
		return -1;
	}

	uint32_t offset = 0;
	int ret = -1;
	xof_context xof_ctx;
	uint32_t e;

	while (offset < sigpartlen) {
		uint32_t next_chunk_size = FIRST_CHUNK_BYTESIZE;
		if (ctx->num_current_chunk > 0) {
			next_chunk_size = OTHER_CHUNK_BYTESIZE;
		}

		uint32_t maxi_to_copy = ((next_chunk_size - ctx->pos) < (sigpartlen - offset)) ? (next_chunk_size - ctx->pos) : (sigpartlen - offset);
		memcpy(&ctx->unprocessed_part[ctx->pos], sigpart + offset, maxi_to_copy);
		offset += maxi_to_copy;
		ctx->pos += maxi_to_copy;

		if (ctx->pos == next_chunk_size) {
			if (ctx->num_current_chunk == 0) {
				/* Parsing */
				memcpy(ctx->hash, &ctx->unprocessed_part[0], MQOM2_PARAM_DIGEST_SIZE);
				memcpy(ctx->salt, &ctx->unprocessed_part[MQOM2_PARAM_DIGEST_SIZE], MQOM2_PARAM_SALT_SIZE);
				memcpy(ctx->com1, &ctx->unprocessed_part[MQOM2_PARAM_DIGEST_SIZE + MQOM2_PARAM_SALT_SIZE], MQOM2_PARAM_DIGEST_SIZE);
				memcpy(ctx->nonce, &ctx->unprocessed_part[2 * MQOM2_PARAM_DIGEST_SIZE + MQOM2_PARAM_SALT_SIZE], 4);

				/* Sample Challenge */
				uint8_t tmp[MQOM2_PARAM_TAU * 2 + 2];
				ret = xof_init(&xof_ctx);
				ERR(ret, err);
				ret = xof_update(&xof_ctx, (const uint8_t*) "\x05", 1);
				ERR(ret, err);
				ret = xof_update(&xof_ctx, ctx->hash, MQOM2_PARAM_DIGEST_SIZE);
				ERR(ret, err);
				ret = xof_update(&xof_ctx, ctx->nonce, 4);
				ERR(ret, err);
				ret = xof_squeeze(&xof_ctx, tmp, MQOM2_PARAM_TAU * 2 + 2);
				ERR(ret, err);
				for (e = 0; e < MQOM2_PARAM_TAU; e++) {
					ctx->i_star[e] = (tmp[2 * e] + 256 * tmp[2 * e + 1]) & ((1 << MQOM2_PARAM_NB_EVALS_LOG) -1);
				}
				ctx->grinding_val = (tmp[2 * MQOM2_PARAM_TAU] + tmp[2 * MQOM2_PARAM_TAU + 1] * 256) & ((1 << MQOM2_PARAM_W) -1);

				/* Finalize PIOP public data */
				FinalizePIOPPublicData(ctx->piop_data, ctx->com1);

			} else {
				e = ctx->num_current_chunk - 1;

				/* Parsing */
				const uint8_t *opening = &ctx->unprocessed_part[0];
				const uint8_t *serialized_alpha1_e = &ctx->unprocessed_part[MQOM2_PARAM_OPENING_X1_SIZE];
				field_ext_parse(serialized_alpha1_e, MQOM2_PARAM_ETA, ctx->alpha1[e]);

				/* BLC + PIOP, for one repetition */
				field_ext_elt x_eval_e[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
				field_ext_elt u_eval_e[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
				field_ext_elt r = get_evaluation_point(ctx->i_star[e]);
				ret = BLC_Eval_x1_streaming(e, ctx->salt, opening, ctx->i_star[e], ctx->hash_ls_com[e], x_eval_e, u_eval_e);
				ERR(ret, err);
				ret = RecomputePAlpha_x1(ctx->piop_data, r, ctx->alpha1[e], x_eval_e, u_eval_e, ctx->alpha0[e]);
			}

			ctx->num_current_chunk++;
			ctx->pos = 0;
		}
	}

	ret = 0;
err:
	return ret;
}

int StreamedVerify_Finalize(stream_verif_context_t* ctx, const uint8_t *msg, unsigned long long mlen) {
	int ret = -1;
	uint32_t e;
	xof_context xof_ctx;

	/* Hash message */
	uint8_t msg_hash[MQOM2_PARAM_DIGEST_SIZE];
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x02", 1);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, msg, mlen);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, msg_hash, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	/* Hash hash_ls_com */
	uint8_t com1_[MQOM2_PARAM_DIGEST_SIZE];
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x07", 1);
	ERR(ret, err);
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		ret = xof_update(&xof_ctx, ctx->hash_ls_com[e], MQOM2_PARAM_DIGEST_SIZE);
		ERR(ret, err);
	}
	ret = xof_squeeze(&xof_ctx, com1_, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	/* Hash P_alpha */
	uint8_t com2[MQOM2_PARAM_DIGEST_SIZE];
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x03", 1);
	ERR(ret, err);
	uint8_t alpha0[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU)];
	uint8_t alpha1[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU)];
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		field_ext_serialize(ctx->alpha0[e], MQOM2_PARAM_ETA, alpha0);
		field_ext_serialize(ctx->alpha1[e], MQOM2_PARAM_ETA, alpha1);
		ret = xof_update(&xof_ctx, alpha0, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
		ERR(ret, err);
		ret = xof_update(&xof_ctx, alpha1, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA * MQOM2_PARAM_MU));
		ERR(ret, err);
	}
	ret = xof_squeeze(&xof_ctx, com2, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	/* Compute Fiat-Shamir hash */
	uint8_t hash_[MQOM2_PARAM_DIGEST_SIZE];
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x04", 1);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, ctx->pk, MQOM2_PK_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, ctx->com1, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, com2, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, msg_hash, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, hash_, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	/* Verification */
	if (ctx->grinding_val != 0) {
		ret = -1;
		goto err;
	}
	if (memcmp(ctx->com1, com1_, MQOM2_PARAM_DIGEST_SIZE)) {
		ret = -1;
		goto err;
	}
	if (memcmp(ctx->hash, hash_, MQOM2_PARAM_DIGEST_SIZE)) {
		ret = -1;
		goto err;
	}

	ret = 0;
err:
	return ret;
}

void StreamedVerify_Clean(stream_verif_context_t* ctx) {
	CleanPIOPPublicData(ctx->piop_data);
	if (ctx) {
		mqom_free(ctx, sizeof(stream_verif_context_t));
	}
}

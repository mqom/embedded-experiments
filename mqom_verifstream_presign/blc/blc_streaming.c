#include "blc_streaming.h"
#include "ggm_tree.h"
#include "benchmark.h"

#ifndef BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE
/* If not defined by the user, default to 1 */
#define BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE 1
#else
#if BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE > MQOM2_PARAM_NB_EVALS
#error "Error, BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE should be smaller (or equal) to MQOM2_PARAM_NB_EVALS"
#endif
#endif

/* SeedCommit variants
 * NOTE: we factorize the key schedule, the tweaked salt is inside the encryption context
 */
static inline void SeedCommit(enc_ctx *ctx1, enc_ctx *ctx2, const uint8_t seed[MQOM2_PARAM_SEED_SIZE], uint8_t seed_com[2 * MQOM2_PARAM_SEED_SIZE]) {
	uint8_t linortho_seed[MQOM2_PARAM_SEED_SIZE];
	LinOrtho(seed, linortho_seed);
	enc_encrypt_x2(ctx1, ctx2, seed, seed, &seed_com[0], &seed_com[MQOM2_PARAM_SEED_SIZE]);
	/* Xor with LinOrtho seed */
	xor_blocks(&seed_com[0], linortho_seed, &seed_com[0]);
	xor_blocks(&seed_com[MQOM2_PARAM_SEED_SIZE], linortho_seed, &seed_com[MQOM2_PARAM_SEED_SIZE]);

	return;
}

int BLC_Commit_x1_streaming(uint32_t e, const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], uint8_t com[MQOM2_PARAM_DIGEST_SIZE], blc_key_streaming_x1_t* key, field_ext_elt x0[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u0[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt u1[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t i;
	prg_key_sched_cache* prg_cache = NULL;

	/* Compute delta */
	uint8_t delta[MQOM2_PARAM_SEED_SIZE];
	DeriveDelta(x, delta);

	xof_context xof_ctx_hash_ls_com;
	ret = xof_init(&xof_ctx_hash_ls_com);
	ERR(ret, err);
	ret = xof_update(&xof_ctx_hash_ls_com, (const uint8_t*) "\x07", 1);
	ERR(ret, err);

	enc_ctx ctx_seed_commit1, ctx_seed_commit2;
	uint8_t lseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t ls_com[BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE][MQOM2_PARAM_DIGEST_SIZE];
	uint8_t exp[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	field_base_elt bar_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt bar_u[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt tmp_n[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt tmp_eta[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_base_elt acc_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	uint8_t data_folding[MQOM2_PARAM_NB_EVALS_LOG][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	uint8_t acc[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	xof_context xof_ctx;
	ggmtree_ctx_t ggm_tree;
	uint8_t tweaked_salt[MQOM2_PARAM_SALT_SIZE];

	/* Initialize the PRG cache when used */
#ifndef NO_BLC_PRG_CACHE
	prg_cache = init_prg_cache(PRG_BLC_SIZE);
#endif

	__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
	memcpy(key->rseed, rseed, MQOM2_PARAM_SEED_SIZE);
	ret = GGMTree_InitIncrementalExpansion(&ggm_tree, salt, rseed, delta, e);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

	__BENCHMARK_START__(BS_BLC_SEED_COMMIT);
	TweakSalt(salt, tweaked_salt, 0, e, 0);
	ret = enc_key_sched(&ctx_seed_commit1, tweaked_salt);
	ERR(ret, err);
	tweaked_salt[0] ^= 0x01;
	ret = enc_key_sched(&ctx_seed_commit2, tweaked_salt);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_SEED_COMMIT);

	__BENCHMARK_START__(BS_BLC_XOF);
	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x06", 1);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_XOF);

	memset((uint8_t*) data_folding, 0, sizeof(data_folding));
	memset((uint8_t*) acc, 0, sizeof(acc));
	for (i = 0; i < MQOM2_PARAM_NB_EVALS; i++) {
		uint32_t i_mod = i % BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE;
		__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
		GGMTree_GetNextLeaf(&ggm_tree, lseed);
		__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

		__BENCHMARK_START__(BS_BLC_PRG);
		memcpy(exp, lseed, MQOM2_PARAM_SEED_SIZE);
		ret = PRG(salt, e, lseed, PRG_BLC_SIZE, exp + MQOM2_PARAM_SEED_SIZE, prg_cache);
		ERR(ret, err);
		__BENCHMARK_STOP__(BS_BLC_PRG);

		__BENCHMARK_START__(BS_BLC_ARITH);
		field_base_vect_add(acc, exp, acc, MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
		uint8_t j = get_gray_code_bit_position(i);
		field_base_vect_add(data_folding[j], acc, data_folding[j], MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
		__BENCHMARK_STOP__(BS_BLC_ARITH);

		__BENCHMARK_START__(BS_BLC_SEED_COMMIT);
		SeedCommit(&ctx_seed_commit1, &ctx_seed_commit2, lseed, ls_com[i_mod]);
		__BENCHMARK_STOP__(BS_BLC_SEED_COMMIT);

		if (i_mod == BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE - 1) {
			__BENCHMARK_START__(BS_BLC_XOF);
			ret = xof_update(&xof_ctx, (uint8_t*) ls_com, BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE * MQOM2_PARAM_DIGEST_SIZE);
			ERR(ret, err);
			__BENCHMARK_STOP__(BS_BLC_XOF);
		}
	}
	/* Invalidate the PRG cache */
	destroy_prg_cache(prg_cache);
	prg_cache = NULL;

	__BENCHMARK_START__(BS_BLC_ARITH);
	memset(x0, 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_N));
	for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
		field_base_parse(data_folding[j], MQOM2_PARAM_MQ_N, bar_x);
		field_ext_base_constant_vect_mult((1 << j), bar_x, tmp_n, MQOM2_PARAM_MQ_N);
		field_ext_vect_add(x0, tmp_n, x0, MQOM2_PARAM_MQ_N);
	}

	memset(u0, 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA));
	for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
		field_ext_parse(data_folding[j] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, bar_u);
		field_ext_constant_vect_mult((1 << j), bar_u, tmp_eta, MQOM2_PARAM_ETA);
		field_ext_vect_add(u0, tmp_eta, u0, MQOM2_PARAM_ETA);
	}

	field_base_parse(acc, MQOM2_PARAM_MQ_N, acc_x);
	field_ext_parse(acc + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, u1);

	field_base_elt delta_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	uint8_t serialized_delta_x[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)];
	field_base_vect_add(x, acc_x, delta_x, MQOM2_PARAM_MQ_N);
	field_base_serialize(delta_x, MQOM2_PARAM_MQ_N, serialized_delta_x);
	memcpy(key->partial_delta_x, serialized_delta_x + MQOM2_PARAM_SEED_SIZE, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	__BENCHMARK_STOP__(BS_BLC_ARITH);

	__BENCHMARK_START__(BS_BLC_XOF);
	ret = xof_update(&xof_ctx, key->partial_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, com, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_XOF);

	ret = 0;
err:
	destroy_prg_cache(prg_cache);
	return ret;
}

int BLC_Open_x1_streaming(uint32_t e, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], const blc_key_streaming_x1_t* key, uint16_t i_star, uint8_t opening[MQOM2_PARAM_OPENING_X1_SIZE]) {
	int ret = -1;
	enc_ctx ctx_seed_commit1, ctx_seed_commit2;
	uint8_t lseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t tweaked_salt[MQOM2_PARAM_SALT_SIZE];

	uint8_t *path = &opening[0];
	uint8_t *out_ls_com = &opening[MQOM2_PARAM_SEED_SIZE * MQOM2_PARAM_NB_EVALS_LOG];
	uint8_t *partial_delta_x = &opening[(MQOM2_PARAM_SEED_SIZE * MQOM2_PARAM_NB_EVALS_LOG) + MQOM2_PARAM_DIGEST_SIZE];

	/* Compute delta */
	uint8_t delta[MQOM2_PARAM_SEED_SIZE];
	DeriveDelta(x, delta);

	ret = GGMTree_ExpandPath(salt, key->rseed, delta, e, i_star, (uint8_t(*)[MQOM2_PARAM_SEED_SIZE]) path, lseed);
	ERR(ret, err);
	TweakSalt(salt, tweaked_salt, 0, e, 0);
	ret = enc_key_sched(&ctx_seed_commit1, tweaked_salt);
	ERR(ret, err);
	tweaked_salt[0] ^= 0x01;
	ret = enc_key_sched(&ctx_seed_commit2, tweaked_salt);
	ERR(ret, err);
	SeedCommit(&ctx_seed_commit1, &ctx_seed_commit2, lseed, out_ls_com);

	memcpy(partial_delta_x, key->partial_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);

	ret = 0;
err:
	return ret;
}

int BLC_Eval_x1_streaming(uint32_t e, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t opening[MQOM2_PARAM_OPENING_X1_SIZE], uint16_t i_star, uint8_t com[MQOM2_PARAM_DIGEST_SIZE], field_ext_elt x_eval[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u_eval[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t i;

	prg_key_sched_cache *prg_cache = NULL;

	uint8_t tweaked_salt[MQOM2_PARAM_SALT_SIZE];

	const uint8_t *path = &opening[0];
	const uint8_t *out_ls_com = &opening[MQOM2_PARAM_SEED_SIZE * MQOM2_PARAM_NB_EVALS_LOG];
	const uint8_t *partial_delta_x = &opening[(MQOM2_PARAM_SEED_SIZE * MQOM2_PARAM_NB_EVALS_LOG) + MQOM2_PARAM_DIGEST_SIZE];

	xof_context xof_ctx_hash_ls_com;
	ret = xof_init(&xof_ctx_hash_ls_com);
	ERR(ret, err);
	ret = xof_update(&xof_ctx_hash_ls_com, (const uint8_t*) "\x07", 1);
	ERR(ret, err);

	enc_ctx ctx_seed_commit1, ctx_seed_commit2;
	uint8_t lseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t ls_com[BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE][MQOM2_PARAM_DIGEST_SIZE];
	uint8_t exp[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	uint8_t data_folding[MQOM2_PARAM_NB_EVALS_LOG][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	uint8_t acc[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	field_base_elt bar_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt bar_u[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt tmp_n[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt tmp_eta[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_base_elt acc_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	xof_context xof_ctx;
	ggmtree_ctx_partial_t ggm_tree;

	/* Initialize the PRG cache when used */
#ifndef NO_BLC_PRG_CACHE
	prg_cache = init_prg_cache(PRG_BLC_SIZE);
#endif

	ret = GGMTree_InitIncrementalPartialExpansion(&ggm_tree, salt, (uint8_t(*)[MQOM2_PARAM_SEED_SIZE]) path, e, i_star);
	ERR(ret, err);

	TweakSalt(salt, tweaked_salt, 0, e, 0);
	ret = enc_key_sched(&ctx_seed_commit1, tweaked_salt);
	ERR(ret, err);
	tweaked_salt[0] ^= 0x01;
	ret = enc_key_sched(&ctx_seed_commit2, tweaked_salt);
	ERR(ret, err);

	ret = xof_init(&xof_ctx);
	ERR(ret, err);
	ret = xof_update(&xof_ctx, (const uint8_t*) "\x06", 1);
	ERR(ret, err);

	memset((uint8_t*) data_folding, 0, sizeof(data_folding));
	memset((uint8_t*) acc, 0, sizeof(acc));
	for (i = 0; i < MQOM2_PARAM_NB_EVALS; i++) {
		uint32_t i_mod = i % BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE;
		GGMTree_GetNextLeafPartial(&ggm_tree, lseed);

		if (i != i_star) {
			memcpy(exp, lseed, MQOM2_PARAM_SEED_SIZE);
			ret = PRG(salt, e, lseed, PRG_BLC_SIZE, exp + MQOM2_PARAM_SEED_SIZE, prg_cache);
			ERR(ret, err);

			SeedCommit(&ctx_seed_commit1, &ctx_seed_commit2, lseed, ls_com[i_mod]);
		} else {
			memset(exp, 0, MQOM2_PARAM_SEED_SIZE + PRG_BLC_SIZE);
			memcpy(ls_com[i_mod], out_ls_com, MQOM2_PARAM_DIGEST_SIZE);
		}

		field_base_vect_add(acc, exp, acc, MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
		uint8_t j = get_gray_code_bit_position(i);
		field_base_vect_add(data_folding[j], acc, data_folding[j], MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);

		if (i_mod == BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE - 1) {
			ret = xof_update(&xof_ctx, (uint8_t*) ls_com, BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE * MQOM2_PARAM_DIGEST_SIZE);
			ERR(ret, err);
		}
	}
	/* Invalidate the PRG cache */
	destroy_prg_cache(prg_cache);
	prg_cache = NULL;

	field_ext_elt r = get_evaluation_point(i_star);

	field_base_elt delta_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	uint8_t serialized_delta_x[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)];
	memset(serialized_delta_x, 0, MQOM2_PARAM_SEED_SIZE);
	memcpy(serialized_delta_x + MQOM2_PARAM_SEED_SIZE, partial_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	field_base_parse(serialized_delta_x, MQOM2_PARAM_MQ_N, delta_x);

	memset(x_eval, 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_N));
	for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
		field_base_parse(data_folding[j], MQOM2_PARAM_MQ_N, bar_x);
		field_ext_base_constant_vect_mult((1 << j), bar_x, tmp_n, MQOM2_PARAM_MQ_N);
		field_ext_vect_add(x_eval, tmp_n, x_eval, MQOM2_PARAM_MQ_N);
	}
	field_base_parse(acc, MQOM2_PARAM_MQ_N, acc_x);
	field_base_vect_add(acc_x, delta_x, acc_x, MQOM2_PARAM_MQ_N);
	field_ext_base_constant_vect_mult(r, acc_x, tmp_n, MQOM2_PARAM_MQ_N);
	field_ext_vect_add(x_eval, tmp_n, x_eval, MQOM2_PARAM_MQ_N);

	memset(u_eval, 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA));
	for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
		field_ext_parse(data_folding[j] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, bar_u);
		field_ext_constant_vect_mult((1 << j), bar_u, tmp_eta, MQOM2_PARAM_ETA);
		field_ext_vect_add(u_eval, tmp_eta, u_eval, MQOM2_PARAM_ETA);
	}
	field_ext_parse(acc + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, tmp_eta);
	field_ext_constant_vect_mult(r, tmp_eta, tmp_eta, MQOM2_PARAM_ETA);
	field_ext_vect_add(u_eval, tmp_eta, u_eval, MQOM2_PARAM_ETA);

	ret = xof_update(&xof_ctx, partial_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&xof_ctx, com, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	ret = 0;
err:
	destroy_prg_cache(prg_cache);
	return ret;
}

int BLC_Commit_streaming(const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], blc_key_streaming_t* key, field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt u1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t e;

	/* Compute the rseed table */
	uint8_t rseed[MQOM2_PARAM_TAU][MQOM2_PARAM_SEED_SIZE];
	memcpy(key->mseed, mseed, MQOM2_PARAM_SEED_SIZE);
	ret = DeriveRootSeeds(mseed, rseed);
	ERR(ret, err);

	memcpy(key->salt, salt, MQOM2_PARAM_SALT_SIZE);
	memcpy(key->x, x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N));

	xof_context xof_ctx_hash_ls_com;
	ret = xof_init(&xof_ctx_hash_ls_com);
	ERR(ret, err);
	ret = xof_update(&xof_ctx_hash_ls_com, (const uint8_t*) "\x07", 1);
	ERR(ret, err);
	uint8_t hash_ls_com[MQOM2_PARAM_DIGEST_SIZE];

	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		ret = BLC_Commit_x1_streaming(e, rseed[e], salt, x, hash_ls_com, &key->subkey[e], x0[e], u0[e], u1[e]);
		ERR(ret, err);
		__BENCHMARK_START__(BS_BLC_XOF);
		ret = xof_update(&xof_ctx_hash_ls_com, hash_ls_com, MQOM2_PARAM_DIGEST_SIZE);
		ERR(ret, err);
		__BENCHMARK_STOP__(BS_BLC_XOF);
	}

	__BENCHMARK_START__(BS_BLC_XOF);
	ret = xof_squeeze(&xof_ctx_hash_ls_com, com1, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_XOF);

	ret = 0;
err:
	return ret;
}

int BLC_Open_streaming(const blc_key_streaming_t* key, const uint16_t i_star[MQOM2_PARAM_TAU], uint8_t opening[MQOM2_PARAM_OPENING_SIZE]) {
	int ret = -1;
	int e;

	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		ret = BLC_Open_x1_streaming(e, key->salt, key->x, &key->subkey[e], i_star[e], &opening[e * MQOM2_PARAM_OPENING_X1_SIZE]);
		ERR(ret, err);
	}

	ret = 0;
err:
	return ret;
}

int BLC_Eval_streaming(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], const uint8_t opening[MQOM2_PARAM_OPENING_SIZE], const uint16_t i_star[MQOM2_PARAM_TAU], field_ext_elt x_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t e;

	uint8_t com1_[MQOM2_PARAM_DIGEST_SIZE];

	xof_context xof_ctx_hash_ls_com;
	ret = xof_init(&xof_ctx_hash_ls_com);
	ERR(ret, err);
	ret = xof_update(&xof_ctx_hash_ls_com, (const uint8_t*) "\x07", 1);
	ERR(ret, err);
	uint8_t hash_ls_com[MQOM2_PARAM_DIGEST_SIZE];

	e = 0;
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		ret = BLC_Eval_x1_streaming(e, salt, &opening[e * MQOM2_PARAM_OPENING_X1_SIZE], i_star[e], hash_ls_com, x_eval[e], u_eval[e]);
		ERR(ret, err);
		ret = xof_update(&xof_ctx_hash_ls_com, hash_ls_com, MQOM2_PARAM_DIGEST_SIZE);
		ERR(ret, err);
	}

	ret = xof_squeeze(&xof_ctx_hash_ls_com, com1_, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	if (memcmp(com1, com1_, MQOM2_PARAM_DIGEST_SIZE) != 0) {
		ret = -1;
		goto err;
	}

	ret = 0;
err:
	return ret;
}

int BLC_Serialize_Opening_Key_streaming(const blc_key_streaming_t* key, uint8_t preopening[MQOM2_PARAM_PREOPENING_SIZE]) {
	memcpy(&preopening[0], key->mseed, MQOM2_PARAM_SEED_SIZE);
	for (uint32_t e = 0; e < MQOM2_PARAM_TAU; e++) {
		memcpy(&preopening[MQOM2_PARAM_SEED_SIZE + e * (BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE)], key->subkey[e].partial_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	}
	return 0;
}

int BLC_Deserialize_Opening_Key_streaming(const uint8_t preopening[MQOM2_PARAM_PREOPENING_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], blc_key_streaming_t* key) {
	int ret = -1;

	memcpy(key->salt, salt, MQOM2_PARAM_SALT_SIZE);
	memcpy(key->x, x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N));

	uint8_t rseed[MQOM2_PARAM_TAU][MQOM2_PARAM_SEED_SIZE];
	memcpy(key->mseed, &preopening[0], MQOM2_PARAM_SEED_SIZE);
	ret = DeriveRootSeeds(key->mseed, rseed);
	ERR(ret, err);

	for (uint32_t e = 0; e < MQOM2_PARAM_TAU; e++) {
		memcpy(key->subkey[e].rseed, rseed[e], MQOM2_PARAM_SEED_SIZE);
		memcpy(key->subkey[e].partial_delta_x, &preopening[MQOM2_PARAM_SEED_SIZE + e * (BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE)], BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	}

	ret = 0;
err:
	return ret;
}

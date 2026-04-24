#include "blc_streaming.h"
#include "ggm_tree.h"
#include "benchmark.h"
#include "blc_memopt_x1.h"
#include "blc_memopt_x1_folding.h"
#include "blc_memopt_x1_seedcommit.h"

#include "ggm_tree_incr_batch.h"
#if GGMTREE_NB_SIMULTANEOUS_LEAVES % BLC_NB_LEAF_SEEDS_IN_PARALLEL != 0
#error BLC_NB_LEAF_SEEDS_IN_PARALLEL should divide GGMTREE_NB_SIMULTANEOUS_LEAVES.
#endif

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
	uint32_t i, i_;

	uint8_t lseeds[GGMTREE_NB_SIMULTANEOUS_LEAVES][MQOM2_PARAM_SEED_SIZE];
	ggmtree_ctx_batch_t DECL_VAR(ggm_tree);
	folding_sign_t folding;
	seedcommit_sign_ctx_t DECL_VAR(seedcommit_ctx);

	/* Compute delta */
	uint8_t delta[MQOM2_PARAM_SEED_SIZE];
	DeriveDelta(x, delta);

	// Initialize the GGM tree
	__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
	memcpy(key->rseed, rseed, MQOM2_PARAM_SEED_SIZE);
	ret = GGMTree_InitIncrementalExpansion_batch(&ggm_tree, salt, rseed, delta, e);
	__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

	// Initialize the hash context
	ret = init_seedcommit_sign(&seedcommit_ctx, salt, e);
	ERR(ret, err);

	ret = InitializeFolding_sign(&folding, salt, e);
	ERR(ret, err);
	for (i = 0; i < MQOM2_PARAM_NB_EVALS; i+= GGMTREE_NB_SIMULTANEOUS_LEAVES) {
		// Derive the next leaf seeds
		__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
		ret = GGMTree_GetNextLeafs_batch(&ggm_tree, lseeds);
		ERR(ret, err);
		__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

		for (i_ = 0; i_ < (GGMTREE_NB_SIMULTANEOUS_LEAVES/BLC_NB_LEAF_SEEDS_IN_PARALLEL); i_++) {
			// Compute the individual commitments for all the seed leafs,
			// and incrementally hash them.
			ret = SeedCommitThenAbsorb_sign(&seedcommit_ctx, &lseeds[i_*BLC_NB_LEAF_SEEDS_IN_PARALLEL]);
			ERR(ret, err);

			// Expand each seed and accumulate the expanded tapes
			ret = SeedExpandThenAccumulate_sign(&folding, i + i_*BLC_NB_LEAF_SEEDS_IN_PARALLEL, &lseeds[i_*BLC_NB_LEAF_SEEDS_IN_PARALLEL]);
			ERR(ret, err);
		}
	}

	// Finalize the folding to get the committed polynomials
	__BENCHMARK_START__(BS_BLC_ARITH);
	FinalizeFolding_sign(&folding, x, key->partial_delta_x, x0, u0, u1);
	__BENCHMARK_STOP__(BS_BLC_ARITH);

	// Get the global commitment digest
	__BENCHMARK_START__(BS_BLC_XOF);
	ret = xof_update(&seedcommit_ctx.xof_ctx, key->partial_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&seedcommit_ctx.xof_ctx, com, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_XOF);

	ret = 0;
err:
	seedcommit_sign_clean_ctx(&seedcommit_ctx);
	ggmtree_ctx_batch_t_clean(&ggm_tree);
	folding_sign_clean_ctx(&folding);
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
	uint32_t i, i_;

	uint8_t lseeds[GGMTREE_NB_SIMULTANEOUS_LEAVES][MQOM2_PARAM_SEED_SIZE];
	ggmtree_ctx_partial_batch_t DECL_VAR(ggm_tree);
	folding_verify_t folding;
	seedcommit_verify_ctx_t DECL_VAR(seedcommit_ctx);

	const uint8_t *path = &opening[0];
	const uint8_t *out_ls_com = &opening[MQOM2_PARAM_SEED_SIZE * MQOM2_PARAM_NB_EVALS_LOG];
	const uint8_t *partial_delta_x = &opening[(MQOM2_PARAM_SEED_SIZE * MQOM2_PARAM_NB_EVALS_LOG) + MQOM2_PARAM_DIGEST_SIZE];

	// Initialize the GGM tree
	ret = GGMTree_InitIncrementalPartialExpansion_batch(&ggm_tree, salt, (const uint8_t(*)[MQOM2_PARAM_SEED_SIZE]) path, e, i_star);
	ERR(ret, err);

	// Initialize the hash context
	ret = init_seedcommit_verify(&seedcommit_ctx, salt, e, i_star, out_ls_com);
	ERR(ret, err);

	ret = InitializeFolding_verify(&folding, salt, e);
	ERR(ret, err);
	for (i = 0; i < MQOM2_PARAM_NB_EVALS; i+= GGMTREE_NB_SIMULTANEOUS_LEAVES) {
		ret = GGMTree_GetNextLeafsPartial_batch(&ggm_tree, lseeds);
		ERR(ret, err);

		for (i_ = 0; i_ < (GGMTREE_NB_SIMULTANEOUS_LEAVES/BLC_NB_LEAF_SEEDS_IN_PARALLEL); i_++) {
		ret = SeedCommitThenAbsorb_verify(&seedcommit_ctx, i + i_*BLC_NB_LEAF_SEEDS_IN_PARALLEL, &lseeds[i_*BLC_NB_LEAF_SEEDS_IN_PARALLEL]);
			ERR(ret, err);
			ret = SeedExpandThenAccumulate_verify(&folding, i + i_*BLC_NB_LEAF_SEEDS_IN_PARALLEL, &lseeds[i_*BLC_NB_LEAF_SEEDS_IN_PARALLEL], i_star);
			ERR(ret, err);
		}
	}
	FinalizeFolding_verify(&folding, i_star, partial_delta_x, x_eval, u_eval);

	ret = xof_update(&seedcommit_ctx.xof_ctx, partial_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE);
	ERR(ret, err);
	ret = xof_squeeze(&seedcommit_ctx.xof_ctx, com, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);

	ret = 0;
err:
	seedcommit_verify_clean_ctx(&seedcommit_ctx);
	ggmtree_ctx_partial_batch_t_clean(&ggm_tree);
	folding_verify_clean_ctx(&folding);
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

void BLC_PrintConfig_streaming() {
	printf("  BLC: streaming\r\n");
#ifdef BLC_NB_LEAF_SEEDS_IN_PARALLEL
	mqom_print("    BLC_NB_LEAF_SEEDS_IN_PARALLEL %d\r\n", BLC_NB_LEAF_SEEDS_IN_PARALLEL);
#else
	mqom_print("    BLC_NB_LEAF_SEEDS_IN_PARALLEL 8 (default)\r\n");
#endif
#ifdef BLC_SEEDEXPAND_CACHE
	mqom_print("    SeedExpand cache ON\r\n");
#endif
#ifdef BLC_SEEDCOMMIT_CACHE
	mqom_print("    SeedCommit cache ON\r\n");
#endif

	// GGM Tree
#ifdef GGM_TREE_NO_BATCHING
#ifdef BLC_NB_LEAF_SEEDS_IN_PARALLEL
	mqom_print("    GGMTREE_NB_PARALLEL_DERIVATIONS_LOG %d\r\n", GGMTREE_NB_PARALLEL_DERIVATIONS_LOG);
#else
	mqom_print("    GGMTREE_NB_PARALLEL_DERIVATIONS_LOG 0 (default)\r\n");
#endif
#else
	mqom_print("    GGMTREE_NB_SIMULTANEOUS_LEAVES_LOG %d\r\n", GGMTREE_NB_SIMULTANEOUS_LEAVES_LOG);
#endif

#ifdef GGMTREE_NB_ENC_CTX_IN_MEMORY
	mqom_print("    GGMTREE_NB_ENC_CTX_IN_MEMORY %d\r\n", GGMTREE_NB_ENC_CTX_IN_MEMORY);
#else
	mqom_print("    GGMTREE_NB_ENC_CTX_IN_MEMORY 1 (default)\r\n");
#endif
}

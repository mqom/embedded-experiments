#include "blc_onetree_memopt.h"
#include "ggm_tree_multi.h"
#include "ggm_tree_multi_2.h"
#include "benchmark.h"
#include "seed_commit.h"

#if defined(SUPERCOP)
#include "crypto_declassify.h"
#endif

#ifndef BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE
/* If not defined by the user, default to 1 */
#define BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE 1
#else
#if BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE > MQOM2_PARAM_NB_EVALS
#error "Error, BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE should be smaller (or equal) to MQOM2_PARAM_NB_EVALS"
#endif
#endif

#define BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN_HALF ((BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN+1)/2)

#define PRG_BLC_SIZE (BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA))

static inline int DeriveTreeRandomness(const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], uint8_t delta[MQOM2_PARAM_SEED_SIZE], uint8_t rseed[MQOM2_PARAM_SEED_SIZE]) {
	int ret = -1;

	uint8_t tree_prg_salt[MQOM2_PARAM_SALT_SIZE] = { 0 };
	uint8_t buffer[2*MQOM2_PARAM_SEED_SIZE];
	ret = PRG(tree_prg_salt, 0, mseed, 2*MQOM2_PARAM_SEED_SIZE, buffer, NULL);
	ERR(ret, err);
	memcpy(delta, &buffer[0], MQOM2_PARAM_SEED_SIZE);
	memcpy(rseed, &buffer[MQOM2_PARAM_SEED_SIZE], MQOM2_PARAM_SEED_SIZE);

	ret = 0;
err:
	return ret;
}

static inline void DeriveHiddenLeaves(const uint16_t i_star[MQOM2_PARAM_TAU], uint32_t hidden_leaves[MQOM2_PARAM_TAU]) {
	uint32_t e, e_;
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		hidden_leaves[e] = (i_star[e]*MQOM2_PARAM_TAU) + e;
	}
	// Sort the list 'hidden_leaves'
	// We do not care about constant-time here
	//  because there is no sensitive data
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		for (e_ = e+1; e_ < MQOM2_PARAM_TAU; e_++) {
			if(hidden_leaves[e] > hidden_leaves[e_]) {
				uint32_t tmp = hidden_leaves[e];
				hidden_leaves[e] = hidden_leaves[e_];
				hidden_leaves[e_] = tmp;
			}
		}
	}
}

int BLC_Commit_partial_onetree_memopt(uint32_t e_start, const uint8_t delta[MQOM2_PARAM_SEED_SIZE], const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], uint8_t hash_ls_com[][MQOM2_PARAM_DIGEST_SIZE], uint8_t all_delta_x[][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)], field_ext_elt x0[][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u0[][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt u1[][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t e, i;
	seedcommit_ctx_t DECL_VAR(seedcommit_ctx[BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN]);
	prg_key_sched_cache_x2* prg_cache[BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN_HALF] = {NULL};

	uint8_t lseed[2][MQOM2_PARAM_SEED_SIZE];
	uint8_t ls_com[BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN][BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE][MQOM2_PARAM_DIGEST_SIZE];
	uint8_t exp[2][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	field_base_elt bar_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt bar_u[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt tmp_n[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt tmp_eta[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_base_elt acc_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
#ifndef BLC_NO_FAST_FOLDING
	uint8_t data_folding[BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN][MQOM2_PARAM_NB_EVALS_LOG][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
#endif
	uint8_t acc[BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	xof_context DECL_VAR(xof_ctx[BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN]);

	gmmtreemulti_ctx_2_t ggm_tree;
	__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
	ret = GGMTreeMulti_InitIterator_2(&ggm_tree, salt, rseed, delta);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

	for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN && e < MQOM2_PARAM_TAU; e++) {
		uint32_t e_mod = e - e_start;

		/* Initialize the PRG cache when used */
#ifndef NO_BLC_PRG_CACHE
		if(e_mod % 2 == 0)
			prg_cache[e_mod/2] = init_prg_cache_x2(PRG_BLC_SIZE);
#endif

		__BENCHMARK_START__(BS_BLC_SEED_COMMIT);
		ret = init_seedcommit(&seedcommit_ctx[e_mod], salt, e);
		ERR(ret, err);
		__BENCHMARK_STOP__(BS_BLC_SEED_COMMIT);

		__BENCHMARK_START__(BS_BLC_XOF);
		ret = xof_init(&xof_ctx[e_mod]);
		ERR(ret, err);
		ret = xof_update(&xof_ctx[e_mod], (const uint8_t*) "\x06", 1);
		ERR(ret, err);
		__BENCHMARK_STOP__(BS_BLC_XOF);

#ifndef BLC_NO_FAST_FOLDING
		memset((uint8_t*) data_folding[e_mod], 0, sizeof(data_folding[e_mod]));
		memset((uint8_t*) acc[e_mod], 0, sizeof(acc[e_mod]));
#else
		memset(x0[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_N));
		memset(u0[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA));
		memset((uint8_t*) acc[e_mod], 0, sizeof(acc[e_mod]));
#endif
	}

	for (i = 0; i < MQOM2_PARAM_NB_EVALS; i++) {
		uint32_t i_mod = i % BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE;

		__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
		for (e = 0; e < e_start; e++) {
			ret = GGMTreeMulti_GetNext_2(&ggm_tree.iterator, lseed[0]);
			ERR(ret, err);
		}
		__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

		for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN && e < MQOM2_PARAM_TAU; e+=2) {
			uint32_t e_mod = e - e_start;
			uint32_t nb_es = 1;
			if((e+1)-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN && (e+1) < MQOM2_PARAM_TAU)
				nb_es = 2;

			__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
			ret = GGMTreeMulti_GetNext_2(&ggm_tree.iterator, lseed[0]);
			if(nb_es == 2) {
				ret = GGMTreeMulti_GetNext_2(&ggm_tree.iterator, lseed[1]);
				ERR(ret, err);
			} else {
				memset(lseed[1], 0, MQOM2_PARAM_SEED_SIZE);
			}
			__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

			__BENCHMARK_START__(BS_BLC_PRG);
			uint32_t es[2] = {e, e+1};
			uint8_t *exp_ptr[2] = {exp[0], exp[1]};
			ret = PRG_x2(salt, es, lseed, PRG_BLC_SIZE, exp_ptr, prg_cache[e_mod/2], 2);
			ERR(ret, err);
			__BENCHMARK_STOP__(BS_BLC_PRG);
			
			for(uint32_t e_=0; e_<nb_es; e_++) {
				__BENCHMARK_START__(BS_BLC_ARITH);
				field_base_vect_add(acc[e_mod+e_], exp[e_], acc[e_mod+e_], MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
				
				uint8_t j = get_gray_code_bit_position(i);
#ifndef BLC_NO_FAST_FOLDING
				field_base_vect_add(data_folding[e_mod+e_][j], acc[e_mod+e_], data_folding[e_mod+e_][j], MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
#else
				field_base_parse(acc[e_mod+e_], MQOM2_PARAM_MQ_N, bar_x);
				field_ext_base_constant_vect_mult((1 << j), bar_x, tmp_n, MQOM2_PARAM_MQ_N);
				field_ext_vect_add(x0[e_mod+e_], tmp_n, x0[e_mod+e_], MQOM2_PARAM_MQ_N);

				field_ext_parse(acc[e_mod+e_] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, bar_u);
				field_ext_constant_vect_mult((1 << j), bar_u, tmp_eta, MQOM2_PARAM_ETA);
				field_ext_vect_add(u0[e_mod+e_], tmp_eta, u0[e_mod+e_], MQOM2_PARAM_ETA);
#endif
				__BENCHMARK_STOP__(BS_BLC_ARITH);

				__BENCHMARK_START__(BS_BLC_SEED_COMMIT);
				SeedCommit(&seedcommit_ctx[e_mod+e_], lseed[e_], ls_com[e_mod+e_][i_mod]);
				__BENCHMARK_STOP__(BS_BLC_SEED_COMMIT);
			}
		}

		__BENCHMARK_START__(BS_BLC_EXPAND_TREE);
		for (e = e_start + BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN; e < MQOM2_PARAM_TAU; e++) {
			ret = GGMTreeMulti_GetNext_2(&ggm_tree.iterator, lseed[0]);
			ERR(ret, err);
		}
		__BENCHMARK_STOP__(BS_BLC_EXPAND_TREE);

		if (i_mod == BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE - 1) {
			__BENCHMARK_START__(BS_BLC_XOF);
			for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN && e < MQOM2_PARAM_TAU; e++) {
				uint32_t e_mod = e - e_start;
				ret = xof_update(&xof_ctx[e_mod], (uint8_t*) ls_com[e_mod], BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE * MQOM2_PARAM_DIGEST_SIZE);
				ERR(ret, err);
			}
			__BENCHMARK_STOP__(BS_BLC_XOF);
		}
	}

	for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN && e < MQOM2_PARAM_TAU; e++) {
		uint32_t e_mod = e - e_start;

		__BENCHMARK_START__(BS_BLC_ARITH);
#ifndef BLC_NO_FAST_FOLDING
		memset(x0[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_N));
		for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
			field_base_parse(data_folding[e_mod][j], MQOM2_PARAM_MQ_N, bar_x);
			field_ext_base_constant_vect_mult((1 << j), bar_x, tmp_n, MQOM2_PARAM_MQ_N);
			field_ext_vect_add(x0[e_mod], tmp_n, x0[e_mod], MQOM2_PARAM_MQ_N);
		}

		memset(u0[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA));
		for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
			field_ext_parse(data_folding[e_mod][j] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, bar_u);
			field_ext_constant_vect_mult((1 << j), bar_u, tmp_eta, MQOM2_PARAM_ETA);
			field_ext_vect_add(u0[e_mod], tmp_eta, u0[e_mod], MQOM2_PARAM_ETA);
		}
#endif

		field_base_parse(acc[e_mod], MQOM2_PARAM_MQ_N, acc_x);
		field_ext_parse(acc[e_mod] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, u1[e_mod]);

		field_base_elt delta_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
		uint8_t serialized_delta_x[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)];
		field_base_vect_add(x, acc_x, delta_x, MQOM2_PARAM_MQ_N);
		field_base_serialize(delta_x, MQOM2_PARAM_MQ_N, serialized_delta_x);
		memcpy(all_delta_x[e_mod], serialized_delta_x, BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N));
		__BENCHMARK_STOP__(BS_BLC_ARITH);

		__BENCHMARK_START__(BS_BLC_XOF);
		ret = xof_squeeze(&xof_ctx[e_mod], hash_ls_com[e_mod], MQOM2_PARAM_DIGEST_SIZE);
		ERR(ret, err);
		__BENCHMARK_STOP__(BS_BLC_XOF);
	}

	ret = 0;
err:
	for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN && e < MQOM2_PARAM_TAU; e++) {
		uint32_t e_mod = e - e_start;
		if(e_mod % 2 == 0)
			destroy_prg_cache_x2(prg_cache[e_mod/2]);
		seedcommit_clean_ctx(&seedcommit_ctx[e_mod]);
		xof_clean_ctx(&xof_ctx[e_mod]);
	}
	return ret;
}

int BLC_Commit_onetree_memopt(const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], blc_key_onetree_memopt_t* key, field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt u1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t e;

	/* Compute delta & Root seed */
	DeriveTreeRandomness(mseed, key->delta, key->rseed);
	memcpy(key->salt, salt, MQOM2_PARAM_SALT_SIZE);

	xof_context DECL_VAR(xof_ctx_hash_ls_com);
	ret = xof_init(&xof_ctx_hash_ls_com);
	ERR(ret, err);
	ret = xof_update(&xof_ctx_hash_ls_com, (const uint8_t*) "\x07", 1);
	ERR(ret, err);

	uint8_t hash_ls_com[BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN][MQOM2_PARAM_DIGEST_SIZE];
	for (e = 0; e < MQOM2_PARAM_TAU; e+=BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN) {
		ret = BLC_Commit_partial_onetree_memopt(e, key->delta, key->rseed, key->salt, x, hash_ls_com, &key->delta_x[e], &x0[e], &u0[e], &u1[e]);
		ERR(ret, err);

		__BENCHMARK_START__(BS_BLC_XOF);
		for (uint32_t e_mod = 0; e_mod < BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN && e + e_mod < MQOM2_PARAM_TAU; e_mod++) {
			ret = xof_update(&xof_ctx_hash_ls_com, hash_ls_com[e_mod], MQOM2_PARAM_DIGEST_SIZE);
			ERR(ret, err);
		}
		__BENCHMARK_STOP__(BS_BLC_XOF);
	}

	__BENCHMARK_START__(BS_BLC_XOF);
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		ret = xof_update(&xof_ctx_hash_ls_com, key->delta_x[e], BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N));
		ERR(ret, err);
	}
	ret = xof_squeeze(&xof_ctx_hash_ls_com, com1, MQOM2_PARAM_DIGEST_SIZE);
	ERR(ret, err);
	__BENCHMARK_STOP__(BS_BLC_XOF);

	ret = 0;
err:
	xof_clean_ctx(&xof_ctx_hash_ls_com);
	return ret;
}

int BLC_CheckChallenge_onetree_memopt(const uint16_t i_star[MQOM2_PARAM_TAU]) {
	uint32_t hidden_leaves[MQOM2_PARAM_TAU];
	DeriveHiddenLeaves(i_star, hidden_leaves);
	return GGMTreeMulti_CheckChallenge(hidden_leaves);
}

int BLC_Open_onetree_memopt(const blc_key_onetree_memopt_t* key, const uint16_t i_star[MQOM2_PARAM_TAU], uint8_t opening[MQOM2_PARAM_OPENING_ONETREE_SIZE]) {
	int ret = -1;
	uint32_t e, e_;
	seedcommit_ctx_t DECL_VAR(seedcommit_ctx);
    uint8_t lseed[MQOM2_PARAM_TAU][MQOM2_PARAM_SEED_SIZE];

	uint8_t (*path)[MQOM2_PARAM_SEED_SIZE] = (uint8_t(*)[MQOM2_PARAM_SEED_SIZE]) &opening[0];
	uint8_t *out_ls_com = &opening[MQOM2_PARAM_TOPEN * MQOM2_PARAM_SEED_SIZE];
	uint8_t *delta_x = &opening[MQOM2_PARAM_TOPEN * MQOM2_PARAM_SEED_SIZE + MQOM2_PARAM_TAU * MQOM2_PARAM_DIGEST_SIZE];

	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
#if defined(SUPERCOP)
		/* XXX: NOTE: we explicitly declassify i_star[e] as it is public data but comes from a dataflow involving secret data
		 * through hashing */
		crypto_declassify(&i_star[e], sizeof(i_star[e]));
#endif
	}
	uint32_t hidden_leaves[MQOM2_PARAM_TAU];
	DeriveHiddenLeaves(i_star, hidden_leaves);
	ret = GGMTreeMulti_Open(key->salt, key->rseed, key->delta, hidden_leaves, path, lseed);
	ERR(ret, err);

	for (e_ = 0; e_ < MQOM2_PARAM_TAU; e_++) {
		e = hidden_leaves[e_] % MQOM2_PARAM_TAU;
		ret = init_seedcommit(&seedcommit_ctx, key->salt, e);
		ERR(ret, err);
		SeedCommit(&seedcommit_ctx, lseed[e_], &out_ls_com[e * MQOM2_PARAM_DIGEST_SIZE]);

		memcpy(&delta_x[e * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)], key->delta_x[e], BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N));
	}

	ret = 0;
err:
	seedcommit_clean_ctx(&seedcommit_ctx);
	return ret;
}

int BLC_Eval_partial_onetree_memopt(uint32_t e_start, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t opening[MQOM2_PARAM_OPENING_ONETREE_SIZE], const uint16_t i_star[MQOM2_PARAM_TAU], const uint32_t hidden_leaves[], uint8_t hash_ls_com[][MQOM2_PARAM_DIGEST_SIZE], field_ext_elt x_eval[][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u_eval[][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t e, i;
	seedcommit_ctx_pub_t DECL_VAR(seedcommit_ctx[BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY]);
	prg_key_sched_cache_pub *prg_cache[BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY] = {NULL};

	const uint8_t (*path)[MQOM2_PARAM_SEED_SIZE] = (const uint8_t(*)[MQOM2_PARAM_SEED_SIZE]) &opening[0];
	const uint8_t *out_ls_com = &opening[MQOM2_PARAM_TOPEN * MQOM2_PARAM_SEED_SIZE];
	const uint8_t *all_delta_x = &opening[MQOM2_PARAM_TOPEN * MQOM2_PARAM_SEED_SIZE + MQOM2_PARAM_TAU * MQOM2_PARAM_DIGEST_SIZE];

	uint8_t lseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t ls_com[BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY][BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE][MQOM2_PARAM_DIGEST_SIZE];
	uint8_t exp[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	field_base_elt bar_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt bar_u[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt tmp_n[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)];
	field_ext_elt tmp_eta[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_base_elt acc_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
#ifndef BLC_NO_FAST_FOLDING
	uint8_t data_folding[BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY][MQOM2_PARAM_NB_EVALS_LOG][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
#endif
	uint8_t acc[BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) + BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA)];
	xof_context DECL_VAR(xof_ctx[BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY]);

	gmmtreemulti_ctx_partial_t ggm_tree;
	ret = GGMTreeMulti_InitIteratorPartial(&ggm_tree, salt, path, hidden_leaves);
	ERR(ret, err);

	for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY && e < MQOM2_PARAM_TAU; e++) {
		uint32_t e_mod = e - e_start;

		/* Initialize the PRG cache when used */
#ifndef NO_BLC_PRG_CACHE
		prg_cache[e_mod] = init_prg_cache_pub(PRG_BLC_SIZE);
#endif

		ret = init_seedcommit_pub(&seedcommit_ctx[e_mod], salt, e);
		ERR(ret, err);

		ret = xof_init(&xof_ctx[e_mod]);
		ERR(ret, err);
		ret = xof_update(&xof_ctx[e_mod], (const uint8_t*) "\x06", 1);
		ERR(ret, err);

#ifndef BLC_NO_FAST_FOLDING
		memset((uint8_t*) data_folding[e_mod], 0, sizeof(data_folding[e_mod]));
		memset((uint8_t*) acc[e_mod], 0, sizeof(acc[e_mod]));
#else
		memset(x_eval[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_N));
		memset(u_eval[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA));
		memset((uint8_t*) acc[e_mod], 0, sizeof(acc[e_mod]));
#endif
	}

	for (i = 0; i < MQOM2_PARAM_NB_EVALS; i++) {
		uint32_t i_mod = i % BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE;

		for (e = 0; e < MQOM2_PARAM_TAU; e++) {
			ret = GGMTreeMulti_GetNext(&ggm_tree.iterator, lseed);
			if(ret && (ret != -2 || i != i_star[e])) {
				goto err;
			}
			if(!ret && i == i_star[e]) {
				ret = -5;
				goto err;
			}

			if (e < e_start || e >= e_start + BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY) {
				continue;
			}
			uint32_t e_mod = e - e_start;

			if (i != i_star[e]) {
				ret = PRG_pub(salt, e, lseed, PRG_BLC_SIZE, exp, prg_cache[e_mod]);
				ERR(ret, err);

				SeedCommit_pub(&seedcommit_ctx[e_mod], lseed, ls_com[e_mod][i_mod]);
			} else {
				memset(exp, 0, PRG_BLC_SIZE);
				memcpy(ls_com[e_mod][i_mod], &out_ls_com[e * MQOM2_PARAM_DIGEST_SIZE], MQOM2_PARAM_DIGEST_SIZE);
			}

			field_base_vect_add(acc[e_mod], exp, acc[e_mod], MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);

			uint8_t j = get_gray_code_bit_position(i);
#ifndef BLC_NO_FAST_FOLDING
			field_base_vect_add(data_folding[e_mod][j], acc[e_mod], data_folding[e_mod][j], MQOM2_PARAM_MQ_N + MQOM2_PARAM_ETA * MQOM2_PARAM_MU);
#else
			field_base_parse(acc[e_mod], MQOM2_PARAM_MQ_N, bar_x);
			field_ext_base_constant_vect_mult((1 << j), bar_x, tmp_n, MQOM2_PARAM_MQ_N);
			field_ext_vect_add(x_eval[e_mod], tmp_n, x_eval[e_mod], MQOM2_PARAM_MQ_N);

			field_ext_parse(acc[e_mod] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, bar_u);
			field_ext_constant_vect_mult((1 << j), bar_u, tmp_eta, MQOM2_PARAM_ETA);
			field_ext_vect_add(u_eval[e_mod], tmp_eta, u_eval[e_mod], MQOM2_PARAM_ETA);
#endif
		}

		if (i_mod == BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE - 1) {
			for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY && e < MQOM2_PARAM_TAU; e++) {
				uint32_t e_mod = e - e_start;
				ret = xof_update(&xof_ctx[e_mod], (uint8_t*) ls_com[e_mod], BLC_NB_SEED_COMMITMENTS_PER_HASH_UPDATE * MQOM2_PARAM_DIGEST_SIZE);
				ERR(ret, err);
			}
		}
	}

	for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY && e < MQOM2_PARAM_TAU; e++) {
		uint32_t e_mod = e - e_start;

#ifndef BLC_NO_FAST_FOLDING
		memset(x_eval[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_MQ_N));
		for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
			field_base_parse(data_folding[e_mod][j], MQOM2_PARAM_MQ_N, bar_x);
			field_ext_base_constant_vect_mult((1 << j), bar_x, tmp_n, MQOM2_PARAM_MQ_N);
			field_ext_vect_add(x_eval[e_mod], tmp_n, x_eval[e_mod], MQOM2_PARAM_MQ_N);
		}

		memset(u_eval[e_mod], 0, BYTE_SIZE_FIELD_EXT(MQOM2_PARAM_ETA));
		for (uint32_t j = 0; j < MQOM2_PARAM_NB_EVALS_LOG; j++) {
			field_ext_parse(data_folding[e_mod][j] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, bar_u);
			field_ext_constant_vect_mult((1 << j), bar_u, tmp_eta, MQOM2_PARAM_ETA);
			field_ext_vect_add(u_eval[e_mod], tmp_eta, u_eval[e_mod], MQOM2_PARAM_ETA);
		}
#endif
		field_ext_elt r = get_evaluation_point(i_star[e]);

		field_base_elt delta_x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
		uint8_t serialized_delta_x[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)];
		memcpy(serialized_delta_x, &all_delta_x[e * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)], BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N));
		field_base_parse(serialized_delta_x, MQOM2_PARAM_MQ_N, delta_x);

		field_base_parse(acc[e_mod], MQOM2_PARAM_MQ_N, acc_x);
		field_base_vect_add(acc_x, delta_x, acc_x, MQOM2_PARAM_MQ_N);
		field_ext_base_constant_vect_mult(r, acc_x, tmp_n, MQOM2_PARAM_MQ_N);
		field_ext_vect_add(x_eval[e_mod], tmp_n, x_eval[e_mod], MQOM2_PARAM_MQ_N);

		field_ext_parse(acc[e_mod] + BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N), MQOM2_PARAM_ETA, tmp_eta);
		field_ext_constant_vect_mult(r, tmp_eta, tmp_eta, MQOM2_PARAM_ETA);
		field_ext_vect_add(u_eval[e_mod], tmp_eta, u_eval[e_mod], MQOM2_PARAM_ETA);

		ret = xof_squeeze(&xof_ctx[e_mod], hash_ls_com[e_mod], MQOM2_PARAM_DIGEST_SIZE);
		ERR(ret, err);
	}

	ret = 0;
err:
	for (e = e_start; e-e_start < BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY && e < MQOM2_PARAM_TAU; e++) {
		uint32_t e_mod = e - e_start;
		seedcommit_clean_ctx_pub(&seedcommit_ctx[e_mod]);
		destroy_prg_cache_pub(prg_cache[e_mod]);
		xof_clean_ctx(&xof_ctx[e_mod]);
	}
	return ret;
}


int BLC_Eval_onetree_memopt(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], const uint8_t opening[MQOM2_PARAM_OPENING_ONETREE_SIZE], const uint16_t i_star[MQOM2_PARAM_TAU], field_ext_elt x_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]) {
	int ret = -1;
	uint32_t e;
	uint8_t com1_[MQOM2_PARAM_DIGEST_SIZE];

	uint32_t hidden_leaves[MQOM2_PARAM_TAU];
	DeriveHiddenLeaves(i_star, hidden_leaves);

	xof_context DECL_VAR(xof_ctx_hash_ls_com);
	ret = xof_init(&xof_ctx_hash_ls_com);
	ERR(ret, err);
	ret = xof_update(&xof_ctx_hash_ls_com, (const uint8_t*) "\x07", 1);
	ERR(ret, err);

	uint8_t hash_ls_com[BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY][MQOM2_PARAM_DIGEST_SIZE];
	for (e = 0; e < MQOM2_PARAM_TAU; e+=BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY) {
		ret = BLC_Eval_partial_onetree_memopt(e, salt, opening, i_star, hidden_leaves, hash_ls_com, &x_eval[e], &u_eval[e]);
		ERR(ret, err);

		for (uint32_t e_mod = 0; e_mod < BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY && e + e_mod < MQOM2_PARAM_TAU; e_mod++) {
			ret = xof_update(&xof_ctx_hash_ls_com, hash_ls_com[e_mod], MQOM2_PARAM_DIGEST_SIZE);
			ERR(ret, err);
		}
	}

	const uint8_t *all_delta_x = &opening[MQOM2_PARAM_TOPEN * MQOM2_PARAM_SEED_SIZE + MQOM2_PARAM_TAU * MQOM2_PARAM_DIGEST_SIZE];

	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		ret = xof_update(&xof_ctx_hash_ls_com, &all_delta_x[e * BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)], BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N));
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
	xof_clean_ctx(&xof_ctx_hash_ls_com);
	return ret;
}

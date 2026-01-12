#include "ggm_tree_multi_2.h"

static inline void SeedDerive_x2(enc_ctx *ctx1, enc_ctx *ctx2, const uint8_t seed1[MQOM2_PARAM_SEED_SIZE], const uint8_t seed2[MQOM2_PARAM_SEED_SIZE],
                                 uint8_t new_seed1[MQOM2_PARAM_SEED_SIZE], uint8_t new_seed2[MQOM2_PARAM_SEED_SIZE]) {
	
	uint8_t linortho_seed1[MQOM2_PARAM_SEED_SIZE];
	uint8_t linortho_seed2[MQOM2_PARAM_SEED_SIZE];
	LinOrtho(seed1, linortho_seed1);
	LinOrtho(seed2, linortho_seed2);
	/* Encrypt the LinOrtho seed with the tweaked salt */
	enc_encrypt_x2(ctx1, ctx2, seed1, seed2, new_seed1, new_seed2);
	/* Xor with LinOrtho seed */
	xor_blocks(new_seed1, linortho_seed1, new_seed1);
	xor_blocks(new_seed2, linortho_seed2, new_seed2);

	return;
}

static inline int SeedBlockDerive(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], uint32_t node_index, const uint8_t seed0[MQOM2_PARAM_SEED_SIZE], const uint8_t seed1[MQOM2_PARAM_SEED_SIZE], uint8_t child[4][MQOM2_PARAM_SEED_SIZE]) {
	uint8_t tweaked_salt[MQOM2_PARAM_SALT_SIZE];
	enc_ctx DECL_VAR(ctx_enc0), DECL_VAR(ctx_enc1);
	int ret = -1;

	TweakSalt(salt, tweaked_salt, 2, (node_index - 1) & 0xFF, (node_index - 1) >> 8);
	ret = enc_key_sched(&ctx_enc0, tweaked_salt);
	ERR(ret, err);
	TweakSalt(salt, tweaked_salt, 2, (node_index) & 0xFF, (node_index) >> 8);
	ret = enc_key_sched(&ctx_enc1, tweaked_salt);
	ERR(ret, err);
	SeedDerive_x2(&ctx_enc0, &ctx_enc1, seed0, seed1, child[0], child[2]);

	xor_blocks(child[0], seed0, child[1]);
	xor_blocks(child[2], seed1, child[3]);

	ret = 0;
err:
	enc_clean_ctx(&ctx_enc0);
	enc_clean_ctx(&ctx_enc1);
	return ret;
}

static inline iternode_2_t* GMMTreeMulti_AddNodeInStack_2(tree_iterator_2_t* it, const uint8_t root[4][MQOM2_PARAM_SEED_SIZE], uint16_t node_index) {
	iternode_2_t* node = &it->stack[it->stack_len];
	memcpy(node->seed, root, 4*MQOM2_PARAM_SEED_SIZE);
	node->node_index = node_index;
	node->status = 0;
	it->stack_len++;
	return node;
}

static inline iternode_2_t* GMMTreeMulti_GetStackHead_2(tree_iterator_2_t* it) {
	return &it->stack[it->stack_len - 1];
}

int GGMTreeMulti_ComputeNextBlockWithIndex_2(tree_iterator_2_t* it) {
	uint8_t child[4][MQOM2_PARAM_SEED_SIZE];
	int ret = -1;

	// Ascent in the tree
	while ((it->stack_len > 0) && (GMMTreeMulti_GetStackHead_2(it)->status == 4)) {
		it->stack_len--;
	}

	if (it->stack_len == 0) {
		ret = -3;
		goto err;
	}

	// Descent in the tree
	iternode_2_t *stack_head = GMMTreeMulti_GetStackHead_2(it);
	while (stack_head->node_index < MQOM2_PARAM_NB_EVALS * MQOM2_PARAM_TAU) {
		//printf("Head: %d (%d)\n", stack_head->node_index, stack_head->status);
		if (stack_head->status == 0) {

			ret = SeedBlockDerive(it->salt, stack_head->node_index, stack_head->seed[0], stack_head->seed[1], child);
			ERR(ret, err);

			GMMTreeMulti_AddNodeInStack_2(it, child, 2 * stack_head->node_index);
			stack_head->status = 1;

		} else if (stack_head->status == 1) {
			
			ret = SeedBlockDerive(it->salt, stack_head->node_index+2, stack_head->seed[2], stack_head->seed[3], child);
			ERR(ret, err);

			GMMTreeMulti_AddNodeInStack_2(it, child, 2 * (stack_head->node_index+2));
			stack_head->status = 4;
		}

		stack_head = GMMTreeMulti_GetStackHead_2(it);
	}
	//printf("Exit Loop Iteration\n");

	stack_head->status = 4;
	ret = 0;
err:
	return ret;
}

int GGMTreeMulti_GetNextWithIndex_2(tree_iterator_2_t* it, uint8_t node[MQOM2_PARAM_SEED_SIZE], uint32_t* node_index) {
	int ret = -1;
	if(it->counter == 0) {
		ret = GGMTreeMulti_ComputeNextBlockWithIndex_2(it);
		ERR(ret, err);
	}
	iternode_2_t *stack_head = GMMTreeMulti_GetStackHead_2(it);
	memcpy(node, stack_head->seed[it->counter], MQOM2_PARAM_SEED_SIZE);
	if (node_index != NULL) {
		*node_index = stack_head->node_index + it->counter;
	}

	it->counter = (it->counter + 1) % 4;
	ret = 0;
err:
	return ret;
}

int GGMTreeMulti_GetNext_2(tree_iterator_2_t* it, uint8_t node[MQOM2_PARAM_SEED_SIZE]) {
	return GGMTreeMulti_GetNextWithIndex_2(it, node, NULL);
}

int GGMTreeMulti_InitIterator_2(gmmtreemulti_ctx_2_t* ctx, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t delta[MQOM2_PARAM_SEED_SIZE]) {
	int ret = -1;

	// Initialize the iterator
	memcpy(ctx->iterator.salt, salt, MQOM2_PARAM_SALT_SIZE);
	memcpy(ctx->iterator.rseed, rseed, MQOM2_PARAM_SEED_SIZE);
	ctx->iterator.stack_len = 0;
	ctx->iterator.counter = 0;

	uint8_t child[4][MQOM2_PARAM_SEED_SIZE];
	uint8_t brother[MQOM2_PARAM_SEED_SIZE];
	xor_blocks(rseed, delta, brother);
	ret = SeedBlockDerive(salt, 2, rseed, brother, child);
	ERR(ret, err);

	// Pre-fill the stack
	GMMTreeMulti_AddNodeInStack_2(&ctx->iterator, child, 4);

	ret = 0;
err:
	return 0;
}

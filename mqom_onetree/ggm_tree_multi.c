#include "ggm_tree_multi.h"

#define printf(...) {}

/* SeedDerive variants
 * NOTE: we factorize the key schedule, the tweaked salt is inside the encryption context */
static inline int SeedDerive(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], uint32_t node_index, const uint8_t seed[MQOM2_PARAM_SEED_SIZE], uint8_t new_seed[MQOM2_PARAM_SEED_SIZE]) {
	int ret = -1;
	uint8_t tweaked_salt[MQOM2_PARAM_SALT_SIZE];
	enc_ctx DECL_VAR(ctx_enc);

	TweakSalt(salt, tweaked_salt, 2, (node_index - 1) & 0xFF,  (node_index - 1) >> 8);
	ret = enc_key_sched(&ctx_enc, tweaked_salt);
	ERR(ret, err);

	uint8_t linortho_seed[MQOM2_PARAM_SEED_SIZE];
	LinOrtho(seed, linortho_seed);
	/* Encrypt the LinOrtho seed with the tweaked salt */
	enc_encrypt(&ctx_enc, seed, new_seed);
	/* Xor with LinOrtho seed */
	xor_blocks(new_seed, linortho_seed, new_seed);

	ret = 0;
err:
	enc_clean_ctx(&ctx_enc);
	return ret;
}
static inline int SeedDerive_pub(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], uint32_t node_index, const uint8_t seed[MQOM2_PARAM_SEED_SIZE], uint8_t new_seed[MQOM2_PARAM_SEED_SIZE]) {
	int ret = -1;
	uint8_t tweaked_salt[MQOM2_PARAM_SALT_SIZE];
	enc_ctx_pub DECL_VAR(ctx_enc);

	TweakSalt(salt, tweaked_salt, 2, (node_index - 1) & 0xFF,  (node_index - 1) >> 8);
	ret = enc_key_sched_pub(&ctx_enc, tweaked_salt);
	ERR(ret, err);

	uint8_t linortho_seed[MQOM2_PARAM_SEED_SIZE];
	LinOrtho(seed, linortho_seed);
	/* Encrypt the LinOrtho seed with the tweaked salt */
	enc_encrypt_pub(&ctx_enc, seed, new_seed);
	/* Xor with LinOrtho seed */
	xor_blocks(new_seed, linortho_seed, new_seed);

	ret = 0;
err:
	enc_clean_ctx_pub(&ctx_enc);
	return ret;
}

static inline iternode_t* GMMTreeMulti_AddNodeInStack(tree_iterator_t* it, const uint8_t root[MQOM2_PARAM_SEED_SIZE], uint16_t node_index) {
	iternode_t* node = &it->stack[it->stack_len];
	memcpy(node->seed, root, MQOM2_PARAM_SEED_SIZE);
	node->node_index = node_index;
	node->status = 0;
	it->stack_len++;
	return node;
}

static inline iternode_t* GMMTreeMulti_AddNodeInStackWithoutSeed(tree_iterator_t* it, uint16_t node_index) {
	iternode_t* node = &it->stack[it->stack_len];
	node->node_index = node_index;
	node->status = 2;
	it->stack_len++;
	return node;
}

static inline iternode_t* GMMTreeMulti_GetStackHead(tree_iterator_t* it) {
	return &it->stack[it->stack_len - 1];
}

int GGMTreeMulti_GetNextWithIndex(tree_iterator_t* it, uint8_t node[MQOM2_PARAM_SEED_SIZE], uint32_t* node_index) {
	uint8_t child[MQOM2_PARAM_SEED_SIZE];
	int ret = -1;

	// Ascent in the tree
	while ((it->stack_len > 0) && (GMMTreeMulti_GetStackHead(it)->status == 4)) {
		it->stack_len--;
	}

	if (it->stack_len == 0) {
		ret = -3;
		goto err;
	}

	// Descent in the tree
	iternode_t *stack_head = GMMTreeMulti_GetStackHead(it);
	while (stack_head->node_index < MQOM2_PARAM_NB_EVALS * MQOM2_PARAM_TAU) {
		//printf("Head: %d (%d)\n", stack_head->node_index, stack_head->status);
		if (stack_head->status == 0) {
			
			//printf("Start Status 0\n");
			// Derive the left child
			if(stack_head->node_index == 1) {
				memcpy(child, it->rseed, MQOM2_PARAM_SEED_SIZE);
			} else {
				if (it->is_public) {
					ret = SeedDerive_pub(it->salt, stack_head->node_index, stack_head->seed, child);
				} else {
					ret = SeedDerive(it->salt, stack_head->node_index, stack_head->seed, child);
				}
				ERR(ret, err);
			}

			// Add the next node in the stack
			GMMTreeMulti_AddNodeInStack(it, child, 2 * stack_head->node_index);
			stack_head->status = 1;
			//printf("End Status 0\n");

		} else if (stack_head->status == 1) {
			// Derive the right child
			//   Assuming that the left child is still in memory
			//   in it->stack[it->stack_len]
			xor_blocks(stack_head->seed, it->stack[it->stack_len].seed, child);

			// Add the next node in the stack
			GMMTreeMulti_AddNodeInStack(it, child, 2 * stack_head->node_index + 1);
			stack_head->status = 4;

		} else if (stack_head->status == 2) {
			GMMTreeMulti_AddNodeInStackWithoutSeed(it, 2 * stack_head->node_index);
			stack_head->status = 3;

		} else if (stack_head->status == 3) {
			GMMTreeMulti_AddNodeInStackWithoutSeed(it, 2 * stack_head->node_index + 1);
			stack_head->status = 4;
		}

		stack_head = GMMTreeMulti_GetStackHead(it);
		if (it->treat_node) {
			//printf("Start Treat Node\n");
			if (it->treat_node(it->data, stack_head)) {
				break;
			}
			//printf("End Treat Node\n");
		}
	}
	//printf("Exit Loop Iteration\n");

	if (node_index != NULL) {
		*node_index = stack_head->node_index;
	}

	if (stack_head->status != 0) {
		ret = -2;
	} else {
		memcpy(node, stack_head->seed, MQOM2_PARAM_SEED_SIZE);
		ret = 0;
	}
	stack_head->status = 4;
err:
	return ret;
}

int GGMTreeMulti_GetNext(tree_iterator_t* it, uint8_t node[MQOM2_PARAM_SEED_SIZE]) {
	return GGMTreeMulti_GetNextWithIndex(it, node, NULL);
}

int GGMTreeMulti_InitIterator(gmmtreemulti_ctx_t* ctx, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t delta[MQOM2_PARAM_SEED_SIZE]) {
	// Initialize the iterator
	memcpy(ctx->iterator.salt, salt, MQOM2_PARAM_SALT_SIZE);
	memcpy(ctx->iterator.rseed, rseed, MQOM2_PARAM_SEED_SIZE);
	ctx->iterator.stack_len = 0;
	ctx->iterator.treat_node = NULL;
	ctx->iterator.data = NULL;
	ctx->iterator.is_public = 0;

	// Pre-fill the stack
	GMMTreeMulti_AddNodeInStack(&ctx->iterator, delta, 1);
	return 0;
}

static inline int get_hidden_nodes(const uint32_t hidden_leaves[MQOM2_PARAM_TAU], uint32_t hidden_nodes[MQOM2_PARAM_TAU * (MQOM2_PARAM_TREE_HEIGHT + 1)], uint32_t* nb_hidden_nodes, uint32_t* size_sibling_path) {
	uint32_t e, j;
	uint32_t last_path_to_root[MQOM2_PARAM_TREE_HEIGHT + 1] = {0};
	*nb_hidden_nodes = 0;
	*size_sibling_path = 0;
	for (e = 0; e < MQOM2_PARAM_TAU; e++) {
		uint32_t current_node_index, depth;
		if(hidden_leaves[e] < ((2*MQOM2_PARAM_NB_EVALS * MQOM2_PARAM_TAU) - (1<<MQOM2_PARAM_TREE_HEIGHT))) {
			current_node_index = (1<<MQOM2_PARAM_TREE_HEIGHT) + hidden_leaves[e];
			depth = MQOM2_PARAM_TREE_HEIGHT;
		} else {
			current_node_index = (1<<MQOM2_PARAM_TREE_HEIGHT) + hidden_leaves[e] - (MQOM2_PARAM_NB_EVALS * MQOM2_PARAM_TAU);
			depth = MQOM2_PARAM_TREE_HEIGHT - 1;
		}
		for (j = 0; (j <= depth) && (last_path_to_root[depth - j] != current_node_index); j++) {
			last_path_to_root[depth - j] = current_node_index;
			(*size_sibling_path)++;
			current_node_index /= 2; // Go the parent child
		}
		(*size_sibling_path)--;
		if(e>0) (*size_sibling_path)--;
		for (j = (depth + 1) - j; j <= depth; j++) {
			hidden_nodes[*nb_hidden_nodes] = last_path_to_root[j];
			(*nb_hidden_nodes)++;
		}
		printf("%d: %d vs %d\n", e, *nb_hidden_nodes, *size_sibling_path);
	}
	return 0;
}

int treat_node_opening(void* it_data_, iternode_t* node) {
	opening_iterator_data_t *it_data = (opening_iterator_data_t*) it_data_;
	if ((it_data->pos < it_data->nb_hidden_nodes) && (node->node_index == it_data->hidden_nodes[it_data->pos])) {
		printf("Treat: %d (hidden)\n", node->node_index);
		it_data->pos++;
	} else if ((it_data->size_sibling_path < MQOM2_PARAM_TOPEN) && (node->node_index < MQOM2_PARAM_NB_EVALS * MQOM2_PARAM_TAU)) {
		printf("Treat: %d (public: %d)\n", node->node_index, it_data->size_sibling_path);
		it_data->size_sibling_path++;
	} else {
		printf("Treat: %d (in path)\n", node->node_index);
		return 1; // Iterator should output this (potentially internal) node
	}
	return 0;
}

int GGMTreeMulti_CheckChallenge(const uint32_t hidden_leaves[MQOM2_PARAM_TAU]) {
	opening_iterator_data_t it_data;
	get_hidden_nodes(hidden_leaves, it_data.hidden_nodes, &it_data.nb_hidden_nodes, &it_data.size_sibling_path);
	return (it_data.size_sibling_path > MQOM2_PARAM_TOPEN);
}

int GGMTreeMulti_Open(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t delta[MQOM2_PARAM_SEED_SIZE], const uint32_t hidden_leaves[MQOM2_PARAM_TAU], uint8_t path[MQOM2_PARAM_TOPEN][MQOM2_PARAM_SEED_SIZE], uint8_t lseed[MQOM2_PARAM_TAU][MQOM2_PARAM_SEED_SIZE]) {

	int ret = -1;

	opening_iterator_data_t it_data;
	printf("Start get_hidden_nodes\n");
	get_hidden_nodes(hidden_leaves, it_data.hidden_nodes, &it_data.nb_hidden_nodes, &it_data.size_sibling_path);
	printf("End get_hidden_nodes\n");
	printf("it_data.hidden_nodes[0]=%d\n", it_data.hidden_nodes[0]);
	printf("it_data.hidden_nodes[1]=%d\n", it_data.hidden_nodes[1]);
	printf("it_data.nb_hidden_nodes = %d\n", it_data.nb_hidden_nodes);
	printf("it_data.size_sibling_path = %d\n", it_data.size_sibling_path);
	printf("topen = %d\n", MQOM2_PARAM_TOPEN);

	it_data.pos = 1;
	uint32_t pos_leaf = 0;

	// Initialize the iterator
	tree_iterator_t iterator;
	memcpy(iterator.salt, salt, MQOM2_PARAM_SALT_SIZE);
	memcpy(iterator.rseed, rseed, MQOM2_PARAM_SEED_SIZE);
	iterator.stack_len = 0;
	iterator.treat_node = treat_node_opening;
	iterator.data = (void*) &it_data;
	iterator.is_public = 0;

	// Pre-fill the stack
	GMMTreeMulti_AddNodeInStack(&iterator, delta, 1);

	uint32_t node_index;
	uint8_t node[MQOM2_PARAM_SEED_SIZE];
	for (uint32_t i = 0; i < MQOM2_PARAM_TOPEN + MQOM2_PARAM_TAU; i++) {
        printf(" - %d\n", i);
		ret = GGMTreeMulti_GetNextWithIndex(&iterator, node, &node_index);
		ERR(ret, err);
		printf("GGMTreeMulti_GetNext: %d\n", ret);
		printf("it_data.pos = %d\n", it_data.pos);

		// it_data.pos can not be 0, because it has seen at least the tree root.
		uint32_t last_hidden_node_index = it_data.hidden_nodes[it_data.pos - 1];
		if (node_index == last_hidden_node_index) {
			printf("Hidden node: %d\n", node_index);
			memcpy(lseed[pos_leaf], node, MQOM2_PARAM_SEED_SIZE);
			pos_leaf++;
		} else {
			printf("Add to path: %d\n", node_index);
			memcpy(path[i-pos_leaf], node, MQOM2_PARAM_SEED_SIZE);
		}
	}

	ret = 0;
err:
	return ret;
}

int treat_node_partial_expand(void* it_data_, iternode_t* node) {
	partial_expand_iterator_data_t *it_data = (partial_expand_iterator_data_t*) it_data_;
	if ((it_data->pos < it_data->nb_hidden_nodes) && (node->node_index == it_data->hidden_nodes[it_data->pos])) {
		printf("Treat: %d (hidden)\n", node->node_index);
		it_data->pos++;
	} else if ((it_data->size_sibling_path < MQOM2_PARAM_TOPEN) && (node->node_index < MQOM2_PARAM_NB_EVALS * MQOM2_PARAM_TAU)) {
		printf("Treat: %d (public: %d)\n", node->node_index, it_data->size_sibling_path);
		it_data->size_sibling_path++;
	} else if (node->status == 2) {
		printf("Treat: %d (in path)\n", node->node_index);
		memcpy(node->seed, it_data->path[it_data->pos_in_path], MQOM2_PARAM_SEED_SIZE);
		it_data->pos_in_path++;
		node->status = 0;
	}
	return 0;
}

int GGMTreeMulti_InitIteratorPartial(gmmtreemulti_ctx_partial_t* ctx, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t path[MQOM2_PARAM_TOPEN][MQOM2_PARAM_SEED_SIZE], const uint32_t hidden_leaves[MQOM2_PARAM_TAU]) {

	get_hidden_nodes(hidden_leaves, ctx->data.hidden_nodes, &ctx->data.nb_hidden_nodes, &ctx->data.size_sibling_path);
	ctx->data.pos = 1;
	ctx->data.path = path;
	ctx->data.pos_in_path = 0;

	// Initialize the iterator
	memcpy(ctx->iterator.salt, salt, MQOM2_PARAM_SALT_SIZE);
	ctx->iterator.stack_len = 0;
	ctx->iterator.treat_node = treat_node_partial_expand;
	ctx->iterator.data = (void*) &ctx->data;
	ctx->iterator.is_public = 1;

	// Pre-fill the stack
	GMMTreeMulti_AddNodeInStackWithoutSeed(&ctx->iterator, 1);
	return 0;
}

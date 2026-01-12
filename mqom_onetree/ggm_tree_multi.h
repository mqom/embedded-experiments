#ifndef __GGM_TREE_MULTI_H__
#define __GGM_TREE_MULTI_H__

/* MQOM2 parameters */
#include "mqom2_parameters.h"
/* Encryption primitive */
#include "enc.h"
/* Common helpers */
#include "common.h"

/* Deal with namespacing */
#define GGMTreeMulti_GetNext MQOM_NAMESPACE(GGMTreeMulti_GetNext)
#define GGMTreeMulti_InitIterator MQOM_NAMESPACE(GGMTreeMulti_InitIterator)
#define GGMTreeMulti_Open MQOM_NAMESPACE(GGMTreeMulti_Open)
#define GGMTreeMulti_InitIteratorPartial MQOM_NAMESPACE(GGMTreeMulti_InitIteratorPartial)

typedef struct {
	uint8_t seed[MQOM2_PARAM_SEED_SIZE];
	uint16_t node_index;
	uint8_t status;
} iternode_t;

typedef struct {
	iternode_t stack[MQOM2_PARAM_TREE_HEIGHT + 1];
	uint32_t stack_len;
	uint8_t salt[MQOM2_PARAM_SALT_SIZE];
	uint8_t rseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t is_public;

	int (*treat_node)(void*, iternode_t*);
	void *data;
} tree_iterator_t;

int GGMTreeMulti_GetNext(tree_iterator_t* it, uint8_t node[MQOM2_PARAM_SEED_SIZE]);

typedef struct {
	tree_iterator_t iterator;
} gmmtreemulti_ctx_t;

int GGMTreeMulti_InitIterator(gmmtreemulti_ctx_t* ctx, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t delta[MQOM2_PARAM_SEED_SIZE]);

typedef struct {
	uint32_t hidden_nodes[MQOM2_PARAM_TAU * (MQOM2_PARAM_TREE_HEIGHT + 1)];
	uint32_t nb_hidden_nodes;
	uint32_t size_sibling_path;
	uint32_t pos;
} opening_iterator_data_t;

int GGMTreeMulti_CheckChallenge(const uint32_t hidden_leaves[MQOM2_PARAM_TAU]);

int GGMTreeMulti_Open(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t delta[MQOM2_PARAM_SEED_SIZE], const uint32_t hidden_leaves[MQOM2_PARAM_TAU], uint8_t path[MQOM2_PARAM_TOPEN][MQOM2_PARAM_SEED_SIZE], uint8_t lseed[MQOM2_PARAM_TAU][MQOM2_PARAM_SEED_SIZE]);

typedef struct {
	uint32_t hidden_nodes[MQOM2_PARAM_TAU * (MQOM2_PARAM_TREE_HEIGHT + 1)];
	uint32_t nb_hidden_nodes;
	uint32_t size_sibling_path;
	uint32_t pos;

	const uint8_t (*path)[MQOM2_PARAM_SEED_SIZE];
	uint32_t pos_in_path;
} partial_expand_iterator_data_t;

typedef struct {
	partial_expand_iterator_data_t data;
	tree_iterator_t iterator;
} gmmtreemulti_ctx_partial_t;

int GGMTreeMulti_InitIteratorPartial(gmmtreemulti_ctx_partial_t* ctx, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t path[MQOM2_PARAM_TOPEN][MQOM2_PARAM_SEED_SIZE], const uint32_t hidden_leaves[MQOM2_PARAM_TAU]);

#endif /* __GGM_TREE_MULTI_H__ */

#ifndef __GGM_TREE_MULTI_2_H__
#define __GGM_TREE_MULTI_2_H__

/* MQOM2 parameters */
#include "mqom2_parameters.h"
/* Encryption primitive */
#include "enc.h"
/* Common helpers */
#include "common.h"

/* Deal with namespacing */
#define GGMTreeMulti_GetNext_2 MQOM_NAMESPACE(GGMTreeMulti_GetNext_2)
#define GGMTreeMulti_InitIterator_2 MQOM_NAMESPACE(GGMTreeMulti_InitIterator_2)

typedef struct {
	uint8_t seed[4][MQOM2_PARAM_SEED_SIZE];
	uint16_t node_index;
	uint8_t status;
} iternode_2_t;

typedef struct {
	iternode_2_t stack[MQOM2_PARAM_TREE_HEIGHT - 1];
	uint32_t stack_len;
	uint32_t counter;

	uint8_t salt[MQOM2_PARAM_SALT_SIZE];
	uint8_t rseed[MQOM2_PARAM_SEED_SIZE];

	int (*treat_node)(void*, iternode_2_t*);
	void *data;
} tree_iterator_2_t;

int GGMTreeMulti_GetNext_2(tree_iterator_2_t* it, uint8_t node[MQOM2_PARAM_SEED_SIZE]);

typedef struct {
	tree_iterator_2_t iterator;
} gmmtreemulti_ctx_2_t;

int GGMTreeMulti_InitIterator_2(gmmtreemulti_ctx_2_t* ctx, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t delta[MQOM2_PARAM_SEED_SIZE]);

#endif /* __GGM_TREE_MULTI_2_H__ */

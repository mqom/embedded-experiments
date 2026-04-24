#ifndef __BLC_ONETREE_MEMOPT_H__
#define __BLC_ONETREE_MEMOPT_H__

/* MQOM2 parameters */
#include "mqom2_parameters.h"
#include "enc.h"
#include "prg.h"
#include "xof.h"
#include "fields.h"

#ifndef BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN
#define BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN MQOM2_PARAM_TAU
#endif
#ifndef BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY
#define BLC_ONETREE_NB_PARALLEL_REPETITIONS_VERIFY MQOM2_PARAM_TAU
#endif

#if (BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN == 0) || (BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN > MQOM2_PARAM_TAU)
#error "BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN must be > 0 and <= MQOM2_PARAM_TAU."
#endif
#if (BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN == 0) || (BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN > MQOM2_PARAM_TAU)
#error "BLC_ONETREE_NB_PARALLEL_REPETITIONS_SIGN must be > 0 and <= MQOM2_PARAM_TAU."
#endif

/* Deal with namespacing */
#define BLC_Commit_onetree_memopt MQOM_NAMESPACE(BLC_Commit_onetree_memopt)
#define BLC_Open_onetree_memopt MQOM_NAMESPACE(BLC_Open_onetree_memopt)
#define BLC_Eval_onetree_memopt MQOM_NAMESPACE(BLC_Eval_onetree_memopt)

typedef struct blc_key_onetree_memopt_t {
	uint8_t salt[MQOM2_PARAM_SALT_SIZE];
	uint8_t delta[MQOM2_PARAM_SEED_SIZE];
	uint8_t rseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t delta_x[MQOM2_PARAM_TAU][BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N)];
} blc_key_onetree_memopt_t;

int BLC_Commit_onetree_memopt(const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], blc_key_onetree_memopt_t* key, field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt u1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

int BLC_CheckChallenge_onetree_memopt(const uint16_t i_star[MQOM2_PARAM_TAU]);

int BLC_Open_onetree_memopt(const blc_key_onetree_memopt_t* key, const uint16_t i_star[MQOM2_PARAM_TAU], uint8_t opening[MQOM2_PARAM_OPENING_ONETREE_SIZE]);

int BLC_Eval_onetree_memopt(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], const uint8_t opening[MQOM2_PARAM_OPENING_ONETREE_SIZE], const uint16_t i_star[MQOM2_PARAM_TAU], field_ext_elt x_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

void BLC_PrintConfig_onetree_memopt();

#endif /* __BLC_ONETREE_MEMOPT_H__ */

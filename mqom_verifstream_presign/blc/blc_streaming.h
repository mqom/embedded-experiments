#ifndef __BLC_STREAMING_H__
#define __BLC_STREAMING_H__

/* MQOM2 parameters */
#include "mqom2_parameters.h"
#include "enc.h"
#include "prg.h"
#include "xof.h"
#include "fields.h"
#include "blc_common.h"

typedef struct blc_key_streaming_x1_t {
	uint8_t rseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t partial_delta_x[BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_MQ_N) - MQOM2_PARAM_SEED_SIZE];
} blc_key_streaming_x1_t;

int BLC_Commit_x1_streaming(uint32_t e, const uint8_t rseed[MQOM2_PARAM_SEED_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], uint8_t com[MQOM2_PARAM_DIGEST_SIZE], blc_key_streaming_x1_t* key, field_ext_elt x0[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u0[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt u1[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

int BLC_Open_x1_streaming(uint32_t e, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], const blc_key_streaming_x1_t* key, uint16_t i_star, uint8_t opening[MQOM2_PARAM_OPENING_X1_SIZE]);

int BLC_Eval_x1_streaming(uint32_t e, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t opening[MQOM2_PARAM_OPENING_X1_SIZE], uint16_t i_star, uint8_t com[MQOM2_PARAM_DIGEST_SIZE], field_ext_elt x_eval[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u_eval[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

typedef struct blc_key_streaming_t {
	uint8_t mseed[MQOM2_PARAM_SEED_SIZE];
	uint8_t salt[MQOM2_PARAM_SALT_SIZE];
	field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)];
	blc_key_streaming_x1_t subkey[MQOM2_PARAM_TAU];
} blc_key_streaming_t;

int BLC_Commit_streaming(const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], blc_key_streaming_t* key, field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt u1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

int BLC_Open_streaming(const blc_key_streaming_t* key, const uint16_t i_star[MQOM2_PARAM_TAU], uint8_t opening[MQOM2_PARAM_OPENING_SIZE]);

int BLC_Eval_streaming(const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t com1[MQOM2_PARAM_DIGEST_SIZE], const uint8_t opening[MQOM2_PARAM_OPENING_SIZE], const uint16_t i_star[MQOM2_PARAM_TAU], field_ext_elt x_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], field_ext_elt u_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

int BLC_Serialize_Opening_Key_streaming(const blc_key_streaming_t* key, uint8_t preopening[MQOM2_PARAM_PREOPENING_SIZE]);

int BLC_Deserialize_Opening_Key_streaming(const uint8_t preopening[MQOM2_PARAM_PREOPENING_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], blc_key_streaming_t* key);

#endif /* __BLC_STREAMING_H__ */

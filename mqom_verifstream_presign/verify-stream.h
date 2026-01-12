#ifndef __VERIFY_STREAM_H__
#define __VERIFY_STREAM_H__

#include "common.h"
#include "fields.h"
#include "piop_streaming.h"

/* Deal with namespacing */
#define StreamedVerify_Sign MQOM_NAMESPACE(StreamedVerify_Sign)
#define StreamedVerify_Init MQOM_NAMESPACE(StreamedVerify_Init)
#define StreamedVerify_Update MQOM_NAMESPACE(StreamedVerify_Update)
#define StreamedVerify_Finalize MQOM_NAMESPACE(StreamedVerify_Finalize)
#define StreamedVerify_Clean MQOM_NAMESPACE(StreamedVerify_Clean)

#define FIRST_CHUNK_BYTESIZE (4 + (MQOM2_PARAM_SALT_SIZE) + (2*(MQOM2_PARAM_DIGEST_SIZE)))
#define OTHER_CHUNK_BYTESIZE (MQOM2_PARAM_OPENING_X1_SIZE+BYTE_SIZE_FIELD_BASE(MQOM2_PARAM_ETA*MQOM2_PARAM_MU))
#if FIRST_CHUNK_BYTESIZE < OTHER_CHUNK_BYTESIZE
#define LARGEST_CHUNK_BYTESIZE (OTHER_CHUNK_BYTESIZE)
#else
#define LARGEST_CHUNK_BYTESIZE (FIRST_CHUNK_BYTESIZE)
#endif

typedef struct {
	uint8_t pk[MQOM2_PK_SIZE];
	//uint8_t mseed_eq[2 * MQOM2_PARAM_SEED_SIZE];
	//field_ext_elt y[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_M/MQOM2_PARAM_MU)];
	uint8_t hash[MQOM2_PARAM_DIGEST_SIZE];
	uint8_t salt[MQOM2_PARAM_SALT_SIZE];
	uint8_t com1[MQOM2_PARAM_DIGEST_SIZE];
	uint8_t nonce[4];
	uint16_t i_star[MQOM2_PARAM_TAU];
	uint16_t grinding_val;
	piop_data_t *piop_data;
	uint8_t hash_ls_com[MQOM2_PARAM_TAU][MQOM2_PARAM_DIGEST_SIZE];
	field_ext_elt alpha0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	field_ext_elt alpha1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)];
	uint32_t num_current_chunk;
	uint32_t pos;
	uint8_t unprocessed_part[LARGEST_CHUNK_BYTESIZE];
} stream_verif_context_t;

int StreamedVerify_Sign(const uint8_t sk[MQOM2_SK_SIZE], const uint8_t *msg, unsigned long long mlen, const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], uint8_t sig[MQOM2_SIG_SIZE]);

stream_verif_context_t *StreamedVerify_Init(const uint8_t pk[MQOM2_PK_SIZE]);

int StreamedVerify_Update(stream_verif_context_t* ctx, const uint8_t* sigpart, unsigned long long sigpartlen);

int StreamedVerify_Finalize(stream_verif_context_t* ctx, const uint8_t *msg, unsigned long long mlen);

void StreamedVerify_Clean(stream_verif_context_t* ctx);

#endif /* __VERIFY_STREAM_H__ */


#ifndef __PIOP_STREAMING_H__
#define __PIOP_STREAMING_H__

#include "mqom2_parameters.h"
#include "fields.h"
#include "common.h"

/* Deal with namespacing */
#define PreparePIOPPublicData MQOM_NAMESPACE(PreparePIOPPublicData)
#define FinalizePIOPPublicData MQOM_NAMESPACE(FinalizePIOPPublicData)
#define RecomputePAlpha_x1 MQOM_NAMESPACE(RecomputePAlpha_x1)
#define ComputePAlpha_streaming MQOM_NAMESPACE(ComputePAlpha_streaming)
#define RecomputePAlpha_streaming MQOM_NAMESPACE(RecomputePAlpha_streaming)
#define CleanPIOPPublicData MQOM_NAMESPACE(CleanPIOPPublicData)

typedef struct piop_data_t {
	field_ext_elt *_A_hat;
	field_ext_elt *_b_hat;
	field_ext_elt y[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)];
#if MQOM2_PARAM_WITH_STATISTICAL_BATCHING == 1
	field_ext_elt Gamma[MQOM2_PARAM_ETA][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)];
#endif
} piop_data_t;

piop_data_t *PreparePIOPPublicData(const uint8_t mseed_eq[2 * MQOM2_PARAM_SEED_SIZE], const field_ext_elt y[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)]);
int FinalizePIOPPublicData(piop_data_t* piop_data, const uint8_t com[MQOM2_PARAM_DIGEST_SIZE]);

int RecomputePAlpha_x1(piop_data_t* piop_data, field_ext_elt r, const field_ext_elt alpha1[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], const field_ext_elt x_eval[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], const field_ext_elt u_eval[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt alpha0[FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

int ComputePAlpha_streaming(const uint8_t com[MQOM2_PARAM_DIGEST_SIZE], const field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], const field_ext_elt u0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], const field_ext_elt u1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], const field_base_elt x[FIELD_BASE_PACKING(MQOM2_PARAM_MQ_N)], const uint8_t mseed_eq[2 * MQOM2_PARAM_SEED_SIZE], field_ext_elt alpha0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], field_ext_elt alpha1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

int RecomputePAlpha_streaming(const uint8_t com[MQOM2_PARAM_DIGEST_SIZE], const field_ext_elt alpha1[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], const uint16_t i_star[MQOM2_PARAM_TAU], const field_ext_elt x_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], const field_ext_elt u_eval[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)], const uint8_t mseed_eq[2 * MQOM2_PARAM_SEED_SIZE], const field_ext_elt y[FIELD_EXT_PACKING(MQOM2_PARAM_MQ_M / MQOM2_PARAM_MU)], field_ext_elt alpha0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_ETA)]);

void CleanPIOPPublicData(piop_data_t *piop_data);

#endif /* __PIOP_STREAMING_H__ */

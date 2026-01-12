/* Cleanup possible previous fields definitions */
#undef FIELDS_BITSLICE_COMPOSITE
#undef FIELDS_BITSLICE_PUBLIC_JUMP

/* We use the MQOM abstraction layer for bitslicing */
#define FIELDS_BITSLICE_COMPOSITE
#define FIELDS_BITSLICE_PUBLIC_JUMP
#include "fields_bitsliced.h"
#include "bitsliced_common.h"

void transform_composite_cond_vect(const field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)], felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)]) {
	TRANSFORM_VECT
}

void transform_composite_cond_back_vect(felt_ext_elt_bitsliced_t x0_bitsliced[BITSLICED_PACKING(MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU)], field_ext_elt x0[MQOM2_PARAM_TAU][FIELD_EXT_PACKING(MQOM2_PARAM_MQ_N)]) {
	TRANSFORM_BACK_VECT
}

void bitsliced_composite_cond_vect_mult_hybrid_public(felt_ext_elt_bitsliced_t *res, const field_ext_elt *a, const felt_ext_elt_bitsliced_t* b_bitsliced, uint32_t len) {
	VECT_MULT
}

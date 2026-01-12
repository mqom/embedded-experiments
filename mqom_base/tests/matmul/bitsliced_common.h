#ifndef __BITSLICED_COMMON_H__
#define __BITSLICED_COMMON_H__

#define TRANSFORM_VECT do { \
	field_ext_bitslice_vect_pack_pre(x0_bitsliced, MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU); \
	for(uint32_t e=0; e<MQOM2_PARAM_TAU; e++) { \
		field_ext_bitslice_vect_pack(x0[e], x0_bitsliced, e, MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU); \
	} \
	field_ext_bitslice_vect_pack_post(x0_bitsliced, MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU); \
} while(0);

#define TRANSFORM_BACK_VECT do { \
	field_ext_bitslice_vect_unpack_pre(x0_bitsliced, MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU); \
	for(uint32_t e=0; e<MQOM2_PARAM_TAU; e++) { \
		field_ext_bitslice_vect_unpack(x0_bitsliced, x0[e], e, MQOM2_PARAM_MQ_N, MQOM2_PARAM_TAU); \
	} \
} while(0);

#define VECT_MULT do { \
	field_ext_bitslice_vect_mult_hybrid_public( \
		field_ext_bitslice_vect_get(res, len, MQOM2_PARAM_TAU), \
		a, b_bitsliced, len + 1, MQOM2_PARAM_TAU); \
} while(0);

#endif /* __BITSLICED_COMMON_H__ */

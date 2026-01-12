#ifndef __SIGN_PRE_H__
#define __SIGN_PRE_H__

#include "common.h"

/* Deal with namespacing */
#define Sign_Prepare MQOM_NAMESPACE(Sign_Prepare)
#define Sign_Finalize MQOM_NAMESPACE(Sign_Finalize)

int Sign_Prepare(const uint8_t sk[MQOM2_SK_SIZE], const uint8_t salt[MQOM2_PARAM_SALT_SIZE], const uint8_t mseed[MQOM2_PARAM_SEED_SIZE], uint8_t presig[MQOM2_PRESIG_SIZE]);

int Sign_Finalize(const uint8_t sk[MQOM2_SK_SIZE], const uint8_t *msg, unsigned long long mlen, const uint8_t presig[MQOM2_PRESIG_SIZE], uint8_t sig[MQOM2_SIG_SIZE]);

#endif /* __SIGN_PRE_H__ */

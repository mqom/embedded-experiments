#ifndef __BLC_H__
#define __BLC_H__

#if defined(MEMORY_EFFICIENT_BLC)
#include "blc_memopt.h"
#define blc_key_t blc_key_memopt_t
#define BLC_Commit BLC_Commit_memopt
#define BLC_Open BLC_Open_memopt
#define BLC_Eval BLC_Eval_memopt
#define BLC_Serialize_Opening_Key BLC_Serialize_Opening_Key_memopt
#define BLC_Deserialize_Opening_Key BLC_Deserialize_Opening_Key_memopt
#define BLC_PrintConfig BLC_PrintConfig_memopt
#elif defined(BLC_STREAMING)
#include "blc_streaming.h"
#define blc_key_t blc_key_streaming_t
#define BLC_Commit BLC_Commit_streaming
#define BLC_Open BLC_Open_streaming
#define BLC_Eval BLC_Eval_streaming
#define BLC_Serialize_Opening_Key BLC_Serialize_Opening_Key_streaming
#define BLC_Deserialize_Opening_Key BLC_Deserialize_Opening_Key_streaming
#define BLC_PrintConfig BLC_PrintConfig_streaming
#else
#include "blc_default.h"
#define blc_key_t blc_key_default_t
#define BLC_Commit BLC_Commit_default
#define BLC_Open BLC_Open_default
#define BLC_Eval BLC_Eval_default
#define BLC_Serialize_Opening_Key BLC_Serialize_Opening_Key_default
#define BLC_Deserialize_Opening_Key BLC_Deserialize_Opening_Key_default
#define BLC_PrintConfig BLC_PrintConfig_default
#endif

#endif /* __BLC_H__ */

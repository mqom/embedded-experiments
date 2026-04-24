#ifndef __BLC_H__
#define __BLC_H__

#if defined(BLC_ONETREE_MEMOPT) && !defined(BLC_ONETREE)
#error "BLC_ONETREE_MEMOPT is defined without BLC_ONETREE."
#endif
#if !defined(BLC_ONETREE_MEMOPT) && defined(BLC_ONETREE)
#error "BLC_ONETREE_MEMOPT is not defined, while BLC_ONETREE is activated."
#endif

#if defined(BLC_ONETREE_MEMOPT) && defined(MEMORY_EFFICIENT_BLC)
#error "BLC_ONETREE_MEMOPT and MEMORY_EFFICIENT_BLC are not compatible."
#endif

#if defined(BLC_ONETREE_MEMOPT)
#include "blc_onetree_memopt.h"
#define blc_key_t blc_key_onetree_memopt_t
#define BLC_Commit BLC_Commit_onetree_memopt
#define BLC_CheckChallenge BLC_CheckChallenge_onetree_memopt
#define BLC_Open BLC_Open_onetree_memopt
#define BLC_Eval BLC_Eval_onetree_memopt
#define BLC_PrintConfig BLC_PrintConfig_onetree_memopt
#elif defined(MEMORY_EFFICIENT_BLC)
#include "blc_memopt.h"
#define blc_key_t blc_key_memopt_t
#define BLC_Commit BLC_Commit_memopt
#define BLC_CheckChallenge BLC_CheckChallenge_memopt
#define BLC_Open BLC_Open_memopt
#define BLC_Eval BLC_Eval_memopt
#define BLC_PrintConfig BLC_PrintConfig_memopt
#else
#include "blc_default.h"
#define blc_key_t blc_key_default_t
#define BLC_Commit BLC_Commit_default
#define BLC_CheckChallenge BLC_CheckChallenge_default
#define BLC_Open BLC_Open_default
#define BLC_Eval BLC_Eval_default
#define BLC_PrintConfig BLC_PrintConfig_default
#endif

#endif /* __BLC_H__ */

#ifndef __TEST_RINJNDAEL_H__
#define __TEST_RINJNDAEL_H__

#include <stdio.h>
#include <stdlib.h>
#include "../rijndael.h"

/* Structure describing a test */
typedef struct {
        rijndael_type type;
        unsigned int key_len;
        const uint8_t *key;
        unsigned int text_len;
        uint8_t *pt;
        uint8_t *ct;
} rijndael_test;

#endif

#if defined(BOARD_HAS_HW_AES) && defined(RIJNDAEL_EXTERNAL) && !defined(NO_HW_AES)
#include <enc.h>

/***** For the boards with an MCU using a CRYP engine, we override the encryption ****/
extern void hal_cryp_aes_128_set_key(const uint8_t key[16]);
extern void hal_cryp_aes_128_set_key_dma(const uint8_t key[16]);
extern void hal_cryp_aes_128_enc(const uint8_t *pt, uint8_t *ct, uint32_t sz);
extern void hal_cryp_aes_128_enc_dma(const uint8_t *pt, uint8_t *ct, uint32_t sz);


static inline void local_cryp_aes_128_set_key(const uint8_t key[16])
{
	hal_cryp_aes_128_set_key(key);
}

static inline void local_cryp_aes_128_set_key_dma(const uint8_t key[16])
{
	hal_cryp_aes_128_set_key_dma(key);
}

/***/
static int hw_aes128_enc(const rijndael_ctx_aes128 *ctx, const uint8_t data_in[16], uint8_t data_out[16])
{
        local_cryp_aes_128_set_key((uint8_t*)ctx->opaque);
        hal_cryp_aes_128_enc(data_in, data_out, 16);
        return 0;
}
static int hw_aes128_enc_x2(const rijndael_ctx_aes128 *ctx1, const rijndael_ctx_aes128 *ctx2, const uint8_t plainText1[16], const uint8_t plainText2[16], uint8_t cipherText1[16], uint8_t cipherText2[16])
{
        if(ctx1 == ctx2){
                hal_cryp_aes_128_set_key((uint8_t*)ctx1->opaque);
                hal_cryp_aes_128_enc(plainText1, cipherText1, 16);
                hal_cryp_aes_128_enc(plainText2, cipherText2, 16); 
        }
        else{
                hw_aes128_enc(ctx1, plainText1, cipherText1);
                hw_aes128_enc(ctx2, plainText2, cipherText2);
        }
        return 0;
}       
static int hw_aes128_enc_x4(const rijndael_ctx_aes128 *ctx1, const rijndael_ctx_aes128 *ctx2, const rijndael_ctx_aes128 *ctx3, const rijndael_ctx_aes128 *ctx4,
                const uint8_t plainText1[16], const uint8_t plainText2[16], const uint8_t plainText3[16], const uint8_t plainText4[16],
                uint8_t cipherText1[16], uint8_t cipherText2[16], uint8_t cipherText3[16], uint8_t cipherText4[16])
{       
        if((ctx1 == ctx2) && (ctx2 == ctx3) && (ctx3 == ctx4)){
                hal_cryp_aes_128_set_key((uint8_t*)ctx1->opaque);
                hal_cryp_aes_128_enc(plainText1, cipherText1, 16);
                hal_cryp_aes_128_enc(plainText2, cipherText2, 16);
                hal_cryp_aes_128_enc(plainText3, cipherText3, 16);
                hal_cryp_aes_128_enc(plainText4, cipherText4, 16);
        }
        else{
                hw_aes128_enc(ctx1, plainText1, cipherText1);
                hw_aes128_enc(ctx2, plainText2, cipherText2);
                hw_aes128_enc(ctx3, plainText3, cipherText3);
                hw_aes128_enc(ctx4, plainText4, cipherText4);
        }
        return 0;
}

static int hw_aes128_enc_x8(const rijndael_ctx_aes128 *ctx1, const rijndael_ctx_aes128 *ctx2, const rijndael_ctx_aes128 *ctx3, const rijndael_ctx_aes128 *ctx4,
                  const rijndael_ctx_aes128 *ctx5, const rijndael_ctx_aes128 *ctx6, const rijndael_ctx_aes128 *ctx7, const rijndael_ctx_aes128 *ctx8,
                const uint8_t plainText1[16], const uint8_t plainText2[16], const uint8_t plainText3[16], const uint8_t plainText4[16],
                const uint8_t plainText5[16], const uint8_t plainText6[16], const uint8_t plainText7[16], const uint8_t plainText8[16],
                uint8_t cipherText1[16], uint8_t cipherText2[16], uint8_t cipherText3[16], uint8_t cipherText4[16],
                uint8_t cipherText5[16], uint8_t cipherText6[16], uint8_t cipherText7[16], uint8_t cipherText8[16])
{
        if((ctx1 == ctx2) && (ctx2 == ctx3) && (ctx3 == ctx4) && (ctx4 == ctx5) && (ctx5 == ctx6) && (ctx6 == ctx7) && (ctx7 == ctx8)){
                hal_cryp_aes_128_set_key((uint8_t*)ctx1->opaque);
                hal_cryp_aes_128_enc(plainText1, cipherText1, 16);
                hal_cryp_aes_128_enc(plainText2, cipherText2, 16);
                hal_cryp_aes_128_enc(plainText3, cipherText3, 16);
                hal_cryp_aes_128_enc(plainText4, cipherText4, 16);
                hal_cryp_aes_128_enc(plainText5, cipherText5, 16);
                hal_cryp_aes_128_enc(plainText6, cipherText6, 16);
                hal_cryp_aes_128_enc(plainText7, cipherText7, 16);
                hal_cryp_aes_128_enc(plainText8, cipherText8, 16);
        }
        else{
                hw_aes128_enc(ctx1, plainText1, cipherText1);
                hw_aes128_enc(ctx2, plainText2, cipherText2);
                hw_aes128_enc(ctx3, plainText3, cipherText3);
                hw_aes128_enc(ctx4, plainText4, cipherText4);
                hw_aes128_enc(ctx5, plainText5, cipherText5);
                hw_aes128_enc(ctx6, plainText6, cipherText6);
                hw_aes128_enc(ctx7, plainText7, cipherText7);
                hw_aes128_enc(ctx8, plainText8, cipherText8);
        }
        return 0;
}


static int hw_aes128_enc_ecb(const rijndael_ctx_aes128_ecb *ctx, uint32_t nblocks, const uint8_t* in, uint8_t* out, int is_dma)
{
        local_cryp_aes_128_set_key_dma((uint8_t*)ctx->opaque);
	if(is_dma){
		hal_cryp_aes_128_enc_dma(in, out, 16 * nblocks);
	}
	else{
		hal_cryp_aes_128_enc(in, out, 16 * nblocks);
	}
        return 0;
}

/****** AES-128 encryption override *****/
/* "Dummy" context for hardware acceleration */
/***/
int aes128_external_setkey_enc(rijndael_ctx_aes128 *ctx, const uint8_t key[16]){
	if(ctx->opaque == NULL){
		/* Allocate the key */
		ctx->opaque = (void*)mqom_malloc(16 * sizeof(uint8_t));
	}
	memcpy(ctx->opaque, key, 16);
        return 0;
}
int aes128_external_setkey_enc_ecb(rijndael_ctx_aes128_ecb *ctx, const uint8_t key[16]){
	return aes128_external_setkey_enc((rijndael_ctx_aes128_ecb*)ctx, key);
}

/***/
int aes128_external_enc(const rijndael_ctx_aes128 *ctx, const uint8_t data_in[16], uint8_t data_out[16])
{
        return hw_aes128_enc(ctx, data_in, data_out);
}
int aes128_external_enc_x2(const rijndael_ctx_aes128 *ctx1, const rijndael_ctx_aes128 *ctx2, const uint8_t plainText1[16], const uint8_t plainText2[16], uint8_t cipherText1[16], uint8_t cipherText2[16])
{
        return hw_aes128_enc_x2(ctx1, ctx2, plainText1, plainText2, cipherText1, cipherText2);
}
int aes128_external_enc_x4(const rijndael_ctx_aes128 *ctx1, const rijndael_ctx_aes128 *ctx2, const rijndael_ctx_aes128 *ctx3, const rijndael_ctx_aes128 *ctx4,
                const uint8_t plainText1[16], const uint8_t plainText2[16], const uint8_t plainText3[16], const uint8_t plainText4[16],
                uint8_t cipherText1[16], uint8_t cipherText2[16], uint8_t cipherText3[16], uint8_t cipherText4[16])
{
        return hw_aes128_enc_x4(ctx1, ctx2, ctx3, ctx4, plainText1, plainText2, plainText3, plainText4, cipherText1, cipherText2, cipherText3, cipherText4);
}
int aes128_external_enc_x8(const rijndael_ctx_aes128 *ctx1, const rijndael_ctx_aes128 *ctx2, const rijndael_ctx_aes128 *ctx3, const rijndael_ctx_aes128 *ctx4,
                  const rijndael_ctx_aes128 *ctx5, const rijndael_ctx_aes128 *ctx6, const rijndael_ctx_aes128 *ctx7, const rijndael_ctx_aes128 *ctx8,
                const uint8_t plainText1[16], const uint8_t plainText2[16], const uint8_t plainText3[16], const uint8_t plainText4[16],
                const uint8_t plainText5[16], const uint8_t plainText6[16], const uint8_t plainText7[16], const uint8_t plainText8[16],
                uint8_t cipherText1[16], uint8_t cipherText2[16], uint8_t cipherText3[16], uint8_t cipherText4[16],
                uint8_t cipherText5[16], uint8_t cipherText6[16], uint8_t cipherText7[16], uint8_t cipherText8[16])
{
        return hw_aes128_enc_x8(ctx1, ctx2, ctx3, ctx4, ctx5, ctx6, ctx7, ctx8,
                            plainText1, plainText2, plainText3, plainText4, plainText5, plainText6, plainText7, plainText8,
                            cipherText1, cipherText2, cipherText3, cipherText4, cipherText5, cipherText6, cipherText7, cipherText8);
}
int aes128_external_enc_ecb(const rijndael_ctx_aes128_ecb *ctx, uint32_t nblocks, const uint8_t* in, uint8_t* out)
{
	return hw_aes128_enc_ecb(ctx, nblocks, in, out, 1);
}

void enc_clean_ctx(enc_ctx *ctx){
	if(ctx->opaque != NULL){
		mqom_free(ctx->opaque, 16 * sizeof(uint8_t));
	}
	ctx->opaque = NULL;
}
void enc_clean_ctx_ecb(enc_ctx_ecb *ctx){
	enc_clean_ctx((enc_ctx_ecb*)ctx);
}
void enc_clean_ctx_x2(enc_ctx_x2 *ctx){
	enc_clean_ctx(&ctx->ctx[0]);
	enc_clean_ctx(&ctx->ctx[1]);
}
void enc_clean_ctx_x4(enc_ctx_x4 *ctx){
	enc_clean_ctx(&ctx->ctx[0]);
	enc_clean_ctx(&ctx->ctx[1]);
	enc_clean_ctx(&ctx->ctx[2]);
	enc_clean_ctx(&ctx->ctx[3]);
}
void enc_clean_ctx_x8(enc_ctx_x8 *ctx){
	enc_clean_ctx(&ctx->ctx[0]);
	enc_clean_ctx(&ctx->ctx[1]);
	enc_clean_ctx(&ctx->ctx[2]);
	enc_clean_ctx(&ctx->ctx[3]);
	enc_clean_ctx(&ctx->ctx[4]);
	enc_clean_ctx(&ctx->ctx[5]);
	enc_clean_ctx(&ctx->ctx[6]);
	enc_clean_ctx(&ctx->ctx[7]);
}
void enc_clean_ctx_pub(enc_ctx_pub *ctx){
	enc_clean_ctx(ctx);
}
void enc_clean_ctx_pub_x2(enc_ctx_pub_x2 *ctx){
	enc_clean_ctx_x2(ctx);
}
void enc_clean_ctx_pub_x4(enc_ctx_pub_x4 *ctx){
	enc_clean_ctx_x4(ctx);
}
void enc_clean_ctx_pub_x8(enc_ctx_pub_x8 *ctx){
	enc_clean_ctx_x8(ctx);
}
void enc_clean_ctx_pub_ecb(enc_ctx_pub_ecb *ctx){
	enc_clean_ctx((enc_ctx_ecb*)ctx);
}
void enc_uninit_ctx(enc_ctx *ctx){
	if(ctx){
		ctx->opaque = NULL;
	}
}
void enc_uninit_ctx_ecb(enc_ctx_ecb *ctx){
	enc_uninit_ctx((enc_ctx_ecb*)ctx);
}
void enc_uninit_ctx_x2(enc_ctx_x2 *ctx){
	enc_uninit_ctx(&ctx->ctx[0]);
	enc_uninit_ctx(&ctx->ctx[1]);
}
void enc_uninit_ctx_x4(enc_ctx_x4 *ctx){
	enc_uninit_ctx(&ctx->ctx[0]);
	enc_uninit_ctx(&ctx->ctx[1]);
	enc_uninit_ctx(&ctx->ctx[2]);
	enc_uninit_ctx(&ctx->ctx[3]);
}
void enc_uninit_ctx_x8(enc_ctx_x8 *ctx){
	enc_uninit_ctx(&ctx->ctx[0]);
	enc_uninit_ctx(&ctx->ctx[1]);
	enc_uninit_ctx(&ctx->ctx[2]);
	enc_uninit_ctx(&ctx->ctx[3]);
	enc_uninit_ctx(&ctx->ctx[4]);
	enc_uninit_ctx(&ctx->ctx[5]);
	enc_uninit_ctx(&ctx->ctx[6]);
	enc_uninit_ctx(&ctx->ctx[7]);
}
void enc_uninit_ctx_pub(enc_ctx_pub *ctx){
	enc_uninit_ctx(ctx);
}
void enc_uninit_ctx_pub_x2(enc_ctx_pub_x2 *ctx){
	enc_uninit_ctx_x2(ctx);
}
void enc_uninit_ctx_pub_x4(enc_ctx_pub_x4 *ctx){
	enc_uninit_ctx_x4(ctx);
}
void enc_uninit_ctx_pub_x8(enc_ctx_pub_x8 *ctx){
	enc_uninit_ctx_x8(ctx);
}
void enc_uninit_ctx_pub_ecb(enc_ctx_pub_ecb *ctx){
	enc_uninit_ctx((enc_ctx_ecb*)ctx);
}

#else
/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif

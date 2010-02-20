/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 * Vitaly Rozhkov <v.rozhkov at soft-industry.com>
 */

#include "zrtp.h"

#define _ZTU_ "zrtp cipher"

typedef struct zrtp_aes_cfb_ctx {
	uint8_t				mode;
	aes_encrypt_ctx		aes_ctx[1];
	zrtp_v128_t			iv;
} zrtp_aes_cfb_ctx_t;

typedef struct zrtp_aes_ctr_ctx {
	uint8_t				mode;
	aes_encrypt_ctx		aes_ctx[1];
	zrtp_v128_t			salt;
	zrtp_v128_t			counter;
}zrtp_aes_ctr_ctx_t;


/*===========================================================================*/
/*	Global AES functions													 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_aes_cfb_stop(zrtp_cipher_t *self, void *cipher_ctx) {
	zrtp_memset(cipher_ctx, 0, sizeof(zrtp_aes_cfb_ctx_t));
	zrtp_sys_free(cipher_ctx);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_aes_ctr_stop(zrtp_cipher_t *self, void *cipher_ctx) {
	zrtp_memset(cipher_ctx, 0, sizeof(zrtp_aes_ctr_ctx_t));
	zrtp_sys_free(cipher_ctx);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_aes_stop(zrtp_cipher_t *self, void *cipher_ctx)
{
	zrtp_status_t res;
	zrtp_cipher_mode_t *mode = (zrtp_cipher_mode_t*)cipher_ctx;
	switch (mode->mode) {
		case ZRTP_CIPHER_MODE_CTR:
			res = zrtp_aes_ctr_stop(self, cipher_ctx);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			res = zrtp_aes_cfb_stop(self, cipher_ctx);
			break;
		default:
			res = zrtp_status_bad_param;
			break;
	}
	return res;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_aes_cfb_set_iv(zrtp_cipher_t *self, void* cipher_ctx, zrtp_v128_t *iv)
{
	zrtp_aes_cfb_ctx_t* ctx = (zrtp_aes_cfb_ctx_t*)cipher_ctx;	
	zrtp_memcpy(&ctx->iv, iv, sizeof(zrtp_v128_t));
	
	/* clear previous context except the first byte (key length) */
	zrtp_bg_aes_mode_reset(ctx->aes_ctx);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_aes_ctr_set_iv(zrtp_cipher_t *self, void *cipher_ctx, zrtp_v128_t *iv )
{
	zrtp_aes_ctr_ctx_t* ctx = (zrtp_aes_ctr_ctx_t*)cipher_ctx;
	zrtp_v128_xor(&ctx->counter, &ctx->salt, iv);

	/* clear previous context except the first byte (key length) */
	zrtp_bg_aes_mode_reset(ctx->aes_ctx);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_aes_set_iv(zrtp_cipher_t *self, void *cipher_ctx, zrtp_v128_t *iv )
{
	zrtp_status_t res;
	zrtp_cipher_mode_t *mode = (zrtp_cipher_mode_t*)cipher_ctx;
	
	switch (mode->mode) {
		case ZRTP_CIPHER_MODE_CTR:
			res = zrtp_aes_ctr_set_iv(self, cipher_ctx, iv);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			res = zrtp_aes_cfb_set_iv(self, cipher_ctx, iv);
			break;
		default:
			res = zrtp_status_bad_param;
			break;
	}
	return res;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_aes_cfb_encrypt( zrtp_cipher_t *self,
								    void* cipher_ctx,
									unsigned char *buf,
									int len) {
	zrtp_aes_cfb_ctx_t* ctx = (zrtp_aes_cfb_ctx_t*)cipher_ctx;
	AES_RETURN res = zrtp_bg_aes_cfb_encrypt(buf, buf, len, ctx->iv.v8, ctx->aes_ctx);
	
	return (EXIT_SUCCESS == res) ? zrtp_status_ok : zrtp_status_cipher_fail;
}

void zrtp_aes_ctr_inc(unsigned char *counter) {
	if(!(++counter[15])) {
		++counter[14];
	}
}

zrtp_status_t zrtp_aes_ctr_encrypt( zrtp_cipher_t *self,
								    void *cipher_ctx,
									unsigned char *buf,
									int len ) {
	zrtp_aes_ctr_ctx_t* ctx = (zrtp_aes_ctr_ctx_t*)cipher_ctx;	
	AES_RETURN res = zrtp_bg_aes_ctr_crypt(buf, buf, len, ctx->counter.v8, zrtp_aes_ctr_inc, ctx->aes_ctx);
	
	return (EXIT_SUCCESS == res) ? zrtp_status_ok : zrtp_status_cipher_fail;
}

zrtp_status_t zrtp_aes_encrypt( zrtp_cipher_t *self,
							    void *cipher_ctx,
								unsigned char *buf,
								int len)
{
	zrtp_status_t res;
	zrtp_cipher_mode_t* mode = (zrtp_cipher_mode_t*)cipher_ctx;
	switch (mode->mode) {
		case ZRTP_CIPHER_MODE_CTR:
			res = zrtp_aes_ctr_encrypt(self, cipher_ctx, buf, len);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			res = zrtp_aes_cfb_encrypt(self, cipher_ctx, buf, len);
			break;
		default:
			res = zrtp_status_bad_param;
			break;
	}
	return res;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_aes_cfb_decrypt( zrtp_cipher_t *self,
								    void* cipher_ctx,
									unsigned char *buf,
									int len) {
	zrtp_aes_cfb_ctx_t* ctx = (zrtp_aes_cfb_ctx_t*)cipher_ctx;	
	AES_RETURN res = zrtp_bg_aes_cfb_decrypt(buf, buf, len, ctx->iv.v8, ctx->aes_ctx);
	
	return (EXIT_SUCCESS == res) ? zrtp_status_ok : zrtp_status_cipher_fail;
}

zrtp_status_t zrtp_aes_ctr_decrypt( zrtp_cipher_t *self,
								    void *cipher_ctx,
									unsigned char *buf,
									int len) {
	zrtp_aes_ctr_ctx_t* ctx = (zrtp_aes_ctr_ctx_t*)cipher_ctx;
	
	AES_RETURN res = zrtp_bg_aes_ctr_crypt(buf, buf, len, ctx->counter.v8, zrtp_aes_ctr_inc, ctx->aes_ctx);
	return (EXIT_SUCCESS == res) ? zrtp_status_ok : zrtp_status_cipher_fail;	
}

zrtp_status_t zrtp_aes_decrypt( zrtp_cipher_t *self,
							    void *cipher_ctx,
								unsigned char *buf,
								int len)
{
	zrtp_status_t res;
	zrtp_cipher_mode_t *mode = (zrtp_cipher_mode_t*)cipher_ctx;
	
	switch(mode->mode){
		case ZRTP_CIPHER_MODE_CTR:
			res = zrtp_aes_ctr_decrypt(self, cipher_ctx, buf, len);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			res = zrtp_aes_cfb_decrypt(self, cipher_ctx, buf, len);
			break;
		default:
			res = zrtp_status_bad_param;
			break;
	}
	return res;
}


/*===========================================================================*/
/*	AES 128 implementation													 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
void *zrtp_aes_cfb128_start(zrtp_cipher_t *self, void *key, void *extra_data)
{
	zrtp_aes_cfb_ctx_t *cipher_ctx = zrtp_sys_alloc(sizeof(zrtp_aes_cfb_ctx_t));
	if(NULL == cipher_ctx) {
		return NULL;
	}
	cipher_ctx->mode = ZRTP_CIPHER_MODE_CFB;
	zrtp_bg_aes_encrypt_key128(((zrtp_v128_t*)key)->v8, cipher_ctx->aes_ctx);

	return cipher_ctx;
}


void *zrtp_aes_ctr128_start( zrtp_cipher_t *self, void *key, void *extra_data)
{
	zrtp_aes_ctr_ctx_t *cipher_ctx = zrtp_sys_alloc(sizeof(zrtp_aes_ctr_ctx_t));
	if(NULL == cipher_ctx) {
		return NULL;
	}
	
	cipher_ctx->mode = ZRTP_CIPHER_MODE_CTR;
	zrtp_memcpy(&cipher_ctx->salt, extra_data, sizeof(zrtp_v128_t)-2);
	cipher_ctx->salt.v8[14] = cipher_ctx->salt.v8[15] =0;
	
	zrtp_memset(&cipher_ctx->counter, 0, sizeof(zrtp_v128_t));	
	zrtp_bg_aes_encrypt_key128(((zrtp_v128_t*)key)->v8, cipher_ctx->aes_ctx);

	return cipher_ctx;
}

void *zrtp_aes128_start( zrtp_cipher_t *self, void *key, void *extra_data, uint8_t mode)
{
	void *ctx;
	switch (mode) {
		case ZRTP_CIPHER_MODE_CTR:
			ctx = zrtp_aes_ctr128_start(self, key, extra_data);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			ctx = zrtp_aes_cfb128_start(self, key, extra_data);
			break;
		default:
			ctx = NULL;
			break;
	};
	return ctx;
}

/*---------------------------------------------------------------------------*/
/* Global CFB Test-Vectors */
static uint8_t aes_cfb_test_key[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

static uint8_t aes_cfb_test_iv[16] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};

static uint8_t aes_cfb_test_buf1a[50] = {
	0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
	0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a
};

static uint8_t aes_cfb_test_buf1b[50];
//static uint8_t aes_cfb_test_buf1c[50];

static uint8_t aes_cfb_test_buf2a[50] = {
	0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf,
	0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60, 0x89
};

static uint8_t aes_cfb_test_buf2b[50];

static uint8_t aes_cfb_test_key3[32];
static uint8_t aes_cfb_test_iv3[16];
static uint8_t aes_cfb_test_buf3a[50];

static uint8_t aes_cfb_test_buf3b[50] = {
	0x66, 0xe9, 0x4b, 0xd4, 0xef, 0x8a, 0x2c, 0x3b,
	0x88, 0x4c, 0xfa, 0x59, 0xca, 0x34, 0x2b, 0x2e,
	0xf7, 0x95, 0xbd, 0x4a, 0x52, 0xe2, 0x9e, 0xd7,
	0x13, 0xd3, 0x13, 0xfa, 0x20, 0xe9, 0x8d, 0xbc,
	0xa1, 0x0c, 0xf6, 0x6d, 0x0f, 0xdd, 0xf3, 0x40,
	0x53, 0x70, 0xb4, 0xbf, 0x8d, 0xf5, 0xbf, 0xb3,
	0x47, 0xc7
};

uint8_t aes_cfb_test_buf3c[50] = {
	0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89,
	0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87,
	0x08, 0xc3, 0x74, 0x84, 0x8c, 0x22, 0x82, 0x33,
	0xc2, 0xb3, 0x4f, 0x33, 0x2b, 0xd2, 0xe9, 0xd3,
	0x8b, 0x70, 0xc5, 0x15, 0xa6, 0x66, 0x3d, 0x38,
	0xcd, 0xb8, 0xe6, 0x53, 0x2b, 0x26, 0x64, 0x91,
	0x5d, 0x0d
};

/* Global CTR Test-Vectors */
uint8_t aes_ctr_test_nonce[16] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 258-bit AES CTR Test-Vectors */
uint8_t aes_ctr_test_key256[48] = {
	0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08,
	0x0A, 0x0B, 0x0C, 0x0D, 0x0F, 0x10, 0x11, 0x12,
	0x14, 0x15, 0x16, 0x17, 0x19, 0x1A, 0x1B, 0x1C,
	0x1E, 0x1F, 0x20, 0x21, 0x23, 0x24, 0x25, 0x26,
	0x83, 0x4E, 0xAD, 0xFC, 0xCA, 0xC7, 0xE1, 0xB3,
	0x06, 0x64, 0xB1, 0xAB, 0xA4, 0x48, 0x15, 0xAB
};

uint8_t aes_ctr_test_plaintext256[16] = {
	0x83, 0x4E, 0xAD, 0xFC, 0xCA, 0xC7, 0xE1, 0xB3,
	0x06, 0x64, 0xB1, 0xAB, 0xA4, 0x48, 0x15, 0xAB
};

uint8_t aes_ctr_test_ciphertext256[16] = {
	0x5d, 0x8e, 0xfd, 0xe6, 0x69, 0x62, 0xbf, 0x49,
	0xda, 0xe2, 0xea, 0xcf, 0x0b, 0x69, 0xe4, 0xf6
};

/* 128-bit AES CFB Test-Vectors */
uint8_t aes_ctr_test_key128[32] = {
0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c,
0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0x00, 0x00
};

uint8_t aes_ctr_test_plaintext128[32] =  {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

uint8_t aes_ctr_test_ciphertext128[32] = {
0xe0, 0x3e, 0xad, 0x09, 0x35, 0xc9, 0x5e, 0x80,
0xe1, 0x66, 0xb1, 0x6d, 0xd9, 0x2b, 0x4e, 0xb4,
0xd2, 0x35, 0x13, 0x16, 0x2b, 0x02, 0xd0, 0xf7,
0x2a, 0x43, 0xa2, 0xfe, 0x4a, 0x5f, 0x97, 0xab
};


zrtp_status_t zrtp_aes_cfb128_self_test(zrtp_cipher_t *self)
{

	zrtp_status_t err = zrtp_status_fail;
	int i = 0;
	zrtp_v128_t tmp_iv;
	zrtp_aes_cfb_ctx_t *ctx = (zrtp_aes_cfb_ctx_t*)self->start( self,
																aes_cfb_test_key,
																NULL,
																ZRTP_CIPHER_MODE_CFB);
	if(NULL == ctx) {
		return zrtp_status_fail;
	}

	ZRTP_LOG(3, (_ZTU_,"128 bit AES CFB\n"));
	ZRTP_LOG(3, (_ZTU_,"1st test...\n"));
	
	zrtp_memcpy(aes_cfb_test_buf1b, aes_cfb_test_buf1a, sizeof(aes_cfb_test_buf1a));
	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv, sizeof(aes_cfb_test_iv));
	self->set_iv(self, ctx, &tmp_iv);
	
	ZRTP_LOG(3, (_ZTU_,"\tencryption... "));
	
	err = self->encrypt(self, ctx, aes_cfb_test_buf1b, sizeof(aes_cfb_test_buf1b));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 128-bit AES CFB encrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}
	
	for (i=0; i<16; i++) {
		if (aes_cfb_test_buf1b[i] != 0x00) {
			ZRTP_LOGC(1, ("ERROR! 128-bit AES CFB failed on encrypt test"));
			self->stop(self, ctx);
            return zrtp_status_fail;
        }
	}
	ZRTP_LOGC(3, ("OK\n"));

	ZRTP_LOG(3, (_ZTU_,"\tdecryption... "));
	
	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv, sizeof(aes_cfb_test_iv));

	self->set_iv(self, ctx, &tmp_iv);
	err = self->decrypt(self, ctx, aes_cfb_test_buf1b, sizeof(aes_cfb_test_buf1b));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(3, ("ERROR! 128-bit AES CFB decrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}

	for (i=0; i<sizeof(aes_cfb_test_buf1a); i++) {
		if (aes_cfb_test_buf1b[i] != aes_cfb_test_buf1a[i]) {
			ZRTP_LOGC(1, ("ERROR! 128-bit AES CFB failed on decrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	self->stop(self, ctx);
	ZRTP_LOGC(3, ("OK\n"));

	ZRTP_LOG(3, (_ZTU_, "2nd test...\n"));
	
	ctx = self->start(self, aes_cfb_test_key3, NULL, ZRTP_CIPHER_MODE_CFB);
	if (NULL == ctx) {
		return zrtp_status_fail;
	}
	
	ZRTP_LOG(3, (_ZTU_, "\tencryption... "));

	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv3, sizeof(tmp_iv));
	self->set_iv(self, ctx, &tmp_iv);
	
	err = self->encrypt(self, ctx, aes_cfb_test_buf3a, sizeof(aes_cfb_test_buf3a));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 128-bit AES CFB encrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}
	
	for (i=0; i<sizeof(aes_cfb_test_buf3a); i++) {
		if (aes_cfb_test_buf3a[i] != aes_cfb_test_buf3b[i]) {
			ZRTP_LOGC(1, ("ERROR! 128-bit AES CFB failed on encrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	ZRTP_LOGC(3, ("OK\n"));

	ZRTP_LOG(3, (_ZTU_, "\tdecryption... "));
	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv3, sizeof(tmp_iv));
	self->set_iv(self, ctx, &tmp_iv);

	err = self->decrypt(self, ctx, aes_cfb_test_buf3b, sizeof(aes_cfb_test_buf3b));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 128-bit AES CFB decrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}

	for (i=0; i<sizeof(aes_cfb_test_buf3b); i++) {
		if (aes_cfb_test_buf3b[i] != 0x00) {
			ZRTP_LOGC(1, ("ERROR! 128-bit AES CFB failed on decrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	ZRTP_LOGC(3, ("OK\n"));
	
	self->stop(self, ctx);
	return zrtp_status_ok;
}

zrtp_status_t zrtp_aes_ctr128_self_test(zrtp_cipher_t *self)
{
	uint8_t tmp_buf[32];	
	zrtp_status_t err = zrtp_status_fail;
	int i;

	zrtp_aes_ctr_ctx_t *ctx = (zrtp_aes_ctr_ctx_t*)self->start( self,
															    aes_ctr_test_key128,
																aes_ctr_test_key128+16,
																ZRTP_CIPHER_MODE_CTR);
		
	if (NULL == ctx) {
		return zrtp_status_fail;
	}

	ZRTP_LOG(3, (_ZTU_,"128 bit AES CTR\n"));
	ZRTP_LOG(3, (_ZTU_, "1st test...\n"));
	
	ZRTP_LOG(3, (_ZTU_, "\tencryption... "));
	
	self->set_iv(self, ctx, (zrtp_v128_t*)aes_ctr_test_nonce);
	
	zrtp_memcpy(tmp_buf, aes_ctr_test_plaintext128, sizeof(tmp_buf));
	err = self->encrypt(self, ctx, tmp_buf, sizeof(tmp_buf));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 128-bit encrypt returns error %d\n", err));
		self->stop(self, ctx);
		return zrtp_status_fail;
	}

    for (i=0; i<sizeof(aes_ctr_test_ciphertext128); i++) {
		if (tmp_buf[i] != aes_ctr_test_ciphertext128[i]) {
			ZRTP_LOGC(1, ("ERROR! Fail on 128 bit encrypt test. i=%i\n", i));
			self->stop(self, ctx);
			return err;
		}
	}
	ZRTP_LOGC(3, ("OK\n"));
	
	ZRTP_LOG(3, (_ZTU_, "\tdecryption..."));
	
	self->set_iv(self, ctx, (zrtp_v128_t*)aes_ctr_test_nonce);

	err = self->decrypt(self, ctx, tmp_buf, sizeof(tmp_buf));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 128-bit AES CTR decrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;		
	}

	for (i=0; i<sizeof(aes_ctr_test_plaintext128); i++) {
		if (tmp_buf[i] != aes_ctr_test_plaintext128[i]) {
			ZRTP_LOGC(1, ("ERROR! 128-bit AES CTR failed on decrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	self->stop(self, ctx);
	ZRTP_LOGC(3, ("OK\n"));

	return zrtp_status_ok;
}

zrtp_status_t zrtp_aes128_self_test(zrtp_cipher_t *self, uint8_t mode)
{
	zrtp_status_t res;
	switch(mode){
		case ZRTP_CIPHER_MODE_CTR:
			res = zrtp_aes_ctr128_self_test(self);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			res = zrtp_aes_cfb128_self_test(self);
			break;
		default:
			res = zrtp_status_bad_param;
			break;
	}
	return res;
}

/*===========================================================================*/
/*	AES 256 implementation													 */
/*===========================================================================*/

/*---------------------------------------------------------------------------*/
void *zrtp_aes_cfb256_start(zrtp_cipher_t *self, void *key, void *extra_data)
{
	zrtp_aes_cfb_ctx_t *cipher_ctx = zrtp_sys_alloc(sizeof(zrtp_aes_cfb_ctx_t));
	if(NULL == cipher_ctx) {
		return NULL;
	}
	
	cipher_ctx->mode = ZRTP_CIPHER_MODE_CFB;
	zrtp_bg_aes_encrypt_key256(((zrtp_v256_t*)key)->v8, cipher_ctx->aes_ctx);
	return cipher_ctx;
}

void *zrtp_aes_ctr256_start(zrtp_cipher_t *self, void *key, void *extra_data)
{
	zrtp_aes_ctr_ctx_t *cipher_ctx = zrtp_sys_alloc(sizeof(zrtp_aes_ctr_ctx_t));
	if(NULL == cipher_ctx) {
		return NULL;
	}

	cipher_ctx->mode = ZRTP_CIPHER_MODE_CTR;
	zrtp_memcpy(&cipher_ctx->salt, extra_data, sizeof(zrtp_v128_t)-2);
	cipher_ctx->salt.v8[14] = cipher_ctx->salt.v8[15] =0;

	zrtp_memset(&cipher_ctx->counter, 0, sizeof(zrtp_v128_t));
	
	zrtp_bg_aes_encrypt_key256(((zrtp_v256_t*)key)->v8, cipher_ctx->aes_ctx);
	
	return cipher_ctx;
}

void *zrtp_aes256_start(zrtp_cipher_t *self, void *key, void *extra_data, uint8_t mode)
{
	void *ctx = NULL;
	switch (mode) {
		case ZRTP_CIPHER_MODE_CTR:
			ctx = zrtp_aes_ctr256_start(self, key, extra_data);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			ctx = zrtp_aes_cfb256_start(self, key, extra_data);
			break;
		default:
			ctx = NULL;
			break;
	}
	return ctx;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_aes_cfb256_self_test(zrtp_cipher_t *self)
{	
	zrtp_status_t err;
	int i;
	zrtp_v128_t tmp_iv;

	zrtp_aes_cfb_ctx_t *ctx = (zrtp_aes_cfb_ctx_t*)self->start( self,
															    aes_cfb_test_key,
															    NULL,
															    ZRTP_CIPHER_MODE_CFB);
	if (NULL == ctx) {
		return zrtp_status_fail;
	}
	
	ZRTP_LOG(3, (_ZTU_,"256 bit AES CFB\n"));
	ZRTP_LOG(3, (_ZTU_, "1st test...\n"));
	
	zrtp_memcpy(aes_cfb_test_buf2b, aes_cfb_test_buf2a, sizeof(aes_cfb_test_buf2a));
	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv, sizeof(tmp_iv));
	
	ZRTP_LOG(3, (_ZTU_, "\tencryption... "));
					 
	self->set_iv(self, ctx, &tmp_iv);
	err = self->encrypt(self, ctx, aes_cfb_test_buf2b, sizeof(aes_cfb_test_buf2b));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB encrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}

	for (i=0; i<16; i++) {
		if (aes_cfb_test_buf2b[i] != 0x00) {
			ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB failed on encrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	ZRTP_LOGC(3, ("OK\n"));

	ZRTP_LOG(3, (_ZTU_, "\tdecryption... "));
	
	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv, sizeof(tmp_iv));
	self->set_iv(self, ctx,  &tmp_iv);

	err = self->decrypt(self, ctx, aes_cfb_test_buf2b, sizeof(aes_cfb_test_buf2b));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB decrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}
	for (i=0; i<sizeof(aes_cfb_test_buf2b); i++) {
		if (aes_cfb_test_buf2b[i] != aes_cfb_test_buf2a[i]) {
			ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB failed on decrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	self->stop(self, ctx);
	ZRTP_LOGC(3, ("OK\n"));
		
	ZRTP_LOG(3, (_ZTU_, "2nd test...\n"));
	
	ctx = self->start(self, aes_cfb_test_key3, NULL, ZRTP_CIPHER_MODE_CFB);
    if(NULL == ctx){
        return zrtp_status_fail;
    }
	
	ZRTP_LOG(3, (_ZTU_, "\tencryption..."));

	zrtp_memset (aes_cfb_test_buf3a, 0, sizeof(aes_cfb_test_buf3a));
	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv3, sizeof(tmp_iv));
	
	self->set_iv(self, ctx, &tmp_iv);
	err = self->encrypt(self, ctx, aes_cfb_test_buf3a, sizeof(aes_cfb_test_buf3a));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB encrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}
	
	for (i=0; i<sizeof(aes_cfb_test_buf3a); i++) {
		if (aes_cfb_test_buf3a[i] != aes_cfb_test_buf3c[i]) {
			ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB failed on bit encrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	ZRTP_LOGC(3, ("OK\n"));

	ZRTP_LOG(3, (_ZTU_, "\tdecryption..."));

	zrtp_memcpy(&tmp_iv, aes_cfb_test_iv3, sizeof(tmp_iv));
	self->set_iv(self, ctx, &tmp_iv);

	err = self->decrypt(self, ctx, aes_cfb_test_buf3c, sizeof(aes_cfb_test_buf3c));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB decrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;
	}
    
	for (i=0; i<sizeof(aes_cfb_test_buf3c); i++) {
		if (aes_cfb_test_buf3c[i] != 0x00) {
			ZRTP_LOGC(1, ("ERROR! 256-bit AES CFB failed on decrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	self->stop(self, ctx);
	ZRTP_LOGC(3, ("OK\n"));
	
	return zrtp_status_ok;
}

zrtp_status_t zrtp_aes_ctr256_self_test(zrtp_cipher_t *self)
{
	uint8_t tmp_buf[32];	
	zrtp_status_t err = zrtp_status_fail;
	int i;

	zrtp_aes_ctr_ctx_t *ctx =  (zrtp_aes_ctr_ctx_t*)self->start( self,
																 aes_ctr_test_key256,
																 aes_ctr_test_key256+32,
																 ZRTP_CIPHER_MODE_CTR);
	if (NULL == ctx) {
		return zrtp_status_fail;
	}

	ZRTP_LOG(3, (_ZTU_,"256 bit AES CTR\n"));
	ZRTP_LOG(3, (_ZTU_, "1st test...\n"));
	
	ZRTP_LOG(3, (_ZTU_, "\tencryption... "));
	
	self->set_iv(self, ctx, (zrtp_v128_t*)aes_ctr_test_nonce);
	
	zrtp_memcpy(tmp_buf, aes_ctr_test_plaintext256, sizeof(aes_ctr_test_plaintext256));
	err = self->encrypt(self, ctx, tmp_buf, sizeof(aes_ctr_test_plaintext256));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 256-bit encrypt returns error %d\n", err));
		self->stop(self, ctx);
		return zrtp_status_fail;
	}

    for (i=0; i<sizeof(aes_ctr_test_ciphertext256); i++) {
		if (tmp_buf[i] != aes_ctr_test_ciphertext256[i]) {
			ZRTP_LOGC(1, ("ERROR! Fail on 256 bit encrypt test. i=%i\n", i));
			self->stop(self, ctx);
			return err;
		}
	}

	ZRTP_LOGC(3, ("OK\n"));

	ZRTP_LOG(3, (_ZTU_, "\tdecryption..."));
	
	self->set_iv(self, ctx, (zrtp_v128_t*)aes_ctr_test_nonce);

	err = self->decrypt(self, ctx, tmp_buf, sizeof(tmp_buf));
	if (zrtp_status_ok != err) {
		ZRTP_LOGC(1, ("ERROR! 256-bit AES CTR decrypt returns error %d\n", err));
		self->stop(self, ctx);
		return err;		
	}

	for (i=0; i<sizeof(aes_ctr_test_plaintext256); i++) {
		if (tmp_buf[i] != aes_ctr_test_plaintext256[i]) {
			ZRTP_LOGC(1, (_ZTU_, "ERROR! 256-bit AES CTR failed on decrypt test\n"));
			self->stop(self, ctx);
			return zrtp_status_fail;
		}
	}
	self->stop(self, ctx);
	ZRTP_LOGC(3, ("OK\n"));

	return zrtp_status_ok;	
}

zrtp_status_t zrtp_aes256_self_test(zrtp_cipher_t *self, uint8_t mode)
{
	zrtp_status_t res;
	switch (mode) {
		case ZRTP_CIPHER_MODE_CTR:
			res = zrtp_aes_ctr256_self_test(self);
			break;
		case ZRTP_CIPHER_MODE_CFB:
			res = zrtp_aes_cfb256_self_test(self);
			break;
		default:
			res = zrtp_status_bad_param;
			break;
	}
	return res;
}


/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_defaults_aes_cipher(zrtp_global_t* global_ctx)
{
	zrtp_cipher_t* cipher_aes128 = zrtp_sys_alloc(sizeof(zrtp_cipher_t));
	zrtp_cipher_t* cipher_aes256 = zrtp_sys_alloc(sizeof(zrtp_cipher_t));
	if (!cipher_aes128 || !cipher_aes256) {
		if (cipher_aes128) {
			zrtp_sys_free(cipher_aes128);
		}
		if (cipher_aes256) {
			zrtp_sys_free(cipher_aes256);
		}
		return zrtp_status_alloc_fail;
	}
    
	zrtp_memset(cipher_aes128, 0, sizeof(zrtp_cipher_t));
	zrtp_memset(cipher_aes256, 0, sizeof(zrtp_cipher_t));

	zrtp_memcpy(cipher_aes128->base.type, ZRTP_AES1, ZRTP_COMP_TYPE_SIZE);
	cipher_aes128->base.id		= ZRTP_CIPHER_AES128;
	cipher_aes128->base.zrtp	= global_ctx;	
	cipher_aes128->start		= zrtp_aes128_start;
	cipher_aes128->set_iv		= zrtp_aes_set_iv;
	cipher_aes128->encrypt		= zrtp_aes_encrypt;
	cipher_aes128->decrypt		= zrtp_aes_decrypt;
	cipher_aes128->self_test	= zrtp_aes128_self_test;
	cipher_aes128->stop			= zrtp_aes_stop;	

	zrtp_memcpy(cipher_aes256->base.type, ZRTP_AES3, ZRTP_COMP_TYPE_SIZE);
	cipher_aes256->base.id		= ZRTP_CIPHER_AES256;
	cipher_aes256->base.zrtp	= global_ctx;
	cipher_aes256->start		= zrtp_aes256_start;
	cipher_aes256->set_iv		= zrtp_aes_set_iv;
	cipher_aes256->encrypt		= zrtp_aes_encrypt;
	cipher_aes256->decrypt		= zrtp_aes_decrypt;
	cipher_aes256->self_test	= zrtp_aes256_self_test;
	cipher_aes256->stop			= zrtp_aes_stop;	
	
	zrtp_comp_register(ZRTP_CC_CIPHER, cipher_aes128, global_ctx);
    zrtp_comp_register(ZRTP_CC_CIPHER, cipher_aes256, global_ctx);	
	
	return zrtp_status_ok;		
}

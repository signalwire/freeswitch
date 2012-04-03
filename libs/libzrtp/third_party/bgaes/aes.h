/*
 ---------------------------------------------------------------------------
 Copyright (c) 1998-2006, Brian Gladman, Worcester, UK. All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products
      built using this software without specific written permission.

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue 09/09/2006

 This file contains the definitions required to use AES in C. See aesopt.h
 for optimisation details.
*/

#ifndef __aes_h__
#define __aes_h__

#include <stdlib.h>


//#include "zrtp_types.h"

/*  This include is used to find 8 & 32 bit unsigned integer types  */
#include "brg_types.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#ifndef ZRTP_RESTRICT
#define AES_128     /* define if AES with 128 bit keys is needed    */
#define AES_192     /* define if AES with 192 bit keys is needed    */
#define AES_256     /* define if AES with 256 bit keys is needed    */
#define AES_VAR     /* define if a variable key size is needed      */
#define AES_MODES   /* define if support is needed for modes        */
#else
#define AES_128     /* define if AES with 128 bit keys is needed    */
#define AES_256     /* define if AES with 256 bit keys is needed    */
#define AES_MODES   /* define if support is needed for modes        */
#endif //ZRTP_RESTRICT

/* The following must also be set in assembler files if being used  */

#define AES_ENCRYPT /* if support for encryption is needed          */

#ifndef ZRTP_RESTRICT
#define AES_DECRYPT /* if support for decryption is needed          */
#define AES_ERR_CHK /* for parameter checks & error return codes    */
#define AES_REV_DKS /* define to reverse decryption key schedule    */
#else
#define AES_DECRYPT /* if support for decryption is needed          */
#define AES_ERR_CHK /* for parameter checks & error return codes    */
#endif //ZRTP_RESTRICT

#define AES_BLOCK_SIZE  16  /* the AES block size in bytes          */
#define N_COLS           4  /* the number of columns in the state   */

/* The key schedule length is 11, 13 or 15 16-byte blocks for 128,  */
/* 192 or 256-bit keys respectively. That is 176, 208 or 240 bytes  */
/* or 44, 52 or 60 32-bit words.                                    */

#if defined( AES_VAR ) || defined( AES_256 )
#define KS_LENGTH       60
#elif defined( AES_192 )
#define KS_LENGTH       52
#else
#define KS_LENGTH       44
#endif

#if defined( AES_ERR_CHK )
#define AES_RETURN     INT_RETURN
#else
#define AES_RETURN     VOID_RETURN
#endif

/* the character array 'inf' in the following structures is used    */
/* to hold AES context information. This AES code uses cx->inf.b[0] */
/* to hold the number of rounds multiplied by 16. The other three   */
/* elements can be used by code that implements additional modes    */

typedef union
{   uint_32t l;
    uint_8t b[4];
} aes_inf;

typedef struct
{   uint_32t ks[KS_LENGTH];
    aes_inf inf;
} aes_encrypt_ctx;

typedef struct
{   uint_32t ks[KS_LENGTH];
    aes_inf inf;
} aes_decrypt_ctx;

/* This routine must be called before first use if non-static       */
/* tables are being used                                            */

AES_RETURN zrtp_bg_gen_tabs(void);

/* Key lengths in the range 16 <= key_len <= 32 are given in bytes, */
/* those in the range 128 <= key_len <= 256 are given in bits       */

#if defined( AES_ENCRYPT )

#if defined(AES_128) || defined(AES_VAR)
AES_RETURN zrtp_bg_aes_encrypt_key128(const unsigned char *key, aes_encrypt_ctx cx[1]);
#endif

#if defined(AES_192) || defined(AES_VAR)
AES_RETURN zrtp_bg_aes_encrypt_key192(const unsigned char *key, aes_encrypt_ctx cx[1]);
#endif

#if defined(AES_256) || defined(AES_VAR)
AES_RETURN zrtp_bg_aes_encrypt_key256(const unsigned char *key, aes_encrypt_ctx cx[1]);
#endif

#if defined(AES_VAR)
AES_RETURN zrtp_bg_aes_encrypt_key(const unsigned char *key, int key_len, aes_encrypt_ctx cx[1]);
#endif

AES_RETURN zrtp_bg_aes_encrypt(const unsigned char *in, unsigned char *out, const aes_encrypt_ctx cx[1]);

#endif

#if defined( AES_DECRYPT )

#if defined(AES_128) || defined(AES_VAR)
AES_RETURN zrtp_bg_aes_decrypt_key128(const unsigned char *key, aes_decrypt_ctx cx[1]);
#endif

#if defined(AES_192) || defined(AES_VAR)
AES_RETURN zrtp_bg_aes_decrypt_key192(const unsigned char *key, aes_decrypt_ctx cx[1]);
#endif

#if defined(AES_256) || defined(AES_VAR)
AES_RETURN zrtp_bg_aes_decrypt_key256(const unsigned char *key, aes_decrypt_ctx cx[1]);
#endif

#if defined(AES_VAR)
AES_RETURN zrtp_bg_aes_decrypt_key(const unsigned char *key, int key_len, aes_decrypt_ctx cx[1]);
#endif

AES_RETURN zrtp_bg_aes_decrypt(const unsigned char *in, unsigned char *out, const aes_decrypt_ctx cx[1]);

#endif

#if defined(AES_MODES)

/* Multiple calls to the following subroutines for multiple block   */
/* ECB, CBC, CFB, OFB and CTR mode encryption can be used to handle */
/* long messages incremantally provided that the context AND the iv */
/* are preserved between all such calls.  For the ECB and CBC modes */
/* each individual call within a series of incremental calls must   */
/* process only full blocks (i.e. len must be a multiple of 16) but */
/* the CFB, OFB and CTR mode calls can handle multiple incremental  */
/* calls of any length. Each mode is reset when a new AES key is    */
/* set but ECB and CBC operations can be reset without setting a    */
/* new key by setting a new IV value.  To reset CFB, OFB and CTR    */
/* without setting the key, zrtp_bg_aes_mode_reset() must be called and the */
/* IV must be set.  NOTE: All these calls update the IV on exit so  */
/* this has to be reset if a new operation with the same IV as the  */
/* previous one is required (or decryption follows encryption with  */
/* the same IV array).                                              */

AES_RETURN zrtp_bg_aes_ecb_encrypt(const unsigned char *ibuf, unsigned char *obuf,
                    int len, const aes_encrypt_ctx cx[1]);

AES_RETURN zrtp_bg_aes_ecb_decrypt(const unsigned char *ibuf, unsigned char *obuf,
                    int len, const aes_decrypt_ctx cx[1]);

#ifndef ZRTP_RESTRICT
AES_RETURN zrtp_bg_aes_cbc_encrypt(const unsigned char *ibuf, unsigned char *obuf,
                    int len, unsigned char *iv, const aes_encrypt_ctx cx[1]);

AES_RETURN zrtp_bg_aes_cbc_decrypt(const unsigned char *ibuf, unsigned char *obuf,
                    int len, unsigned char *iv, const aes_decrypt_ctx cx[1]);
#endif //ZRTP_RESTRICT

AES_RETURN zrtp_bg_aes_mode_reset(aes_encrypt_ctx cx[1]);

AES_RETURN zrtp_bg_aes_cfb_encrypt(const unsigned char *ibuf, unsigned char *obuf,
                    int len, unsigned char *iv, aes_encrypt_ctx cx[1]);

AES_RETURN zrtp_bg_aes_cfb_decrypt(const unsigned char *ibuf, unsigned char *obuf,
                    int len, unsigned char *iv, aes_encrypt_ctx cx[1]);
#ifndef ZRTP_RESTRICT
#define zrtp_bg_aes_ofb_encrypt zrtp_bg_aes_ofb_crypt
#define zrtp_bg_aes_ofb_decrypt zrtp_bg_aes_ofb_crypt

AES_RETURN aes_ofb_crypt(const unsigned char *ibuf, unsigned char *obuf,
                    int len, unsigned char *iv, aes_encrypt_ctx cx[1]);
#endif //ZRTP_RESTRICT

typedef void cbuf_inc(unsigned char *cbuf);

#define zrtp_bg_aes_ctr_encrypt zrtp_bg_aes_ctr_crypt
#define zrtp_bg_aes_ctr_decrypt zrtp_bg_aes_ctr_crypt

//[winfix]
AES_RETURN zrtp_bg_aes_ctr_crypt(const unsigned char *ibuf, unsigned char *obuf,
            int len, unsigned char *cbuf, cbuf_inc ctr_inc, aes_encrypt_ctx cx[1]);

#endif

#if defined(__cplusplus)
}
#endif

#endif

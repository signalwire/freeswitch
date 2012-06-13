/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include <setjmp.h>
#include <stdio.h>

#include "zrtp.h"
#include "cmockery/cmockery.h"

zrtp_global_t *zrtp;

void setup() {
	zrtp_status_t s;
	zrtp_config_t zrtp_config;
	
	zrtp_config_defaults(&zrtp_config);
	
	s = zrtp_init(&zrtp_config, &zrtp);
	assert_int_equal(s, zrtp_status_ok);
}

void teardown() {
	zrtp_down(zrtp);
}

static void aes128_ctr_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CTR);
}

static void aes128_cfb_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CFB);
}

static void aes256_ctr_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES256, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CTR);
}

static void aes256_cfb_test() {
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES256, zrtp);
	assert_non_null(cipher);
	cipher->self_test(cipher, ZRTP_CIPHER_MODE_CFB);
}
/*
static void aes256_cfb_test2() {
		rtp_aes_cfb_ctx_t *ctx = (zrtp_aes_cfb_ctx_t*)self->start( self,
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

}
*/
int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(aes128_ctr_test, setup, teardown),
		unit_test_setup_teardown(aes128_cfb_test, setup, teardown),
		unit_test_setup_teardown(aes256_ctr_test, setup, teardown),
		unit_test_setup_teardown(aes256_cfb_test, setup, teardown),
  	};

	return run_tests(tests);
}

/*
 * Copyright (c) 2007-2014, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ks.h>

static ks_mutex_t **ssl_mutexes;
static ks_pool_t *ssl_pool = NULL;
static int ssl_count = 0;
static int is_init = 0;

static inline void ks_ssl_lock_callback(int mode, int type, char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		ks_mutex_lock(ssl_mutexes[type]);
	}
	else {
		ks_mutex_unlock(ssl_mutexes[type]);
	}
}

static inline unsigned long ks_ssl_thread_id(void)
{
	return (unsigned long) ks_thread_self();
}

KS_DECLARE(void) ks_ssl_init_ssl_locks(void)
{

	int i, num;
	
	if (is_init) return;

	is_init = 1;

	SSL_library_init();

	if (ssl_count == 0) {
		num = CRYPTO_num_locks();
		
		ssl_mutexes = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(ks_mutex_t*));
		ks_assert(ssl_mutexes != NULL);

		ks_pool_open(&ssl_pool);

		for (i = 0; i < num; i++) {
			ks_mutex_create(&(ssl_mutexes[i]), KS_MUTEX_FLAG_DEFAULT, ssl_pool);
			ks_assert(ssl_mutexes[i] != NULL);
		}

		CRYPTO_set_id_callback(ks_ssl_thread_id);
		CRYPTO_set_locking_callback((void (*)(int, int, const char*, int))ks_ssl_lock_callback);
	}

	ssl_count++;
}

KS_DECLARE(void) ks_ssl_destroy_ssl_locks(void)
{
	int i;

	if (!is_init) return;

	is_init = 0;

	if (ssl_count == 1) {
		CRYPTO_set_locking_callback(NULL);
		for (i = 0; i < CRYPTO_num_locks(); i++) {
			if (ssl_mutexes[i]) {
				ks_mutex_destroy(&ssl_mutexes[i]);
			}
		}

		OPENSSL_free(ssl_mutexes);
		ssl_count--;
	}
}



static int mkcert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days);

KS_DECLARE(int) ks_gen_cert(const char *dir, const char *file)
{
	//BIO *bio_err;
	X509 *x509 = NULL;
	EVP_PKEY *pkey = NULL;
	char *rsa = NULL, *pvt = NULL;
	FILE *fp;
	char *pem = NULL;

	if (ks_stristr(".pem", file)) {
		pem = ks_mprintf("%s%s%s", dir, KS_PATH_SEPARATOR, file);
	} else {
		pvt = ks_mprintf("%s%s%s.key", dir, KS_PATH_SEPARATOR, file);
		rsa = ks_mprintf("%s%s%s.crt", dir, KS_PATH_SEPARATOR, file);
	}

	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
		
	//bio_err=BIO_new_fp(stderr, BIO_NOCLOSE);
		
	mkcert(&x509, &pkey, 1024, 0, 36500);

	//RSA_print_fp(stdout, pkey->pkey.rsa, 0);
	//X509_print_fp(stdout, x509);

	if (pem) {
		if ((fp = fopen(pem, "w"))) {
			PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL);
			PEM_write_X509(fp, x509);
			fclose(fp);
		}

	} else {
		if (pvt && (fp = fopen(pvt, "w"))) {
			PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL);
			fclose(fp);
		}
		
		if (rsa && (fp = fopen(rsa, "w"))) {
			PEM_write_X509(fp, x509);
			fclose(fp);
		}
	}

	X509_free(x509);
	EVP_PKEY_free(pkey);

#ifndef OPENSSL_NO_ENGINE
	ENGINE_cleanup();
#endif
	CRYPTO_cleanup_all_ex_data();

	//CRYPTO_mem_leaks(bio_err);
	//BIO_free(bio_err);


	ks_safe_free(pvt);
	ks_safe_free(rsa);
	ks_safe_free(pem);

	return(0);
}

static int mkcert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days)
{
	X509 *x;
	EVP_PKEY *pk;
	RSA *rsa;
	X509_NAME *name=NULL;
	
	ks_assert(pkeyp);
	ks_assert(x509p);

	if (*pkeyp == NULL) {
		if ((pk = EVP_PKEY_new()) == NULL) {
			abort(); 
		}
	} else {
		pk = *pkeyp;
	}

	if (*x509p == NULL) {
		if ((x = X509_new()) == NULL) {
			goto err;
		}
	} else {
		x = *x509p;
	}

	rsa = RSA_generate_key(bits, RSA_F4, NULL, NULL);

	if (!EVP_PKEY_assign_RSA(pk, rsa)) {
		abort();
		goto err;
	}

	rsa = NULL;

	X509_set_version(x, 0);
	ASN1_INTEGER_set(X509_get_serialNumber(x), serial);
	X509_gmtime_adj(X509_get_notBefore(x), -(long)60*60*24*7);
	X509_gmtime_adj(X509_get_notAfter(x), (long)60*60*24*days);
	X509_set_pubkey(x, pk);

	name = X509_get_subject_name(x);

	/* This function creates and adds the entry, working out the
	 * correct string type and performing checks on its length.
	 * Normally we'd check the return value for errors...
	 */
	X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"US", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"FreeSWITCH-libKS", -1, -1, 0);
							   

	/* Its self signed so set the issuer name to be the same as the
 	 * subject.
	 */
	X509_set_issuer_name(x, name);

	if (!X509_sign(x, pk, EVP_sha1()))
		goto err;

	*x509p = x;
	*pkeyp = pk;
	return(1);
 err:
	return(0);
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_cert.c -- Cert Functions
 *
 */

#include <switch.h>
#include <switch_ssl.h>

static switch_mutex_t **ssl_mutexes;
static switch_memory_pool_t *ssl_pool = NULL;
static int ssl_count = 0;

static inline void switch_ssl_ssl_lock_callback(int mode, int type, char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		switch_mutex_lock(ssl_mutexes[type]);
	}
	else {
		switch_mutex_unlock(ssl_mutexes[type]);
	}
}

static inline unsigned long switch_ssl_ssl_thread_id(void)
{
	return (unsigned long) switch_thread_self();
}

SWITCH_DECLARE(void) switch_ssl_init_ssl_locks(void)
{

	int i, num;

	if (ssl_count == 0) {
		num = CRYPTO_num_locks();
		
		ssl_mutexes = OPENSSL_malloc(CRYPTO_num_locks() * sizeof(switch_mutex_t*));
		switch_assert(ssl_mutexes != NULL);

		switch_core_new_memory_pool(&ssl_pool);

		for (i = 0; i < num; i++) {
			switch_mutex_init(&(ssl_mutexes[i]), SWITCH_MUTEX_NESTED, ssl_pool);
			switch_assert(ssl_mutexes[i] != NULL);
		}

		CRYPTO_set_id_callback(switch_ssl_ssl_thread_id);
		CRYPTO_set_locking_callback((void (*)(int, int, const char*, int))switch_ssl_ssl_lock_callback);
	}

	ssl_count++;
}

SWITCH_DECLARE(void) switch_ssl_destroy_ssl_locks(void)
{
	int i;

	if (ssl_count == 1) {
		CRYPTO_set_locking_callback(NULL);
		for (i = 0; i < CRYPTO_num_locks(); i++) {
			if (ssl_mutexes[i]) {
				switch_mutex_destroy(ssl_mutexes[i]);
			}
		}

		OPENSSL_free(ssl_mutexes);
		ssl_count--;
	}
}

static const EVP_MD *get_evp_by_name(const char *name)
{
	if (!strcasecmp(name, "md5")) return EVP_md5();
	if (!strcasecmp(name, "sha1")) return EVP_sha1();
	if (!strcasecmp(name, "sha-256")) return EVP_sha256();
	if (!strcasecmp(name, "sha-512")) return EVP_sha512();

	return NULL;
}
#if defined(_MSC_VER) || (defined(__SunOS_5_10) && defined(__SUNPRO_C))
/*
 * Visual C do not have strsep?
 *
 * Solaris 10 with the Sun Studio compilers doesn't have strsep in the
 * C library either.
 */
char
    *strsep(char **stringp, const char *delim)
{
	char *res;

	if (!stringp || !*stringp || !**stringp)
		return (char *) 0;

	res = *stringp;
	while (**stringp && !strchr(delim, **stringp))
		++(*stringp);

	if (**stringp) {
		**stringp = '\0';
		++(*stringp);
	}

	return res;
}
#endif

SWITCH_DECLARE(int) switch_core_cert_verify(dtls_fingerprint_t *fp)
{
	unsigned char fdata[MAX_FPLEN] = { 0 };
	char *tmp = strdup(fp->str);
	char *p = tmp;
	int i = 0;
	char *v;

	while ((v = strsep(&p, ":")) && (i != (MAX_FPLEN - 1))) {
		sscanf(v, "%02x", (uint32_t *) &fdata[i++]);
	}
	
	free(tmp);

	i = !memcmp(fdata, fp->data, i);

	return i;
}

SWITCH_DECLARE(int) switch_core_cert_expand_fingerprint(dtls_fingerprint_t *fp, const char *str)
{
	char *tmp = strdup(str);
	char *p = tmp;
	int i = 0;
	char *v;

	while ((v = strsep(&p, ":")) && (i != (MAX_FPLEN - 1))) {
		sscanf(v, "%02x", (uint32_t *) &fp->data[i++]);
	}
	
	free(tmp);

	return i;
}

SWITCH_DECLARE(int) switch_core_cert_extract_fingerprint(X509* x509, dtls_fingerprint_t *fp)
{
	const EVP_MD *evp;
	unsigned int i, j;

	evp = get_evp_by_name(fp->type);
	
	if (X509_digest(x509, evp, fp->data, &fp->len) != 1 ||  fp->len <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "FP DIGEST ERR!\n");
		return -1;
	}

	for (i = 0, j = 0; i < fp->len; ++i, j += 3){
		sprintf((char*)&fp->str[j], (i == (fp->len - 1)) ? "%.2X" : "%.2X:", fp->data[i]);
	}
	*(&fp->str[fp->len * 3]) = '\0';

	return 0;
	
}

SWITCH_DECLARE(int) switch_core_cert_gen_fingerprint(const char *prefix, dtls_fingerprint_t *fp)
{
	X509* x509 = NULL;
	BIO* bio = NULL;
	int ret = 0;
	char *rsa;


	rsa = switch_mprintf("%s%s%s.crt", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, prefix);

	if (!(bio = BIO_new(BIO_s_file()))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "FP BIO ERR!\n");
		goto end;
	}
	
	if (BIO_read_filename(bio, rsa) != 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "FP FILE ERR!\n");
		goto end;
	}

	if (!(x509 = PEM_read_bio_X509(bio, NULL, 0, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "FP READ ERR!\n");
		goto end;
	}

	switch_core_cert_extract_fingerprint(x509, fp);
	
	ret = 1;

 end:

	if (bio) {
		BIO_free_all(bio);
	}

	if (x509) {
		X509_free(x509);
	}

	free(rsa);
	
	return ret;
}


static int mkcert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days);
static int add_ext(X509 *cert, int nid, char *value);

SWITCH_DECLARE(int) switch_core_gen_certs(const char *prefix)
{
	//BIO *bio_err;
	X509 *x509 = NULL;
	EVP_PKEY *pkey = NULL;
	char *rsa = NULL, *pvt = NULL;
	FILE *fp;
	char *pem = NULL;

	if (switch_stristr(".pem", prefix)) {

		if (switch_is_file_path(prefix)) {
			pem = strdup(prefix);
		} else {
			pem = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, prefix);
		}

		if (switch_file_exists(pem, NULL) == SWITCH_STATUS_SUCCESS) {
			goto end;
		}
	} else {
		if (switch_is_file_path(prefix)) {
			pvt = switch_mprintf("%s.key", prefix);
			rsa = switch_mprintf("%s.crt", prefix);
		} else {
			pvt = switch_mprintf("%s%s%s.key", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, prefix);
			rsa = switch_mprintf("%s%s%s.crt", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, prefix);
		}

		if (switch_file_exists(pvt, NULL) == SWITCH_STATUS_SUCCESS || switch_file_exists(rsa, NULL) == SWITCH_STATUS_SUCCESS) {
			goto end;
		}
	}

	CRYPTO_mem_ctrl(CRYPTO_MEM_CHECK_ON);
		
	//bio_err=BIO_new_fp(stderr, BIO_NOCLOSE);
		
	mkcert(&x509, &pkey, 2048, 0, 365);

	//RSA_print_fp(stdout, pkey->pkey.rsa, 0);
	//X509_print_fp(stdout, x509);

	if (pem) {
		if ((fp = fopen(pem, "w"))) {
			PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL);
			PEM_write_X509(fp, x509);
			fclose(fp);
		}

	} else {
		if ((fp = fopen(pvt, "w"))) {
			PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL);
			fclose(fp);
		}
		
		if ((fp = fopen(rsa, "w"))) {
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


 end:

	switch_safe_free(pvt);
	switch_safe_free(rsa);
	switch_safe_free(pem);

	return(0);
}

static void callback(int p, int n, void *arg)
{
	char c='B';

	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	fputc(c, stderr);
}

static int mkcert(X509 **x509p, EVP_PKEY **pkeyp, int bits, int serial, int days)
{
	X509 *x;
	EVP_PKEY *pk;
	RSA *rsa;
	X509_NAME *name=NULL;
	
	if ((pkeyp == NULL) || (*pkeyp == NULL))
		{
			if ((pk=EVP_PKEY_new()) == NULL)
				{
					abort(); 
				}
		}
	else
		pk= *pkeyp;

	if ((x509p == NULL) || (*x509p == NULL))
		{
			if ((x=X509_new()) == NULL)
				goto err;
		}
	else
		x= *x509p;

	rsa=RSA_generate_key(bits, RSA_F4, callback, NULL);
	if (!EVP_PKEY_assign_RSA(pk, rsa))
		{
			abort();
			goto err;
		}
	rsa=NULL;

	X509_set_version(x, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(x), serial);
	X509_gmtime_adj(X509_get_notBefore(x), 0);
	X509_gmtime_adj(X509_get_notAfter(x), (long)60*60*24*days);
	X509_set_pubkey(x, pk);

	name=X509_get_subject_name(x);

	/* This function creates and adds the entry, working out the
	 * correct string type and performing checks on its length.
	 * Normally we'd check the return value for errors...
	 */
	X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"US", -1, -1, 0);
							   
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"FreeSWITCH", -1, -1, 0);
							   

	/* Its self signed so set the issuer name to be the same as the
 	 * subject.
	 */
	X509_set_issuer_name(x, name);

	/* Add various extensions: standard extensions */
	add_ext(x, NID_basic_constraints, "critical, CA:TRUE");
	add_ext(x, NID_key_usage, "critical, keyCertSign, cRLSign");

	add_ext(x, NID_subject_key_identifier, "hash");

	/* Some Netscape specific extensions */
	add_ext(x, NID_netscape_cert_type, "sslCA");

	add_ext(x, NID_netscape_comment, "Self-Signed CERT for DTLS");


	if (!X509_sign(x, pk, EVP_sha1()))
		goto err;

	*x509p=x;
	*pkeyp=pk;
	return(1);
 err:
	return(0);
}

/* Add extension using V3 code: we can set the config file as NULL
 * because we wont reference any other sections.
 */

static int add_ext(X509 *cert, int nid, char *value)
{
	X509_EXTENSION *ex;
	X509V3_CTX ctx;
	/* This sets the 'context' of the extensions. */
	/* No configuration database */
	X509V3_set_ctx_nodb(&ctx);
	/* Issuer and subject certs: both the target since it is self signed, 
	 * no request and no CRL
	 */
	X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
	ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);
	if (!ex)
		return 0;

	X509_add_ext(cert, ex, -1);
	X509_EXTENSION_free(ex);
	return 1;
}
	

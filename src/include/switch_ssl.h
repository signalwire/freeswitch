/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * switch_ssl.h
 *
 */

#ifndef __SWITCH_SSL_H
#define __SWITCH_SSL_H

#include "switch.h"
#include <string.h>

SWITCH_BEGIN_EXTERN_C

#if defined(HAVE_OPENSSL)
#if defined (MACOSX) || defined(DARWIN)
/* Disable depricated-declarations on OS X */
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/conf.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

SWITCH_DECLARE(int) switch_core_cert_extract_fingerprint(X509* x509, dtls_fingerprint_t *fp);

/* the assistant macros for compatible with openssl < 1.1.0 */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define BIO_set_init(bio, val)     ((bio)->init = (val))
#define BIO_set_data(bio, filter)  ((bio)->ptr = (filter))
#define BIO_clear_flags(bio, flag) ((bio)->flags &= ~(flag))
#define BIO_get_data(bio)          ((bio) ? (bio)->ptr : NULL)
#define BIO_next(bio)              ((bio) ? (bio)->next_bio : NULL)
#define BIO_get_new_index()        (0)

#define BIO_meth_set_write(biom, fn_write)     ((biom)->bwrite = (fn_write))
#define BIO_meth_set_ctrl(biom, fn_ctrl)       ((biom)->ctrl = (fn_ctrl))
#define BIO_meth_set_create(biom, fn_create)   ((biom)->create = (fn_create))
#define BIO_meth_set_destroy(biom, fn_destroy) ((biom)->destroy = (fn_destroy))
#define BIO_meth_new switch_ssl_BIO_meth_new
#define BIO_meth_free(biom)                                               \
	do {                                                                  \
		if (biom != NULL) {                                               \
			if (biom->name != NULL) { OPENSSL_free((char *)biom->name); } \
			OPENSSL_free(biom);                                           \
		}                                                                 \
	} while (0)

#define BIO_F_BIO_METH_NEW BIO_F_BIO_NEW_MEM_BUF
static inline BIO_METHOD *switch_ssl_BIO_meth_new(int type, const char *name) {
	BIO_METHOD *biom = OPENSSL_malloc(sizeof(BIO_METHOD));

	if (biom == NULL) {
		BIOerr(BIO_F_BIO_METH_NEW, ERR_R_MALLOC_FAILURE);
		return NULL;
	}
	memset(biom, 0, sizeof(*biom));
	biom->name = OPENSSL_strdup(name);
	if (biom->name == NULL) {
		OPENSSL_free(biom);
		BIOerr(BIO_F_BIO_METH_NEW, ERR_R_MALLOC_FAILURE);
	}
	biom->type = type;
	return biom;
}

#endif /* end OPENSSL_VERSION_NUMBER */

#else
static inline int switch_core_cert_extract_fingerprint(void* x509, dtls_fingerprint_t *fp) { return 0; }
#endif /* end HAVE_OPENSSL */

SWITCH_DECLARE(void) switch_ssl_destroy_ssl_locks(void);
SWITCH_DECLARE(void) switch_ssl_init_ssl_locks(void);


SWITCH_END_EXTERN_C

#endif /* end __SWITCH_SSL_H */

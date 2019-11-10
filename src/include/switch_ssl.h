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

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define HAVE_OPENSSL_1_1_API
#endif

#ifdef HAVE_OPENSSL_1_1_API
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#else
#define X509_get0_notBefore(X) X509_get_notBefore(X)
#define X509_get0_notAfter(X) X509_get_notAfter(X)
#endif

SWITCH_DECLARE(int) switch_core_cert_extract_fingerprint(X509* x509, dtls_fingerprint_t *fp);

#else
static inline int switch_core_cert_extract_fingerprint(void* x509, dtls_fingerprint_t *fp) { return 0; }
#endif

#ifndef HAVE_OPENSSL_1_1_API
SWITCH_DECLARE(void) switch_ssl_destroy_ssl_locks(void);
SWITCH_DECLARE(void) switch_ssl_init_ssl_locks(void);
#endif

#endif

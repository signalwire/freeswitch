/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Daniel Swarbrick <freeswitch@pressure.net.nz>
 *
 *
 * mod_spidermonkey_rsa.c -- OpenSSL RSA Javascript Module
 *
 */
#include "mod_spidermonkey.h"

/* OpenSSL includes */
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/pkcs12.h>
#include <openssl/rsa.h>

static const char modname[] = "RSA";

struct rsa_obj {
    RSA *rsa;
    JSContext *cx;
    JSObject *obj;
    JSFunction *function;
    JSObject *user_data;
    jsrefcount saveDepth;
    jsval ret;
};

/* RSA Object */
/*********************************************************************************/
static JSBool rsa_construct(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct rsa_obj *ro = NULL;

	ro = malloc(sizeof(*ro));
	switch_assert(ro);

	memset(ro, 0, sizeof(*ro));
    
	ro->rsa = RSA_new();
	if (ro->rsa == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OpenSSL RSA_new() error: %s\n", ERR_error_string(ERR_get_error(), NULL));
	}

	ro->cx = cx;
	ro->obj = obj;
    
	JS_SetPrivate(cx, obj, ro);

	return JS_TRUE;
}

static void rsa_destroy(JSContext * cx, JSObject * obj)
{
	struct rsa_obj *ro = JS_GetPrivate(cx, obj);

    if (ro && ro->rsa) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Freeing RSA struct: RSA_free()\n");
		RSA_free(ro->rsa);
	}

	switch_safe_free(ro);
	JS_SetPrivate(cx, obj, NULL);
}

static JSBool read_rsa_pub_key(JSContext * cx, JSObject * obj, uintN argc, jsval * argv, jsval * rval)
{
	struct rsa_obj *ro = JS_GetPrivate(cx, obj);
    EVP_PKEY * key = NULL;
    BIO *bio_in;
    char * inputstr = NULL;

    if (argc < 1 || !ro) {
        return JS_FALSE;
    }

    inputstr = JS_GetStringBytes(JS_ValueToString(cx, argv[0]));

    bio_in = BIO_new_mem_buf(inputstr, sizeof(inputstr));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PUBKEY: %s\n", inputstr);
    key = PEM_read_bio_PUBKEY(bio_in, NULL, NULL, NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "OpenSSL PEM_read_bio_PUBKEY: %s\n", ERR_error_string(ERR_get_error(), NULL));
    BIO_free(bio_in);

	return JS_FALSE;
}

static JSFunctionSpec rsa_methods[] = {
	{"readRSAPublicKey", read_rsa_pub_key, 1},
	{0}
};


static JSPropertySpec rsa_props[] = {
	{0}
};


static JSBool rsa_getProperty(JSContext * cx, JSObject * obj, jsval id, jsval * vp)
{
	JSBool res = JS_TRUE;

	return res;
}

JSClass rsa_class = {
	modname, JSCLASS_HAS_PRIVATE,
	JS_PropertyStub, JS_PropertyStub, rsa_getProperty, DEFAULT_SET_PROPERTY,
	JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, rsa_destroy, NULL, NULL, NULL,
	rsa_construct
};


switch_status_t rsa_load(JSContext * cx, JSObject * obj)
{
	JS_InitClass(cx, obj, NULL, &rsa_class, rsa_construct, 3, rsa_props, rsa_methods, rsa_props, rsa_methods);
	return SWITCH_STATUS_SUCCESS;
}


const sm_module_interface_t rsa_module_interface = {
	/*.name = */ modname,
	/*.spidermonkey_load */ rsa_load,
	/*.next */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) spidermonkey_init(const sm_module_interface_t ** module_interface)
{
	SSL_library_init();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_algorithms();

	ERR_load_ERR_strings();
	ERR_load_crypto_strings();
	ERR_load_EVP_strings();

	*module_interface = &rsa_module_interface;
	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

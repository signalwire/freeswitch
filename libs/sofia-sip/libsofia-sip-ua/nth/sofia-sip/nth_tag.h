/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef NTH_TAG_H
/** Defined when <sofia-sip/nth_tag.h> has been included. */
#define NTH_TAG_H

/**@file sofia-sip/nth_tag.h
 * @brief Tags for @b nth, HTTP engine module.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Sun Oct 13 22:23:48 2002 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

#ifndef URL_TAG_H
#include <sofia-sip/url_tag.h>
#endif

#ifndef HTTP_TAG_H
#include <sofia-sip/http_tag.h>
#endif

SOFIA_BEGIN_DECLS

/** List of all nth tags */
NTH_DLL extern tagi_t nth_client_tags[];

/** Filter tag matching any nth tag. */
#define NTHTAG_ANY()         nthtag_any, ((tag_value_t)0)
NTH_DLL extern tag_typedef_t nthtag_any;

/* Common tags */

NTH_DLL extern tag_typedef_t nthtag_mclass;
/** Pointer to a mclass, message factory object. @HI */
#define NTHTAG_MCLASS(x) nthtag_mclass, tag_cptr_v((x))

NTH_DLL extern tag_typedef_t nthtag_mclass_ref;
#define NTHTAG_MCLASS_REF(x) nthtag_mclass_ref, tag_cptr_vr(&(x), (x))

NTH_DLL extern tag_typedef_t nthtag_mflags;
/** Message flags used by nth_engine_msg_create()/nth_site_msg(). @HI */
#define NTHTAG_MFLAGS(x) nthtag_mflags, tag_int_v((x))

NTH_DLL extern tag_typedef_t nthtag_mflags_ref;
#define NTHTAG_MFLAGS_REF(x) nthtag_mflags_ref, tag_int_vr(&(x))

NTH_DLL extern tag_typedef_t nthtag_streaming;
/** Enable streaming. @HI */
#define NTHTAG_STREAMING(x) nthtag_streaming, tag_bool_v((x))

NTH_DLL extern tag_typedef_t nthtag_streaming_ref;
#define NTHTAG_STREAMING_REF(x) nthtag_streaming_ref, tag_bool_vr(&(x))

/* Client-only tags */

NTH_DLL extern tag_typedef_t nthtag_proxy;
/** URL for (default) proxy. @HI */
#define NTHTAG_PROXY(x) nthtag_proxy, urltag_url_v((x))

NTH_DLL extern tag_typedef_t nthtag_proxy_ref;
#define NTHTAG_PROXY_REF(x) nthtag_proxy_ref, urltag_url_vr(&(x))

NTH_DLL extern tag_typedef_t nthtag_expires;
/** Expires in milliseconds for client transactions. @HI */
#define NTHTAG_EXPIRES(x) nthtag_expires, tag_uint_v((x))

NTH_DLL extern tag_typedef_t nthtag_expires_ref;
#define NTHTAG_EXPIRES_REF(x) nthtag_expires_ref, tag_uint_vr(&(x))

NTH_DLL extern tag_typedef_t nthtag_error_msg;
/** If true, nth engine generates complete error messages. @HI */
#define NTHTAG_ERROR_MSG(x) nthtag_error_msg, tag_bool_v((x))

NTH_DLL extern tag_typedef_t nthtag_error_msg_ref;
#define NTHTAG_ERROR_MSG_REF(x) nthtag_error_msg_ref, tag_bool_vr(&(x))

#if SU_INLINE_TAG_CAST
struct nth_client_s;
su_inline tag_value_t nthtag_template_v(struct nth_client_s const *v)
{ return (tag_value_t)v; }
su_inline tag_value_t nthtag_template_vr(struct nth_client_s const **vp)
{return(tag_value_t)vp;}
#else
#define nthtag_template_v(v) ((tag_value_t)(v))
#define nthtag_template_vr(vp) ((tag_value_t)(vp))
#endif

NTH_DLL extern tag_typedef_t nthtag_template;
/** Use existing client request as template. @HI */
#define NTHTAG_TEMPLATE(x) nthtag_template, nthtag_template_v((x))

NTH_DLL extern tag_typedef_t nthtag_template_ref;
#define NTHTAG_TEMPLATE_REF(x) nthtag_template_ref, nthtag_template_vr(&(x))

#if SU_INLINE_TAG_CAST
su_inline tag_value_t nthtag_message_v(struct msg_s *v)
{ return (tag_value_t)v; }
su_inline tag_value_t nthtag_message_vr(struct msg_s **vp)
{ return(tag_value_t)vp; }
#else
#define nthtag_message_v(v) ((tag_value_t)(v))
#define nthtag_message_vr(vp) ((tag_value_t)(vp))
#endif

NTH_DLL extern tag_typedef_t nthtag_message;
/** Use existing request message. @HI */
#define NTHTAG_MESSAGE(x) nthtag_message, nthtag_message_v((x))

NTH_DLL extern tag_typedef_t nthtag_message_ref;
#define NTHTAG_MESSAGE_REF(x) nthtag_message_ref, nthtag_message_vr(&(x))

#if SU_HAVE_INLINE
struct auth_client_s;
su_inline tag_value_t nthtag_authentication_v(struct auth_client_s **v) { return (tag_value_t)v; }
su_inline tag_value_t nthtag_authentication_vr(struct auth_client_s ***vp) {return(tag_value_t)vp;}
#else
#define nthtag_authentication_v(v) ((tag_value_t)(v))
#define nthtag_authentication_vr(vp) ((tag_value_t)(vp))
#endif

NTH_DLL extern tag_typedef_t nthtag_authentication;
/** Use stack of authenticators. @HI */
#define NTHTAG_AUTHENTICATION(x) \
nthtag_authentication, nthtag_authentication_v((x))

NTH_DLL extern tag_typedef_t nthtag_authentication_ref;
#define NTHTAG_AUTHENTICATION_REF(x) \
nthtag_authentication_ref, nthtag_authentication_vr(&(x))

NTH_DLL extern tag_typedef_t nthtag_max_retry_after;
/** Maximum value for retry interval. @HI */
#define NTHTAG_MAX_RETRY_AFTER(x) nthtag_max_retry_after, tag_int_v((x))

NTH_DLL extern tag_typedef_t nthtag_max_retry_after_ref;
#define NTHTAG_MAX_RETRY_AFTER_REF(x) \
nthtag_max_retry_after_ref, tag_int_vr(&(x))

/* Server-side tags */

NTH_DLL extern tag_typedef_t nthtag_root;
/** Pointer to root reactor object. @HI */
#define NTHTAG_ROOT(x) nthtag_root, tag_ptr_v((x))

NTH_DLL extern tag_typedef_t nthtag_root_ref;
#define NTHTAG_ROOT_REF(x) nthtag_root_ref, tag_ptr_vr(&(x), (x))

NTH_DLL extern tag_typedef_t nthtag_strict_host;
/** Do not serve requests to mismatching hosts by default host. @HI */
#define NTHTAG_STRICT_HOST(x) nthtag_strict_host, tag_bool_v((x))

NTH_DLL extern tag_typedef_t nthtag_strict_host_ref;
#define NTHTAG_STRICT_HOST_REF(x) nthtag_strict_host_ref, tag_bool_vr(&(x))

NTH_DLL extern tag_typedef_t nthtag_auth_module;
/** Pointer to authentication module. @HI. @NEW_1_12_4. */
#define NTHTAG_AUTH_MODULE(x) nthtag_auth_module, tag_ptr_v((x))

NTH_DLL extern tag_typedef_t nthtag_auth_module_ref;
#define NTHTAG_AUTH_MODULE_REF(x) nthtag_auth_module_ref, tag_ptr_vr(&(x), (x))

SOFIA_END_DECLS

#endif /* !defined NTH_TAG_H */

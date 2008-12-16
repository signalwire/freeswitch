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

#ifndef MSG_HEADER_H
/** Defined when <sofia-sip/msg_header.h> has been included. */
#define MSG_HEADER_H
/**@ingroup msg_headers
 * @file sofia-sip/msg_header.h
 *
 * @brief Message headers.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Aug 27 15:44:27 2001 ppessi
 *
 */

#include <stdarg.h>
#include <string.h>

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif
#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif
#ifndef MSG_H
#include <sofia-sip/msg.h>
#endif
#ifndef URL_H
#include <sofia-sip/url.h>
#endif

SOFIA_BEGIN_DECLS

SOFIAPUBFUN msg_header_t *msg_header_alloc(su_home_t *,
					   msg_hclass_t *hc,
					   isize_t extra)
  __attribute__((__malloc__));

SOFIAPUBFUN isize_t msg_header_size(msg_header_t const *h);

SOFIAPUBFUN msg_header_t **msg_header_offset(msg_t const *,
					 msg_pub_t const *,
					 msg_header_t const *);
SOFIAPUBFUN msg_header_t **msg_hclass_offset(msg_mclass_t const *,
					     msg_pub_t const *,
					     msg_hclass_t *);
SOFIAPUBFUN msg_header_t *msg_header_access(msg_pub_t const *pub,
					    msg_hclass_t *hc);

SOFIAPUBFUN msg_header_t *msg_header_copy_as(su_home_t *home,
					     msg_hclass_t *hc,
					     msg_header_t const *o)
  __attribute__((__malloc__));
SOFIAPUBFUN msg_header_t *msg_header_copy(su_home_t *home,
					  msg_header_t const *o)
  __attribute__((__malloc__));
SOFIAPUBFUN msg_header_t *msg_header_copy_one(su_home_t *home,
					      msg_header_t const *o)
  __attribute__((__malloc__));
SOFIAPUBFUN msg_header_t *msg_header_dup_as(su_home_t *home,
					    msg_hclass_t *hc,
					    msg_header_t const *o)
  __attribute__((__malloc__));
SOFIAPUBFUN msg_header_t *msg_header_dup(su_home_t *home,
					 msg_header_t const *h)
  __attribute__((__malloc__));
SOFIAPUBFUN msg_header_t *msg_header_dup_one(su_home_t *home,
					     msg_header_t const *h)
  __attribute__((__malloc__));

SOFIAPUBFUN msg_header_t *msg_header_d(su_home_t *home,
				       msg_t const *msg,
				       char const *b);
SOFIAPUBFUN issize_t msg_header_e(char b[], isize_t bsiz,
				  msg_header_t const *h,
				  int flags);
SOFIAPUBFUN issize_t msg_object_e(char b[], isize_t size,
				  msg_pub_t const *mo,
				  int flags);

SOFIAPUBFUN issize_t msg_header_field_e(char b[], isize_t bsiz,
					msg_header_t const *h,
					int flags);

SOFIAPUBFUN int msg_header_remove(msg_t *msg,
				  msg_pub_t *mo,
				  msg_header_t *h);

SOFIAPUBFUN int msg_header_remove_all(msg_t *msg,
				      msg_pub_t *mo,
				      msg_header_t *h);

SOFIAPUBFUN int msg_header_insert(msg_t *msg, msg_pub_t *mo,
				  msg_header_t *h);

SOFIAPUBFUN int msg_header_replace(msg_t *msg, msg_pub_t *mo,
				   msg_header_t *old_header,
				   msg_header_t *new_header);

SOFIAPUBFUN int msg_header_add_dup(msg_t *msg,
				   msg_pub_t *pub,
				   msg_header_t const *o);

SOFIAPUBFUN int msg_header_add_str(msg_t *msg,
				   msg_pub_t *pub,
				   char const *str);

SOFIAPUBFUN int msg_header_parse_str(msg_t *msg,
				     msg_pub_t *pub,
				     char *s);

SOFIAPUBFUN int msg_header_add_dup_as(msg_t *msg,
				      msg_pub_t *pub,
				      msg_hclass_t *hc,
				      msg_header_t const *o);

SOFIAPUBFUN int msg_header_add_make(msg_t *msg,
				    msg_pub_t *pub,
				    msg_hclass_t *hc,
				    char const *s);

SOFIAPUBFUN int msg_header_add_format(msg_t *msg,
				      msg_pub_t *pub,
				      msg_hclass_t *hc,
				      char const *fmt,
				      ...);

SOFIAPUBFUN int msg_header_prepend(msg_t *msg,
				   msg_pub_t *pub,
				   msg_header_t **hh,
				   msg_header_t *h);

SOFIAPUBFUN msg_header_t *msg_header_make(su_home_t *home,
					  msg_hclass_t *hc,
					  char const *s)
     __attribute__((__malloc__));

SOFIAPUBFUN msg_header_t *msg_header_format(su_home_t *home,
					    msg_hclass_t *hc,
					    char const *fmt, ...)
     __attribute__ ((__malloc__, __format__ (printf, 3, 4)));

SOFIAPUBFUN msg_header_t *msg_header_vformat(su_home_t *home,
					     msg_hclass_t *hc,
					     char const *fmt,
					     va_list ap)
     __attribute__((__malloc__));


SOFIAPUBFUN void msg_header_free(su_home_t *home,
				 msg_header_t *h);

SOFIAPUBFUN void msg_header_free_all(su_home_t *home,
				     msg_header_t *h);

SOFIAPUBFUN msg_payload_t *msg_payload_create(su_home_t *home,
					      void const *data,
					      usize_t len)
     __attribute__((__malloc__));

SOFIAPUBFUN msg_separator_t *msg_separator_create(su_home_t *home)
  __attribute__((__malloc__));

/* Chunk handling macros */

/** Get pointer to beginning of available buffer space */
#define MSG_CHUNK_BUFFER(pl) \
  ((char *)pl->pl_common->h_data + (pl)->pl_common->h_len)
/** Get size of available buffer space */
#define MSG_CHUNK_AVAIL(pl) \
  ((pl)->pl_len + ((pl)->pl_data - (char *)pl->pl_common->h_data) - \
   (pl)->pl_common->h_len)
/** Get next chunk in list */
#define MSG_CHUNK_NEXT(pl) \
  ((pl)->pl_next)

SOFIAPUBFUN issize_t msg_headers_prepare(msg_t *,
					 msg_header_t *headers,
					 int flags);

#ifdef SU_HAVE_INLINE
/** Clear encoded data from header structure. */
su_inline void msg_fragment_clear(msg_common_t *h)
{
  h->h_data = NULL, h->h_len = 0;
}

/** Pointer to header parameters. */
su_inline
msg_param_t **msg_header_params(msg_common_t const *h)
{
  if (h && h->h_class->hc_params) {
    return (msg_param_t **)((char *)h + h->h_class->hc_params);
  }
  return NULL;
}
#else
#define msg_fragment_clear(h) ((h)->h_data = NULL, (h)->h_len = 0)
#define msg_header_params(h) \
 (((h) && ((msg_common_t *)h)->h_class->hc_params) ? \
  (msg_param_t **)((char *)(h) + ((msg_common_t *)h)->h_class->hc_params) : NULL)
#endif

SOFIAPUBFUN char const *msg_header_find_param(msg_common_t const *,
					      char const *name);
SOFIAPUBFUN int msg_header_add_param(su_home_t *, msg_common_t *h,
				     char const *param);
SOFIAPUBFUN int msg_header_replace_param(su_home_t *, msg_common_t *h,
					 char const *param);
SOFIAPUBFUN int msg_header_remove_param(msg_common_t *h, char const *name);

SOFIAPUBFUN char const *msg_header_find_item(msg_common_t const *h,
					     char const *item);

SOFIAPUBFUN int msg_header_replace_item(su_home_t *, msg_common_t *h,
					char const *item);
SOFIAPUBFUN int msg_header_remove_item(msg_common_t *h, char const *name);

/** Append a list of constant items to a list. */
SOFIAPUBFUN int msg_list_append_items(su_home_t *home,
				      msg_list_t *k,
				      msg_param_t const items[]);

/** Replace a list of constant items on a list */
SOFIAPUBFUN int msg_list_replace_items(su_home_t *home,
				       msg_list_t *k,
				       msg_param_t const items[]);

SOFIAPUBFUN int msg_header_join_items(su_home_t *home,
				      msg_common_t *dst,
				      msg_common_t const *src,
				      int duplicate);

SOFIAPUBFUN issize_t msg_random_token(char token[], isize_t tlen,
				      void const *d, isize_t dlen);

SOFIAPUBFUN msg_param_t msg_params_find(msg_param_t const pp[],
					char const *name);
SOFIAPUBFUN msg_param_t *msg_params_find_slot(msg_param_t [],
					      char const *name);
SOFIAPUBFUN int msg_params_add(su_home_t *sh,
			       msg_param_t **pp,
			       char const *param);
SOFIAPUBFUN int msg_params_cmp(char const * const a[],
			       char const * const b[]);
SOFIAPUBFUN int msg_params_replace(su_home_t *,
				   char const * **inout_paramlist,
				   char const *);
SOFIAPUBFUN int msg_params_remove(char const **paramlist,
				  char const *name);
SOFIAPUBFUN size_t msg_params_length(char const * const * params);

/** Unquote a string, return a duplicate. */
SOFIAPUBFUN char *msg_unquote_dup(su_home_t *home, char const *q)
     __attribute__((__malloc__));

SOFIAPUBFUN char *msg_unquote(char *dst, char const *s);

/** Calculate a hash over a string. */
SOFIAPUBFUN unsigned long msg_hash_string(char const *id);

/* Align pointer p for multiple of t (which must be a power of 2) */
#define MSG_ALIGN(p, t) (((uintptr_t)(p) + (t) - 1) & (0 - (uintptr_t)(t)))
#define MSG_STRUCT_SIZE_ALIGN(rv) ((rv) = MSG_ALIGN(rv, sizeof(void *)))
#define MSG_STRUCT_ALIGN(p) ((p) = (void*)MSG_ALIGN(p, sizeof(void *)))

enum {
 msg_n_params = 8	/* allocation size of parameter string list */
#define MSG_N_PARAMS msg_n_params
};

/** Initialize a header structure. @HIDE */
#define MSG_HEADER_INIT(h, msg_class, size)			\
  ((void)memset((h), 0, (size)),				\
   (void)(((msg_common_t *)(h))->h_class = (msg_class)),	\
   (h))

/** No header. */
#define MSG_HEADER_NONE ((msg_header_t *)-1)

SOFIA_END_DECLS

#ifndef MSG_PROTOS_H
#include <sofia-sip/msg_protos.h>
#endif

#endif /** !defined(MSG_HEADER_H) */

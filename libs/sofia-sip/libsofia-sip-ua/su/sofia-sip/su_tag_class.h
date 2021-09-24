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

#ifndef SU_TAG_CLASS_H
/** Defined when <sofia-sip/su_tag_class.h> has been included. */
#define SU_TAG_CLASS_H

/**@SU_TAG
 * @file  su_tag_class.h
 * @brief Tag class interface for object-oriented tags
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 00:49:41 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

SOFIA_BEGIN_DECLS

/* Macros for defining tag classes */

#ifndef TAG_NAMESPACE
/** Default namespace for tags */
#define TAG_NAMESPACE ""
#endif

#define TAG_TYPEDEF(t, type) \
  {{ TAG_NAMESPACE, #t, type ## _tag_class, 0 }}

#define INTTAG_TYPEDEF(t)      TAG_TYPEDEF(t, int)
#define UINTTAG_TYPEDEF(t)     TAG_TYPEDEF(t, uint)
#define USIZETAG_TYPEDEF(t)    TAG_TYPEDEF(t, usize)
#define SIZETAG_TYPEDEF(t)     TAG_TYPEDEF(t, size)
#define BOOLTAG_TYPEDEF(t)     TAG_TYPEDEF(t, bool)
#define PTRTAG_TYPEDEF(t)      TAG_TYPEDEF(t, ptr)
#define SOCKETTAG_TYPEDEF(t)   TAG_TYPEDEF(t, socket)
#define CSTRTAG_TYPEDEF(t)     TAG_TYPEDEF(t, cstr)
#define STRTAG_TYPEDEF(t)      TAG_TYPEDEF(t, str)
#define NSTAG_TYPEDEF(t)       TAG_TYPEDEF(t, ns)

struct tag_class_s {
  int             tc_size;	/**< Size of the tag_class_t structure */
  tagi_t const *(*tc_next)(tagi_t const *t);
  size_t        (*tc_len)(tagi_t const *t);
  tagi_t       *(*tc_move)(tagi_t *dst, tagi_t const *src);
  size_t        (*tc_xtra)(tagi_t const *t, size_t offset);
  tagi_t       *(*tc_dup)(tagi_t *dst, tagi_t const *src, void **b);
  tagi_t       *(*tc_free)(tagi_t *t);
  tagi_t const *(*tc_find)(tag_type_t t, tagi_t const lst[]);
  int           (*tc_snprintf)(tagi_t const *t, char b[], size_t size);
  tagi_t       *(*tc_filter)(tagi_t *dst, tagi_t const f[], tagi_t const *src,
			     void **bb);
  int           (*tc_ref_set)(tag_type_t tt, void *ref, tagi_t const value[]);
  int           (*tc_scan)(tag_type_t tt, su_home_t *home,
			   char const *str,
			   tag_value_t *return_value);
};

SOFIAPUBVAR tag_class_t end_tag_class[];
SOFIAPUBVAR tag_class_t int_tag_class[];
SOFIAPUBVAR tag_class_t uint_tag_class[];
SOFIAPUBVAR tag_class_t usize_tag_class[];
SOFIAPUBVAR tag_class_t size_tag_class[];
SOFIAPUBVAR tag_class_t bool_tag_class[];
SOFIAPUBVAR tag_class_t ptr_tag_class[];
SOFIAPUBVAR tag_class_t socket_tag_class[];
SOFIAPUBVAR tag_class_t cstr_tag_class[];
SOFIAPUBVAR tag_class_t str_tag_class[];
SOFIAPUBVAR tag_class_t ns_tag_class[];

#define REFTAG_TYPEDEF(tag) \
  {{ TAG_NAMESPACE, #tag "_ref", ref_tag_class, (tag_value_t)tag }}

SOFIAPUBVAR tag_class_t ref_tag_class[];

SOFIAPUBFUN tagi_t *t_filter(tagi_t *, tagi_t const [],
			     tagi_t const *, void **);
SOFIAPUBFUN tagi_t *t_null_filter(tagi_t *dst, tagi_t const filter[],
				  tagi_t const *src, void **bb);
SOFIAPUBFUN tagi_t *t_end_filter(tagi_t *, tagi_t const [],
				 tagi_t const *, void **);

SOFIAPUBFUN int t_ptr_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN int t_ptr_ref_set(tag_type_t tt, void *ref, tagi_t const value[]);
SOFIAPUBFUN int t_ptr_scan(tag_type_t, su_home_t *, char const *,
			   tag_value_t *return_value);

SOFIAPUBFUN int t_bool_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN int t_bool_ref_set(tag_type_t tt, void *ref, tagi_t const value[]);
SOFIAPUBFUN int t_bool_scan(tag_type_t, su_home_t *, char const *,
			    tag_value_t *return_value);

SOFIAPUBFUN int t_int_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN int t_int_ref_set(tag_type_t tt, void *ref, tagi_t const value[]);
SOFIAPUBFUN int t_int_scan(tag_type_t, su_home_t *, char const *,
			   tag_value_t *return_value);

SOFIAPUBFUN int t_uint_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN int t_uint_ref_set(tag_type_t tt, void *ref, tagi_t const value[]);
SOFIAPUBFUN int t_uint_scan(tag_type_t, su_home_t *, char const *,
			    tag_value_t *return_value);

SOFIAPUBFUN int t_socket_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN int t_socket_ref_set(tag_type_t tt, void *ref, tagi_t const value[]);

SOFIAPUBFUN tagi_t *t_str_dup(tagi_t *dst, tagi_t const *src, void **b);
SOFIAPUBFUN size_t t_str_xtra(tagi_t const *t, size_t offset);
SOFIAPUBFUN int t_str_snprintf(tagi_t const *t, char b[], size_t size);
SOFIAPUBFUN int t_str_scan(tag_type_t, su_home_t *, char const *,
			   tag_value_t *return_value);

SOFIA_END_DECLS

#endif /* !defined(SU_TAG_CLASS_H) */

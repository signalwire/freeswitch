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

#ifndef SU_TAG_H
/** Defined when <sofia-sip/su_tag.h> has been included. */
#define SU_TAG_H

/**@SU_TAG
 * @file sofia-sip/su_tag.h  Object-oriented tags and tag list interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Feb 20 19:48:18 2001 ppessi
 */

#ifndef SU_CONFIG_H
#include <sofia-sip/su_config.h>
#endif

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

#include <stdarg.h>
#include <stddef.h> /* ANSI C: size_t */

SOFIA_BEGIN_DECLS

/** Tag item type */
typedef struct tag_type_s const *tag_type_t;
/** Tag item value */
typedef intptr_t                 tag_value_t;

/** Tag list signature */
#define TAG_LIST tag_type_t tag, tag_value_t value, ...

/** Tag item. */
typedef struct {
  tag_type_t   t_tag;		/**< Tag */
  tag_value_t  t_value;		/**< Value */
} tagi_t;

/** Tag type class */
typedef struct tag_class_s const tag_class_t;

/** Tag structure.
 *
 * The tag structure contains the name, namespace and class of the tag. The
 * fourth field, @a tt_magic, is interpreted by the tag class.
 */
struct tag_type_s {
  char const    *tt_ns;		/**< Tag namespace (e.g., "sip" or "nua") */
  char const 	*tt_name;	/**< Tag name (e.g, "min_se")  */
  tag_class_t   *tt_class;	/**< Tag class defines the type of the value */
  tag_value_t    tt_magic;	/**< Class-specific data
				   (e.g., pointer to header class structure) */
};

/** Definition of tag type. */
typedef struct tag_type_s const tag_typedef_t[1];

/** End of tag list */
SOFIAPUBVAR tag_typedef_t tag_null;

/** Ignore tag item. */
SOFIAPUBVAR tag_typedef_t tag_skip;

/** Jump to another tag list */
SOFIAPUBVAR tag_typedef_t tag_next;

/** Any tag accepted when filtering. */
SOFIAPUBVAR tag_typedef_t tag_any;

/** Filter tag using function as argument.
 * @since New in @VERSION_1_12_2.
 */
SOFIAPUBVAR tag_typedef_t tag_filter;

/** Prototype for filtering function used with TAG_FILTER().
 * @since New in @VERSION_1_12_2.
 */
typedef int tag_filter_f(tagi_t const *filter, tagi_t const *dest);

/** @HI Initialize a tag item marking the end of list. Equivalent to TAG_END(). */
#define TAG_NULL()  (tag_type_t)0, (tag_value_t)0

/** @HI Initialize a tag item marking the end of list. Equivalent to TAG_NULL(). */
#define TAG_END()   (tag_type_t)0, (tag_value_t)0

/** @HI Initialize an empty tag item. */
#define TAG_SKIP(x)    tag_skip, (tag_value_t)(x)

/** @HI Initialize a tag item pointing to another tag list at @a next. */
#define TAG_NEXT(next) tag_next, (tag_value_t)(next)

/** @HI Initialize a filter tag item accepting any item. */
#define TAG_ANY()   tag_any,  (tag_value_t)0

/** @HI Initialize a @a item if condition is true;
 * otherwise, initialize an empty tag item. */
#define TAG_IF(condition, item) !(condition) ? tag_skip : item

/** @HI Initialize a filter tag item accepting any item.
 * @since New in @VERSION_1_12_2.
 */
#define TAG_FILTER(function)  tag_filter, tag_filter_v(function)

/** Convert tag item to a string  */
SOFIAPUBFUN int t_snprintf(tagi_t const *t, char b[], size_t size);

/** Convert string to a tag value */
SOFIAPUBFUN int t_scan(tag_type_t tt, su_home_t *home, char const *s,
		       tag_value_t *return_value);

/* Tagarg functions */
SOFIAPUBFUN tagi_t *tl_tlist(su_home_t *, tag_type_t, tag_value_t, ...);
SOFIAPUBFUN size_t tl_tmove(tagi_t *dst, size_t, tag_type_t, tag_value_t, ...);
SOFIAPUBFUN int tl_gets(tagi_t const lst[], tag_type_t, tag_value_t, ...);
SOFIAPUBFUN int tl_tgets(tagi_t lst[], tag_type_t, tag_value_t, ...);
SOFIAPUBFUN tagi_t *tl_tfilter(su_home_t *, tagi_t const lst[],
			       tag_type_t, tag_value_t, ...);
SOFIAPUBFUN int tl_tremove(tagi_t lst[], tag_type_t, tag_value_t, ...);

/* Low-level taglist manipulation functions */
SOFIAPUBFUN size_t tl_len(tagi_t const lst[]);
SOFIAPUBFUN size_t tl_vllen(tag_type_t tag, tag_value_t value, va_list ap);
SOFIAPUBFUN size_t tl_xtra(tagi_t const lst[], size_t offset);
SOFIAPUBFUN tagi_t *tl_next(tagi_t const *lst);
SOFIAPUBFUN tagi_t *tl_move(tagi_t *dst, tagi_t const src[]);
SOFIAPUBFUN tagi_t *tl_dup(tagi_t dst[], tagi_t const lst[], void **bb);
SOFIAPUBFUN tagi_t *tl_adup(su_home_t *, tagi_t const lst[]);
SOFIAPUBFUN void tl_free(tagi_t list[]);

SOFIAPUBFUN tagi_t *tl_find(tagi_t const lst[], tag_type_t tt);
SOFIAPUBFUN tagi_t *tl_find_last(tagi_t const lst[], tag_type_t tt);
SOFIAPUBFUN tagi_t *tl_filter(tagi_t *, tagi_t const filter[],
			      tagi_t const lst[], void **b);
SOFIAPUBFUN tagi_t *tl_afilter(su_home_t *, tagi_t const filter[],
			       tagi_t const lst[]);

SOFIAPUBFUN tagi_t *tl_filtered_tlist(su_home_t *home, tagi_t const filter[],
				      tag_type_t, tag_value_t, ...);

SOFIAPUBFUN size_t  tl_vlen(va_list ap);
SOFIAPUBFUN tagi_t *tl_list(tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN tagi_t *tl_vlist2(tag_type_t tag, tag_value_t value, va_list ap);
SOFIAPUBFUN tagi_t *tl_vlist(va_list ap);
SOFIAPUBFUN tagi_t *tl_llist(tag_type_t tag, tag_value_t value, ...);
SOFIAPUBFUN tagi_t *tl_vllist(tag_type_t tag, tag_value_t value, va_list ap);
SOFIAPUBFUN void    tl_vfree(tagi_t *t);

/** Align to pointer size */
#define SU_ALIGN(x) \
((sizeof(void *) - ((intptr_t)(x) & (sizeof(void *) - 1))) & (sizeof(void *) - 1))

#if SU_INLINE_TAG_CAST
su_inline tag_value_t tag_int_v(int v) { return (tag_value_t)v; }
su_inline tag_value_t tag_int_vr(int *vp) { return (tag_value_t)vp; }
su_inline tag_value_t tag_uint_v(unsigned v) { return (tag_value_t)v; }
su_inline tag_value_t tag_uint_vr(unsigned *vp) { return (tag_value_t)vp; }
su_inline tag_value_t tag_usize_v(usize_t v) { return (tag_value_t)v; }
su_inline tag_value_t tag_usize_vr(usize_t *vp) { return (tag_value_t)vp; }
su_inline tag_value_t tag_size_v(size_t v) { return (tag_value_t)v; }
su_inline tag_value_t tag_size_vr(size_t *vp) { return (tag_value_t)vp; }
su_inline tag_value_t tag_bool_v(int v) { return v != 0; }
su_inline tag_value_t tag_bool_vr(int *vp) { return (tag_value_t)vp; }
su_inline tag_value_t tag_ptr_v(void *v) { return (tag_value_t)v; }
su_inline tag_value_t tag_ptr_vr(void *vp, void *v)
  { (void)v; return(tag_value_t)vp; }
su_inline tag_value_t tag_cptr_v(void const *v) { return (tag_value_t)v; }
su_inline tag_value_t tag_cptr_vr(void *vp, void const *v)
  { (void)v; return(tag_value_t)vp; }
su_inline tag_value_t tag_cstr_v(char const *v) { return (tag_value_t)v; }
su_inline tag_value_t tag_cstr_vr(char const**vp) {return(tag_value_t)vp;}
su_inline tag_value_t tag_str_v(char const *v) { return (tag_value_t)v; }
su_inline tag_value_t tag_str_vr(char const **vp) {return(tag_value_t)vp;}
#if __cplusplus
extern "C++" {
  su_inline tag_value_t tag_ptr_v(void const *v)
  { return (tag_value_t)v; }
  su_inline tag_value_t tag_ptr_vr(void *vp, void const *p)
  { return (tag_value_t)vp; }
  su_inline tag_value_t tag_str_v(char *v) { return (tag_value_t)v; }
  su_inline tag_value_t tag_str_vr(char **vp) {return (tag_value_t)vp;}
}
#endif
su_inline tag_value_t tag_filter_v(tag_filter_f *v) {return(tag_value_t)v;}
#else
#define tag_int_v(v)   (tag_value_t)(v)
#define tag_int_vr(v)  (tag_value_t)(v)
#define tag_uint_v(v)  (tag_value_t)(v)
#define tag_uint_vr(v) (tag_value_t)(v)
#define tag_usize_v(v) (tag_value_t)(v)
#define tag_usize_vr(v) (tag_value_t)(v)
#define tag_size_v(v) (tag_value_t)(v)
#define tag_size_vr(v) (tag_value_t)(v)
#define tag_bool_v(v)  (tag_value_t)(v != 0)
#define tag_bool_vr(v) (tag_value_t)(v)
#define tag_ptr_v(v)   (tag_value_t)(v)
#define tag_ptr_vr(v,x) (tag_value_t)(v)
#define tag_cptr_v(v)   (tag_value_t)(v)
#define tag_cptr_vr(v,x) (tag_value_t)(v)
#define tag_cstr_v(v)  (tag_value_t)(v)
#define tag_cstr_vr(v) (tag_value_t)(v)
#define tag_str_v(v)   (tag_value_t)(v)
#define tag_str_vr(v)  (tag_value_t)(v)
#define tag_filter_v(v) (tag_value_t)(v)
#endif

SOFIA_END_DECLS

#endif /** !defined(SU_TAG_H) */

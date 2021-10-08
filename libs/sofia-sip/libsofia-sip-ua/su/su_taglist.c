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

/**@SU_TAG
 *
 * @CFILE su_taglist.c
 *
 * Implementation of tag items and lists.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Feb 20 20:03:38 2001 ppessi
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>

#if defined(va_copy)
/* Xyzzy */
#elif defined(__va_copy)
#define va_copy(dst, src) __va_copy((dst), (src))
#else
#define va_copy(dst, src) (memcpy(&(dst), &(src), sizeof (va_list)))
#endif

#include <assert.h>

#include <sofia-sip/su_config.h>

#include <sofia-sip/su_tag.h>
#include <sofia-sip/su_tag_class.h>
#include <sofia-sip/su_tag_inline.h>
#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_string.h>

#ifndef HAVE_STRTOULL
#if !((defined(WIN32) || defined(_WIN32)) && (_MSC_VER >= 1800))
unsigned longlong strtoull(const char *, char **, int);
#endif
#endif

/**@defgroup su_tag Tag Item Lists
 *
 * Object-oriented tag routines for Sofia utility library.
 *
 * The <sofia-sip/su_tag.h> defines a interface to object-oriented tag list routines.
 * A tag list is a linear list (array) of tag items, tagi_t structures,
 * terminated by a TAG_END() item. Each tag item has a label, tag, (@c
 * t_tag) and a value (@c t_value). The tag is a pointer (tag_type_t) to a
 * structure defining how the value should be interpreted, in other words,
 * the name and the type of the value. The value or pointer to the actual
 * value is stored as opaque data (tag_value_t). The tag item structure is
 * defined as follows:
 *
 * @code
 * typedef struct {
 *   tag_type_t   t_tag;
 *   tag_value_t  t_value;
 * } tagi_t;
 * @endcode
 *
 * The tag lists are central concept in the Sofia APIs. The tags lists can
 * be used to a list of named arguments to a @ref tagarg "@em tagarg"
 * function, to store variable amount of data in a memory area, and pass
 * data between processes and threads.
 *
 * The tagged argument lists can be used like named arguments in
 * higher-level languages. The argument list consists of tag-value pairs;
 * tags specify the name and type of the value. All the tag items are not
 * necessarily interpreted by the called function, but it can pass the list
 * to other functions. This feature is used also by the Sofia APIs, the
 * lower-layer settings and options are frequently passed through the
 * upper-layer API in the tag lists.
 *
 * The tagged argument lists are constructed using special macros that
 * expand to two function arguments, tag and value. Each tag item macro
 * checks its arguments type so the tagged argument lists are typesafe if
 * the list is correctly constructed.
 *
 * Each function documents the tags it accepts and also the tags it may pass
 * to the lower layers (at least in theory).
 *
 * @par Special Tags
 *
 * There are a new special tags that are used to control and modify the tag
 * list processing itself. These special tags are as follows:
 * - TAG_NULL() or TAG_END() - indicates the end of tag list
 * - TAG_SKIP() - indicates an empty (overwritten) tag item
 * - TAG_NEXT() - contains a pointer to the next tag list.
 *
 * The tag type structures are declared as tag_typedef_t. They can be
 * defined by the macros found in <sofia-sip/su_tag_class.h>. See nta_tag.c or
 * su_tag_test.c for an example.
 *
 */

/**@class tag_class_s sofia-sip/su_tag_class.h <sofia-sip/su_tag_class.h>
 *
 * @brief Virtual function table for @ref su_tag "tags".
 *
 * The struct tag_class_s contains virtual function table for tags,
 * specifying non-default behaviour of different tags. It provides functions
 * for copying, matching, printing and converting the tagged values.
 */

#ifdef longlong
typedef longlong unsigned llu;
#else
typedef long unsigned llu;
#endif

/** Print a tag. */
int t_snprintf(tagi_t const *t, char b[], size_t size)
{
  tag_type_t tt = TAG_TYPE_OF(t);
  int n, m;

  n = snprintf(b, size, "%s::%s: ",
               tt->tt_ns ? tt->tt_ns : "",
	       tt->tt_name ? tt->tt_name : "null");
  if (n < 0)
    return n;

  if ((size_t)n > size)
    size = n;

  if (tt->tt_snprintf)
    m = tt->tt_snprintf(t, b + n, size - n);
  else
    m = snprintf(b + n, size - n, "%llx", (llu)t->t_value);

  if (m < 0)
    return m;

  if (m == 0 && 0 < n && (size_t)n < size)
    b[--n] = '\0';

  return n + m;
}

/** Get next tag item from list.
 */
tagi_t *tl_next(tagi_t const *t)
{
  tag_type_t tt;

  t = t_next(t);

  for (tt = TAG_TYPE_OF(t); t && tt->tt_next; tt = TAG_TYPE_OF(t)) {
    t = tt->tt_next(t);
  }

  return (tagi_t *)t;
}

/**Move a tag list.
 *
 * The function tl_tmove() moves the tag list arguments to @a dst.  The @a
 * dst must have big enough for all arguments.
 *
 * @param dst   pointer to the destination buffer
 * @param size  sizeof @a dst
 * @param t_tag,t_value,... tag list
 *
 * @return
 * The function tl_tmove() returns number of tag list items initialized.
 */
size_t tl_tmove(tagi_t *dst, size_t size,
		tag_type_t t_tag, tag_value_t t_value, ...)
{
  size_t n = 0, N = size / sizeof(tagi_t);
  tagi_t tagi[1];
  va_list ap;

  va_start(ap, t_value);

  tagi->t_tag = t_tag, tagi->t_value = t_value;

  for (;;) {
    assert((size_t)((char *)&dst[n] - (char *)dst) < size);
    if (n < N)
      dst[n] = *tagi;
    n++;
    if (t_end(tagi))
      break;

    tagi->t_tag = va_arg(ap, tag_type_t);
    tagi->t_value = va_arg(ap, tag_value_t);
  }

  va_end(ap);

  return n;
}

/**Move a tag list.
 *
 * The function tl_move() copies the tag list @a src to the buffer @a
 * dst. The size of the @a dst list must be at least @c tl_len(src) bytes.
 *
 * @param dst pointer to the destination buffer
 * @param src tag list to be moved
 *
 * @return
 * The function tl_move() returns a pointer to the @a dst list after last
 * moved element.
 */
tagi_t *tl_move(tagi_t *dst, tagi_t const src[])
{
  do {
    dst = t_move(dst, src);
  }
  while ((src = t_next(src)));

  return dst;
}

/** Calculate effective length of a tag list as bytes. */
size_t tl_len(tagi_t const lst[])
{
  size_t len = 0;

  do {
    len += t_len(lst);
  }
  while ((lst = t_next(lst)));

  return len;
}

/** Calculate the size of extra memory areas associated with tag list. */
size_t tl_xtra(tagi_t const lst[], size_t offset)
{
  size_t xtra = offset;

  for (; lst; lst = t_next(lst))
    xtra += t_xtra(lst, xtra);

  return xtra - offset;
}

/** Duplicate a tag list.
 *
 * Deep copy the tag list @a src to the buffer @a dst. Memory areas
 * associated with @a src are copied to buffer at @a **bb.
 *
 * This is a rather low-level function. See tl_adup() for a more convenient
 * functionality.
 *
 * The size of the @a dst buffer must be at least @c tl_len(src) bytes.  The
 * size of buffer @a **bb must be at least @c tl_dup_xtra(src) bytes.
 *
 * @param[out] dst pointer to the destination buffer
 * @param[in] src tag list to be duplicated
 * @param[in,out] bb  pointer to pointer to buffer
 *
 * @return
 * A pointer to the @a dst list after last
 * duplicated taglist element.
 *
 * The pointer at @a *bb is updated to the byte after last duplicated memory
 * area.
 */
tagi_t *tl_dup(tagi_t dst[], tagi_t const src[], void **bb)
{
  do {
    dst = t_dup(dst, src, bb);
  } while ((src = t_next(src)));

  return dst;
}


/** Free a tag list.
 *
 * The function tl_free() frees resources associated with a tag list.
 * In other words, it calls t_free on each tag item on the list.
 *
 */
void tl_free(tagi_t list[])
{
  while (list)
    list = t_free(list);
}

/** Allocate and duplicate a tag list using memory home. */
tagi_t *tl_adup(su_home_t *home, tagi_t const lst[])
{
  size_t len = tl_len(lst);
  size_t xtra = tl_xtra(lst, 0);
  void *b = su_alloc(home, len + xtra);
  tagi_t *d, *newlst = b;

  void *end = (char *)b + len + xtra;
  tagi_t *tend = (tagi_t*)((char *)b + len);

  b = (char *)b + len;

  d = tl_dup(newlst, lst, &b);

  assert(b == end); assert(tend == d); (void)end; (void)tend;

  return newlst;
}

/** Allocate and duplicate tagged arguments as a tag list using memory home. */
tagi_t *tl_tlist(su_home_t *home, tag_type_t tag, tag_value_t value, ...)
{
  tagi_t *tl;
  ta_list ta;

  ta_start(ta, tag, value);
  tl = tl_adup(home, ta_args(ta));
  ta_end(ta);

  return tl;
}

/** Find first tag item with type @a tt from list. */
tagi_t *tl_find(tagi_t const lst[], tag_type_t tt)
{
  return (tagi_t *)t_find(tt, lst);
}

/** Find last tag item with type @a tt from list. */
tagi_t *tl_find_last(tagi_t const lst[], tag_type_t tt)
{
  tagi_t const *last, *next;

  for (next = last = t_find(tt, lst); next; next = t_find(tt, t_next(last)))
    last = next;

  return (tagi_t *)last;
}

su_inline
int t_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  if (value == NULL)
    return 0;

  if (tt->tt_class->tc_ref_set)
    return tt->tt_class->tc_ref_set(tt, ref, value);

  *(tag_value_t *)ref = value->t_value;

  return 1;
}

static int tl_get(tag_type_t tt, void *p, tagi_t const lst[])
{
  tagi_t const *t, *latest = NULL;

  assert(tt);

  if (tt == NULL || p == NULL)
    return 0;

  if (tt->tt_class == ref_tag_class)
    tt = (tag_type_t)tt->tt_magic;

  for (t = t_find(tt, lst); t; t = t_find(tt, t_next(t)))
    latest = t;

  return t_ref_set(tt, p, latest);
}

/** Find tags from given list. */
int tl_gets(tagi_t const lst[], tag_type_t tag, tag_value_t value, ...)
{
  int n = 0;
  tagi_t *t;
  ta_list ta;

  ta_start(ta, tag, value);

  for (t = ta_args(ta); t; t = (tagi_t *)t_next(t)) {
    tag_type_t tt = t->t_tag;

    if (!tt)
      continue;

    if (tt->tt_class == ref_tag_class) {
      assert(((tag_type_t)tt->tt_magic)->tt_class->tc_ref_set);
      n += tl_get(tt, (void *)t->t_value, lst);
    }
#if !defined(NDEBUG)
    else if (tt->tt_class && tt->tt_class->tc_ref_set) {
      fprintf(stderr, "WARNING: tag %s::%s directly used by tl_gets()\n",
			  tt->tt_ns ? tt->tt_ns : "", tt->tt_name ? tt->tt_name : "");
      assert(tt->tt_class == ref_tag_class);
    }
#endif
  }

  ta_end(ta);

  return n;
}

/** Find tags from given list.
 *
 * Copies values of argument tag list into the reference tags in the tag
 * list @a lst.
 *
 * @sa tl_gets()
 */
int tl_tgets(tagi_t lst[], tag_type_t tag, tag_value_t value, ...)
{
  int n = 0;
  tagi_t *t;

  ta_list ta;
  ta_start(ta, tag, value);

  for (t = lst; t; t = (tagi_t *)t_next(t)) {
    tag_type_t tt = t->t_tag;

    if (!tt)
      continue;

    if (tt->tt_class == ref_tag_class) {
      assert(((tag_type_t)tt->tt_magic)->tt_class->tc_ref_set);
      n += tl_get(tt, (void *)t->t_value, ta_args(ta));
    }
#if !defined(NDEBUG)
    else if (tt->tt_class->tc_ref_set) {
      fprintf(stderr, "WARNING: tag %s::%s used in tl_tgets(lst)\n",
	      tt->tt_ns, tt->tt_name);
      assert(tt->tt_class == ref_tag_class);
    }
#endif
  }

  ta_end(ta);

  return n;
}


/** Filter an element in tag list */
tagi_t *t_filter(tagi_t *dst,
		 tagi_t const filter[],
		 tagi_t const *src,
		 void **bb)
{
  tag_type_t tt = TAG_TYPE_OF(src);
  tagi_t const *f;

  if (dst) {
    for (f = filter; f; f = t_next(f)) {
      if (TAG_TYPE_OF(f)->tt_filter)
	dst = TAG_TYPE_OF(f)->tt_filter(dst, f, src, bb);
      else if (f->t_tag == tt)
	dst = t_dup(dst, src, bb);
    }
  }
  else {
    size_t d = 0;

    for (f = filter; f; f = t_next(f)) {
      if (TAG_TYPE_OF(f)->tt_filter)
	d += (size_t)TAG_TYPE_OF(f)->tt_filter(NULL, f, src, bb);
      else if (tt == f->t_tag) {
	d += t_len(src);
	*bb = (char *)*bb + t_xtra(src, (size_t)*bb);
      }
    }

    dst = (tagi_t *)d;
  }

  return dst;
}

/** Make filtered copy of a tag list @a src with @a filter to @a dst.
 *
 * Each tag in @a src is checked against tags in list @a filter. If the tag
 * is in the @a filter list, or there is a special filter tag in the list
 * which matches with the tag in @a src, the tag is duplicated to @a dst using
 * memory buffer in @a b.
 *
 * When @a dst is NULL, this function calculates the size of the filtered list.
 *
 * @sa tl_afilter(), tl_tfilter(), tl_filtered_tlist(),
 * TAG_FILTER(), TAG_ANY(), #ns_tag_class
 */
tagi_t *tl_filter(tagi_t dst[],
		  tagi_t const filter[],
		  tagi_t const src[],
		  void **b)
{
  tagi_t const *s;
  tagi_t *d;

  if (dst) {
    for (s = src, d = dst; s; s = t_next(s))
      d = t_filter(d, filter, s, b);
  }
  else {
    size_t rv = 0;

    for (s = src, d = dst; s; s = t_next(s)) {
      d = t_filter(NULL, filter, s, b);
      rv += (char *)d - (char *)NULL;
    }

    d = (tagi_t *)rv;
  }

  return d;
}



/**Filter a tag list.
 *
 * The function tl_afilter() will build a tag list containing tags specified
 * in @a filter and extracted from @a src.  It will allocate the memory used by
 * tag list via the specified memory @a home, which may be also @c NULL.
 *
 * @sa tl_afilter(), tl_tfilter(), tl_filtered_tlist(),
 * TAG_FILTER(), TAG_ANY(), #ns_tag_class
 */
tagi_t *tl_afilter(su_home_t *home, tagi_t const filter[], tagi_t const src[])
{
  tagi_t *dst, *d, *t_end = NULL;
  void *b, *end = NULL;
  size_t len;

  /* Calculate length of the result */
  t_end = tl_filter(NULL, filter, src, &end);
  len = ((char *)t_end - (char *)NULL) + ((char *)end - (char*)NULL);

  if (len == 0)
    return NULL;

  /* Allocate the result */
  if (!(dst = su_alloc(home, len)))
    return NULL;

  /* Build the result */
  b = (dst + (t_end - (tagi_t *)NULL));
  d = tl_filter(dst, filter, src, (void **)&b);

  /* Ensure that everything is consistent */
  assert(d == dst + (t_end - (tagi_t *)NULL));
  assert(b == (char *)dst + len);

  return dst;
}

/** Filter tag list @a src with given tags.
 *
 * @sa tl_afilter(), tl_filtered_tlist(), TAG_FILTER(), TAG_ANY(), #ns_tag_class
 */
tagi_t *tl_tfilter(su_home_t *home, tagi_t const src[],
		   tag_type_t tag, tag_value_t value, ...)
{
  tagi_t *tl;
  ta_list ta;
  ta_start(ta, tag, value);
  tl = tl_afilter(home, ta_args(ta), src);
  ta_end(ta);
  return tl;
}

/** Create a filtered tag list.
 *
 * @sa tl_afilter(), tl_tfilter(), TAG_FILTER(), TAG_ANY(), #ns_tag_class
 */
tagi_t *tl_filtered_tlist(su_home_t *home, tagi_t const filter[],
			  tag_type_t tag, tag_value_t value, ...)
{
  tagi_t *tl;
  ta_list ta;

  ta_start(ta, tag, value);
  tl = tl_afilter(home, filter, ta_args(ta));
  ta_end(ta);

  return tl;
}


/** Remove listed tags from the list @a lst. */
int tl_tremove(tagi_t lst[], tag_type_t tag, tag_value_t value, ...)
{
  tagi_t *l, *l_next;
  int retval = 0;
  ta_list ta;

  ta_start(ta, tag, value);

  for (l = lst; l; l = l_next) {
    if ((l_next = (tagi_t *)t_next(l))) {
      if (tl_find(ta_args(ta), l->t_tag))
	l->t_tag = tag_skip;
      else
	retval++;
    }
  }

  ta_end(ta);

  return retval;
}

/** Calculate length of a tag list with a @c va_list. */
size_t tl_vlen(va_list ap)
{
  size_t len = 0;
  tagi_t tagi[2] = {{ NULL }};

  do {
    tagi->t_tag = va_arg(ap, tag_type_t );
    tagi->t_value = va_arg(ap, tag_value_t);
    len += sizeof(tagi_t);
  } while (!t_end(tagi));

  return len;
}

/** Convert va_list to tag list */
tagi_t *tl_vlist(va_list ap)
{
  tagi_t *t, *rv;
  va_list aq;

  va_copy(aq, ap);
  rv = malloc(tl_vlen(aq));
  va_end(aq);

  for (t = rv; t; t++) {
    t->t_tag = va_arg(ap, tag_type_t);
    t->t_value = va_arg(ap, tag_value_t);

    if (t_end(t))
      break;
  }

  return rv;
}

tagi_t *tl_vlist2(tag_type_t tag, tag_value_t value, va_list ap)
{
  tagi_t *t, *rv;
  tagi_t tagi[1];
  size_t size;

  tagi->t_tag = tag, tagi->t_value = value;

  if (!t_end(tagi)) {
    va_list aq;
    va_copy(aq, ap);
    size = sizeof(tagi) + tl_vlen(aq);
    va_end(aq);
  }
  else
    size = sizeof(tagi);

  t = rv = malloc(size);

  for (;t;) {
    *t++ = *tagi;

    if (t_end(tagi))
      break;

    tagi->t_tag = va_arg(ap, tag_type_t);
    tagi->t_value = va_arg(ap, tag_value_t);
  }

  assert((char *)rv + size == (char *)t);

  return rv;
}

/** Make a tag list until TAG_NEXT() or TAG_END() */
tagi_t *tl_list(tag_type_t tag, tag_value_t value, ...)
{
  va_list ap;
  tagi_t *t;

  va_start(ap, value);
  t = tl_vlist2(tag, value, ap);
  va_end(ap);

  return t;
}

/** Calculate length of a linear tag list. */
size_t tl_vllen(tag_type_t tag, tag_value_t value, va_list ap)
{
  size_t len = sizeof(tagi_t);
  tagi_t const *next;
  tagi_t tagi[3];

  tagi[0].t_tag = tag;
  tagi[0].t_value = value;
  tagi[1].t_tag = tag_any;
  tagi[1].t_value = 0;

  for (;;) {
    next = tl_next(tagi);
    if (next != tagi + 1)
      break;

    if (tagi->t_tag != tag_skip)
      len += sizeof(tagi_t);
    tagi->t_tag = va_arg(ap, tag_type_t);
    tagi->t_value = va_arg(ap, tag_value_t);
  }

  for (; next; next = tl_next(next))
    len += sizeof(tagi_t);

  return len;
}

/** Make a linear tag list. */
tagi_t *tl_vllist(tag_type_t tag, tag_value_t value, va_list ap)
{
  va_list aq;
  tagi_t *t, *rv;
  tagi_t const *next;
  tagi_t tagi[2];

  size_t size;

  va_copy(aq, ap);
  size = tl_vllen(tag, value, aq);
  va_end(aq);

  t = rv = malloc(size);
  if (rv == NULL)
    return rv;

  tagi[0].t_tag = tag;
  tagi[0].t_value = value;
  tagi[1].t_tag = tag_any;
  tagi[1].t_value = 0;

  for (;;) {
    next = tl_next(tagi);
    if (next != tagi + 1)
      break;

    if (tagi->t_tag != tag_skip)
      *t++ = *tagi;

    tagi->t_tag = va_arg(ap, tag_type_t);
    tagi->t_value = va_arg(ap, tag_value_t);
  }

  for (; next; next = tl_next(next))
    *t++ = *next;

  t->t_tag = NULL; t->t_value = 0; t++;

  assert((char *)rv + size == (char *)t);

  return rv;
}

/** Make a linear tag list until TAG_END().
 *
 */
tagi_t *tl_llist(tag_type_t tag, tag_value_t value, ...)
{
  va_list ap;
  tagi_t *t;

  va_start(ap, value);
  t = tl_vllist(tag, value, ap);
  va_end(ap);

  return t;
}

/** Free a tag list allocated by tl_list(), tl_llist() or tl_vlist(). */
void tl_vfree(tagi_t *t)
{
  if (t)
    free(t);
}

/** Convert a string to the a value of a tag. */
int t_scan(tag_type_t tt, su_home_t *home, char const *s,
	   tag_value_t *return_value)
{
  if (tt == NULL || s == NULL || return_value == NULL)
    return -1;

  if (tt->tt_class->tc_scan) {
    return tt->tt_class->tc_scan(tt, home, s, return_value);
  }
  else {			/* Not implemented */
    *return_value = (tag_value_t)0;
    return -2;
  }
}


/* ====================================================================== */
/* null tag */

static
tagi_t const *t_null_next(tagi_t const *t)
{
  return NULL;
}

static
tagi_t *t_null_move(tagi_t *dst, tagi_t const *src)
{
  memset(dst, 0, sizeof(*dst));
  return dst + 1;
}

static
tagi_t *t_null_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  memset(dst, 0, sizeof(*dst));
  return dst + 1;
}

static
tagi_t const * t_null_find(tag_type_t tt, tagi_t const lst[])
{
  return NULL;
}

tagi_t *t_null_filter(tagi_t *dst,
		      tagi_t const filter[],
		      tagi_t const *src,
		      void **bb)
{
  if (TAG_TYPE_OF(src) == tag_null) {
    if (dst) {
      dst->t_tag = NULL;
      dst->t_value = 0;
    }
    return dst + 1;
  }
  return dst;
}

tag_class_t null_tag_class[1] =
  {{
    sizeof(null_tag_class),
    /* tc_next */     t_null_next,
    /* tc_len */      NULL,
    /* tc_move */     t_null_move,
    /* tc_xtra */     NULL,
    /* tc_dup */      t_null_dup,
    /* tc_free */     NULL,
    /* tc_find */     t_null_find,
    /* tc_snprintf */ NULL,
    /* tc_filter */   t_null_filter,
    /* tc_ref_set */  NULL,
    /* tc_scan */     NULL,
  }};

tag_typedef_t tag_null = TAG_TYPEDEF(tag_null, null);

/* ====================================================================== */
/* end tag */

tagi_t *t_end_filter(tagi_t *dst,
		     tagi_t const filter[],
		     tagi_t const *src,
		     void **bb)
{
  return dst;
}

tag_class_t end_tag_class[1] =
  {{
    sizeof(end_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ NULL,
    /* tc_filter */   t_end_filter,
    /* tc_ref_set */  NULL,
    /* tc_scan */     NULL,
  }};

/* ====================================================================== */
/* skip tag - placeholder in tag list */

static
tagi_t const *t_skip_next(tagi_t const *t)
{
  return t + 1;
}

static
tagi_t *t_skip_move(tagi_t *dst, tagi_t const *src)
{
  return dst;
}

static
size_t t_skip_len(tagi_t const *t)
{
  return 0;
}

static
tagi_t *t_skip_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  return dst;
}

static
tagi_t *t_skip_filter(tagi_t *dst,
		    tagi_t const filter[],
		    tagi_t const *src,
		    void **bb)
{
  return dst;
}

tag_class_t skip_tag_class[1] =
  {{
    sizeof(skip_tag_class),
    /* tc_next */     t_skip_next,
    /* tc_len */      t_skip_len,
    /* tc_move */     t_skip_move,
    /* tc_xtra */     NULL,
    /* tc_dup */      t_skip_dup,
    /* tc_free */     NULL,
    /* tc_find */     t_null_find,
    /* tc_snprintf */ NULL,
    /* tc_filter */   t_skip_filter,
    /* tc_ref_set */  NULL,
    /* tc_scan */     NULL,
  }};

tag_typedef_t tag_skip = TAG_TYPEDEF(tag_skip, skip);

/* ====================================================================== */
/* next tag - jump to next tag list */

static
tagi_t const *t_next_next(tagi_t const *t)
{
  return (tagi_t *)(t->t_value);
}

static
tagi_t *t_next_move(tagi_t *dst, tagi_t const *src)
{
  if (!src->t_value)
    return t_null_move(dst, src);
  return dst;
}

static
size_t t_next_len(tagi_t const *t)
{
  if (!t->t_value)
    return sizeof(*t);
  return 0;
}

static
tagi_t *t_next_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  if (!src->t_value)
    return t_null_dup(dst, src, bb);
  return dst;
}

static
tagi_t *t_next_filter(tagi_t *dst,
		    tagi_t const filter[],
		    tagi_t const *src,
		    void **bb)
{
  return dst;
}

tag_class_t next_tag_class[1] =
  {{
    sizeof(next_tag_class),
    /* tc_next */     t_next_next,
    /* tc_len */      t_next_len,
    /* tc_move */     t_next_move,
    /* tc_xtra */     NULL,
    /* tc_dup */      t_next_dup,
    /* tc_free */     NULL,
    /* tc_find */     t_null_find,
    /* tc_snprintf */ NULL,
    /* tc_filter */   t_next_filter,
    /* tc_ref_set */  NULL,
    /* tc_scan */     NULL,
  }};

tag_typedef_t tag_next = TAG_TYPEDEF(tag_next, next);

/* ====================================================================== */
/* filter tag  - use function to filter tag */

static
tagi_t *t_filter_with(tagi_t *dst,
		      tagi_t const *t,
		      tagi_t const *src,
		      void **bb)
{
  tag_filter_f *function;

  if (!src || !t)
    return dst;

  function = (tag_filter_f *)t->t_value;

  if (!function || !function(t, src))
    return dst;

  if (dst) {
    return t_dup(dst, src, bb);
  }
  else {
    dst = (tagi_t *)((char *)dst + t_len(src));
    *bb = (char *)*bb + t_xtra(src, (size_t)*bb);
    return dst;
  }
}

tag_class_t filter_tag_class[1] =
  {{
    sizeof(filter_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ NULL,
    /* tc_filter */   t_filter_with,
    /* tc_ref_set */  NULL,
    /* tc_scan */     NULL,
  }};

/** Filter tag - apply function in order to filter tag. */
tag_typedef_t tag_filter = TAG_TYPEDEF(tag_filter, filter);

/* ====================================================================== */
/* any tag - match to any tag when filtering */

static
tagi_t *t_any_filter(tagi_t *dst,
		     tagi_t const filter[],
		     tagi_t const *src,
		     void **bb)
{
  if (!src)
    return dst;
  else if (dst) {
    return t_dup(dst, src, bb);
  }
  else {
    dst = (tagi_t *)((char *)dst + t_len(src));
    *bb = (char *)*bb + t_xtra(src, (size_t)*bb);
    return dst;
  }
}

tag_class_t any_tag_class[1] =
  {{
    sizeof(any_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ NULL,
    /* tc_filter */   t_any_filter,
    /* tc_ref_set */  NULL,
    /* tc_scan */     NULL,
  }};

/** Any tag - match any tag when filtering. */
tag_typedef_t tag_any = TAG_TYPEDEF(tag_any, any);

/* ====================================================================== */
/* ns tag - match to any tag with same namespace when filtering */

static
tagi_t *t_ns_filter(tagi_t *dst,
		    tagi_t const filter[],
		    tagi_t const *src,
		    void **bb)
{
  char const *match, *ns;

  if (!src)
    return dst;

  assert(filter);

  match = TAG_TYPE_OF(filter)->tt_ns;
  ns = TAG_TYPE_OF(src)->tt_ns;

  if (match == NULL)
    /* everything matches with this */;
  else if (match == ns)
    /* namespaces matche */;
  else if (ns == NULL)
    /* no match */
    return dst;
  else if (strcmp(match, ns))
    /* no match */
    return dst;

  if (dst) {
    return t_dup(dst, src, bb);
  }
  else {
    dst = (tagi_t *)((char *)dst + t_len(src));
    *bb = (char *)*bb + t_xtra(src, (size_t)*bb);
    return dst;
  }
}

/** Namespace filtering class */
tag_class_t ns_tag_class[1] =
  {{
    sizeof(ns_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ NULL,
    /* tc_filter */   t_ns_filter,
    /* tc_ref_set */  NULL,
    /* tc_scan */     NULL,
  }};

/* ====================================================================== */
/* int tag - pass integer value */

int t_int_snprintf(tagi_t const *t, char b[], size_t size)
{
  return snprintf(b, size, "%i", (int)t->t_value);
}

int t_int_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  *(int *)ref = (int)value->t_value;

  return 1;
}

int t_int_scan(tag_type_t tt, su_home_t *home,
	       char const *s,
	       tag_value_t *return_value)
{
  int value;
  char *rest;

  value = strtol(s, &rest, 0);

  if (s != rest) {
    *return_value = (tag_value_t)value;
    return 1;
  }
  else {
    *return_value = (tag_value_t)0;
    return -1;
  }
}

tag_class_t int_tag_class[1] =
  {{
    sizeof(int_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_int_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_int_ref_set,
    /* tc_scan */     t_int_scan,
  }};

/* ====================================================================== */
/* uint tag - pass unsigned integer value */

int t_uint_snprintf(tagi_t const *t, char b[], size_t size)
{
  return snprintf(b, size, "%u", (unsigned)t->t_value);
}

int t_uint_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  *(unsigned *)ref = (unsigned)value->t_value;

  return 1;
}

int t_uint_scan(tag_type_t tt, su_home_t *home,
	       char const *s,
	       tag_value_t *return_value)
{
  unsigned value;
  char *rest;

  value = strtoul(s, &rest, 0);

  if (s != rest) {
    *return_value = (tag_value_t)value;
    return 1;
  }
  else {
    *return_value = (tag_value_t)0;
    return -1;
  }
}

tag_class_t uint_tag_class[1] =
  {{
    sizeof(int_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_uint_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_uint_ref_set,
    /* tc_scan */     t_uint_scan,
  }};


/* ====================================================================== */
/* size tag - pass size_t value @NEW_1_12_5 */

static
int t_size_snprintf(tagi_t const *t, char b[], size_t size)
{
  return snprintf(b, size, MOD_ZU, (size_t)t->t_value);
}

static
int t_size_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  *(size_t *)ref = (size_t)value->t_value;

  return 1;
}

static
int t_size_scan(tag_type_t tt, su_home_t *home,
		 char const *s,
		 tag_value_t *return_value)
{
  unsigned longlong value;
  char *rest;

  value = strtoull(s, &rest, 0);

  if (s != rest && value <= SIZE_MAX) {
    *return_value = (tag_value_t)value;
    return 1;
  }
  else {
    *return_value = (tag_value_t)0;
    return -1;
  }
}

/** Tag class for tags with size_t value. @NEW_1_12_5. */
tag_class_t size_tag_class[1] =
  {{
    sizeof(int_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_size_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_size_ref_set,
    /* tc_scan */     t_size_scan,
  }};

/* ====================================================================== */
/* usize tag - pass usize_t value */

static
int t_usize_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  *(usize_t *)ref = (usize_t)value->t_value;

  return 1;
}

static
int t_usize_scan(tag_type_t tt, su_home_t *home,
		 char const *s,
		 tag_value_t *return_value)
{
  unsigned longlong value;
  char *rest;

  value = strtoull(s, &rest, 0);

  if (s != rest && value <= USIZE_MAX) {
    *return_value = (tag_value_t)value;
    return 1;
  }
  else {
    *return_value = (tag_value_t)0;
    return -1;
  }
}

/** Tag class for tags with usize_t value. @NEW_1_12_5. */
tag_class_t usize_tag_class[1] =
  {{
    sizeof(int_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_size_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_usize_ref_set,
    /* tc_scan */     t_usize_scan,
  }};


/* ====================================================================== */
/* bool tag - pass boolean value */

int t_bool_snprintf(tagi_t const *t, char b[], size_t size)
{
  return snprintf(b, size, "%s", t->t_value ? "true" : "false");
}

int t_bool_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  *(int *)ref = (value->t_value != 0);

  return 1;
}

int t_bool_scan(tag_type_t tt, su_home_t *home,
	       char const *s,
	       tag_value_t *return_value)
{
  int retval;
  int value = 0;

  if (su_casenmatch(s, "true", 4)
      && strlen(s + 4) == strspn(s + 4, " \t\r\n")) {
    value = 1, retval = 1;
  } else if (su_casenmatch(s, "false", 5)
	     && strlen(s + 5) == strspn(s + 5, " \t\r\n")) {
    value = 0, retval = 1;
  } else {
    retval = t_int_scan(tt, home, s, return_value);
    value = *return_value != 0;
  }

  if (retval == 1)
    *return_value = (tag_value_t)value;
  else
    *return_value = (tag_value_t)0;

  return retval;
}

tag_class_t bool_tag_class[1] =
  {{
    sizeof(bool_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_bool_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_bool_ref_set,
    /* tc_scan */     t_bool_scan,
  }};

/* ====================================================================== */
/* ptr tag - pass pointer value */

int t_ptr_snprintf(tagi_t const *t, char b[], size_t size)
{
  return snprintf(b, size, "%p", (void *)t->t_value);
}

int t_ptr_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  *(void **)ref = (void *)value->t_value;

  return 1;
}

/* This is not usually very safe, so it is not used */
int t_ptr_scan(tag_type_t tt, su_home_t *home,
	       char const *s,
	       tag_value_t *return_value)
{
  int retval;
  void *ptr;

  retval = sscanf(s, "%p", &ptr);

  if (retval == 1)
    *return_value = (tag_value_t)ptr;
  else
    *return_value = (tag_value_t)NULL;

  return retval;
}

tag_class_t ptr_tag_class[1] =
  {{
    sizeof(ptr_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_ptr_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     NULL,
  }};

/* ====================================================================== */
/* socket tag - pass socket */

#include <sofia-sip/su.h>

int t_socket_snprintf(tagi_t const *t, char b[], size_t size)
{
  /* socket can be int or DWORD (or QWORD on win64?) */
  return snprintf(b, size, LLI, (longlong)t->t_value);
}

int t_socket_ref_set(tag_type_t tt, void *ref, tagi_t const value[])
{
  *(su_socket_t *)ref = (su_socket_t)value->t_value;

  return 1;
}

tag_class_t socket_tag_class[1] =
  {{
    sizeof(socket_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_socket_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_socket_ref_set,
    /* tc_scan */     NULL,
  }};

/* ====================================================================== */
/* str tag - pass string value */

int t_str_snprintf(tagi_t const *t, char b[], size_t size)
{
  if (t->t_value)
    return snprintf(b, size, "\"%s\"", (char const *)t->t_value);
  else
    return snprintf(b, size, "<null>");
}

int t_str_scan(tag_type_t tt, su_home_t *home,
	       char const *s,
	       tag_value_t *return_value)
{
  int retval;

  s = su_strdup(home, s);

  if (s)
    *return_value = (tag_value_t)s, retval = 1;
  else
    *return_value = (tag_value_t)NULL, retval = -1;

  return retval;
}

tagi_t *t_str_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  dst->t_tag = src->t_tag;
  if (src->t_value) {
    char const *s = (char const *)src->t_value;
    size_t len = strlen(s) + 1;
    dst->t_value = (tag_value_t)strcpy(*bb, s);
    *bb = (char *)*bb + len;
  }
  else
    dst->t_value = (tag_value_t)0;

  return dst + 1;
}

size_t t_str_xtra(tagi_t const *t, size_t offset)
{
  return t->t_value ? strlen((char *)t->t_value) + 1 : 0;
}

tag_class_t str_tag_class[1] =
  {{
    sizeof(str_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     t_str_xtra,
    /* tc_dup */      t_str_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_str_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     t_str_scan,
  }};

/* ====================================================================== */
/* cstr tag - pass constant string value (no need to dup) */

/** Tag class for constant strings */
tag_class_t cstr_tag_class[1] =
  {{
    sizeof(cstr_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_str_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     t_str_scan,
  }};

/* ====================================================================== */
/* ref tag - pass reference */

tag_class_t ref_tag_class[1] =
  {{
    sizeof(ref_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     NULL,
    /* tc_dup */      NULL,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ t_ptr_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     NULL,
  }};



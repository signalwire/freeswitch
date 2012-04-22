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

#ifndef SU_TAG_INLINE_H
/** Defined when <sofia-sip/su_tag_inline.h> has been included */
#define SU_TAG_INLINE_H
/**@SU_TAG
 * @file sofia-sip/su_tag_inline.h
 * Inline functions for object tags and tag lists.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Feb 20 19:48:18 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif
#ifndef SU_TAG_CLASS_H
#include <sofia-sip/su_tag_class.h>
#endif

SOFIA_BEGIN_DECLS

#define tt_next     tt_class->tc_next
#define tt_len      tt_class->tc_len
#define tt_move     tt_class->tc_move
#define tt_xtra     tt_class->tc_xtra
#define tt_dup      tt_class->tc_dup
#define tt_free     tt_class->tc_free
#define tt_find     tt_class->tc_find
#define tt_snprintf tt_class->tc_snprintf
#define tt_filter   tt_class->tc_filter

#define TAG_TYPE_OF(t) ((t) && (t)->t_tag ? (t)->t_tag : tag_null)

/** Check if the tag item is last in current list */
su_inline int t_end(tagi_t const *t)
{
  tag_type_t tt = TAG_TYPE_OF(t);

  /* XXX - virtualize this */

  return tt == tag_null || tt == tag_next;
}

su_inline tagi_t const *t_next(tagi_t const *t)
{
  tag_type_t tt = TAG_TYPE_OF(t);

  if (tt->tt_next)
    return tt->tt_next(t);
  else
    return t + 1;
}

su_inline tagi_t *t_move(tagi_t *dst, tagi_t const *src)
{
  tag_type_t tt = TAG_TYPE_OF(src);

  if (tt->tt_move)
    return tt->tt_move(dst, src);

  *dst = *src;
  return dst + 1;
}

su_inline size_t t_xtra(tagi_t const *t, size_t offset)
{
  tag_type_t tt = TAG_TYPE_OF(t);

  if (tt->tt_xtra)
    return tt->tt_xtra(t, offset);

  return 0;
}

su_inline tagi_t *t_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  tag_type_t tt = TAG_TYPE_OF(src);

  if (tt->tt_dup)
    return tt->tt_dup(dst, src, bb);

  *dst = *src;
  return dst + 1;
}

su_inline tagi_t const *t_find(tag_type_t tt, tagi_t const *lst)
{
  if (!tt)
    return NULL;

  if (tt->tt_find)
    return tt->tt_find(tt, lst);

  for (; lst; lst = t_next(lst)) {
    if (tt == lst->t_tag)
      return lst;
  }

  return NULL;
}

su_inline tagi_t *t_free(tagi_t *t)
{
  tag_type_t tt = TAG_TYPE_OF(t);

  if (tt->tt_free)
    return tt->tt_free(t);
  else if (tt->tt_next)
    return (tagi_t *)tt->tt_next(t);
  else
    return t + 1;
}

su_inline size_t t_len(tagi_t const *t)
{
  tag_type_t tt = TAG_TYPE_OF(t);

  if (tt->tt_len)
    return tt->tt_len(t);

  return sizeof(*t);
}

SOFIA_END_DECLS

#endif /* !defined(SU_TAG_INLINE_H) */

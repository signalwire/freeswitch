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

#ifndef SU_TAGARG_H
/** Defined when <sofia-sip/su_tagarg.h> has been included. */
#define SU_TAGARG_H

/**@SU_TAG
 * @file sofia-sip/su_tagarg.h  Tagged argument lists
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Feb 20 19:48:18 2001 ppessi
 */

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

SOFIA_BEGIN_DECLS

/**@page tagarg Tagarg Functions
 *
 * A @em tagarg function may be called with a varying number of tagged
 * arguments.  The include file <sofia-sip/su_tagarg.h> declares a type ta_list and
 * defines four macros (ta_start(), ta_args(), ta_tags() and ta_end()) for
 * accessing the argument list.
 *
 * An example of prototype of a a @em tagarg function is as follows:
 * @code
 * int tag_print(FILE *f, tag_type_t tag, tag_value_t value, ...);
 * @endcode
 *
 * Such a function could be called as follows:
 * @code
 *   tag_print(stdout,
 *             TAG_STRING("a is"), TAG_INT(a),
 *             TAG_STRING("b is"), URLTAG_URL(b),
 *             TAG_IF(c, TAG_STRING("and c is true")),
 *             TAG_END());
 * @endcode
 *
 * @note
 * The tagged argument list @b must be terminated by a TAG_END(),
 * TAG_NULL() or TAG_NEXT().
 */

/**Structure for accessing tagged argument lists.
 *
 * The function called with tagged arguments must declare an object of type
 * ta_list which is used by the macros ta_start(), ta_args(), ta_tags(), and
 * ta_end().
 *
 * If a tagged list is not finished with TAG_END(), TAG_NULL(), or
 * TAG_NEXT() items, random errors may occur.
 *
 * @hideinitializer
 */
typedef struct {
  tagi_t  tl[2];
  va_list ap;
} ta_list;

#if defined(va_copy)
#define su_va_copy(dst, src) va_copy((dst), (src))
#elif defined(__va_copy)
#define su_va_copy(dst, src) __va_copy((dst), (src))
#else
#define su_va_copy(dst, src) (memcpy(&(dst), &(src), sizeof (va_list)))
#endif

/**Macro initializing a ta_list object.
 *
 * The ta_start() macro initializes @a ta for subsequent use by ta_args(),
 * ta_tags() and ta_end(), and must be called first.
 *
 * The parameters @a t and @a v are the names of the @c tag and @c value in
 * the first tag list item before the variable argument list (...).
 *
 * The ta_start() macro returns no value.
 *
 * @hideinitializer
 */
#if SU_HAVE_TAGSTACK
/* All arguments are saved into stack (left-to-right) */
#define ta_start(ta, t, v)						\
   do {									\
    tag_type_t ta_start__tag = (t); tag_value_t ta_start__value = (v);	\
    va_start((ta).ap, (v));						\
    while ((ta_start__tag) == tag_next && (ta_start__value) != 0) {	\
      ta_start__tag = ((tagi_t *)ta_start__value)->t_tag;		\
      if (ta_start__tag == tag_null || ta_start__tag == NULL)		\
	break;								\
      if (ta_start__tag == tag_next) {					\
	ta_start__value = ((tagi_t *)ta_start__value)->t_value; }	\
      else {								\
	ta_start__tag = tag_next;					\
	break;								\
      }									\
    }									\
    (ta).tl->t_tag = ta_start__tag; (ta).tl->t_value = ta_start__value;	\
    if (ta_start__tag != NULL &&					\
	ta_start__tag != tag_null &&					\
	ta_start__tag != tag_next) {					\
      (ta).tl[1].t_tag = tag_next;					\
      (ta).tl[1].t_value = (tag_value_t)(&(v) + 1);			\
    } else {								\
      (ta).tl[1].t_tag = 0; (ta).tl[1].t_value = (tag_value_t)0;	\
    }									\
  } while(0)
#else
/* Tagged arguments are in registers - copy all of them. */
#define ta_start(ta, t, v)						\
   do {									\
    tag_type_t ta_start__tag = (t); tag_value_t ta_start__value = (v);	\
    va_start((ta).ap, (v));						\
    while ((ta_start__tag) == tag_next && (ta_start__value) != 0) {	\
      ta_start__tag = ((tagi_t *)ta_start__value)->t_tag;		\
      if (ta_start__tag == tag_null || ta_start__tag == NULL)		\
	break;								\
      if (ta_start__tag == tag_next) {					\
	ta_start__value = ((tagi_t *)ta_start__value)->t_value;		\
      } else {								\
	ta_start__tag = tag_next;					\
	break;								\
      }									\
    }									\
    (ta).tl->t_tag = ta_start__tag; (ta).tl->t_value = ta_start__value;	\
    if (ta_start__tag != NULL &&					\
	ta_start__tag != tag_null &&					\
	ta_start__tag != tag_next) {					\
      va_list ta_start__ap;						\
      su_va_copy(ta_start__ap, (ta).ap);				\
      (ta).tl[1].t_tag = tag_next;					\
      (ta).tl[1].t_value = (tag_value_t)tl_vlist(ta_start__ap);		\
      va_end(ta_start__ap);						\
    } else {								\
      (ta).tl[1].t_value = 0; (ta).tl[1].t_value = (tag_value_t)0;	\
    }									\
  } while(0)
#endif

/**Macro accessing tagged argument list.
 *
 * The ta_args() returns a pointer to tag list containing the arguments.
 *
 * @hideinitializer
 */
#define ta_args(ta) (ta).tl

/**Macro passing tagged argument list as an argument to another function.
 *
 * The ta_tags() macro expands to an tag list that can be given as arguments
 * to a function taking an variable tag item list as an argument.
 *
 * @hideinitializer
 */
#define ta_tags(ta) \
  (ta).tl[0].t_tag, (ta).tl[0].t_value, (ta).tl[1].t_tag, (ta).tl[1].t_value

/**Handle return from function with tagged argument list
 *
 * The ta_end() macro handles return from function whose tagged argument
 * list was initialized by ta_start().
 *
 * The ta_end() macro returns no value.
 *
 * @hideinitializer
 */
#if SU_HAVE_TAGSTACK
#define ta_end(ta) (va_end((ta).ap), (ta).tl->t_tag = NULL, 0)
#else
#define ta_end(ta)					   \
  ((((ta).tl[1].t_value) ?				   \
    (tl_vfree((tagi_t *)((ta).tl[1].t_value))) : (void)0), \
   (ta).tl[1].t_value = 0, va_end((ta).ap), 0)
#endif

SOFIA_END_DECLS

#endif /* !defined(SU_TAGARG_H) */

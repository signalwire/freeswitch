/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
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

#ifndef SU_STRLST_H  /** Defined when <sofia-sip/su_strlst.h> has been included. */
#define SU_STRLST_H
/**@ingroup su_strlst
 * @file sofia-sip/su_strlst.h String lists
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri May  3 09:13:22 2002 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct su_strlst_s su_strlst_t;

/** Create a string list. */
SU_DLL su_strlst_t *su_strlst_create(su_home_t *home)
     __attribute__((__malloc__));
/** Destroy a string list. */
SU_DLL void su_strlst_destroy(su_strlst_t *);

SU_DLL su_strlst_t *su_strlst_create_with(su_home_t *, char const *, ...)
     __attribute__((__malloc__));

SU_DLL su_strlst_t *su_strlst_create_with_dup(su_home_t *, char const *, ...)
     __attribute__((__malloc__));

SU_DLL su_strlst_t *su_strlst_vcreate_with(su_home_t *,
					   char const *,
					   va_list va)
     __attribute__((__malloc__));

SU_DLL su_strlst_t *su_strlst_vcreate_with_dup(su_home_t *,
					       char const *,
					       va_list va)
     __attribute__((__malloc__));

/** Shallow copy a string list. */
SU_DLL su_strlst_t *su_strlst_copy(su_home_t *home, su_strlst_t const *orig);

/** Deep copy a string list. */
SU_DLL su_strlst_t *su_strlst_dup(su_home_t *home, su_strlst_t const *orig);

/** Duplicate and append a string to list. */
SU_DLL char *su_strlst_dup_append(su_strlst_t *, char const *str);

/** Append a string to list. */
SU_DLL char const *su_strlst_append(su_strlst_t *, char const *str);

/** Append a formatted string to the list. */
SU_DLL char const *su_slprintf(su_strlst_t *self, char const *fmt, ...);

/** Append a formatted string to the list. */
SU_DLL char const *su_slvprintf(su_strlst_t *self, char const *fmt, va_list ap);

/** Get a numbered item from list. */
SU_DLL char const *su_strlst_item(su_strlst_t const *, usize_t i);

/** Set a numbered item to list */
SU_DLL char const *su_strlst_set_item(su_strlst_t *self, usize_t i,
				      char const *s);

/** Remove a numbered item from list. */
SU_DLL char const *su_strlst_remove(su_strlst_t *, usize_t i);

/** Concatenate list of strings to one string. */
SU_DLL char *su_strlst_join(su_strlst_t *s, su_home_t *home,
			     char const *sep)
     __attribute__((__malloc__));

/** Split a string. */
SU_DLL su_strlst_t *su_strlst_split(su_home_t *, char *s, char const *sep)
     __attribute__((__malloc__));

/** Duplicate and split a string. */
SU_DLL su_strlst_t *su_strlst_dup_split(su_home_t *, char const *s,
					char const *sep)
     __attribute__((__malloc__));

/** Get number of items in list. */
SU_DLL usize_t su_strlst_len(su_strlst_t const *l);

#if SU_HAVE_INLINE
su_inline
su_home_t *su_strlst_home(su_strlst_t *s)
{
  return (su_home_t *)s;
}
#else
#define su_strlst_home(s) ((su_home_t *)(s))
#endif

/** Get a string array from list. */
SU_DLL char const **su_strlst_get_array(su_strlst_t *self)
     __attribute__((__malloc__));

/** Free a string array */
SU_DLL void su_strlst_free_array(su_strlst_t *self, char const * array[]);

SOFIA_END_DECLS

#endif /* ! defined(SU_STRLST_H) */

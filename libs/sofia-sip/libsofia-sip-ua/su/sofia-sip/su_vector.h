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

#ifndef SU_VECTOR_H
/** Defined when <sofia-sip/su_vector.h> has been included. */
#define SU_VECTOR_H

/**@file sofia-sip/su_vector.h
 * @brief Vector interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Sep 27 14:31:15 2002 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct su_vector_s su_vector_t;
typedef void (* su_free_func_t) (void *data);

/** Create a vector. */
SU_DLL su_vector_t *su_vector_create(su_home_t *home, su_free_func_t free_f)
     __attribute__((__malloc__));

/** Destroy a vector. */
SU_DLL void su_vector_destroy(su_vector_t *);

/** Insert an item to vector. */
SU_DLL int su_vector_insert(su_vector_t *vector, usize_t index, void *item);

SU_DLL int su_vector_remove(su_vector_t *vector, usize_t index);

/** Append a item to vector. */
SU_DLL int su_vector_append(su_vector_t *, void *item);

/** Get a numbered item from vector. */
SU_DLL void *su_vector_item(su_vector_t const *, usize_t i);

/** Get number of items in vector. */
SU_DLL usize_t su_vector_len(su_vector_t const *l);

SU_DLL int su_vector_empty(su_vector_t *vector);
SU_DLL int su_vector_is_empty(su_vector_t const *vector);

#if SU_HAVE_INLINE
su_inline
su_home_t *su_vector_home(su_vector_t *s)
{
  return (su_home_t *)s;
}
#else
#define su_vector_home(s) ((su_home_t *)(s))
#endif

/** Get an array of pointers from the vector. */
SU_DLL void **su_vector_get_array(su_vector_t *)
     __attribute__((__malloc__));

/** Free the array */
SU_DLL void su_vector_free_array(su_vector_t *, void *array[]);

SOFIA_END_DECLS

#endif /* !defined SU_VECTOR_H */

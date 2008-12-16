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

/**@defgroup su_vector Vectors
 *
 * Pointer vectors.
 *
 * Unidimensional arrays using #su_home_t.
 *
 */

/**@ingroup su_vector
 * @CFILE su_vector.c
 * @brief Simple pointer vectors
 *
 * The vectors are resizeable unidimensional arrays.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Sep 27 14:43:29 2002 ppessi
 */

#include "config.h"

#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include <limits.h>
#include <string.h>

#include <assert.h>

#include "sofia-sip/su_config.h"
#include "sofia-sip/su_vector.h"

enum { N = 8 };

struct su_vector_s
{
  su_home_t         v_home[1];
  su_home_t        *v_parent;
  size_t            v_size;
  size_t            v_len;
  su_free_func_t    v_free_func;
  void              **v_list;
};

/** Create a vector.
 *
 * The function su_vector_create() creates a pointer vector object. The
 * vector is initially empty. The function clones a memory home for the
 * vector object from @a home. If a @a free_func is provided then that
 * will be used to free the individual nodes (NULL if not used).
 */
su_vector_t *su_vector_create(su_home_t *home, su_free_func_t free_func)
{
  su_vector_t *vector;

  vector = su_home_clone(home, sizeof(*vector) + N * sizeof(*vector->v_list));
  if (vector) {
    vector->v_parent = home;
    vector->v_size = N;
    vector->v_free_func = free_func;
    vector->v_list = (void **)(vector + 1);
  }
  return vector;
}

/** Destroy a vector.
 *
 * The function su_vector_destroy() destroys a vector and frees all
 * its nodes if a freeing function is available
 */
void su_vector_destroy(su_vector_t *vector)
{
  size_t i;

  if (vector) {
    if (vector->v_free_func != NULL) {
      for (i = 0; i < vector->v_len; i++) {
        (vector->v_free_func)(vector->v_list[i]);
      }
    }

    su_home_zap(vector->v_home);
  }
}

/** Increase the list size for next item, if necessary. */
static int su_vector_make_place(su_vector_t *vector, usize_t index)
{
  if (vector->v_size <= vector->v_len + 1) {
    size_t newsize = 2 * vector->v_size * sizeof(vector->v_list[0]);
    void **list;

    if (newsize < vector->v_size * sizeof(vector->v_list[0])) /* overflow */
      return -1;

    if (vector->v_list != (void **)(vector + 1) && index == vector->v_len) {
      if (!(list = su_realloc(vector->v_home, vector->v_list, newsize)))
	return 0;
    }
    else {
      if (!(list = su_alloc(vector->v_home, newsize)))
	return 0;

      memcpy(list, vector->v_list, index * sizeof(vector->v_list[0]));
      memcpy(list + index + 1, vector->v_list + index,
	     (vector->v_len - index) * sizeof(vector->v_list[0]));

      if (vector->v_list != (void **)(vector + 1)) {
	su_free(vector->v_home, vector->v_list);
      }
    }

    vector->v_list = list;
    vector->v_size *= 2;
  }
  else {
    memmove(vector->v_list + index + 1, vector->v_list + index,
	    (vector->v_len - index) * sizeof(vector->v_list[0]));
  }

  vector->v_len++;

  return 1;
}

/**Insert an item to vector.
 *
 * The function su_vector_insert() inserts an @a item to the @a vector.
 * The items after the @a index will be moved further within the vector.
 *
 * @param vector pointer to a vector object
 * @param item   item to be appended
 * @param index  index for the new item
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int su_vector_insert(su_vector_t *vector, usize_t index, void *item)
{
  if (vector &&
      index <= vector->v_len &&
      su_vector_make_place(vector, index) > 0) {
    vector->v_list[index] = item;
    return 0;
  }
  return -1;
}

/**Remove an item from vector.
 *
 * The function su_vector_remove() removes an item from the @a vector.
 * The items after the @a index will be moved backwards within the vector.
 *
 * @param vector pointer to a vector object
 * @param index  index for the removed item
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int su_vector_remove(su_vector_t *vector, usize_t index)
{
  if (vector && index < vector->v_len) {
    if (vector->v_free_func)
        (vector->v_free_func)(vector->v_list[index]);

    memmove(vector->v_list + index,
            vector->v_list + index + 1,
            (vector->v_len - index - 1) * sizeof(vector->v_list[0]));
    vector->v_len--;
    return 0;
  }

  return -1;
}

/**Remove all items from vector.
 *
 * The function su_vector_empty() removes all items from the @a vector.
 *
 * @param vector pointer to a vector object
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 */
int su_vector_empty(su_vector_t *vector)
{
  size_t i;

  if (vector) {
    if (vector->v_free_func != NULL) {
      for (i = 0; i < vector->v_len; i++) {
        (vector->v_free_func)(vector->v_list[i]);
      }
    }

    vector->v_len = 0;
    return 0;
  }

  return -1;
}

/**Append an item to vector.
 *
 * The function su_vector_append() appends an @a item to the @a vector.
 *
 * @param vector  pointer to a vector object
 * @param item    item to be appended
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 */
int su_vector_append(su_vector_t *vector, void *item)
{
  size_t index;

  if (vector == 0)
    return -1;

  index = vector->v_len;

  if (su_vector_make_place(vector, index) <= 0)
    return -1;

  vector->v_list[index] = item;
  return 0;
}

/**Get a numbered item from list.
 *
 * The function su_vector_item() returns a numbered item from vector. The
 * numbering starts from 0.
 *
 * @param vector  pointer to a vector object
 * @param i     index
 *
 * @return
 * Pointer, if item exists, or NULL upon an error.
 */
void *su_vector_item(su_vector_t const *vector, usize_t i)
{
  if (vector && i < vector->v_len)
    return vector->v_list[i];
  else
    return NULL;
}

/** Get number of items in list.
 *
 * The function su_vector_len() returns the number of items in the
 * vector.
 */
usize_t su_vector_len(su_vector_t const *l)
{
  return l ? l->v_len : 0;
}

int su_vector_is_empty(su_vector_t const *vector)
{
  return su_vector_len(vector) == 0;
}

/**Get a pointer array from list.
 *
 * The function su_vector_get_array() returns an array of pointer. The
 * length of the array is always one longer than the length of the vector,
 * and the last item in the returned array is always NULL.
 *
 * @param vector  pointer to a vector object
 *
 * @return
 * Pointer to array, or NULL if error occurred.
 */
void **su_vector_get_array(su_vector_t *vector)
{
  if (vector) {
    void **retval;
    size_t newsize = sizeof(retval[0]) * (vector->v_len + 1);

    retval = su_alloc(vector->v_home, newsize);

    if (retval) {
      retval[vector->v_len] = NULL;
      return memcpy(retval, vector->v_list, sizeof(retval[0]) * vector->v_len);
    }
  }

  return NULL;
}

/**Free a string array.
 *
 * The function su_vector_free_array() discards a string array allocated
 * with su_vector_get_array().
 *
 * @param vector  pointer to a vector object
 * @param array  string array to be freed
 *
 */
void su_vector_free_array(su_vector_t *vector, void **array)
{
  su_free(vector->v_home, array);
}

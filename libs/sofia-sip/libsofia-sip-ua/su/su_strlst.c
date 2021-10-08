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

/**@defgroup su_strlst String Lists
 *
 * Lists of strings.
 *
 * String lists using #su_home_t.
 *
 */

/**@ingroup su_strlst
 * @CFILE su_strlst.c
 * @brief String lists.
 *
 * The string lists can be used to concatenate a large number of strings, or
 * split a string to smaller pieces (e.g., lines).
 *
 * Example of concatenating a number of strings to @a s:
 * @code
 * su_strlst_t *l = su_strlist_create(home);
 * su_strlst_append(l, "=============");
 * su_slprintf(l, "a is: %d", a)
 * su_slprintf(l, "b is: %d", b)
 * su_slprintf(l, "c is: %d", c)
 * su_strlst_append(l, "------------");
 * su_slprintf(l, "total: %d", a + b + c));
 * su_strlst_append(l, "=============");
 * s = su_strlst_join(l, "\n");
 * @endcode
 *
 * Another example, splitting a string into lines and counting the number of
 * non-empty ones:
 * @code
 * usize_t i, n;
 * su_strlst_t *l;
 *
 * l = su_strlst_split(NULL, buf, "\n");
 *
 * nonempty = 0;
 *
 * for (i = 0; i < su_strlst_len(l); i++) {
 *   n = strcspn(su_strlst_item(l, i), " \t");
 *   if (su_strlst_item(l, i)[n])
 *     nonempty++;
 * }
 *
 * su_strlst_destroy(l);
 * @endcode
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri May  3 09:22:59 2002 ppessi
 */

#include "config.h"

#include "sofia-sip/su_config.h"
#include "sofia-sip/su_strlst.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <memory.h>
#include <limits.h>
#include <string.h>

#include <assert.h>

#if defined(va_copy)
/* Xyzzy */
#elif defined(__va_copy)
#define va_copy(dst, src) __va_copy((dst), (src))
#else
#define va_copy(dst, src) (memcpy(&(dst), &(src), sizeof (va_list)))
#endif

enum { N = 8 };

struct su_strlst_s
{
  su_home_t    sl_home[1];
  size_t       sl_size;
  size_t       sl_len;
  size_t       sl_total;
  char const **sl_list;
};

/** Create a list with initial values */
static
su_strlst_t *su_strlst_vcreate_with_by(su_home_t *home,
				       char const *value,
				       va_list va,
				       int deeply)
{
  su_strlst_t *self;
  size_t i, n, m;
  size_t total, size;

  m = 0, total = 0;

  /* Count arguments and their length */
  if (value) {
    char const *s;
    va_list va0;

    va_copy(va0, va);
    for (s = value; s; m++, s = va_arg(va0, char *))
      total += strlen(s);
    va_end(va0);
  }

  for (n = N; m > n; n *= 2)
    ;

  size = sizeof(*self) + n * sizeof(*self->sl_list);
  if (deeply)
    size += total + m;

  self = su_home_clone(home, size);
  if (self) {
    self->sl_size = n;
    self->sl_list = (char const **)(self + 1);
    self->sl_len = m;
    self->sl_total = total;

    if (deeply) {
      char *s = (char *)(self->sl_list + self->sl_size);
      char *end = s + total + m;

      for (i = 0; i < m; i++) {
	assert(s);
	self->sl_list[i] = s;
	s = memccpy(s, value, '\0', end - s);
	value = va_arg(va, char const *);
      }
    }
    else {
      for (i = 0; i < m; i++) {
	self->sl_list[i] = value;
	value = va_arg(va, char const *);
      }
    }
  }

  return self;
}

/** Create an empty string list.
 *
 * The list is initially empty. The memory home for the list is cloned from
 * @a home.
 *
 */
su_strlst_t *su_strlst_create(su_home_t *home)
{
  su_strlst_t *self;

  self = su_home_clone(home, sizeof(*self) + N * sizeof(*self->sl_list));
  if (self) {
    self->sl_size = N;
    self->sl_list = (char const **)(self + 1);
  }
  return self;
}

/** Create a string list with initial values.
 *
 * The list is initialized with strings in argument. The argument list is
 * terminated with NULL. The memory home for the list is cloned from @a
 * home.
 */
su_strlst_t *su_strlst_create_with(su_home_t *home, char const *value, ...)
{
  va_list va;
  su_strlst_t *l;

  va_start(va, value);
  l = su_strlst_vcreate_with_by(home, value, va, 0);
  va_end(va);

  return l;
}

/** Create a string list with initial values.
 *
 * The string list is initialized with strings from @a va_list @a va. The
 * argument list is terminated with NULL. The memory home for the list is
 * cloned from @a home.
 */
su_strlst_t *su_strlst_vcreate_with(su_home_t *home,
				    char const *value,
				    va_list va)
{
  return su_strlst_vcreate_with_by(home, value, va, 0);
}

/** Create a string list with duplicatedd initial values.
 *
 * The list is initialized with copies of strings in argument list. The
 * argument list is terminated with NULL. The memory home for the list is
 * cloned from @a home.
 */
su_strlst_t *su_strlst_create_with_dup(su_home_t *home, char const *value, ...)
{
  va_list va;
  su_strlst_t *l;

  va_start(va, value);
  l = su_strlst_vcreate_with_by(home, value, va, 1);
  va_end(va);

  return l;
}

/** Create a string list with duplicates of initial values.
 *
 * The string list is initialized with copies of strings from @a va_list @a
 * va. The argument list is terminated with NULL. The memory home for the
 * list is cloned from @a home.
 */
su_strlst_t *su_strlst_vcreate_with_dup(su_home_t *home,
					char const *value,
					va_list va)
{
  return su_strlst_vcreate_with_by(home, value, va, 1);
}


/** Copy a string list */
static
su_strlst_t *su_strlst_copy_by(su_home_t *home,
			       su_strlst_t const *orig,
			       int deeply)
{
  su_strlst_t *self;
  size_t N, i, size, deepsize = 0;

  if (orig == NULL)
    return NULL;

  N = orig->sl_size;

  if (deeply)
    deepsize = orig->sl_len + orig->sl_total;

  size = sizeof(*self) + N * sizeof(self->sl_list[0]) + deepsize;

  self = su_home_clone(home, size);
  if (self) {
    self->sl_size = N;
    self->sl_list = (char const **)(self + 1);

    self->sl_len = N = orig->sl_len;
    self->sl_total = orig->sl_total;

    if (deeply) {
      char *s = (char *)(self->sl_list + self->sl_size);
      char *end = s + deepsize;
      for (i = 0; i < N; i++) {
	self->sl_list[i] = s;
	s = memccpy(s, orig->sl_list[i], '\0', end - s);
	assert(s);
      }
    }
    else {
      for (i = 0; i < N; i++) {
	self->sl_list[i] = orig->sl_list[i];
      }
    }
  }

  return self;
}

/** Shallow copy a string list. */
su_strlst_t *su_strlst_copy(su_home_t *home, su_strlst_t const *orig)
{
  return su_strlst_copy_by(home, orig, 0);
}

/** Deep copy a string list. */
su_strlst_t *su_strlst_dup(su_home_t *home, su_strlst_t const *orig)
{
  return su_strlst_copy_by(home, orig, 1);
}

/** Destroy a string list.
 *
 * The function su_strlst_destroy() destroys a list of strings and frees all
 * duplicated strings belonging to it.
 */
void su_strlst_destroy(su_strlst_t *self)
{
  su_home_zap(self->sl_home);
}

/** Increase the list size for next item, if necessary. */
static int su_strlst_increase(su_strlst_t *self)
{
  if (self->sl_size <= self->sl_len + 1) {
    size_t size = 2 * self->sl_size * sizeof(self->sl_list[0]);
    char const **list;

    if (self->sl_list != (char const **)(self + 1)) {
      list = su_realloc(self->sl_home, (void *)self->sl_list, size);
    } else {
      list = su_alloc(self->sl_home, size);
      if (list)
	memcpy(list, self->sl_list, self->sl_len * sizeof(*self->sl_list));
    }

    if (!list)
      return 0;

    self->sl_list = list;
    self->sl_size = 2 * self->sl_size;
  }

  return 1;
}

/**Duplicate and append a string to list.
 *
 * @param self  pointer to a string list object
 * @param str   string to be duplicated and appended
 *
 * @return
 * Pointer to duplicated string, if successful, or NULL upon an error.
 */
char *su_strlst_dup_append(su_strlst_t *self, char const *str)
{
  size_t len;

  if (str == NULL)
    str = "";

  len = strlen(str);

  if (self && su_strlst_increase(self)) {
    char *retval = su_alloc(self->sl_home, len + 1);
    if (retval) {
      memcpy(retval, str, len);
      retval[len] = 0;
      self->sl_list[self->sl_len++] = retval;
      self->sl_total += len;
    }
    return retval;
  }
  return NULL;
}

/**Append a string to list.
 *
 * The string is not copied, and it @b must not be modified while string
 * list exists.
 *
 * @param self  pointer to a string list object
 * @param str   string to be appended
 *
 * @return
 * Pointer to string, if successful, or NULL upon an error.
 */
char const *su_strlst_append(su_strlst_t *self, char const *str)
{
  if (str == NULL)
    str = "";

  if (self && su_strlst_increase(self)) {
    self->sl_list[self->sl_len++] = str;
    self->sl_total += strlen(str);
    return str;
  }
  return NULL;
}

/**Append a formatted string to the list.
 *
 * Format a string according to a @a fmt like printf(). The resulting string
 * is copied to a memory area freshly allocated from a the memory home of
 * the list and appended to the string list.
 *
 * @param self  pointer to a string list object
 * @param fmt format string
 * @param ... argument list (must match with the @a fmt format string)
 *
 * @return A pointer to a fresh copy of formatting result, or NULL upon an
 * error.
 */
char const *su_slprintf(su_strlst_t *self, char const *fmt, ...)
{
  char const *str;
  va_list ap;
  va_start(ap, fmt);
  str = su_slvprintf(self, fmt, ap);
  va_end(ap);

  return str;
}

/**Append a formatted string to the list.
 *
 * Format a string according to a @a fmt like vprintf(). The resulting
 * string is copied to a memory area freshly allocated from a the memory
 * home of the list and appended to the string list.
 *
 * @param self  pointer to a string list object
 * @param fmt  format string
 * @param ap   stdarg argument list (must match with the @a fmt format string)
 *
 * @return A pointer to a fresh copy of formatting result, or NULL upon an
 * error.
 */
char const *su_slvprintf(su_strlst_t *self, char const *fmt, va_list ap)
{
  char *str = NULL;

  if (self && su_strlst_increase(self)) {
    str = su_vsprintf(self->sl_home, fmt, ap);
    if (str) {
      self->sl_list[self->sl_len++] = str;
      self->sl_total += strlen(str);
    }
  }
  return str;
}

/**Returns a numbered item from the list of strings. The numbering starts from
 * 0.
 *
 * @param self  pointer to a string list object
 * @param i     string index
 *
 * @return
 * Pointer to string, if item exists, or NULL if index is out of bounds or
 * list does not exist.
 */
char const *su_strlst_item(su_strlst_t const *self, usize_t i)
{
  if (self && i < self->sl_len)
    return self->sl_list[i];
  else
    return NULL;
}

/**Sets a item to the list of strings.
 *
 * Note that the item numbering starts from 0.
 *
 * @param self  pointer to a string list object
 * @param i     string index
 * @param s     string to be set as item @a i
 *
 * @return
 * Pointer to string, if item exists, or NULL if index is out of bounds or
 * list does not exist.
 */
char const *su_strlst_set_item(su_strlst_t *self, usize_t i, char const *s)
{
  char const *old = NULL;

  if (self == NULL)
    return NULL;
  else if (i == self->sl_len)
    return (void)su_strlst_append(self, s), NULL;
  else if (i > self->sl_len)
    return NULL;

  if (s == NULL)
    s = "";

  old = self->sl_list[i];

  self->sl_list[i] = s;

  return old;
}

/**Removes a numbered item from the list of strings. The numbering starts from
 * 0. The caller is responsible of reclaiming memory used by the removed string.
 *
 * @param self  pointer to a string list object
 * @param i     string index
 *
 * @return
 * Pointer to string, if item exists, or NULL if index is out of bounds or
 * list does not exist.
 */
SU_DLL char const *su_strlst_remove(su_strlst_t *self, usize_t i)
{
  if (self && i < self->sl_len) {
    char const *s = self->sl_list[i];

    memmove(&self->sl_list[i], &self->sl_list[i + 1],
	    &self->sl_list[self->sl_len] - &self->sl_list[i]);

    self->sl_len--;

    return s;
  }
  else
    return NULL;
}



/** Concatenate list of strings to one string.
 *
 * The function su_strlst_join() concatenates the list of strings. Between
 * each string in list it uses @a sep. The separator is not inserted after
 * the last string in list, but one can always append an empty string to the
 * list.
 *
 * The string is allocated from the memory @a home. If @a home is NULL, the
 * string is allocated using malloc().
 *
 * @param self  pointer to a string list object
 * @param home  home pointer
 * @param sep   separator (may be NULL)
 *
 * @return
 *
 * The function su_strlst_join() returns a concatenation of the strings in
 * list, or NULL upon an error.
 */
char *su_strlst_join(su_strlst_t *self, su_home_t *home, char const *sep)
{
  if (!sep)
    sep = "";

  if (self && self->sl_len > 0) {
    size_t seplen = strlen(sep);
    size_t total = self->sl_total + seplen * (self->sl_len - 1);
    char *retval;

    retval = su_alloc(home, total + 1);

    if (retval) {
      char *s = retval;
      size_t i = 0, len;

      for (;;) {
	len = strlen(self->sl_list[i]);
	memcpy(s, self->sl_list[i], len), s += len;
	if (++i >= self->sl_len)
	  break;
	memcpy(s, sep, seplen),	s += seplen;
      }
      *s = '\0';
      assert(s == retval + total);
    }

    return retval;
  }

  return su_strdup(home, "");
}

su_inline
su_strlst_t *
su_strlst_split0(su_strlst_t *l, char *str, char const *sep)
{
  size_t n = sep ? strlen(sep) : 0;
  char *s;

  if (n > 0) {
    while ((s = strstr(str, sep))) {
      *s = '\0';
      if (!su_strlst_append(l, str))
	return NULL;
      str = s + n;
    }
  }

  if (!su_strlst_append(l, str))
    return NULL;

  return l;
}

/**Split a string.
 *
 * Splits a string to substrings. It returns a string list object. The
 * string to be split is not copied but instead modified in place. Use
 * su_strlst_dup_split() if you do not want to modify @a str.
 *
 * @param home  home pointer
 * @param str     string to be split
 * @param sep   separator between substrings
 *
 * @return
 * Pointer to list of strings, if successful, or NULL upon an error.
 */
su_strlst_t *
su_strlst_split(su_home_t *home, char *str, char const *sep)
{
  if (str) {
    su_strlst_t *l = su_strlst_create(home);

    if (!su_strlst_split0(l, str, sep))
      su_strlst_destroy(l), l = NULL;

    return l;
  }
  return NULL;
}

/**Duplicate and split a string.
 *
 * Duplicates a string and splits the result to substrings. It returns a
 * string list object. The string to be splitted is not modified.
 *
 * @param home  home pointer
 * @param cstr  string to be split
 * @param sep   separator between substrings
 *
 * @return
 * Pointer to list of strings, if successful, or NULL upon an error.
 */
su_strlst_t *su_strlst_dup_split(su_home_t *home,
				 char const *cstr,
				 char const *sep)
{
  if (cstr) {
    su_strlst_t *l = su_strlst_create(home);

    if (l) {
      char *s = su_strdup(su_strlst_home(l), cstr);

      if (s && !su_strlst_split0(l, s, sep))
	su_strlst_destroy(l), l = NULL;
    }

    return l;
  }
  return NULL;
}

/** Get number of items in list.
 *
 * The function su_strlst_len() returns the number of items in the
 * string list.
 *
 */
usize_t su_strlst_len(su_strlst_t const *l)
{
  return l ? l->sl_len : 0;
}

/**Get a string array from list.
 *
 * The function su_strlst_get_array() returns an array of strings. The
 * length of the array is always one longer than the length of the string
 * list, and the last string in the returned array is always NULL.
 *
 * @param self pointer to a string list object
 *
 * @return
 * Pointer to array of strings, or NULL if error occurred.
 */
char const **su_strlst_get_array(su_strlst_t *self)
{
  if (self) {
    char const **retval;
    size_t size = sizeof(retval[0]) * (self->sl_len + 1);

    retval = su_alloc(self->sl_home, size);

    if (retval) {
      memcpy(retval, self->sl_list, sizeof(retval[0]) * self->sl_len);
      retval[self->sl_len] = NULL;
      return retval;
    }
  }

  return NULL;
}

/**Free a string array.
 *
 * The function su_strlst_free_array() discards a string array allocated
 * with su_strlst_get_array().
 *
 * @param self  pointer to a string list object
 * @param array  string array to be freed
 *
 */
void su_strlst_free_array(su_strlst_t *self, char const **array)
{
  if (array)
    su_free(self->sl_home, (void *)array);
}

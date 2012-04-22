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

/**@ingroup su_alloc
 *
 * @CFILE su_strdup.c  Home-based string duplication functions
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Jul 19 10:06:14 2000 ppessi
 */

#include "config.h"

#include <string.h>
#include "sofia-sip/su_alloc.h"

/** Duplicate a string, allocate memory from @a home.
 *
 * The function su_strdup() duplicates the string @a s.  It allocates @c
 * strlen(s)+1 bytes from @a home, copies the contents of @a s to the newly
 * allocated memory, and returns pointer to the duplicated string.
 *
 * @param home  pointer to memory home
 * @param s     string to be duplicated
 *
 * @return The function su_strdup() returns pointer to the newly created
 *         string, or @c NULL upon an error.
 */
char *su_strdup(su_home_t *home, char const *s)
{
  if (s) {
    size_t n = strlen(s);
    char *retval = su_alloc(home, n + 1);
    if (retval)
      strncpy(retval, s, n)[n] = 0;
    return retval;
  }
  return NULL;
}

/**Concate two strings, allocate memory for result from @a home.
 *
 * Concatenate the strings @a s1 and @a s2. The @c strlen(s1)+strlen(s2)+1
 * bytes is allocated from @a home, the contents of @a s1 and @a s2 is
 * copied to the newly allocated memory area, and pointer to the
 * concatenated string is returned.
 *
 * @param home  pointer to memory home
 * @param s1    string to be first string
 * @param s2    string to be first string
 *
 * @return Pointer to the newly created string is returned, or @c NULL upon
 * an error.
 */
char *su_strcat(su_home_t *home, char const *s1, char const *s2)
{
  size_t n1, n2;
  char *retval;

  if (s1 == NULL)
    return su_strdup(home, s2);
  else if (s2 == NULL)
    return su_strdup(home, s1);

  n1 = strlen(s1); n2 = strlen(s2);
  retval = su_alloc(home, n1 + n2 + 1);
  if (retval) {
    memcpy(retval, s1, n1);
    memcpy(retval + n1, s2, n2);
    retval[n1 + n2] = '\0';
  }

  return retval;
}

/**Concate multiple strings, allocate memory for result from @a home.
 *
 * Concatenate the strings in list. The lenght of result is calculate,
 * result is allocated from @a home, the contents of strings is copied to
 * the newly allocated memory arex, and pointer to the concatenated string is
 * returned.
 *
 * @param home  pointer to memory home
 * @param ...  NULL-terminated list of strings to be concatenated
 *
 * @return Pointer to the newly created string is returned, or @c NULL upon
 * an error.
 */
char *su_strcat_all(su_home_t *home, ...)
{
  int i, n;
  size_t size = 0;
  va_list va;
  char *s, *retval, *end;

  /* Count number arguments and their size */
  va_start(va, home);
  s = va_arg(va, char *);
  for (n = 0; s; s = va_arg(va, char *), n++)
    size += strlen(s);
  va_end(va);

  retval = su_alloc(home, size + 1);
  if (retval) {
    s = retval;
    end = s + size + 1;

    va_start(va, home);

    for (i = 0; i < n; i++)
      s = (char *)memccpy(s, va_arg(va, char const *), '\0', end - s) - 1;

    va_end(va);

    retval[size] = '\0';
  }

  return retval;
}



/** Duplicate a string with given size, allocate memory from @a home.
 *
 * The function su_strndup() duplicates the string @a s.  It allocates @c n+1
 * bytes from @a home, copies the contents of @a s to the newly allocated
 * memory, and returns pointer to the duplicated string.  The duplicated
 * string is always NUL-terminated.
 *
 * @param home  pointer to memory home
 * @param s     string to be duplicated
 * @param n     size of the resulting string
 *
 * @return The function su_strndup() returns pointer to the newly created
 *         string, or @c NULL upon an error.
 */
char *su_strndup(su_home_t *home, char const *s, isize_t n)
{
  if (s) {
    char *retval = su_alloc(home, n + 1);
    if (retval)
      strncpy(retval, s, n)[n] = 0;
    return retval;
  }
  return NULL;
}

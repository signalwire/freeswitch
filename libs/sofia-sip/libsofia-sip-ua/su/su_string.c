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

/**@internal @file su_string.c
 * @brief Various string utility functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 */

#include "config.h"

#include <sofia-sip/su_string.h>

#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>

/** ASCII-case-insensitive substring search.
 *
 * Search for substring ASCII-case-insensitively.
 *
 */
char *
su_strcasestr(const char *haystack,
	      const char *needle)
{
  unsigned char lcn, ucn;
  size_t i;

  if (haystack == NULL || needle == NULL)
    return NULL;

  lcn = ucn = needle[0];
  if ('A' <= lcn && lcn <= 'Z')
    lcn += 'a' - 'A';
  else if ('a' <= ucn && ucn <= 'z')
    ucn -= 'a' - 'A';

  if (lcn == 0)
    return (char *)haystack;

  while (haystack[0] != 0) {
    if (lcn == haystack[0] || ucn == haystack[0]) {
      for (i = 1; ; i++) {
	char n = needle[i], h = haystack[i];
	if (n == 0)
	  return (char *)haystack;
	if (h == 0)
	  return NULL;
	if (n == h)
	  continue;
	if ((n ^ h) != ('A' ^ 'a'))
	  break;
	if ('A' <= n && n <= 'Z')
	  n += 'a' - 'A';
	else if ('A' <= h && h <= 'Z')
	  h += 'a' - 'A';
	if (n != h)
	  break;
      }
    }
    haystack++;
  }

  return NULL;		/* Not found */
}

/** ASCII-case-insensitive comparison.
 *
 * Compare two strings colliding upper case and lower case ASCII chars.
 * Avoid using locale-dependent strcasecmp(). A NULL pointer compares as an
 * empty string.
 *
 * @retval An int less than zero if @a s1 is less than @a s2
 * @retval Zero if @a s1 matches @a s2
 * @retval An int greater than zero if @a s1 is greater than @a s2
 */
int
su_strcasecmp(char const *s1,
	      char const *s2)
{
  unsigned char const *A = (unsigned char const *)(s1 ? s1 : "");
  unsigned char const *B = (unsigned char const *)(s2 ? s2 : "");

  for (;;) {
    unsigned char a = *A++, b = *B++;

    int value = (int)a - (int)b;

    if (a == 0)
      return value;

    if (value == 0)
      continue;

    if ('A' <= a && a <= 'Z')
      a += 'a' - 'A';

    if ('A' <= b && b <= 'Z')
      b += 'a' - 'A';

    value = (int)a - (int)b;

    if (value)
      return value;
  }
}

/** ASCII-case-insensitive comparison.
 *
 * Compare first @a n bytes of two strings colliding upper case and lower
 * case ASCII chars. Avoid using locale-dependent strncasecmp(). A NULL
 * pointer compares as an empty string.
 *
 * @retval An int less than zero if first @a n bytes of @a s1 is less than @a s2
 * @retval Zero if first @a n bytes of @a s1 matches @a s2
 * @retval An int greater than zero if first @a n bytes of  @a s1 is greater than @a s2
 */
int
su_strncasecmp(char const *s1,
	       char const *s2,
	       size_t n)
{
  unsigned char const *A = (unsigned char const *)(s1 ? s1 : "");
  unsigned char const *B = (unsigned char const *)(s2 ? s2 : "");

  if (n == 0 || A == B || memcmp(A, B, n) == 0)
    return 0;

  for (;;) {
    unsigned char a, b;
    int value;

    if (n-- == 0)
      return 0;

    a = *A++, b = *B++;

    value = a - b;

    if (a == 0)
      return value;
    if (value == 0)
      continue;

    if ('A' <= a && a <= 'Z')
      a += 'a' - 'A';

    if ('A' <= b && b <= 'Z')
      b += 'a' - 'A';

    value = a - b;

    if (value)
      return value;
  }
}

/** Check if two strings match.
 *
 * Compare two strings. Accept NULL arguments: two NULL pointers match each
 * other, but otherwise NULL pointer does not match anything else, not even
 * empty string.
 *
 * @param s1 
 *
 * @retval One if @a s1 matches @a s2
 * @retval Zero if @a s1 does not match @a s2
 */
int
su_strmatch(char const *s1, char const *s2)
{
  if (s1 == s2)
    return 1;

  if (s1 == NULL || s2 == NULL)
    return 0;

  return strcmp(s1, s2) == 0;
}

/** ASCII-case-insensitive string match.
 *
 * Match two strings colliding upper case and lower case ASCII characters.
 * Avoid using locale-dependent strncasecmp(). Accept NULL arguments: two
 * NULL pointers match each other, but otherwise NULL pointer does not match
 * anything else, not even empty string.
 *
 * @retval One if first @a n bytes of @a s1 matches @a s2
 * @retval Zero if first @a n bytes of @a s1 do not match @a s2
 */
int
su_casematch(char const *s1, char const *s2)
{
  if (s1 == s2)
    return 1;

  if (s1 == NULL || s2 == NULL)
    return 0;

  for (;;) {
    unsigned char a = *s1++, b = *s2++;

    if (b == 0)
      return a == b;

    if (a == b)
      continue;

    if ('A' <= a && a <= 'Z') {
      if (a + 'a' - 'A' != b)
	return 0;
    }
    else if ('A' <= b && b <= 'Z') {
      if (a != b + 'a' - 'A')
	return 0;
    }
    else
      return 0;
  }
}

/** String prefix match.
 *
 * Match first @a n bytes of two strings. If @a n is 0, match always, even
 * if arguments are NULL. Otherwise, accept NULL arguments: two NULL
 * pointers match each other. NULL pointer does not match
 * anything else, not even empty string.
 *
 * @retval One if first @a n bytes of @a s1 matches @a s2
 * @retval Zero if first @a n bytes of @a s1 do not match @a s2
 */
int
su_strnmatch(char const *s1,
	      char const *s2,
	      size_t n)
{
  if (n == 0)
    return 1;

  if (s1 == s2)
    return 1;

  if (s1 == NULL || s2 == NULL)
    return 0;

  return strncmp(s1, s2, n) == 0;
}

/** ASCII-case-insensitive string match.
 *
 * Compare two strings colliding upper case and lower case ASCII characters.
 * Avoid using locale-dependent strncasecmp().
 *
 * @retval One if first @a n bytes of @a s1 matches @a s2
 * @retval Zero if first @a n bytes of @a s1 do not match @a s2
 */
int
su_casenmatch(char const *s1,
	      char const *s2,
	      size_t n)
{
  if (n == 0)
    return 1;

  if (s1 == s2)
    return 1;

  if (s1 == NULL || s2 == NULL)
    return 0;

  if (strncmp(s1, s2, n) == 0)
    return 1;

  while (n-- > 0) {
    unsigned char a = *s1++, b = *s2++;

    if (a == 0 || b == 0)
      return a == b;

    if (a == b)
      continue;

    if ('A' <= a && a <= 'Z') {
      if (a + 'a' - 'A' != b)
	return 0;
    }
    else if ('A' <= b && b <= 'Z') {
      if (a != b + 'a' - 'A')
	return 0;
    }
    else
      return 0;
  }

  return 1;
}

/** Search a string for a set of characters.
 *
 * Calculate the length of the initial segment of first @a n bytes of @a s
 * which consists entirely of characters in @a accept.
 *
 * @param s string to search for characters
 * @param n limit of search length
 * @param accept set of characters to accept
 *
 * @return
 * Number of characters in the prefix of @a s which consists only of
 * characters from @a accept.
 */
size_t
su_strnspn(char const *s, size_t n, char const *accept)
{
  size_t len;
  size_t asize;

  if (accept == NULL || s == NULL)
    return 0;

  asize = strlen(accept);

  if (asize == 0) {
    return 0;
  }
  else if (asize == 1) {
    char c, a = accept[0];
    for (len = 0; len < n && (c = s[len]) && c == a; len++)
      ;
  }
  else if (asize == 2) {
    char c, a1 = accept[0], a2 = accept[1];
    for (len = 0; len < n && (c = s[len]) && (c == a1 || c == a2); len++)
      ;
  }
  else {
    size_t i;
    char c, a1 = accept[0], a2 = accept[1];

    for (len = 0; len < n && (c = s[len]); len++) {

      if (c == a1 || c == a2)
	continue;

      for (i = 2; i < asize; i++) {
	if (c == accept[i])
	  break;
      }

      if (i == asize)
	break;
    }
  }

  return len;
}

/** Search a string for a set of characters.
 *
 * Calculate the length of the initial segment of first @a n bytes of @a s
 * which does not constists of characters in @a reject.
 *
 * @param s string to search for characters
 * @param n limit of search length
 * @param reject set of characters to reject
 *
 * @return
 * Number of characters in the prefix of @a s which are not in @a reject.
 */
size_t
su_strncspn(char const *s, size_t n, char const *reject)
{
  size_t len;
  size_t rsize;

  if (s == NULL)
    return 0;

  if (reject == NULL)
    rsize = 0;
  else
    rsize = strlen(reject);

  if (rsize == 0) {
#if HAVE_STRNLEN
    len = strnlen(s, n);
#else
    for (len = 0; len < n && s[len]; len++)
      ;
#endif
  }
  else if (rsize == 1) {
    char c, rej = reject[0];
    for (len = 0; len < n && (c = s[len]) && c != rej; len++)
      ;
  }
  else if (rsize == 2) {
    char c, rej1 = reject[0], rej2 = reject[1];
    for (len = 0; len < n && (c = s[len]) && c != rej1 && c != rej2; len++)
      ;
  }
  else {
    size_t i;
    char c, rej1 = reject[0], rej2 = reject[1];
    for (len = 0; len < n && (c = s[len]) && c != rej1 && c != rej2; len++) {
      for (i = 2; i < rsize; i++)
	if (c == reject[i])
	  return len;
    }
  }

  return len;
}

/**Scan memory for a set of bytes.
 *
 * Calculates the length of the memory area @a mem which consists entirely
o * of bytes in @a accept.
 *
 * @param mem        pointer to memory area
 * @param memlen     size of @a mem in bytes
 * @param accept     pointer to table containing bytes to accept
 * @param acceptlen  size of @a accept table
 *
 * @return
 * The number of consequtive bytes in the memory area @a which consists
 * entirely of bytes in @a accept.
 */
size_t su_memspn(const void *mem, size_t memlen,
		 const void *accept, size_t acceptlen)
{
  size_t i;

  unsigned char const *m = mem, *a = accept;

  char accepted[UCHAR_MAX + 1];

  if (mem == NULL || memlen == 0 || acceptlen == 0 || accept == NULL)
    return 0;

  memset(accepted, 0, sizeof accepted);

  for (i = 0; i < acceptlen; i++)
    accepted[a[i]] = 1;

  for (i = 0; i < memlen; i++)
    if (!accepted[m[i]])
      break;

  return i;
}

/**Search memory for bytes not in a given set.
 *
 * Calculates the length of the memory area @a mem which consists entirely
 * of bytes not in @a reject.
 *
 * @param mem        pointer to memory area
 * @param memlen     size of @a mem in bytes
 * @param reject     pointer to table containing bytes to reject
 * @param rejectlen  size of @a reject table
 *
 * @return
 * The number of consequtive bytes in the memory area @a which are not in @a
 * reject.
 * @par
 * If @a rejectlen is 0, or @a reject is NULL, it returns @a memlen, size of
 * the memory area.
 */
size_t 
su_memcspn(const void *mem, size_t memlen,
	   const void *reject, size_t rejectlen)
{
  size_t i;

  unsigned char const *m = mem, *r = reject;

  char rejected[UCHAR_MAX + 1];

  if (mem == NULL || memlen == 0)
    return 0;

  if (rejectlen == 0 || reject == 0)
    return memlen;

  memset(rejected, 0, sizeof rejected);

  for (i = 0; i < rejectlen; i++)
    rejected[r[i]] = 1;

  for (i = 0; i < memlen; i++)
    if (rejected[m[i]])
      break;

  return i;
}

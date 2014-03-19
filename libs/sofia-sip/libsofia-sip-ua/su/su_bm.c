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

/**@internal
 * @file su_bm.c
 * @brief Search with Boyer-Moore algorithm
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Apr 11 16:35:16 2005 ppessi
 *
 */

#include "config.h"

#include <sofia-sip/su_bm.h>

#include <sys/types.h>
#include <stddef.h>
#include <limits.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifndef TORTURELOG
#define TORTURELOG(x) (void)0
#endif

struct bw_fwd_table {
  unsigned char table[UCHAR_MAX + 1];
};

/** Build forward skip table #bm_fwd_table_t for Boyer-Moore algorithm. */
static
bm_fwd_table_t *
bm_memmem_study0(char const *needle, size_t nlen, bm_fwd_table_t *fwd)
{
  size_t i;

  if (nlen >= UCHAR_MAX) {
    needle += nlen - UCHAR_MAX;
    nlen = UCHAR_MAX;
  }

  memset(&fwd->table, (unsigned char)nlen, sizeof fwd->table);

  for (i = 0; i < nlen; i++) {
    fwd->table[(unsigned short)needle[i]] = (unsigned char)(nlen - i - 1);
  }

  return fwd;
}

/** @defgroup su_bm Fast string searching with Boyer-Moore algorithm
 *
 * The Boyer-Moore algorithm is used to implement fast substring search. The
 * algorithm has some overhead caused by filling a table. Substring search
 * then requires at most 1 / substring-length less string comparisons. On
 * modern desktop hardware, Boyer-Moore algorithm is seldom faster than the
 * naive implementation if the searched substring is shorter than the cache
 * line.
 *
 */

/**@ingroup su_bm
 * @typedef struct bw_fwd_table bm_fwd_table_t;
 *
 * Forward skip table for Boyer-Moore algorithm.
 *
 */

/** Build case-sensitive forward skip table #bm_fwd_table_t
 *  for Boyer-Moore algorithm.
 * @ingroup su_bm
 */
bm_fwd_table_t *
bm_memmem_study(char const *needle, size_t nlen)
{
  bm_fwd_table_t *fwd = malloc(sizeof *fwd);

  if (fwd)
    bm_memmem_study0(needle, nlen, fwd);

  return fwd;
}

/** Search for a substring using Boyer-Moore algorithm.
 * @ingroup su_bm
 */
char *
bm_memmem(char const *haystack, size_t hlen,
	  char const *needle, size_t nlen,
	  bm_fwd_table_t *fwd)
{
  size_t i, j;
  bm_fwd_table_t fwd0[1];

  if (nlen == 0)
    return (char *)haystack;
  if (needle == NULL || haystack == NULL || nlen > hlen)
    return NULL;

  if (nlen == 1) {
    for (i = 0; i < hlen; i++)
      if (haystack[i] == needle[0])
	return (char *)haystack + i;
    return NULL;
  }

  if (!fwd)
    fwd = bm_memmem_study0(needle, nlen, fwd0);

  for (i = j = nlen - 1; i < hlen;) {
    unsigned char h = haystack[i];
    if (h == needle[j]) {
      TORTURELOG(("match \"%s\" at %u\nwith  %*s\"%.*s*%s\": %s\n",
		  haystack, (unsigned)i,
		  (int)(i - j), "", (int)j, needle, needle + j + 1,
		  j == 0 ? "match!" : "back by 1"));
      if (j == 0)
	return (char *)haystack + i;
      i--, j--;
    }
    else {
      if (fwd->table[h] > nlen - j) {
	TORTURELOG(("match \"%s\" at %u\n"
		    "last  %*s\"%.*s*%s\": (by %u)\n",
		    haystack, (unsigned)i,
		    (int)(i - j), "",
		    (int)j, needle, needle + j + 1, fwd->table[h]));
      	i += fwd->table[h];
      }
      else {
	TORTURELOG(("match \"%s\" at %u\n"
		    "2nd   %*s\"%.*s*%s\": (by %u)\n",
		    haystack, (unsigned)i,
		    (int)(i - j), "",
		    (int)j, needle, needle + j + 1, (unsigned)(nlen - j)));
	i += nlen - j;
      }
      j = nlen - 1;
    }
  }

  return NULL;
}


/** Build forward skip table for Boyer-Moore algorithm */
static
bm_fwd_table_t *
bm_memcasemem_study0(char const *needle, size_t nlen, bm_fwd_table_t *fwd)
{
  size_t i;

  if (nlen >= UCHAR_MAX) {
    needle += nlen - UCHAR_MAX;
    nlen = UCHAR_MAX;
  }

  for (i = 0; i < UCHAR_MAX; i++)
    fwd->table[i] = (unsigned char)nlen;

  for (i = 0; i < nlen; i++) {
    unsigned char n = tolower((const unsigned char)needle[i]);
    fwd->table[n] = (unsigned char)(nlen - i - 1);
  }

  return fwd;
}

/** Build case-insensitive forward skip table for Boyer-Moore algorithm.
 * @ingroup su_bm
 */
bm_fwd_table_t *
bm_memcasemem_study(char const *needle, size_t nlen)
{
  bm_fwd_table_t *fwd = malloc(sizeof *fwd);

  if (fwd)
    bm_memcasemem_study0(needle, nlen, fwd);

  return fwd;
}

/** Search for substring using Boyer-Moore algorithm.
 * @ingroup su_bm
 */
char *
bm_memcasemem(char const *haystack, size_t hlen,
	      char const *needle, size_t nlen,
	      bm_fwd_table_t *fwd)
{
  size_t i, j;
  bm_fwd_table_t fwd0[1];

  if (nlen == 0)
    return (char *)haystack;
  if (needle == 0 || haystack == 0 || nlen > hlen)
    return NULL;

  if (nlen == 1) {
    for (i = 0; i < hlen; i++)
      if (haystack[i] == needle[0])
	return (char *)haystack + i;
    return NULL;
  }

  if (!fwd) {
    fwd = bm_memcasemem_study0(needle, nlen, fwd0);
  }

  for (i = j = nlen - 1; i < hlen;) {
    unsigned char h = haystack[i], n = needle[j];
    if (isupper(h))
      h = tolower(h);
    if (isupper(n))
      n = tolower(n);

    if (h == n) {
      TORTURELOG(("match \"%s\" at %u\n"
		  "with  %*s\"%.*s*%s\": %s\n",
		  haystack, (unsigned)i,
		  (int)(i - j), "", (int)j, needle, needle + j + 1,
		  j == 0 ? "match!" : "back by 1"));
      if (j == 0)
	return (char *)haystack + i;
      i--, j--;
    }
    else {
      if (fwd->table[h] > nlen - j) {
	TORTURELOG(("match \"%s\" at %u\n"
		    "last  %*s\"%.*s*%s\": (by %u)\n",
		    haystack, (unsigned)i,
		    (int)(i - j), "", (int)j, needle, needle + j + 1,
		    fwd->table[h]));
      	i += fwd->table[h];
      }
      else {
	TORTURELOG(("match \"%s\" at %u\n"
		    "2nd   %*s\"%.*s*%s\": (by %u)\n",
		    haystack, (unsigned)i,
		    (int)(i - j), "", (int)j, needle, needle + j + 1,
		    (unsigned)(nlen - j)));
	i += nlen - j;
      }
      j = nlen - 1;
    }
  }

  return NULL;
}

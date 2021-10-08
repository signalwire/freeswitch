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
 * @ingroup msg_parser
 * @file msg_generic.c
 * @brief Functions for generic headers
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jan 23 20:08:00 2003 ppessi
 *
 */

#include "config.h"

#include <sofia-sip/su_alloc.h>

#include "sofia-sip/msg.h"
#include "sofia-sip/bnf.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/msg_header.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

/**
 * Parse a generic header.
 *
 * The function msg_generic_d() parses a generic header structure.
 *
 * @param[in]     home memory home
 * @param[in,out] h    header structure
 * @param[in]     s    string to be parsed
 * @param[in]     slen length of the string
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
issize_t msg_generic_d(su_home_t *home,
		       msg_header_t *h,
		       char *s,
		       isize_t slen)
{
  msg_generic_t *g = (msg_generic_t *)h;
  g->g_string = s;
  return 0;
}

/**
 * Encode a generic header.
 *
 * The function @c msg_generic_e encodes a generic header.
 *
 */
issize_t msg_generic_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  msg_generic_t const *g = (msg_generic_t const *)h;
  size_t n = strlen(g->g_string);

  if (bsiz > n)
    strcpy(b, g->g_string);

  return (issize_t)n;
}

/** Calculate the size of strings associated with a @c msg_generic_t object. */
isize_t msg_generic_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_generic_t const *g = (msg_generic_t const *)h;
  return offset + MSG_STRING_SIZE(g->g_string);
}

/** Duplicate one @c msg_generic_t object. */
char *msg_generic_dup_one(msg_header_t *dst,
			  msg_header_t const *src,
			  char *b,
			  isize_t xtra)
{
  msg_generic_t *g = (msg_generic_t *)dst;
  msg_generic_t const *o = (msg_generic_t const *)src;
  char *end = b + xtra;
  MSG_STRING_DUP(b, g->g_string, o->g_string);
  assert(b <= end); (void)end;
  return b;
}

issize_t msg_numeric_d(su_home_t *home,
		      msg_header_t *h,
		      char *s,
		      isize_t slen)
{
  msg_numeric_t *x = (msg_numeric_t *)h;
  uint32_t value = 0;
  issize_t retval = msg_uint32_d(&s, &value);

  assert(h->sh_class->hc_size >= sizeof *x);

  x->x_value = value;

  if (*s)
    return -1;

  return retval;
}

issize_t msg_numeric_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  msg_numeric_t *x = (msg_numeric_t *)h;

  assert(x->x_common->h_class->hc_size >= sizeof *x);

  if (x->x_value > 0xffffffffU)
    return -1;

  return snprintf(b, bsiz, "%lu", x->x_value);
}

/* ====================================================================== */
/* Comma-separated list */

/** @typedef struct msg_list_s msg_list_t;
 *
 * Type for token list headers.
 *
 */

issize_t msg_list_d(su_home_t *home, msg_header_t *h, char *s, isize_t slen)
{
  msg_list_t *k = (msg_list_t *)h;
  return msg_commalist_d(home, &s, &k->k_items, NULL);
}

issize_t msg_list_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  msg_list_t *k = (msg_list_t *)h;
  int compact = MSG_IS_COMPACT(flags);
  char *b0 = b, *end = b + bsiz;

  MSG_COMMALIST_E(b, end, k->k_items, compact);
  MSG_TERM_E(b, end);

  return b - b0;
}

/**@internal
 * Extra size of a msg_auth_t object.
 *
 * This function calculates extra size required by a msg_auth_t object.
 *
 * @param a pointer to a msg_auth_t object
 *
 * @return
 *   Size of strings related to msg_auth_t object.
 */
isize_t msg_list_dup_xtra(msg_header_t const *h, isize_t offset)
{
  msg_list_t const *k = (msg_list_t const *)h;
  MSG_PARAMS_SIZE(offset, k->k_items);
  return offset;
}

char *msg_list_dup_one(msg_header_t *dst,
		       msg_header_t const *src,
		       char *b,
		       isize_t xtra)
{
  msg_list_t *k = (msg_list_t *)dst;
  msg_list_t const *o = (msg_list_t const *)src;
  char *end = b + xtra;
  msg_param_t const ** items = (msg_param_t const **)&k->k_items;

  b = msg_params_dup(items, o->k_items, b, xtra);

  assert(b <= end); (void)end;

  return b;
}

/** Append a list of constant items to a list.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int msg_list_append_items(su_home_t *home,
			  msg_list_t *k,
			  msg_param_t const items[])
{
  size_t i;

  if (k == NULL) return -1;
  if (items == NULL) return 0;

  for (i = 0; items[i]; i++) {
    if (msg_header_add_param(home, (msg_common_t *)k, items[i]) < 0)
      return -1;
  }

  return 0;
}

/** Replace a list of constant items.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int msg_list_replace_items(su_home_t *home,
			   msg_list_t *k,
			   msg_param_t const items[])
{
  size_t i;

  if (k == NULL) return -1;
  if (items == NULL) return 0;

  for (i = 0; items[i]; i++) {
    if (msg_header_replace_item(home, (msg_common_t *)k, items[i]) < 0)
      return -1;
  }

  return 0;
}

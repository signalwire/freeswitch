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

/**@CFILE url_tag.c  URL Tag classes
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Feb 21 10:15:20 2001 ppessi
 */

#include "config.h"

#define TAG_NAMESPACE "url"

#include <sofia-sip/su.h>

#include <sofia-sip/url_tag.h>
#include <sofia-sip/url_tag_class.h>
#include <sofia-sip/su_tag_class.h>

#include <sofia-sip/url.h>

#include <string.h>

tag_typedef_t urltag_any = NSTAG_TYPEDEF(*);

tag_typedef_t urltag_url = URLTAG_TYPEDEF(url);

int urltag_snprintf(tagi_t const *t, char b[], size_t size)
{
  url_string_t const *us = (url_string_t const *)t->t_value;

  if (us == NULL)
    return snprintf(b, size, "<null>");
  if (URL_STRING_P(us))
    return snprintf(b, size, "<%s>", us->us_str);
  else
    return snprintf(b, size, "<" URL_PRINT_FORMAT ">",
		    URL_PRINT_ARGS(us->us_url));
}

size_t urltag_xtra(tagi_t const *t, size_t offset)
{
  url_t const *url = (url_t const *)t->t_value;

  if (url == NULL || url == (url_t *)-1)
    return 0;
  else if (URL_STRING_P(url))
    return t_str_xtra(t, offset);
  else
    return SU_ALIGN(offset) + sizeof(*url) + url_xtra(url);
}

tagi_t *urltag_dup(tagi_t *dst, tagi_t const *src, void **bb)
{
  url_t const *url = (url_t const *)src->t_value;

  if (url == NULL || url == (url_t *)-1) {
    dst->t_tag = src->t_tag;
    dst->t_value = src->t_value;
  }
  else if (URL_STRING_P(url)) {
    return t_str_dup(dst, src, bb);
  } else {
    size_t xtra = url_xtra(url);
    char *b = *bb;
    url_t *d;

    b += SU_ALIGN(b);
    d = (url_t *)b;
    url_dup(b + sizeof(*d), xtra, d, url);

    dst->t_tag = src->t_tag;
    dst->t_value = (tag_value_t)d;
    *bb = b + sizeof(*d) + xtra;
  }

  return dst + 1;
}

#define IS_EXCLUDED(u)						\
  (u <= ' '							\
   || u >= '\177'						\
   || (u < 64 ? (0xb400000aU  & (1 << (63 - u)))			\
       : (u < 96 ? (0x0000001eU & (1 << (95 - u)))		\
	  : /*u < 128*/ (0x8000001dU & (1 << (127 - u))))) != 0)

/** Tag function used to convert a string to a URL tag value.
 *
 * @param tt tag type
 * @param home memory home used to allocate new #url_t structure
 * @param str string to convert
 * @param return_value return-value parameter for converted url
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @since New in @VERSION_1_12_2.
 */
int urltag_scan(tag_type_t tt, su_home_t *home,
		char const *str,
		tag_value_t *return_value)
{
  size_t len;
  url_t *url;
  char *s;

  (void)tt;

  for (len = 0; !IS_EXCLUDED(str[len]); len++)
    ;

  url = su_alloc(home, (sizeof *url) + len + 1);
  if (!url)
    return -1;
  s = memcpy((char *)(url + 1), str, len);
  s[len] = 0;

  if (url_d(url, s) < 0)
    return (void)su_free(home, url), -1;

  *return_value = (tag_value_t)url;

  return 0;
}

tag_class_t url_tag_class[1] =
  {{
    sizeof(url_tag_class),
    /* tc_next */     NULL,
    /* tc_len */      NULL,
    /* tc_move */     NULL,
    /* tc_xtra */     urltag_xtra,
    /* tc_dup */      urltag_dup,
    /* tc_free */     NULL,
    /* tc_find */     NULL,
    /* tc_snprintf */ urltag_snprintf,
    /* tc_filter */   NULL,
    /* tc_ref_set */  t_ptr_ref_set,
    /* tc_scan */     urltag_scan,
  }};


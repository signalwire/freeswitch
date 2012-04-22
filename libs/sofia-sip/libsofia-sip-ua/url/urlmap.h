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

#ifndef URLMAP_H
/** Defined when <urlmap.h> has been included. */
#define URLMAP_H

/**
 * @file urlmap.h
 * @brief
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed Mar 10 17:06:20 2004 ppessi
 */

#ifndef URL_H
#include <sofia-sip/url.h>
#endif

SOFIA_BEGIN_DECLS

/** Mapping of URLs */
typedef struct _UrlMap UrlMap;

struct _UrlMap {
  UrlMap        *um_left, *um_right, *um_dad;
  unsigned       um_black:1;
  unsigned       um_inserted:1;
  unsigned       :0;
  url_t          um_url[1];
};

UrlMap *url_map_new(su_home_t *home, url_string_t const *url, unsigned size);

int url_map_insert(UrlMap **tree, UrlMap * um, UrlMap **return_old);
void url_map_remove(UrlMap **tree, UrlMap *ume);

UrlMap *url_map_find(UrlMap *tree, url_string_t const *u, int relative);


SOFIA_END_DECLS
#endif /* !defined URLMAP_H */


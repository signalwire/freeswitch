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

/**@ingroup sl_utils
 * @CFILE sl_read_payload.c
 *
 * @brief Functions for reading SIP message payload from a file.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Sep  5 00:44:34 2002 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sofia-sip/sip_header.h>

#include <sofia-sip/sl_utils.h>

/** Read payload from named file.
 *
 * The function sl_read_payload() reads the contents to a SIP payload
 * structure from a the named file. If @a fname is NULL, the payload
 * contents are read from standard input.
 */
sip_payload_t *sl_read_payload(su_home_t *home, char const *fname)
{
  FILE *f;
  sip_payload_t *pl;

  if (fname == NULL || strcmp(fname, "-") == 0)
    f = stdin, fname = "<stdin>";
  else
    f = fopen(fname, "rb");

  if (f == NULL)
    return NULL;

  pl = sl_fread_payload(home, f);
  if (f != stdin)
    fclose(f);

  return pl;
}

sip_payload_t *sl_fread_payload(su_home_t *home, FILE *f)
{
  sip_payload_t *pl;
  size_t n;
  char *buf;
  char const *who;
  size_t used, size;

  if (f == NULL) {
    errno = EINVAL;
    return NULL;
  }

  pl = sip_payload_create(home, NULL, 0);

  if (pl == NULL)
    return NULL;

  /* Read block by block */
  used = 0;
  size = 4096;
  buf = malloc(size);
  who = "sl_fread_payload: malloc";

  while (buf) {
    n = fread(buf + used, 1, size - used, f);
    used += n;
    if (n < size - used) {
      if (feof(f))
	;
      else if (ferror(f)) {
	free(buf); buf = NULL;
	who = "sl_fread_payload: fread";
      }
      break;
    }
    buf = realloc(buf, size = 2 * size);
    if (buf == NULL)
      who = "sl_fread_payload: realloc";
  }

  if (buf == NULL) {
    perror(who);
    su_free(home, pl);
    return NULL;
  }

  if (used < size)
    buf[used] = '\0';

  pl->pl_common->h_data = pl->pl_data = buf;
  pl->pl_common->h_len = pl->pl_len = used;

  return pl;
}

/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 200 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: maildriver_types_helper.c,v 1.5 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "maildriver_types_helper.h"

#include "mail.h"

#include "clist.h"
#include <string.h>
#include <stdlib.h>

int mail_flags_add_extension(struct mail_flags * flags,
			     char * ext_flag)
{
  char * str;
  int r;

  if (mail_flags_has_extension(flags, ext_flag))
    return MAIL_NO_ERROR;

  str = strdup(ext_flag);
  if (str == NULL)
    return MAIL_ERROR_MEMORY;
  
  r = clist_append(flags->fl_extension, str);
  if (r < 0) {
    free(str);
    return MAIL_ERROR_MEMORY;
  }

  return MAIL_NO_ERROR;
}

int mail_flags_remove_extension(struct mail_flags * flags,
				char * ext_flag)
{
  clistiter * cur;
  
  cur = clist_begin(flags->fl_extension);
  while (cur != NULL) {
    char * flag_name;

    flag_name = clist_content(cur);

    if (strcasecmp(flag_name, ext_flag) == 0) {
      free(flag_name);
      cur = clist_delete(flags->fl_extension, cur);
    }
    else
      cur = clist_next(cur);
  }

  return MAIL_NO_ERROR;
}

int mail_flags_has_extension(struct mail_flags * flags,
			     char * ext_flag)
{
  clistiter * cur;

  for(cur = clist_begin(flags->fl_extension) ; cur != NULL ;
      cur = clist_next(cur)) {
    char * flag_name;

    flag_name = clist_content(cur);

    if (strcasecmp(flag_name, ext_flag) == 0)
      return TRUE;
  }

  return FALSE;
}

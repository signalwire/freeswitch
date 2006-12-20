/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_sender.h"
#include "annotatemore_types.h"

static int annotatemore_string_send(mailstream * fd, const char * str)
{
  return mailimap_quoted_send(fd, str);
}

static int
annotatemore_entry_match_list_send(mailstream * fd,
    struct mailimap_annotatemore_entry_match_list * em_list)
{
  return mailimap_struct_spaced_list_send(fd, em_list->entry_match_list,
      (mailimap_struct_sender *) annotatemore_string_send);
}

static int
annotatemore_attrib_match_list_send(mailstream * fd,
    struct mailimap_annotatemore_attrib_match_list * am_list)
{
  /* TODO actually attrib-match is defined as string, not astring */
  return mailimap_struct_spaced_list_send(fd, am_list->attrib_match_list,
      (mailimap_struct_sender *) annotatemore_string_send);
}

int mailimap_annotatemore_getannotation_send(mailstream * fd,
        const char * list_mb,
        struct mailimap_annotatemore_entry_match_list * entries,
        struct mailimap_annotatemore_attrib_match_list * attribs)
{
  int r;

  r = mailimap_token_send(fd, "GETANNOTATION");
  if (r != MAILIMAP_NO_ERROR)
	  return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_list_mailbox_send(fd, list_mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '(');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = annotatemore_entry_match_list_send(fd, entries);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, ')');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '(');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = annotatemore_attrib_match_list_send(fd, attribs);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, ')');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

static int
annotatemore_att_value_send(mailstream * fd,
        struct mailimap_annotatemore_att_value * att_value)
{
  int r;

  r = annotatemore_string_send(fd, att_value->attrib);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = annotatemore_string_send(fd, att_value->value);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

static int
annotatemore_entry_att_send(mailstream * fd,
        struct mailimap_annotatemore_entry_att * en_att)
{
  int r;

  r = annotatemore_string_send(fd, en_att->entry);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, '(');
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_struct_spaced_list_send(fd, en_att->att_value_list,
        (mailimap_struct_sender *) annotatemore_att_value_send);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_char_send(fd, ')');
  if (r != MAILIMAP_NO_ERROR)
    return r;
          
  return MAILIMAP_NO_ERROR;
}

static int
annotatemore_entry_att_list_send(mailstream * fd,
        struct mailimap_annotatemore_entry_att_list * en_list)
{
  return mailimap_struct_spaced_list_send(fd, en_list->entry_att_list,
      (mailimap_struct_sender *) annotatemore_entry_att_send);
}

int mailimap_annotatemore_setannotation_send(mailstream * fd,
        const char * list_mb,
        struct mailimap_annotatemore_entry_att_list * en_list)
{
  int r;

  r = mailimap_token_send(fd, "SETANNOTATION");
  if (r != MAILIMAP_NO_ERROR)
	  return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_list_mailbox_send(fd, list_mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (en_list->entry_att_list->count > 1) {
    r = mailimap_char_send(fd, '(');
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }

  r = annotatemore_entry_att_list_send(fd, en_list);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (en_list->entry_att_list->count > 1) {
    r = mailimap_char_send(fd, ')');
    if (r != MAILIMAP_NO_ERROR)
      return r;
  }

  return MAILIMAP_NO_ERROR;
}

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

#include "mailimap_types.h"
#include "annotatemore_types.h"
#include "mailimap_extension.h"

#include <stdlib.h>
#include <string.h>

void mailimap_annotatemore_attrib_free(char * attrib)
{
 mailimap_string_free(attrib);
}

void mailimap_annotatemore_value_free(char * value)
{
 mailimap_nstring_free(value);
}

void mailimap_annotatemore_entry_free(char * entry)
{
 mailimap_string_free(entry);
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_att_value *
mailimap_annotatemore_att_value_new(char * attrib, char * value)
{
  struct mailimap_annotatemore_att_value * att_value;

  att_value = malloc(sizeof(* att_value));
  if (att_value == NULL)
    return NULL;

  att_value->attrib = attrib;
  att_value->value = value;

  return att_value;
}

void mailimap_annotatemore_att_value_free(struct
        mailimap_annotatemore_att_value * att_value)
{
  mailimap_annotatemore_attrib_free(att_value->attrib);
  mailimap_annotatemore_value_free(att_value->value);

  free(att_value);
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att *
mailimap_annotatemore_entry_att_new(char * entry, clist * list)
{
  struct mailimap_annotatemore_entry_att * entry_att;

  entry_att = malloc(sizeof(* entry_att));
  if (entry_att == NULL)
    return NULL;

  entry_att->entry = entry;
  entry_att->att_value_list = list;

  return entry_att;
}

LIBETPAN_EXPORT
void mailimap_annotatemore_entry_att_free(struct
        mailimap_annotatemore_entry_att * en_att)
{
  mailimap_annotatemore_entry_free(en_att->entry);
  clist_foreach(en_att->att_value_list,
      (clist_func) mailimap_annotatemore_att_value_free, NULL);
  clist_free(en_att->att_value_list);
  free(en_att);
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att *
mailimap_annotatemore_entry_att_new_empty(char * entry)
{
  struct mailimap_annotatemore_entry_att * entry_att;
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  entry_att = mailimap_annotatemore_entry_att_new(entry, list);
  if (entry_att == NULL)
    return NULL;

  return entry_att;
}

LIBETPAN_EXPORT
int mailimap_annotatemore_entry_att_add(struct
        mailimap_annotatemore_entry_att * en_att,
        struct mailimap_annotatemore_att_value * at_value)
{
  int r;

  if (en_att->att_value_list == NULL) {
    /* catch this error by creating a new clist */
    en_att->att_value_list = clist_new();
    if (en_att->att_value_list == NULL)
      return MAILIMAP_ERROR_MEMORY;
  }

  r = clist_append(en_att->att_value_list, at_value);
  if (r < 0) {
    return MAILIMAP_ERROR_MEMORY;
  }

  return MAILIMAP_NO_ERROR;
}

struct mailimap_annotatemore_entry_list *
mailimap_annotatemore_entry_list_new(int type, clist * en_att_list, clist * en_list)
{
  struct mailimap_annotatemore_entry_list * entry_list;

  entry_list = malloc(sizeof(* entry_list));
  if (entry_list == NULL)
    return NULL;

  entry_list->en_list_type = type;
  switch (type) {
  case MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_ATT_LIST:
    entry_list->en_list_data = en_att_list;
    break;
  case MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_LIST:
    entry_list->en_list_data = en_list;
    break;
  }

  return entry_list;
}

void mailimap_annotatemore_entry_list_free(struct
        mailimap_annotatemore_entry_list * en_list)
{
  switch(en_list->en_list_type) {
  case MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_ATT_LIST:
    clist_foreach(en_list->en_list_data,
      (clist_func) mailimap_annotatemore_entry_att_free, NULL);
    break;
  case MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_LIST:
    clist_foreach(en_list->en_list_data,
      (clist_func) mailimap_annotatemore_entry_free, NULL);
    break;
  }
  clist_free(en_list->en_list_data);
  free(en_list);
}

struct mailimap_annotatemore_annotate_data *
mailimap_annotatemore_annotate_data_new(char * mb, struct
        mailimap_annotatemore_entry_list * en_list)
{
  struct mailimap_annotatemore_annotate_data * annotate_data;

  annotate_data = malloc(sizeof(* annotate_data));
  if (annotate_data == NULL)
    return NULL;

  annotate_data->mailbox = mb;
  annotate_data->entry_list = en_list;

  return annotate_data;
}

LIBETPAN_EXPORT
void mailimap_annotatemore_annotate_data_free(struct
        mailimap_annotatemore_annotate_data * an_data)
{
  mailimap_mailbox_free(an_data->mailbox);
  mailimap_annotatemore_entry_list_free(an_data->entry_list);
  free(an_data);
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_match_list *
mailimap_annotatemore_entry_match_list_new(clist * en_list)
{
  struct mailimap_annotatemore_entry_match_list * entry_match_list;

  entry_match_list = malloc(sizeof(* entry_match_list));
  if (entry_match_list == NULL)
    return NULL;
  entry_match_list->entry_match_list = en_list;

  return entry_match_list;
}

LIBETPAN_EXPORT
void mailimap_annotatemore_entry_match_list_free(
        struct mailimap_annotatemore_entry_match_list * en_list)
{
  clist_foreach(en_list->entry_match_list, (clist_func) free, NULL);
  clist_free(en_list->entry_match_list);
  free(en_list);
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_attrib_match_list *
mailimap_annotatemore_attrib_match_list_new(clist * at_list)
{
  struct mailimap_annotatemore_attrib_match_list * attrib_match_list;

  attrib_match_list = malloc(sizeof(* attrib_match_list));
  if (attrib_match_list == NULL)
    return NULL;
  attrib_match_list->attrib_match_list = at_list;

  return attrib_match_list;
}

LIBETPAN_EXPORT
void mailimap_annotatemore_attrib_match_list_free(
        struct mailimap_annotatemore_attrib_match_list * at_list)
{
  clist_foreach(at_list->attrib_match_list, (clist_func) free, NULL);
  clist_free(at_list->attrib_match_list);
  free(at_list);
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_match_list *
mailimap_annotatemore_entry_match_list_new_empty()
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_annotatemore_entry_match_list_new(list);
}

LIBETPAN_EXPORT
int mailimap_annotatemore_entry_match_list_add(
  struct mailimap_annotatemore_entry_match_list * en_list, char * entry)
{
  char * pentry;
  int r;

  pentry = strdup(entry);
  if (pentry == NULL)
    return MAILIMAP_ERROR_MEMORY;

  r = clist_append(en_list->entry_match_list, pentry);
  if (r < 0) {
    free(pentry);
    return MAILIMAP_ERROR_MEMORY;
  }

  return MAILIMAP_NO_ERROR;
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_attrib_match_list *
mailimap_annotatemore_attrib_match_list_new_empty()
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_annotatemore_attrib_match_list_new(list);
}

LIBETPAN_EXPORT
int mailimap_annotatemore_attrib_match_list_add(
  struct mailimap_annotatemore_attrib_match_list * at_list, char * attrib)
{
  char * pattrib;
  int r;

  pattrib = strdup(attrib);
  if (pattrib == NULL)
    return MAILIMAP_ERROR_MEMORY;

  r = clist_append(at_list->attrib_match_list, pattrib);
  if (r < 0) {
    free(pattrib);
    return MAILIMAP_ERROR_MEMORY;
  }

  return MAILIMAP_NO_ERROR;
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att_list *
mailimap_annotatemore_entry_att_list_new(clist * en_list)
{
  struct mailimap_annotatemore_entry_att_list * entry_att_list;

  entry_att_list = malloc(sizeof(* entry_att_list));
  if (entry_att_list == NULL)
    return NULL;
  entry_att_list->entry_att_list = en_list;

  return entry_att_list;
}

LIBETPAN_EXPORT
void mailimap_annotatemore_entry_att_list_free(
      struct mailimap_annotatemore_entry_att_list * en_list)
{
  clist_foreach(en_list->entry_att_list,
      (clist_func) mailimap_annotatemore_entry_att_free, NULL);
  clist_free(en_list->entry_att_list);
  free(en_list);
}

LIBETPAN_EXPORT
struct mailimap_annotatemore_entry_att_list *
mailimap_annotatemore_entry_att_list_new_empty()
{
  clist * list;

  list = clist_new();
  if (list == NULL)
    return NULL;

  return mailimap_annotatemore_entry_att_list_new(list);
}

LIBETPAN_EXPORT
int mailimap_annotatemore_entry_att_list_add(
      struct mailimap_annotatemore_entry_att_list * en_list,
      struct mailimap_annotatemore_entry_att * en_att)
{
  int r;
  
  r = clist_append(en_list->entry_att_list, en_att);
  if (r < 0)
    return MAILIMAP_ERROR_MEMORY;
  
  return MAILIMAP_NO_ERROR;
}

void
mailimap_annotatemore_free(struct mailimap_extension_data * ext_data)
{
  if (ext_data == NULL)
    return;

  switch (ext_data->ext_type)
  {
    case MAILIMAP_ANNOTATEMORE_TYPE_ANNOTATE_DATA:
      mailimap_annotatemore_annotate_data_free(ext_data->ext_data);
      break;
    case MAILIMAP_ANNOTATEMORE_TYPE_RESP_TEXT_CODE:
      /* nothing malloced for resp_text_code */
      break;
  }

  free (ext_data);
}


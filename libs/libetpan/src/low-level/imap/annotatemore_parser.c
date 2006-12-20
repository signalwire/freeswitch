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

#include "annotatemore_parser.h"
#include "mailimap_keywords.h"
#include "mailimap_extension.h"
#include "annotatemore.h"

#include <stdlib.h>

int
mailimap_annotatemore_annotate_data_parse(mailstream * fd, MMAPString *buffer,
    size_t * index, struct mailimap_annotatemore_annotate_data ** result,
    size_t progr_rate, progress_function * progr_fun)
{
  size_t cur_token;
  char * mailbox;
  struct mailimap_annotatemore_entry_list * entry_list;
  struct mailimap_annotatemore_annotate_data * annotate_data;
  int r;
  int res;

  cur_token = * index;

  mailbox = NULL; /* XXX - removes a gcc warning */

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "ANNOTATION");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_mailbox_parse(fd, buffer, &cur_token, &mailbox,
          progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto mailbox_free;
  }

  r = mailimap_annotatemore_entry_list_parse(fd, buffer, &cur_token,
          &entry_list, progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto mailbox_free;
  }

  annotate_data = mailimap_annotatemore_annotate_data_new(mailbox,
      entry_list);
  if (annotate_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto entry_list_free;
  }

  * result = annotate_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 entry_list_free:
  mailimap_annotatemore_entry_list_free(entry_list);
 mailbox_free:
  mailimap_mailbox_free(mailbox);
 err:
  return res;
}

int
mailimap_annotatemore_entry_list_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_annotatemore_entry_list ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  clist * en_att_list;
  clist * en_list;
  int type;
  struct mailimap_annotatemore_entry_list * entry_list;
  int r;
  int res;

  cur_token = * index;

  /* XXX - removes a gcc warning */
  type = MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ERROR;
  en_list = NULL;
  en_att_list = NULL;

  r = mailimap_struct_spaced_list_parse(fd, buffer,
        &cur_token, &en_att_list,
        (mailimap_struct_parser * )
        mailimap_annotatemore_entry_att_parse,
        (mailimap_struct_destructor * )
        mailimap_annotatemore_entry_att_free,
        progr_rate, progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    type = MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_ATT_LIST;

  if (r == MAILIMAP_ERROR_PARSE) {
    r = mailimap_oparenth_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }

    r = mailimap_struct_spaced_list_parse(fd, buffer,
          &cur_token, &en_list,
          (mailimap_struct_parser * )
          mailimap_annotatemore_entry_parse,
          (mailimap_struct_destructor * )
          mailimap_annotatemore_entry_free,
          progr_rate, progr_fun);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }

    r = mailimap_cparenth_parse(fd, buffer, &cur_token);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto en_list_free;
    }

    type = MAILIMAP_ANNOTATEMORE_ENTRY_LIST_TYPE_ENTRY_LIST;
  }

  entry_list = mailimap_annotatemore_entry_list_new(type, en_att_list, en_list);
  if (entry_list == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto list_free;
  }

  * result = entry_list;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 list_free:
  if (en_att_list != NULL) {
    clist_foreach(en_att_list,
      (clist_func) mailimap_annotatemore_entry_att_free, NULL);
    clist_free(en_att_list);
  }
 en_list_free:
  if (en_list != NULL) {
    clist_foreach(en_list,
      (clist_func) mailimap_annotatemore_entry_free, NULL);
    clist_free(en_list);
  }
 err:
  return res;
}

int
mailimap_annotatemore_entry_att_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_annotatemore_entry_att ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  char * entry;
  clist * list;
  struct mailimap_annotatemore_entry_att * entry_att;
  int r;
  int res;
  
  cur_token = * index;
  entry = NULL;

  r = mailimap_annotatemore_entry_parse(fd, buffer, &cur_token, &entry,
      progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto entry_free;
  }

  r = mailimap_oparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto entry_free;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer,
        &cur_token, &list,
        (mailimap_struct_parser * )
        mailimap_annotatemore_att_value_parse,
        (mailimap_struct_destructor * )
        mailimap_annotatemore_att_value_free,
        progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto entry_free;
  }

  r = mailimap_cparenth_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto list_free;
  }

  entry_att = mailimap_annotatemore_entry_att_new(entry, list);
  if (entry_att == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto list_free;
  }

  * result = entry_att;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;
  
 list_free:
  clist_foreach(list,
    (clist_func) mailimap_annotatemore_att_value_free, NULL);
  clist_free(list);
 entry_free:
  mailimap_annotatemore_entry_free(entry);
 err:
  return res;
}

int
mailimap_annotatemore_att_value_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_annotatemore_att_value ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  char * attrib;
  char * value;
  struct mailimap_annotatemore_att_value * att_value;
  int r;
  int res;

  cur_token = * index;
  attrib = NULL;
  value = NULL;

  r = mailimap_annotatemore_attrib_parse(fd, buffer, &cur_token, &attrib,
            progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto attrib_free;
  }

  r = mailimap_annotatemore_value_parse(fd, buffer, &cur_token, &value,
            progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto attrib_free;
  }

  att_value = mailimap_annotatemore_att_value_new(attrib, value);
  if (att_value == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto value_free;
  }

  * result = att_value;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 value_free:
  mailimap_annotatemore_value_free(value);
 attrib_free:
  mailimap_annotatemore_attrib_free(attrib);
 err:
  return res;
}

int
mailimap_annotatemore_attrib_parse(mailstream * fd, MMAPString *buffer,
              size_t * index, char ** result,
              size_t progr_rate, progress_function * progr_fun)
{
  return mailimap_string_parse(fd, buffer, index, result, NULL,
    progr_rate, progr_fun);
}

int
mailimap_annotatemore_value_parse(mailstream * fd, MMAPString *buffer,
              size_t * index, char ** result,
		          size_t progr_rate, progress_function * progr_fun)
{
  return mailimap_nstring_parse(fd, buffer, index, result, NULL,
    progr_rate, progr_fun);
}

int
mailimap_annotatemore_entry_parse(mailstream * fd, MMAPString *buffer,
              size_t * index, char ** result,
              size_t progr_rate, progress_function * progr_fun)
{
  return mailimap_string_parse(fd, buffer, index, result, NULL,
    progr_rate, progr_fun);
}

int
mailimap_annotatemore_text_code_annotatemore_parse(mailstream * fd,
              MMAPString *buffer, size_t * index, int * result,
              size_t progr_rate, progress_function * progr_fun)
{
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "ANNOTATEMORE");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "TOOBIG");
  if (r == MAILIMAP_NO_ERROR) {
    * result = MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_TOOBIG;
  } else {
    r = mailimap_token_case_insensitive_parse(fd, buffer,
                &cur_token, "TOOMANY");
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto err;
    }

    * result = MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_TOOMANY;
  }

  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 err:
  return res;
}


/*
  this is the extension's initial parser. it switches on calling_parser
  and calls the corresponding actual parser. annotatemore extends
  imap as follows:
       response-data     /= "*" SP annotate-data CRLF
                           ; adds to original IMAP data responses

       resp-text-code    =/ "ANNOTATEMORE" SP "TOOBIG" /
                            "ANNOTATEMORE" SP "TOOMANY"
                           ; new response codes for SETANNOTATION failures
*/
int mailimap_annotatemore_parse(int calling_parser, mailstream * fd,
    MMAPString * buffer, size_t * index,
    struct mailimap_extension_data ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  int r;

  struct mailimap_annotatemore_annotate_data * an_data;
  int resp_text_code;

  switch (calling_parser)
  {
    case MAILIMAP_EXTENDED_PARSER_RESPONSE_DATA:
      r = mailimap_annotatemore_annotate_data_parse(fd, buffer, index,
        &an_data, progr_rate, progr_fun);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      * result = mailimap_extension_data_new(&mailimap_extension_annotatemore,
                MAILIMAP_ANNOTATEMORE_TYPE_ANNOTATE_DATA, an_data);
      if (result == NULL) {
        mailimap_annotatemore_annotate_data_free(an_data);
        return MAILIMAP_ERROR_MEMORY;
      }
      break;
    case MAILIMAP_EXTENDED_PARSER_RESP_TEXT_CODE:
      r = mailimap_annotatemore_text_code_annotatemore_parse(fd, buffer, index,
        &resp_text_code, progr_rate, progr_fun);
      if (r != MAILIMAP_NO_ERROR)
        return r;
      * result = mailimap_extension_data_new(&mailimap_extension_annotatemore,
        MAILIMAP_ANNOTATEMORE_TYPE_RESP_TEXT_CODE, &resp_text_code);
      if (result == NULL)
        return MAILIMAP_ERROR_MEMORY;
      break;
    default:
      /* return a MAILIMAP_ERROR_PARSE if the extension
         doesn't extend calling_parser. */
      return MAILIMAP_ERROR_PARSE;
      break;
  }

  return MAILIMAP_NO_ERROR;
}


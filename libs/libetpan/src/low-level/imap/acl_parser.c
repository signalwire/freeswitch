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

#include "acl_parser.h"
#include "mailimap_keywords.h"
#include "mailimap_extension.h"
#include "acl.h"

#include <stdlib.h>

int
mailimap_acl_acl_data_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_acl_acl_data ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  char * mailbox;
  clist * ir_list;
  struct mailimap_acl_acl_data * acl_data;
  int r;
  int res;

  cur_token = * index;

  mailbox = NULL; /* XXX - removes a gcc warning */

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "ACL");
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

  r = mailimap_struct_spaced_list_parse(fd, buffer,
        &cur_token, &ir_list,
        (mailimap_struct_parser * )
        mailimap_acl_identifier_rights_parse,
        (mailimap_struct_destructor * )
        mailimap_acl_identifier_rights_free,
        progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto mailbox_free;
  }

  acl_data = mailimap_acl_acl_data_new(mailbox,
      ir_list);
  if (acl_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto ir_list_free;
  }

  * result = acl_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 ir_list_free:
  if (ir_list != NULL) {
    clist_foreach(ir_list,
      (clist_func) mailimap_acl_identifier_rights_free, NULL);
    clist_free(ir_list);
  }
 mailbox_free:
  mailimap_mailbox_free(mailbox);
 err:
  return res;
}

int
mailimap_acl_listrights_data_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_acl_listrights_data ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  char * mailbox;
  char * identifier;
  clist * rights_list;
  struct mailimap_acl_listrights_data * lr_data;
  int r;
  int res;

  cur_token = * index;

  mailbox = NULL; /* XXX - removes a gcc warning */
  identifier = NULL;

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "LISTRIGHTS");
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

  r = mailimap_acl_identifier_parse(fd, buffer, &cur_token, &identifier,
          progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto mailbox_free;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto identifier_free;
  }

  r = mailimap_struct_spaced_list_parse(fd, buffer,
        &cur_token, &rights_list,
        (mailimap_struct_parser * )
        mailimap_acl_rights_parse,
        (mailimap_struct_destructor * )
        mailimap_acl_rights_free,
        progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto identifier_free;
  }

  lr_data = mailimap_acl_listrights_data_new(mailbox, identifier,
      rights_list);
  if (lr_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto rights_list_free;
  }

  * result = lr_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 rights_list_free:
  if (rights_list != NULL) {
    clist_foreach(rights_list,
      (clist_func) mailimap_acl_rights_free, NULL);
    clist_free(rights_list);
  }
 identifier_free:
  mailimap_acl_identifier_free(identifier);
 mailbox_free:
  mailimap_mailbox_free(mailbox);
 err:
  return res;
}

int
mailimap_acl_myrights_data_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_acl_myrights_data ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  char * mailbox;
  char * rights;
  struct mailimap_acl_myrights_data * mr_data;
  int r;
  int res;

  cur_token = * index;

  mailbox = NULL; /* XXX - removes a gcc warning */
  rights = NULL;

  r = mailimap_token_case_insensitive_parse(fd, buffer,
					    &cur_token, "MYRIGHTS");
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

  r = mailimap_acl_rights_parse(fd, buffer, &cur_token, &rights,
          progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto mailbox_free;
  }

  mr_data = mailimap_acl_myrights_data_new(mailbox, rights);
  if (mr_data == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto rights_free;
  }

  * result = mr_data;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 rights_free:
  mailimap_acl_rights_free(rights);
 mailbox_free:
  mailimap_mailbox_free(mailbox);
 err:
  return res;
}

int
mailimap_acl_identifier_rights_parse(mailstream * fd,
    MMAPString *buffer, size_t * index,
    struct mailimap_acl_identifier_rights ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  size_t cur_token;
  char * identifier;
  char * rights;
  struct mailimap_acl_identifier_rights * id_rights;
  int r;
  int res;

  cur_token = * index;

  identifier = NULL; /* XXX - removes a gcc warning */
  rights = NULL;

  r = mailimap_acl_identifier_parse(fd, buffer, &cur_token, &identifier,
          progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto identifier_free;
  }

  r = mailimap_acl_rights_parse(fd, buffer, &cur_token, &rights,
          progr_rate, progr_fun);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto identifier_free;
  }

  id_rights = mailimap_acl_identifier_rights_new(identifier, rights);
  if (id_rights == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto rights_free;
  }

  * result = id_rights;
  * index = cur_token;

  return MAILIMAP_NO_ERROR;

 rights_free:
  mailimap_acl_rights_free(rights);
 identifier_free:
  mailimap_acl_identifier_free(identifier);
 err:
  return res;
}

int
mailimap_acl_identifier_parse(mailstream * fd,
    MMAPString *buffer, size_t * index,
    char ** result, size_t progr_rate,
    progress_function * progr_fun)
{
  return mailimap_astring_parse(fd, buffer, index, result,
      progr_rate, progr_fun);
}

int mailimap_acl_rights_parse(mailstream * fd,
    MMAPString *buffer, size_t * index,
    char ** result, size_t progr_rate,
    progress_function * progr_fun)
{
  return mailimap_astring_parse(fd, buffer, index, result,
      progr_rate, progr_fun);
}

/*
  this is the extension's initial parser. it switches on calling_parser
  and calls the corresponding actual parser. acl extends
  imap as follows:
  mailbox-data    =/ acl-data / listrights-data / myrights-data
                      ;;mailbox-data is defined in [IMAP4]
  capability      =/ rights-capa
                      ;;capability is defined in [IMAP4]

  the extension to capability is omitted so far.
*/

int mailimap_acl_parse(int calling_parser, mailstream * fd,
    MMAPString * buffer, size_t * index,
    struct mailimap_extension_data ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  int r;
  int res;

  struct mailimap_acl_acl_data * acl_data;
  struct mailimap_acl_listrights_data * lr_data;
  struct mailimap_acl_myrights_data * mr_data;

  void * data;

  int type;

  switch (calling_parser)
  {
    case MAILIMAP_EXTENDED_PARSER_MAILBOX_DATA:
      r = mailimap_acl_acl_data_parse(fd, buffer, index,
        &acl_data, progr_rate, progr_fun);
      if (r == MAILIMAP_NO_ERROR) {
        type = MAILIMAP_ACL_TYPE_ACL_DATA;
        data = acl_data;
      }

      if (r == MAILIMAP_ERROR_PARSE) {
        r = mailimap_acl_listrights_data_parse(fd, buffer, index,
          &lr_data, progr_rate, progr_fun);
        if (r == MAILIMAP_NO_ERROR) {
          type = MAILIMAP_ACL_TYPE_LISTRIGHTS_DATA;
          data = lr_data;
        }
      }

      if (r == MAILIMAP_ERROR_PARSE) {
        r = mailimap_acl_myrights_data_parse(fd, buffer, index,
          &mr_data, progr_rate, progr_fun);
        if (r == MAILIMAP_NO_ERROR) {
          type = MAILIMAP_ACL_TYPE_MYRIGHTS_DATA;
          data = mr_data;
        }
      }

      if (r != MAILIMAP_NO_ERROR) {
        res = r;
        goto err;
      }

      * result = mailimap_extension_data_new(&mailimap_extension_acl,
                type, data);
      if (result == NULL) {
        res = MAILIMAP_ERROR_MEMORY;
        goto data_free;
      }
      break;
    default:
      /* return a MAILIMAP_ERROR_PARSE if the extension
         doesn't extend calling_parser. */
      return MAILIMAP_ERROR_PARSE;
      break;
  }

  return MAILIMAP_NO_ERROR;

 data_free:
  if (acl_data != NULL)
    mailimap_acl_acl_data_free(acl_data);
  if (lr_data != NULL)
    mailimap_acl_listrights_data_free(lr_data);
  if (mr_data != NULL)
    mailimap_acl_myrights_data_free(mr_data);
 err:
  return res;
}


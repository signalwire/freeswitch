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

#include "mailimap.h"
#include "mailimap_extension.h"
#include "acl.h"
#include "acl_types.h"
#include "acl_parser.h"
#include "acl_sender.h"

#include <stdlib.h>

LIBETPAN_EXPORT
struct mailimap_extension_api mailimap_extension_acl = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* name */          "ACL",
  /* extension_id */  MAILIMAP_EXTENSION_ACL,
  /* parser */        mailimap_acl_parse,
  /* free */          mailimap_acl_free,
#else
  .name             = "ACL",
  .extension_id     = MAILIMAP_EXTENSION_ACL,
  .parser           = mailimap_acl_parse,
  .free             = mailimap_acl_free,
#endif
};

LIBETPAN_EXPORT
int mailimap_acl_setacl(mailimap * session,
    const char * mailbox,
    const char * identifier,
    const char * mod_rights)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_AUTHENTICATED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_setacl_send(session->imap_stream,
    mailbox, identifier, mod_rights);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_EXTENSION;
  }
}

LIBETPAN_EXPORT
int mailimap_acl_deleteacl(mailimap * session,
    const char * mailbox,
    const char * identifier)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_AUTHENTICATED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_deleteacl_send(session->imap_stream,
    mailbox, identifier);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_EXTENSION;
  }
}

LIBETPAN_EXPORT
int mailimap_acl_getacl(mailimap * session,
    const char * mailbox,
    clist ** result)
{
  struct mailimap_response * response;
  struct mailimap_extension_data * ext_data;
  clistiter * cur;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_AUTHENTICATED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_getacl_send(session->imap_stream,
    mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = clist_new();
  if (* result == NULL)
    return MAILIMAP_ERROR_MEMORY;

  /* get all acl_data received and copy it to result */
  for (cur = clist_begin(session->imap_response_info->rsp_extension_list);
    cur != NULL; cur = clist_next(cur)) {
      ext_data = (struct mailimap_extension_data *) clist_content(cur);
      if (
        ext_data->ext_extension->ext_id == MAILIMAP_EXTENSION_ACL &&
        ext_data->ext_type == MAILIMAP_ACL_TYPE_ACL_DATA) {
          r = clist_append((* result), ext_data->ext_data);
          if (r != 0)
            return MAILIMAP_ERROR_MEMORY;

          ext_data->ext_data = NULL;
          ext_data->ext_type = -1;
      }
  }

  clist_foreach(session->imap_response_info->rsp_extension_list,
        (clist_func) mailimap_extension_data_free, NULL);
  clist_free(session->imap_response_info->rsp_extension_list);
  session->imap_response_info->rsp_extension_list = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_EXTENSION;
  }
}

LIBETPAN_EXPORT
int mailimap_acl_listrights(mailimap * session,
    const char * mailbox,
    const char * identifier,
    struct mailimap_acl_listrights_data ** result)
{
  struct mailimap_response * response;
  struct mailimap_extension_data * ext_data;
  clistiter * cur;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_AUTHENTICATED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_listrights_send(session->imap_stream,
    mailbox, identifier);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = NULL;

  /* in rsp_extension_list there is at most one listrights_data */
  for (cur = clist_begin(session->imap_response_info->rsp_extension_list);
    cur != NULL; cur = clist_next(cur)) {
      ext_data = (struct mailimap_extension_data *) clist_content(cur);
      if (
        ext_data->ext_extension->ext_id == MAILIMAP_EXTENSION_ACL &&
        ext_data->ext_type == MAILIMAP_ACL_TYPE_LISTRIGHTS_DATA) {
          * result = (struct mailimap_acl_listrights_data *)ext_data->ext_data;
          /* remove the element from rsp_extension_list */
          clist_delete(session->imap_response_info->rsp_extension_list, cur);

          break;
      }
  }

  clist_foreach(session->imap_response_info->rsp_extension_list,
        (clist_func) mailimap_extension_data_free, NULL);
  clist_free(session->imap_response_info->rsp_extension_list);
  session->imap_response_info->rsp_extension_list = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  /* if there is no data to be returned, return MAILIMAP_ERROR_EXTENSION */
  if (* result == NULL)
    return MAILIMAP_ERROR_EXTENSION;

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_EXTENSION;
  }
}

LIBETPAN_EXPORT
int mailimap_acl_myrights(mailimap * session,
    const char * mailbox,
    struct mailimap_acl_myrights_data ** result)
{
  struct mailimap_response * response;
  struct mailimap_extension_data * ext_data;
  clistiter * cur;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_AUTHENTICATED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_myrights_send(session->imap_stream, mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = NULL;

  /* in rsp_extension_list there is at most one myrights_data */
  for (cur = clist_begin(session->imap_response_info->rsp_extension_list);
    cur != NULL; cur = clist_next(cur)) {
      ext_data = (struct mailimap_extension_data *) clist_content(cur);
      if (
        ext_data->ext_extension->ext_id == MAILIMAP_EXTENSION_ACL &&
        ext_data->ext_type == MAILIMAP_ACL_TYPE_MYRIGHTS_DATA) {
          * result = (struct mailimap_acl_myrights_data *)ext_data->ext_data;
          /* remove the element from rsp_extension_list */
          clist_delete(session->imap_response_info->rsp_extension_list, cur);

          break;
      }
  }

  clist_foreach(session->imap_response_info->rsp_extension_list,
        (clist_func) mailimap_extension_data_free, NULL);
  clist_free(session->imap_response_info->rsp_extension_list);
  session->imap_response_info->rsp_extension_list = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  /* if there is no data to be returned, return MAILIMAP_ERROR_EXTENSION */
  if (* result == NULL)
    return MAILIMAP_ERROR_EXTENSION;

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_EXTENSION;
  }
}

LIBETPAN_EXPORT
int mailimap_has_acl(mailimap * session)
{
  return mailimap_has_extension(session, "ACL");
}

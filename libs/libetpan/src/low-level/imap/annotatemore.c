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
#include "annotatemore.h"
#include "annotatemore_types.h"
#include "annotatemore_parser.h"
#include "annotatemore_sender.h"

#include <stdlib.h>

LIBETPAN_EXPORT
struct mailimap_extension_api mailimap_extension_annotatemore = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* name */          "ANNOTATEMORE",
  /* extension_id */  MAILIMAP_EXTENSION_ANNOTATEMORE,
  /* parser */        mailimap_annotatemore_parse,
  /* free */          mailimap_annotatemore_free,
#else
  .name             = "ANNOTATEMORE",
  .extension_id     = MAILIMAP_EXTENSION_ANNOTATEMORE,
  .parser           = mailimap_annotatemore_parse,
  .free             = mailimap_annotatemore_free,
#endif
};

/*
  this is one of the imap commands annotatemore adds. setannotation is
  yet to be implemented.
*/
LIBETPAN_EXPORT
int mailimap_annotatemore_getannotation(mailimap * session,
    const char * list_mb,
    struct mailimap_annotatemore_entry_match_list * entries,
    struct mailimap_annotatemore_attrib_match_list * attribs,
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

  r = mailimap_annotatemore_getannotation_send(session->imap_stream,
    list_mb, entries, attribs);
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

  /* copy all annotate_data to the result-list */
  for (cur = clist_begin(session->imap_response_info->rsp_extension_list);
    cur != NULL; cur = clist_next(cur)) {
      ext_data = (struct mailimap_extension_data *) clist_content(cur);
      if (
        ext_data->ext_extension->ext_id == MAILIMAP_EXTENSION_ANNOTATEMORE &&
        ext_data->ext_type == MAILIMAP_ANNOTATEMORE_TYPE_ANNOTATE_DATA) {
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
int mailimap_annotatemore_setannotation(mailimap * session,
    const char * list_mb,
    struct mailimap_annotatemore_entry_att_list * en_att,
    int * result)
{
  struct mailimap_response * response;
  int r;
  int error_code;
  clistiter * cur;
  struct mailimap_extension_data * ext_data;

  if (session->imap_state != MAILIMAP_STATE_AUTHENTICATED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_annotatemore_setannotation_send(session->imap_stream,
    list_mb, en_att);
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
    break;
  case MAILIMAP_RESP_COND_STATE_NO:
    * result = MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_UNSPECIFIED;
    if (session->imap_response_info->rsp_extension_list != NULL) {
      for (cur = clist_begin(session->imap_response_info->rsp_extension_list);
        cur != NULL; cur = clist_next(cur)) {
          ext_data = clist_content(cur);
          if ((ext_data->ext_extension->ext_id == 
                MAILIMAP_EXTENSION_ANNOTATEMORE) &&
              (ext_data->ext_type ==
                MAILIMAP_ANNOTATEMORE_TYPE_RESP_TEXT_CODE))
          {
        * result = * ((int *)ext_data->ext_data);
          }
      }
    }
    return MAILIMAP_ERROR_EXTENSION;
    break;
  default:
    * result = MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_UNSPECIFIED;
    return MAILIMAP_ERROR_EXTENSION;
    break;
  }
}

LIBETPAN_EXPORT
int mailimap_has_annotatemore(mailimap * session)
{
  return mailimap_has_extension(session, "ANNOTATEMORE");
}

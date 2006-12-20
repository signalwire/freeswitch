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
#include "uidplus.h"
#include "uidplus_types.h"
#include "uidplus_parser.h"
#include "mailimap_sender.h"
#include "uidplus_sender.h"

#include <stdio.h>
#include <stdlib.h>

void
mailimap_uidplus_free(struct mailimap_extension_data * ext_data);

LIBETPAN_EXPORT
struct mailimap_extension_api mailimap_extension_uidplus = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* name */          "UIDPLUS",
  /* extension_id */  MAILIMAP_EXTENSION_UIDPLUS,
  /* parser */        mailimap_uidplus_parse,
  /* free */          mailimap_uidplus_free,
#else
  .name             = "UIDPLUS",
  .extension_id     = MAILIMAP_EXTENSION_UIDPLUS,
  .parser           = mailimap_uidplus_parse,
  .free             = mailimap_uidplus_free,
#endif
};

LIBETPAN_EXPORT
int mailimap_uid_expunge(mailimap * session, struct mailimap_set * set)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;
  
  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_uid_expunge_send(session->imap_stream, set);
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
    return MAILIMAP_ERROR_EXPUNGE;
  }
}

static void extract_copy_uid(mailimap * session,
    uint32_t * uidvalidity_result,
    struct mailimap_set ** source_result,
    struct mailimap_set ** dest_result)
{
  clistiter * cur;
  
  * uidvalidity_result = 0;
  * source_result = NULL;
  * dest_result = NULL;
  
  if (session->imap_response_info == NULL) {
    return;
  }
  
  for(cur = clist_begin(session->imap_response_info->rsp_extension_list) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_extension_data * ext_data;
    struct mailimap_uidplus_resp_code_copy * resp_code_copy;
    
    ext_data = clist_content(cur);
    if (ext_data->ext_extension != &mailimap_extension_uidplus)
      continue;
    
    if (ext_data->ext_type != MAILIMAP_UIDPLUS_RESP_CODE_COPY)
      continue;
    
    resp_code_copy = ext_data->ext_data;
    
    * uidvalidity_result = resp_code_copy->uid_uidvalidity;
    * source_result = resp_code_copy->uid_source_set;
    * dest_result = resp_code_copy->uid_dest_set;
    resp_code_copy->uid_source_set = NULL;
    resp_code_copy->uid_dest_set = NULL;
    break;
  }
}

LIBETPAN_EXPORT
int mailimap_uidplus_copy(mailimap * session, struct mailimap_set * set,
    const char * mb,
    uint32_t * uidvalidity_result,
    struct mailimap_set ** source_result,
    struct mailimap_set ** dest_result)
{
  int r;
  
  r = mailimap_copy(session, set, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  extract_copy_uid(session, uidvalidity_result, source_result, dest_result);
  
  return MAILIMAP_NO_ERROR;
}

LIBETPAN_EXPORT
int mailimap_uidplus_uid_copy(mailimap * session, struct mailimap_set * set,
    const char * mb,
    uint32_t * uidvalidity_result,
    struct mailimap_set ** source_result,
    struct mailimap_set ** dest_result)
{
  int r;
  
  r = mailimap_uid_copy(session, set, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  extract_copy_uid(session, uidvalidity_result, source_result, dest_result);
  
  return MAILIMAP_NO_ERROR;
}

static void extract_apnd_uid(mailimap * session,
    uint32_t * uidvalidity_result,
    struct mailimap_set ** result)
{
  clistiter * cur;
  
  * uidvalidity_result = 0;
  * result = NULL;
  
  if (session->imap_response_info == NULL) {
    return;
  }
  
  for(cur = clist_begin(session->imap_response_info->rsp_extension_list) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_extension_data * ext_data;
    struct mailimap_uidplus_resp_code_apnd * resp_code_apnd;
    
    ext_data = clist_content(cur);
    if (ext_data->ext_extension != &mailimap_extension_uidplus)
      continue;
    
    if (ext_data->ext_type != MAILIMAP_UIDPLUS_RESP_CODE_APND)
      continue;
    
    resp_code_apnd = ext_data->ext_data;
    
    * uidvalidity_result = resp_code_apnd->uid_uidvalidity;
    * result = resp_code_apnd->uid_set;
    resp_code_apnd->uid_set = NULL;
    break;
  }
}

static void extract_apnd_single_uid(mailimap * session,
    uint32_t * uidvalidity_result,
    uint32_t * uid_result)
{
  struct mailimap_set * set;
  
  extract_apnd_uid(session, uidvalidity_result, &set);
  * uid_result = 0;
  if (set != NULL) {
    clistiter * cur;
    
    cur = clist_begin(set->set_list);
    if (cur != NULL) {
      struct mailimap_set_item * item;
      
      item = clist_content(cur);
      * uid_result = item->set_first;
    }
    mailimap_set_free(set);
  }
}

LIBETPAN_EXPORT
int mailimap_uidplus_append(mailimap * session, const char * mailbox,
    struct mailimap_flag_list * flag_list,
    struct mailimap_date_time * date_time,
    const char * literal, size_t literal_size,
    uint32_t * uidvalidity_result,
    uint32_t * uid_result)
{
  int r;
  
  r = mailimap_append(session, mailbox, flag_list, date_time,
      literal, literal_size);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  extract_apnd_single_uid(session, uidvalidity_result, uid_result);
  
  return MAILIMAP_NO_ERROR;
}

LIBETPAN_EXPORT
int mailimap_uidplus_append_simple(mailimap * session, const char * mailbox,
    const char * content, uint32_t size,
    uint32_t * uidvalidity_result,
    uint32_t * uid_result)
{
  int r;
  
  r = mailimap_append_simple(session, mailbox,
      content, size);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  extract_apnd_single_uid(session, uidvalidity_result, uid_result);
  
  return MAILIMAP_NO_ERROR;
}

LIBETPAN_EXPORT
int mailimap_has_uidplus(mailimap * session)
{
  return mailimap_has_extension(session, "UIDPLUS");
}

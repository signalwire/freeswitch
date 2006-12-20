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

/*
 * $Id: mailimap.c,v 1.37 2006/10/20 00:13:30 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap.h"
#include "mailimap_parser.h"
#include "mailimap_sender.h"
#include "mailimap_extension.h"
#include "mail.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

#include "mailsasl.h"

#ifdef DEBUG
#include "mailimap_print.h"
#endif

/*
  RFC 2060 : IMAP4rev1
  draft-crispin-imapv-15
  RFC 2222 : Simple Authentication and Security Layer

2061 IMAP4 Compatibility with IMAP2bis. M. Crispin. December 1996.
     (Format: TXT=5867 bytes) (Obsoletes RFC1730) (Status: INFORMATIONAL)

2062 Internet Message Access Protocol - Obsolete Syntax. M. Crispin.
     December 1996. (Format: TXT=14222 bytes) (Status: INFORMATIONAL)

2086 IMAP4 ACL extension. J. Myers. January 1997. (Format: TXT=13925
     bytes) (Status: PROPOSED STANDARD)

2087 IMAP4 QUOTA extension. J. Myers. January 1997. (Format: TXT=8542
     bytes) (Status: PROPOSED STANDARD)

2088 IMAP4 non-synchronizing literals. J. Myers. January 1997.
     (Format: TXT=4052 bytes) (Status: PROPOSED STANDARD)

2177 IMAP4 IDLE command. B. Leiba. June 1997. (Format: TXT=6770 bytes)
     (Status: PROPOSED STANDARD)

2180 IMAP4 Multi-Accessed Mailbox Practice. M. Gahrns. July 1997.
     (Format: TXT=24750 bytes) (Status: INFORMATIONAL)

2192 IMAP URL Scheme. C. Newman. September 1997. (Format: TXT=31426
     bytes) (Status: PROPOSED STANDARD)

2193 IMAP4 Mailbox Referrals. M. Gahrns. September 1997. (Format:
     TXT=16248 bytes) (Status: PROPOSED STANDARD)

2195 IMAP/POP AUTHorize Extension for Simple Challenge/Response. J.
     Klensin, R. Catoe, P. Krumviede. September 1997. (Format: TXT=10468
     bytes) (Obsoletes RFC2095) (Status: PROPOSED STANDARD)

2221 IMAP4 Login Referrals. M. Gahrns. October 1997. (Format: TXT=9251
     bytes) (Status: PROPOSED STANDARD)

2342 IMAP4 Namespace. M. Gahrns, C. Newman. May 1998. (Format:
     TXT=19489 bytes) (Status: PROPOSED STANDARD)

2359 IMAP4 UIDPLUS extension. J. Myers. June 1998. (Format: TXT=10862
     bytes) (Status: PROPOSED STANDARD)

2595 Using TLS with IMAP, POP3 and ACAP. C. Newman. June 1999.
     (Format: TXT=32440 bytes) (Status: PROPOSED STANDARD)

2683 IMAP4 Implementation Recommendations. B. Leiba. September 1999.
     (Format: TXT=56300 bytes) (Status: INFORMATIONAL)

2971 IMAP4 ID extension. T. Showalter. October 2000. (Format:
     TXT=14670 bytes) (Status: PROPOSED STANDARD)

http://www.ietf.org/ids.by.wg/imapext.html
*/

static int parse_greeting(mailimap * session,
			   struct mailimap_greeting ** result);


/* struct mailimap_response_info * */

static void resp_text_store(mailimap * session,
			    struct mailimap_resp_text *
			    resp_text)
{
  struct mailimap_resp_text_code * resp_text_code;

  resp_text_code = resp_text->rsp_code;
  
  if (resp_text_code != NULL) {
    switch (resp_text_code->rc_type) {
    case MAILIMAP_RESP_TEXT_CODE_ALERT:
      if (session->imap_response_info)
	if (session->imap_response_info->rsp_alert != NULL)
	  free(session->imap_response_info->rsp_alert);
      session->imap_response_info->rsp_alert = strdup(resp_text->rsp_text);
      break;
      
    case MAILIMAP_RESP_TEXT_CODE_BADCHARSET:
      if (session->imap_response_info) {
	if (session->imap_response_info->rsp_badcharset != NULL) {
	  clist_foreach(resp_text_code->rc_data.rc_badcharset,
              (clist_func) mailimap_astring_free, NULL);
	  clist_free(resp_text_code->rc_data.rc_badcharset);
	}
	session->imap_response_info->rsp_badcharset =
          resp_text_code->rc_data.rc_badcharset;
	resp_text_code->rc_data.rc_badcharset = NULL;
      }
      break;

    case MAILIMAP_RESP_TEXT_CODE_CAPABILITY_DATA:
      if (session->imap_connection_info) {
	if (session->imap_connection_info->imap_capability != NULL)
	  mailimap_capability_data_free(session->imap_connection_info->imap_capability);
	session->imap_connection_info->imap_capability =
          resp_text_code->rc_data.rc_cap_data;
        /* detach before free */
	resp_text_code->rc_data.rc_cap_data = NULL;
      }
      break;

    case MAILIMAP_RESP_TEXT_CODE_PARSE:
      if (session->imap_response_info) {
	if (session->imap_response_info->rsp_parse != NULL)
	  free(session->imap_response_info->rsp_parse);
	session->imap_response_info->rsp_parse = strdup(resp_text->rsp_text);
      }
      break;

    case MAILIMAP_RESP_TEXT_CODE_PERMANENTFLAGS:
      if (session->imap_selection_info) {
	if (session->imap_selection_info->sel_perm_flags != NULL) {
	  clist_foreach(session->imap_selection_info->sel_perm_flags,
              (clist_func) mailimap_flag_perm_free, NULL);
	  clist_free(session->imap_selection_info->sel_perm_flags);
	}
	session->imap_selection_info->sel_perm_flags =
          resp_text_code->rc_data.rc_perm_flags;
        /* detach before free */
	resp_text_code->rc_data.rc_perm_flags = NULL;
      }
      break;

    case MAILIMAP_RESP_TEXT_CODE_READ_ONLY:
      if (session->imap_selection_info)
	session->imap_selection_info->sel_perm = MAILIMAP_MAILBOX_READONLY;
      break;
	  
    case MAILIMAP_RESP_TEXT_CODE_READ_WRITE:
      if (session->imap_selection_info)
	session->imap_selection_info->sel_perm = MAILIMAP_MAILBOX_READWRITE;
      break;
	  
    case MAILIMAP_RESP_TEXT_CODE_TRY_CREATE:
      if (session->imap_response_info)
	session->imap_response_info->rsp_trycreate = TRUE;
      break;
	  
    case MAILIMAP_RESP_TEXT_CODE_UIDNEXT:
      if (session->imap_selection_info)
	session->imap_selection_info->sel_uidnext =
          resp_text_code->rc_data.rc_uidnext;
      break;
	  
    case MAILIMAP_RESP_TEXT_CODE_UIDVALIDITY:
      if (session->imap_selection_info)
	session->imap_selection_info->sel_uidvalidity =
          resp_text_code->rc_data.rc_uidvalidity;
      break;
	  
    case MAILIMAP_RESP_TEXT_CODE_UNSEEN:
      if (session->imap_selection_info)
	session->imap_selection_info->sel_first_unseen =
          resp_text_code->rc_data.rc_first_unseen;
      break;
      
    case MAILIMAP_RESP_TEXT_CODE_OTHER:
      if (session->imap_response_info) {
        if (session->imap_response_info->rsp_atom != NULL)
          free(session->imap_response_info->rsp_atom);
        if (session->imap_response_info->rsp_value != NULL)
          free(session->imap_response_info->rsp_value);
	session->imap_response_info->rsp_atom =
          resp_text_code->rc_data.rc_atom.atom_name;
        resp_text_code->rc_data.rc_atom.atom_name = NULL;
	session->imap_response_info->rsp_value =
          resp_text_code->rc_data.rc_atom.atom_value;
        resp_text_code->rc_data.rc_atom.atom_value = NULL;
      }
      break;
    case MAILIMAP_RESP_TEXT_CODE_EXTENSION:
      mailimap_extension_data_store(session, &(resp_text_code->rc_data.rc_ext_data));
      break;
    }
  }
}

static void resp_cond_state_store(mailimap * session,
    struct mailimap_resp_cond_state * resp_cond_state)
{
  resp_text_store(session, resp_cond_state->rsp_text);
}

static void mailbox_data_store(mailimap * session,
    struct mailimap_mailbox_data * mb_data)
{
  int r;
  
  switch (mb_data->mbd_type) {
  case MAILIMAP_MAILBOX_DATA_FLAGS:
    if (session->imap_selection_info) {
      if (session->imap_selection_info->sel_flags != NULL)
	mailimap_flag_list_free(session->imap_selection_info->sel_flags);
      session->imap_selection_info->sel_flags = mb_data->mbd_data.mbd_flags;
      mb_data->mbd_data.mbd_flags = NULL;
    }
    break;

  case MAILIMAP_MAILBOX_DATA_LIST:
    if (session->imap_response_info) {
      r = clist_append(session->imap_response_info->rsp_mailbox_list,
          mb_data->mbd_data.mbd_list);
      if (r == 0)
	mb_data->mbd_data.mbd_list = NULL;
      else {
	/* TODO must handle error case */	
      }
    }
    break;
    
  case MAILIMAP_MAILBOX_DATA_LSUB:
    if (session->imap_response_info) {
      r =  clist_append(session->imap_response_info->rsp_mailbox_lsub,
          mb_data->mbd_data.mbd_lsub);
      if (r == 0)
	mb_data->mbd_data.mbd_lsub = NULL;
      else {
	/* TODO must handle error case */
      }
    }
    break;

  case MAILIMAP_MAILBOX_DATA_SEARCH:
    if (session->imap_response_info) {
      if (session->imap_response_info->rsp_search_result != NULL) {
        if (mb_data->mbd_data.mbd_search != NULL) {
          clist_concat(session->imap_response_info->rsp_search_result,
              mb_data->mbd_data.mbd_search);
          clist_free(mb_data->mbd_data.mbd_search);
          mb_data->mbd_data.mbd_search = NULL;
        }
      }
      else {
        if (mb_data->mbd_data.mbd_search != NULL) {
          session->imap_response_info->rsp_search_result =
            mb_data->mbd_data.mbd_search;
          mb_data->mbd_data.mbd_search = NULL;
        }
      }
    }
    break;

  case MAILIMAP_MAILBOX_DATA_STATUS:
    if (session->imap_response_info) {
      if (session->imap_response_info->rsp_status != NULL)
        mailimap_mailbox_data_status_free(session->imap_response_info->rsp_status);
      session->imap_response_info->rsp_status = mb_data->mbd_data.mbd_status;
#if 0
      if (session->imap_selection_info != NULL) {
        clistiter * cur;
        
        for(cur = clist_begin(mb_data->status->status_info_list)
            ; cur != NULL ; cur = clist_next(cur)) {
          struct mailimap_status_info * info;
          
          info = clist_content(cur);
          switch (info->att) {
            case MAILIMAP_STATUS_ATT_MESSAGES:
              session->imap_selection_info->exists = info->value;
              break;
            case MAILIMAP_STATUS_ATT_RECENT:
              session->imap_selection_info->recent = info->value;
              break;
            case MAILIMAP_STATUS_ATT_UIDNEXT:
              session->imap_selection_info->uidnext = info->value;
              break;
            case MAILIMAP_STATUS_ATT_UIDVALIDITY:
              session->imap_selection_info->uidvalidity = info->value;
              break;
            case MAILIMAP_STATUS_ATT_UNSEEN:
              session->imap_selection_info->unseen = info->value;
              break;
          }
        }
      }
#endif
#if 0
      mailimap_mailbox_data_status_free(mb_data->status);
#endif
      mb_data->mbd_data.mbd_status = NULL;
    }
    break;
	
  case MAILIMAP_MAILBOX_DATA_EXISTS:
    if (session->imap_selection_info)
      session->imap_selection_info->sel_exists = mb_data->mbd_data.mbd_exists;
    break;

  case MAILIMAP_MAILBOX_DATA_RECENT:
    if (session->imap_selection_info)
      session->imap_selection_info->sel_recent =
        mb_data->mbd_data.mbd_recent;
    break;
  case MAILIMAP_MAILBOX_DATA_EXTENSION_DATA:
    if (session->imap_response_info) {
      r = clist_append(session->imap_response_info->rsp_extension_list,
          mb_data->mbd_data.mbd_extension);
      if (r == 0)
	mb_data->mbd_data.mbd_extension = NULL;
      else {
	/* TODO must handle error case */	
      }
    }
  }
}

static void
message_data_store(mailimap * session,
    struct mailimap_message_data * msg_data)
{
  uint32_t * expunged;
  int r;
  
  switch (msg_data->mdt_type) {
  case MAILIMAP_MESSAGE_DATA_EXPUNGE:
    if (session->imap_response_info) {
      expunged = mailimap_number_alloc_new(msg_data->mdt_number);
      if (expunged != NULL) {
	r = clist_append(session->imap_response_info->rsp_expunged, expunged);
	if (r == 0) {
	  /* do nothing */
	}
	else {
	  /* TODO : must handle error case */
	  mailimap_number_alloc_free(expunged);
	}
        if (session->imap_selection_info != NULL)
          session->imap_selection_info->sel_exists --;
      }
    }
    break;

  case MAILIMAP_MESSAGE_DATA_FETCH:
    r = clist_append(session->imap_response_info->rsp_fetch_list,
        msg_data->mdt_msg_att);
    if (r == 0) {
      msg_data->mdt_msg_att->att_number = msg_data->mdt_number;
      msg_data->mdt_msg_att = NULL;
    }
    else {
      /* TODO : must handle error case */
    }
    break;
  }
}

static void
cont_req_or_resp_data_store(mailimap * session,
    struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data)
{
  if (cont_req_or_resp_data->rsp_type == MAILIMAP_RESP_RESP_DATA) {
    struct mailimap_response_data * resp_data;

    resp_data = cont_req_or_resp_data->rsp_data.rsp_resp_data;

    switch (resp_data->rsp_type) {
    case MAILIMAP_RESP_DATA_TYPE_COND_STATE:
      resp_cond_state_store(session, resp_data->rsp_data.rsp_cond_state);
      break;
    case MAILIMAP_RESP_DATA_TYPE_MAILBOX_DATA:
      mailbox_data_store(session, resp_data->rsp_data.rsp_mailbox_data);
      break;
    case MAILIMAP_RESP_DATA_TYPE_MESSAGE_DATA:
      message_data_store(session, resp_data->rsp_data.rsp_message_data);
      break;
    case MAILIMAP_RESP_DATA_TYPE_CAPABILITY_DATA:
      if (session->imap_connection_info) {
	if (session->imap_connection_info->imap_capability != NULL)
	  mailimap_capability_data_free(session->imap_connection_info->imap_capability);
	session->imap_connection_info->imap_capability = resp_data->rsp_data.rsp_capability_data;
	resp_data->rsp_data.rsp_capability_data = NULL;
      }
      break;
    case MAILIMAP_RESP_DATA_TYPE_EXTENSION_DATA:
      mailimap_extension_data_store(session, &(resp_data->rsp_data.rsp_extension_data));
      break;
    }
  }
}

static void response_tagged_store(mailimap * session,
    struct mailimap_response_tagged * tagged)
{
  resp_cond_state_store(session, tagged->rsp_cond_state);
}

static void resp_cond_bye_store(mailimap * session,
    struct mailimap_resp_cond_bye * resp_cond_bye)
{
  resp_text_store(session, resp_cond_bye->rsp_text);
}

static void response_fatal_store(mailimap * session,
    struct mailimap_response_fatal * fatal)
{
  resp_cond_bye_store(session, fatal->rsp_bye);
}

static void response_done_store(mailimap * session,
    struct mailimap_response_done * resp_done)
{
  switch(resp_done->rsp_type) {
  case MAILIMAP_RESP_DONE_TYPE_TAGGED:
    response_tagged_store(session, resp_done->rsp_data.rsp_tagged);
    break;
  case MAILIMAP_RESP_DONE_TYPE_FATAL:
    response_fatal_store(session, resp_done->rsp_data.rsp_fatal);
    break;
  }
}

static void
response_store(mailimap * session,
    struct mailimap_response * response)
{
  clistiter * cur;

  if (session->imap_response_info) {
    mailimap_response_info_free(session->imap_response_info);
    session->imap_response_info = NULL;
  }

  session->imap_response_info = mailimap_response_info_new();
  if (session->imap_response_info == NULL) {
    /* ignored error */
    return;
  }

  if (response->rsp_cont_req_or_resp_data_list != NULL) {
    for(cur = clist_begin(response->rsp_cont_req_or_resp_data_list) ;
	cur != NULL ; cur = clist_next(cur)) {
      struct mailimap_cont_req_or_resp_data * cont_req_or_resp_data;
      
      cont_req_or_resp_data = clist_content(cur);
      
      cont_req_or_resp_data_store(session, cont_req_or_resp_data);
    }
  }

  response_done_store(session, response->rsp_resp_done);
}

static void resp_cond_auth_store(mailimap * session,
    struct mailimap_resp_cond_auth * cond_auth)
{
  resp_text_store(session, cond_auth->rsp_text);
}

static void greeting_store(mailimap * session,
    struct mailimap_greeting * greeting)
{
  switch (greeting->gr_type) {
  case MAILIMAP_GREETING_RESP_COND_AUTH:
    resp_cond_auth_store(session, greeting->gr_data.gr_auth);
    break;

  case MAILIMAP_GREETING_RESP_COND_BYE:
    resp_cond_bye_store(session, greeting->gr_data.gr_bye);
    break;
  }
}

LIBETPAN_EXPORT
int mailimap_connect(mailimap * session, mailstream * s)
{
  struct mailimap_greeting * greeting;
  int r;
  int auth_type;
  struct mailimap_connection_info * connection_info;
  
  if (session->imap_state != MAILIMAP_STATE_DISCONNECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  session->imap_stream = s;
  
  if (session->imap_connection_info)
    mailimap_connection_info_free(session->imap_connection_info);
  connection_info = mailimap_connection_info_new();
  if (connection_info != NULL)
    session->imap_connection_info = connection_info;

  if (read_line(session) == NULL) {
    return MAILIMAP_ERROR_STREAM;
  }

  r = parse_greeting(session, &greeting);
  if (r != MAILIMAP_NO_ERROR) {
    return r;
  }
  
  auth_type = greeting->gr_data.gr_auth->rsp_type;

  mailimap_greeting_free(greeting);

  switch (auth_type) {
  case MAILIMAP_RESP_COND_AUTH_PREAUTH:
    session->imap_state = MAILIMAP_STATE_AUTHENTICATED;
    return MAILIMAP_NO_ERROR_AUTHENTICATED;
  default:
    session->imap_state = MAILIMAP_STATE_NON_AUTHENTICATED;
    return MAILIMAP_NO_ERROR_NON_AUTHENTICATED;
  }
}








/* ********************************************************************** */



LIBETPAN_EXPORT
int mailimap_append(mailimap * session, const char * mailbox,
    struct mailimap_flag_list * flag_list,
    struct mailimap_date_time * date_time,
    const char * literal, size_t literal_size)
{
  struct mailimap_response * response;
  int r;
  int error_code;
  struct mailimap_continue_req * cont_req;
  size_t index;
  size_t fixed_literal_size;
  
  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;
  
  fixed_literal_size = mailstream_get_data_crlf_size(literal, literal_size);
  
  r = mailimap_append_send(session->imap_stream, mailbox, flag_list, date_time,
      fixed_literal_size);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  index = 0;

  r = mailimap_continue_req_parse(session->imap_stream,
      session->imap_stream_buffer,
      &index, &cont_req,
      session->imap_progr_rate, session->imap_progr_fun);
  if (r == MAILIMAP_NO_ERROR)
    mailimap_continue_req_free(cont_req);

  if (r == MAILIMAP_ERROR_PARSE) {
    r = parse_response(session, &response);
    if (r != MAILIMAP_NO_ERROR)
      return r;
    mailimap_response_free(response);
    
    return MAILIMAP_ERROR_APPEND;
  }

  r = mailimap_literal_data_send(session->imap_stream, literal, literal_size,
      session->imap_progr_rate, session->imap_progr_fun);
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
    return MAILIMAP_ERROR_APPEND;
  }
}

LIBETPAN_EXPORT
int mailimap_noop(mailimap * session)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_noop_send(session->imap_stream);
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
    return MAILIMAP_ERROR_NOOP;
  }
}

LIBETPAN_EXPORT
int mailimap_logout(mailimap * session)
{
  struct mailimap_response * response;
  int r;
  int error_code;
  int res;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto close;
  }

  r = mailimap_logout_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto close;
  }

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto close;
  }

  if (mailstream_flush(session->imap_stream) == -1) {
    res = MAILIMAP_ERROR_STREAM;
    goto close;
  }

  if (read_line(session) == NULL) {
    res = MAILIMAP_ERROR_STREAM;
    goto close;
  }

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto close;
  }

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    if (session->imap_connection_info) {
      mailimap_connection_info_free(session->imap_connection_info);
      session->imap_connection_info = NULL;
    }
    res = MAILIMAP_NO_ERROR;
    goto close;

  default:
    res = MAILIMAP_ERROR_LOGOUT;
    goto close;
  }

 close:
  mailstream_close(session->imap_stream);
  session->imap_stream = NULL;
  session->imap_state = MAILIMAP_STATE_DISCONNECTED;
  return res;
}

/* send the results back to the caller */
/* duplicate the result */

static struct mailimap_capability *
mailimap_capability_dup(struct mailimap_capability * orig_cap)
{
  struct mailimap_capability * cap;
  char * auth_type;
  char * name;
  
  name = NULL;
  auth_type = NULL;
  switch (orig_cap->cap_type) {
  case MAILIMAP_CAPABILITY_NAME:
    name = strdup(orig_cap->cap_data.cap_name);
    if (name == NULL)
      goto err;
    break;
  case MAILIMAP_CAPABILITY_AUTH_TYPE:
    auth_type = strdup(orig_cap->cap_data.cap_auth_type);
    if (auth_type == NULL)
      goto err;
    break;
  }
  
  cap = mailimap_capability_new(orig_cap->cap_type, auth_type, name);
  if (cap == NULL)
    goto free;

  return cap;

 free:
  if (name != NULL)
    free(name);
  if (auth_type != NULL)
    free(auth_type);
 err:
  return NULL;
}

static struct mailimap_capability_data *
mailimap_capability_data_dup(struct mailimap_capability_data * orig_cap_data)
{
  struct mailimap_capability_data * cap_data;
  struct mailimap_capability * cap_dup;
  clist * list;
  clistiter * cur;
  int r;

  list = clist_new();
  if (list == NULL)
	goto err;
  
  for(cur = clist_begin(orig_cap_data->cap_list) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_capability * cap;
    
    cap = clist_content(cur);

    cap_dup = mailimap_capability_dup(cap);
    if (cap_dup == NULL)
      goto list;
    
    r = clist_append(list, cap_dup);
    if (r < 0) {
      mailimap_capability_free(cap_dup);
      goto list;
    }
  }

  cap_data = mailimap_capability_data_new(list);
  if (cap_data == NULL)
    goto list;

  return cap_data;

list:
  clist_foreach(list, (clist_func) mailimap_capability_free, NULL);
  clist_free(list);
err:
  return NULL;
}

LIBETPAN_EXPORT
int mailimap_capability(mailimap * session,
				struct mailimap_capability_data ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;
  struct mailimap_capability_data * cap_data;
  
  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_capability_send(session->imap_stream);
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
    cap_data =
      mailimap_capability_data_dup(session->imap_connection_info->imap_capability);
    if (cap_data == NULL)
	  return MAILIMAP_ERROR_MEMORY;
		  
	* result = cap_data;

	return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_CAPABILITY;
  }
}

LIBETPAN_EXPORT
int mailimap_check(mailimap * session)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_check_send(session->imap_stream);
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
    return MAILIMAP_ERROR_CHECK;
  }
}

LIBETPAN_EXPORT
int mailimap_close(mailimap * session)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_close_send(session->imap_stream);
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
    /* leave selected state */
    mailimap_selection_info_free(session->imap_selection_info);
    session->imap_selection_info = NULL;

    session->imap_state = MAILIMAP_STATE_AUTHENTICATED;
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_CLOSE;
  }
}

LIBETPAN_EXPORT
int mailimap_expunge(mailimap * session)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_expunge_send(session->imap_stream);
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

LIBETPAN_EXPORT
int mailimap_copy(mailimap * session, struct mailimap_set * set,
    const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_copy_send(session->imap_stream, set, mb);
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
    return MAILIMAP_ERROR_COPY;
  }
}

LIBETPAN_EXPORT
int mailimap_uid_copy(mailimap * session, struct mailimap_set * set,
    const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_uid_copy_send(session->imap_stream, set, mb);
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
    return MAILIMAP_ERROR_UID_COPY;
  }
}

LIBETPAN_EXPORT
int mailimap_create(mailimap * session, const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_create_send(session->imap_stream, mb);
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
    return MAILIMAP_ERROR_CREATE;
  }
}


LIBETPAN_EXPORT
int mailimap_delete(mailimap * session, const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_delete_send(session->imap_stream, mb);
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
    return MAILIMAP_ERROR_DELETE;
  }
}

LIBETPAN_EXPORT
int mailimap_examine(mailimap * session, const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_examine_send(session->imap_stream, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  if (session->imap_selection_info != NULL)
    mailimap_selection_info_free(session->imap_selection_info);
  session->imap_selection_info = mailimap_selection_info_new();

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    session->imap_state = MAILIMAP_STATE_SELECTED;
    return MAILIMAP_NO_ERROR;

  default:
    mailimap_selection_info_free(session->imap_selection_info);
    session->imap_selection_info = NULL;
    session->imap_state = MAILIMAP_STATE_AUTHENTICATED;
    return MAILIMAP_ERROR_EXAMINE;
  }
}

LIBETPAN_EXPORT
int
mailimap_fetch(mailimap * session, struct mailimap_set * set,
	       struct mailimap_fetch_type * fetch_type, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_fetch_send(session->imap_stream, set, fetch_type);
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

  * result = session->imap_response_info->rsp_fetch_list;
  session->imap_response_info->rsp_fetch_list = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_FETCH;
  }
}

LIBETPAN_EXPORT
void mailimap_fetch_list_free(clist * fetch_list)
{
  clist_foreach(fetch_list, (clist_func) mailimap_msg_att_free, NULL);
  clist_free(fetch_list);
}

LIBETPAN_EXPORT
int
mailimap_uid_fetch(mailimap * session,
		   struct mailimap_set * set,
		   struct mailimap_fetch_type * fetch_type, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_uid_fetch_send(session->imap_stream, set, fetch_type);
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

  * result = session->imap_response_info->rsp_fetch_list;
  session->imap_response_info->rsp_fetch_list = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_UID_FETCH;
  }
}

LIBETPAN_EXPORT
int mailimap_list(mailimap * session, const char * mb,
		   const char * list_mb, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_list_send(session->imap_stream, mb, list_mb);
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

  * result = session->imap_response_info->rsp_mailbox_list;
  session->imap_response_info->rsp_mailbox_list = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_LIST;
  }
}

LIBETPAN_EXPORT
int mailimap_login(mailimap * session,
    const char * userid, const char * password)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_NON_AUTHENTICATED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_login_send(session->imap_stream, userid, password);
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
    session->imap_state = MAILIMAP_STATE_AUTHENTICATED;
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_LOGIN;
  }
}

#ifdef USE_SASL
static int sasl_getsimple(void * context, int id,
    const char ** result, unsigned * len)
{
  mailimap * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_USER:
    if (result != NULL)
      * result = session->imap_sasl.sasl_login;
    if (len != NULL)
      * len = strlen(session->imap_sasl.sasl_login);
    return SASL_OK;
    
  case SASL_CB_AUTHNAME:
    if (result != NULL)
      * result = session->imap_sasl.sasl_auth_name;
    if (len != NULL)
      * len = strlen(session->imap_sasl.sasl_auth_name);
    return SASL_OK;
  }
  
  return SASL_FAIL;
}

static int sasl_getsecret(sasl_conn_t * conn, void * context, int id,
    sasl_secret_t ** psecret)
{
  mailimap * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_PASS:
    if (psecret != NULL)
      * psecret = session->imap_sasl.sasl_secret;
    return SASL_OK;
  }
  
  return SASL_FAIL;
}

static int sasl_getrealm(void * context, int id,
    const char ** availrealms,
    const char ** result)
{
  mailimap * session;
  
  session = context;
  
  switch (id) {
  case SASL_CB_GETREALM:
    if (result != NULL)
      * result = session->imap_sasl.sasl_realm;
    return SASL_OK;
  }
  
  return SASL_FAIL;
}
#endif

LIBETPAN_EXPORT
int mailimap_authenticate(mailimap * session, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm)
{
#ifdef USE_SASL
  struct mailimap_response * response;
  int r;
  int error_code;
  size_t index;
  struct mailimap_cont_req_or_resp_data * cont_or_resp_data;
  sasl_callback_t sasl_callback[5];
  const char * sasl_out;
  unsigned sasl_out_len;
  const char * mechusing;
  sasl_secret_t * secret;
  int res;
  size_t len;
  
  if (session->imap_state != MAILIMAP_STATE_NON_AUTHENTICATED) {
    res = MAILIMAP_ERROR_BAD_STATE;
    goto err;
  }
  
  sasl_callback[0].id = SASL_CB_GETREALM;
  sasl_callback[0].proc =  sasl_getrealm;
  sasl_callback[0].context = session;
  sasl_callback[1].id = SASL_CB_USER;
  sasl_callback[1].proc =  sasl_getsimple;
  sasl_callback[1].context = session;
  sasl_callback[2].id = SASL_CB_AUTHNAME;
  sasl_callback[2].proc =  sasl_getsimple;
  sasl_callback[2].context = session; 
  sasl_callback[3].id = SASL_CB_PASS;
  sasl_callback[3].proc =  sasl_getsecret;
  sasl_callback[3].context = session;
  sasl_callback[4].id = SASL_CB_LIST_END;
  sasl_callback[4].proc =  NULL;
  sasl_callback[4].context = NULL;

  len = strlen(password);
  secret = malloc(sizeof(* secret) + len);
  if (secret == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto err;
  }
  secret->len = len;
  memcpy(secret->data, password, len + 1);

  session->imap_sasl.sasl_server_fqdn = server_fqdn;
  session->imap_sasl.sasl_login = login;
  session->imap_sasl.sasl_auth_name = auth_name;
  session->imap_sasl.sasl_password = password;
  session->imap_sasl.sasl_realm = realm;
  session->imap_sasl.sasl_secret = secret;
  
  /* init SASL */
  if (session->imap_sasl.sasl_conn != NULL) {
    sasl_dispose((sasl_conn_t **) &session->imap_sasl.sasl_conn);
    session->imap_sasl.sasl_conn = NULL;
  }
  else {
    mailsasl_ref();
  }
  
  r = sasl_client_new("imap", server_fqdn,
      local_ip_port, remote_ip_port, sasl_callback, 0,
      (sasl_conn_t **) &session->imap_sasl.sasl_conn);
  if (r != SASL_OK) {
    res = MAILIMAP_ERROR_LOGIN;
    goto free_secret;
  }
  
  r = sasl_client_start(session->imap_sasl.sasl_conn,
      auth_type, NULL, &sasl_out, &sasl_out_len, &mechusing);
  if ((r != SASL_CONTINUE) && (r != SASL_OK)) {
    res = MAILIMAP_ERROR_LOGIN;
    goto free_sasl_conn;
  }
  
  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_sasl_conn;
  }

  r = mailimap_authenticate_send(session->imap_stream, auth_type);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_sasl_conn;
  }

  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_sasl_conn;
  }

  if (mailstream_flush(session->imap_stream) == -1) {
    res = MAILIMAP_ERROR_STREAM;
    goto free_sasl_conn;
  }
  
  while (1) {
    struct mailimap_continue_req * cont_req;
    char * response;
    int got_response;
    char * encoded;
    unsigned int encoded_len;
    unsigned int max_encoded;
    
    if (read_line(session) == NULL) {
      res = MAILIMAP_ERROR_STREAM;
      goto free_sasl_conn;
    }
    
    index = 0;
    
    r = mailimap_continue_req_parse(session->imap_stream,
        session->imap_stream_buffer,
        &index, &cont_req,
        session->imap_progr_rate, session->imap_progr_fun);
    if (r != MAILIMAP_NO_ERROR)
      break;
    
    got_response = 1;
    if (cont_req->cr_type == MAILIMAP_CONTINUE_REQ_BASE64) {
      response = cont_req->cr_data.cr_base64;
      if (* response == '\0')
        got_response = 0;
    }
    else {
      response = "";
      got_response = 0;
    }
    
    if (got_response) {
      size_t response_len;
      char * decoded;
      unsigned int decoded_len;
      unsigned int max_decoded;
      
      response_len = strlen(response);
      max_decoded = response_len * 3 / 4;
      decoded = malloc(max_decoded + 1);
      if (decoded == NULL) {
        mailimap_continue_req_free(cont_req);
        res = MAILIMAP_ERROR_MEMORY;
        goto free_sasl_conn;
      }
      
      r = sasl_decode64(response, response_len,
          decoded, max_decoded + 1, &decoded_len);
      
      mailimap_continue_req_free(cont_req);
      
      if (r != SASL_OK) {
        free(decoded);
        res = MAILIMAP_ERROR_MEMORY;
        goto free_sasl_conn;
      }
      
      r = sasl_client_step(session->imap_sasl.sasl_conn,
          decoded, decoded_len, NULL, &sasl_out, &sasl_out_len);
      
      free(decoded);
      
      if ((r != SASL_CONTINUE) && (r != SASL_OK)) {
        res = MAILIMAP_ERROR_LOGIN;
        goto free_sasl_conn;
      }
    }
    else {
      mailimap_continue_req_free(cont_req);
    }
    
    max_encoded = ((sasl_out_len + 2) / 3) * 4;
    encoded = malloc(max_encoded + 1);
    if (encoded == NULL) {
      res = MAILIMAP_ERROR_MEMORY;
      goto free_sasl_conn;
    }
    
    r = sasl_encode64(sasl_out, sasl_out_len,
        encoded, max_encoded + 1, &encoded_len);
    if (r != SASL_OK) {
      free(encoded);
      res = MAILIMAP_ERROR_MEMORY;
      goto free_sasl_conn;
    }
    
    r = mailimap_token_send(session->imap_stream, encoded);
    
    free(encoded);
    
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto free_sasl_conn;
    }
    
    r = mailimap_crlf_send(session->imap_stream);
    if (r != MAILIMAP_NO_ERROR) {
      res = r;
      goto free_sasl_conn;
    }
    
    if (mailstream_flush(session->imap_stream) == -1) {
      res = MAILIMAP_ERROR_STREAM;
      goto free_sasl_conn;
    }
  }
  
  free(session->imap_sasl.sasl_secret);
  session->imap_sasl.sasl_secret = NULL;
  
  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_sasl_conn;
  }
  
  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;
  
  mailimap_response_free(response);
  
  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    session->imap_state = MAILIMAP_STATE_AUTHENTICATED;
    res = MAILIMAP_NO_ERROR;
    goto free_sasl_conn;
    
  default:
    res = MAILIMAP_ERROR_LOGIN;
    goto free_sasl_conn;
  }
  
 free_sasl_conn:
  sasl_dispose((sasl_conn_t **) &session->imap_sasl.sasl_conn);
  session->imap_sasl.sasl_conn = NULL;
  mailsasl_unref();
 free_secret:
  free(session->imap_sasl.sasl_secret);
  session->imap_sasl.sasl_secret = NULL;
 err:
  return res;
#else
  return MAILIMAP_ERROR_LOGIN;
#endif
}


LIBETPAN_EXPORT
int mailimap_lsub(mailimap * session, const char * mb,
    const char * list_mb, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_lsub_send(session->imap_stream, mb, list_mb);
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

  * result = session->imap_response_info->rsp_mailbox_lsub;
  session->imap_response_info->rsp_mailbox_lsub = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_LSUB;
  }
}

LIBETPAN_EXPORT
void mailimap_list_result_free(clist * list)
{
  clist_foreach(list, (clist_func) mailimap_mailbox_list_free, NULL);
  clist_free(list);
}

LIBETPAN_EXPORT
int mailimap_rename(mailimap * session,
    const char * mb, const char * new_name)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_rename_send(session->imap_stream, mb, new_name);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  if (!mailimap_crlf_send(session->imap_stream))
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
    return MAILIMAP_ERROR_RENAME;
  }
}

LIBETPAN_EXPORT
int
mailimap_search(mailimap * session, const char * charset,
    struct mailimap_search_key * key, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_search_send(session->imap_stream, charset, key);
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

  * result = session->imap_response_info->rsp_search_result;
  session->imap_response_info->rsp_search_result = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_SEARCH;
  }
}

LIBETPAN_EXPORT
int
mailimap_uid_search(mailimap * session, const char * charset,
    struct mailimap_search_key * key, clist ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_uid_search_send(session->imap_stream, charset, key);
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

  * result = session->imap_response_info->rsp_search_result;
  session->imap_response_info->rsp_search_result = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_UID_SEARCH;
  }
}

LIBETPAN_EXPORT
void mailimap_search_result_free(clist * search_result)
{
  clist_foreach(search_result, (clist_func) free, NULL);
  clist_free(search_result);
}

LIBETPAN_EXPORT
int
mailimap_select(mailimap * session, const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_select_send(session->imap_stream, mb);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_crlf_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  if (mailstream_flush(session->imap_stream) == -1)
    return MAILIMAP_ERROR_STREAM;

  if (read_line(session) == NULL)
    return MAILIMAP_ERROR_STREAM;

  if (session->imap_selection_info != NULL)
    mailimap_selection_info_free(session->imap_selection_info);
  session->imap_selection_info = mailimap_selection_info_new();

  r = parse_response(session, &response);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    session->imap_state = MAILIMAP_STATE_SELECTED;
    return MAILIMAP_NO_ERROR;

  default:
    mailimap_selection_info_free(session->imap_selection_info);
    session->imap_selection_info = NULL;
    session->imap_state = MAILIMAP_STATE_AUTHENTICATED;
    return MAILIMAP_ERROR_SELECT;
  }
}

LIBETPAN_EXPORT
int
mailimap_status(mailimap * session, const char * mb,
    struct mailimap_status_att_list * status_att_list,
    struct mailimap_mailbox_data_status ** result)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_status_send(session->imap_stream, mb, status_att_list);
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

  * result = session->imap_response_info->rsp_status;
  session->imap_response_info->rsp_status = NULL;

  error_code = response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type;

  mailimap_response_free(response);

  switch (error_code) {
  case MAILIMAP_RESP_COND_STATE_OK:
    return MAILIMAP_NO_ERROR;

  default:
    return MAILIMAP_ERROR_STATUS;
  }
}


LIBETPAN_EXPORT
int
mailimap_store(mailimap * session,
	       struct mailimap_set * set,
	       struct mailimap_store_att_flags * store_att_flags)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_store_send(session->imap_stream, set, store_att_flags);
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
    return MAILIMAP_ERROR_STORE;
  }
}

LIBETPAN_EXPORT
int
mailimap_uid_store(mailimap * session,
		   struct mailimap_set * set,
		   struct mailimap_store_att_flags * store_att_flags)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if (session->imap_state != MAILIMAP_STATE_SELECTED)
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_uid_store_send(session->imap_stream, set, store_att_flags);
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
    return MAILIMAP_ERROR_UID_STORE;
  }
}

LIBETPAN_EXPORT
int mailimap_subscribe(mailimap * session, const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
	return r;

  r = mailimap_subscribe_send(session->imap_stream, mb);
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
    return MAILIMAP_ERROR_SUBSCRIBE;
  }
}

LIBETPAN_EXPORT
int mailimap_unsubscribe(mailimap * session, const char * mb)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  if ((session->imap_state != MAILIMAP_STATE_AUTHENTICATED) &&
      (session->imap_state != MAILIMAP_STATE_SELECTED))
    return MAILIMAP_ERROR_BAD_STATE;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_unsubscribe_send(session->imap_stream, mb);
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
    return MAILIMAP_ERROR_UNSUBSCRIBE;
  }
}


LIBETPAN_EXPORT
int mailimap_starttls(mailimap * session)
{
  struct mailimap_response * response;
  int r;
  int error_code;

  r = send_current_tag(session);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_starttls_send(session->imap_stream);
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
    return MAILIMAP_ERROR_STARTTLS;
  }
}



char * read_line(mailimap * session)
{
  return mailstream_read_line(session->imap_stream, session->imap_stream_buffer);
}

int send_current_tag(mailimap * session)
{
  char tag_str[15];
  int r;
  
  session->imap_tag ++;
  snprintf(tag_str, 15, "%i", session->imap_tag);

  r = mailimap_tag_send(session->imap_stream, tag_str);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(session->imap_stream);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int parse_response(mailimap * session,
	 			struct mailimap_response ** result)
{
  size_t index;
  struct mailimap_response * response;
  char tag_str[15];
  int r;
  
  index = 0;

  session->imap_response = NULL;

  r = mailimap_response_parse(session->imap_stream,
      session->imap_stream_buffer,
      &index, &response,
      session->imap_progr_rate, session->imap_progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

#if 0
  mailimap_response_print(response);
#endif

  response_store(session, response);
  
  if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_text != NULL) {
    if (mmap_string_assign(session->imap_response_buffer,
            response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_text->rsp_text)
        == NULL)
      return MAILIMAP_ERROR_MEMORY;
  }

  session->imap_response = session->imap_response_buffer->str;

  if (response->rsp_resp_done->rsp_type == MAILIMAP_RESP_DONE_TYPE_FATAL)
    return MAILIMAP_ERROR_FATAL;

  snprintf(tag_str, 15, "%i", session->imap_tag);
  if (strcmp(response->rsp_resp_done->rsp_data.rsp_tagged->rsp_tag, tag_str) != 0)
    return MAILIMAP_ERROR_PROTOCOL;
  
  if (response->rsp_resp_done->rsp_data.rsp_tagged->rsp_cond_state->rsp_type ==
      MAILIMAP_RESP_COND_STATE_BAD)
    return MAILIMAP_ERROR_PROTOCOL;

  * result = response;

  return MAILIMAP_NO_ERROR;
}


static int parse_greeting(mailimap * session,
	 			struct mailimap_greeting ** result)
{
  size_t index;
  struct mailimap_greeting * greeting;
  int r;
  
  index = 0;
  
  session->imap_response = NULL;

  r = mailimap_greeting_parse(session->imap_stream,
      session->imap_stream_buffer,
      &index, &greeting, session->imap_progr_rate,
      session->imap_progr_fun);
  if (r != MAILIMAP_NO_ERROR)
    return r;

#if 0
  mailimap_greeting_print(greeting);
#endif

  greeting_store(session, greeting);

  if (greeting->gr_type == MAILIMAP_GREETING_RESP_COND_BYE) {
    if (mmap_string_assign(session->imap_response_buffer,
            greeting->gr_data.gr_bye->rsp_text->rsp_text) == NULL)
      return MAILIMAP_ERROR_MEMORY;

    session->imap_response = session->imap_response_buffer->str;
    
    return MAILIMAP_ERROR_DONT_ACCEPT_CONNECTION;
  }

  if (mmap_string_assign(session->imap_response_buffer,
          greeting->gr_data.gr_auth->rsp_text->rsp_text) == NULL)
    return MAILIMAP_ERROR_MEMORY;

  session->imap_response = session->imap_response_buffer->str;

  * result = greeting;

  return MAILIMAP_NO_ERROR;
}


LIBETPAN_EXPORT
mailimap * mailimap_new(size_t imap_progr_rate,
    progress_function * imap_progr_fun)
{
  mailimap * f;

  f = malloc(sizeof(* f));
  if (f == NULL)
    goto err;

  f->imap_response = NULL;
  
  f->imap_stream = NULL;

  f->imap_progr_rate = imap_progr_rate;
  f->imap_progr_fun = imap_progr_fun;

  f->imap_stream_buffer = mmap_string_new("");
  if (f->imap_stream_buffer == NULL)
    goto free_f;

  f->imap_response_buffer = mmap_string_new("");
  if (f->imap_response_buffer == NULL)
    goto free_stream_buffer;

  f->imap_state = MAILIMAP_STATE_DISCONNECTED;
  f->imap_tag = 0;

  f->imap_selection_info = NULL;
  f->imap_response_info = NULL;
  f->imap_connection_info = NULL;

#ifdef USE_SASL
  f->imap_sasl.sasl_conn = NULL;
#endif
  
  return f;

 free_stream_buffer:
  mmap_string_free(f->imap_stream_buffer);
 free_f:
  free(f);
 err:
  return NULL;
}

LIBETPAN_EXPORT
void mailimap_free(mailimap * session)
{
#ifdef USE_SASL
  if (session->imap_sasl.sasl_conn != NULL) {
    sasl_dispose((sasl_conn_t **) &session->imap_sasl.sasl_conn);
    mailsasl_unref();
  }
#endif
  
  if (session->imap_stream)
    mailimap_logout(session);

  mmap_string_free(session->imap_response_buffer);
  mmap_string_free(session->imap_stream_buffer);

  if (session->imap_response_info)
    mailimap_response_info_free(session->imap_response_info);
  if (session->imap_selection_info)
    mailimap_selection_info_free(session->imap_selection_info);
  if (session->imap_connection_info)
    mailimap_connection_info_free(session->imap_connection_info);

  free(session);
}

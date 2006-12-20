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
 * $Id: mailpop3_types.h,v 1.18 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef MAILPOP3_TYPES_H

#define MAILPOP3_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailstream.h>
#include <libetpan/mmapstring.h>
#include <libetpan/carray.h>
#include <libetpan/clist.h>

enum {
  MAILPOP3_NO_ERROR = 0,
  MAILPOP3_ERROR_BAD_STATE,
  MAILPOP3_ERROR_UNAUTHORIZED,
  MAILPOP3_ERROR_STREAM,
  MAILPOP3_ERROR_DENIED,
  MAILPOP3_ERROR_BAD_USER,
  MAILPOP3_ERROR_BAD_PASSWORD,
  MAILPOP3_ERROR_CANT_LIST,
  MAILPOP3_ERROR_NO_SUCH_MESSAGE,
  MAILPOP3_ERROR_MEMORY,
  MAILPOP3_ERROR_CONNECTION_REFUSED,
  MAILPOP3_ERROR_APOP_NOT_SUPPORTED,
  MAILPOP3_ERROR_CAPA_NOT_SUPPORTED,
  MAILPOP3_ERROR_STLS_NOT_SUPPORTED
};

struct mailpop3
{
  char * pop3_response;               /* response message */
  char * pop3_timestamp;              /* connection timestamp */
  
  /* internals */
  mailstream * pop3_stream;
  size_t pop3_progr_rate;
  progress_function * pop3_progr_fun;

  MMAPString * pop3_stream_buffer;        /* buffer for lines reading */
  MMAPString * pop3_response_buffer;      /* buffer for responses */

  carray * pop3_msg_tab;               /* list of pop3_msg_info structures */
  int pop3_state;                        /* state */

  unsigned int pop3_deleted_count;
  
  struct {
    void * sasl_conn;
    const char * sasl_server_fqdn;
    const char * sasl_login;
    const char * sasl_auth_name;
    const char * sasl_password;
    const char * sasl_realm;
    void * sasl_secret;
  } pop3_sasl;
};

typedef struct mailpop3 mailpop3;

struct mailpop3_msg_info
{
  unsigned int msg_index;
  uint32_t msg_size;
  char * msg_uidl;
  int msg_deleted;
};


struct mailpop3_capa {
  char * cap_name;
  clist * cap_param; /* (char *) */
};

#ifdef __cplusplus
}
#endif

#endif

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
 * $Id: mailsmtp_types.h,v 1.16 2006/10/13 14:24:03 alfie Exp $
 */

#ifndef MAILSMTP_TYPES_H

#define MAILSMTP_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailstream.h>
#include <libetpan/mmapstring.h>

enum {
  MAILSMTP_NO_ERROR = 0,
  MAILSMTP_ERROR_UNEXPECTED_CODE,
  MAILSMTP_ERROR_SERVICE_NOT_AVAILABLE,
  MAILSMTP_ERROR_STREAM,
  MAILSMTP_ERROR_HOSTNAME,
  MAILSMTP_ERROR_NOT_IMPLEMENTED,
  MAILSMTP_ERROR_ACTION_NOT_TAKEN,
  MAILSMTP_ERROR_EXCEED_STORAGE_ALLOCATION,
  MAILSMTP_ERROR_IN_PROCESSING,
  MAILSMTP_ERROR_INSUFFICIENT_SYSTEM_STORAGE,
  MAILSMTP_ERROR_MAILBOX_UNAVAILABLE,
  MAILSMTP_ERROR_MAILBOX_NAME_NOT_ALLOWED,
  MAILSMTP_ERROR_BAD_SEQUENCE_OF_COMMAND,
  MAILSMTP_ERROR_USER_NOT_LOCAL,
  MAILSMTP_ERROR_TRANSACTION_FAILED,
  MAILSMTP_ERROR_MEMORY,
  MAILSMTP_ERROR_AUTH_NOT_SUPPORTED,
  MAILSMTP_ERROR_AUTH_LOGIN,
  MAILSMTP_ERROR_AUTH_REQUIRED,
  MAILSMTP_ERROR_AUTH_TOO_WEAK,
  MAILSMTP_ERROR_AUTH_TRANSITION_NEEDED,
  MAILSMTP_ERROR_AUTH_TEMPORARY_FAILTURE,
  MAILSMTP_ERROR_AUTH_ENCRYPTION_REQUIRED,
  MAILSMTP_ERROR_STARTTLS_TEMPORARY_FAILURE,
  MAILSMTP_ERROR_STARTTLS_NOT_SUPPORTED,
  MAILSMTP_ERROR_CONNECTION_REFUSED,
  MAILSMTP_ERROR_AUTH_AUTHENTICATION_FAILED
};

enum {
  MAILSMTP_AUTH_NOT_CHECKED = 0,
  MAILSMTP_AUTH_CHECKED = 1,
  MAILSMTP_AUTH_CRAM_MD5 = 2,
  MAILSMTP_AUTH_PLAIN = 4,
  MAILSMTP_AUTH_LOGIN = 8,
  MAILSMTP_AUTH_DIGEST_MD5 = 16
};

enum {
  MAILSMTP_ESMTP = 1,
  MAILSMTP_ESMTP_EXPN = 2,
  MAILSMTP_ESMTP_8BITMIME = 4,
  MAILSMTP_ESMTP_SIZE = 8,
  MAILSMTP_ESMTP_ETRN = 16,
  MAILSMTP_ESMTP_STARTTLS = 32,
  MAILSMTP_ESMTP_DSN = 64
};
  
struct mailsmtp {
  mailstream * stream;

  size_t progr_rate;
  progress_function * progr_fun;

  char * response;

  MMAPString * line_buffer;
  MMAPString * response_buffer;

  int esmtp;		/* contains flags MAILSMTP_ESMTP_* */
  int auth;             /* contains flags MAILSMTP_AUTH_* */
  
  struct {
    void * sasl_conn;
    const char * sasl_server_fqdn;
    const char * sasl_login;
    const char * sasl_auth_name;
    const char * sasl_password;
    const char * sasl_realm;
    void * sasl_secret;
  } smtp_sasl;
};

typedef struct mailsmtp mailsmtp;

#define MAILSMTP_DSN_NOTIFY_SUCCESS 1
#define MAILSMTP_DSN_NOTIFY_FAILURE 2
#define MAILSMTP_DSN_NOTIFY_DELAY   4
#define MAILSMTP_DSN_NOTIFY_NEVER   8

struct esmtp_address {
  char * address;
  int notify;
  char * orcpt;
};

#ifdef __cplusplus
}
#endif

#endif

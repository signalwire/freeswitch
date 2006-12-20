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
 * $Id: pop3driver_types.h,v 1.9 2006/05/22 13:39:40 hoa Exp $
 */

#ifndef POP3DRIVER_TYPES_H

#define POP3DRIVER_TYPES_H

#include <libetpan/libetpan-config.h>

#include <libetpan/maildriver_types.h>
#include <libetpan/mailpop3.h>
#include <libetpan/maildriver_types.h>
#include <libetpan/chash.h>
#include <libetpan/mailstorage_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* POP3 driver for session */

enum {
  POP3DRIVER_SET_AUTH_TYPE = 1
};

enum {
  POP3DRIVER_AUTH_TYPE_PLAIN = 0,
  POP3DRIVER_AUTH_TYPE_APOP,
  POP3DRIVER_AUTH_TYPE_TRY_APOP
};

struct pop3_session_state_data {
  int pop3_auth_type;
  mailpop3 * pop3_session;
};

/* cached POP3 driver for session */

enum {
  /* the mapping of the parameters should be the same as for pop3 */
  POP3DRIVER_CACHED_SET_AUTH_TYPE = 1,
  /* cache specific */
  POP3DRIVER_CACHED_SET_CACHE_DIRECTORY,
  POP3DRIVER_CACHED_SET_FLAGS_DIRECTORY
};

struct pop3_cached_session_state_data {
  mailsession * pop3_ancestor;
  char pop3_cache_directory[PATH_MAX];
  char pop3_flags_directory[PATH_MAX];
  chash * pop3_flags_hash;
  carray * pop3_flags_array;
  struct mail_flags_store * pop3_flags_store;
};

/* pop3 storage */

/*
  pop3_mailstorage is the state data specific to the POP3 storage.

  - servername  this is the name of the POP3 server

  - port is the port to connect to, on the server.
      you give 0 to use the default port.

  - connection_type is the type of socket layer to use.
      The value can be CONNECTION_TYPE_PLAIN, CONNECTION_TYPE_STARTTLS,
      CONNECTION_TYPE_TRY_STARTTLS or CONNECTION_TYPE_TLS.
    
  - auth_type is the authenticate mechanism to use.
      The value can be POP3_AUTH_TYPE_PLAIN, POP3_AUTH_TYPE_APOP
      or POP3_AUTH_TYPE_TRY_APOP. Other values are not yet implemented.

  - login is the login of the POP3 account.

  - password is the password of the POP3 account.

  - cached if this value is != 0, a persistant cache will be
      stored on local system.
  
  - cache_directory is the location of the cache.

  - flags_directory is the location of the flags.
*/

struct pop3_mailstorage {
  char * pop3_servername;
  uint16_t pop3_port;
  char * pop3_command;
  int pop3_connection_type;

  int pop3_auth_type;
  char * pop3_login; /* deprecated */
  char * pop3_password; /* deprecated */

  int pop3_cached;
  char * pop3_cache_directory;
  char * pop3_flags_directory;
  
  struct {
    int sasl_enabled;
    char * sasl_auth_type;
    char * sasl_server_fqdn;
    char * sasl_local_ip_port;
    char * sasl_remote_ip_port;
    char * sasl_login;
    char * sasl_auth_name;
    char * sasl_password;
    char * sasl_realm;
  } pop3_sasl;
};

/* this is the type of POP3 authentication */

enum {
  POP3_AUTH_TYPE_PLAIN,             /* plain text authentication */
  POP3_AUTH_TYPE_APOP,              /* APOP authentication */
  POP3_AUTH_TYPE_TRY_APOP,          /* first, try APOP, if it fails,
                                       try plain text */
  POP3_AUTH_TYPE_SASL_ANONYMOUS,    /* SASL anonymous */
  POP3_AUTH_TYPE_SASL_CRAM_MD5,     /* SASL CRAM MD5 */
  POP3_AUTH_TYPE_SASL_KERBEROS_V4,  /* SASL KERBEROS V4 */
  POP3_AUTH_TYPE_SASL_PLAIN,        /* SASL plain */
  POP3_AUTH_TYPE_SASL_SCRAM_MD5,    /* SASL SCRAM MD5 */
  POP3_AUTH_TYPE_SASL_GSSAPI,       /* SASL GSSAPI */
  POP3_AUTH_TYPE_SASL_DIGEST_MD5    /* SASL digest MD5 */
};

#ifdef __cplusplus
}
#endif

#endif

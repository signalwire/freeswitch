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
 * $Id: pop3storage.h,v 1.11 2006/06/02 15:44:30 smarinier Exp $
 */

#ifndef POP3STORAGE_H

#define POP3STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/pop3driver_types.h>
#include <libetpan/pop3driver.h>
#include <libetpan/pop3driver_cached.h>

/*
  pop3_mailstorage_init is the constructor for a POP3 storage

  @param storage this is the storage to initialize.

  @param servername  this is the name of the POP3 server
  
  @param port is the port to connect to, on the server.
    you give 0 to use the default port.

  @param command the command used to connect to the server instead of
    allowing normal TCP connections to be used.
    
  @param connection_type is the type of socket layer to use.
    The value can be CONNECTION_TYPE_PLAIN, CONNECTION_TYPE_STARTTLS,
    CONNECTION_TYPE_TRY_STARTTLS, CONNECTION_TYPE_TLS,
    CONNECTION_TYPE_COMMAND, CONNECTION_TYPE_COMMAND_STARTTLS,
    CONNECTION_TYPE_COMMAND_TRY_STARTTLS, CONNECTION_TYPE_COMMAND_TLS,.
    
  @param auth_type is the authenticate mechanism to use.
    The value can be POP3_AUTH_TYPE_PLAIN, POP3_AUTH_TYPE_APOP
    or POP3_AUTH_TYPE_TRY_APOP. Other values are not yet implemented.

  @param login is the login of the POP3 account.

  @param password is the password of the POP3 account.

  @param cached if this value is != 0, a persistant cache will be
    stored on local system.

  @param cache_directory is the location of the cache

  @param flags_directory is the location of the flags
*/

LIBETPAN_EXPORT
int pop3_mailstorage_init(struct mailstorage * storage,
    const char * pop3_servername, uint16_t pop3_port,
    const char * pop3_command,
    int pop3_connection_type, int pop3_auth_type,
    const char * pop3_login, const char * pop3_password,
    int pop3_cached, const char * pop3_cache_directory,
    const char * pop3_flags_directory);

LIBETPAN_EXPORT
int pop3_mailstorage_init_sasl(struct mailstorage * storage,
    const char * pop3_servername, uint16_t pop3_port,
    const char * pop3_command,
    int pop3_connection_type,
    const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm,
    int pop3_cached, const char * pop3_cache_directory,
    const char * pop3_flags_directory);

#ifdef __cplusplus
}
#endif

#endif

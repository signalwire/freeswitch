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
 * $Id: maildriver_tools.h,v 1.15 2006/06/07 15:10:01 smarinier Exp $
 */

#ifndef MAILDRIVER_TOOLS_H

#define MAILDRIVER_TOOLS_H

#include "maildriver_types.h"
#include "mail_cache_db_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int
maildriver_generic_get_envelopes_list(mailsession * session,
    struct mailmessage_list * env_list);

#if 0
int maildriver_generic_search_messages(mailsession * session, char * charset,
    struct mail_search_key * key,
    struct mail_search_result ** result);
#endif

int
maildriver_env_list_to_msg_list(struct mailmessage_list * env_list,
    clist ** result);

int maildriver_imf_error_to_mail_error(int error);

char * maildriver_quote_mailbox(const char * mb);

int
maildriver_env_list_to_msg_list_no_flags(struct mailmessage_list * env_list,
    clist ** result);

int maildriver_cache_clean_up(struct mail_cache_db * cache_db_env,
    struct mail_cache_db * cache_db_flags,
    struct mailmessage_list * env_list);

int maildriver_message_cache_clean_up(char * cache_dir,
    struct mailmessage_list * env_list,
    void (* get_uid_from_filename)(char *));

#ifdef __cplusplus
}
#endif

#endif

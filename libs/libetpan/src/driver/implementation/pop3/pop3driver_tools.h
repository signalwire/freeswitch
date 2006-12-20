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
 * $Id: pop3driver_tools.h,v 1.10 2004/11/21 21:53:34 hoa Exp $
 */

#ifndef POP3DRIVER_TOOLS_H

#define POP3DRIVER_TOOLS_H

#include "mail_cache_db_types.h"
#include "pop3driver_types.h"
#include "mailpop3.h"

#ifdef __cplusplus
extern "C" {
#endif

int pop3driver_pop3_error_to_mail_error(int error);

int pop3driver_retr(mailsession * session, uint32_t index,
		    char ** result, size_t * result_len);

int pop3driver_header(mailsession * session, uint32_t index,
		      char ** result,
		      size_t * result_len);

int pop3driver_size(mailsession * session, uint32_t index,
		    size_t * result);

int
pop3driver_get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session,
    uint32_t num, 
    struct mail_flags ** result);

int
pop3driver_write_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * uid,
    struct mail_flags * flags);

int pop3_get_messages_list(mailpop3 * pop3,
			   mailsession * session,
			   mailmessage_driver * driver,
			   struct mailmessage_list ** result);

#ifdef __cplusplus
}
#endif

#endif

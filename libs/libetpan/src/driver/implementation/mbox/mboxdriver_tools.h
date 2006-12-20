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
 * $Id: mboxdriver_tools.h,v 1.10 2004/11/21 21:53:32 hoa Exp $
 */

#ifndef MBOXDRIVER_TOOLS_H

#define MBOXDRIVER_TOOLS_H

#include "mail_cache_db_types.h"
#include "mboxdriver_types.h"
#include "mailmbox.h"

#ifdef __cplusplus
extern "C" {
#endif

int mboxdriver_mbox_error_to_mail_error(int error);

int mboxdriver_fetch_msg(mailsession * session, uint32_t index,
			 char ** result, size_t * result_len);

int mboxdriver_fetch_size(mailsession * session, uint32_t index,
			  size_t * result);

int
mboxdriver_get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session,
    uint32_t num,
    struct mail_flags ** result);

int
mboxdriver_write_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    char * uid, struct mail_flags * flags);

int mbox_get_uid_messages_list(struct mailmbox_folder * folder,
    mailsession * session, 
    mailmessage_driver * driver,
    struct mailmessage_list ** result);

int mbox_get_messages_list(struct mailmbox_folder * folder,
			   mailsession * session, 
			   mailmessage_driver * driver,
			   struct mailmessage_list ** result);

int mboxdriver_fetch_header(mailsession * session, uint32_t index,
			    char ** result, size_t * result_len);

#ifdef __cplusplus
}
#endif

#endif

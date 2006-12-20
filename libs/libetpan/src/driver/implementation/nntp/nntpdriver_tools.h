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
 * $Id: nntpdriver_tools.h,v 1.12 2006/06/07 15:10:01 smarinier Exp $
 */

#ifndef NNTPDRIVER_TOOLS_H

#define NNTPDRIVER_TOOLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mail_cache_db_types.h"
#include "nntpdriver_types.h"

int nntpdriver_nntp_error_to_mail_error(int error);

int nntpdriver_authenticate_password(mailsession * session);

int nntpdriver_authenticate_user(mailsession * session);

int nntpdriver_article(mailsession * session, uint32_t index,
		       char ** result, size_t * result_len);

int nntpdriver_head(mailsession * session, uint32_t index,
		    char ** result,
		    size_t * result_len);

int nntpdriver_size(mailsession * session, uint32_t index,
		    size_t * result);

int
nntpdriver_get_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    uint32_t num, 
    struct mail_flags ** result);
  
int
nntpdriver_write_cached_flags(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    uint32_t num,
    struct mail_flags * flags);

int nntpdriver_select_folder(mailsession * session, const char * mb);

int nntp_get_messages_list(mailsession * nntp_session,
			   mailsession * session,
			   mailmessage_driver * driver,
			   struct mailmessage_list ** result);

int nntpdriver_mode_reader(mailsession * session);

#ifdef __cplusplus
}
#endif

#endif

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
 * $Id: mailimap_sender.h,v 1.14 2006/10/20 00:13:30 hoa Exp $
 */

#ifndef MAILIMAP_SENDER_H

#define MAILIMAP_SENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mailimap_types.h"

int mailimap_append_send(mailstream * fd,
			 const char * mailbox,
			 struct mailimap_flag_list * flag_list,
			 struct mailimap_date_time * date_time,
			 size_t literal_size);

int mailimap_authenticate_send(mailstream * fd,
				const char * auth_type);

int mailimap_authenticate_resp_send(mailstream * fd,
				const char * base64);

int mailimap_noop_send(mailstream * fd);

int mailimap_logout_send(mailstream * fd);

int mailimap_capability_send(mailstream * fd);

int mailimap_check_send(mailstream * fd);

int mailimap_close_send(mailstream * fd);

int mailimap_expunge_send(mailstream * fd);

int mailimap_copy_send(mailstream * fd,
				struct mailimap_set * set,
				const char * mb);

int mailimap_uid_copy_send(mailstream * fd,
				struct mailimap_set * set,
				const char * mb);

int mailimap_create_send(mailstream * fd,
				const char * mb);


int mailimap_delete_send(mailstream * fd, const char * mb);

int mailimap_examine_send(mailstream * fd, const char * mb);

int
mailimap_fetch_send(mailstream * fd,
		    struct mailimap_set * set,
		    struct mailimap_fetch_type * fetch_type);

int
mailimap_uid_fetch_send(mailstream * fd,
			struct mailimap_set * set,
			struct mailimap_fetch_type * fetch_type);

int mailimap_list_send(mailstream * fd,
				const char * mb, const char * list_mb);

int mailimap_login_send(mailstream * fd,
   				const char * userid, const char * password);

int mailimap_lsub_send(mailstream * fd,
				const char * mb, const char * list_mb);

int mailimap_rename_send(mailstream * fd, const char * mb,
			      const char * new_name);

int
mailimap_search_send(mailstream * fd, const char * charset,
		     struct mailimap_search_key * key);

int
mailimap_uid_search_send(mailstream * fd, const char * charset,
			 struct mailimap_search_key * key);

int
mailimap_select_send(mailstream * fd, const char * mb);

int
mailimap_status_send(mailstream * fd, const char * mb,
		     struct mailimap_status_att_list * status_att_list);

int
mailimap_store_send(mailstream * fd,
		    struct mailimap_set * set,
		    struct mailimap_store_att_flags * store_att_flags);

int
mailimap_uid_store_send(mailstream * fd,
			struct mailimap_set * set,
			struct mailimap_store_att_flags * store_att_flags);

int mailimap_subscribe_send(mailstream * fd, const char * mb);


int mailimap_tag_send(mailstream * fd, const char * tag);

int mailimap_unsubscribe_send(mailstream * fd,
				const char * mb);

int mailimap_crlf_send(mailstream * fd);

int mailimap_space_send(mailstream * fd);

int
mailimap_literal_send(mailstream * fd, const char * literal,
		      size_t progr_rate,
		      progress_function * progr_fun);

int
mailimap_literal_count_send(mailstream * fd, uint32_t count);

int
mailimap_literal_data_send(mailstream * fd, const char * literal, uint32_t len,
			   size_t progr_rate,
			   progress_function * progr_fun);

int mailimap_starttls_send(mailstream * fd);

int mailimap_token_send(mailstream * fd, const char * atom);

int mailimap_quoted_send(mailstream * fd, const char * quoted);

typedef int mailimap_struct_sender(mailstream * fd, void * data);

int
mailimap_struct_spaced_list_send(mailstream * fd, clist * list,
				 mailimap_struct_sender * sender);

int
mailimap_list_mailbox_send(mailstream * fd, const char * pattern);

int mailimap_char_send(mailstream * fd, char ch);

int mailimap_mailbox_send(mailstream * fd, const char * mb);

int mailimap_astring_send(mailstream * fd, const char * astring);

int mailimap_set_send(mailstream * fd,
    struct mailimap_set * set);

#ifdef __cplusplus
}
#endif

#endif

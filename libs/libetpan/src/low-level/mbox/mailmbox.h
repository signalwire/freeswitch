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
 * $Id: mailmbox.h,v 1.19 2004/11/21 21:53:38 hoa Exp $
 */

#ifndef MAILMBOX_H

#define MAILMBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailmbox_types.h>

int
mailmbox_append_message_list(struct mailmbox_folder * folder,
			     carray * append_tab);

int
mailmbox_append_message(struct mailmbox_folder * folder,
			const char * data, size_t len);

int
mailmbox_append_message_uid(struct mailmbox_folder * folder,
    const char * data, size_t len, unsigned int * puid);

int mailmbox_fetch_msg(struct mailmbox_folder * folder,
		       uint32_t num, char ** result,
		       size_t * result_len);

int mailmbox_fetch_msg_headers(struct mailmbox_folder * folder,
			       uint32_t num, char ** result,
			       size_t * result_len);

void mailmbox_fetch_result_free(char * msg);

int mailmbox_copy_msg_list(struct mailmbox_folder * dest_folder,
			   struct mailmbox_folder * src_folder,
			   carray * tab);

int mailmbox_copy_msg(struct mailmbox_folder * dest_folder,
		      struct mailmbox_folder * src_folder,
		      uint32_t uid);

int mailmbox_expunge(struct mailmbox_folder * folder);

int mailmbox_delete_msg(struct mailmbox_folder * folder, uint32_t uid);

int mailmbox_init(const char * filename,
		  int force_readonly,
		  int force_no_uid,
		  uint32_t default_written_uid,
		  struct mailmbox_folder ** result_folder);

void mailmbox_done(struct mailmbox_folder * folder);

/* low-level access primitives */

int mailmbox_write_lock(struct mailmbox_folder * folder);

int mailmbox_write_unlock(struct mailmbox_folder * folder);

int mailmbox_read_lock(struct mailmbox_folder * folder);

int mailmbox_read_unlock(struct mailmbox_folder * folder);


/* memory map */

int mailmbox_map(struct mailmbox_folder * folder);

void mailmbox_unmap(struct mailmbox_folder * folder);

void mailmbox_sync(struct mailmbox_folder * folder);


/* open & close file */

int mailmbox_open(struct mailmbox_folder * folder);

void mailmbox_close(struct mailmbox_folder * folder);


/* validate cache */

int mailmbox_validate_write_lock(struct mailmbox_folder * folder);

int mailmbox_validate_read_lock(struct mailmbox_folder * folder);


/* fetch message */

int mailmbox_fetch_msg_no_lock(struct mailmbox_folder * folder,
			       uint32_t num, char ** result,
			       size_t * result_len);

int mailmbox_fetch_msg_headers_no_lock(struct mailmbox_folder * folder,
				       uint32_t num, char ** result,
				       size_t * result_len);

/* append message */

int
mailmbox_append_message_list_no_lock(struct mailmbox_folder * folder,
				     carray * append_tab);

int mailmbox_expunge_no_lock(struct mailmbox_folder * folder);

#ifdef __cplusplus
}
#endif

#endif

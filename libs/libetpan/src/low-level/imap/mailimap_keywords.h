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
 * $Id: mailimap_keywords.h,v 1.8 2004/11/21 21:53:36 hoa Exp $
 */

#ifndef MAILIMAP_COMMON_H

#define MAILIMAP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mailstream.h"


/* tools */

int mailimap_char_parse(mailstream * fd, MMAPString * buffer,
			size_t * index, char token);

int mailimap_space_parse(mailstream * fd, MMAPString * buffer,
			 size_t * index);

/* tokens */

int mailimap_token_case_insensitive_parse(mailstream * fd,
					  MMAPString * buffer,
					  size_t * index,
					  const char * token);

int mailimap_status_att_get_token_value(mailstream * fd, MMAPString * buffer,
					size_t * index);
const char * mailimap_status_att_get_token_str(size_t index);


int mailimap_month_get_token_value(mailstream * fd, MMAPString * buffer,
				   size_t * index);
const char * mailimap_month_get_token_str(size_t index);


int mailimap_flag_get_token_value(mailstream * fd, MMAPString * buffer,
				  size_t * index);

const char * mailimap_flag_get_token_str(size_t index);

int mailimap_encoding_get_token_value(mailstream * fd, MMAPString * buffer,
				      size_t * index);

int mailimap_mbx_list_sflag_get_token_value(mailstream * fd,
					    MMAPString * buffer,
					    size_t * index);

int mailimap_media_basic_get_token_value(mailstream * fd, MMAPString * buffer,
					 size_t * index);

int mailimap_resp_cond_state_get_token_value(mailstream * fd,
					     MMAPString * buffer,
					     size_t * index);

int mailimap_resp_text_code_1_get_token_value(mailstream * fd,
					      MMAPString * buffer,
					      size_t * index);

int mailimap_resp_text_code_2_get_token_value(mailstream * fd,
					      MMAPString * buffer,
					      size_t * index);

int mailimap_section_msgtext_get_token_value(mailstream * fd,
					     MMAPString * buffer,
					     size_t * index);

#ifdef __cplusplus
}
#endif

#endif

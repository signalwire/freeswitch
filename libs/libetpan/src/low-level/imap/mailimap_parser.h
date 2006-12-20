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
 * $Id: mailimap_parser.h,v 1.10 2006/10/20 00:13:30 hoa Exp $
 */

#ifndef MAILIMAP_PARSER_H

#define MAILIMAP_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mailimap_types.h"

int mailimap_greeting_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_greeting ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);

int
mailimap_response_parse(mailstream * fd, MMAPString * buffer,
			size_t * index, struct mailimap_response ** result,
			size_t progr_rate,
			progress_function * progr_fun);

int
mailimap_continue_req_parse(mailstream * fd, MMAPString * buffer,
			    size_t * index,
			    struct mailimap_continue_req ** result,
			    size_t progr_rate,
			    progress_function * progr_fun);

typedef int mailimap_struct_parser(mailstream * fd, MMAPString * buffer,
				   size_t * index, void * result,
				   size_t progr_rate,
				   progress_function * progr_fun);
typedef void mailimap_struct_destructor(void * result);

int
mailimap_mailbox_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index, char ** result,
		       size_t progr_rate,
		       progress_function * progr_fun);

int mailimap_nstring_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index, char ** result,
				  size_t * result_len,
				  size_t progr_rate,
				  progress_function * progr_fun);

int
mailimap_string_parse(mailstream * fd, MMAPString * buffer,
		      size_t * index, char ** result,
		      size_t * result_len,
		      size_t progr_rate,
		      progress_function * progr_fun);

int
mailimap_struct_spaced_list_parse(mailstream * fd, MMAPString * buffer,
				  size_t * index, clist ** result,
				  mailimap_struct_parser * parser,
				  mailimap_struct_destructor * destructor,
				  size_t progr_rate,
				  progress_function * progr_fun);

int mailimap_oparenth_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index);

int mailimap_cparenth_parse(mailstream * fd, MMAPString * buffer,
				   size_t * index);

int
mailimap_astring_parse(mailstream * fd, MMAPString * buffer,
		       size_t * index,
		       char ** result,
		       size_t progr_rate,
		       progress_function * progr_fun);

int
mailimap_nz_number_parse(mailstream * fd, MMAPString * buffer,
			 size_t * index, uint32_t * result);

int
mailimap_struct_list_parse(mailstream * fd, MMAPString * buffer,
    size_t * index, clist ** result,
    char symbol,
    mailimap_struct_parser * parser,
    mailimap_struct_destructor * destructor,
    size_t progr_rate,
    progress_function * progr_fun);

int mailimap_uniqueid_parse(mailstream * fd, MMAPString * buffer,
    size_t * index, uint32_t * result);

int mailimap_colon_parse(mailstream * fd, MMAPString * buffer,
    size_t * index);

#ifdef __cplusplus
}
#endif

#endif

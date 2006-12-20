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
 * $Id: mailmessage_tools.h,v 1.7 2004/11/21 21:53:35 hoa Exp $
 */

#ifndef MAILMESSAGE_TOOLS_H

#define MAILMESSAGE_TOOLS_H

#include "mailmessage_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int
mailmessage_generic_initialize(mailmessage *
				msg_info);

void mailmessage_generic_uninitialize(mailmessage *
				      msg_info);

void mailmessage_generic_flush(mailmessage * msg_info);

void mailmessage_generic_fetch_result_free(mailmessage * msg_info,
					   char * msg);

int mailmessage_generic_fetch(mailmessage * msg_info,
			      char ** result,
			      size_t * result_len);

int mailmessage_generic_fetch_header(mailmessage * msg_info,
				      char ** result,
				      size_t * result_len);

int mailmessage_generic_fetch_body(mailmessage * msg_info,
				    char ** result, size_t * result_len);

int mailmessage_generic_get_bodystructure(mailmessage *
					   msg_info,
					   struct mailmime ** result);

int
mailmessage_generic_fetch_section(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result, size_t * result_len);

int
mailmessage_generic_fetch_section_header(mailmessage * msg_info,
					  struct mailmime * mime,
					  char ** result,
					  size_t * result_len);

int
mailmessage_generic_fetch_section_mime(mailmessage * msg_info,
					struct mailmime * mime,
					char ** result,
					size_t * result_len);

int
mailmessage_generic_fetch_section_body(mailmessage * msg_info,
					struct mailmime * mime,
					char ** result,
					size_t * result_len);

int mailmessage_generic_fetch_envelope(mailmessage * msg_info,
				       struct mailimf_fields ** result);

#ifdef __cplusplus
}
#endif

#endif

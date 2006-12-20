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
 * $Id: mailmime_types_helper.h,v 1.16 2005/06/01 12:22:19 smarinier Exp $
 */

#ifndef MAILMIME_TYPES_HELPER_H

#define MAILMIME_TYPES_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailmime_types.h>

LIBETPAN_EXPORT
int mailmime_transfer_encoding_get(struct mailmime_fields * fields);

LIBETPAN_EXPORT
struct mailmime_disposition *
mailmime_disposition_new_filename(int type, char * filename);

LIBETPAN_EXPORT
struct mailmime_fields * mailmime_fields_new_empty(void);

LIBETPAN_EXPORT
int mailmime_fields_add(struct mailmime_fields * fields,
			struct mailmime_field * field);

LIBETPAN_EXPORT
struct mailmime_fields *
mailmime_fields_new_with_data(struct mailmime_mechanism * encoding,
			      char * id,
			      char * description,
			      struct mailmime_disposition * disposition,
			      struct mailmime_language * language);

LIBETPAN_EXPORT
struct mailmime_fields *
mailmime_fields_new_with_version(struct mailmime_mechanism * encoding,
				 char * id,
				 char * description,
				 struct mailmime_disposition * disposition,
				 struct mailmime_language * language);

LIBETPAN_EXPORT
struct mailmime_content * mailmime_get_content_message(void);
LIBETPAN_EXPORT
struct mailmime_content * mailmime_get_content_text(void);
/* struct mailmime_content * mailmime_get_content(char * mime_type); */

#define mailmime_get_content mailmime_content_new_with_str

LIBETPAN_EXPORT
struct mailmime_data *
mailmime_data_new_data(int encoding, int encoded,
		       const char * data, size_t length);

LIBETPAN_EXPORT
struct mailmime_data *
mailmime_data_new_file(int encoding, int encoded,
		       char * filename);

#if 0
struct mailmime *
mailmime_new_message_file(char * filename);

struct mailmime *
mailmime_new_message_text(char * data_str, size_t length);
#endif

LIBETPAN_EXPORT
struct mailmime *
mailmime_new_message_data(struct mailmime * msg_mime);

LIBETPAN_EXPORT
struct mailmime *
mailmime_new_empty(struct mailmime_content * content,
		   struct mailmime_fields * mime_fields);

LIBETPAN_EXPORT
int
mailmime_new_with_content(const char * content_type,
			  struct mailmime_fields * mime_fields,
			  struct mailmime ** result);

LIBETPAN_EXPORT
int mailmime_set_preamble_file(struct mailmime * build_info,
			       char * filename);

LIBETPAN_EXPORT
int mailmime_set_epilogue_file(struct mailmime * build_info,
			       char * filename);

LIBETPAN_EXPORT
int mailmime_set_preamble_text(struct mailmime * build_info,
			       char * data_str, size_t length);

LIBETPAN_EXPORT
int mailmime_set_epilogue_text(struct mailmime * build_info,
			       char * data_str, size_t length);

LIBETPAN_EXPORT
int mailmime_set_body_file(struct mailmime * build_info,
			   char * filename);

LIBETPAN_EXPORT
int mailmime_set_body_text(struct mailmime * build_info,
			   char * data_str, size_t length);

LIBETPAN_EXPORT
int mailmime_add_part(struct mailmime * build_info,
		      struct mailmime * part);

LIBETPAN_EXPORT
void mailmime_remove_part(struct mailmime * mime);

LIBETPAN_EXPORT
void mailmime_set_imf_fields(struct mailmime * build_info,
    struct mailimf_fields * fields);


LIBETPAN_EXPORT
struct mailmime_disposition *
mailmime_disposition_new_with_data(int type,
    char * filename, char * creation_date, char * modification_date,
    char * read_date, size_t size);

LIBETPAN_EXPORT
void mailmime_single_fields_init(struct mailmime_single_fields * single_fields,
    struct mailmime_fields * fld_fields,
    struct mailmime_content * fld_content);

LIBETPAN_EXPORT
struct mailmime_single_fields *
mailmime_single_fields_new(struct mailmime_fields * fld_fields,
    struct mailmime_content * fld_content);

LIBETPAN_EXPORT
void mailmime_single_fields_free(struct mailmime_single_fields *
    single_fields);

LIBETPAN_EXPORT
int mailmime_smart_add_part(struct mailmime * mime,
    struct mailmime * mime_sub);

LIBETPAN_EXPORT
int mailmime_smart_remove_part(struct mailmime * mime);

LIBETPAN_EXPORT
struct mailmime_content * mailmime_content_new_with_str(const char * str);

LIBETPAN_EXPORT
struct mailmime_fields * mailmime_fields_new_encoding(int type);

LIBETPAN_EXPORT
struct mailmime * mailmime_multiple_new(const char * type);

LIBETPAN_EXPORT
struct mailmime_fields * mailmime_fields_new_filename(int dsp_type,
    char * filename, int encoding_type);

#ifdef __cplusplus
}
#endif

#endif

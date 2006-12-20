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
 * $Id: mailmime_types.h,v 1.32 2006/05/22 13:39:42 hoa Exp $
 */

#ifndef MAILMIME_TYPES_H

#define MAILMIME_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LIBETPAN_CONFIG_H
#	include <libetpan/libetpan-config.h>
#endif

#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif
#include <libetpan/mailimf.h>
#include <libetpan/clist.h>

enum {
  MAILMIME_COMPOSITE_TYPE_ERROR,
  MAILMIME_COMPOSITE_TYPE_MESSAGE,
  MAILMIME_COMPOSITE_TYPE_MULTIPART,
  MAILMIME_COMPOSITE_TYPE_EXTENSION
};

struct mailmime_composite_type {
  int ct_type;
  char * ct_token;
};


struct mailmime_content {
  struct mailmime_type * ct_type;
  char * ct_subtype;
  clist * ct_parameters; /* elements are (struct mailmime_parameter *) */
};


enum {
  MAILMIME_DISCRETE_TYPE_ERROR,
  MAILMIME_DISCRETE_TYPE_TEXT,
  MAILMIME_DISCRETE_TYPE_IMAGE,
  MAILMIME_DISCRETE_TYPE_AUDIO,
  MAILMIME_DISCRETE_TYPE_VIDEO,
  MAILMIME_DISCRETE_TYPE_APPLICATION,
  MAILMIME_DISCRETE_TYPE_EXTENSION
};

struct mailmime_discrete_type {
  int dt_type;
  char * dt_extension;
};

enum {
  MAILMIME_FIELD_NONE,
  MAILMIME_FIELD_TYPE,
  MAILMIME_FIELD_TRANSFER_ENCODING,
  MAILMIME_FIELD_ID,
  MAILMIME_FIELD_DESCRIPTION,
  MAILMIME_FIELD_VERSION,
  MAILMIME_FIELD_DISPOSITION,
  MAILMIME_FIELD_LANGUAGE
};

struct mailmime_field {
  int fld_type;
  union {
    struct mailmime_content * fld_content;
    struct mailmime_mechanism * fld_encoding;
    char * fld_id;
    char * fld_description;
    uint32_t fld_version;
    struct mailmime_disposition * fld_disposition;
    struct mailmime_language * fld_language;
  } fld_data;
};

enum {
  MAILMIME_MECHANISM_ERROR,
  MAILMIME_MECHANISM_7BIT,
  MAILMIME_MECHANISM_8BIT,
  MAILMIME_MECHANISM_BINARY,
  MAILMIME_MECHANISM_QUOTED_PRINTABLE,
  MAILMIME_MECHANISM_BASE64,
  MAILMIME_MECHANISM_TOKEN
};

struct mailmime_mechanism {
  int enc_type;
  char * enc_token;
};


struct mailmime_fields {
  clist * fld_list; /* list of (struct mailmime_field *) */
};


struct mailmime_parameter {
  char * pa_name;
  char * pa_value;
};

enum {
  MAILMIME_TYPE_ERROR,
  MAILMIME_TYPE_DISCRETE_TYPE,
  MAILMIME_TYPE_COMPOSITE_TYPE
};

struct mailmime_type {
  int tp_type;
  union {
    struct mailmime_discrete_type * tp_discrete_type;
    struct mailmime_composite_type * tp_composite_type;
  } tp_data;
};

LIBETPAN_EXPORT
void mailmime_attribute_free(char * attribute);

LIBETPAN_EXPORT
struct mailmime_composite_type *
mailmime_composite_type_new(int ct_type, char * ct_token);

LIBETPAN_EXPORT
void mailmime_composite_type_free(struct mailmime_composite_type * ct);

LIBETPAN_EXPORT
struct mailmime_content *
mailmime_content_new(struct mailmime_type * ct_type,
		     char * ct_subtype,
		     clist * ct_parameters);

LIBETPAN_EXPORT
void mailmime_content_free(struct mailmime_content * content);

LIBETPAN_EXPORT
void mailmime_description_free(char * description);

LIBETPAN_EXPORT
struct mailmime_discrete_type *
mailmime_discrete_type_new(int dt_type, char * dt_extension);

LIBETPAN_EXPORT
void mailmime_discrete_type_free(struct mailmime_discrete_type *
				 discrete_type);

LIBETPAN_EXPORT
void mailmime_encoding_free(struct mailmime_mechanism * encoding);

LIBETPAN_EXPORT
void mailmime_extension_token_free(char * extension);

LIBETPAN_EXPORT
void mailmime_id_free(char * id);

LIBETPAN_EXPORT
struct mailmime_mechanism * mailmime_mechanism_new(int enc_type, char * enc_token);

LIBETPAN_EXPORT
void mailmime_mechanism_free(struct mailmime_mechanism * mechanism);

LIBETPAN_EXPORT
struct mailmime_parameter *
mailmime_parameter_new(char * pa_name, char * pa_value);

LIBETPAN_EXPORT
void mailmime_parameter_free(struct mailmime_parameter * parameter);

LIBETPAN_EXPORT
void mailmime_subtype_free(char * subtype);

LIBETPAN_EXPORT
void mailmime_token_free(char * token);

LIBETPAN_EXPORT
struct mailmime_type *
mailmime_type_new(int tp_type,
		  struct mailmime_discrete_type * tp_discrete_type,
		  struct mailmime_composite_type * tp_composite_type);

LIBETPAN_EXPORT
void mailmime_type_free(struct mailmime_type * type);

LIBETPAN_EXPORT
void mailmime_value_free(char * value);



struct mailmime_language {
  clist * lg_list; /* atom (char *) */
};

LIBETPAN_EXPORT
struct mailmime_language * mailmime_language_new(clist * lg_list);

LIBETPAN_EXPORT
void mailmime_language_free(struct mailmime_language * lang);


/*
void mailmime_x_token_free(gchar * x_token);
*/

LIBETPAN_EXPORT
struct mailmime_field *
mailmime_field_new(int fld_type,
		   struct mailmime_content * fld_content,
		   struct mailmime_mechanism * fld_encoding,
		   char * fld_id,
		   char * fld_description,
		   uint32_t fld_version,
		   struct mailmime_disposition * fld_disposition,
		   struct mailmime_language * fld_language);

LIBETPAN_EXPORT
void mailmime_field_free(struct mailmime_field * field);

LIBETPAN_EXPORT
struct mailmime_fields * mailmime_fields_new(clist * fld_list);

LIBETPAN_EXPORT
void mailmime_fields_free(struct mailmime_fields * fields);


struct mailmime_multipart_body {
  clist * bd_list;
};

LIBETPAN_EXPORT
struct mailmime_multipart_body *
mailmime_multipart_body_new(clist * bd_list);

LIBETPAN_EXPORT
void mailmime_multipart_body_free(struct mailmime_multipart_body * mp_body);


enum {
  MAILMIME_DATA_TEXT,
  MAILMIME_DATA_FILE
};

struct mailmime_data {
  int dt_type;
  int dt_encoding;
  int dt_encoded;
  union {
    struct {
      const char * dt_data;
      size_t dt_length;
    } dt_text;
    char * dt_filename;
  } dt_data;
};

LIBETPAN_EXPORT
struct mailmime_data * mailmime_data_new(int dt_type, int dt_encoding,
    int dt_encoded, const char * dt_data, size_t dt_length,
    char * dt_filename);

LIBETPAN_EXPORT
void mailmime_data_free(struct mailmime_data * mime_data);


enum {
  MAILMIME_NONE,
  MAILMIME_SINGLE,
  MAILMIME_MULTIPLE,
  MAILMIME_MESSAGE
};

struct mailmime {
  /* parent information */
  int mm_parent_type;
  struct mailmime * mm_parent;
  clistiter * mm_multipart_pos;

  int mm_type;
  const char * mm_mime_start;
  size_t mm_length;
  
  struct mailmime_fields * mm_mime_fields;
  struct mailmime_content * mm_content_type;
  
  struct mailmime_data * mm_body;
  union {
    /* single part */
    struct mailmime_data * mm_single; /* XXX - was body */
    
    /* multi-part */
    struct {
      struct mailmime_data * mm_preamble;
      struct mailmime_data * mm_epilogue;
      clist * mm_mp_list;
    } mm_multipart;
    
    /* message */
    struct {
      struct mailimf_fields * mm_fields;
      struct mailmime * mm_msg_mime;
    } mm_message;
    
  } mm_data;
};

LIBETPAN_EXPORT
struct mailmime * mailmime_new(int mm_type,
    const char * mm_mime_start, size_t mm_length,
    struct mailmime_fields * mm_mime_fields,
    struct mailmime_content * mm_content_type,
    struct mailmime_data * mm_body,
    struct mailmime_data * mm_preamble,
    struct mailmime_data * mm_epilogue,
    clist * mm_mp_list,
    struct mailimf_fields * mm_fields,
    struct mailmime * mm_msg_mime);

LIBETPAN_EXPORT
void mailmime_free(struct mailmime * mime);

struct mailmime_encoded_word {
  char * wd_charset;
  char * wd_text;
};

LIBETPAN_EXPORT
struct mailmime_encoded_word *
mailmime_encoded_word_new(char * wd_charset, char * wd_text);

LIBETPAN_EXPORT
void mailmime_encoded_word_free(struct mailmime_encoded_word * ew);

LIBETPAN_EXPORT
void mailmime_charset_free(char * charset);

LIBETPAN_EXPORT
void mailmime_encoded_text_free(char * text);


struct mailmime_disposition {
  struct mailmime_disposition_type * dsp_type;
  clist * dsp_parms; /* struct mailmime_disposition_parm */
};


enum {
  MAILMIME_DISPOSITION_TYPE_ERROR,
  MAILMIME_DISPOSITION_TYPE_INLINE,
  MAILMIME_DISPOSITION_TYPE_ATTACHMENT,
  MAILMIME_DISPOSITION_TYPE_EXTENSION
};

struct mailmime_disposition_type {
  int dsp_type;
  char * dsp_extension;
};


enum {
  MAILMIME_DISPOSITION_PARM_FILENAME,
  MAILMIME_DISPOSITION_PARM_CREATION_DATE,
  MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE,
  MAILMIME_DISPOSITION_PARM_READ_DATE,
  MAILMIME_DISPOSITION_PARM_SIZE,
  MAILMIME_DISPOSITION_PARM_PARAMETER
};

struct mailmime_disposition_parm {
  int pa_type;
  union {
    char * pa_filename;
    char * pa_creation_date;
    char * pa_modification_date;
    char * pa_read_date;
    size_t pa_size;
    struct mailmime_parameter * pa_parameter;
  } pa_data;
};

LIBETPAN_EXPORT
struct mailmime_disposition *
mailmime_disposition_new(struct mailmime_disposition_type * dsp_type,
			 clist * dsp_parms);

LIBETPAN_EXPORT
void mailmime_disposition_free(struct mailmime_disposition * dsp);

LIBETPAN_EXPORT
struct mailmime_disposition_type *
mailmime_disposition_type_new(int dt_type, char * dt_extension);

LIBETPAN_EXPORT
void mailmime_disposition_type_free(struct mailmime_disposition_type * dsp_type);

LIBETPAN_EXPORT
struct mailmime_disposition_parm *
mailmime_disposition_parm_new(int pa_type,
			      char * pa_filename,
			      char * pa_creation_date,
			      char * pa_modification_date,
			      char * pa_read_date,
			      size_t pa_size,
			      struct mailmime_parameter * pa_parameter);

LIBETPAN_EXPORT
void mailmime_disposition_parm_free(struct mailmime_disposition_parm *
				    dsp_parm);

LIBETPAN_EXPORT
void mailmime_filename_parm_free(char * filename);

LIBETPAN_EXPORT
void mailmime_creation_date_parm_free(char * date);

LIBETPAN_EXPORT
void mailmime_modification_date_parm_free(char * date);

LIBETPAN_EXPORT
void mailmime_read_date_parm_free(char * date);

LIBETPAN_EXPORT
void mailmime_quoted_date_time_free(char * date);

struct mailmime_section {
  clist * sec_list; /* list of (uint32 *) */
};

LIBETPAN_EXPORT
struct mailmime_section * mailmime_section_new(clist * list);

LIBETPAN_EXPORT
void mailmime_section_free(struct mailmime_section * section);


LIBETPAN_EXPORT
void mailmime_decoded_part_free(char * part);

struct mailmime_single_fields {
  struct mailmime_content * fld_content;
  char * fld_content_charset;
  char * fld_content_boundary;
  char * fld_content_name;
  struct mailmime_mechanism * fld_encoding;
  char * fld_id;
  char * fld_description;
  uint32_t fld_version;
  struct mailmime_disposition * fld_disposition;
  char * fld_disposition_filename;
  char * fld_disposition_creation_date;
  char * fld_disposition_modification_date;
  char * fld_disposition_read_date;
  size_t fld_disposition_size;
  struct mailmime_language * fld_language;
};

#ifdef __cplusplus
}
#endif

#endif


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
 * $Id: mailmime_types.c,v 1.20 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime_types.h"
#include "mmapstring.h"

#include <string.h>
#include <stdlib.h>

void mailmime_attribute_free(char * attribute)
{
  mailmime_token_free(attribute);
}



struct mailmime_composite_type *
mailmime_composite_type_new(int ct_type, char * ct_token)
{
  struct mailmime_composite_type * ct;

  ct = malloc(sizeof(* ct));
  if (ct == NULL)
    return NULL;

  ct->ct_type = ct_type;
  ct->ct_token = ct_token;

  return ct;
}

void mailmime_composite_type_free(struct mailmime_composite_type * ct)
{
  if (ct->ct_token != NULL)
    mailmime_extension_token_free(ct->ct_token);
  free(ct);
}


struct mailmime_content *
mailmime_content_new(struct mailmime_type * ct_type,
		     char * ct_subtype,
		     clist * ct_parameters)
{
  struct mailmime_content * content;

  content = malloc(sizeof(* content));
  if (content == NULL)
    return NULL;

  content->ct_type = ct_type;
  content->ct_subtype = ct_subtype;
  content->ct_parameters = ct_parameters;

  return content;
}

void mailmime_content_free(struct mailmime_content * content)
{
  mailmime_type_free(content->ct_type);
  mailmime_subtype_free(content->ct_subtype);
  if (content->ct_parameters != NULL) {
    clist_foreach(content->ct_parameters,
		  (clist_func) mailmime_parameter_free, NULL);
    clist_free(content->ct_parameters);
  }

  free(content);
}


void mailmime_description_free(char * description)
{
  free(description);
}

struct mailmime_discrete_type *
mailmime_discrete_type_new(int dt_type, char * dt_extension)
{
  struct mailmime_discrete_type * discrete_type;

  discrete_type = malloc(sizeof(* discrete_type));
  if (discrete_type == NULL)
    return NULL;

  discrete_type->dt_type = dt_type;
  discrete_type->dt_extension = dt_extension;

  return discrete_type;
}

void mailmime_discrete_type_free(struct mailmime_discrete_type * discrete_type)
{
  if (discrete_type->dt_extension != NULL)
    mailmime_extension_token_free(discrete_type->dt_extension);
  free(discrete_type);
}

void mailmime_encoding_free(struct mailmime_mechanism * encoding)
{
  mailmime_mechanism_free(encoding);
}

void mailmime_extension_token_free(char * extension)
{
  mailmime_token_free(extension);
}

void mailmime_id_free(char * id)
{
  mailimf_msg_id_free(id);
}

struct mailmime_mechanism * mailmime_mechanism_new(int enc_type, char * enc_token)
{
  struct mailmime_mechanism * mechanism;

  mechanism = malloc(sizeof(* mechanism));
  if (mechanism == NULL)
    return NULL;

  mechanism->enc_type = enc_type;
  mechanism->enc_token = enc_token;

  return mechanism;
}

void mailmime_mechanism_free(struct mailmime_mechanism * mechanism)
{
  if (mechanism->enc_token != NULL)
    mailmime_token_free(mechanism->enc_token);
  free(mechanism);
}

struct mailmime_parameter *
mailmime_parameter_new(char * pa_name, char * pa_value)
{
  struct mailmime_parameter * parameter;

  parameter = malloc(sizeof(* parameter));
  if (parameter == NULL)
    return NULL;

  parameter->pa_name = pa_name;
  parameter->pa_value = pa_value;

  return parameter;
}

void mailmime_parameter_free(struct mailmime_parameter * parameter)
{
  mailmime_attribute_free(parameter->pa_name);
  mailmime_value_free(parameter->pa_value);
  free(parameter);
}


void mailmime_subtype_free(char * subtype)
{
  mailmime_extension_token_free(subtype);
}


void mailmime_token_free(char * token)
{
  free(token);
}


struct mailmime_type *
mailmime_type_new(int tp_type,
		  struct mailmime_discrete_type * tp_discrete_type,
		  struct mailmime_composite_type * tp_composite_type)
{
  struct mailmime_type * mime_type;
  
  mime_type = malloc(sizeof(* mime_type));
  if (mime_type == NULL)
    return NULL;

  mime_type->tp_type = tp_type;
  switch (tp_type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    mime_type->tp_data.tp_discrete_type = tp_discrete_type;
    break;
  case MAILMIME_TYPE_COMPOSITE_TYPE:
    mime_type->tp_data.tp_composite_type = tp_composite_type;
    break;
  }

  return mime_type;
}

void mailmime_type_free(struct mailmime_type * type)
{
  switch (type->tp_type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    mailmime_discrete_type_free(type->tp_data.tp_discrete_type);
    break;
  case MAILMIME_TYPE_COMPOSITE_TYPE:
    mailmime_composite_type_free(type->tp_data.tp_composite_type);
    break;
  }
  free(type);
}

void mailmime_value_free(char * value)
{
  free(value);
}


/*
void mailmime_x_token_free(gchar * x_token)
{
  g_free(x_token);
}
*/

struct mailmime_field *
mailmime_field_new(int fld_type,
		   struct mailmime_content * fld_content,
		   struct mailmime_mechanism * fld_encoding,
		   char * fld_id,
		   char * fld_description,
		   uint32_t fld_version,
		   struct mailmime_disposition * fld_disposition,
		   struct mailmime_language * fld_language)
{
  struct mailmime_field * field;
  
  field = malloc(sizeof(* field));
  if (field == NULL)
    return NULL;
  field->fld_type = fld_type;
  
  switch (fld_type) {
  case MAILMIME_FIELD_TYPE:
    field->fld_data.fld_content = fld_content;
    break;
  case MAILMIME_FIELD_TRANSFER_ENCODING:
    field->fld_data.fld_encoding = fld_encoding;
    break;
  case MAILMIME_FIELD_ID:
    field->fld_data.fld_id = fld_id;
    break;
  case MAILMIME_FIELD_DESCRIPTION:
    field->fld_data.fld_description = fld_description;
    break;
  case MAILMIME_FIELD_VERSION:
    field->fld_data.fld_version = fld_version;
    break;
  case MAILMIME_FIELD_DISPOSITION:
    field->fld_data.fld_disposition = fld_disposition;
    break;
  case MAILMIME_FIELD_LANGUAGE:
    field->fld_data.fld_language = fld_language;
    break;
  }  
  return field;
}

void mailmime_field_free(struct mailmime_field * field)
{
  switch (field->fld_type) {
  case MAILMIME_FIELD_TYPE:
    if (field->fld_data.fld_content != NULL)
      mailmime_content_free(field->fld_data.fld_content);
    break;
  case MAILMIME_FIELD_TRANSFER_ENCODING:
    if (field->fld_data.fld_encoding != NULL)
      mailmime_encoding_free(field->fld_data.fld_encoding);
    break;
  case MAILMIME_FIELD_ID:
    if (field->fld_data.fld_id != NULL)
      mailmime_id_free(field->fld_data.fld_id);
    break;
  case MAILMIME_FIELD_DESCRIPTION:
    if (field->fld_data.fld_description != NULL)
      mailmime_description_free(field->fld_data.fld_description);
    break;
  case MAILMIME_FIELD_DISPOSITION:
    if (field->fld_data.fld_disposition != NULL)
      mailmime_disposition_free(field->fld_data.fld_disposition);
    break;
  case MAILMIME_FIELD_LANGUAGE:
    if (field->fld_data.fld_language != NULL)
      mailmime_language_free(field->fld_data.fld_language);
    break;
  }

  free(field);
}

struct mailmime_fields * mailmime_fields_new(clist * fld_list)
{
  struct mailmime_fields * fields;

  fields = malloc(sizeof(* fields));
  if (fields == NULL)
    return NULL;

  fields->fld_list = fld_list;

  return fields;
}

void mailmime_fields_free(struct mailmime_fields * fields)
{
  clist_foreach(fields->fld_list, (clist_func) mailmime_field_free, NULL);
  clist_free(fields->fld_list);
  free(fields);
}


/*
struct mailmime_body_part *
mailmime_body_part_new(gchar * text, guint32 size)
{
  struct mailmime_body_part * body_part;

  body_part = g_new(struct mailmime_body_part, 1);
  if (body_part == NULL)
    return NULL;

  body_part->text = text;
  body_part->size = size;

  return body_part;
}

void mailmime_body_part_free(struct mailmime_body_part * body_part)
{
  g_free(body_part);
}
*/

struct mailmime_multipart_body *
mailmime_multipart_body_new(clist * bd_list)
{
  struct mailmime_multipart_body * mp_body;

  mp_body = malloc(sizeof(* mp_body));
  if (mp_body == NULL)
    return NULL;

  mp_body->bd_list = bd_list;

  return mp_body;
}

void mailmime_multipart_body_free(struct mailmime_multipart_body * mp_body)
{
  clist_foreach(mp_body->bd_list, (clist_func) mailimf_body_free, NULL);
  clist_free(mp_body->bd_list);
  free(mp_body);
}




struct mailmime * mailmime_new(int mm_type,
    const char * mm_mime_start, size_t mm_length,
    struct mailmime_fields * mm_mime_fields,
    struct mailmime_content * mm_content_type,
    struct mailmime_data * mm_body,
    struct mailmime_data * mm_preamble,
    struct mailmime_data * mm_epilogue,
    clist * mm_mp_list,
    struct mailimf_fields * mm_fields,
    struct mailmime * mm_msg_mime)
{
  struct mailmime * mime;
  clistiter * cur;

  mime = malloc(sizeof(* mime));
  if (mime == NULL)
    return NULL;

  mime->mm_parent = NULL;
  mime->mm_parent_type = MAILMIME_NONE;
  mime->mm_multipart_pos = NULL;

  mime->mm_type = mm_type;
  mime->mm_mime_start = mm_mime_start;
  mime->mm_length = mm_length;
  mime->mm_mime_fields = mm_mime_fields;
  mime->mm_content_type = mm_content_type;
  
  mime->mm_body = mm_body;

  switch (mm_type) {
  case MAILMIME_SINGLE:
    mime->mm_data.mm_single = mm_body;
    break;

  case MAILMIME_MULTIPLE:
    mime->mm_data.mm_multipart.mm_preamble = mm_preamble;
    mime->mm_data.mm_multipart.mm_epilogue = mm_epilogue;
    mime->mm_data.mm_multipart.mm_mp_list = mm_mp_list;

    for(cur = clist_begin(mm_mp_list) ; cur != NULL ;
        cur = clist_next(cur)) {
      struct mailmime * submime;

      submime = clist_content(cur);
      submime->mm_parent = mime;
      submime->mm_parent_type = MAILMIME_MULTIPLE;
      submime->mm_multipart_pos = cur;
    }
    break;

  case MAILMIME_MESSAGE:
    mime->mm_data.mm_message.mm_fields = mm_fields;
    mime->mm_data.mm_message.mm_msg_mime = mm_msg_mime;
    if (mm_msg_mime != NULL) {
      mm_msg_mime->mm_parent = mime;
      mm_msg_mime->mm_parent_type = MAILMIME_MESSAGE;
    }
    break;

  }
  
  return mime;
}

void mailmime_free(struct mailmime * mime)
{
  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    if ((mime->mm_body == NULL) && (mime->mm_data.mm_single != NULL))
      mailmime_data_free(mime->mm_data.mm_single);
    /* do nothing */
    break;
    
  case MAILMIME_MULTIPLE:
    if (mime->mm_data.mm_multipart.mm_preamble != NULL)
      mailmime_data_free(mime->mm_data.mm_multipart.mm_preamble);
    if (mime->mm_data.mm_multipart.mm_epilogue != NULL)
      mailmime_data_free(mime->mm_data.mm_multipart.mm_epilogue);
    clist_foreach(mime->mm_data.mm_multipart.mm_mp_list,
        (clist_func) mailmime_free, NULL);
    clist_free(mime->mm_data.mm_multipart.mm_mp_list);
    break;
    
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_fields != NULL)
      mailimf_fields_free(mime->mm_data.mm_message.mm_fields);
    if (mime->mm_data.mm_message.mm_msg_mime != NULL)
      mailmime_free(mime->mm_data.mm_message.mm_msg_mime);
    break;
    
  }
  if (mime->mm_body != NULL)
    mailmime_data_free(mime->mm_body);

  if (mime->mm_mime_fields != NULL)
    mailmime_fields_free(mime->mm_mime_fields);
  if (mime->mm_content_type != NULL)
    mailmime_content_free(mime->mm_content_type);
  free(mime);
}



struct mailmime_encoded_word *
mailmime_encoded_word_new(char * wd_charset, char * wd_text)
{
  struct mailmime_encoded_word * ew;
  
  ew = malloc(sizeof(* ew));
  if (ew == NULL)
    return NULL;
  ew->wd_charset = wd_charset;
  ew->wd_text = wd_text;
  
  return ew;
}

void mailmime_charset_free(char * charset)
{
  free(charset);
}

void mailmime_encoded_text_free(char * text)
{
  free(text);
}

void mailmime_encoded_word_free(struct mailmime_encoded_word * ew)
{
  mailmime_charset_free(ew->wd_charset);
  mailmime_encoded_text_free(ew->wd_text);
  free(ew);
}



/* mailmime_disposition */


struct mailmime_disposition *
mailmime_disposition_new(struct mailmime_disposition_type * dsp_type,
			 clist * dsp_parms)
{
  struct mailmime_disposition * dsp;

  dsp = malloc(sizeof(* dsp));
  if (dsp == NULL)
    return NULL;
  dsp->dsp_type = dsp_type;
  dsp->dsp_parms = dsp_parms;

  return dsp;
}

void mailmime_disposition_free(struct mailmime_disposition * dsp)
{
  mailmime_disposition_type_free(dsp->dsp_type);
  clist_foreach(dsp->dsp_parms,
      (clist_func) mailmime_disposition_parm_free, NULL);
  clist_free(dsp->dsp_parms);
  free(dsp);
}



struct mailmime_disposition_type *
mailmime_disposition_type_new(int dsp_type, char * dsp_extension)
{
  struct mailmime_disposition_type * m_dsp_type;

  m_dsp_type = malloc(sizeof(* m_dsp_type));
  if (m_dsp_type == NULL)
    return NULL;

  m_dsp_type->dsp_type = dsp_type;
  m_dsp_type->dsp_extension = dsp_extension;

  return m_dsp_type;
}

void mailmime_disposition_type_free(struct mailmime_disposition_type * dsp_type)
{
  if (dsp_type->dsp_extension != NULL)
    free(dsp_type->dsp_extension);
  free(dsp_type);
}


struct mailmime_disposition_parm *
mailmime_disposition_parm_new(int pa_type,
    char * pa_filename,
    char * pa_creation_date,
    char * pa_modification_date,
    char * pa_read_date,
    size_t pa_size,
    struct mailmime_parameter * pa_parameter)
{
  struct mailmime_disposition_parm * dsp_parm;

  dsp_parm = malloc(sizeof(* dsp_parm));
  if (dsp_parm == NULL)
    return NULL;

  dsp_parm->pa_type = pa_type;
  switch (pa_type) {
  case MAILMIME_DISPOSITION_PARM_FILENAME:
    dsp_parm->pa_data.pa_filename = pa_filename;
    break;
  case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
    dsp_parm->pa_data.pa_creation_date = pa_creation_date;
    break;
  case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
    dsp_parm->pa_data.pa_modification_date = pa_modification_date;
    break;
  case MAILMIME_DISPOSITION_PARM_READ_DATE:
    dsp_parm->pa_data.pa_read_date = pa_read_date;
    break;
  case MAILMIME_DISPOSITION_PARM_SIZE:
    dsp_parm->pa_data.pa_size = pa_size;
    break;
  case MAILMIME_DISPOSITION_PARM_PARAMETER:
    dsp_parm->pa_data.pa_parameter = pa_parameter;
    break;
  }

  return dsp_parm;
}

void mailmime_disposition_parm_free(struct mailmime_disposition_parm *
				    dsp_parm)
{
  switch (dsp_parm->pa_type) {
  case MAILMIME_DISPOSITION_PARM_FILENAME:
    mailmime_filename_parm_free(dsp_parm->pa_data.pa_filename);
    break;
  case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
    mailmime_creation_date_parm_free(dsp_parm->pa_data.pa_creation_date);
    break;
  case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
    mailmime_modification_date_parm_free(dsp_parm->pa_data.pa_modification_date);
    break;
  case MAILMIME_DISPOSITION_PARM_READ_DATE:
    mailmime_read_date_parm_free(dsp_parm->pa_data.pa_read_date);
    break;
  case MAILMIME_DISPOSITION_PARM_PARAMETER:
    mailmime_parameter_free(dsp_parm->pa_data.pa_parameter);
    break;
  }
  
  free(dsp_parm);
}


void mailmime_filename_parm_free(char * filename)
{
  mailmime_value_free(filename);
}

void mailmime_creation_date_parm_free(char * date)
{
  mailmime_quoted_date_time_free(date);
}

void mailmime_modification_date_parm_free(char * date)
{
  mailmime_quoted_date_time_free(date);
}

void mailmime_read_date_parm_free(char * date)
{
  mailmime_quoted_date_time_free(date);
}

void mailmime_quoted_date_time_free(char * date)
{
  mailimf_quoted_string_free(date);
}

struct mailmime_section * mailmime_section_new(clist * sec_list)
{
  struct mailmime_section * section;

  section = malloc(sizeof(* section));
  if (section == NULL)
    return NULL;

  section->sec_list = sec_list;

  return section;
}

void mailmime_section_free(struct mailmime_section * section)
{
  clist_foreach(section->sec_list, (clist_func) free, NULL);
  clist_free(section->sec_list);
  free(section);
}



struct mailmime_language * mailmime_language_new(clist * lg_list)
{
  struct mailmime_language * lang;

  lang = malloc(sizeof(* lang));
  if (lang == NULL)
    return NULL;

  lang->lg_list = lg_list;

  return lang;
}

void mailmime_language_free(struct mailmime_language * lang)
{
  clist_foreach(lang->lg_list, (clist_func) mailimf_atom_free, NULL);
  clist_free(lang->lg_list);
  free(lang);
}

void mailmime_decoded_part_free(char * part)
{
  mmap_string_unref(part);
}

struct mailmime_data * mailmime_data_new(int dt_type, int dt_encoding,
    int dt_encoded, const char * dt_data, size_t dt_length, char * dt_filename)
{
  struct mailmime_data * mime_data;

  mime_data = malloc(sizeof(* mime_data));
  if (mime_data == NULL)
    return NULL;

  mime_data->dt_type = dt_type;
  mime_data->dt_encoding = dt_encoding;
  mime_data->dt_encoded = dt_encoded;
  switch (dt_type) {
  case MAILMIME_DATA_TEXT:
    mime_data->dt_data.dt_text.dt_data = dt_data;
    mime_data->dt_data.dt_text.dt_length = dt_length;
    break;
  case MAILMIME_DATA_FILE:
    mime_data->dt_data.dt_filename = dt_filename;
    break;
  }
  
  return mime_data;
}

void mailmime_data_free(struct mailmime_data * mime_data)
{
  switch (mime_data->dt_type) {
  case MAILMIME_DATA_FILE:
    free(mime_data->dt_data.dt_filename);
    break;
  }
  free(mime_data);
}

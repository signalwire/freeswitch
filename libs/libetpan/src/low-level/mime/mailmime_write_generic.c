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
 * $Id: mailmime_write_generic.c,v 1.6 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime_write_generic.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_SYS_MMAN_H
#	include <sys/mman.h>
#endif
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif

#include "mailimf_write_generic.h"
#include "mailmime_content.h"
#include "mailmime_types_helper.h"

#define MAX_MAIL_COL 78

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static int mailmime_field_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailmime_field * field);

static int mailmime_id_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, char * id);

static int mailmime_description_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, char * descr);

static int mailmime_version_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, uint32_t version);

static int mailmime_encoding_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailmime_mechanism * encoding);

static int mailmime_language_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailmime_language * language);

static int mailmime_disposition_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				      struct mailmime_disposition *
				      disposition);

static int
mailmime_disposition_param_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailmime_disposition_parm * param);

static int mailmime_parameter_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailmime_parameter * param);

/*
static int mailmime_content_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  struct mailmime_content * content);
*/

static int mailmime_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			       struct mailmime_type * type);

static int
mailmime_discrete_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			     struct mailmime_discrete_type * discrete_type);

static int
mailmime_composite_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			      struct mailmime_composite_type * composite_type);

static int mailmime_sub_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    struct mailmime * build_info);


/* ***** */

int mailmime_fields_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, struct mailmime_fields * fields)
{
  int r;
  clistiter * cur;

  for(cur = clist_begin(fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailmime_field * field;

    field = cur->data;
    r = mailmime_field_write_driver(do_write, data, col, field);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_field_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailmime_field * field)
{
  int r;

  switch (field->fld_type) {
  case MAILMIME_FIELD_TYPE:
    r = mailmime_content_write_driver(do_write, data, col, field->fld_data.fld_content);
    break;

  case MAILMIME_FIELD_TRANSFER_ENCODING:
    r = mailmime_encoding_write_driver(do_write, data, col, field->fld_data.fld_encoding);
    break;

  case MAILMIME_FIELD_ID:
    r = mailmime_id_write_driver(do_write, data, col, field->fld_data.fld_id);
    break;

  case MAILMIME_FIELD_DESCRIPTION:
    r = mailmime_description_write_driver(do_write, data, col, field->fld_data.fld_description);
    break;

  case MAILMIME_FIELD_VERSION:
    r = mailmime_version_write_driver(do_write, data, col, field->fld_data.fld_version);
    break;

  case MAILMIME_FIELD_DISPOSITION:
    r = mailmime_disposition_write_driver(do_write, data, col, field->fld_data.fld_disposition);
    break;

  case MAILMIME_FIELD_LANGUAGE:
    r = mailmime_language_write_driver(do_write, data, col, field->fld_data.fld_language);
    break;

  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }

  if (r != MAILIMF_NO_ERROR)
    return r;
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_id_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, char * id)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Content-ID: ", 12);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "<", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, id, strlen(id));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, ">", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_description_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, char * descr)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Content-Description: ", 21);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, descr, strlen(descr));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_version_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, uint32_t version)
{
  int r;
  char versionstr[40];

  r = mailimf_string_write_driver(do_write, data, col, "MIME-Version: ", 14);
  if (r != MAILIMF_NO_ERROR)
    return r;

  snprintf(versionstr, 40, "%i.%i", version >> 16, version & 0xFFFF);

  r = mailimf_string_write_driver(do_write, data, col, versionstr, strlen(versionstr));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_encoding_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailmime_mechanism * encoding)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Content-Transfer-Encoding: ", 27);
  if (r != MAILIMF_NO_ERROR)
    return r;

  switch (encoding->enc_type) {
  case MAILMIME_MECHANISM_7BIT:
    r = mailimf_string_write_driver(do_write, data, col, "7bit", 4);
    break;

  case MAILMIME_MECHANISM_8BIT:
    r = mailimf_string_write_driver(do_write, data, col, "8bit", 4);
    break;

  case MAILMIME_MECHANISM_BINARY:
    r = mailimf_string_write_driver(do_write, data, col, "binary", 6);
    break;

  case MAILMIME_MECHANISM_QUOTED_PRINTABLE:
    r = mailimf_string_write_driver(do_write, data, col, "quoted-printable", 16);
    break;

  case MAILMIME_MECHANISM_BASE64:
    r = mailimf_string_write_driver(do_write, data, col, "base64", 6);
    break;

  case MAILMIME_MECHANISM_TOKEN:
    r = mailimf_string_write_driver(do_write, data, col, encoding->enc_token,
        strlen(encoding->enc_token));
    break;

  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }

  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_language_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				   struct mailmime_language * language)
{
  int r;
  clistiter * cur;
  int first;
  
  r = mailimf_string_write_driver(do_write, data, col, "Content-Language: ", 18);
  if (r != MAILIMF_NO_ERROR)
    return r;

  first = TRUE;

  for(cur = clist_begin(language->lg_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    char * lang;
    size_t len;

    lang = clist_content(cur);
    len = strlen(lang);

    if (!first) {
      r = mailimf_string_write_driver(do_write, data, col, ", ", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
    else {
      first = FALSE;
    }

    if (* col > 1) {
      
      if (* col + len > MAX_MAIL_COL) {
	r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
	if (r != MAILIMF_NO_ERROR)
	  return r;
#if 0
	* col = 1;
#endif
      }
    }

    r = mailimf_string_write_driver(do_write, data, col, lang, len);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif

  return MAILIMF_NO_ERROR;
}

static int mailmime_disposition_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				      struct mailmime_disposition *
				      disposition)
{
  struct mailmime_disposition_type * dsp_type;
  int r;
  clistiter * cur;

  dsp_type = disposition->dsp_type;

  r = mailimf_string_write_driver(do_write, data, col, "Content-Disposition: ", 21);
  if (r != MAILIMF_NO_ERROR)
    return r;

  switch (dsp_type->dsp_type) {
  case MAILMIME_DISPOSITION_TYPE_INLINE:
    r = mailimf_string_write_driver(do_write, data, col, "inline", 6);
    break;

  case MAILMIME_DISPOSITION_TYPE_ATTACHMENT:
    r = mailimf_string_write_driver(do_write, data, col, "attachment", 10);
    break;

  case MAILMIME_DISPOSITION_TYPE_EXTENSION:
    r = mailimf_string_write_driver(do_write, data, col, dsp_type->dsp_extension,
			     strlen(dsp_type->dsp_extension));
    break;

  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }

  if (r != MAILIMF_NO_ERROR)
    return r;

  for(cur = clist_begin(disposition->dsp_parms) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_disposition_parm * param;

    param = cur->data;

    r = mailimf_string_write_driver(do_write, data, col, "; ", 2);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailmime_disposition_param_write_driver(do_write, data, col, param);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}

static int
mailmime_disposition_param_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 struct mailmime_disposition_parm * param)
{
  size_t len;
  char sizestr[20];
  int r;

  switch (param->pa_type) {
  case MAILMIME_DISPOSITION_PARM_FILENAME:
    len = strlen("filename=") + strlen(param->pa_data.pa_filename);
    break;

  case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
    len = strlen("creation-date=") + strlen(param->pa_data.pa_creation_date);
    break;

  case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
    len = strlen("modification-date=") +
      strlen(param->pa_data.pa_modification_date);
    break;

  case MAILMIME_DISPOSITION_PARM_READ_DATE:
    len = strlen("read-date=") + strlen(param->pa_data.pa_read_date);
    break;

  case MAILMIME_DISPOSITION_PARM_SIZE:
    snprintf(sizestr, 20, "%lu", (unsigned long) param->pa_data.pa_size);
    len = strlen("size=") + strlen(sizestr);
    break;

  case MAILMIME_DISPOSITION_PARM_PARAMETER:
    len = strlen(param->pa_data.pa_parameter->pa_name) + 1 +
      strlen(param->pa_data.pa_parameter->pa_value);
    break;

  default:
    return MAILIMF_ERROR_INVAL;
  }

  if (* col > 1) {
      
    if (* col + len > MAX_MAIL_COL) {
      r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
      if (r != MAILIMF_NO_ERROR)
	return r;
#if 0
      * col = 1;
#endif
    }
  }

  switch (param->pa_type) {
  case MAILMIME_DISPOSITION_PARM_FILENAME:
    r = mailimf_string_write_driver(do_write, data, col, "filename=", 9);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_quoted_string_write_driver(do_write, data, col,
        param->pa_data.pa_filename, strlen(param->pa_data.pa_filename));
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
    r = mailimf_string_write_driver(do_write, data, col, "creation-date=", 14);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_quoted_string_write_driver(do_write, data, col, param->pa_data.pa_creation_date,
        strlen(param->pa_data.pa_creation_date));
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
    r = mailimf_string_write_driver(do_write, data, col, "modification-date=", 18);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_quoted_string_write_driver(do_write, data, col,
        param->pa_data.pa_modification_date,
        strlen(param->pa_data.pa_modification_date));
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_DISPOSITION_PARM_READ_DATE:
    r = mailimf_string_write_driver(do_write, data, col, "read-date=", 10);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_quoted_string_write_driver(do_write, data, col, param->pa_data.pa_read_date,
        strlen(param->pa_data.pa_read_date));
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_DISPOSITION_PARM_SIZE:
    r = mailimf_string_write_driver(do_write, data, col, "size=", 5);
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_string_write_driver(do_write, data, col, sizestr, strlen(sizestr));
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_DISPOSITION_PARM_PARAMETER:
    r = mailmime_parameter_write_driver(do_write, data, col, param->pa_data.pa_parameter);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;
  }

  return MAILIMF_NO_ERROR;
}

static int mailmime_parameter_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				    struct mailmime_parameter * param)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, param->pa_name,
      strlen(param->pa_name));
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "=", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_quoted_string_write_driver(do_write, data, col, param->pa_value,
      strlen(param->pa_value));
  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}

int mailmime_content_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				struct mailmime_content * content)
{
  clistiter * cur;
  size_t len;
  int r;

  r = mailmime_type_write_driver(do_write, data, col, content->ct_type);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "/", 1);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, content->ct_subtype,
      strlen(content->ct_subtype));
  if (r != MAILIMF_NO_ERROR)
    return r;

  if (content->ct_parameters != NULL) {
    for(cur = clist_begin(content->ct_parameters) ;
	cur != NULL ; cur = clist_next(cur)) {
      struct mailmime_parameter * param;

      param = cur->data;

      r = mailimf_string_write_driver(do_write, data, col, "; ", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;

      len = strlen(param->pa_name) + 1 + strlen(param->pa_value);

      if (* col > 1) {
      
	if (* col + len > MAX_MAIL_COL) {
	  r = mailimf_string_write_driver(do_write, data, col, "\r\n ", 3);
	  if (r != MAILIMF_NO_ERROR)
	    return r;
#if 0
	  * col = 1;
#endif
	}
      }
    
      r = mailmime_parameter_write_driver(do_write, data, col, param);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
  }

  return MAILIMF_NO_ERROR;
}

int mailmime_content_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			   struct mailmime_content * content)
{
  int r;

  r = mailimf_string_write_driver(do_write, data, col, "Content-Type: ", 14);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailmime_content_type_write_driver(do_write, data, col, content);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
 
  return MAILIMF_NO_ERROR;
}

static int mailmime_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			       struct mailmime_type * type)
{
  int r;

  switch (type->tp_type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    r = mailmime_discrete_type_write_driver(do_write, data, col, type->tp_data.tp_discrete_type);
    break;

  case MAILMIME_TYPE_COMPOSITE_TYPE:
    r = mailmime_composite_type_write_driver(do_write, data, col, type->tp_data.tp_composite_type);
    break;

  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }

  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}

static int
mailmime_discrete_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			     struct mailmime_discrete_type * discrete_type)
{
  int r;

  switch (discrete_type->dt_type) {
  case MAILMIME_DISCRETE_TYPE_TEXT:
    r = mailimf_string_write_driver(do_write, data, col, "text", 4);
    break;

  case MAILMIME_DISCRETE_TYPE_IMAGE:
    r = mailimf_string_write_driver(do_write, data, col, "image", 5);
    break;

  case MAILMIME_DISCRETE_TYPE_AUDIO:
    r = mailimf_string_write_driver(do_write, data, col, "audio", 5);
    break;

  case MAILMIME_DISCRETE_TYPE_VIDEO:
    r = mailimf_string_write_driver(do_write, data, col, "video", 5);
    break;

  case MAILMIME_DISCRETE_TYPE_APPLICATION:
    r = mailimf_string_write_driver(do_write, data, col, "application", 11);
    break;

  case MAILMIME_DISCRETE_TYPE_EXTENSION:
    r = mailimf_string_write_driver(do_write, data, col, discrete_type->dt_extension,
			     strlen(discrete_type->dt_extension));
    break;

  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }

  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}

static int
mailmime_composite_type_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			      struct mailmime_composite_type * composite_type)
{
  int r;

  switch (composite_type->ct_type) {
  case MAILMIME_COMPOSITE_TYPE_MESSAGE:
    r = mailimf_string_write_driver(do_write, data, col, "message", 7);
    break;

  case MAILMIME_COMPOSITE_TYPE_MULTIPART:
    r = mailimf_string_write_driver(do_write, data, col, "multipart", 9);
    break;

  case MAILMIME_COMPOSITE_TYPE_EXTENSION:
    r = mailimf_string_write_driver(do_write, data, col, composite_type->ct_token,
			     strlen(composite_type->ct_token));
    break;

  default:
    r = MAILIMF_ERROR_INVAL;
    break;
  }

  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}




/* ****************************************************************** */
/* message */

/*
static int mailmime_data_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
			       struct mailmime_data * data,
			       int is_text);
*/

static int mailmime_text_content_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, int encoding,
				       int istext,
				       const char * text, size_t size);

/*
static int mailmime_base64_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				 char * text, size_t size);

static int mailmime_quoted_printable_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, int istext,
					   char * text, size_t size);
*/

static int mailmime_part_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    struct mailmime * build_info)
{
  clistiter * cur;
  int first;
  int r;
  char * boundary;
  int istext;

  istext = TRUE;
  boundary = NULL;

  if (build_info->mm_content_type != NULL) {
    if (build_info->mm_type == MAILMIME_MULTIPLE) {
      boundary = mailmime_extract_boundary(build_info->mm_content_type);
      if (boundary == NULL)
        return MAILIMF_ERROR_INVAL;
    }
    
    if (build_info->mm_content_type->ct_type->tp_type ==
        MAILMIME_TYPE_DISCRETE_TYPE) {
      if (build_info->mm_content_type->ct_type->tp_data.tp_discrete_type->dt_type !=
          MAILMIME_DISCRETE_TYPE_TEXT)
        istext = FALSE;
    }
  }
    
  switch (build_info->mm_type) {
  case MAILMIME_SINGLE:
    
    /* 1-part body */

    if (build_info->mm_data.mm_single != NULL) {
      r = mailmime_data_write_driver(do_write, data, col, build_info->mm_data.mm_single, istext);
      if (r != MAILIMF_NO_ERROR)
        return r;
    }
    
    break;

  case MAILMIME_MULTIPLE:

    /* multi-part */


    /* preamble */

    if (build_info->mm_data.mm_multipart.mm_preamble != NULL) {
      r = mailmime_data_write_driver(do_write, data, col,
          build_info->mm_data.mm_multipart.mm_preamble, TRUE);
      if (r != MAILIMF_NO_ERROR)
	return r;
      
      r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;
#if 0
      * col = 0;
#endif
    }

    /* sub-parts */

    first = TRUE;

    for(cur = clist_begin(build_info->mm_data.mm_multipart.mm_mp_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      struct mailmime * subpart;

      subpart = cur->data;

      if (!first) {
	r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
	if (r != MAILIMF_NO_ERROR)
	  return r;
#if 0
	* col = 0;
#endif
      }
      else {
	first = FALSE;
      }

      r = mailimf_string_write_driver(do_write, data, col, "--", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;

      r = mailimf_string_write_driver(do_write, data, col, boundary, strlen(boundary));
      if (r != MAILIMF_NO_ERROR)
	return r;

      r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;
#if 0
      * col = 0;
#endif

      r = mailmime_sub_write_driver(do_write, data, col, subpart);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
    
    r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
    if (r != MAILIMF_NO_ERROR)
      return r;
#if 0
    * col = 0;
#endif
    
    r = mailimf_string_write_driver(do_write, data, col, "--", 2);
    if (r != MAILIMF_NO_ERROR)
      return r;
    
    r = mailimf_string_write_driver(do_write, data, col, boundary, strlen(boundary));
    if (r != MAILIMF_NO_ERROR)
      return r;

    r = mailimf_string_write_driver(do_write, data, col, "--", 2);
    if (r != MAILIMF_NO_ERROR)
      return r;


    /* epilogue */

    r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
    if (r != MAILIMF_NO_ERROR)
      return r;
#if 0
    * col = 0;
#endif
    
    if (build_info->mm_data.mm_multipart.mm_epilogue != NULL) {
      r = mailmime_data_write_driver(do_write, data, col,
          build_info->mm_data.mm_multipart.mm_epilogue, TRUE);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }

    break;

  case MAILMIME_MESSAGE:

    if (build_info->mm_data.mm_message.mm_fields != NULL) {
      r = mailimf_fields_write_driver(do_write, data, col,
          build_info->mm_data.mm_message.mm_fields);
      if (r != MAILIMF_NO_ERROR)
        return r;
    }

    if (build_info->mm_mime_fields != NULL) {
      r = mailmime_fields_write_driver(do_write, data, col, build_info->mm_mime_fields);
      if (r != MAILIMF_NO_ERROR)
        return r;
    }

    /* encapsuled message */
    
    if (build_info->mm_data.mm_message.mm_msg_mime != NULL) {
      r = mailmime_sub_write_driver(do_write, data, col,
          build_info->mm_data.mm_message.mm_msg_mime);
      if (r != MAILIMF_NO_ERROR)
        return r;
    }
    break;
    
  }
  
  return MAILIMF_NO_ERROR;
}


static int mailmime_sub_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    struct mailmime * build_info)
{
  int r;

#if 0
  * col = 0;
#endif
  /* MIME field - Content-Type */
  
  if (build_info->mm_content_type != NULL) {
    r = mailmime_content_write_driver(do_write, data, col, build_info->mm_content_type);
    if (r != MAILIMF_NO_ERROR)
      return r;
  }

  /* other MIME fields */
  
  if (build_info->mm_type != MAILMIME_MESSAGE) {
    if (build_info->mm_mime_fields != NULL) {
      r = mailmime_fields_write_driver(do_write, data, col, build_info->mm_mime_fields);
      if (r != MAILIMF_NO_ERROR)
        return r;
    }
  }

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
  if (r != MAILIMF_NO_ERROR)
    return r;
#if 0
  * col = 0;
#endif
  
  return mailmime_part_write_driver(do_write, data, col, build_info);
}

int mailmime_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
		   struct mailmime * build_info)
{
  if (build_info->mm_parent != NULL)
    return mailmime_sub_write_driver(do_write, data, col, build_info);
  else
    return mailmime_part_write_driver(do_write, data, col, build_info);
}


int mailmime_data_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    struct mailmime_data * mime_data,
    int istext)
{
  int fd;
  int r;
  char * text;
  struct stat buf;
  int res;

  switch (mime_data->dt_type) {
  case MAILMIME_DATA_TEXT:

    if (mime_data->dt_encoded) {
      r = mailimf_string_write_driver(do_write, data, col,
          mime_data->dt_data.dt_text.dt_data,
          mime_data->dt_data.dt_text.dt_length);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }
    else {
      r = mailmime_text_content_write_driver(do_write, data, col, mime_data->dt_encoding, istext,
          mime_data->dt_data.dt_text.dt_data,
          mime_data->dt_data.dt_text.dt_length);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }

    break;

  case MAILMIME_DATA_FILE:
    fd = open(mime_data->dt_data.dt_filename, O_RDONLY);
    if (fd < 0) {
      res = MAILIMF_ERROR_FILE;
      goto err;
    }

    r = fstat(fd, &buf);
    if (r < 0) {
      res = MAILIMF_ERROR_FILE;
      goto close;
    }

    if (buf.st_size != 0) {
      text = mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
      if (text == (char *)MAP_FAILED) {
	res = MAILIMF_ERROR_FILE;
	goto close;
      }
      
      if (mime_data->dt_encoded) {
	r = mailimf_string_write_driver(do_write, data, col, text, buf.st_size);
	if (r != MAILIMF_NO_ERROR) {
	  res = r;
          goto unmap;
        }
      }
      else {
	r = mailmime_text_content_write_driver(do_write, data, col, mime_data->dt_encoding, istext,
            text, buf.st_size);
	if (r != MAILIMF_NO_ERROR) {
	  res = r;
          goto unmap;
        }
      }
      
      munmap(text, buf.st_size);
    }
    close(fd);

    if (r != MAILIMF_NO_ERROR)
      return r;

    break;

  unmap:
    munmap(text, buf.st_size);
  close:
    close(fd);
  err:
    return res;
  }
  
  return MAILIMF_NO_ERROR;
}

static int mailmime_text_content_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, int encoding,
				       int istext,
				       const char * text, size_t size)
{
  switch (encoding) {
  case MAILMIME_MECHANISM_QUOTED_PRINTABLE:
    return mailmime_quoted_printable_write_driver(do_write, data, col, istext, text, size);
    break;

  case MAILMIME_MECHANISM_BASE64:
    return mailmime_base64_write_driver(do_write, data, col, text, size);
    break;

  case MAILMIME_MECHANISM_7BIT:
  case MAILMIME_MECHANISM_8BIT:
  case MAILMIME_MECHANISM_BINARY:
  default:
    return mailimf_string_write_driver(do_write, data, col, text, size);
  }
}


static const char base64_encoding[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define BASE64_MAX_COL 76

int mailmime_base64_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col,
    const char * text, size_t size)
{
  int a;
  int b;
  int c;
  size_t remains;
  const char * p;
  size_t count;
  char ogroup[4];
  int r;

  remains = size;
  p = text;

  while (remains > 0) {
    switch (remains) {
    case 1:
      a = (unsigned char) p[0];
      b = 0;
      c = 0;
      count = 1;
      break;
    case 2:
      a = (unsigned char) p[0];
      b = (unsigned char) p[1];
      c = 0;
      count = 2;
      break;
    default:
      a = (unsigned char) p[0];
      b = (unsigned char) p[1];
      c = (unsigned char) p[2];
      count = 3;
      break;
    }

    ogroup[0]= base64_encoding[a >> 2];
    ogroup[1]= base64_encoding[((a & 3) << 4) | (b >> 4)];
    ogroup[2]= base64_encoding[((b & 0xF) << 2) | (c >> 6)];
    ogroup[3]= base64_encoding[c & 0x3F];

    switch (count) {
    case 1:
      ogroup[2]= '=';
      ogroup[3]= '=';
      break;
    case 2:
      ogroup[3]= '=';
      break;
    }

    if (* col + 4 > BASE64_MAX_COL) {
      r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
      if (r != MAILIMF_NO_ERROR)
	return r;
#if 0
      * col = 0;
#endif
    }
    
    r = mailimf_string_write_driver(do_write, data, col, ogroup, 4);
    if (r != MAILIMF_NO_ERROR)
      return r;

    remains -= count;
    p += count;
  }

  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);

  return MAILIMF_NO_ERROR;
}

#if 0
#define MAX_WRITE_SIZE 512
#endif

enum {
  STATE_INIT,
  STATE_CR,
  STATE_SPACE,
  STATE_SPACE_CR
};

#if 0
static inline int write_try_buf(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				char ** pstart, size_t * plen)
{
  int r;

  if (* plen >= MAX_WRITE_SIZE) {
    r = mailimf_string_write_driver(do_write, data, col, * pstart, * plen);
    if (r != MAILIMF_NO_ERROR)
      return r;
    * plen = 0;
  }

  return MAILIMF_NO_ERROR;
}
#endif

static inline int write_remaining(int (* do_write)(void *, const char *, size_t), void * data, int * col,
				  const char ** pstart, size_t * plen)
{
  int r;

  if (* plen > 0) {
    r = mailimf_string_write_driver(do_write, data, col, * pstart, * plen);
    if (r != MAILIMF_NO_ERROR)
      return r;
    * plen = 0;
  }

  return MAILIMF_NO_ERROR;
}



#define QP_MAX_COL 72

int mailmime_quoted_printable_write_driver(int (* do_write)(void *, const char *, size_t), void * data, int * col, int istext,
    const char * text, size_t size)
{
  size_t i;
  const char * start;
  size_t len;
  char hexstr[6];
  int r;
  int state;

  start = text;
  len = 0;
  state = STATE_INIT;

  i = 0;
  while (i < size) {
    unsigned char ch;

    if (* col + len > QP_MAX_COL) {
      r = write_remaining(do_write, data, col, &start, &len);
      if (r != MAILIMF_NO_ERROR)
	return r;
      start = text + i;

      r = mailimf_string_write_driver(do_write, data, col, "=\r\n", 3);
      if (r != MAILIMF_NO_ERROR)
	return r;
    }

    ch = text[i];

    switch (state) {

    case STATE_INIT:
      switch (ch) {
      case ' ':
      case '\t':
        state = STATE_SPACE;
	break;
        
      case '\r':
	state = STATE_CR;
	break;

      case '!':
      case '"':
      case '#':
      case '$':
      case '@':
      case '[':
      case '\\':
      case ']':
      case '^':
      case '`':
      case '{':
      case '|':
      case '}':
      case '~':
      case '=':
      case '?':
      case '_':
      case 'F': /* there is no more 'From' at the beginning of a line */
	r = write_remaining(do_write, data, col, &start, &len);
	if (r != MAILIMF_NO_ERROR)
	  return r;
        start = text + i + 1;

	snprintf(hexstr, 6, "=%02X", ch);

	r = mailimf_string_write_driver(do_write, data, col, hexstr, 3);
	if (r != MAILIMF_NO_ERROR)
	  return r;
	break;

      default:
	if (istext && (ch == '\n')) {
	  r = write_remaining(do_write, data, col, &start, &len);
	  if (r != MAILIMF_NO_ERROR)
	    return r;
          start = text + i + 1;
          
	  r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
	  if (r != MAILIMF_NO_ERROR)
	    return r;
	  break;
	}
	else {
	  if (((ch >= 33) && (ch <= 60)) || ((ch >= 62) && (ch <= 126))) {
	    len ++;
	  }
	  else {
	    r = write_remaining(do_write, data, col, &start, &len);
	    if (r != MAILIMF_NO_ERROR)
	      return r;
            start = text + i + 1;

	    snprintf(hexstr, 6, "=%02X", ch);

	    r = mailimf_string_write_driver(do_write, data, col, hexstr, 3);
	    if (r != MAILIMF_NO_ERROR)
	      return r;
	  }
	}

	break;
      }

      i ++;
      break;

    case STATE_CR:
      switch (ch) {
      case '\n':
	r = write_remaining(do_write, data, col, &start, &len);
	if (r != MAILIMF_NO_ERROR)
	  return r;
        start = text + i + 1;
	r = mailimf_string_write_driver(do_write, data, col, "\r\n", 2);
	if (r != MAILIMF_NO_ERROR)
	  return r;
	i ++;
	state = STATE_INIT;
	break;

      default:
	r = write_remaining(do_write, data, col, &start, &len);
	if (r != MAILIMF_NO_ERROR)
	  return r;
        start = text + i;
	snprintf(hexstr, 6, "=%02X", '\r');
	r = mailimf_string_write_driver(do_write, data, col, hexstr, 3);
	if (r != MAILIMF_NO_ERROR)
	  return r;
	state = STATE_INIT;
	break;
      }
      break;

    case STATE_SPACE:
      switch (ch) {
      case '\r':
	state = STATE_SPACE_CR;
	i ++;
	break;

      case '\n':
        r = write_remaining(do_write, data, col, &start, &len);
        if (r != MAILIMF_NO_ERROR)
          return r;
        start = text + i + 1;
        snprintf(hexstr, 6, "=%02X\r\n", text[i - 1]);
        r = mailimf_string_write_driver(do_write, data, col, hexstr, strlen(hexstr));
        if (r != MAILIMF_NO_ERROR)
          return r;
        state = STATE_INIT;
	i ++;
        break;
        
      case ' ':
      case '\t':
	len ++;
        i ++;
        break;

      default:
#if 0
	len += 2;
        state = STATE_INIT;
        i ++;
#endif
        len ++;
        state = STATE_INIT;
	break;
      }

      break;

    case STATE_SPACE_CR:
      switch (ch) {
      case '\n':
	r = write_remaining(do_write, data, col, &start, &len);
	if (r != MAILIMF_NO_ERROR)
	  return r;
        start = text + i + 1;
	snprintf(hexstr, 6, "=%02X\r\n", text[i - 2]);
	r = mailimf_string_write_driver(do_write, data, col, hexstr, strlen(hexstr));
	if (r != MAILIMF_NO_ERROR)
	  return r;
	state = STATE_INIT;
        i ++;
	break;

      default:
	r = write_remaining(do_write, data, col, &start, &len);
	if (r != MAILIMF_NO_ERROR)
	  return r;
        start = text + i + 1;
	snprintf(hexstr, 6, "%c=%02X", text[i - 2], '\r');
	r = mailimf_string_write_driver(do_write, data, col, hexstr, strlen(hexstr));
	if (r != MAILIMF_NO_ERROR)
	  return r;
	state = STATE_INIT;
	break;
      }

      break;
    }
  }

  return MAILIMF_NO_ERROR;
}

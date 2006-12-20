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
 * $Id: mailmime_types_helper.c,v 1.26 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime_types_helper.h"

#include "clist.h"
#include "mailmime.h"

#include <string.h>
#include <time.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <stdlib.h>
#ifdef _MSC_VER
#	include "win_etpan.h"
#endif

#define MIME_VERSION (1 << 16)

int mailmime_transfer_encoding_get(struct mailmime_fields * fields)
{
  clistiter * cur;

  for(cur = clist_begin(fields->fld_list) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_field * field;

    field = clist_content(cur);
    if (field->fld_type == MAILMIME_FIELD_TRANSFER_ENCODING)
      return field->fld_data.fld_encoding->enc_type;
  }

  return MAILMIME_MECHANISM_8BIT;
}

struct mailmime_disposition *
mailmime_disposition_new_filename(int type, char * filename)
{
  return mailmime_disposition_new_with_data(type, filename,
    NULL, NULL, NULL, (size_t) -1);

}

struct mailmime_fields * mailmime_fields_new_empty(void)
{
  clist * list;
  struct mailmime_fields * fields;

  list = clist_new();
  if (list == NULL)
    goto err;

  fields = mailmime_fields_new(list);
  if (fields == NULL)
    goto free;

  return fields;

 free:
  clist_free(list);
 err:
  return NULL;
}

int mailmime_fields_add(struct mailmime_fields * fields,
			struct mailmime_field * field)
{
  int r;

  r = clist_append(fields->fld_list, field);
  if (r < 0)
    return MAILIMF_ERROR_MEMORY;

  return MAILIMF_NO_ERROR;
}

static void mailmime_field_detach(struct mailmime_field * field)
{
  switch (field->fld_type) {
  case MAILMIME_FIELD_TYPE:
    field->fld_data.fld_content = NULL;
    break;
  case MAILMIME_FIELD_TRANSFER_ENCODING:
    field->fld_data.fld_encoding = NULL;
    break;
  case MAILMIME_FIELD_ID:
    field->fld_data.fld_id = NULL;
    break;
  case MAILMIME_FIELD_DESCRIPTION:
    field->fld_data.fld_description = NULL;
    break;
  case MAILMIME_FIELD_DISPOSITION:
    field->fld_data.fld_disposition = NULL;
    break;
  case MAILMIME_FIELD_LANGUAGE:
    field->fld_data.fld_language = NULL;
    break;
  }
}

struct mailmime_fields *
mailmime_fields_new_with_data(struct mailmime_mechanism * encoding,
			      char * id,
			      char * description,
			      struct mailmime_disposition * disposition,
			      struct mailmime_language * language)
{
  struct mailmime_field * field;
  struct mailmime_fields * fields;
  int r;

  fields = mailmime_fields_new_empty();
  if (fields == NULL)
    goto err;

#if 0
  if (content != NULL) {
    field = mailmime_field_new(MAILMIME_FIELD_TYPE,
			       content, NULL, NULL, NULL, 0, NULL, NULL);
    if (field == NULL)
      goto free;

    r = mailmime_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR) {
      mailmime_field_detach(field);
      mailmime_field_free(field);
      goto free;
    }
  }
#endif

  if (encoding != NULL) {
    field = mailmime_field_new(MAILMIME_FIELD_TRANSFER_ENCODING,
			       NULL, encoding, NULL, NULL, 0, NULL, NULL);
    if (field == NULL)
      goto free;

    r = mailmime_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR) {
      mailmime_field_detach(field);
      mailmime_field_free(field);
      goto free;
    }
  }

  if (id != NULL) {
    field = mailmime_field_new(MAILMIME_FIELD_ID,
			       NULL, NULL, id, NULL, 0, NULL, NULL);
    if (field == NULL)
      goto free;

    r = mailmime_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR) {
      mailmime_field_detach(field);
      mailmime_field_free(field);
      goto free;
    }
  }

  if (description != NULL) {
    field = mailmime_field_new(MAILMIME_FIELD_DESCRIPTION,
			       NULL, NULL, NULL, description, 0, NULL, NULL);
    if (field == NULL)
      goto free;

    r = mailmime_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR) {
      mailmime_field_detach(field);
      mailmime_field_free(field);
      goto free;
    }
  }

  if (disposition != NULL) {
    field = mailmime_field_new(MAILMIME_FIELD_DISPOSITION,
			       NULL, NULL, NULL, NULL, 0, disposition, NULL);
    if (field == NULL)
      goto free;

    r = mailmime_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR) {
      mailmime_field_detach(field);
      mailmime_field_free(field);
      goto free;
    }
  }

  if (language != NULL) {
    field = mailmime_field_new(MAILMIME_FIELD_DISPOSITION,
			       NULL, NULL, NULL, NULL, 0, NULL, language);
    if (field == NULL)
      goto free;

    r = mailmime_fields_add(fields, field);
    if (r != MAILIMF_NO_ERROR) {
      mailmime_field_detach(field);
      mailmime_field_free(field);
      goto free;
    }
  }

  return fields;

 free:
  clist_foreach(fields->fld_list, (clist_func) mailmime_field_detach, NULL);
  mailmime_fields_free(fields);
 err:
  return NULL;
}

struct mailmime_fields *
mailmime_fields_new_with_version(struct mailmime_mechanism * encoding,
				 char * id,
				 char * description,
				 struct mailmime_disposition * disposition,
				 struct mailmime_language * language)
{
  struct mailmime_field * field;
  struct mailmime_fields * fields;
  int r;

  fields = mailmime_fields_new_with_data(encoding, id, description,
					 disposition, language);
  if (fields == NULL)
    goto err;

  field = mailmime_field_new(MAILMIME_FIELD_VERSION,
			     NULL, NULL, NULL, NULL, MIME_VERSION, NULL, NULL);
  if (field == NULL)
    goto free;

  r = mailmime_fields_add(fields, field);
  if (r != MAILIMF_NO_ERROR) {
    mailmime_field_detach(field);
    mailmime_field_free(field);
    goto free;
  }

  return fields;

 free:
  clist_foreach(fields->fld_list, (clist_func) mailmime_field_detach, NULL);
  mailmime_fields_free(fields);
 err:
  return NULL;
}


struct mailmime_content * mailmime_get_content_message(void)
{
  clist * list;
  struct mailmime_composite_type * composite_type;
  struct mailmime_type * mime_type;
  struct mailmime_content * content;
  char * subtype;

  composite_type =
    mailmime_composite_type_new(MAILMIME_COMPOSITE_TYPE_MESSAGE,
				NULL);
  if (composite_type == NULL)
    goto err;
  
  mime_type = mailmime_type_new(MAILMIME_TYPE_COMPOSITE_TYPE,
				NULL, composite_type);
  if (mime_type == NULL)
    goto free_composite;
  composite_type = NULL;

  list = clist_new();
  if (list == NULL)
    goto free_mime_type;

  subtype = strdup("rfc822");
  if (subtype == NULL)
    goto free_list;

  content = mailmime_content_new(mime_type, subtype, list);
  if (content == NULL)
    goto free_subtype;

  return content;

 free_subtype:
  free(subtype);
 free_list:
  clist_free(list);
 free_mime_type:
  mailmime_type_free(mime_type);
 free_composite:
  if (composite_type != NULL)
    mailmime_composite_type_free(composite_type);
 err:
  return NULL;
}

struct mailmime_content * mailmime_get_content_text(void)
{
  clist * list;
  struct mailmime_discrete_type * discrete_type;
  struct mailmime_type * mime_type;
  struct mailmime_content * content;
  char * subtype;

  discrete_type = mailmime_discrete_type_new(MAILMIME_DISCRETE_TYPE_TEXT,
					     NULL);
  if (discrete_type == NULL)
    goto err;
  
  mime_type = mailmime_type_new(MAILMIME_TYPE_DISCRETE_TYPE,
				discrete_type, NULL);
  if (mime_type == NULL)
    goto free_discrete;
  discrete_type = NULL;
  
  list = clist_new();
  if (list == NULL)
    goto free_type;

  subtype = strdup("plain");
  if (subtype == NULL)
    goto free_list;
  
  content = mailmime_content_new(mime_type, subtype, list);
  if (content == NULL)
    goto free_subtype;

  return content;

 free_subtype:
  free(subtype);
 free_list:
  clist_free(list);
 free_type:
  mailmime_type_free(mime_type);
 free_discrete:
  if (discrete_type != NULL)
    mailmime_discrete_type_free(discrete_type);
 err:
  return NULL;
}








/* mailmime build */


#if 0
struct mailmime *
mailmime_new_message_file(char * filename)
{
  struct mailmime_content * content;
  struct mailmime * build_info;
  struct mailmime_data * msg_content;
  struct mailmime_fields * mime_fields;

  content = mailmime_get_content_message();
  if (content == NULL) {
    goto err;
  }

  mime_fields = mailmime_fields_new_with_version(NULL, NULL,
      NULL, NULL, NULL);
  if (mime_fields == NULL)
    goto free_content;

  msg_content = mailmime_data_new(MAILMIME_DATA_FILE, MAILMIME_MECHANISM_8BIT,
				  1, NULL, 0, filename);
  if (msg_content == NULL)
    goto free_fields;

  build_info = mailmime_new(MAILMIME_MESSAGE,
      NULL, 0, mime_fields, content,
      msg_content, NULL, NULL, NULL, NULL, NULL);
  if (build_info == NULL)
    goto free_msg_content;

  return build_info;

 free_msg_content:
  mailmime_data_free(msg_content);
 free_fields:
  mailmime_fields_free(mime_fields);
 free_content:
  mailmime_content_free(content);
 err:
  return NULL;
}

struct mailmime *
mailmime_new_message_text(char * data_str, size_t length)
{
  struct mailmime_content * content;
  struct mailmime * build_info;
  struct mailmime_data * msg_content;
  struct mailmime_fields * mime_fields;

  content = mailmime_get_content_message();
  if (content == NULL) {
    goto err;
  }

  mime_fields = mailmime_fields_new_with_version(NULL, NULL,
      NULL, NULL, NULL);
  if (mime_fields == NULL)
    goto free_fields;

  msg_content = mailmime_data_new(MAILMIME_DATA_TEXT, MAILMIME_MECHANISM_8BIT,
				  1, data_str, length, NULL);
  if (msg_content == NULL)
    goto free_content;

  build_info = mailmime_new(MAILMIME_MESSAGE,
      NULL, 0, mime_fields, content,
      msg_content, NULL, NULL, NULL,
      NULL, NULL);
  if (build_info == NULL)
    goto free_msg_content;

  return build_info;

 free_msg_content:
  mailmime_data_free(msg_content);
 free_fields:
  mailmime_fields_free(mime_fields);
 free_content:
  mailmime_content_free(content);
 err:
  return NULL;
}
#endif

struct mailmime *
mailmime_new_message_data(struct mailmime * msg_mime)
{
  struct mailmime_content * content;
  struct mailmime * build_info;
  struct mailmime_fields * mime_fields;

  content = mailmime_get_content_message();
  if (content == NULL)
    goto err;

  mime_fields = mailmime_fields_new_with_version(NULL, NULL,
      NULL, NULL, NULL);
  if (mime_fields == NULL)
    goto free_content;

  build_info = mailmime_new(MAILMIME_MESSAGE,
      NULL, 0, mime_fields, content,
      NULL, NULL, NULL, NULL,
      NULL, msg_mime);
  if (build_info == NULL)
    goto free_fields;

  return build_info;

 free_fields:
  mailmime_fields_free(mime_fields);
 free_content:
  mailmime_content_free(content);
 err:
  return NULL;
}

#define MAX_MESSAGE_ID 512

static char * generate_boundary()
{
  char id[MAX_MESSAGE_ID];
  time_t now;
  char name[MAX_MESSAGE_ID];
  long value;

  now = time(NULL);
  value = random();

  gethostname(name, MAX_MESSAGE_ID);
  snprintf(id, MAX_MESSAGE_ID, "%lx_%lx_%x", now, value, getpid());

  return strdup(id);
}

struct mailmime *
mailmime_new_empty(struct mailmime_content * content,
		   struct mailmime_fields * mime_fields)
{
  struct mailmime * build_info;
  clist * list;
  int r;
  int mime_type;
  
  list = NULL;
  
  switch (content->ct_type->tp_type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    mime_type = MAILMIME_SINGLE;
    break;

  case MAILMIME_TYPE_COMPOSITE_TYPE:
    switch (content->ct_type->tp_data.tp_composite_type->ct_type) {
    case MAILMIME_COMPOSITE_TYPE_MULTIPART:
      mime_type = MAILMIME_MULTIPLE;
      break;

    case MAILMIME_COMPOSITE_TYPE_MESSAGE:
      if (strcasecmp(content->ct_subtype, "rfc822") == 0)
        mime_type = MAILMIME_MESSAGE;
      else
	mime_type = MAILMIME_SINGLE;
      break;

    default:
      goto err;
    }
    break;

  default:
    goto err;
  }

  if (mime_type == MAILMIME_MULTIPLE) {
    char * attr_name;
    char * attr_value;
    struct mailmime_parameter * param;
    clist * parameters;
    char * boundary;

    list = clist_new();
    if (list == NULL)
      goto err;

    attr_name = strdup("boundary");
    if (attr_name == NULL)
      goto free_list;

    boundary = generate_boundary();
    attr_value = boundary;
    if (attr_name == NULL) {
      free(attr_name);
      goto free_list;
    }

    param = mailmime_parameter_new(attr_name, attr_value);
    if (param == NULL) {
      free(attr_value);
      free(attr_name);
      goto free_list;
    }

    if (content->ct_parameters == NULL) {
      parameters = clist_new();
      if (parameters == NULL) {
        mailmime_parameter_free(param);
        goto free_list;
      }
    }
    else
      parameters = content->ct_parameters;

    r = clist_append(parameters, param);
    if (r != 0) {
      clist_free(parameters);
      mailmime_parameter_free(param);
      goto free_list;
    }

    if (content->ct_parameters == NULL)
      content->ct_parameters = parameters;
  }

  build_info = mailmime_new(mime_type,
      NULL, 0, mime_fields, content,
      NULL, NULL, NULL, list,
      NULL, NULL);
  if (build_info == NULL) {
    clist_free(list);
    return NULL;
  }

  return build_info;

 free_list:
  clist_free(list);
 err:
  return NULL;
}


int
mailmime_new_with_content(const char * content_type,
			  struct mailmime_fields * mime_fields,
			  struct mailmime ** result)
{
  int r;
  size_t cur_token;
  struct mailmime_content * content;
  struct mailmime * build_info;
#if 0
  int mime_type;
#endif
  int res;

  cur_token = 0;
  r = mailmime_content_parse(content_type, strlen(content_type),
			     &cur_token,
			     &content);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

#if 0
  switch (content->type->type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    mime_type = MAILMIME_SINGLE;
    break;

  case MAILMIME_TYPE_COMPOSITE_TYPE:
    switch (content->type->composite_type->type) {
    case MAILMIME_COMPOSITE_TYPE_MULTIPART:
      mime_type = MAILMIME_MULTIPLE;
      break;

    case MAILMIME_COMPOSITE_TYPE_MESSAGE:
      if (strcasecmp(content->subtype, "rfc822") == 0)
        mime_type = MAILMIME_MESSAGE;
      else
	mime_type = MAILMIME_SINGLE;
      break;

    default:
      res = MAILIMF_ERROR_INVAL;
      goto free;
    }
    break;

  default:
    res = MAILIMF_ERROR_INVAL;
    goto free;
  }
#endif

  build_info = mailmime_new_empty(content, mime_fields);
  if (build_info == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = build_info;

  return MAILIMF_NO_ERROR;

 free:
  mailmime_content_free(content);
 err:
  return res;
}

int mailmime_set_preamble_file(struct mailmime * build_info,
			       char * filename)
{
  struct mailmime_data * data;

  data = mailmime_data_new(MAILMIME_DATA_FILE, MAILMIME_MECHANISM_8BIT,
			   0, NULL, 0, filename);
  if (data == NULL)
    return MAILIMF_ERROR_MEMORY;

  build_info->mm_data.mm_multipart.mm_preamble = data;
  
  return MAILIMF_NO_ERROR;
}

int mailmime_set_epilogue_file(struct mailmime * build_info,
			       char * filename)
{
  struct mailmime_data * data;

  data = mailmime_data_new(MAILMIME_DATA_FILE, MAILMIME_MECHANISM_8BIT,
			   0, NULL, 0, filename);
  if (data == NULL)
    return MAILIMF_ERROR_MEMORY;

  build_info->mm_data.mm_multipart.mm_epilogue = data;
  
  return MAILIMF_NO_ERROR;
}

int mailmime_set_preamble_text(struct mailmime * build_info,
			       char * data_str, size_t length)
{
  struct mailmime_data * data;

  data = mailmime_data_new(MAILMIME_DATA_TEXT, MAILMIME_MECHANISM_8BIT,
			   0, data_str, length, NULL);
  if (data == NULL)
    return MAILIMF_ERROR_MEMORY;

  build_info->mm_data.mm_multipart.mm_preamble = data;
  
  return MAILIMF_NO_ERROR;
}

int mailmime_set_epilogue_text(struct mailmime * build_info,
			       char * data_str, size_t length)
{
  struct mailmime_data * data;

  data = mailmime_data_new(MAILMIME_DATA_TEXT, MAILMIME_MECHANISM_8BIT,
			   0, data_str, length, NULL);
  if (data == NULL)
    return MAILIMF_ERROR_MEMORY;

  build_info->mm_data.mm_multipart.mm_epilogue = data;
  
  return MAILIMF_NO_ERROR;
}


int mailmime_set_body_file(struct mailmime * build_info,
			   char * filename)
{
  int encoding;
  struct mailmime_data * data;

  encoding = mailmime_transfer_encoding_get(build_info->mm_mime_fields);

  data = mailmime_data_new(MAILMIME_DATA_FILE, encoding,
			   0, NULL, 0, filename);
  if (data == NULL)
    return MAILIMF_ERROR_MEMORY;

  build_info->mm_data.mm_single = data;
  
  return MAILIMF_NO_ERROR;
}

int mailmime_set_body_text(struct mailmime * build_info,
			   char * data_str, size_t length)
{
  int encoding;
  struct mailmime_data * data;

  encoding = mailmime_transfer_encoding_get(build_info->mm_mime_fields);

  data = mailmime_data_new(MAILMIME_DATA_TEXT, encoding,
			   0, data_str, length, NULL);
  if (data == NULL)
    return MAILIMF_ERROR_MEMORY;

  build_info->mm_data.mm_single = data;
  
  return MAILIMF_NO_ERROR;
}


/* add a part as subpart of a mime part */

int mailmime_add_part(struct mailmime * build_info,
		      struct mailmime * part)
{
  int r;

  if (build_info->mm_type == MAILMIME_MESSAGE) {
    build_info->mm_data.mm_message.mm_msg_mime = part;
    part->mm_parent_type = MAILMIME_MESSAGE;
    part->mm_parent = build_info;
  }
  else if (build_info->mm_type == MAILMIME_MULTIPLE) {
    r = clist_append(build_info->mm_data.mm_multipart.mm_mp_list, part);
    if (r != 0)
      return MAILIMF_ERROR_MEMORY;
    
    part->mm_parent_type = MAILMIME_MULTIPLE;
    part->mm_parent = build_info;
    part->mm_multipart_pos =
      clist_end(build_info->mm_data.mm_multipart.mm_mp_list);
  }
  else {
    return MAILIMF_ERROR_INVAL;
  }
  return MAILIMF_NO_ERROR;
}

/* detach part from parent */

void mailmime_remove_part(struct mailmime * mime)
{
  struct mailmime * parent;

  parent = mime->mm_parent;
  if (parent == NULL)
    return;

  switch (mime->mm_parent_type) {
  case MAILMIME_MESSAGE:
    mime->mm_parent = NULL;
    parent->mm_data.mm_message.mm_msg_mime = NULL;
    break;

  case MAILMIME_MULTIPLE:
    mime->mm_parent = NULL;
    clist_delete(parent->mm_data.mm_multipart.mm_mp_list,
        mime->mm_multipart_pos);
    break;
  }
}


/*
  attach a part to a mime part and create multipart/mixed
  when needed, when the parent part has already some part
  attached to it.
*/

int mailmime_smart_add_part(struct mailmime * mime,
    struct mailmime * mime_sub)
{
  struct mailmime * saved_sub;
  struct mailmime * mp;
  int res;
  int r;

  switch (mime->mm_type) {
  case MAILMIME_SINGLE:
    res = MAILIMF_ERROR_INVAL;
    goto err;

  case MAILMIME_MULTIPLE:
    r = mailmime_add_part(mime, mime_sub);
    if (r != MAILIMF_NO_ERROR) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }

    return MAILIMF_NO_ERROR;
  }

  /* MAILMIME_MESSAGE */

  if (mime->mm_data.mm_message.mm_msg_mime == NULL) {
    /* there is no subpart, we can simply attach it */
    
    r = mailmime_add_part(mime, mime_sub);
    if (r != MAILIMF_NO_ERROR) {
      res = MAILIMF_ERROR_MEMORY;
      goto err;
    }

    return MAILIMF_NO_ERROR;
  }

  if (mime->mm_data.mm_message.mm_msg_mime->mm_type == MAILMIME_MULTIPLE) {
    /* in case the subpart is multipart, simply attach it to the subpart */
    
    return mailmime_add_part(mime->mm_data.mm_message.mm_msg_mime, mime_sub);
  }

  /* we save the current subpart, ... */

  saved_sub = mime->mm_data.mm_message.mm_msg_mime;

  /* create a multipart */
  
  mp = mailmime_multiple_new("multipart/mixed");
  if (mp == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  /* detach the saved subpart from the parent */

  mailmime_remove_part(saved_sub);
  
  /* the created multipart is the new child of the parent */

  r = mailmime_add_part(mime, mp);
  if (r != MAILIMF_NO_ERROR) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_mp;
  }

  /* then, attach the saved subpart and ... */
  
  r = mailmime_add_part(mp, saved_sub);
  if (r != MAILIMF_NO_ERROR) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_saved_sub;
  }

  /* the given part to the parent */

  r = mailmime_add_part(mp, mime_sub);
  if (r != MAILIMF_NO_ERROR) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_saved_sub;
  }

  return MAILIMF_NO_ERROR;

 free_mp:
  mailmime_free(mp);
 free_saved_sub:
  mailmime_free(saved_sub);
 err:
  return res;
}



/* detach part from parent and free it only if the part has no child */

int mailmime_smart_remove_part(struct mailmime * mime)
{
  struct mailmime * parent;
  int res;

  parent = mime->mm_parent;
  if (parent == NULL) {
    res = MAILIMF_ERROR_INVAL;
    goto err;
  }

  switch (mime->mm_type) {
  case MAILMIME_MESSAGE:
    if (mime->mm_data.mm_message.mm_msg_mime != NULL) {
      res = MAILIMF_ERROR_INVAL;
      goto err;
    }

    mailmime_remove_part(mime);
    
    mailmime_free(mime);

    return MAILIMF_NO_ERROR;

  case MAILMIME_MULTIPLE:
    if (!clist_isempty(mime->mm_data.mm_multipart.mm_mp_list)) {
      res = MAILIMF_ERROR_INVAL;
      goto err;
    }
      
    mailmime_remove_part(mime);
    
    mailmime_free(mime);
    
    return MAILIMF_NO_ERROR;

  case MAILMIME_SINGLE:
    mailmime_remove_part(mime);

    mailmime_free(mime);

    return MAILIMF_NO_ERROR;
    
  default:
    return MAILIMF_ERROR_INVAL;
  }

 err:
  return res;
}


/* create a mailmime_content structure (Content-Type field) */

struct mailmime_content * mailmime_content_new_with_str(const char * str)
{
  int r;
  size_t cur_token;
  struct mailmime_content * content;

  cur_token = 0;
  r =  mailmime_content_parse(str, strlen(str), &cur_token, &content);
  if (r != MAILIMF_NO_ERROR)
    return NULL;
  
  return content;
}

/* create MIME fields with only the field Content-Transfer-Encoding */

struct mailmime_fields * mailmime_fields_new_encoding(int type)
{
  struct mailmime_mechanism * encoding;
  struct mailmime_fields * mime_fields;

  encoding = mailmime_mechanism_new(type, NULL);
  if (encoding == NULL)
    goto err;

  mime_fields = mailmime_fields_new_with_data(encoding,
      NULL, NULL, NULL, NULL);
  if (mime_fields == NULL)
    goto free;

  return mime_fields;

 free:
  mailmime_mechanism_free(encoding);
 err:
  return NULL;
}


/* create a multipart MIME part */

struct mailmime * mailmime_multiple_new(const char * type)
{
  struct mailmime_fields * mime_fields;
  struct mailmime_content * content;
  struct mailmime * mp;

  mime_fields = mailmime_fields_new_encoding(MAILMIME_MECHANISM_8BIT);
  if (mime_fields == NULL)
    goto err;
  
  content = mailmime_content_new_with_str(type);
  if (content == NULL)
    goto free_fields;
  
  mp = mailmime_new_empty(content, mime_fields);
  if (mp == NULL)
    goto free_content;
  
  return mp;

 free_content:
  mailmime_content_free(content);
 free_fields:
  mailmime_fields_free(mime_fields);
 err:
  return NULL;
}



void mailmime_set_imf_fields(struct mailmime * build_info,
    struct mailimf_fields * mm_fields)
{
  build_info->mm_data.mm_message.mm_fields = mm_fields;
}

#if 0
struct mailmime_content * mailmime_get_content(char * mime_type)
{
  struct mailmime_content *content;
  int r;
  size_t cur_token;

  cur_token = 0;
  r = mailmime_content_parse(mime_type, strlen(mime_type),
			     &cur_token, &content);
  if (r != MAILIMF_NO_ERROR)
    return NULL;
  
  return content;
}
#endif




struct mailmime_disposition *
mailmime_disposition_new_with_data(int type,
    char * filename, char * creation_date, char * modification_date,
    char * read_date, size_t size)
{
  struct mailmime_disposition_type * dsp_type;
  clist * list;
  int r;
  struct mailmime_disposition_parm * parm;
  struct mailmime_disposition * dsp;

  dsp_type = mailmime_disposition_type_new(type, NULL);
  if (dsp_type == NULL)
    goto err;

  list = clist_new();
  if (list == NULL)
    goto free_dsp_type;

  if (filename != NULL) {
    parm = mailmime_disposition_parm_new(MAILMIME_DISPOSITION_PARM_FILENAME,
        filename, NULL, NULL, NULL, 0, NULL);
    if (parm == NULL)
      goto free_list;
    
    r = clist_append(list, parm);
    if (r < 0) {
      mailmime_disposition_parm_free(parm);
      goto free_list;
    }
  }

  if (creation_date != NULL) {
    parm = mailmime_disposition_parm_new(MAILMIME_DISPOSITION_PARM_CREATION_DATE,
        NULL, creation_date, NULL, NULL, 0, NULL);
    if (parm == NULL)
      goto free_list;
    
    r = clist_append(list, parm);
    if (r < 0) {
      mailmime_disposition_parm_free(parm);
      goto free_list;
    }
  }

  if (modification_date != NULL) {
    parm = mailmime_disposition_parm_new(MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE,
        NULL, NULL, modification_date, NULL, 0, NULL);
    if (parm == NULL)
      goto free_list;
    
    r = clist_append(list, parm);
    if (r < 0) {
      mailmime_disposition_parm_free(parm);
      goto free_list;
    }
  }

  if (read_date != NULL) {
    parm = mailmime_disposition_parm_new(MAILMIME_DISPOSITION_PARM_READ_DATE,
        NULL, NULL, NULL, read_date, 0, NULL);
    if (parm == NULL)
      goto free_list;
    
    r = clist_append(list, parm);
    if (r < 0) {
      mailmime_disposition_parm_free(parm);
      goto free_list;
    }
  }

  if (size != (size_t) -1) {
    parm = mailmime_disposition_parm_new(MAILMIME_DISPOSITION_PARM_SIZE,
        NULL, NULL, NULL, NULL, size, NULL);
    if (parm == NULL)
      goto free_list;
    
    r = clist_append(list, parm);
    if (r < 0) {
      mailmime_disposition_parm_free(parm);
      goto free_list;
    }
  }

  dsp = mailmime_disposition_new(dsp_type, list);

  return dsp;

 free_list:
  clist_foreach(list, (clist_func) mailmime_disposition_parm_free, NULL);
  clist_free(list);
 free_dsp_type:
  mailmime_disposition_type_free(dsp_type);
 err:
  return NULL;
}


static void mailmime_disposition_single_fields_init(struct
    mailmime_single_fields * single_fields,
    struct mailmime_disposition * fld_disposition)
{
  clistiter * cur;

  single_fields->fld_disposition = fld_disposition;

  for(cur = clist_begin(fld_disposition->dsp_parms) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailmime_disposition_parm * param;

    param = clist_content(cur);

    switch (param->pa_type) {
    case MAILMIME_DISPOSITION_PARM_FILENAME:
      single_fields->fld_disposition_filename = param->pa_data.pa_filename;
      break;

    case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
      single_fields->fld_disposition_creation_date =
        param->pa_data.pa_creation_date;
      break;
      
    case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
      single_fields->fld_disposition_modification_date =
        param->pa_data.pa_modification_date;
      break;
      
    case MAILMIME_DISPOSITION_PARM_READ_DATE:
      single_fields->fld_disposition_read_date =
        param->pa_data.pa_read_date;
      break;
      
    case MAILMIME_DISPOSITION_PARM_SIZE:
      single_fields->fld_disposition_size = param->pa_data.pa_size;
      break;
    }
  }
}

static void mailmime_content_single_fields_init(struct
    mailmime_single_fields * single_fields,
    struct mailmime_content * fld_content)
{
  clistiter * cur;

  single_fields->fld_content = fld_content;

  for(cur = clist_begin(fld_content->ct_parameters) ;
      cur != NULL ; cur = clist_next(cur)) {
    struct mailmime_parameter * param;
    
    param = clist_content(cur);

    if (strcasecmp(param->pa_name, "boundary") == 0)
      single_fields->fld_content_boundary = param->pa_value;

    if (strcasecmp(param->pa_name, "charset") == 0)
      single_fields->fld_content_charset = param->pa_value;

    if (strcasecmp(param->pa_name, "name") == 0)
      single_fields->fld_content_name = param->pa_value;
  }
}

void mailmime_single_fields_init(struct mailmime_single_fields * single_fields,
    struct mailmime_fields * fld_fields,
    struct mailmime_content * fld_content)
{
  clistiter * cur;

  memset(single_fields, 0, sizeof(struct mailmime_single_fields));

  if (fld_content != NULL)
    mailmime_content_single_fields_init(single_fields, fld_content);
  
  if (fld_fields == NULL)
    return;
  
  for(cur = clist_begin(fld_fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailmime_field * field;

    field = clist_content(cur);

    switch (field->fld_type) {
    case MAILMIME_FIELD_TYPE:
      mailmime_content_single_fields_init(single_fields,
          field->fld_data.fld_content);
      break;

    case MAILMIME_FIELD_TRANSFER_ENCODING:
      single_fields->fld_encoding = field->fld_data.fld_encoding;
      break;

    case MAILMIME_FIELD_ID:
      single_fields->fld_id = field->fld_data.fld_id;
      break;

    case MAILMIME_FIELD_DESCRIPTION:
      single_fields->fld_description = field->fld_data.fld_description;
      break;

    case MAILMIME_FIELD_VERSION:
      single_fields->fld_version = field->fld_data.fld_version;
      break;

    case MAILMIME_FIELD_DISPOSITION:
      mailmime_disposition_single_fields_init(single_fields,
          field->fld_data.fld_disposition);
      break;

    case MAILMIME_FIELD_LANGUAGE:
      single_fields->fld_language = field->fld_data.fld_language;
      break;
    }
  }
}

struct mailmime_single_fields *
mailmime_single_fields_new(struct mailmime_fields * fld_fields,
    struct mailmime_content * fld_content)
{
  struct mailmime_single_fields * single_fields;

  single_fields = malloc(sizeof(struct mailmime_single_fields));
  if (single_fields == NULL)
    goto err;

  mailmime_single_fields_init(single_fields, fld_fields, fld_content);

  return single_fields;

 err:
  return NULL;
}


void mailmime_single_fields_free(struct mailmime_single_fields *
    single_fields)
{
  free(single_fields);
}

struct mailmime_fields * mailmime_fields_new_filename(int dsp_type,
    char * filename, int encoding_type)
{
  struct mailmime_disposition * dsp;
  struct mailmime_mechanism * encoding;
  struct mailmime_fields * mime_fields;

  dsp = mailmime_disposition_new_with_data(dsp_type,
    filename, NULL, NULL, NULL, (size_t) -1);
  if (dsp == NULL)
    goto err;

  encoding = mailmime_mechanism_new(encoding_type, NULL);
  if (encoding == NULL)
    goto free_dsp;

  mime_fields = mailmime_fields_new_with_data(encoding,
			      NULL, NULL, dsp, NULL);
  if (mime_fields == NULL)
    goto free_encoding;

  return mime_fields;

 free_encoding:
  mailmime_encoding_free(encoding);
 free_dsp:
  mailmime_disposition_free(dsp);
 err:
  return NULL;
}

struct mailmime_data *
mailmime_data_new_data(int encoding, int encoded,
		       const char * data, size_t length)
{
  return mailmime_data_new(MAILMIME_DATA_TEXT, encoding, encoded, data, length, NULL);
}

struct mailmime_data *
mailmime_data_new_file(int encoding, int encoded,
		       char * filename)
{
  return mailmime_data_new(MAILMIME_DATA_FILE, encoding, encoded, NULL, 0, filename);
}


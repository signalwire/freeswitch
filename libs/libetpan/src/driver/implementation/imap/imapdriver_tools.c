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
 * $Id: imapdriver_tools.c,v 1.22 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "imapdriver_tools.h"

#include "maildriver.h"

#include <stdlib.h>
#include <string.h>

#include "mail.h"
#include "imapdriver_types.h"
#include "maildriver_tools.h"
#include "generic_cache.h"
#include "mailmessage.h"
#include "mail_cache_db.h"



static inline struct imap_session_state_data *
session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline struct imap_cached_session_state_data *
cached_session_get_data(mailsession * session)
{
  return session->sess_data;
}

static inline mailsession *
cached_session_get_ancestor(mailsession * session)
{
  return cached_session_get_data(session)->imap_ancestor;
}

static inline struct imap_session_state_data *
cached_session_get_ancestor_data(mailsession * session)
{
  return session_get_data(cached_session_get_ancestor(session));
}

static inline mailimap *
cached_session_get_imap_session(mailsession * session)
{
  return cached_session_get_ancestor_data(session)->imap_session;
}

static int imap_flags_to_flags(struct mailimap_msg_att_dynamic * att_dyn,
			       struct mail_flags ** result);


int imap_error_to_mail_error(int error)
{
  switch (error) {
  case MAILIMAP_NO_ERROR:
    return MAIL_NO_ERROR;

  case MAILIMAP_NO_ERROR_AUTHENTICATED:
    return MAIL_NO_ERROR_AUTHENTICATED;

  case MAILIMAP_NO_ERROR_NON_AUTHENTICATED:
    return MAIL_NO_ERROR_NON_AUTHENTICATED;

  case MAILIMAP_ERROR_BAD_STATE:
    return MAIL_ERROR_BAD_STATE;

  case MAILIMAP_ERROR_STREAM:
    return MAIL_ERROR_STREAM;

  case MAILIMAP_ERROR_PARSE:
    return MAIL_ERROR_PARSE;

  case MAILIMAP_ERROR_CONNECTION_REFUSED:
    return MAIL_ERROR_CONNECT;

  case MAILIMAP_ERROR_MEMORY:
    return MAIL_ERROR_MEMORY;
    
  case MAILIMAP_ERROR_FATAL:
    return MAIL_ERROR_FATAL;

  case MAILIMAP_ERROR_PROTOCOL:
    return MAIL_ERROR_PROTOCOL;

  case MAILIMAP_ERROR_DONT_ACCEPT_CONNECTION:
    return MAIL_ERROR_CONNECT;

  case MAILIMAP_ERROR_APPEND:
    return MAIL_ERROR_APPEND;

  case MAILIMAP_ERROR_NOOP:
    return MAIL_ERROR_NOOP;

  case MAILIMAP_ERROR_LOGOUT:
    return MAIL_ERROR_LOGOUT;

  case MAILIMAP_ERROR_CAPABILITY:
    return MAIL_ERROR_CAPABILITY;

  case MAILIMAP_ERROR_CHECK:
    return MAIL_ERROR_CHECK;

  case MAILIMAP_ERROR_CLOSE:
    return MAIL_ERROR_CLOSE;

  case MAILIMAP_ERROR_EXPUNGE:
    return MAIL_ERROR_EXPUNGE;

  case MAILIMAP_ERROR_COPY:
  case MAILIMAP_ERROR_UID_COPY:
    return MAIL_ERROR_COPY;

  case MAILIMAP_ERROR_CREATE:
    return MAIL_ERROR_CREATE;

  case MAILIMAP_ERROR_DELETE:
    return MAIL_ERROR_DELETE;

  case MAILIMAP_ERROR_EXAMINE:
    return MAIL_ERROR_EXAMINE;

  case MAILIMAP_ERROR_FETCH:
  case MAILIMAP_ERROR_UID_FETCH:
    return MAIL_ERROR_FETCH;

  case MAILIMAP_ERROR_LIST:
    return MAIL_ERROR_LIST;

  case MAILIMAP_ERROR_LOGIN:
    return MAIL_ERROR_LOGIN;

  case MAILIMAP_ERROR_LSUB:
    return MAIL_ERROR_LSUB;

  case MAILIMAP_ERROR_RENAME:
    return MAIL_ERROR_RENAME;

  case MAILIMAP_ERROR_SEARCH:
  case MAILIMAP_ERROR_UID_SEARCH:
    return MAIL_ERROR_SEARCH;

  case MAILIMAP_ERROR_SELECT:
    return MAIL_ERROR_SELECT;

  case MAILIMAP_ERROR_STATUS:
    return MAIL_ERROR_STATUS;

  case MAILIMAP_ERROR_STORE:
  case MAILIMAP_ERROR_UID_STORE:
    return MAIL_ERROR_STORE;

  case MAILIMAP_ERROR_SUBSCRIBE:
    return MAIL_ERROR_SUBSCRIBE;

  case MAILIMAP_ERROR_UNSUBSCRIBE:
    return MAIL_ERROR_UNSUBSCRIBE;

  case MAILIMAP_ERROR_STARTTLS:
    return MAIL_ERROR_STARTTLS;

  case MAILIMAP_ERROR_INVAL:
    return MAIL_ERROR_INVAL;

  default:
    return MAIL_ERROR_INVAL;
  }
}





static int
imap_body_parameter_to_content(struct mailimap_body_fld_param *
			       body_parameter,
			       char * subtype,
			       struct mailmime_type * mime_type,
			       struct mailmime_content ** result);

static int
imap_body_type_text_to_content_type(char * subtype,
				    struct mailimap_body_fld_param *
				    body_parameter,
				    struct mailmime_content ** result);


int imap_list_to_list(clist * imap_list, struct mail_list ** result)
{
  clistiter * cur;
  clist * list;
  struct mail_list * resp;
  int r;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(imap_list) ; cur != NULL ; cur = clist_next(cur)) {
    struct mailimap_mailbox_list * mb_list;
    char * new_mb;
    
    mb_list = clist_content(cur);

    new_mb = strdup(mb_list->mb_name);
    if (new_mb == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = clist_append(list, new_mb);
    if (r != 0) {
      free(new_mb);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  resp = mail_list_new(list);
  if (resp == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = resp;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) free, NULL);
  clist_free(list);
 err:
  return res;
}

int
section_to_imap_section(struct mailmime_section * section, int type,
			struct mailimap_section ** result)
{
  struct mailimap_section_part * section_part;
  struct mailimap_section * imap_section;
  clist * list;
  clistiter * cur;
  int r;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(section->sec_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    uint32_t value;
    uint32_t * id;

    value = * (uint32_t *) clist_content(cur);
    id = malloc(sizeof(* id));
    if (id == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
    * id = value;
    r  = clist_append(list, id);
    if (r != 0) {
      res = MAIL_ERROR_MEMORY;
      free(id);
      goto free_list;
    }
  }

  section_part = mailimap_section_part_new(list);
  if (section_part == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  imap_section = NULL;

  switch (type) {
  case IMAP_SECTION_MESSAGE:
    imap_section = mailimap_section_new_part(section_part);
    break;
  case IMAP_SECTION_HEADER:
    imap_section = mailimap_section_new_part_header(section_part);
    break;
  case IMAP_SECTION_MIME:
    imap_section = mailimap_section_new_part_mime(section_part);
    break;
  case IMAP_SECTION_BODY:
    imap_section = mailimap_section_new_part_text(section_part);
    break;
  }

  if (imap_section == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_part;
  }

  * result = imap_section;

  return MAIL_NO_ERROR;

 free_part:
  mailimap_section_part_free(section_part);
 free_list:
  if (list != NULL) {
    clist_foreach(list, (clist_func) free, NULL);
    clist_free(list);
  }
 err:
  return res;
}



static int
imap_body_media_basic_to_content_type(struct mailimap_media_basic *
				      media_basic,
				      struct mailimap_body_fld_param *
				      body_parameter,
				      struct mailmime_content ** result)
{
  struct mailmime_content * content;
  struct mailmime_type * mime_type;
  struct mailmime_discrete_type * discrete_type;
  struct mailmime_composite_type * composite_type;
  char * discrete_type_extension;
  int discrete_type_type;
  int composite_type_type;
  int mime_type_type;
  char * subtype;
  int r;
  int res;

  discrete_type = NULL;
  composite_type = NULL;
  discrete_type_extension = NULL;
  subtype = NULL;
  discrete_type_type = 0;
  composite_type_type = 0;
  mime_type_type = 0;

  switch (media_basic->med_type) {
  case MAILIMAP_MEDIA_BASIC_APPLICATION:
    mime_type_type = MAILMIME_TYPE_DISCRETE_TYPE;
    discrete_type_type = MAILMIME_DISCRETE_TYPE_APPLICATION;
    break;

  case MAILIMAP_MEDIA_BASIC_AUDIO:
    mime_type_type = MAILMIME_TYPE_DISCRETE_TYPE;
    discrete_type_type = MAILMIME_DISCRETE_TYPE_APPLICATION;
    break;

  case MAILIMAP_MEDIA_BASIC_IMAGE:
    mime_type_type = MAILMIME_TYPE_DISCRETE_TYPE;
    discrete_type_type = MAILMIME_DISCRETE_TYPE_IMAGE;
    break;

  case MAILIMAP_MEDIA_BASIC_MESSAGE:
    mime_type_type = MAILMIME_TYPE_COMPOSITE_TYPE;
    composite_type_type = MAILMIME_COMPOSITE_TYPE_MESSAGE;
    break;

  case MAILIMAP_MEDIA_BASIC_VIDEO:
    mime_type_type = MAILMIME_TYPE_DISCRETE_TYPE;
    discrete_type_type = MAILMIME_DISCRETE_TYPE_VIDEO;
    break;

  case MAILIMAP_MEDIA_BASIC_OTHER:
    mime_type_type = MAILMIME_TYPE_DISCRETE_TYPE;
    discrete_type_type = MAILMIME_DISCRETE_TYPE_EXTENSION;
    discrete_type_extension = media_basic->med_basic_type;
    if (discrete_type_extension == NULL) {
      res = MAIL_ERROR_INVAL;
      goto err;
    }

    break;

  default:
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  switch (mime_type_type) {
  case MAILMIME_TYPE_DISCRETE_TYPE:
    if (discrete_type_extension != NULL) {
      discrete_type_extension = strdup(discrete_type_extension);
      if (discrete_type_extension == NULL) {
        res = MAIL_ERROR_MEMORY;
        goto err;
      }
    }
    
    discrete_type = mailmime_discrete_type_new(discrete_type_type,
					       discrete_type_extension);
    if (discrete_type == NULL) {
      if (discrete_type_extension != NULL)
        free(discrete_type_extension);
      res = MAIL_ERROR_MEMORY;
      goto err;
    }

    break; 
    
  case MAILMIME_TYPE_COMPOSITE_TYPE:
    composite_type = mailmime_composite_type_new(composite_type_type,
						 NULL);
    if (composite_type == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    
    break; 

  default:
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  mime_type = mailmime_type_new(mime_type_type, discrete_type, composite_type);
  if (mime_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free;
  }

  r = imap_body_parameter_to_content(body_parameter, media_basic->med_subtype,
				     mime_type, &content);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_type;
  }

  * result = content;

  return MAIL_NO_ERROR;

 free_type:
  mailmime_type_free(mime_type);
 free:
  if (discrete_type != NULL)
    mailmime_discrete_type_free(discrete_type);
  if (composite_type != NULL)
    mailmime_composite_type_free(composite_type);
 err:
  return res;
}

static int
imap_disposition_to_mime_disposition(struct mailimap_body_fld_dsp * imap_dsp,
				     struct mailmime_disposition ** result)
{
  size_t cur_token;
  int r;
  struct mailmime_disposition_type * dsp_type;
  struct mailmime_disposition * dsp;
  clist * parameters;
  int res;

  cur_token = 0;
  r = mailmime_disposition_type_parse(imap_dsp->dsp_type,
      strlen(imap_dsp->dsp_type), &cur_token, &dsp_type);
  if (r != MAILIMF_NO_ERROR) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  parameters = clist_new();
  if (parameters == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  if (imap_dsp->dsp_attributes != NULL) {
    clistiter * cur;

    for(cur = clist_begin(imap_dsp->dsp_attributes->pa_list) ; cur != NULL ;
	cur = clist_next(cur)) {
      struct mailimap_single_body_fld_param * imap_param;
      struct mailmime_disposition_parm * dsp_param;
      struct mailmime_parameter * param;
      char * filename;
      char * creation_date;
      char * modification_date;
      char * read_date;
      size_t size;
      int type;

      imap_param = clist_content(cur);

      filename = NULL;
      creation_date = NULL;
      modification_date = NULL;
      read_date = NULL;
      size = 0;
      param = NULL;

      type = mailmime_disposition_guess_type(imap_param->pa_name,
          strlen(imap_param->pa_name), 0);

      switch (type) {
      case MAILMIME_DISPOSITION_PARM_FILENAME:
	if (strcasecmp(imap_param->pa_name, "filename") != 0) {
	  type = MAILMIME_DISPOSITION_PARM_PARAMETER;
	  break;
	}
	filename = strdup(imap_param->pa_value);
	if (filename == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_dsp_type;
	}
	break;

      case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
	if (strcasecmp(imap_param->pa_name, "creation-date") != 0) {
	  type = MAILMIME_DISPOSITION_PARM_PARAMETER;
	  break;
	}
	creation_date = strdup(imap_param->pa_value);
	if (creation_date == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_dsp_type;
	}
	break;

      case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
	if (strcasecmp(imap_param->pa_name, "modification-date") != 0) {
	  type = MAILMIME_DISPOSITION_PARM_PARAMETER;
	  break;
	}
	modification_date = strdup(imap_param->pa_value);
	if (modification_date == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_dsp_type;
	}
	break;
    
      case MAILMIME_DISPOSITION_PARM_READ_DATE:
	if (strcasecmp(imap_param->pa_name, "read-date") != 0) {
	  type = MAILMIME_DISPOSITION_PARM_PARAMETER;
	  break;
	}
	read_date = strdup(imap_param->pa_value);
	if (read_date == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_dsp_type;
	}
	break;

      case MAILMIME_DISPOSITION_PARM_SIZE:
	if (strcasecmp(imap_param->pa_name, "size") != 0) {
	  type = MAILMIME_DISPOSITION_PARM_PARAMETER;
	  break;
	}
	size = strtoul(imap_param->pa_value, NULL, 10);
	break;
      }

      if (type == MAILMIME_DISPOSITION_PARM_PARAMETER) {
	char * name;
	char * value;

	name = strdup(imap_param->pa_name);
	if (name == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_dsp_type;
	}
	
	value = strdup(imap_param->pa_value);
	if (value == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  free(name);
	  goto free_dsp_type;
	}

	param = mailmime_parameter_new(name, value);
	if (param == NULL) {
	  free(value);
	  free(name);
	  res = MAIL_ERROR_MEMORY;
	  goto free_dsp_type;
	}

      }

      dsp_param = mailmime_disposition_parm_new(type, filename,
						creation_date,
						modification_date,
						read_date,
						size, param);
      if (dsp_param == NULL) {
	if (filename != NULL)
	  free(filename);
	if (creation_date != NULL)
	  free(creation_date);
	if (modification_date != NULL)
	  free(modification_date);
	if (read_date != NULL)
	  free(read_date);
	if (param != NULL)
	  mailmime_parameter_free(param);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(parameters, dsp_param);
      if (r != 0) {
	mailmime_disposition_parm_free(dsp_param);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  dsp = mailmime_disposition_new(dsp_type, parameters);
  if (dsp == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = dsp;
 
  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(parameters,
		(clist_func) mailmime_disposition_parm_free, NULL);
  clist_free(parameters);
 free_dsp_type:
  mailmime_disposition_type_free(dsp_type);
 err:
  return res;
}

static int
imap_language_to_mime_language(struct mailimap_body_fld_lang * imap_lang,
			       struct mailmime_language ** result)
{
  clist * list;
  clistiter * cur;
  int res;
  char * single;
  int r;
  struct mailmime_language * lang;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  switch (imap_lang->lg_type) {
  case MAILIMAP_BODY_FLD_LANG_SINGLE:
    if (imap_lang->lg_data.lg_single != NULL) {
      single = strdup(imap_lang->lg_data.lg_single);
      if (single == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free;
      }
      r = clist_append(list, single);
      if (r < 0) {
	free(single);
	res = MAIL_ERROR_MEMORY;
	goto free;
      }
    }

    break;

  case MAILIMAP_BODY_FLD_LANG_LIST:
    for(cur = clist_begin(imap_lang->lg_data.lg_list) ;
        cur != NULL ; cur = clist_next(cur)) {
      char * original_single;
      
      original_single = clist_content(cur);
      
      single = strdup(original_single);
      if (single == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free;
      }
      r = clist_append(list, single);
      if (r < 0) {
	free(single);
	res = MAIL_ERROR_MEMORY;
	goto free;
      }
    }
  }

  lang = mailmime_language_new(list);
  if (lang == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free;
  }

  * result = lang;

  return MAIL_NO_ERROR;

 free: 
  clist_foreach(list, (clist_func) free, NULL);
  clist_free(list);
 err:
  return res; 
}

static int
imap_body_fields_to_mime_fields(struct mailimap_body_fields * body_fields,
    struct mailimap_body_fld_dsp * imap_dsp,
    struct mailimap_body_fld_lang * imap_lang,
    struct mailmime_fields ** result,
    uint32_t * pbody_size)
{
  struct mailmime_field * mime_field;
  struct mailmime_fields * mime_fields;
  clist * list;
  char * id;
  struct mailmime_mechanism * encoding;
  char * description;
  struct mailmime_disposition * dsp;
  struct mailmime_language * lang;
  int type;
  int r;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
 
  if (body_fields != NULL) {
    
    if (pbody_size != NULL)
      * pbody_size = body_fields->bd_size;
    
    if (body_fields->bd_id != NULL) {
      type = MAILMIME_FIELD_ID;
      id = strdup(body_fields->bd_id);
      if (id == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      mime_field = mailmime_field_new(type, NULL,
          NULL, id, NULL, 0, NULL, NULL);
      if (mime_field == NULL) {
	free(id);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, mime_field);
      if (r != 0) {
	mailmime_field_free(mime_field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }

    if (body_fields->bd_description != NULL) {
      type = MAILMIME_FIELD_DESCRIPTION;
      description = strdup(body_fields->bd_description);
      if (description == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      mime_field = mailmime_field_new(type, NULL,
          NULL, NULL, description, 0, NULL, NULL);
      if (mime_field == NULL) {
	free(description);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, mime_field);
      if (r != 0) {
	mailmime_field_free(mime_field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  
    if (body_fields->bd_encoding != NULL) {
      char * encoding_value;
      int encoding_type;

      type = MAILMIME_FIELD_TRANSFER_ENCODING;

      encoding_value = NULL;
      switch (body_fields->bd_encoding->enc_type) {
      case MAILIMAP_BODY_FLD_ENC_7BIT:
	encoding_type = MAILMIME_MECHANISM_7BIT;
	break;
      case MAILIMAP_BODY_FLD_ENC_8BIT:
	encoding_type = MAILMIME_MECHANISM_8BIT;
	break;
      case MAILIMAP_BODY_FLD_ENC_BINARY:
	encoding_type = MAILMIME_MECHANISM_BINARY;
	break;
      case MAILIMAP_BODY_FLD_ENC_BASE64:
	encoding_type = MAILMIME_MECHANISM_BASE64;
	break;
      case MAILIMAP_BODY_FLD_ENC_QUOTED_PRINTABLE:
	encoding_type = MAILMIME_MECHANISM_QUOTED_PRINTABLE;
	break;
      case MAILIMAP_BODY_FLD_ENC_OTHER:
	encoding_type = MAILMIME_MECHANISM_TOKEN;
	encoding_value = strdup(body_fields->bd_encoding->enc_value);
	if (encoding_value == NULL) {
	  res = MAIL_ERROR_MEMORY;
	  goto free_list;
	}
	break;
      default:
	res = MAIL_ERROR_INVAL;
	goto free_list;
      }

      encoding = mailmime_mechanism_new(encoding_type, encoding_value);
      if (encoding == NULL) {
	if (encoding_value != NULL)
	  free(encoding_value);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      mime_field = mailmime_field_new(type, NULL,
          encoding, NULL, NULL, 0, NULL, NULL);
      if (mime_field == NULL) {
	mailmime_mechanism_free(encoding);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, mime_field);
      if (r != 0) {
	mailmime_field_free(mime_field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (imap_dsp != NULL) {
    r = imap_disposition_to_mime_disposition(imap_dsp, &dsp);
    if (r != MAIL_ERROR_PARSE) {
      if (r != MAIL_NO_ERROR) {
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      type = MAILMIME_FIELD_DISPOSITION;
      
      mime_field = mailmime_field_new(type, NULL,
				      NULL, NULL, NULL, 0, dsp, NULL);
      if (mime_field == NULL) {
	mailmime_disposition_free(dsp);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      r = clist_append(list, mime_field);
      if (r != 0) {
	mailmime_field_free(mime_field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (imap_lang != NULL) {
    r = imap_language_to_mime_language(imap_lang, &lang);
    if (r != MAIL_NO_ERROR) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    type = MAILMIME_FIELD_LANGUAGE;

    mime_field = mailmime_field_new(type, NULL,
        NULL, NULL, NULL, 0, NULL, lang);
    if (mime_field == NULL) {
      mailmime_language_free(lang);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = clist_append(list, mime_field);
    if (r != 0) {
      mailmime_field_free(mime_field);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  mime_fields = mailmime_fields_new(list);
  if (mime_fields == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = mime_fields;
  
  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailmime_fields_free, NULL);
  clist_free(list);
 err:
  return res;
}

static int
imap_body_type_basic_to_body(struct mailimap_body_type_basic *
			     imap_type_basic,
			     struct mailimap_body_ext_1part *
			     body_ext_1part,
			     struct mailmime ** result)
{
  struct mailmime_content * content;
  struct mailmime_fields * mime_fields;
  struct mailmime * body;
  int r;
  int res;
  uint32_t mime_size;

  content = NULL;
  r = imap_body_media_basic_to_content_type(imap_type_basic->bd_media_basic,
      imap_type_basic->bd_fields->bd_parameter, &content);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  if (body_ext_1part != NULL)
    r = imap_body_fields_to_mime_fields(imap_type_basic->bd_fields,
        body_ext_1part->bd_disposition,
        body_ext_1part->bd_language,
        &mime_fields, &mime_size);
  else
    r = imap_body_fields_to_mime_fields(imap_type_basic->bd_fields,
        NULL, NULL,
        &mime_fields, &mime_size);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_content;
  }

  body = mailmime_new(MAILMIME_SINGLE, NULL,
      mime_size, mime_fields, content,
      NULL, NULL, NULL, NULL, NULL, NULL);

  if (body == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fields;
  }

  * result = body;

  return MAIL_NO_ERROR;

 free_fields:
  mailmime_fields_free(mime_fields);
 free_content:
  mailmime_content_free(content);
 err:
  return res;
}

static int
imap_body_type_text_to_body(struct mailimap_body_type_text *
			    imap_type_text,
			    struct mailimap_body_ext_1part *
			    body_ext_1part,
			    struct mailmime ** result)
{
  struct mailmime_content * content;
  struct mailmime_fields * mime_fields;
  struct mailmime * body;
  int r;
  int res;
  uint32_t mime_size;

  r = imap_body_type_text_to_content_type(imap_type_text->bd_media_text,
      imap_type_text->bd_fields->bd_parameter,
      &content);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  if (body_ext_1part == NULL) {
    r = imap_body_fields_to_mime_fields(imap_type_text->bd_fields,
        NULL, NULL,
        &mime_fields, &mime_size);
  }
  else {
    r = imap_body_fields_to_mime_fields(imap_type_text->bd_fields,
        body_ext_1part->bd_disposition,
        body_ext_1part->bd_language,
        &mime_fields, &mime_size);
  }
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_content;
  }

  body = mailmime_new(MAILMIME_SINGLE, NULL,
      mime_size, mime_fields, content,
      NULL, NULL, NULL, NULL, NULL, NULL);

  if (body == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fields;
  }

  * result = body;

  return MAIL_NO_ERROR;

 free_fields:
  mailmime_fields_free(mime_fields);
 free_content:
  mailmime_content_free(content);
 err:
  return res;
}

static int
imap_body_parameter_to_content(struct mailimap_body_fld_param *
			       body_parameter,
			       char * subtype,
			       struct mailmime_type * mime_type,
			       struct mailmime_content ** result)
{
  clist * parameters;
  char * new_subtype;
  struct mailmime_content * content;
  int r;
  int res;

  new_subtype = strdup(subtype);
  if (new_subtype == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  parameters = clist_new();
  if (parameters == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_subtype;
  }

  if (body_parameter != NULL) {
    clistiter * cur;

    for(cur = clist_begin(body_parameter->pa_list) ; cur != NULL ;
	cur = clist_next(cur)) {
      struct mailimap_single_body_fld_param * imap_param;
      struct mailmime_parameter * param;
      char * name;
      char * value;

      imap_param = clist_content(cur);
      name = strdup(imap_param->pa_name);
      if (name == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_parameters;
      }

      value = strdup(imap_param->pa_value);
      if (value == NULL) {
	free(name);
	res = MAIL_ERROR_MEMORY;
	goto free_parameters;
      }

      param = mailmime_parameter_new(name, value);
      if (param == NULL) {
	free(value);
	free(name);
	res = MAIL_ERROR_MEMORY;
	goto free_parameters;
      }

      r = clist_append(parameters, param);
      if (r != 0) {
	mailmime_parameter_free(param);
	res = MAIL_ERROR_MEMORY;
	goto free_parameters;
      }
    }
  }

  content = mailmime_content_new(mime_type, new_subtype, parameters);
  if (content == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_parameters;
  }

  * result = content;

  return MAIL_NO_ERROR;

 free_parameters:
  clist_foreach(parameters, (clist_func) mailmime_parameter_free, NULL);
  clist_free(parameters);
 free_subtype:
  free(new_subtype);
 err:
  return res;
}

static int
imap_body_type_text_to_content_type(char * subtype,
				    struct mailimap_body_fld_param *
				    body_parameter,
				    struct mailmime_content ** result)
{
  struct mailmime_content * content;
  struct mailmime_type * mime_type;
  struct mailmime_discrete_type * discrete_type;
  int r;
  int res;

  discrete_type = NULL;

  discrete_type = mailmime_discrete_type_new(MAILMIME_DISCRETE_TYPE_TEXT,
					     NULL);
  if (discrete_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
    
  mime_type = mailmime_type_new(MAILMIME_TYPE_DISCRETE_TYPE,
				discrete_type, NULL);
  if (mime_type == NULL) {
    mailmime_discrete_type_free(discrete_type);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = imap_body_parameter_to_content(body_parameter, subtype,
				     mime_type, &content);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_type;
  }

  * result = content;

  return MAIL_NO_ERROR;

 free_type:
  mailmime_type_free(mime_type);
 err:
  return res;
}


static int
imap_body_type_msg_to_body(struct mailimap_body_type_msg *
			   imap_type_msg,
			   struct mailimap_body_ext_1part *
			   body_ext_1part,
			   struct mailmime ** result)
{
  struct mailmime * body;
  struct mailmime * msg_body;
  struct mailmime_fields * mime_fields;
  struct mailmime_composite_type * composite_type;
  struct mailmime_type * mime_type;
  struct mailmime_content * content_type;
  struct mailimf_fields * fields;
  int r;
  int res;
  uint32_t mime_size;
  
  r = imap_body_fields_to_mime_fields(imap_type_msg->bd_fields,
      body_ext_1part->bd_disposition, body_ext_1part->bd_language,
      &mime_fields, &mime_size);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = imap_env_to_fields(imap_type_msg->bd_envelope, NULL, 0, &fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_mime_fields;
  }

  r = imap_body_to_body(imap_type_msg->bd_body, &msg_body);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_fields;
  }
  
  composite_type =
    mailmime_composite_type_new(MAILMIME_COMPOSITE_TYPE_MESSAGE,
				NULL);
  if (composite_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fields;
  }
  
  mime_type = mailmime_type_new(MAILMIME_TYPE_COMPOSITE_TYPE,
				NULL, composite_type);
  if (mime_type == NULL) {
    mailmime_composite_type_free(composite_type);
    res = MAIL_ERROR_MEMORY;
    goto free_fields;
  }

  r = imap_body_parameter_to_content(imap_type_msg->bd_fields->bd_parameter,
      "rfc822", mime_type, &content_type);
  if (r != MAIL_NO_ERROR) {
    mailmime_type_free(mime_type);
    res = MAIL_ERROR_MEMORY;
    goto free_fields;
  }

  body = mailmime_new(MAILMIME_MESSAGE, NULL,
      mime_size, mime_fields, content_type,
      NULL, NULL, NULL, NULL, fields, msg_body);

  if (body == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_content;
  }

  * result = body;

  return MAIL_NO_ERROR;

 free_content:
  mailmime_content_free(content_type);
 free_fields:
  mailimf_fields_free(fields);
 free_mime_fields:
  mailmime_fields_free(mime_fields);
 err:
  return res;
}


static int
imap_body_type_1part_to_body(struct mailimap_body_type_1part *
			     type_1part,
			     struct mailmime ** result)
{
  struct mailmime * body;
  int r;
  int res;
  
  body = NULL;
  switch (type_1part->bd_type) {
  case MAILIMAP_BODY_TYPE_1PART_BASIC:
    r = imap_body_type_basic_to_body(type_1part->bd_data.bd_type_basic,
				     type_1part->bd_ext_1part,
				     &body);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }

    break;
  case MAILIMAP_BODY_TYPE_1PART_MSG:
    r = imap_body_type_msg_to_body(type_1part->bd_data.bd_type_msg,
				   type_1part->bd_ext_1part,
				   &body);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }

    break;
  case MAILIMAP_BODY_TYPE_1PART_TEXT:
    r = imap_body_type_text_to_body(type_1part->bd_data.bd_type_text,
				    type_1part->bd_ext_1part,
				    &body);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }

    break;
  }

  * result = body;

  return MAIL_NO_ERROR;

 err:
  return res;
}

static int
imap_body_type_mpart_to_body(struct mailimap_body_type_mpart *
			     type_mpart,
			     struct mailmime ** result)
{
  struct mailmime_fields * mime_fields;
  struct mailmime_composite_type * composite_type;
  struct mailmime_type * mime_type;
  struct mailmime_content * content_type;
  struct mailmime * body;
  clistiter * cur;
  clist * list;
  int r;
  int res;
  uint32_t mime_size;

  r = imap_body_fields_to_mime_fields(NULL,
      type_mpart->bd_ext_mpart->bd_disposition,
      type_mpart->bd_ext_mpart->bd_language,
      &mime_fields, &mime_size);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  composite_type =
    mailmime_composite_type_new(MAILMIME_COMPOSITE_TYPE_MULTIPART,
				NULL);
  if (composite_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fields;
  }
  
  mime_type = mailmime_type_new(MAILMIME_TYPE_COMPOSITE_TYPE,
				NULL, composite_type);
  if (mime_type == NULL) {
    mailmime_composite_type_free(composite_type);
    res = MAIL_ERROR_MEMORY;
    goto free_fields;
  }

  r = imap_body_parameter_to_content(type_mpart->bd_ext_mpart->bd_parameter,
				     type_mpart->bd_media_subtype,
				     mime_type, &content_type);
  if (r != MAIL_NO_ERROR) {
    mailmime_type_free(mime_type);
    res = r;
    goto free_fields;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_content;
  }

  for(cur = clist_begin(type_mpart->bd_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_body * imap_body;
    struct mailmime * sub_body;

    imap_body = clist_content(cur);

    r = imap_body_to_body(imap_body, &sub_body);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = clist_append(list, sub_body);
    if (r != 0) {
      mailmime_free(sub_body);
      res = r;
      goto free_list;
    }
  }

  body = mailmime_new(MAILMIME_MULTIPLE, NULL,
      mime_size, mime_fields, content_type,
      NULL, NULL, NULL, list, NULL, NULL);

  if (body == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  * result = body;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailmime_free, NULL);
  clist_free(list);
 free_content:
  mailmime_content_free(content_type);
 free_fields:
  mailmime_fields_free(mime_fields);
 err:
  return res;
}


int imap_body_to_body(struct mailimap_body * imap_body,
		      struct mailmime ** result)
{
  struct mailmime * body;
  int r;
  int res;

  body = NULL;
  switch (imap_body->bd_type) {
  case MAILIMAP_BODY_1PART:
    r = imap_body_type_1part_to_body(imap_body->bd_data.bd_body_1part, &body);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  case MAILIMAP_BODY_MPART:
    r = imap_body_type_mpart_to_body(imap_body->bd_data.bd_body_mpart, &body);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
    break;
  default:
    return MAIL_ERROR_INVAL;
  }
  
  * result = body;

  return MAIL_NO_ERROR;

 err:
  return res;
}

int imap_address_to_mailbox(struct mailimap_address * imap_addr,
			    struct mailimf_mailbox ** result)
{
  char * dsp_name;
  char * addr;
  struct mailimf_mailbox * mb;
  int res;

  if (imap_addr->ad_personal_name == NULL)
    dsp_name = NULL;
  else {
    dsp_name = strdup(imap_addr->ad_personal_name);
    if (dsp_name == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
  }

  if (imap_addr->ad_host_name == NULL) {
    addr = strdup(imap_addr->ad_mailbox_name);
    if (addr == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_name;
    }
  }
  else {
    addr = malloc(strlen(imap_addr->ad_mailbox_name) +
        strlen(imap_addr->ad_host_name) + 2);
    if (addr == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_name;
    }
    strcpy(addr, imap_addr->ad_mailbox_name);
    strcat(addr, "@");
    strcat(addr, imap_addr->ad_host_name);
  }

  mb = mailimf_mailbox_new(dsp_name, addr);
  if (mb == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_addr;
  }

  * result = mb;

  return MAIL_NO_ERROR;

 free_addr:
  free(addr);
 free_name:
  free(dsp_name);
 err:
  return res;
}

int imap_address_to_address(struct mailimap_address * imap_addr,
			    struct mailimf_address ** result)
{
  struct mailimf_address * addr;
  struct mailimf_mailbox * mb;
  int r;
  int res;

  r = imap_address_to_mailbox(imap_addr, &mb);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  addr = mailimf_address_new(MAILIMF_ADDRESS_MAILBOX, mb, NULL);
  if (addr == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_mb;
  }

  * result = addr;

  return MAIL_NO_ERROR;

 free_mb:
  mailimf_mailbox_free(mb);
 err:
  return res;
}

int
imap_mailbox_list_to_mailbox_list(clist * imap_mailbox_list,
				  struct mailimf_mailbox_list ** result)
{
  clistiter * cur;
  clist * list;
  struct mailimf_mailbox_list * mb_list;
  int r;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(imap_mailbox_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_address * imap_addr;
    struct mailimf_mailbox * mb;

    imap_addr = clist_content(cur);

    if (imap_addr->ad_mailbox_name == NULL)
      continue;

    r = imap_address_to_mailbox(imap_addr, &mb);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }
    
    r = clist_append(list, mb);
    if (r != 0) {
      mailimf_mailbox_free(mb);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  mb_list = mailimf_mailbox_list_new(list);
  if (mb_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = mb_list;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_mailbox_free, NULL);
  clist_free(list);
 err:
  return MAIL_ERROR_MEMORY;
}



/*
  at exit, imap_mb_list will fall on the last element of the group,
  where mailbox name will be NIL, so that imap_mailbox_list_to_address_list
  can continue
*/

static int imap_mailbox_list_to_group(clist * imap_mb_list, clistiter ** iter,
				      struct mailimf_group ** result)
{
  clistiter * imap_mailbox_listiter;
  clist * list;
  struct mailimf_group * group;
  struct mailimap_address * imap_addr;
  char * group_name;
  clistiter * cur;
  struct mailimf_mailbox_list * mb_list;
  int r;
  int res;

  imap_mailbox_listiter = * iter;

  imap_addr = clist_content(imap_mailbox_listiter);
  if (imap_addr->ad_mailbox_name == NULL) {
    res = MAIL_ERROR_INVAL;
    goto err;
  }

  group_name = strdup(imap_addr->ad_mailbox_name);
  if (group_name == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_group_name;
  }

  for(cur = clist_next(imap_mailbox_listiter) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_mailbox * mb;

    imap_addr = clist_content(cur);

    if (imap_addr->ad_mailbox_name == NULL) {
      break;
    }

    r = imap_address_to_mailbox(imap_addr, &mb);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_list;
    }

    r = clist_append(list, mb);
    if (r != 0) {
      mailimf_mailbox_free(mb);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  mb_list = mailimf_mailbox_list_new(list);
  if (mb_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  group = mailimf_group_new(group_name, mb_list);
  if (group == NULL) {
    mailimf_mailbox_list_free(mb_list);
    res = MAIL_ERROR_MEMORY;
    goto free_group_name;
  }

  * result = group;
  * iter = cur;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_mailbox_free, NULL);
  clist_free(list);
 free_group_name:
  free(group_name);
 err:
  return res;
}

int
imap_mailbox_list_to_address_list(clist * imap_mailbox_list,
				  struct mailimf_address_list ** result)
{
  clistiter * cur;
  clist * list;
  struct mailimf_address_list * addr_list;
  int r;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(imap_mailbox_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_address * imap_addr;
    struct mailimf_address * addr;

    imap_addr = clist_content(cur);

    if (imap_addr->ad_mailbox_name == NULL)
      continue;

    if ((imap_addr->ad_host_name == NULL) &&
        (imap_addr->ad_mailbox_name != NULL)) {
      struct mailimf_group * group;

      group = NULL;
      r = imap_mailbox_list_to_group(imap_mailbox_list, &cur, &group);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      addr = mailimf_address_new(MAILIMF_ADDRESS_GROUP, NULL, group);
      if (addr == NULL) {
	mailimf_group_free(group);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
    else {
      r = imap_address_to_address(imap_addr, &addr);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }
    }
    
    r = clist_append(list, addr);
    if (r != 0) {
      mailimf_address_free(addr);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  addr_list = mailimf_address_list_new(list);
  if (addr_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = addr_list;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_address_free, NULL);
  clist_free(list);
 err:
  return res;
}


int imap_add_envelope_fetch_att(struct mailimap_fetch_type * fetch_type)
{
  struct mailimap_fetch_att * fetch_att;
  int res;
  int r;
  char * header;
  clist * hdrlist;
  struct mailimap_header_list * imap_hdrlist;
  struct mailimap_section * section;

  fetch_att = mailimap_fetch_att_new_envelope();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  header = strdup("References");
  if (header == NULL) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  hdrlist = clist_new();
  if (hdrlist == NULL) {
    free(header);
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  r = clist_append(hdrlist, header);
  if (r < 0) {
    free(header);
    clist_foreach(hdrlist, (clist_func) free, NULL);
    clist_free(hdrlist);
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  imap_hdrlist = mailimap_header_list_new(hdrlist);
  if (imap_hdrlist == 0) {
    clist_foreach(hdrlist, (clist_func) free, NULL);
    clist_free(hdrlist);
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }
  
  section = mailimap_section_new_header_fields(imap_hdrlist);
  if (section == NULL) {
    mailimap_header_list_free(imap_hdrlist);
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  fetch_att = mailimap_fetch_att_new_body_peek_section(section);
  if (fetch_att == NULL) {
    mailimap_section_free(section);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  return MAIL_NO_ERROR;

 err:
  return res;
}


int imap_env_to_fields(struct mailimap_envelope * env,
		       char * ref_str, size_t ref_size,
		       struct mailimf_fields ** result)
{
  clist * list;
  struct mailimf_field * field;
  int r;
  struct mailimf_fields * fields;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  if (env->env_date != NULL) {
    size_t cur_token;
    struct mailimf_date_time * date_time;

    cur_token = 0;
    r = mailimf_date_time_parse(env->env_date, strlen(env->env_date),
        &cur_token, &date_time);

    if (r == MAILIMF_NO_ERROR) {
      struct mailimf_orig_date * orig;
      
      orig = mailimf_orig_date_new(date_time);
      if (orig == NULL) {
	mailimf_date_time_free(date_time);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_ORIG_DATE,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, orig, NULL,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_orig_date_free(orig);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (env->env_subject != NULL) {
    char * subject;
    struct mailimf_subject * subject_field;

    subject = strdup(env->env_subject);
    if (subject == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }


    subject_field = mailimf_subject_new(subject);
    if (subject_field == NULL) {
      free(subject);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    field = mailimf_field_new(MAILIMF_FIELD_SUBJECT,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, subject_field, NULL, NULL, NULL);
    if (field == NULL) {
      mailimf_subject_free(subject_field);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = clist_append(list, field);
    if (r != 0) {
      mailimf_field_free(field);
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }
  }

  if (env->env_from != NULL) {
    if (env->env_from->frm_list != NULL) {
      struct mailimf_mailbox_list * mb_list;
      struct mailimf_from * from;
      
      r = imap_mailbox_list_to_mailbox_list(env->env_from->frm_list, &mb_list);
      
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }
      
      from = mailimf_from_new(mb_list);
      if (from == NULL) {
	mailimf_mailbox_list_free(mb_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      field = mailimf_field_new(MAILIMF_FIELD_FROM,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, from,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_from_free(from);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (env->env_sender != NULL) {
    if (env->env_sender->snd_list != NULL) {
      struct mailimf_sender * sender;
      struct mailimf_mailbox * mb;
      
      r = imap_address_to_mailbox(clist_begin(env->env_sender->snd_list)->data, &mb);
      
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }
      
      sender = mailimf_sender_new(mb);
      if (sender == NULL) {
	mailimf_mailbox_free(mb);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_SENDER,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          sender, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_sender_free(sender);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (env->env_reply_to != NULL) {
    if (env->env_reply_to->rt_list != NULL) {
      struct mailimf_address_list * addr_list;
      struct mailimf_reply_to * reply_to;
      
      r = imap_mailbox_list_to_address_list(env->env_reply_to->rt_list,
          &addr_list);

      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      reply_to = mailimf_reply_to_new(addr_list);
      if (reply_to == NULL) {
	mailimf_address_list_free(addr_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_REPLY_TO,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, reply_to, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_reply_to_free(reply_to);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (env->env_to != NULL) {
    if (env->env_to->to_list != NULL) {
      struct mailimf_address_list * addr_list;
      struct mailimf_to * to;

      r = imap_mailbox_list_to_address_list(env->env_to->to_list, &addr_list);

      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      to = mailimf_to_new(addr_list);
      if (to == NULL) {
	mailimf_address_list_free(addr_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_TO,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, to, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_to_free(to);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (env->env_cc != NULL) {
    if (env->env_cc->cc_list != NULL) {
      struct mailimf_address_list * addr_list;
      struct mailimf_cc * cc;

      r = imap_mailbox_list_to_address_list(env->env_cc->cc_list, &addr_list);

      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      cc = mailimf_cc_new(addr_list);
      if (cc == NULL) {
	mailimf_address_list_free(addr_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_CC,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, NULL, cc, NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_cc_free(cc);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (env->env_bcc != NULL) {
    if (env->env_bcc->bcc_list != NULL) {
      struct mailimf_address_list * addr_list;
      struct mailimf_bcc * bcc;
      
      r = imap_mailbox_list_to_address_list(env->env_bcc->bcc_list,
          &addr_list);

      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      bcc = mailimf_bcc_new(addr_list);
      if (bcc == NULL) {
	mailimf_address_list_free(addr_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_BCC,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, bcc, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_bcc_free(bcc);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (env->env_in_reply_to != NULL) {
    struct mailimf_in_reply_to * in_reply_to;
    size_t cur_token;
    clist * msg_id_list;
      
    cur_token = 0;
    r = mailimf_msg_id_list_parse(env->env_in_reply_to,
        strlen(env->env_in_reply_to), &cur_token, &msg_id_list);

    switch (r) {
    case MAILIMF_NO_ERROR:
      in_reply_to = mailimf_in_reply_to_new(msg_id_list);
      if (in_reply_to == NULL) {
	clist_foreach(msg_id_list, (clist_func) mailimf_msg_id_free, NULL);
	clist_free(msg_id_list);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
	
      field = mailimf_field_new(MAILIMF_FIELD_IN_REPLY_TO,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL, NULL,
          in_reply_to,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_in_reply_to_free(in_reply_to);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
	
      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      break;

    case MAILIMF_ERROR_PARSE:
      break;

    default:
      res = maildriver_imf_error_to_mail_error(r);
      goto free_list;
    }
  }

  if (env->env_message_id != NULL) {
    char * id;
    struct mailimf_message_id * msg_id;
    size_t cur_token;

    cur_token = 0;
    r = mailimf_msg_id_parse(env->env_message_id, strlen(env->env_message_id),
        &cur_token, &id);
    switch (r) {
    case MAILIMF_NO_ERROR:

      msg_id = mailimf_message_id_new(id);
      if (msg_id == NULL) {
	mailimf_msg_id_free(id);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }

      field = mailimf_field_new(MAILIMF_FIELD_MESSAGE_ID,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL, msg_id, NULL,
          NULL, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_message_id_free(msg_id);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      
      r = clist_append(list, field);
      if (r != 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      break;

    case MAILIMF_ERROR_PARSE:
      break;

    default:
      res = maildriver_imf_error_to_mail_error(r);
      goto free_list;
    }
  }

  if (ref_str != NULL) {
    struct mailimf_references * references;
    size_t cur_token;

    cur_token = 0;
    r = mailimf_references_parse(ref_str, ref_size,
        &cur_token, &references);
    switch (r) {
    case MAILIMF_NO_ERROR:
      field = mailimf_field_new(MAILIMF_FIELD_REFERENCES,
          NULL, NULL, NULL, NULL, NULL, NULL, NULL,
          NULL, NULL, NULL,
          NULL, NULL, NULL, NULL, NULL, NULL,
          NULL,
          references, NULL, NULL, NULL, NULL);
      if (field == NULL) {
	mailimf_references_free(references);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
	
      r = clist_append(list, field);
      if (r < 0) {
	mailimf_field_free(field);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
      break;

    case MAILIMF_ERROR_PARSE:
      break;

    default:
      res = maildriver_imf_error_to_mail_error(r);
      goto free_list;
    }
  }

  fields = mailimf_fields_new(list);
  if (fields == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = fields;

  return MAIL_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailimf_field_free, NULL);
  clist_free(list);
 err:
  return res;
}

int imap_get_msg_att_info(struct mailimap_msg_att * msg_att,
			  uint32_t * puid,
			  struct mailimap_envelope ** pimap_envelope,
			  char ** preferences,
			  size_t * pref_size,
			  struct mailimap_msg_att_dynamic ** patt_dyn,
			  struct mailimap_body ** pimap_body)
{
  clistiter * item_cur;
  uint32_t uid;
  struct mailimap_envelope * imap_envelope;
  char * references;
  size_t ref_size;
  struct mailimap_msg_att_dynamic * att_dyn;
  struct mailimap_body * imap_body;

  uid = 0;
  imap_envelope = NULL;
  references = NULL;
  ref_size = 0;
  att_dyn = NULL;
  imap_body = NULL;

  for(item_cur = clist_begin(msg_att->att_list) ; item_cur != NULL ;
      item_cur = clist_next(item_cur)) {
    struct mailimap_msg_att_item * item;

    item = clist_content(item_cur);
      
    switch (item->att_type) {
    case MAILIMAP_MSG_ATT_ITEM_STATIC:
      switch (item->att_data.att_static->att_type) {
      case MAILIMAP_MSG_ATT_BODYSTRUCTURE:
	if (imap_body == NULL)
	  imap_body = item->att_data.att_static->att_data.att_bodystructure;
	break;

      case MAILIMAP_MSG_ATT_ENVELOPE:
	if (imap_envelope == NULL) {
	  imap_envelope = item->att_data.att_static->att_data.att_env;
	}
	break;
	  
      case MAILIMAP_MSG_ATT_UID:
	uid = item->att_data.att_static->att_data.att_uid;
	break;

      case MAILIMAP_MSG_ATT_BODY_SECTION:
	if (references == NULL) {
	  references = item->att_data.att_static->att_data.att_body_section->sec_body_part;
	  ref_size = item->att_data.att_static->att_data.att_body_section->sec_length;
	}
	break;
      }
      break;

    case MAILIMAP_MSG_ATT_ITEM_DYNAMIC:
      if (att_dyn == NULL) {
	att_dyn = item->att_data.att_dyn;
      }
      break;
    }
  }

  if (puid != NULL)
    * puid = uid;
  if (pimap_envelope != NULL)
    * pimap_envelope = imap_envelope;
  if (preferences != NULL)
    * preferences = references;
  if (pref_size != NULL)
    * pref_size = ref_size;
  if (patt_dyn != NULL)
    * patt_dyn = att_dyn;
  if (pimap_body != NULL)
    * pimap_body = imap_body;

  return MAIL_NO_ERROR;
}

int
imap_fetch_result_to_envelop_list(clist * fetch_result,
				  struct mailmessage_list * env_list)
{
  clistiter * cur;
  int r;
  unsigned int i;

  i = 0;

  for(cur = clist_begin(fetch_result) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_msg_att * msg_att;
    uint32_t uid;
    struct mailimap_envelope * imap_envelope;
    struct mailimap_msg_att_dynamic * att_dyn;
    char * references;
    size_t ref_size;

    msg_att = clist_content(cur);

    r = imap_get_msg_att_info(msg_att, &uid, &imap_envelope,
			      &references, &ref_size,
			      &att_dyn,
			      NULL);

    if (r == MAIL_NO_ERROR) {
      if (uid != 0) {
	while (i < carray_count(env_list->msg_tab)) {
	  mailmessage * msg;
	  
	  msg = carray_get(env_list->msg_tab, i);
	  
	  if (uid == msg->msg_index) {
	    struct mailimf_fields * fields;
	    struct mail_flags * flags;
	    
	    if (imap_envelope != NULL) {
	      r = imap_env_to_fields(imap_envelope,
				     references, ref_size, &fields);
	      if (r == MAIL_NO_ERROR) {
		msg->msg_fields = fields;
	      }
	    }
	  
	    if (att_dyn != NULL) {
	      r = imap_flags_to_flags(att_dyn, &flags);
	      
	      if (r == MAIL_NO_ERROR) {
		msg->msg_flags = flags;
	      }
	    }
	    
	    i ++;
	    break;
	  }
	  
	  i ++;
	}
      }
    }
  }

  return MAIL_NO_ERROR;
}


int mailimf_date_time_to_imap_date(struct mailimf_date_time * date,
				   struct mailimap_date ** result)
{
  struct mailimap_date * imap_date;
  
  imap_date = mailimap_date_new(date->dt_day, date->dt_month, date->dt_year);
  if (imap_date == NULL)
    return MAIL_ERROR_MEMORY;
  
  * result = imap_date;
  
  return MAIL_NO_ERROR;
}


#if 0
int mail_search_to_imap_search(struct mail_search_key * key,
			       struct mailimap_search_key ** result)
{
  struct mailimap_search_key * imap_key;

  char * bcc;
  struct mailimap_date * before;
  char * body;
  char * cc;
  char * from;
  struct mailimap_date * on;
  struct mailimap_date * since;
  char * subject;
  char * text;
  char * to;
  char * header_name;
  char * header_value;
  size_t larger;
  struct mailimap_search_key * not;
  struct mailimap_search_key * or1;
  struct mailimap_search_key * or2;
  size_t smaller;
  clist * multiple;
  int type;
  clistiter * cur;
  int r;
  int res;

  bcc = NULL;
  before = NULL;
  body = NULL;
  cc = NULL;
  from = NULL;
  on = NULL;
  since = NULL;
  subject = NULL;
  text = NULL;
  to = NULL;
  header_name = NULL;
  header_value = NULL;
  not = NULL;
  or1 = NULL;
  or2 = NULL;
  multiple = NULL;
  larger = 0;
  smaller = 0;
  
  switch (key->sk_type) {
  case MAIL_SEARCH_KEY_ALL:
    type = MAILIMAP_SEARCH_KEY_ALL;
    break;

  case MAIL_SEARCH_KEY_ANSWERED:
    type = MAILIMAP_SEARCH_KEY_ANSWERED;
    break;

  case MAIL_SEARCH_KEY_BCC:
    type = MAILIMAP_SEARCH_KEY_BCC;
    bcc = strdup(key->sk_bcc);
    if (bcc == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_BEFORE:
    type = MAILIMAP_SEARCH_KEY_BEFORE;
    r = mailimf_date_time_to_imap_date(key->sk_before, &before);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_BODY:
    type = MAILIMAP_SEARCH_KEY_BODY;
    body = strdup(key->sk_body);
    if (body == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_CC:
    type = MAILIMAP_SEARCH_KEY_CC;
    cc = strdup(key->sk_cc);
    if (cc == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_DELETED:
    type = MAILIMAP_SEARCH_KEY_DELETED;
    break;

  case MAIL_SEARCH_KEY_FLAGGED:
    type = MAILIMAP_SEARCH_KEY_FLAGGED;
    break;

  case MAIL_SEARCH_KEY_FROM:
    type = MAILIMAP_SEARCH_KEY_FROM;
    from = strdup(key->sk_from);
    if (from == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_NEW:
    type = MAILIMAP_SEARCH_KEY_NEW;
    break;

  case MAIL_SEARCH_KEY_OLD:
    type = MAILIMAP_SEARCH_KEY_OLD;
    break;

  case MAIL_SEARCH_KEY_ON:
    type = MAILIMAP_SEARCH_KEY_ON;
    r = mailimf_date_time_to_imap_date(key->sk_on, &on);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_RECENT:
    type = MAILIMAP_SEARCH_KEY_RECENT;
    break;

  case MAIL_SEARCH_KEY_SEEN:
    type = MAILIMAP_SEARCH_KEY_SEEN;
    break;

  case MAIL_SEARCH_KEY_SINCE:
    type = MAILIMAP_SEARCH_KEY_SINCE;
    r = mailimf_date_time_to_imap_date(key->sk_since, &since);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_SUBJECT:
    type = MAILIMAP_SEARCH_KEY_SUBJECT;
    subject = strdup(key->sk_subject);
    if (subject == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_TEXT:
    type = MAILIMAP_SEARCH_KEY_TEXT;
    text = strdup(key->sk_text);
    if (text == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_TO:
    type = MAILIMAP_SEARCH_KEY_TO;
    to = strdup(key->sk_to);
    if (to == NULL) {
      return MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_UNANSWERED:
    type = MAILIMAP_SEARCH_KEY_UNANSWERED;
    break;

  case MAIL_SEARCH_KEY_UNDELETED:
    type = MAILIMAP_SEARCH_KEY_UNFLAGGED;
    break;

  case MAIL_SEARCH_KEY_UNFLAGGED:
    type = MAILIMAP_SEARCH_KEY_UNANSWERED;
    break;

  case MAIL_SEARCH_KEY_UNSEEN:
    type = MAILIMAP_SEARCH_KEY_UNSEEN;
    break;

  case MAIL_SEARCH_KEY_HEADER:
    type = MAILIMAP_SEARCH_KEY_HEADER;
    header_name = strdup(key->sk_header_name);
    if (header_name == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    header_value = strdup(key->sk_header_value);
    if (header_value == NULL) {
      free(header_name);
      res = MAIL_ERROR_MEMORY;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_LARGER:
    type = MAILIMAP_SEARCH_KEY_LARGER;
    larger = key->sk_larger;
    break;

  case MAIL_SEARCH_KEY_NOT:
    type = MAILIMAP_SEARCH_KEY_NOT;
    r = mail_search_to_imap_search(key->sk_not, &not);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_OR:
    type = MAILIMAP_SEARCH_KEY_OR;
    r = mail_search_to_imap_search(key->sk_or1, &or1);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
    r = mail_search_to_imap_search(key->sk_or2, &or2);
    if (r != MAIL_NO_ERROR) {
      mailimap_search_key_free(or1);
      res = r;
      goto err;
    }
    break;

  case MAIL_SEARCH_KEY_SMALLER:
    type = MAILIMAP_SEARCH_KEY_SMALLER;
    smaller = key->sk_smaller;
    break;

  case MAIL_SEARCH_KEY_MULTIPLE:
    multiple = clist_new();
    if (multiple == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto err;
    }

    type = MAILIMAP_SEARCH_KEY_MULTIPLE;
    for(cur = clist_begin(key->sk_multiple) ; cur != NULL ;
        cur = clist_next(cur)) {
      struct mail_search_key * key_elt;
      struct mailimap_search_key * imap_key_elt;

      key_elt = clist_content(cur);
      r = mail_search_to_imap_search(key_elt, &imap_key_elt);
      if (r != MAIL_NO_ERROR) {
	res = r;
	goto free_list;
      }

      r = clist_append(multiple, imap_key_elt);
      if (r != 0) {
	mailimap_search_key_free(imap_key_elt);
	res = MAIL_ERROR_MEMORY;
	goto free_list;
      }
    }
    break;
    
  free_list:
    clist_foreach(multiple, (clist_func) mailimap_search_key_free, NULL);
    clist_free(multiple);
    goto err;

  default:
    return MAIL_ERROR_INVAL;
  }

  imap_key = mailimap_search_key_new(type, bcc, before, body, cc, from,
				     NULL, on, since, subject, text,
				     to, NULL, header_name,
				     header_value, larger, not, or1, or2,
				     NULL, NULL, NULL, smaller, NULL,
				     NULL, multiple);
  if (imap_key == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free;
  }

  * result = imap_key;

  return MAIL_NO_ERROR;

 free:
  if (bcc != NULL)
    free(bcc);
  if (before != NULL)
    mailimap_date_free(before);
  if (body != NULL)
    free(body);
  if (cc != NULL)
    free(cc);
  if (from != NULL)
    free(from);
  if (on != NULL)
    mailimap_date_free(on);
  if (since != NULL)
    mailimap_date_free(since);
  if (subject != NULL)
    free(subject);
  if (text != NULL)
    free(text);
  if (to != NULL)
    free(to);
  if (header_name != NULL)
    free(header_name);
  if (header_value != NULL)
    free(header_value);
  if (not != NULL)
    mailimap_search_key_free(not);
  if (or1 != NULL)
    mailimap_search_key_free(or1);
  if (or2 != NULL)
    mailimap_search_key_free(or2);
  clist_foreach(multiple, (clist_func) mailimap_search_key_free, NULL);
  clist_free(multiple);
 err:
  return res;
}
#endif


int msg_list_to_imap_set(clist * msg_list,
			 struct mailimap_set ** result)
{
  struct mailimap_set * imap_set;
  clistiter * cur;
  int previous_valid;
  uint32_t first_seq;
  uint32_t previous;
  int r;
  int res;

  imap_set = mailimap_set_new_empty();
  if (imap_set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  cur = clist_begin(msg_list);
  previous_valid = FALSE;
  first_seq = 0;
  previous = 0;
  while (1) {
    uint32_t * pindex;

    if ((cur == NULL) && (previous_valid)) {
      if (first_seq == previous) {
	r = mailimap_set_add_single(imap_set, first_seq);
	if (r != MAILIMAP_NO_ERROR) {
	  res = r;
	  goto free;
	}
      }
      else {
	r = mailimap_set_add_interval(imap_set, first_seq, previous);
	if (r != MAILIMAP_NO_ERROR) {
	  res = r;
	  goto free;
	}
      }
      break;
    }

    pindex = clist_content(cur);

    if (!previous_valid) {
      first_seq = * pindex;
      previous_valid = TRUE;
      previous = * pindex;
      cur = clist_next(cur);
    }
    else {
      if (* pindex != previous + 1) {
	if (first_seq == previous) {
	  r = mailimap_set_add_single(imap_set, first_seq);
	  if (r != MAILIMAP_NO_ERROR) {
	    res = r;
	    goto free;
	  }
	}
	else {
	  r = mailimap_set_add_interval(imap_set, first_seq, previous);
	  if (r != MAILIMAP_NO_ERROR) {
	    res = r;
	    goto free;
	  }
	}
	previous_valid = FALSE;
      }
      else {
	previous = * pindex;
	cur = clist_next(cur);
      }
    }
  }

  * result = imap_set;

  return MAIL_NO_ERROR;

 free:
  mailimap_set_free(imap_set);
 err:
  return res;
}


static int
uid_list_to_env_list(clist * fetch_result,
		     struct mailmessage_list ** result,
		     mailsession * session, mailmessage_driver * driver)
{
  clistiter * cur;
  struct mailmessage_list * env_list;
  int r;
  int res;
  carray * tab;
  unsigned int i;
  mailmessage * msg;

  tab = carray_new(128);
  if (tab == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(fetch_result) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_msg_att * msg_att;
    clistiter * item_cur;
    uint32_t uid;
    size_t size;

    msg_att = clist_content(cur);

    uid = 0;
    size = 0;
    for(item_cur = clist_begin(msg_att->att_list) ; item_cur != NULL ;
	item_cur = clist_next(item_cur)) {
      struct mailimap_msg_att_item * item;

      item = clist_content(item_cur);

      switch (item->att_type) {
      case MAILIMAP_MSG_ATT_ITEM_STATIC:
	switch (item->att_data.att_static->att_type) {
	case MAILIMAP_MSG_ATT_UID:
	  uid = item->att_data.att_static->att_data.att_uid;
	  break;
	  
	case MAILIMAP_MSG_ATT_RFC822_SIZE:
	  size = item->att_data.att_static->att_data.att_rfc822_size;
	  break;
	}
	break;
      }
    }

    msg = mailmessage_new();
    if (msg == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_list;
    }

    r = mailmessage_init(msg, session, driver, uid, size);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto free_msg;
    }

    r = carray_add(tab, msg, NULL);
    if (r < 0) {
      res = MAIL_ERROR_MEMORY;
      goto free_msg;
    }
  }

  env_list = mailmessage_list_new(tab);
  if (env_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_list;
  }

  * result = env_list;

  return MAIL_NO_ERROR;
  
 free_msg:
  mailmessage_free(msg);
 free_list:
  for(i = 0 ; i < carray_count(tab) ; i++)
    mailmessage_free(carray_get(tab, i));
 err:
  return res;
}


/*
  MAILIMAP_FLAG_FETCH_RECENT,
  MAILIMAP_FLAG_FETCH_OTHER

  MAILIMAP_FLAG_ANSWERED,
  MAILIMAP_FLAG_FLAGGED,
  MAILIMAP_FLAG_DELETED,
  MAILIMAP_FLAG_SEEN,
  MAILIMAP_FLAG_DRAFT,
  MAILIMAP_FLAG_KEYWORD,
  MAILIMAP_FLAG_EXTENSION
*/

static int imap_flags_to_flags(struct mailimap_msg_att_dynamic * att_dyn,
			       struct mail_flags ** result)
{
  struct mail_flags * flags;
  clist * flag_list;
  clistiter * cur;

  flags = mail_flags_new_empty();
  if (flags == NULL)
    goto err;
  flags->fl_flags = 0;

  flag_list = att_dyn->att_list;
  if (flag_list != NULL) {
    for(cur = clist_begin(flag_list) ; cur != NULL ;
        cur = clist_next(cur)) {
      struct mailimap_flag_fetch * flag_fetch;

      flag_fetch = clist_content(cur);
      if (flag_fetch->fl_type == MAILIMAP_FLAG_FETCH_RECENT)
	flags->fl_flags |= MAIL_FLAG_NEW;
      else {
	char * keyword;
	int r;

	switch (flag_fetch->fl_flag->fl_type) {
	case MAILIMAP_FLAG_ANSWERED:
	  flags->fl_flags |= MAIL_FLAG_ANSWERED;
	  break;
	case MAILIMAP_FLAG_FLAGGED:
	  flags->fl_flags |= MAIL_FLAG_FLAGGED;
	  break;
	case MAILIMAP_FLAG_DELETED:
	  flags->fl_flags |= MAIL_FLAG_DELETED;
	  break;
	case MAILIMAP_FLAG_SEEN:
	  flags->fl_flags |= MAIL_FLAG_SEEN;
	  break;
	case MAILIMAP_FLAG_DRAFT:
	  keyword = strdup("Draft");
	  if (keyword == NULL)
	    goto free;
	  r = clist_append(flags->fl_extension, keyword);
	  if (r < 0) {
	    free(keyword);
	    goto free;
	  }
	  break;
	case MAILIMAP_FLAG_KEYWORD:
          if (strcasecmp(flag_fetch->fl_flag->fl_data.fl_keyword,
                  "$Forwarded") == 0) {
            flags->fl_flags |= MAIL_FLAG_FORWARDED;
          }
          else {
            keyword = strdup(flag_fetch->fl_flag->fl_data.fl_keyword);
            if (keyword == NULL)
              goto free;
            r = clist_append(flags->fl_extension, keyword);
            if (r < 0) {
              free(keyword);
              goto free;
            }
          }
	  break;
	case MAILIMAP_FLAG_EXTENSION:
	  /* do nothing */
	  break;
	}
      }
    }
    /*
      MAIL_FLAG_NEW was set for \Recent messages.
      Correct this flag for \Seen messages by unsetting it.
    */
    if ((flags->fl_flags & MAIL_FLAG_SEEN) && (flags->fl_flags & MAIL_FLAG_NEW)) {
      flags->fl_flags &= ~MAIL_FLAG_NEW;
    }
  }

  * result = flags;
  
  return MAIL_NO_ERROR;

 free:
  mail_flags_free(flags);
 err:
  return MAIL_ERROR_MEMORY;
}

int imap_flags_to_imap_flags(struct mail_flags * flags,
    struct mailimap_flag_list ** result)
{
  struct mailimap_flag * flag;
  struct mailimap_flag_list * flag_list;
  int res;
  clistiter * cur;
  int r;

  flag_list = mailimap_flag_list_new_empty();
  if (flag_list == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  if ((flags->fl_flags & MAIL_FLAG_DELETED) != 0) {
    flag = mailimap_flag_new_deleted();
    if (flag == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
    r = mailimap_flag_list_add(flag_list, flag);
    if (r != MAILIMAP_NO_ERROR) {
      mailimap_flag_free(flag);
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
  }

  if ((flags->fl_flags & MAIL_FLAG_FLAGGED) != 0) {
    flag = mailimap_flag_new_flagged();
    if (flag == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
    r = mailimap_flag_list_add(flag_list, flag);
    if (r != MAILIMAP_NO_ERROR) {
      mailimap_flag_free(flag);
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
  }

  if ((flags->fl_flags & MAIL_FLAG_SEEN) != 0) {
    flag = mailimap_flag_new_seen();
    if (flag == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
    r = mailimap_flag_list_add(flag_list, flag);
    if (r != MAILIMAP_NO_ERROR) {
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
  }

  if ((flags->fl_flags & MAIL_FLAG_ANSWERED) != 0) {
    flag = mailimap_flag_new_answered();
    if (flag == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
    r = mailimap_flag_list_add(flag_list, flag);
    if (r != MAILIMAP_NO_ERROR) {
      mailimap_flag_free(flag);
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
  }

  if ((flags->fl_flags & MAIL_FLAG_FORWARDED) != 0) {
    char * flag_str;
    
    flag_str = strdup("$Forwarded");
    if (flag_str == NULL) {
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
    flag = mailimap_flag_new_flag_keyword(flag_str);
    if (flag == NULL) {
      free(flag_str);
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
    r = mailimap_flag_list_add(flag_list, flag);
    if (r != MAILIMAP_NO_ERROR) {
      mailimap_flag_free(flag);
      res = MAIL_ERROR_MEMORY;
      goto free_flag_list;
    }
  }

  for(cur = clist_begin(flags->fl_extension) ; cur != NULL ;
      cur = clist_next(cur)) {
    char * flag_str;

    flag_str = clist_content(cur);

    if (strcasecmp(flag_str, "Draft") == 0) {
      flag = mailimap_flag_new_draft();
      if (flag == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_flag_list;
      }
      r = mailimap_flag_list_add(flag_list, flag);
      if (r != MAILIMAP_NO_ERROR) {
	mailimap_flag_free(flag);
	res = MAIL_ERROR_MEMORY;
	goto free_flag_list;
      }
    }
    else {
      flag_str = strdup(flag_str);
      if (flag_str == NULL) {
	res = MAIL_ERROR_MEMORY;
	goto free_flag_list;
      }
      flag = mailimap_flag_new_flag_keyword(flag_str);
      if (flag == NULL) {
	free(flag_str);
	res = MAIL_ERROR_MEMORY;
	goto free_flag_list;
      }
      r = mailimap_flag_list_add(flag_list, flag);
      if (r != MAILIMAP_NO_ERROR) {
	mailimap_flag_free(flag);
	res = MAIL_ERROR_MEMORY;
	goto free_flag_list;
      }
    }
  }
  
  * result = flag_list;
  
  return MAIL_NO_ERROR;
  
 free_flag_list: 
  mailimap_flag_list_free(flag_list);
 err:
  return res;
}

static int flags_to_imap_flags(struct mail_flags * flags,
			       struct mailimap_store_att_flags ** result)
{
  struct mailimap_flag_list * flag_list;
  struct mailimap_store_att_flags * att_flags;
  int res;
  int r;
  
  r = imap_flags_to_imap_flags(flags,
      &flag_list);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  att_flags = mailimap_store_att_flags_new_set_flags_silent(flag_list);
  if (att_flags == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_flag_list;
  }
  
  * result = att_flags;
  
  return MAIL_NO_ERROR;
  
 free_flag_list: 
  mailimap_flag_list_free(flag_list);
 err:
  return res;
}


static int
imap_fetch_result_to_flags(clist * fetch_result, uint32_t index,
			   struct mail_flags ** result)
{
  clistiter * cur;
  int r;

  for(cur = clist_begin(fetch_result) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimap_msg_att * msg_att;
    clistiter * item_cur;
    uint32_t uid;
    struct mailimap_msg_att_dynamic * att_dyn;

    msg_att = clist_content(cur);

    uid = 0;
    att_dyn = NULL;

    for(item_cur = clist_begin(msg_att->att_list) ; item_cur != NULL ;
	item_cur = clist_next(item_cur)) {
      struct mailimap_msg_att_item * item;

      item = clist_content(item_cur);
      
      if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC) {
	switch (item->att_data.att_static->att_type) {
	case MAILIMAP_MSG_ATT_UID:
	  uid = item->att_data.att_static->att_data.att_uid;
	  break;
	}
      }
      else if (item->att_type == MAILIMAP_MSG_ATT_ITEM_DYNAMIC) {
	if (att_dyn == NULL) {
	  att_dyn = item->att_data.att_dyn;
	}
      }
    }

    if (uid != 0) {
      if (uid == index) {
	struct mail_flags * flags;
	
	if (att_dyn != NULL) {
	  r = imap_flags_to_flags(att_dyn, &flags);

	  if (r == MAIL_NO_ERROR) {
	    * result = flags;
	    return MAIL_NO_ERROR;
	  }
	}
      }
    }
  }

  return MAIL_ERROR_MSG_NOT_FOUND;
}


int imap_fetch_flags(mailimap * imap,
		     uint32_t index, struct mail_flags ** result)
{
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  struct mailimap_set * set;
  int r;
  int res;
  clist * fetch_result;
  struct mail_flags * flags;

  fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  fetch_att = mailimap_fetch_att_new_uid();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  fetch_att = mailimap_fetch_att_new_flags();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  set = mailimap_set_new_single(index);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_uid_fetch(imap, set, fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  switch (r) {
  case MAILIMAP_NO_ERROR:
    break;
  default:
    return imap_error_to_mail_error(r);
  }
  
  flags = NULL;
  r = imap_fetch_result_to_flags(fetch_result, index, &flags);
  mailimap_fetch_list_free(fetch_result);

  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }
  
  * result = flags;

  return MAIL_NO_ERROR;

 free_fetch_type:
  mailimap_fetch_type_free(fetch_type);
 err:
  return res;
}

int imap_store_flags(mailimap * imap, uint32_t first, uint32_t last,
		     struct mail_flags * flags)
{
  struct mailimap_store_att_flags * att_flags;
  struct mailimap_set * set;
  int r;
  int res;

  set = mailimap_set_new_interval(first, last);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  r = flags_to_imap_flags(flags, &att_flags);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto free_set;
  }

  r = mailimap_uid_store(imap, set, att_flags);
  if (r != MAILIMAP_NO_ERROR) {
    res = imap_error_to_mail_error(r);
    goto free_flag;
  }

  mailimap_store_att_flags_free(att_flags);
  mailimap_set_free(set);

  return MAIL_NO_ERROR;

 free_flag:
  mailimap_store_att_flags_free(att_flags);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}




int imap_get_messages_list(mailimap * imap,
    mailsession * session, mailmessage_driver * driver,
    uint32_t first_index,
    struct mailmessage_list ** result)
{
  struct mailmessage_list * env_list;
  int r;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  struct mailimap_set * set;
  clist * fetch_result;
  int res;

  set = mailimap_set_new_interval(first_index, 0);
  if (set == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto err;
  }

  fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  if (fetch_type == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_set;
  }

  fetch_att = mailimap_fetch_att_new_uid();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  fetch_att = mailimap_fetch_att_new_rfc822_size();
  if (fetch_att == NULL) {
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att);
  if (r != MAILIMAP_NO_ERROR) {
    mailimap_fetch_att_free(fetch_att);
    res = MAIL_ERROR_MEMORY;
    goto free_fetch_type;
  }

  r = mailimap_uid_fetch(imap, set,
			 fetch_type, &fetch_result);

  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);

  if (r != MAILIMAP_NO_ERROR) {
    res = imap_error_to_mail_error(r);
    goto err;
  }

  env_list = NULL;
  r = uid_list_to_env_list(fetch_result, &env_list, session, driver);
  mailimap_fetch_list_free(fetch_result);

  * result = env_list;

  return MAIL_NO_ERROR;

 free_fetch_type:
  mailimap_fetch_type_free(fetch_type);
 free_set:
  mailimap_set_free(set);
 err:
  return res;
}

static void generate_key_from_message(char * key, size_t size,
				      mailmessage * msg_info,
				      int type)
{
  switch (type) {
  case MAILIMAP_MSG_ATT_RFC822:
    snprintf(key, size, "%s-rfc822", msg_info->msg_uid);
    break;
  case MAILIMAP_MSG_ATT_RFC822_HEADER:
    snprintf(key, size, "%s-rfc822-header", msg_info->msg_uid);
    break;
  case MAILIMAP_MSG_ATT_RFC822_TEXT:
    snprintf(key, size, "%s-rfc822-text", msg_info->msg_uid);
    break;
  case MAILIMAP_MSG_ATT_ENVELOPE:
    snprintf(key, size, "%s-envelope", msg_info->msg_uid);
    break;
  }
}

int
imapdriver_get_cached_envelope(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session, mailmessage * msg,
    struct mailimf_fields ** result)
{
#if 0
  mailsession * imap_session;
#endif
  mailimap * imap;
  int r;
  struct mailimf_fields * fields;
  int res;
  char keyname[PATH_MAX];
  
#if 0
  imap_session = cached_session_get_ancestor(session);
  imap = ((struct imap_session_state_data *) (imap_session->data))->session;
#endif
  imap = cached_session_get_imap_session(session);

  generate_key_from_message(keyname, PATH_MAX,
			    msg, MAILIMAP_MSG_ATT_ENVELOPE);

  r = generic_cache_fields_read(cache_db, mmapstr, keyname, &fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  * result = fields;

  return MAIL_NO_ERROR;

err:
  return res;
}

int
imapdriver_write_cached_envelope(struct mail_cache_db * cache_db,
    MMAPString * mmapstr,
    mailsession * session, mailmessage * msg,
    struct mailimf_fields * fields)
{
  char keyname[PATH_MAX];
  int r;
  int res;

  generate_key_from_message(keyname, PATH_MAX,
			    msg, MAILIMAP_MSG_ATT_ENVELOPE);

  r = generic_cache_fields_write(cache_db, mmapstr, keyname, fields);
  if (r != MAIL_NO_ERROR) {
    res = r;
    goto err;
  }

  return MAIL_NO_ERROR;

err:
  return res;
}


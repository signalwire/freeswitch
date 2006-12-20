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
 * $Id: mailmime.c,v 1.27 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime.h"

/*
  RFC 2045
  RFC 2046
  RFC 2047
  RFC 2048
  RFC 2049
  RFC 2231
  RFC 2387
  RFC 2424
  RFC 2557

  RFC 2183 Content-Disposition

  RFC 1766  Language
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "mailmime_types.h"
#include "mailmime_disposition.h"
#include "mailimf.h"

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static int mailmime_attribute_parse(const char * message, size_t length,
				    size_t * index,
				    char ** result);
static int
mailmime_composite_type_parse(const char * message, size_t length,
			      size_t * index,
			      struct mailmime_composite_type ** result);

static int is_text(char ch);

static int
mailmime_discrete_type_parse(const char * message, size_t length,
			     size_t * index,
			     struct mailmime_discrete_type ** result);

static int mailmime_mechanism_parse(const char * message, size_t length,
				    size_t * index,
				    struct mailmime_mechanism ** result);

static int mailmime_subtype_parse(const char * message, size_t length,
				  size_t * index, char ** result);

static int is_token(char ch);

static int mailmime_token_parse(const char * message, size_t length,
				size_t * index,
				char ** token);

static int is_tspecials(char ch);

static int mailmime_type_parse(const char * message, size_t length,
			       size_t * index,
			       struct mailmime_type ** result);

/*
int mailmime_version_parse(const char * message, guint32 length,
				guint32 * index,
			        guint32 * result);
*/

/*
static gboolean mailmime_x_token_parse(gconst char * message, guint32 length,
				       guint32 * index,
				       gchar ** result);
*/

/* ********************************************************************** */

/*
x  attribute := token
               ; Matching of attributes
               ; is ALWAYS case-insensitive.
*/

static int mailmime_attribute_parse(const char * message, size_t length,
				    size_t * index,
				    char ** result)
{
  return mailmime_token_parse(message, length, index, result);
}

/*
x  composite-type := "message" / "multipart" / extension-token
*/

static int
mailmime_composite_type_parse(const char * message, size_t length,
			      size_t * index,
			      struct mailmime_composite_type ** result)
{
  char * extension_token;
  int type;
  struct mailmime_composite_type * ct;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  extension_token = NULL;

  type = MAILMIME_COMPOSITE_TYPE_ERROR; /* XXX - removes a gcc warning */

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "message");
  if (r == MAILIMF_NO_ERROR)
    type = MAILMIME_COMPOSITE_TYPE_MESSAGE;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "multipart");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_COMPOSITE_TYPE_MULTIPART;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  ct = mailmime_composite_type_new(type, extension_token);
  if (ct == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_extension;
  }

  * result = ct;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_extension:
  if (extension_token != NULL)
    mailmime_extension_token_free(extension_token);
 err:
  return res;
}

/*
x  content := "Content-Type" ":" type "/" subtype
             *(";" parameter)
             ; Matching of media type and subtype
             ; is ALWAYS case-insensitive.
*/

LIBETPAN_EXPORT
int mailmime_content_parse(const char * message, size_t length,
			   size_t * index,
			   struct mailmime_content ** result)
{
  size_t cur_token;
  struct mailmime_type * type;
  char * subtype;
  clist * parameters_list;
  struct mailmime_content * content;
  int r;
  int res;

  cur_token = * index;

  mailimf_cfws_parse(message, length, &cur_token);

  type = NULL;
  r = mailmime_type_parse(message, length, &cur_token, &type);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '/');
  switch (r) {
  case MAILIMF_NO_ERROR:
    r = mailimf_cfws_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto free_type;
    }
    
    r = mailmime_subtype_parse(message, length, &cur_token, &subtype);
    if (r != MAILIMF_NO_ERROR) {
      res = r;
      goto free_type;
    }
    break;

  case MAILIMF_ERROR_PARSE:
    subtype = strdup("unknown");
    break;

  default:
    res = r;
    goto free_type;
  }

  parameters_list = clist_new();
  if (parameters_list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_type;
  }

  while (1) {
    size_t final_token;
    struct mailmime_parameter * parameter;

    final_token = cur_token;
    r = mailimf_unstrict_char_parse(message, length, &cur_token, ';');
    if (r != MAILIMF_NO_ERROR) {
      cur_token = final_token;
      break;
    }

    r = mailimf_cfws_parse(message, length, &cur_token);
    if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
      res = r;
      goto free_type;
    }

    r = mailmime_parameter_parse(message, length, &cur_token, &parameter);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      cur_token = final_token;
      break;
    }
    else {
      res = r;
      goto err;
    }

    r = clist_append(parameters_list, parameter);
    if (r < 0) {
      mailmime_parameter_free(parameter);
      res = MAILIMF_ERROR_MEMORY;
      goto free_parameters;
    }
  }

  content = mailmime_content_new(type, subtype, parameters_list);
  if (content == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_parameters;
  }
 
  * result = content;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
 
 free_parameters:
  clist_foreach(parameters_list, (clist_func) mailmime_parameter_free, NULL);
  clist_free(parameters_list);
  
  mailmime_subtype_free(subtype);
 free_type:
  mailmime_type_free(type);
 err:
  return res;
}

/*
x  description := "Content-Description" ":" *text
*/

static int is_text(char ch)
{
  unsigned char uch = (unsigned char) ch;

  if (uch < 1)
    return FALSE;

  if ((uch == 10) || (uch == 13))
    return FALSE;

  return TRUE;
}

LIBETPAN_EXPORT
int mailmime_description_parse(const char * message, size_t length,
			       size_t * index,
			       char ** result)
{
  return mailimf_custom_string_parse(message, length,
				     index, result,
				     is_text);
}

/*
x  discrete-type := "text" / "image" / "audio" / "video" /
                   "application" / extension-token
*/

/* currently porting */

static int
mailmime_discrete_type_parse(const char * message, size_t length,
			     size_t * index,
			     struct mailmime_discrete_type ** result)
{
  char * extension;
  int type;
  struct mailmime_discrete_type * discrete_type;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  extension = NULL;

  type = MAILMIME_DISCRETE_TYPE_ERROR; /* XXX - removes a gcc warning */

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "text");
  if (r == MAILIMF_NO_ERROR)
    type = MAILMIME_DISCRETE_TYPE_TEXT;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "image");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISCRETE_TYPE_IMAGE;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "audio");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISCRETE_TYPE_AUDIO;
  }
  
  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "video");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISCRETE_TYPE_VIDEO;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "application");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISCRETE_TYPE_APPLICATION;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailmime_extension_token_parse(message, length,
				       &cur_token, &extension);
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISCRETE_TYPE_EXTENSION;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  discrete_type = mailmime_discrete_type_new(type, extension);
  if (discrete_type == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = discrete_type;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  mailmime_extension_token_free(extension);
 err:
  return res;
}

/*
x  encoding := "Content-Transfer-Encoding" ":" mechanism
*/

LIBETPAN_EXPORT
int mailmime_encoding_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailmime_mechanism ** result)
{
  return mailmime_mechanism_parse(message, length, index, result);
}

/*
x  entity-headers := [ content CRLF ]
                    [ encoding CRLF ]
                    [ id CRLF ]
                    [ description CRLF ]
                    *( MIME-extension-field CRLF )
		    */

enum {
  FIELD_STATE_START,
  FIELD_STATE_T,
  FIELD_STATE_D
};

static int guess_field_type(char * name)
{
  int state;

  if (* name == 'M')
    return MAILMIME_FIELD_VERSION;

  if (strncasecmp(name, "Content-", 8) != 0)
    return MAILMIME_FIELD_NONE;

  name += 8;

  state = FIELD_STATE_START;

  while (1) {

    switch (state) {
    
    case FIELD_STATE_START:
      switch ((char) toupper((unsigned char) * name)) {
      case 'T':
	state = FIELD_STATE_T;
	break;
      case 'I':
	return MAILMIME_FIELD_ID;
      case 'D':
	state = FIELD_STATE_D;
	break;
      case 'L':
	return MAILMIME_FIELD_LANGUAGE;
      default:
	return MAILMIME_FIELD_NONE;
      }
      break;

    case FIELD_STATE_T:
      switch ((char) toupper((unsigned char) * name)) {
      case 'Y':
	return MAILMIME_FIELD_TYPE;
      case 'R':
	return MAILMIME_FIELD_TRANSFER_ENCODING;
      default:
	return MAILMIME_FIELD_NONE;
      }
      break;

    case FIELD_STATE_D:
      switch ((char) toupper((unsigned char) * name)) {
      case 'E':
	return MAILMIME_FIELD_DESCRIPTION;
      case 'I':
	return MAILMIME_FIELD_DISPOSITION;
      default:
	return MAILMIME_FIELD_NONE;
      }
      break;
    }
    name ++;
  }
}

LIBETPAN_EXPORT
int
mailmime_field_parse(struct mailimf_optional_field * field,
		     struct mailmime_field ** result)
{
  char * name;
  char * value;
  int guessed_type;
  size_t cur_token;
  struct mailmime_content * content;
  struct mailmime_mechanism * encoding;
  char * id;
  char * description;
  uint32_t version;
  struct mailmime_field * mime_field;
  struct mailmime_language * language;
  struct mailmime_disposition * disposition;
  int res;
  int r;

  name = field->fld_name;
  value = field->fld_value;
  cur_token = 0;

  content = NULL;
  encoding = NULL;
  id = NULL;
  description = NULL;
  version = 0;
  disposition = NULL;
  language = NULL;

  guessed_type = guess_field_type(name);

  switch (guessed_type) {
  case MAILMIME_FIELD_TYPE:
    if (strcasecmp(name, "Content-Type") != 0)
      return MAILIMF_ERROR_PARSE;
    r = mailmime_content_parse(value, strlen(value), &cur_token, &content);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_FIELD_TRANSFER_ENCODING:
    if (strcasecmp(name, "Content-Transfer-Encoding") != 0)
      return MAILIMF_ERROR_PARSE;
    r = mailmime_encoding_parse(value, strlen(value), &cur_token, &encoding);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_FIELD_ID:
    if (strcasecmp(name, "Content-ID") != 0)
      return MAILIMF_ERROR_PARSE;
    r = mailmime_id_parse(value, strlen(value), &cur_token, &id);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_FIELD_DESCRIPTION:
    if (strcasecmp(name, "Content-Description") != 0)
      return MAILIMF_ERROR_PARSE;
    r = mailmime_description_parse(value, strlen(value),
				   &cur_token, &description);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_FIELD_VERSION:
    if (strcasecmp(name, "MIME-Version") != 0)
      return MAILIMF_ERROR_PARSE;
    r = mailmime_version_parse(value, strlen(value), &cur_token, &version);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_FIELD_DISPOSITION:
    if (strcasecmp(name, "Content-Disposition") != 0)
      return MAILIMF_ERROR_PARSE;
    r = mailmime_disposition_parse(value, strlen(value),
				   &cur_token, &disposition);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  case MAILMIME_FIELD_LANGUAGE:
    if (strcasecmp(name, "Content-Language") != 0)
      return MAILIMF_ERROR_PARSE;
    r = mailmime_language_parse(value, strlen(value), &cur_token, &language);
    if (r != MAILIMF_NO_ERROR)
      return r;
    break;

  default:
    return MAILIMF_ERROR_PARSE;
  }

  mime_field = mailmime_field_new(guessed_type, content, encoding,
				  id, description, version, disposition,
				  language);
  if (mime_field == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }
  
  * result = mime_field;

  return MAILIMF_NO_ERROR;

 free:
  if (content != NULL)
    mailmime_content_free(content);
  if (encoding != NULL)
    mailmime_encoding_free(encoding);
  if (id != NULL)
    mailmime_id_free(id);
  if (description != NULL)
    mailmime_description_free(description);
  return res;
}

/*
x  extension-token := ietf-token / x-token
*/

LIBETPAN_EXPORT
int
mailmime_extension_token_parse(const char * message, size_t length,
			       size_t * index, char ** result)
{
  return mailmime_token_parse(message, length, index, result);
}

/*
  hex-octet := "=" 2(DIGIT / "A" / "B" / "C" / "D" / "E" / "F")
               ; Octet must be used for characters > 127, =,
               ; SPACEs or TABs at the ends of lines, and is
               ; recommended for any character not listed in
               ; RFC 2049 as "mail-safe".
*/

/*
x  iana-token := <A publicly-defined extension token. Tokens
                 of this form must be registered with IANA
                 as specified in RFC 2048.>
*/

/*
x  ietf-token := <An extension token defined by a
                 standards-track RFC and registered
                 with IANA.>
*/

/*
x  id := "Content-ID" ":" msg-id
*/

LIBETPAN_EXPORT
int mailmime_id_parse(const char * message, size_t length,
		      size_t * index, char ** result)
{
  return mailimf_msg_id_parse(message, length, index, result);
}

/*
x  mechanism := "7bit" / "8bit" / "binary" /
               "quoted-printable" / "base64" /
               ietf-token / x-token
*/

static int mailmime_mechanism_parse(const char * message, size_t length,
				    size_t * index,
				    struct mailmime_mechanism ** result)
{
  char * token;
  int type;
  struct mailmime_mechanism * mechanism;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;
  
  type = MAILMIME_MECHANISM_ERROR; /* XXX - removes a gcc warning */

  token = NULL;
  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "7bit");
  if (r == MAILIMF_NO_ERROR)
    type = MAILMIME_MECHANISM_7BIT;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "8bit");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_MECHANISM_8BIT;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "binary");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_MECHANISM_BINARY;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "quoted-printable");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_MECHANISM_QUOTED_PRINTABLE;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "base64");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_MECHANISM_BASE64;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailmime_token_parse(message, length, &cur_token, &token);
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_MECHANISM_TOKEN;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  mechanism = mailmime_mechanism_new(type, token);
  if (mechanism == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = mechanism;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (token != NULL)
    mailmime_token_free(token);
 err:
  return res;
}

/*
x  MIME-extension-field := <Any RFC 822 header field which
                           begins with the string
                           "Content-">
*/

/*
in headers

x  MIME-message-headers := entity-headers
                          fields
                          version CRLF
                          ; The ordering of the header
                          ; fields implied by this BNF
                          ; definition should be ignored.
*/

/*
in message

x  MIME-part-headers := entity-headers
                       [fields]
                       ; Any field not beginning with
                       ; "content-" can have no defined
                       ; meaning and may be ignored.
                       ; The ordering of the header
                       ; fields implied by this BNF
                       ; definition should be ignored.
*/

#if 0
LIBETPAN_EXPORT
int
mailmime_unparsed_fields_parse(struct mailimf_unparsed_fields *
			       fields,
			       struct mailmime_fields **
			       result)
{
  clistiter * cur;
  struct mailmime_fields * mime_fields;
  clist * list;
  int r;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  if (fields->list == NULL) {
    res = MAILIMF_ERROR_PARSE;
    goto err;
  }

  for(cur = clist_begin(fields->list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_optional_field * field = cur->data;
    struct mailmime_field * mime_field;

    r = mailmime_field_parse(field, &mime_field);
    if (r == MAILIMF_NO_ERROR) {
      r = clist_append(list, mime_field);
      if (r < 0) {
	mailmime_field_free(mime_field);
	res = MAILIMF_ERROR_MEMORY;
	goto free_list;
      }
    }
  }

  if (clist_begin(list) == NULL) {
    res = MAILIMF_ERROR_PARSE;
    goto free_list;
  }

  mime_fields = mailmime_fields_new(list);
  if (mime_fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = mime_fields;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailmime_field_free, NULL);
  clist_free(list);
 err:
  return res;
}
#endif

LIBETPAN_EXPORT
int
mailmime_fields_parse(struct mailimf_fields *
		      fields,
		      struct mailmime_fields **
		      result)
{
  clistiter * cur;
  struct mailmime_fields * mime_fields;
  clist * list;
  int r;
  int res;

  list = clist_new();
  if (list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  for(cur = clist_begin(fields->fld_list) ; cur != NULL ;
      cur = clist_next(cur)) {
    struct mailimf_field * field;
    struct mailmime_field * mime_field;
    
    field = clist_content(cur);
    
    if (field->fld_type == MAILIMF_FIELD_OPTIONAL_FIELD) {
      r = mailmime_field_parse(field->fld_data.fld_optional_field,
          &mime_field);
      if (r == MAILIMF_NO_ERROR) {
	r = clist_append(list, mime_field);
	if (r < 0) {
	  mailmime_field_free(mime_field);
	  res = MAILIMF_ERROR_MEMORY;
	  goto free_list;
	}
      }
      else if (r == MAILIMF_ERROR_PARSE) {
	/* do nothing */
      }
      else {
	res = r;
	goto free_list;
      }
    }
  }

  if (clist_begin(list) == NULL) {
    res = MAILIMF_ERROR_PARSE;
    goto free_list;
  }

  mime_fields = mailmime_fields_new(list);
  if (mime_fields == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = mime_fields;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailmime_field_free, NULL);
  clist_free(list);
 err:
  return res;
}

/*
x  parameter := attribute "=" value
*/

LIBETPAN_EXPORT
int mailmime_parameter_parse(const char * message, size_t length,
			     size_t * index,
			     struct mailmime_parameter ** result)
{
  char * attribute;
  char * value;
  struct mailmime_parameter * parameter;
  size_t cur_token;
  int r;
  int res;

  cur_token = * index;

  r = mailmime_attribute_parse(message, length, &cur_token, &attribute);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_attr;
  }

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto free_attr;
  }

  r = mailmime_value_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto free_attr;
  }

  parameter = mailmime_parameter_new(attribute, value);
  if (parameter == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_value;
  }

  * result = parameter;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_value:
  mailmime_value_free(value);
 free_attr:
  mailmime_attribute_free(attribute);
 err:
  return res;
}

/*
  ptext := hex-octet / safe-char
*/

/*
  qp-line := *(qp-segment transport-padding CRLF)
             qp-part transport-padding
*/

/*
  qp-part := qp-section
             ; Maximum length of 76 characters
*/

/*
  qp-section := [*(ptext / SPACE / TAB) ptext]
*/

/*
  qp-segment := qp-section *(SPACE / TAB) "="
                ; Maximum length of 76 characters
*/

/*
  quoted-printable := qp-line *(CRLF qp-line)
*/

/*
  safe-char := <any octet with decimal value of 33 through
               60 inclusive, and 62 through 126>
               ; Characters not listed as "mail-safe" in
               ; RFC 2049 are also not recommended.
*/

/*
x  subtype := extension-token / iana-token
*/

static int mailmime_subtype_parse(const char * message, size_t length,
				  size_t * index, char ** result)
{
  return mailmime_extension_token_parse(message, length, index, result);
}

/*
x  token := 1*<any (US-ASCII) CHAR except SPACE, CTLs,
              or tspecials>
*/

static int is_token(char ch)
{
  unsigned char uch = (unsigned char) ch;

  if (uch > 0x7F)
    return FALSE;

  if (uch == ' ')
    return FALSE;

  if (is_tspecials(ch))
    return FALSE;

  return TRUE;
}


static int mailmime_token_parse(const char * message, size_t length,
				size_t * index,
				char ** token)
{
  return mailimf_custom_string_parse(message, length,
				     index, token,
				     is_token);
}

/*
  transport-padding := *LWSP-char
                       ; Composers MUST NOT generate
                       ; non-zero length transport
                       ; padding, but receivers MUST
                       ; be able to handle padding
                       ; added by message transports.
*/

/*
enum {
  LWSP_1,
  LWSP_2,
  LWSP_3,
  LWSP_4,
  LWSP_OK
};

gboolean mailmime_transport_padding_parse(gconst char * message, guint32 length,
					  guint32 * index)
{
  guint32 cur_token;
  gint state;
  guint32 last_valid_pos;

  cur_token = * index;
  
  if (cur_token >= length)
    return FALSE;

  state = LWSP_1;
  
  while (state != LWSP_OUT) {

    if (cur_token >= length)
      return FALSE;

    switch (state) {
    case LWSP_1:
      last_valid_pos = cur_token;

      switch (message[cur_token]) {
      case '\r':
	state = LWSP_2;
	break;
      case '\n':
	state = LWSP_3;
	break;
      case ' ':
      case '\t':
	state = LWSP_4;
	break;
      default:
	state = LWSP_OK;
	break;
      }
    case LWSP_2:
      switch (message[cur_token]) {
      case '\n':
	state = LWSP_3;
	break;
      default:
	state = LWSP_OUT;
	cur_token = last_valid_pos;
	break;
      }
    case LWSP_3:
      switch (message[cur_token]) {
      case ' ':
      case '\t':
	state = LWSP_1;
	break;
      default:
	state = LWSP_OUT;
	cur_token = last_valid_pos;
	break;
      }

      cur_token ++;
    }
  }

  * index = cur_token;

  return TRUE;
}
*/

/*
x  tspecials :=  "(" / ")" / "<" / ">" / "@" /
                "," / ";" / ":" / "\" / <">
                "/" / "[" / "]" / "?" / "="
                ; Must be in quoted-string,
                ; to use within parameter values
*/

static int is_tspecials(char ch)
{
  switch (ch) {
  case '(':
  case ')':
  case '<':
  case '>':
  case '@':
  case ',':
  case ';':
  case ':':
  case '\\':
  case '\"':
  case '/':
  case '[':
  case ']':
  case '?':
  case '=':
    return TRUE;
  default:
    return FALSE;
  }
}

/*
x  type := discrete-type / composite-type
*/

static int mailmime_type_parse(const char * message, size_t length,
			       size_t * index,
			       struct mailmime_type ** result)
{
  struct mailmime_discrete_type * discrete_type;
  struct mailmime_composite_type * composite_type;
  size_t cur_token;
  struct mailmime_type * mime_type;
  int type;
  int res;
  int r;

  cur_token = * index;

  discrete_type = NULL;
  composite_type = NULL;

  type = MAILMIME_TYPE_ERROR;  /* XXX - removes a gcc warning */

  r = mailmime_composite_type_parse(message, length, &cur_token,
				    &composite_type);
  if (r == MAILIMF_NO_ERROR)
    type = MAILMIME_TYPE_COMPOSITE_TYPE;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailmime_discrete_type_parse(message, length, &cur_token,
				     &discrete_type);
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_TYPE_DISCRETE_TYPE;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  mime_type = mailmime_type_new(type, discrete_type, composite_type);
  if (mime_type == NULL) {
    res = r;
    goto free;
  }

  * result = mime_type;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (discrete_type != NULL)
    mailmime_discrete_type_free(discrete_type);
  if (composite_type != NULL)
    mailmime_composite_type_free(composite_type);
 err:
  return res;
}

/*
x  value := token / quoted-string
*/

LIBETPAN_EXPORT
int mailmime_value_parse(const char * message, size_t length,
			 size_t * index, char ** result)
{
  int r;

  r = mailmime_token_parse(message, length, index, result);

  if (r == MAILIMF_ERROR_PARSE)
    r = mailimf_quoted_string_parse(message, length, index, result);

  if (r != MAILIMF_NO_ERROR)
    return r;

  return MAILIMF_NO_ERROR;
}

/*
x  version := "MIME-Version" ":" 1*DIGIT "." 1*DIGIT
*/

LIBETPAN_EXPORT
int mailmime_version_parse(const char * message, size_t length,
			   size_t * index,
			   uint32_t * result)
{
  size_t cur_token;
  uint32_t hi;
  uint32_t low;
  uint32_t version;
  int r;

  cur_token = * index;

  r = mailimf_number_parse(message, length, &cur_token, &hi);
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '.');
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE))
    return r;

  r = mailimf_number_parse(message, length, &cur_token, &low);
  if (r != MAILIMF_NO_ERROR)
    return r;

  version = (hi << 16) + low;

  * result = version;
  * index = cur_token;

  return MAILIMF_NO_ERROR;
}

/*
x  x-token := <The two characters "X-" or "x-" followed, with
              no  intervening white space, by any token>
*/

/*
static gboolean mailmime_x_token_parse(gconst char * message, guint32 length,
				       guint32 * index,
				       gchar ** result)
{
  guint32 cur_token;
  gchar * token;
  gchar * x_token;
  gboolean min_x;

  cur_token = * index;

  if (!mailimf_char_parse(message, length, &cur_token, 'x')) {
    if (!mailimf_char_parse(message, length, &cur_token, 'X'))
      return FALSE;
    min_x = FALSE;
  }
  else
    min_x = TRUE;

  if (!mailimf_char_parse(message, length, &cur_token, '-'))
    return FALSE;

  if (!mailmime_token_parse(message, length, &cur_token, &token))
    return FALSE;

  if (min_x)
    x_token = g_strconcat("x-", token, NULL);
  else
    x_token = g_strconcat("X-", token, NULL);
  mailmime_token_free(token);

  if (x_token == NULL)
    return FALSE;

  * result = x_token;
  * index = cur_token;

  return TRUE;
}
*/


LIBETPAN_EXPORT
int mailmime_language_parse(const char * message, size_t length,
			    size_t * index,
			    struct mailmime_language ** result)
{
  size_t cur_token;
  int r;
  int res;
  clist * list;
  int first;
  struct mailmime_language * language;

  cur_token = * index;

  list = clist_new();
  if (list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto err;
  }

  first = TRUE;

  while (1) {
    char * atom;

    r = mailimf_unstrict_char_parse(message, length, &cur_token, ',');
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      break;
    }
    else {
      res = r;
      goto err;
    }

    r = mailimf_atom_parse(message, length, &cur_token, &atom);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      break;
    }
    else {
      res = r;
      goto err;
    }

    r = clist_append(list, atom);
    if (r < 0) {
      mailimf_atom_free(atom);
      res = MAILIMF_ERROR_MEMORY;
      goto free;
    }
  }

  language = mailmime_language_new(list);
  if (language == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = language;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  clist_foreach(list, (clist_func) mailimf_atom_free, NULL);
  clist_free(list);
 err:
  return res;
}

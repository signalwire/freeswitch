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
 * $Id: mailmime_disposition.c,v 1.13 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmime_disposition.h"
#include "mailmime.h"

#include <ctype.h>
#include <stdlib.h>

static int
mailmime_disposition_parm_parse(const char * message, size_t length,
				size_t * index,
				struct mailmime_disposition_parm **
				result);

static int
mailmime_creation_date_parm_parse(const char * message, size_t length,
				  size_t * index, char ** result);

static int
mailmime_filename_parm_parse(const char * message, size_t length,
			     size_t * index, char ** result);

static int
mailmime_modification_date_parm_parse(const char * message, size_t length,
				      size_t * index, char ** result);

static int
mailmime_read_date_parm_parse(const char * message, size_t length,
			      size_t * index, char ** result);

static int
mailmime_size_parm_parse(const char * message, size_t length,
			 size_t * index, size_t * result);

static int
mailmime_quoted_date_time_parse(const char * message, size_t length,
				size_t * index, char ** result);

/*
     disposition := "Content-Disposition" ":"
                    disposition-type
                    *(";" disposition-parm)

*/


int mailmime_disposition_parse(const char * message, size_t length,
			       size_t * index,
			       struct mailmime_disposition ** result)
{
  size_t final_token;
  size_t cur_token;
  struct mailmime_disposition_type * dsp_type;
  clist * list;
  struct mailmime_disposition * dsp;
  int r;
  int res;

  cur_token = * index;

  r = mailmime_disposition_type_parse(message, length, &cur_token,
				      &dsp_type);
  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  list = clist_new();
  if (list == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_type;
  }

  while (1) {
    struct mailmime_disposition_parm * param;

    final_token = cur_token;
    r = mailimf_unstrict_char_parse(message, length, &cur_token, ';');
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      break;
    }
    else {
      res = r;
      goto free_list;
    }

    param = NULL;
    r = mailmime_disposition_parm_parse(message, length, &cur_token, &param);
    if (r == MAILIMF_NO_ERROR) {
      /* do nothing */
    }
    else if (r == MAILIMF_ERROR_PARSE) {
      cur_token = final_token;
      break;
    }
    else {
      res = r;
      goto free_list;
    }

    r = clist_append(list, param);
    if (r < 0) {
      res = MAILIMF_ERROR_MEMORY;
      goto free_list;
    }
  }

  dsp = mailmime_disposition_new(dsp_type, list);
  if (dsp == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free_list;
  }

  * result = dsp;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free_list:
  clist_foreach(list, (clist_func) mailmime_disposition_parm_free, NULL);
  clist_free(list);
 free_type:
  mailmime_disposition_type_free(dsp_type);
 err:
  return res;
}

/*		    
     disposition-type := "inline"
                       / "attachment"
                       / extension-token
                       ; values are not case-sensitive

*/



int
mailmime_disposition_type_parse(const char * message, size_t length,
				size_t * index,
				struct mailmime_disposition_type ** result)
{
  size_t cur_token;
  int type;
  char * extension;
  struct mailmime_disposition_type * dsp_type;
  int r;
  int res;

  cur_token = * index;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  type = MAILMIME_DISPOSITION_TYPE_ERROR; /* XXX - removes a gcc warning */

  extension = NULL;
  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "inline");
  if (r == MAILIMF_NO_ERROR)
    type = MAILMIME_DISPOSITION_TYPE_INLINE;

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailimf_token_case_insensitive_parse(message, length,
					     &cur_token, "attachment");
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISPOSITION_TYPE_ATTACHMENT;
  }

  if (r == MAILIMF_ERROR_PARSE) {
    r = mailmime_extension_token_parse(message, length, &cur_token,
				       &extension);
    if (r == MAILIMF_NO_ERROR)
      type = MAILMIME_DISPOSITION_TYPE_EXTENSION;
  }

  if (r != MAILIMF_NO_ERROR) {
    res = r;
    goto err;
  }

  dsp_type = mailmime_disposition_type_new(type, extension);
  if (dsp_type == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = dsp_type;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (extension != NULL)
    free(extension);
 err:
  return res;
}

/*
     disposition-parm := filename-parm
                       / creation-date-parm
                       / modification-date-parm
                       / read-date-parm
                       / size-parm
                       / parameter
*/


int mailmime_disposition_guess_type(const char * message, size_t length,
				    size_t index)
{
  if (index >= length)
    return MAILMIME_DISPOSITION_PARM_PARAMETER;

  switch ((char) toupper((unsigned char) message[index])) {
  case 'F':
    return MAILMIME_DISPOSITION_PARM_FILENAME;
  case 'C':
    return MAILMIME_DISPOSITION_PARM_CREATION_DATE;
  case 'M':
    return MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE;
  case 'R':
    return MAILMIME_DISPOSITION_PARM_READ_DATE;
  case 'S':
    return MAILMIME_DISPOSITION_PARM_SIZE;
  default:
    return MAILMIME_DISPOSITION_PARM_PARAMETER;
  }
}

static int
mailmime_disposition_parm_parse(const char * message, size_t length,
				size_t * index,
				struct mailmime_disposition_parm **
				result)
{
  char * filename;
  char * creation_date;
  char * modification_date;
  char * read_date;
  size_t size;
  struct mailmime_parameter * parameter;
  size_t cur_token;
  struct mailmime_disposition_parm * dsp_parm;
  int type;
  int guessed_type;
  int r;
  int res;

  cur_token = * index;

  filename = NULL;
  creation_date = NULL;
  modification_date = NULL;
  read_date = NULL;
  size = 0;
  parameter = NULL;

  r = mailimf_cfws_parse(message, length, &cur_token);
  if ((r != MAILIMF_NO_ERROR) && (r != MAILIMF_ERROR_PARSE)) {
    res = r;
    goto err;
  }

  guessed_type = mailmime_disposition_guess_type(message, length, cur_token);

  type = MAILMIME_DISPOSITION_PARM_PARAMETER;

  switch (guessed_type) {
  case MAILMIME_DISPOSITION_PARM_FILENAME:
    r = mailmime_filename_parm_parse(message, length, &cur_token,
				     &filename);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;

  case MAILMIME_DISPOSITION_PARM_CREATION_DATE:
    r = mailmime_creation_date_parm_parse(message, length, &cur_token,
					  &creation_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;

  case MAILMIME_DISPOSITION_PARM_MODIFICATION_DATE:
    r = mailmime_modification_date_parm_parse(message, length, &cur_token,
					      &modification_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
    
  case MAILMIME_DISPOSITION_PARM_READ_DATE:
    r = mailmime_read_date_parm_parse(message, length, &cur_token,
				      &read_date);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;

  case MAILMIME_DISPOSITION_PARM_SIZE:
    r = mailmime_size_parm_parse(message, length, &cur_token,
				 &size);
    if (r == MAILIMF_NO_ERROR)
      type = guessed_type;
    else if (r == MAILIMF_ERROR_PARSE) {
      /* do nothing */
    }
    else {
      res = r;
      goto err;
    }
    break;
  }

  if (type == MAILMIME_DISPOSITION_PARM_PARAMETER) {
    r = mailmime_parameter_parse(message, length, &cur_token,
				 &parameter);
    if (r != MAILIMF_NO_ERROR) {
      type = guessed_type;
      res = r;
      goto err;
    }
  }

  dsp_parm = mailmime_disposition_parm_new(type, filename, creation_date,
					   modification_date, read_date,
					   size, parameter);

  if (dsp_parm == NULL) {
    res = MAILIMF_ERROR_MEMORY;
    goto free;
  }

  * result = dsp_parm;
  * index = cur_token;

  return MAILIMF_NO_ERROR;

 free:
  if (filename != NULL)
    mailmime_filename_parm_free(dsp_parm->pa_data.pa_filename);
  if (creation_date != NULL)
    mailmime_creation_date_parm_free(dsp_parm->pa_data.pa_creation_date);
  if (modification_date != NULL)
    mailmime_modification_date_parm_free(dsp_parm->pa_data.pa_modification_date);
  if (read_date != NULL)
    mailmime_read_date_parm_free(dsp_parm->pa_data.pa_read_date);
  if (parameter != NULL)
    mailmime_parameter_free(dsp_parm->pa_data.pa_parameter);
 err:
  return res;
}

/*
     filename-parm := "filename" "=" value
*/

static int
mailmime_filename_parm_parse(const char * message, size_t length,
			     size_t * index, char ** result)
{
  char * value;
  int r;
  size_t cur_token;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "filename");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailmime_value_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     creation-date-parm := "creation-date" "=" quoted-date-time
*/

static int
mailmime_creation_date_parm_parse(const char * message, size_t length,
				  size_t * index, char ** result)
{
  char * value;
  int r;
  size_t cur_token;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "creation-date");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailmime_quoted_date_time_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     modification-date-parm := "modification-date" "=" quoted-date-time
*/

static int
mailmime_modification_date_parm_parse(const char * message, size_t length,
				      size_t * index, char ** result)
{
  char * value;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "modification-date");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailmime_quoted_date_time_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     read-date-parm := "read-date" "=" quoted-date-time
*/

static int
mailmime_read_date_parm_parse(const char * message, size_t length,
			      size_t * index, char ** result)
{
  char * value;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "read-date");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailmime_quoted_date_time_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     size-parm := "size" "=" 1*DIGIT
*/

static int
mailmime_size_parm_parse(const char * message, size_t length,
			 size_t * index, size_t * result)
{
  uint32_t value;
  size_t cur_token;
  int r;

  cur_token = * index;

  r = mailimf_token_case_insensitive_parse(message, length,
					   &cur_token, "size");
  if (r != MAILIMF_NO_ERROR)
    return r;

  r = mailimf_unstrict_char_parse(message, length, &cur_token, '=');
  if (r != MAILIMF_NO_ERROR)
    return r;
  
  r = mailimf_number_parse(message, length, &cur_token, &value);
  if (r != MAILIMF_NO_ERROR)
    return r;

  * index = cur_token;
  * result = value;

  return MAILIMF_NO_ERROR;
}

/*
     quoted-date-time := quoted-string
                      ; contents MUST be an RFC 822 `date-time'
                      ; numeric timezones (+HHMM or -HHMM) MUST be used
*/

static int
mailmime_quoted_date_time_parse(const char * message, size_t length,
				size_t * index, char ** result)
{
  return mailimf_quoted_string_parse(message, length, index, result);
}

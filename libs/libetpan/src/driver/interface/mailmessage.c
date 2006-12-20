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
 * $Id: mailmessage.c,v 1.17 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmessage.h"

#include "mail.h"

#include <string.h>

LIBETPAN_EXPORT
int mailmessage_init(mailmessage * msg_info,
    mailsession * msg_session,
    mailmessage_driver * msg_driver,
    uint32_t msg_index, size_t msg_size)
{
  int r;
  int res;

  msg_info->msg_driver = msg_driver;
  msg_info->msg_session = msg_session;
  msg_info->msg_index = msg_index;
  msg_info->msg_uid = NULL;

  msg_info->msg_cached = FALSE;
  msg_info->msg_size = msg_size;
  msg_info->msg_fields = NULL;
  memset(&msg_info->msg_single_fields, 0,
      sizeof(struct mailimf_single_fields));
  msg_info->msg_resolved = FALSE;
  msg_info->msg_flags = NULL;

  msg_info->msg_mime = NULL;
  msg_info->msg_data = NULL;
  msg_info->msg_folder = NULL;
  msg_info->msg_user_data = NULL;

  if (msg_driver->msg_initialize != NULL) {
    r = msg_driver->msg_initialize(msg_info);
    if (r != MAIL_NO_ERROR) {
      res = r;
      goto err;
    }
  }

  return MAIL_NO_ERROR;

 err:
  msg_info->msg_driver = NULL;
  msg_info->msg_session = NULL;
  return res;
}

LIBETPAN_EXPORT
int mailmessage_flush(mailmessage * msg_info)
{
  if (msg_info->msg_driver->msg_flush == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  msg_info->msg_driver->msg_flush(msg_info);

  return MAIL_NO_ERROR;
}

LIBETPAN_EXPORT
int mailmessage_check(mailmessage * msg_info)
{
  if (msg_info->msg_driver->msg_check == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  msg_info->msg_driver->msg_check(msg_info);

  return MAIL_NO_ERROR;
}
LIBETPAN_EXPORT
int mailmessage_fetch_result_free(mailmessage * msg_info,
				  char * msg)
{
  if (msg_info->msg_driver->msg_fetch_result_free == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  msg_info->msg_driver->msg_fetch_result_free(msg_info, msg);

  return MAIL_NO_ERROR;
}

LIBETPAN_EXPORT
int mailmessage_fetch(mailmessage * msg_info,
		      char ** result,
		      size_t * result_len)
{
  if (msg_info->msg_driver->msg_fetch == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch(msg_info, result, result_len);
}

LIBETPAN_EXPORT
int mailmessage_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len)
{
  if (msg_info->msg_driver->msg_fetch_header == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch_header(msg_info, result, result_len);
}

LIBETPAN_EXPORT
int mailmessage_fetch_body(mailmessage * msg_info,
			   char ** result, size_t * result_len)
{
  if (msg_info->msg_driver->msg_fetch_body == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch_body(msg_info, result, result_len);
}

LIBETPAN_EXPORT
int mailmessage_fetch_size(mailmessage * msg_info,
			   size_t * result)
{
  if (msg_info->msg_driver->msg_fetch_size == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch_size(msg_info, result);
}

LIBETPAN_EXPORT
int mailmessage_get_bodystructure(mailmessage * msg_info,
				  struct mailmime ** result)
{
  if (msg_info->msg_driver->msg_get_bodystructure == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_get_bodystructure(msg_info, result);
}

LIBETPAN_EXPORT
int mailmessage_fetch_section(mailmessage * msg_info,
			      struct mailmime * mime,
			      char ** result, size_t * result_len)
{
  if (msg_info->msg_driver->msg_fetch_section == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch_section(msg_info, mime, result, result_len);
}

LIBETPAN_EXPORT
int mailmessage_fetch_section_header(mailmessage * msg_info,
				     struct mailmime * mime,
				     char ** result,
				     size_t * result_len)
{
  if (msg_info->msg_driver->msg_fetch_section_header == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch_section_header(msg_info, mime,
						result, result_len);
}

LIBETPAN_EXPORT
int mailmessage_fetch_section_mime(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len)
{
  if (msg_info->msg_driver->msg_fetch_section_mime == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;
  
  return msg_info->msg_driver->msg_fetch_section_mime(msg_info, mime,
					      result, result_len);
}

LIBETPAN_EXPORT
int mailmessage_fetch_section_body(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len)
{
  if (msg_info->msg_driver->msg_fetch_section_body == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch_section_body(msg_info, mime,
					      result, result_len);
}

LIBETPAN_EXPORT
int mailmessage_fetch_envelope(mailmessage * msg_info,
			       struct mailimf_fields ** result)
{
  if (msg_info->msg_driver->msg_fetch_envelope == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;

  return msg_info->msg_driver->msg_fetch_envelope(msg_info, result);
}

LIBETPAN_EXPORT
int mailmessage_get_flags(mailmessage * msg_info,
			  struct mail_flags ** result)
{
  struct mail_flags * dummy;

  if (msg_info->msg_driver->msg_get_flags == NULL)
    return MAIL_ERROR_NOT_IMPLEMENTED;
  
  if (result != NULL)
    return msg_info->msg_driver->msg_get_flags(msg_info, result);
  else
    return msg_info->msg_driver->msg_get_flags(msg_info, &dummy);
}

LIBETPAN_EXPORT
void mailmessage_resolve_single_fields(mailmessage * msg_info)
{
  if (!msg_info->msg_resolved) {
    if (msg_info->msg_fields != NULL) {
      mailimf_single_fields_init(&msg_info->msg_single_fields,
          msg_info->msg_fields);
      msg_info->msg_resolved = TRUE;
    }
  }
}

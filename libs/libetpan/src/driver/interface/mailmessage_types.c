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
 * $Id: mailmessage_types.c,v 1.13 2006/05/22 13:39:40 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailmessage_types.h"

#include "mail.h"

#include <stdlib.h>
#include <string.h>

LIBETPAN_EXPORT
mailmessage * mailmessage_new(void)
{
  mailmessage * msg_info;

  msg_info = malloc(sizeof(* msg_info));
  if (msg_info == NULL)
    goto err;

  msg_info->msg_driver = NULL;
  msg_info->msg_session = NULL;
  msg_info->msg_index = 0;
  msg_info->msg_uid = NULL;

  msg_info->msg_cached = FALSE;
  msg_info->msg_size = 0;
  msg_info->msg_fields = NULL;
  memset(&msg_info->msg_single_fields,
      0, sizeof(struct mailimf_single_fields));
  msg_info->msg_resolved = FALSE;
  msg_info->msg_flags = NULL;

  msg_info->msg_mime = NULL;
  msg_info->msg_data = NULL;

  msg_info->msg_folder = NULL;
  msg_info->msg_user_data = NULL;

  return msg_info;

 err:
  return NULL;
}

LIBETPAN_EXPORT
void mailmessage_free(mailmessage * msg_info)
{
  if (msg_info->msg_driver != NULL) {
    if (msg_info->msg_driver->msg_uninitialize != NULL)
      msg_info->msg_driver->msg_uninitialize(msg_info);
  }

  if (msg_info->msg_fields != NULL)
    mailimf_fields_free(msg_info->msg_fields);
  if (msg_info->msg_mime != NULL)
    mailmime_free(msg_info->msg_mime);
  if (msg_info->msg_flags != NULL)
    mail_flags_free(msg_info->msg_flags);
  if (msg_info->msg_uid != NULL)
    free(msg_info->msg_uid);
  free(msg_info);
}

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
 * $Id: data_message_driver.c,v 1.10 2006/06/26 11:50:26 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "data_message_driver.h"

#include "mailmessage.h"
#include "mailmessage_tools.h"

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#	include <sys/mman.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

static int fetch_size(mailmessage * msg, size_t * result)
{
  struct generic_message_t * msg_data;

  msg_data = msg->msg_data;
  * result = msg_data->msg_length;

  return MAIL_NO_ERROR;
}


static mailmessage_driver local_data_message_driver = {
#if ! defined __STDC_VERSION__ || __STDC_VERSION__ < 199901L
  /* msg_name */ "data",
  
  /* msg_initialize */ mailmessage_generic_initialize,
  /* msg_uninitialize */ mailmessage_generic_uninitialize,
  
  /* msg_flush */ mailmessage_generic_flush,
  /* msg_check */ NULL,

  /* msg_fetch_result_free */ mailmessage_generic_fetch_result_free,
  
  /* msg_fetch */ mailmessage_generic_fetch,
  /* msg_fetch_header */ mailmessage_generic_fetch_header,
  /* msg_fetch_body */ mailmessage_generic_fetch_body,
  /* msg_fetch_size */ fetch_size,
  /* msg_get_bodystructure */ mailmessage_generic_get_bodystructure,
  /* msg_fetch_section */ mailmessage_generic_fetch_section,
  /* msg_fetch_section_header */ mailmessage_generic_fetch_section_header,
  /* msg_fetch_section_mime */ mailmessage_generic_fetch_section_mime,
  /* msg_fetch_section_body */ mailmessage_generic_fetch_section_body,
  /* msg_fetch_envelope */ mailmessage_generic_fetch_envelope,

  /* msg_get_flags */ NULL,
#else
  .msg_name = "data",
  
  .msg_initialize = mailmessage_generic_initialize,
  .msg_uninitialize = mailmessage_generic_uninitialize,
  
  .msg_flush = mailmessage_generic_flush,
  .msg_check = NULL,

  .msg_fetch_result_free = mailmessage_generic_fetch_result_free,
  
  .msg_fetch = mailmessage_generic_fetch,
  .msg_fetch_header = mailmessage_generic_fetch_header,
  .msg_fetch_body = mailmessage_generic_fetch_body,
  .msg_fetch_size = fetch_size,
  .msg_get_bodystructure = mailmessage_generic_get_bodystructure,
  .msg_fetch_section = mailmessage_generic_fetch_section,
  .msg_fetch_section_header = mailmessage_generic_fetch_section_header,
  .msg_fetch_section_mime = mailmessage_generic_fetch_section_mime,
  .msg_fetch_section_body = mailmessage_generic_fetch_section_body,
  .msg_fetch_envelope = mailmessage_generic_fetch_envelope,

  .msg_get_flags = NULL,
#endif
};

mailmessage_driver * data_message_driver = &local_data_message_driver;



mailmessage * data_message_init(char * data, size_t len)
{
  struct generic_message_t * msg_data;
  mailmessage * msg;
  int r;

  msg = mailmessage_new();
  if (msg == NULL)
    goto err;

  r = mailmessage_init(msg, NULL, data_message_driver, 0, len);
  if (r < 0)
    goto free;

  msg_data = msg->msg_data;
  msg_data->msg_fetched = 1;
  msg_data->msg_message = data;
  msg_data->msg_length = len;

  return msg;

 free:
  mailmessage_free(msg);
 err:
  return NULL;
}

void data_message_detach_mime(mailmessage * msg)
{
  msg->msg_mime = NULL;
}

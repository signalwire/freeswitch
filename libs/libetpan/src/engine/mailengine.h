/*
 * libEtPan! -- a mail library
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
 * $Id: mailengine.h,v 1.3 2004/11/21 21:53:35 hoa Exp $
 */

#ifndef MAILENGINE_H

#define MAILENGINE_H

#include <libetpan/mailmessage.h>
#include <libetpan/mailfolder.h>
#include <libetpan/mailprivacy_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  to run things in thread, you must protect the storage again concurrency.
*/


/*
  storage data
*/

struct mailengine *
libetpan_engine_new(struct mailprivacy * privacy);

void libetpan_engine_free(struct mailengine * engine);


struct mailprivacy *
libetpan_engine_get_privacy(struct mailengine * engine);


/*
  message ref and unref
*/

/*
  these function can only take messages returned by get_msg_list()
  as arguments.
  
  these functions cannot fail.
*/

int libetpan_message_ref(struct mailengine * engine,
    mailmessage * msg);

int libetpan_message_unref(struct mailengine * engine,
    mailmessage * msg);


/*
  when you want to access the MIME structure of the message
  with msg->mime, you have to call libetpan_message_mime_ref()
  and libetpan_message_mime_unref() when you have finished.
  
  if libetpan_mime_ref() returns a value <= 0, it means this failed.
  the value is -MAIL_ERROR_XXX
*/

int libetpan_message_mime_ref(struct mailengine * engine,
    mailmessage * msg);

int libetpan_message_mime_unref(struct mailengine * engine,
    mailmessage * msg);

/*
  message list
*/

/*
  libetpan_folder_get_msg_list()
  
  This function returns two list.
  - List of lost message (the messages that were previously returned
    but that does no more exist) (p_lost_msg_list)
  - List of valid messages (p_new_msg_list).

  These two list can only be freed by libetpan_folder_free_msg_list()
*/

int libetpan_folder_get_msg_list(struct mailengine * engine,
    struct mailfolder * folder,
    struct mailmessage_list ** p_new_msg_list,
    struct mailmessage_list ** p_lost_msg_list);

int libetpan_folder_fetch_env_list(struct mailengine * engine,
    struct mailfolder * folder,
    struct mailmessage_list * msg_list);

void libetpan_folder_free_msg_list(struct mailengine * engine,
    struct mailfolder * folder,
    struct mailmessage_list * env_list);


/*
  connect and disconnect storage
*/

int libetpan_storage_add(struct mailengine * engine,
    struct mailstorage * storage);

void libetpan_storage_remove(struct mailengine * engine,
    struct mailstorage * storage);

int libetpan_storage_connect(struct mailengine * engine,
    struct mailstorage * storage);

void libetpan_storage_disconnect(struct mailengine * engine,
    struct mailstorage * storage);

int libetpan_storage_used(struct mailengine * engine,
    struct mailstorage * storage);


/*
  libetpan_folder_connect()
  libetpan_folder_disconnect()
  
  You can disconnect the folder only when you have freed all the message
  you were given.
*/

int libetpan_folder_connect(struct mailengine * engine,
    struct mailfolder * folder);

void libetpan_folder_disconnect(struct mailengine * engine,
    struct mailfolder * folder);


struct mailfolder *
libetpan_message_get_folder(struct mailengine * engine,
    mailmessage * msg);

struct mailstorage *
libetpan_message_get_storage(struct mailengine * engine,
    mailmessage * msg);


/*
  register a message
*/

int libetpan_message_register(struct mailengine * engine,
    struct mailfolder * folder,
    mailmessage * msg);


void libetpan_engine_debug(struct mailengine * engine, FILE * f);

extern void * engine_app;

#ifdef __cplusplus
}
#endif

#endif

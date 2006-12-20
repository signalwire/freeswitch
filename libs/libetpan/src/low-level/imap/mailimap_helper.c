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
 * $Id: mailimap_helper.c,v 1.12 2006/06/26 11:50:27 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_helper.h"

#include <stdlib.h>
#include "mailimap.h"

int mailimap_fetch_rfc822(mailimap * session,
			  uint32_t msgid, char ** result)
{
  int r;
  clist * fetch_list;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  struct mailimap_set * set;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * item;
  int res;
  
  fetch_att = mailimap_fetch_att_new_rfc822();
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  set = mailimap_set_new_single(msgid);

  r = mailimap_fetch(session, set, fetch_type, &fetch_list);

  mailimap_set_free(set);
  mailimap_fetch_type_free(fetch_type);

  if (r != MAILIMAP_NO_ERROR) {
	res = r;
	goto err;
  }

  if (clist_isempty(fetch_list)) {
    res = MAILIMAP_ERROR_FETCH;
	goto free;
  }
  
  msg_att = (struct mailimap_msg_att *) clist_begin(fetch_list)->data;

  if (clist_isempty(msg_att->att_list)) {
    res = MAILIMAP_ERROR_FETCH;
	goto free;
  }
  
  item = (struct mailimap_msg_att_item *) clist_begin(msg_att->att_list)->data;

  if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) {
	res = MAILIMAP_ERROR_FETCH;
    goto free;
  }
  if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_RFC822) {
	res = MAILIMAP_ERROR_FETCH;
    goto free;
  }
  
  * result = item->att_data.att_static->att_data.att_rfc822.att_content;
  item->att_data.att_static->att_data.att_rfc822.att_content = NULL;
  mailimap_fetch_list_free(fetch_list);

  return MAILIMAP_NO_ERROR;

free:
  mailimap_fetch_list_free(fetch_list);
err:
  return res;
}

int mailimap_fetch_rfc822_header(mailimap * session,
				 uint32_t msgid, char ** result)
{
  int r;
  int res;
  clist * fetch_list;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  struct mailimap_set * set;
  struct mailimap_msg_att * msg_att;
  struct mailimap_msg_att_item * item;

  fetch_att = mailimap_fetch_att_new_rfc822_header();
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  set = mailimap_set_new_single(msgid);

  r = mailimap_fetch(session, set, fetch_type, &fetch_list);

  mailimap_set_free(set);
  mailimap_fetch_type_free(fetch_type);

  if (r != MAILIMAP_NO_ERROR) {
	res = r;
	goto err;
  }

  if (clist_isempty(fetch_list)) {
    res = MAILIMAP_ERROR_FETCH;
	goto free;
  }
  
  msg_att = (struct mailimap_msg_att *) clist_begin(fetch_list)->data;

  if (clist_isempty(msg_att->att_list)) {
    res = MAILIMAP_ERROR_FETCH;
	goto free;
  }
  
  item = (struct mailimap_msg_att_item *) clist_begin(msg_att->att_list)->data;

  if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC) {
	res = MAILIMAP_ERROR_FETCH;
    goto err;
  }
  
  if (item->att_data.att_static->att_type != MAILIMAP_MSG_ATT_RFC822_HEADER) {
	res = MAILIMAP_ERROR_FETCH;
    goto err;
  }

  * result = item->att_data.att_static->att_data.att_rfc822_header.att_content;
  item->att_data.att_static->att_data.att_rfc822_header.att_content = NULL;
  mailimap_fetch_list_free(fetch_list);

  return MAILIMAP_NO_ERROR;

free:
  mailimap_fetch_list_free(fetch_list);
err:
  return res;
}

int mailimap_fetch_envelope(mailimap * session,
    uint32_t first, uint32_t last,
    clist ** result)
{
  int r;
  clist * fetch_list;
  struct mailimap_fetch_att * fetch_att;
  struct mailimap_fetch_type * fetch_type;
  struct mailimap_set * set;

  fetch_att = mailimap_fetch_att_new_envelope();
  fetch_type = mailimap_fetch_type_new_fetch_att(fetch_att);
  set = mailimap_set_new_interval(first, last);

  r = mailimap_fetch(session, set, fetch_type, &fetch_list);

  mailimap_set_free(set);
  mailimap_fetch_type_free(fetch_type);

  if (r != MAILIMAP_NO_ERROR)
    return r;

  * result = fetch_list;

  return MAILIMAP_NO_ERROR;
}

int mailimap_append_simple(mailimap * session, const char * mailbox,
			   const char * content, uint32_t size)
{
  return mailimap_append(session, mailbox, NULL, NULL, content, size);
}

int mailimap_login_simple(mailimap * session,
			  const char * userid, const char * password)
{
  if (session->imap_state == MAILIMAP_STATE_NON_AUTHENTICATED)
    return mailimap_login(session, userid, password);
  else
    return MAILIMAP_NO_ERROR;
}


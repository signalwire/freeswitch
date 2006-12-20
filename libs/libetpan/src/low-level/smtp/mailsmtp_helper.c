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
 * $Id: mailsmtp_helper.c,v 1.14 2006/06/26 11:50:28 hoa Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailsmtp.h"
#include <string.h>
#include <stdlib.h>
#include "mail.h"

int mailsmtp_init(mailsmtp * session)
{
  int r;

  r = mailesmtp_ehlo(session);

  if (r == MAILSMTP_NO_ERROR)
    return MAILSMTP_NO_ERROR;

  r = mailsmtp_helo(session);
  if (r == MAILSMTP_NO_ERROR)
    return MAILSMTP_NO_ERROR;

  return r;
}



int mailesmtp_send(mailsmtp * session,
		    const char * from,
		    int return_full,
		    const char * envid,
		    clist * addresses,
		    const char * message, size_t size)
{
  int r;
  clistiter * l;

  if (!session->esmtp)
    return mailsmtp_send(session, from, addresses, message, size);

  r = mailesmtp_mail(session, from, return_full, envid);
  if (r != MAILSMTP_NO_ERROR)
    return r;

  for(l = clist_begin(addresses) ; l != NULL; l = clist_next(l)) {
    struct esmtp_address * addr;
    
    addr = clist_content(l);

    r = mailesmtp_rcpt(session, addr->address, addr->notify, addr->orcpt);
    if (r != MAILSMTP_NO_ERROR)
      return r;
  }

  r = mailsmtp_data(session);
  if (r != MAILSMTP_NO_ERROR)
    return r;

  r = mailsmtp_data_message(session, message, size);
  if (r != MAILSMTP_NO_ERROR)
    return r;

  return MAILSMTP_NO_ERROR;
}

int mailsmtp_send(mailsmtp * session,
		   const char * from,
		   clist * addresses,
		   const char * message, size_t size)
{
  int r;
  clistiter * l;

  r = mailsmtp_mail(session, from);
  if (r != MAILSMTP_NO_ERROR)
    return r;

  for(l = clist_begin(addresses) ; l != NULL; l = clist_next(l)) {
    struct esmtp_address * addr;
    
    addr = clist_content(l);
    
    r = mailsmtp_rcpt(session, addr->address);
    if (r != MAILSMTP_NO_ERROR)
      return r;
  }

  r = mailsmtp_data(session);
  if (r != MAILSMTP_NO_ERROR)
    return r;

  r = mailsmtp_data_message(session, message, size);
  if (r != MAILSMTP_NO_ERROR)
    return r;

  return MAILSMTP_NO_ERROR;
}













/* esmtp addresses and smtp addresses */

static struct esmtp_address * esmtp_address_new(char * addr,
						int notify, char * orcpt)
{
  struct esmtp_address * esmtpa;
  
  esmtpa = malloc(sizeof(* esmtpa));
  if (esmtpa == NULL)
    return NULL;
  
  esmtpa->address = strdup(addr);
  if (esmtpa->address == NULL) {
    free(esmtpa);
    return NULL;
  }

  if (orcpt != NULL) {
    esmtpa->orcpt = strdup(orcpt);
    if (esmtpa->orcpt == NULL) {
      free(esmtpa->address);
      free(esmtpa);
      return NULL;
    }
  }
  else
    esmtpa->orcpt = NULL;
  
  esmtpa->notify = notify;

  return esmtpa;
}

static void esmtp_address_free(struct esmtp_address * addr)
{
  if (addr->orcpt)
    free(addr->orcpt);
  if (addr->address)
    free(addr->address);

  free(addr);
}

clist * esmtp_address_list_new(void)
{
  return clist_new();
}

void esmtp_address_list_free(clist * l)
{
  clist_foreach(l, (clist_func) esmtp_address_free, NULL);
  clist_free(l);
}

int esmtp_address_list_add(clist * list, char * address,
			       int notify, char * orcpt)
{
  struct esmtp_address * esmtpa;
  int r;

  esmtpa = esmtp_address_new(address, notify, orcpt);
  if (esmtpa == NULL)
    return -1;

  r = clist_append(list, esmtpa);
  if (r < 0) {
    esmtp_address_free(esmtpa);
    return -1;
  }

  return 0;
}

clist * smtp_address_list_new(void)
{
  return esmtp_address_list_new();
}

int smtp_address_list_add(clist * list, char * address)
{
  return esmtp_address_list_add(list, address, 0, NULL);
}

void smtp_address_list_free(clist * l)
{
  esmtp_address_list_free(l);
}

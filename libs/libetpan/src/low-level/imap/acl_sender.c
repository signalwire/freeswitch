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

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailimap_sender.h"
#include "acl_types.h"

static int mailimap_acl_identifier_send(mailstream * fd,
        const char * identifier)
{
  return mailimap_astring_send(fd, identifier);
}

static int mailimap_acl_mod_rights_send(mailstream * fd,
        const char * mod_rights)
{
  return mailimap_astring_send(fd, mod_rights);
}

int mailimap_acl_setacl_send(mailstream * fd,
        const char * mailbox,
        const char * identifier,
        const char * mod_rights)
{
  int r;

  r = mailimap_token_send(fd, "SETACL");
  if (r != MAILIMAP_NO_ERROR)
	  return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_identifier_send(fd, identifier);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_mod_rights_send(fd, mod_rights);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_acl_deleteacl_send(mailstream * fd,
        const char * mailbox,
        const char * identifier)
{
  int r;

  r = mailimap_token_send(fd, "DELETEACL");
  if (r != MAILIMAP_NO_ERROR)
	  return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_identifier_send(fd, identifier);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_acl_getacl_send(mailstream * fd,
        const char * mailbox)
{
  int r;

  r = mailimap_token_send(fd, "GETACL");
  if (r != MAILIMAP_NO_ERROR)
	  return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_acl_listrights_send(mailstream * fd,
        const char * mailbox,
        const char * identifier)
{
  int r;

  r = mailimap_token_send(fd, "LISTRIGHTS");
  if (r != MAILIMAP_NO_ERROR)
	  return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_acl_identifier_send(fd, identifier);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

int mailimap_acl_myrights_send(mailstream * fd,
        const char * mailbox)
{
  int r;

  r = mailimap_token_send(fd, "MYRIGHTS");
  if (r != MAILIMAP_NO_ERROR)
	  return r;

  r = mailimap_space_send(fd);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_mailbox_send(fd, mailbox);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  return MAILIMAP_NO_ERROR;
}

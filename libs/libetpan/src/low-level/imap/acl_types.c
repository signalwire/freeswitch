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

#include "mailimap_types.h"
#include "acl_types.h"
#include "mailimap_extension.h"

#include <stdlib.h>
#include <string.h>

void mailimap_acl_identifier_free(char * identifier)
{
  mailimap_astring_free(identifier);
}

void mailimap_acl_rights_free(char * rights)
{
  mailimap_astring_free(rights);
}

struct mailimap_acl_identifier_rights *
mailimap_acl_identifier_rights_new(char * identifier, char * rights)
{
  struct mailimap_acl_identifier_rights * id_rights;

  id_rights = malloc(sizeof(* id_rights));
  if (id_rights == NULL)
    return NULL;

  id_rights->identifer = identifier;
  id_rights->rights = rights;

  return id_rights;
}

void mailimap_acl_identifier_rights_free(
        struct mailimap_acl_identifier_rights * id_rights)
{
  mailimap_acl_identifier_free(id_rights->identifer);
  mailimap_acl_rights_free(id_rights->rights);

  free(id_rights);
}

struct mailimap_acl_acl_data *
mailimap_acl_acl_data_new(char * mailbox, clist * idrights_list)
{
  struct mailimap_acl_acl_data * acl_data;

  acl_data = malloc(sizeof(* acl_data));
  if (acl_data == NULL)
    return NULL;

  acl_data->mailbox = mailbox;
  acl_data->idrights_list = idrights_list;

  return acl_data;
}

LIBETPAN_EXPORT
void mailimap_acl_acl_data_free(struct
        mailimap_acl_acl_data * acl_data)
{
  mailimap_mailbox_free(acl_data->mailbox);
  clist_foreach(acl_data->idrights_list,
    (clist_func) mailimap_acl_identifier_rights_free, NULL);
  clist_free(acl_data->idrights_list);
  free(acl_data);
}

struct mailimap_acl_listrights_data *
mailimap_acl_listrights_data_new(char * mailbox,
        char * identifier, clist * rights_list)
{
  struct mailimap_acl_listrights_data * lr_data;

  lr_data = malloc(sizeof(* lr_data));
  if (lr_data == NULL)
    return NULL;

  lr_data->mailbox = mailbox;
  lr_data->identifier = identifier;
  lr_data->rights_list = rights_list;

  return lr_data;
}

LIBETPAN_EXPORT
void mailimap_acl_listrights_data_free(struct
        mailimap_acl_listrights_data * lr_data)
{
  mailimap_mailbox_free(lr_data->mailbox);
  mailimap_acl_identifier_free(lr_data->identifier);
  clist_foreach(lr_data->rights_list,
    (clist_func) mailimap_acl_rights_free, NULL);
  clist_free(lr_data->rights_list);
  free(lr_data);
}

struct mailimap_acl_myrights_data *
mailimap_acl_myrights_data_new(char * mailbox, char * rights)
{
  struct mailimap_acl_myrights_data * mr_data;

  mr_data = malloc(sizeof(* mr_data));
  if (mr_data == NULL)
    return NULL;

  mr_data->mailbox = mailbox;
  mr_data->rights = rights;

  return mr_data;
}

LIBETPAN_EXPORT
void mailimap_acl_myrights_data_free(struct
        mailimap_acl_myrights_data * mr_data)
{
  mailimap_mailbox_free(mr_data->mailbox);
  mailimap_acl_rights_free(mr_data->rights);
  free(mr_data);
}

void
mailimap_acl_free(struct mailimap_extension_data * ext_data)
{
  if (ext_data == NULL)
    return;

  switch (ext_data->ext_type)
  {
    case MAILIMAP_ACL_TYPE_ACL_DATA:
      mailimap_acl_acl_data_free(ext_data->ext_data);
      break;
    case MAILIMAP_ACL_TYPE_LISTRIGHTS_DATA:
      mailimap_acl_listrights_data_free(ext_data->ext_data);
      break;
    case MAILIMAP_ACL_TYPE_MYRIGHTS_DATA:
      mailimap_acl_myrights_data_free(ext_data->ext_data);
      break;
  }

  free (ext_data);
}

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

#ifndef ACL_TYPES_H

#define ACL_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/libetpan-config.h>
#include <libetpan/mailstream.h>
#include <libetpan/clist.h>

/*
   ACL grammar
   see [rfc4314] for further information

   LOWER-ALPHA     =  %x61-7A   ;; a-z

   acl-data        = "ACL" SP mailbox *(SP identifier SP
                       rights)

   capability      =/ rights-capa
                       ;;capability is defined in [IMAP4]

   command-auth    =/ setacl / deleteacl / getacl /
                       listrights / myrights
                       ;;command-auth is defined in [IMAP4]

   deleteacl       = "DELETEACL" SP mailbox SP identifier

   getacl          = "GETACL" SP mailbox

   identifier      = astring

   listrights      = "LISTRIGHTS" SP mailbox SP identifier

   listrights-data = "LISTRIGHTS" SP mailbox SP identifier
                           SP rights *(SP rights)

   mailbox-data    =/ acl-data / listrights-data / myrights-data
                       ;;mailbox-data is defined in [IMAP4]

   mod-rights      = astring
                       ;; +rights to add, -rights to remove
                       ;; rights to replace

   myrights        = "MYRIGHTS" SP mailbox

   myrights-data   = "MYRIGHTS" SP mailbox SP rights

   new-rights      = 1*LOWER-ALPHA
                       ;; MUST include "t", "e", "x", and "k".
                       ;; MUST NOT include standard rights listed
                       ;; in section 2.2

   rights          = astring
                       ;; only lowercase ASCII letters and digits
                       ;; are allowed.

   rights-capa     = "RIGHTS=" new-rights
                       ;; RIGHTS=... capability

   setacl          = "SETACL" SP mailbox SP identifier
                       SP mod-rights
*/

/*
  only need to recognize types that can be "embedded" into main
  IMAPrev1 types.
*/
enum {
  MAILIMAP_ACL_TYPE_ACL_DATA,                   /* child of mailbox-data  */
  MAILIMAP_ACL_TYPE_LISTRIGHTS_DATA,            /* child of mailbox-data  */
  MAILIMAP_ACL_TYPE_MYRIGHTS_DATA,              /* child of mailbox-data  */
};

void mailimap_acl_identifier_free(char * identifier);

void mailimap_acl_rights_free(char * rights);

struct mailimap_acl_identifier_rights {
  char * identifer;
  char * rights;
};

struct mailimap_acl_identifier_rights *
mailimap_acl_identifier_rights_new(char * identifier, char * rights);

void mailimap_acl_identifier_rights_free(
        struct mailimap_acl_identifier_rights * id_rights);

struct mailimap_acl_acl_data {
  char * mailbox;
  clist * idrights_list;
  /* list of (struct mailimap_acl_identifier_rights *) */
};

struct mailimap_acl_acl_data *
mailimap_acl_acl_data_new(char * mailbox, clist * idrights_list);

LIBETPAN_EXPORT
void mailimap_acl_acl_data_free(struct
        mailimap_acl_acl_data * acl_data);

struct mailimap_acl_listrights_data {
  char * mailbox;
  char * identifier;
  clist * rights_list; /* list of (char *) */
};

struct mailimap_acl_listrights_data *
mailimap_acl_listrights_data_new(char * mailbox,
        char * identifier, clist * rights_list);

LIBETPAN_EXPORT
void mailimap_acl_listrights_data_free(struct
        mailimap_acl_listrights_data * listrights_data);

struct mailimap_acl_myrights_data {
  char * mailbox;
  char * rights;
};

struct mailimap_acl_myrights_data *
mailimap_acl_myrights_data_new(char * mailbox, char * rights);

LIBETPAN_EXPORT
void mailimap_acl_myrights_data_free(struct
        mailimap_acl_myrights_data * myrights_data);

void
mailimap_acl_free(struct mailimap_extension_data * ext_data);

#ifdef __cplusplus
}
#endif

#endif

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
  TODO: parse extension to capability as defined in [rfc4314].
  capability    =/ rights-capa

  This should actually be automatically parsed by parse_capability_data,
  so maybe it's sufficient to code a higher-level (not mailimap) function
  that returns a list of extra-rights as defined in rights-capa.
*/

#ifndef ACL_H

#define ACL_H

#include <libetpan/mailimap_extension.h>
#include <libetpan/acl_types.h>

#ifdef __cplusplus
extern "C" {
#endif

LIBETPAN_EXPORT
extern struct mailimap_extension_api mailimap_extension_acl;

/*
  mailimap_acl_setacl()

  This will set access for an identifier on the mailbox specified.

  @param session      the IMAP session
  @param mailbox      the mailbox to modify
  @param identifier   the identifier to set access-rights for
  @param mod_rights   the modification to make to the rights

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes

*/

LIBETPAN_EXPORT
int mailimap_acl_setacl(mailimap * session,
    const char * mailbox,
    const char * identifier,
    const char * mod_rights);

/*
  mailimap_acl_deleteacl()

  This will remove the acl on the mailbox for the identifier specified.

  @param session      the IMAP session
  @param mailbox      the mailbox to modify
  @param identifier   the identifier to remove acl for

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes

*/

LIBETPAN_EXPORT
int mailimap_acl_deleteacl(mailimap * session,
    const char * mailbox,
    const char * identifier);

/*
  mailimap_acl_getacl()

  This will get a list of acls for the mailbox

  @param session  the IMAP session
  @param mailbox  the mailbox to get the acls for
  @param result   this will store a clist of (struct mailimap_acl_acl_data *)
      in (* result)

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes

*/

LIBETPAN_EXPORT
int mailimap_acl_getacl(mailimap * session,
    const char * mailbox,
    clist ** result);

/*
  mailimap_acl_listrights()

  The LISTRIGHTS command takes a mailbox name and an identifier and
  returns information about what rights can be granted to the
  identifier in the ACL for the mailbox.

  @param session    the IMAP session
  @param mailbox    the mailbox to get the acls for
  @param identifier the identifier to query the acls for
  @param result     this will store a (struct mailimap_acl_listrights_data *)

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes

*/

LIBETPAN_EXPORT
int mailimap_acl_listrights(mailimap * session,
    const char * mailbox,
    const char * identifier,
    struct mailimap_acl_listrights_data ** result);

/*
  mailimap_acl_myrights()

  This will list the rights for the querying user on the mailbox

  @param session    the IMAP session
  @param mailbox    the mailbox to get the acls for
  @param result     this will store a (struct mailimap_acl_myrights_data *)

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes

*/

LIBETPAN_EXPORT
int mailimap_acl_myrights(mailimap * session,
    const char * mailbox,
    struct mailimap_acl_myrights_data ** result);

LIBETPAN_EXPORT
int mailimap_has_acl(mailimap * session);

#ifdef __cplusplus
}
#endif

#endif

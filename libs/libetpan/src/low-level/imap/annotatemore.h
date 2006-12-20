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

#ifndef ANNOTATEMORE_H

#define ANNOTATEMORE_H

#include <libetpan/mailimap_extension.h>
#include <libetpan/annotatemore_types.h>

#ifdef __cplusplus
extern "C" {
#endif

LIBETPAN_EXPORT
extern struct mailimap_extension_api mailimap_extension_annotatemore;

/*
  mailimap_annotatemore_getannotation()

  This function will get annotations from given mailboxes or the server.

  @param session the IMAP session
  @param list_mb mailbox name with possible wildcard,
                 empty string implies server annotation
  @param entries entry specifier with possible wildcards
  @param attribs attribute specifier with possible wildcards
  @param result  This will store a clist of (struct mailimap_annotate_data *)
      in (* result)

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes
  
*/

LIBETPAN_EXPORT
int mailimap_annotatemore_getannotation(mailimap * session,
    const char * list_mb,
    struct mailimap_annotatemore_entry_match_list * entries,
    struct mailimap_annotatemore_attrib_match_list * attribs,
    clist ** result);

/*
  mailimap_annotatemore_setannotation()

  This function will set annotations on given mailboxes or the server.

  @param session  the IMAP session
  @param list_mb  mailbox name with possible wildcard,
                  empty string implies server annotation
  @param en_att   a list of entries/attributes to set
  @param result   if return is MAILIMAP_ERROR_EXTENSION result
                  is MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_TOOBIG or
                  MAILIMAP_ANNOTATEMORE_RESP_TEXT_CODE_TOOMANY for
                  extra information about the error.

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_annotatemore_setannotation(mailimap * session,
    const char * list_mb,
    struct mailimap_annotatemore_entry_att_list * en_att,
    int * result);

LIBETPAN_EXPORT
int mailimap_has_annotatemore(mailimap * session);

#ifdef __cplusplus
}
#endif

#endif

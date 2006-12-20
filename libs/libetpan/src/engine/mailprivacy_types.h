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
 * $Id: mailprivacy_types.h,v 1.4 2005/11/21 16:17:57 hoa Exp $
 */

#ifndef MAIL_PRIVACY_TYPES_H

#define MAIL_PRIVACY_TYPES_H

#include <libetpan/chash.h>
#include <libetpan/carray.h>
#include <libetpan/mailmessage.h>
#include <libetpan/mailmime.h>

struct mailprivacy {
  char * tmp_dir;               /* working tmp directory */
  chash * msg_ref;              /* mailmessage => present or not */
  chash * mmapstr;              /* mmapstring => present or not present */
  chash * mime_ref;             /* mime => present or not */
  carray * protocols;
  int make_alternative;
  /* if make_alternative is 0, replaces the part with decrypted 
     part, if 1, adds a multipart/alternative and put the decrypted 
     and encrypted part as subparts.
  */
};

struct mailprivacy_encryption {
  char * name;
  char * description;
  
  int (* encrypt)(struct mailprivacy *,
      mailmessage *,
      struct mailmime *, struct mailmime **);
};

struct mailprivacy_protocol {
  char * name;
  char * description;
  
  /* introduced to easy the port to sylpheed */
  int (* is_encrypted)(struct mailprivacy *,
      mailmessage *, struct mailmime *);
  
  int (* decrypt)(struct mailprivacy *,
      mailmessage *, struct mailmime *,
      struct mailmime **);
  
  int encryption_count;
  struct mailprivacy_encryption * encryption_tab;
};

#endif

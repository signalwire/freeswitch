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
 * $Id: mailpop3_helper.c,v 1.10 2006/06/07 15:10:01 smarinier Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "mailpop3_helper.h"

#include <string.h>

int mailpop3_login_apop(mailpop3 * f,
			const char * user,
			const char * password)
{
  return mailpop3_apop(f, user, password);
}


/*
  mailpop3_login
  
  must be used immediately after connect 
*/

int mailpop3_login(mailpop3 * f,
		    const char * user,
		    const char * password)
{
  int r;

  if ((r = mailpop3_user(f, user)) != MAILPOP3_NO_ERROR)
    return r;

  if ((r = mailpop3_pass(f, password)) != MAILPOP3_NO_ERROR)
    return r;
  
  return MAILPOP3_NO_ERROR;
}

void mailpop3_header_free(char * str)
{
  mailpop3_top_free(str);
}

int mailpop3_header(mailpop3 * f, uint32_t index, char ** result,
		    size_t * result_len)
{
  return mailpop3_top(f, index, 0, result, result_len);
}

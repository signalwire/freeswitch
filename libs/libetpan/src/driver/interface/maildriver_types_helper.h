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
 * $Id: maildriver_types_helper.h,v 1.6 2004/11/21 21:53:35 hoa Exp $
 */

#ifndef MAILDRIVER_TYPES_HELPER_H

#define MAILDRIVER_TYPES_HELPER_H

#include <libetpan/maildriver_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  mail_flags_add_extension adds the given flag if it does not exists in
  the flags.

  @param flags this is the flag to change

  @param ext_flag this is the name of an extension flag
    the given flag name is duplicated and is no more needed after
    the function call.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

int mail_flags_add_extension(struct mail_flags * flags,
			     char * ext_flag);

/*
  mail_flags_remove_extension removes the given flag if it does not exists in
  the flags.

  @param flags this is the flag to change

  @param ext_flag this is the name of an extension flag
    the given flag name is no more needed after the function call.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

int mail_flags_remove_extension(struct mail_flags * flags,
				char * ext_flag);

/*
  mail_flags_has_extension returns 1 if the flags is in the given flags,
    0 is returned otherwise.

  @param flags this is the flag to change

  @param ext_flag this is the name of an extension flag
    the given flag name is no more needed after the function call.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

int mail_flags_has_extension(struct mail_flags * flags,
			     char * ext_flag);

#ifdef __cplusplus
}
#endif

#endif

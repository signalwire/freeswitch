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
 * $Id: mailmessage.h,v 1.16 2005/06/01 12:22:00 smarinier Exp $
 */

#include <libetpan/mailmessage_types.h>

#ifndef MAILMESSAGE_H

#define MAILMESSAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
  mailmessage_new

  This function will initializes a new empty message.
  
  @return a new empty message will be returned.
*/
LIBETPAN_EXPORT
mailmessage * mailmessage_new(void);

/*
  mailmessage_free

  This function will release the memory used by this message.
*/
LIBETPAN_EXPORT
void mailmessage_free(mailmessage * info);

/*
  mailmessage_init
  
  This function will initializes a mailmessage structure
  with a message from a given session.

  @param msg_info  This is the message to initialize.
  
  @param session This is the source session of the message. It
    can be NULL if the message does not get the information
    through the session.
  
  @param driver This is the driver to use for the message.

  @param index This is the message number in the session. 0 can
    be given if the message is not attached to a session.

  @param size is an optional parameter, 0 can be given.
    This is informational. This is the size of message content.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/
LIBETPAN_EXPORT
int mailmessage_init(mailmessage * msg_info,
		     mailsession * session,
		     mailmessage_driver * driver,
		     uint32_t index, size_t size);

/*
  mailmessage_flush

  This function will release all the temporary resources that are not
  necessary to use the mailmessage structure from memory. These
  resources are for example cached information, such as the MIME
  structure.

  @param info is the message to clean.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error. We can assume that MAIL_NO_ERROR is always returned.
*/
LIBETPAN_EXPORT
int mailmessage_flush(mailmessage * info);

/*
  mailmessage_check

  This function will notify the new value of the flags to the session,
  it must be called before mailsession_check_folder() in case the flags have
  been changed.

  @param info is the message to checkpoint.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error. We can assume that MAIL_NO_ERROR is always returned.
*/
LIBETPAN_EXPORT
int mailmessage_check(mailmessage * info);

/*
  mailmessage_fetch_result_free

  This function releases the memory used by a message returned
  by any of the fetch function that returns a (char *).

  @param msg_info is the message which the given buffer is from.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
  on error. We can assume that MAIL_NO_ERROR is always returned.
*/
LIBETPAN_EXPORT
int mailmessage_fetch_result_free(mailmessage * msg_info,
				  char * msg);

/*
  mailmessage_fetch

  This function returns the content of the message (headers and text).

  @param msg_info  is the message from which we want to fetch information.

  @param result     The content of the message is returned in (* result)

  @param result_len The length of the returned string is stored
    in (* result_len).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_fetch(mailmessage * msg_info,
		      char ** result,
		      size_t * result_len);

/*
  mailmessage_fetch_header

  This function returns the header of the message as a string.

  @param msg_info  is the message from which we want to fetch information.

  @param result     The header of the message is returned in (* result)

  @param result_len The length of the returned string is stored
    in (* result_len).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_fetch_header(mailmessage * msg_info,
			     char ** result,
			     size_t * result_len);

/*
  mailmessage_fetch_body

  This function returns the content of the message (without headers).

  @param msg_info  is the message from which we want to fetch information.
  @param result     The message text (without headers) is returned
    in (* result)
  @param result_len The length of the returned string is stored
    in (* result_len).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_fetch_body(mailmessage * msg_info,
			   char ** result, size_t * result_len);

/*
  mailmessage_fetch_size

  This function returns the size of the message content.

  @param msg_info  is the message from which we want to fetch information.

  @param result The length of the message content is stored in (* result).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_fetch_size(mailmessage * msg_info,
			   size_t * result);

/*
  mailmessage_get_bodystructure

  This functions returns the MIME structure of the message.
  The returned information MUST not be freed by hand. It is freed by
  mailmessage_flush() or mailmessage_free().

  @param msg_info  is the message from which we want to fetch information.

  @param result The MIME structure is stored in (* result).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_get_bodystructure(mailmessage * msg_info,
				  struct mailmime ** result);

/*
  mailmessage_fetch_section

  This function returns the content of a MIME part.

  @param msg_info  is the message from which we want to fetch information.
  
  @param mime is the MIME part identifier.

  @param result     The content is returned in (* result)

  @param result_len The length of the returned string is stored
    in (* result_len).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
 */
LIBETPAN_EXPORT
int mailmessage_fetch_section(mailmessage * msg_info,
			      struct mailmime * mime,
			      char ** result, size_t * result_len);

/*
  mailmessage_fetch_section_header

  This function returns the header of the message contained
  in the given MIME part.

  @param msg_info  is the message from which we want to fetch information.
  
  @param mime is the MIME part identifier.

  @param result     The header is returned in (* result)

  @param result_len The length of the returned string is stored
    in (* result_len).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_fetch_section_header(mailmessage * msg_info,
				     struct mailmime * mime,
				     char ** result,
				     size_t * result_len);

/*
  mailmessage_fetch_section_mime

  This function returns the MIME header of the given MIME part.

  @param msg_info  is the message from which we want to fetch information.
  
  @param mime is the MIME part identifier.

  @param result     The MIME header is returned in (* result)

  @param result_len The length of the returned string is stored
    in (* result_len).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_fetch_section_mime(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len);

/*
  mailmessage_fetch_section_body

  This function returns the text part of the message contained
  in the given MIME part.

  @param msg_info  is the message from which we want to fetch information.
  
  @param mime is the MIME part identifier.

  @param result     The message text is returned in (* result)

  @param result_len The length of the returned string is stored
    in (* result_len).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
 */
LIBETPAN_EXPORT
int mailmessage_fetch_section_body(mailmessage * msg_info,
				   struct mailmime * mime,
				   char ** result,
				   size_t * result_len);

/*
  mailmessage_fetch_envelope

  This function returns a list of parsed fields of the message,
  chosen by the driver.
  The returned structure must be freed with mailimf_fields_free().

  @param msg_info  is the message from which we want to fetch information.
  
  @param result     The headers list is returned in (* result)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
 */
LIBETPAN_EXPORT
int mailmessage_fetch_envelope(mailmessage * msg_info,
			       struct mailimf_fields ** result);


/*
  mailmessage_get_flags

  This function returns the flags related to the message.
  The returned information MUST not be freed by hand. It is freed by
  mailmessage_free().

  @param msg_info  is the message from which we want to fetch information.

  @param result The flags are stored in (* result).

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
int mailmessage_get_flags(mailmessage * msg_info,
			  struct mail_flags ** result);

/*
  mailmessage_resolve_single_fields

  This function will use the fields information to fill the single_fields
  structure in the mailmessage structure.

  @param msg_info This is the msg_info to process.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error.
*/
LIBETPAN_EXPORT
void mailmessage_resolve_single_fields(mailmessage * msg_info);

#ifdef __cplusplus
}
#endif

#endif

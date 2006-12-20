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
 * $Id: maildriver.h,v 1.34 2006/06/07 15:10:01 smarinier Exp $
 */

#ifndef MAILDRIVER_H

#define MAILDRIVER_H

#include <libetpan/maildriver_types.h>
#include <libetpan/maildriver_types_helper.h>

#ifdef __cplusplus
extern "C" {
#endif

/* mailsession */

/*
  mailsession_new creates a new session, using the given driver

  @return the created session is returned
*/

LIBETPAN_EXPORT
mailsession * mailsession_new(mailsession_driver * sess_driver);

/*
  mailsession_free release the memory used by the session
*/

LIBETPAN_EXPORT
void mailsession_free(mailsession * session);

/*
  mailsession_parameters is used to make calls specific to the driver

  @param id   is the command to send to the driver,
           usually, commands can be found in the header of the driver

  @param value is the parameter of the specific call 

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_parameters(mailsession * session,
			   int id, void * value);

/*
  There are drivers of two kinds : stream drivers (driver that connects
  to servers through TCP or other means of connection) and file drivers
  (driver that are based on filesystem)

  The following function can only be used by stream drivers.
  mailsession_connect_stream connects a stream to the session

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_connect_stream(mailsession * session, mailstream * s);

/*
  The following function can only be used by file drivers.
  mailsession_connect_path selects the main path of the session

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_connect_path(mailsession * session, const char * path);

/*
  NOTE: works only on stream drivers

  mailsession_starttls switches the current connection to TLS (secure layer)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_starttls(mailsession * session);

/*
  mailsession_login notifies the login and the password to authenticate
  to the session

  @param userid    the given string is only needed at this function call
    (it will be duplicated if necessary)
  @param password  the given string is only needed at this function call
    (it will be duplicated if necessary)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_login(mailsession * session,
		      const char * userid, const char * password);

/*
  NOTE: this function doesn't often work on filsystem drivers

  mailsession_logout deconnects the session and closes the stream.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_logout(mailsession * session);

/*
  mailsession_noop does no operation on the session, but it can be
  used to poll for the status of the connection.

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_noop(mailsession * session);

/*
  NOTE: driver's specific should be used

  mailsession_build_folder_name will return an allocated string with
  that contains the complete path of the folder to create

  @param session the sesion
  @param mb is the parent mailbox
  @param name is the name of the folder to create
  @param result the complete path of the folder to create will be
    stored in (* result), this name have to be freed with free()

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_build_folder_name(mailsession * session, const char * mb,
				  const char * name, char ** result);

/*
  NOTE: driver's specific should be used

  mailsession_create_folder creates the folder that corresponds to the
  given name

  @param session the session
  @param mb is the name of the mailbox

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_create_folder(mailsession * session, const char * mb);


/*
  NOTE: driver's specific should be used

  mailsession_delete_folder deletes the folder that corresponds to the
  given name

  @param session the session
  @param mb is the name of the mailbox

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_delete_folder(mailsession * session, const char * mb);


/*
  mailsession_rename_folder changes the name of the folder

  @param session the session
  @param mb is the name of the mailbox whose name has to be changed
  @param new_name is the destination name (the parent
    of the new folder folder can be other)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_rename_folder(mailsession * session,
			      const char * mb, const char * new_name);

/*
  mailsession_check_folder makes a checkpoint of the session
  
  @param session the session
  
  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_check_folder(mailsession * session);


/*
  NOTE: this function is not implemented in most drivers

  mailsession_examine_folder selects a mailbox as readonly
  
  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_examine_folder(mailsession * session, const char * mb);


/*
  mailsession_select_folder selects a mailbox
  
  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_select_folder(mailsession * session, const char * mb);


/*
  mailsession_expunge_folder deletes all messages marked \Deleted
  
  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_expunge_folder(mailsession * session);


/*
  mailsession_status_folder queries the status of the folder
  (number of messages, number of recent messages, number of unseen messages)
  
  @param session the session
  @param mb mailbox to query
  @param result_messages the number of messages is stored
    in (* result_messages)
  @param result_recent the number of messages is stored
    in (* result_recent)
  @param result_unseen the number of messages is stored
    in (* result_unseen)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_status_folder(mailsession * session, const char * mb,
    uint32_t * result_messages, uint32_t * result_recent,
    uint32_t * result_unseen);


/*
  mailsession_messages_number queries the number of messages in the folder

  @param session the session
  @param mb mailbox to query
  @param result the number of messages is stored in (* result)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_messages_number(mailsession * session, const char * mb,
				uint32_t * result);

/*
  mailsession_recent_number queries the number of recent messages in the folder

  @param session the session
  @param mb mailbox to query
  @param result the number of recent messages is stored in (* result)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_recent_number(mailsession * session,
			      const char * mb, uint32_t * result);

/*
  mailsession_unseen_number queries the number of unseen messages in the folder

  @param session the session
  @param mb mailbox to query
  @param result the number of unseen messages is stored in (* result)

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_unseen_number(mailsession * session, const char * mb,
			      uint32_t * result);

/*
  NOTE: driver's specific should be used

  mailsession_list_folders returns the list of all sub-mailboxes
  of the given mailbox

  @param session the session
  @param mb the mailbox
  @param result list of mailboxes if stored in (* result),
    this structure have to be freed with mail_list_free()

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_list_folders(mailsession * session, const char * mb,
			     struct mail_list ** result);

/*
  NOTE: driver's specific should be used

  mailsession_lsub_folders returns the list of subscribed
  sub-mailboxes of the given mailbox

  @param session the session
  @param mb the mailbox
  @param result list of mailboxes if stored in (* result),
    this structure have to be freed with mail_list_free()

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_lsub_folders(mailsession * session, const char * mb,
			     struct mail_list ** result);

/*
  NOTE: driver's specific should be used

  mailsession_subscribe_folder subscribes to the given mailbox

  @param session the session
  @param mb the mailbox

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_subscribe_folder(mailsession * session, const char * mb);

/*
  NOTE: driver's specific should be used

  mailsession_unsubscribe_folder unsubscribes to the given mailbox

  @param session the session
  @param mb the mailbox

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_unsubscribe_folder(mailsession * session, const char * mb);

/*
  mailsession_append_message adds a RFC 2822 message to the current
  given mailbox

  @param session the session
  @param message is a string that contains the RFC 2822 message
  @param size this is the size of the message

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_append_message(mailsession * session,
			       const char * message, size_t size);

LIBETPAN_EXPORT
int mailsession_append_message_flags(mailsession * session,
    const char * message, size_t size, struct mail_flags * flags);

/*
  NOTE: some drivers does not implement this

  mailsession_copy_message copies a message whose number is given to
  a given mailbox. The mailbox must be accessible from the same session.

  @param session the session
  @param num the message number
  @param mb the destination mailbox

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_copy_message(mailsession * session,
			     uint32_t num, const char * mb);

/*
  NOTE: some drivers does not implement this

  mailsession_move_message copies a message whose number is given to
  a given mailbox. The mailbox must be accessible from the same session.

  @param session the session
  @param num the message number
  @param mb the destination mailbox

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_move_message(mailsession * session,
			     uint32_t num, const char * mb);

/*
  mailsession_get_messages_list returns the list of message numbers
  of the current mailbox.

  @param session the session
  @param result the list of message numbers will be stored in (* result),
    this structure have to be freed with mailmessage_list_free()

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_get_messages_list(mailsession * session,
				  struct mailmessage_list ** result);

/*
  mailsession_get_envelopes_list fills the parsed fields in the
  mailmessage structures of the mailmessage_list.

  @param session the session
  @param result this is the list of mailmessage structures

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_get_envelopes_list(mailsession * session,
				   struct mailmessage_list * result);

/*
  NOTE: some drivers does not implement this

  mailsession_remove_message removes the given message from the mailbox.
  The message is permanently deleted.

  @param session the session
  @param num is the message number

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_remove_message(mailsession * session, uint32_t num);


/*
  NOTE: this function is not implemented in most drivers

  mailsession_search_message returns a list of message numbers that
  corresponds to the given criteria.

  @param session the session
  @param charset is the charset to use (it can be NULL)
  @param key is the list of criteria
  @param result the search result is stored in (* result),
    this structure have to be freed with mail_search_result_free()

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

#if 0
LIBETPAN_EXPORT
int mailsession_search_messages(mailsession * session, const char * charset,
				struct mail_search_key * key,
				struct mail_search_result ** result);
#endif

/*
  mailsession_get_message returns a mailmessage structure that corresponds
  to the given message number.
  * WARNING * mailsession_get_message_by_uid() should be used instead.

  @param session the session
  @param num the message number
  @param result the allocated mailmessage structure will be stored
    in (* result), this structure have to be freed with mailmessage_free() 

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_get_message(mailsession * session,
			    uint32_t num, mailmessage ** result);

/*
  mailsession_get_message_by_uid returns a mailmessage structure
  that corresponds to the given message unique identifier.
  This is currently implemented only for cached drivers.
  * WARNING * That will deprecates the use of mailsession_get_message()
  
  @param session the session
  @param uid the message unique identifier
  @param result the allocated mailmessage structure will be stored
    in (* result), this structure have to be freed with mailmessage_free() 
  
  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_get_message_by_uid(mailsession * session,
    const char * uid, mailmessage ** result);


/*
  mailsession_login notifies the SASL authentication information
  to the session

  @param auth_type      type of SASL authentication
  @param server_fqdn    server full qualified domain name
  @param local_ip_port  local IP:port (client)
  @param remote_ip_port remote IP:port (server)
  @param login          login
  @param auth_name      authentication name
  @param password       password
  @param realm          realm

  @return MAIL_NO_ERROR is returned on success, MAIL_ERROR_XXX is returned
    on error
*/

LIBETPAN_EXPORT
int mailsession_login_sasl(mailsession * session, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);

#ifdef __cplusplus
}
#endif

#endif

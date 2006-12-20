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
 * $Id: mailimap.h,v 1.18 2006/10/20 00:13:30 hoa Exp $
 */

#ifndef MAILIMAP_H

#define MAILIMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libetpan/mailimap_types.h>
#include <libetpan/mailimap_types_helper.h>
#include <libetpan/mailimap_helper.h>

#include <libetpan/mailimap_socket.h>
#include <libetpan/mailimap_ssl.h>

#include <libetpan/acl.h>
#include <libetpan/annotatemore.h>
#include <libetpan/uidplus.h>

/*
  mailimap_connect()

  This function will connect the IMAP session with the given stream.

  @param session  the IMAP session
  @param s        stream to use

  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes
  
  note that on success, MAILIMAP_NO_ERROR_AUTHENTICATED or
    MAILIMAP_NO_ERROR_NON_AUTHENTICATED is returned

  MAILIMAP_NO_ERROR_NON_AUTHENTICATED is returned when you need to
  use mailimap_login() to authenticate, else
  MAILIMAP_NO_ERROR_AUTHENTICATED is returned.
*/

LIBETPAN_EXPORT
int mailimap_connect(mailimap * session, mailstream * s);

/*
  mailimap_append()

  This function will append a given message to the given mailbox
  by sending an APPEND command.

  @param session       the IMAP session
  @param mailbox       name of the mailbox
  @param flag_list     flags of the message
  @param date_time     timestamp of the message
  @param literal       content of the message
  @param literal_size  size of the message
  
  @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_append(mailimap * session, const char * mailbox,
    struct mailimap_flag_list * flag_list,
    struct mailimap_date_time * date_time,
    const char * literal, size_t literal_size);

/*
   mailimap_noop()
   
   This function will poll for an event on the server by
   sending a NOOP command to the IMAP server

   @param session IMAP session
   
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR_XXX codes
*/

LIBETPAN_EXPORT
int mailimap_noop(mailimap * session);

/*
   mailimap_logout()
   
   This function will logout from an IMAP server by sending
   a LOGOUT command.

   @param session IMAP session
  
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_logout(mailimap * session);

/*
   mailimap_capability()
   
   This function will query an IMAP server for his capabilities
   by sending a CAPABILITY command.

   @param session IMAP session
   @param result  The result of this command is a list of
     capabilities and it is stored into (* result).

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
 */

LIBETPAN_EXPORT
int mailimap_capability(mailimap * session,
			struct mailimap_capability_data ** result);

/*
   mailimap_check()

   This function will request for a checkpoint of the mailbox by
   sending a CHECK command.
   
   @param session IMAP session

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
 */

LIBETPAN_EXPORT
int mailimap_check(mailimap * session);

/*
   mailimap_close()

   This function will close the selected mailbox by sending
   a CLOSE command.

   @param session IMAP session
   
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
 */

LIBETPAN_EXPORT
int mailimap_close(mailimap * session);

/*
   mailimap_expunge()

   This function will permanently remove from the selected mailbox
   message that have the \Deleted flag set.

   @param session IMAP session

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_expunge(mailimap * session);

/*
   mailimap_copy()

   This function will copy the given messages from the selected mailbox
   to the given mailbox. 

   @param session IMAP session
   @param set     This is a set of message numbers.
   @param mb      This is the destination mailbox.

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
 */

LIBETPAN_EXPORT
int mailimap_copy(mailimap * session, struct mailimap_set * set,
    const char * mb);

/*
   mailimap_uid_copy()

   This function will copy the given messages from the selected mailbox
   to the given mailbox. 

   @param session IMAP session
   @param set     This is a set of message unique identifiers.
   @param mb      This is the destination mailbox.

   @return the return code is one of MAILIMAP_ERROR_XXX or
   MAILIMAP_NO_ERROR codes
 */

LIBETPAN_EXPORT
int mailimap_uid_copy(mailimap * session,
    struct mailimap_set * set, const char * mb);

/*
   mailimap_create()

   This function will create a mailbox.

   @param session IMAP session
   @param mb      This is the name of the mailbox to create.

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_create(mailimap * session, const char * mb);

/*
   mailimap_delete()

   This function will delete a mailox.

   @param session IMAP session
   @param mb      This is the name of the mailbox to delete.

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_delete(mailimap * session, const char * mb);

/*
   mailimap_examine()

   This function will select the mailbox for read-only operations.

   @param session IMAP session
   @param mb      This is the name of the mailbox to select.

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_examine(mailimap * session, const char * mb);

/*
  mailimap_fetch()

  This function will retrieve data associated with the given message
  numbers.
  
  @param session    IMAP session
  @param set        set of message numbers
  @param fetch_type type of information to be retrieved
  @param result     The result of this command is a clist
    and it is stored into (* result). Each element of the clist is a
    (struct mailimap_msg_att *).

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int
mailimap_fetch(mailimap * session, struct mailimap_set * set,
	       struct mailimap_fetch_type * fetch_type, clist ** result);

/*
  mailimap_fetch()

  This function will retrieve data associated with the given message
  numbers.
  
  @param session    IMAP session
  @param set        set of message unique identifiers
  @param fetch_type type of information to be retrieved
  @param result     The result of this command is a clist
    and it is stored into (* result). Each element of the clist is a
    (struct mailimap_msg_att *).

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/
   
LIBETPAN_EXPORT
int
mailimap_uid_fetch(mailimap * session,
		   struct mailimap_set * set,
		   struct mailimap_fetch_type * fetch_type, clist ** result);

/*
   mailimap_fetch_list_free()
   
   This function will free the result of a fetch command.

   @param fetch_list  This is the clist containing
     (struct mailimap_msg_att *) elements to free.
*/

LIBETPAN_EXPORT
void mailimap_fetch_list_free(clist * fetch_list);

/*
   mailimap_list()

   This function will return the list of the mailbox
   available on the server.

   @param session IMAP session
   @param mb      This is the reference name that informs
     of the level of hierarchy
   @param list_mb mailbox name with possible wildcard
   @param result  This will store a clist of (struct mailimap_mailbox_list *)
     in (* result)
 
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_list(mailimap * session, const char * mb,
    const char * list_mb, clist ** result);

/*
   mailimap_login()

   This function will authenticate the client.

   @param session   IMAP session
   @param userid    login of the user
   @param password  password of the user

   @return the return code is one of MAILIMAP_ERROR_XXX or
    MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_login(mailimap * session,
    const char * userid, const char * password);

/*
   mailimap_authenticate()
   
   This function will authenticate the client.
   TODO : documentation
*/

LIBETPAN_EXPORT
int mailimap_authenticate(mailimap * session, const char * auth_type,
    const char * server_fqdn,
    const char * local_ip_port,
    const char * remote_ip_port,
    const char * login, const char * auth_name,
    const char * password, const char * realm);


/*
   mailimap_lsub()

   This function will return the list of the mailbox
   that the client has subscribed to.

   @param session IMAP session
   @param mb      This is the reference name that informs
   of the level of hierarchy
   @param list_mb mailbox name with possible wildcard
   @param result  This will store a list of (struct mailimap_mailbox_list *)
   in (* result)
   
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_lsub(mailimap * session, const char * mb,
		  const char * list_mb, clist ** result);

/*
   mailimap_list_result_free()

   This function will free the clist of (struct mailimap_mailbox_list *)

   @param list  This is the clist to free.
*/

LIBETPAN_EXPORT
void mailimap_list_result_free(clist * list);

/*
   mailimap_rename()

   This function will change the name of a mailbox.

   @param session  IMAP session
   @param mb       current name
   @param new_name new name

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_rename(mailimap * session,
    const char * mb, const char * new_name);

/*
   mailimap_search()

   All mails that match the given criteria will be returned
   their numbers in the result list.

   @param session  IMAP session
   @param charset  This indicates the charset of the strings that appears
   in the searching criteria
   @param key      This is the searching criteria
   @param result   The result is a clist of (uint32_t *) and will be
   stored in (* result).
   
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int
mailimap_search(mailimap * session, const char * charset,
    struct mailimap_search_key * key, clist ** result);

/*
   mailimap_uid_search()


   All mails that match the given criteria will be returned
   their unique identifiers in the result list.

   @param session  IMAP session
   @param charset  This indicates the charset of the strings that appears
   in the searching criteria
   @param key      This is the searching criteria
   @param result   The result is a clist of (uint32_t *) and will be
   stored in (* result).
   
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int
mailimap_uid_search(mailimap * session, const char * charset,
    struct mailimap_search_key * key, clist ** result);

/*
   mailimap_search_result_free()

   This function will free the result of the a search.

   @param search_result   This is a clist of (uint32_t *) returned
     by mailimap_uid_search() or mailimap_search()
*/

LIBETPAN_EXPORT
void mailimap_search_result_free(clist * search_result);

/*
   mailimap_select()

   This function will select a given mailbox so that messages in the
   mailbox can be accessed.
   
   @param session          IMAP session
   @param mb  This is the name of the mailbox to select.

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int
mailimap_select(mailimap * session, const char * mb);

/*
   mailimap_status()

   This function will return informations about a given mailbox.

   @param session          IMAP session
   @param mb               This is the name of the mailbox
   @param status_att_list  This is the list of mailbox information to return
   @param result           List of returned values

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int
mailimap_status(mailimap * session, const char * mb,
		struct mailimap_status_att_list * status_att_list,
		struct mailimap_mailbox_data_status ** result);

/*
   mailimap_uid_store()

   This function will alter the data associated with some messages
   (flags of the messages).

   @param session          IMAP session
   @param set              This is a list of message numbers.
   @param store_att_flags  This is the data to associate with the
     given messages
   
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int
mailimap_store(mailimap * session,
	       struct mailimap_set * set,
	       struct mailimap_store_att_flags * store_att_flags);

/*
   mailimap_uid_store()

   This function will alter the data associated with some messages
   (flags of the messages).

   @param session          IMAP session
   @param set              This is a list of message unique identifiers.
   @param store_att_flags  This is the data to associate with the
     given messages
   
   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int
mailimap_uid_store(mailimap * session,
    struct mailimap_set * set,
    struct mailimap_store_att_flags * store_att_flags);

/*
   mailimap_subscribe()

   This function adds the specified mailbox name to the
   server's set of "active" or "subscribed" mailboxes.

   @param session   IMAP session
   @param mb        This is the name of the mailbox

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_subscribe(mailimap * session, const char * mb);

/*
   mailimap_unsubscribe()

   This function removes the specified mailbox name to the
   server's set of "active" or "subscribed" mailboxes.

   @param session   IMAP session
   @param mb        This is the name of the mailbox

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR codes
*/

LIBETPAN_EXPORT
int mailimap_unsubscribe(mailimap * session, const char * mb);

/*
   mailimap_starttls()

   This function starts change the mode of the connection to
   switch to SSL connection.

   @param session   IMAP session

   @return the return code is one of MAILIMAP_ERROR_XXX or
     MAILIMAP_NO_ERROR_XXX codes
 */

LIBETPAN_EXPORT
int mailimap_starttls(mailimap * session);

/*
   mailimap_new()

   This function returns a new IMAP session.

   @param progr_rate  When downloading messages, a function will be called
     each time the amount of bytes downloaded reaches a multiple of this
     value, this can be 0.
   @param progr_fun   This is the function to call to notify the progress,
     this can be NULL.

   @return an IMAP session is returned.
 */

LIBETPAN_EXPORT
mailimap * mailimap_new(size_t imap_progr_rate,
    progress_function * imap_progr_fun);

/*
   mailimap_free()

   This function will free the data structures associated with
   the IMAP session.

   @param session   IMAP session
 */

LIBETPAN_EXPORT
void mailimap_free(mailimap * session);

int send_current_tag(mailimap * session);

char * read_line(mailimap * session);

int parse_response(mailimap * session,
			   struct mailimap_response ** result);

#ifdef __cplusplus
}
#endif

#endif

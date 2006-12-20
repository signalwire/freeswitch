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
 * $Id: maildriver_types.h,v 1.47 2006/06/26 11:50:27 hoa Exp $
 */

#ifndef MAILDRIVER_TYPES_H

#define MAILDRIVER_TYPES_H

#ifndef _MSC_VER
#	ifdef HAVE_INTTYPES_H
#		include <inttypes.h>
#	endif
#	include <sys/types.h>
#endif

#include <libetpan/mailstream.h>
#include <libetpan/mailimf.h>
#include <libetpan/mailmime.h>
#include <libetpan/carray.h>

#include <libetpan/mailthread_types.h>
#include <libetpan/maildriver_errors.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mailsession_driver mailsession_driver;

typedef struct mailsession mailsession;

typedef struct mailmessage_driver mailmessage_driver;

typedef struct mailmessage mailmessage;


/*
  mailmessage_list is a list of mailmessage
  
  - tab is an array of mailmessage structures
*/

struct mailmessage_list {
  carray * msg_tab; /* elements are (mailmessage *) */
};

LIBETPAN_EXPORT
struct mailmessage_list * mailmessage_list_new(carray * msg_tab);

LIBETPAN_EXPORT
void mailmessage_list_free(struct mailmessage_list * env_list);

/*
  mail_list is a list of mailbox names

  - list is a list of mailbox names
*/

struct mail_list {
  clist * mb_list; /* elements are (char *) */
};

LIBETPAN_EXPORT
struct mail_list * mail_list_new(clist * mb_list);

LIBETPAN_EXPORT
void mail_list_free(struct mail_list * resp);

/*
  This is a flag value.
  Flags can be combined with OR operation
*/

enum {
  MAIL_FLAG_NEW       = 1 << 0,
  MAIL_FLAG_SEEN      = 1 << 1,
  MAIL_FLAG_FLAGGED   = 1 << 2,
  MAIL_FLAG_DELETED   = 1 << 3,
  MAIL_FLAG_ANSWERED  = 1 << 4,
  MAIL_FLAG_FORWARDED = 1 << 5,
  MAIL_FLAG_CANCELLED = 1 << 6
};

/*
  mail_flags is the value of a flag related to a message.
  
  - flags is the standard flags value

  - extension is a list of unknown flags for libEtPan!
*/

struct mail_flags {
  uint32_t fl_flags;
  clist * fl_extension; /* elements are (char *) */
};

LIBETPAN_EXPORT
struct mail_flags * mail_flags_new(uint32_t fl_flags, clist * fl_ext);

LIBETPAN_EXPORT
void mail_flags_free(struct mail_flags * flags);

/*
  This function creates a flag for a new message
*/

LIBETPAN_EXPORT
struct mail_flags * mail_flags_new_empty(void);


/*
  mailimf_date_time_comp compares two dates
  
  
*/

LIBETPAN_EXPORT
int32_t mailimf_date_time_comp(struct mailimf_date_time * date1,
    struct mailimf_date_time * date2);

/*
  this is type type of the search criteria
*/

enum {
  MAIL_SEARCH_KEY_ALL,        /* all messages correspond */
  MAIL_SEARCH_KEY_ANSWERED,   /* messages with flag \Answered */
  MAIL_SEARCH_KEY_BCC,        /* messages which Bcc field contains
                                 a given string */
  MAIL_SEARCH_KEY_BEFORE,     /* messages which internal date is earlier
                                 than the specified date */
  MAIL_SEARCH_KEY_BODY,       /* message that contains the given string
                                 (in header and text parts) */
  MAIL_SEARCH_KEY_CC,         /* messages whose Cc field contains the
                                 given string */
  MAIL_SEARCH_KEY_DELETED,    /* messages with the flag \Deleted */
  MAIL_SEARCH_KEY_FLAGGED,    /* messages with the flag \Flagged */ 
  MAIL_SEARCH_KEY_FROM,       /* messages whose From field contains the
                                 given string */
  MAIL_SEARCH_KEY_NEW,        /* messages with the flag \Recent and not
                                 the \Seen flag */
  MAIL_SEARCH_KEY_OLD,        /* messages that do not have the
                                 \Recent flag set */
  MAIL_SEARCH_KEY_ON,         /* messages whose internal date is the
                                 specified date */
  MAIL_SEARCH_KEY_RECENT,     /* messages with the flag \Recent */
  MAIL_SEARCH_KEY_SEEN,       /* messages with the flag \Seen */
  MAIL_SEARCH_KEY_SINCE,      /* messages whose internal date is later
                                 than specified date */
  MAIL_SEARCH_KEY_SUBJECT,    /* messages whose Subject field contains the
                                 given string */
  MAIL_SEARCH_KEY_TEXT,       /* messages whose text part contains the
                                 given string */
  MAIL_SEARCH_KEY_TO,         /* messages whose To field contains the
                                 given string */
  MAIL_SEARCH_KEY_UNANSWERED, /* messages with no flag \Answered */
  MAIL_SEARCH_KEY_UNDELETED,  /* messages with no flag \Deleted */
  MAIL_SEARCH_KEY_UNFLAGGED,  /* messages with no flag \Flagged */
  MAIL_SEARCH_KEY_UNSEEN,     /* messages with no flag \Seen */
  MAIL_SEARCH_KEY_HEADER,     /* messages whose given field 
                                 contains the given string */
  MAIL_SEARCH_KEY_LARGER,     /* messages whose size is larger then
                                 the given size */
  MAIL_SEARCH_KEY_NOT,        /* not operation of the condition */
  MAIL_SEARCH_KEY_OR,         /* or operation between two conditions */
  MAIL_SEARCH_KEY_SMALLER,    /* messages whose size is smaller than
                                 the given size */
  MAIL_SEARCH_KEY_MULTIPLE    /* the boolean operator between the
                                 conditions is AND */
};

/*
  mail_search_key is the condition on the messages to return
  
  - type is the type of the condition

  - bcc is the text to search in the Bcc field when type is
    MAIL_SEARCH_KEY_BCC, should be allocated with malloc()

  - before is a date when type is MAIL_SEARCH_KEY_BEFORE

  - body is the text to search in the message when type is
    MAIL_SEARCH_KEY_BODY, should be allocated with malloc()

  - cc is the text to search in the Cc field when type is
    MAIL_SEARCH_KEY_CC, should be allocated with malloc()
  
  - from is the text to search in the From field when type is
    MAIL_SEARCH_KEY_FROM, should be allocated with malloc()

  - on is a date when type is MAIL_SEARCH_KEY_ON

  - since is a date when type is MAIL_SEARCH_KEY_SINCE
  
  - subject is the text to search in the Subject field when type is
    MAILIMAP_SEARCH_KEY_SUBJECT, should be allocated with malloc()

  - text is the text to search in the text part of the message when
    type is MAILIMAP_SEARCH_KEY_TEXT, should be allocated with malloc()

  - to is the text to search in the To field when type is
    MAILIMAP_SEARCH_KEY_TO, should be allocated with malloc()

  - header_name is the header name when type is MAILIMAP_SEARCH_KEY_HEADER,
    should be allocated with malloc()

  - header_value is the text to search in the given header when type is
    MAILIMAP_SEARCH_KEY_HEADER, should be allocated with malloc()

  - larger is a size when type is MAILIMAP_SEARCH_KEY_LARGER

  - not is a condition when type is MAILIMAP_SEARCH_KEY_NOT

  - or1 is a condition when type is MAILIMAP_SEARCH_KEY_OR

  - or2 is a condition when type is MAILIMAP_SEARCH_KEY_OR
  
  - sentbefore is a date when type is MAILIMAP_SEARCH_KEY_SENTBEFORE

  - senton is a date when type is MAILIMAP_SEARCH_KEY_SENTON

  - sentsince is a date when type is MAILIMAP_SEARCH_KEY_SENTSINCE

  - smaller is a size when type is MAILIMAP_SEARCH_KEY_SMALLER

  - multiple is a set of message when type is MAILIMAP_SEARCH_KEY_MULTIPLE
*/

#if 0
struct mail_search_key {
  int sk_type;
  union {
    char * sk_bcc;
    struct mailimf_date_time * sk_before;
    char * sk_body;
    char * sk_cc;
    char * sk_from;
    struct mailimf_date_time * sk_on;
    struct mailimf_date_time * sk_since;
    char * sk_subject;
    char * sk_text;
    char * sk_to;
    char * sk_header_name;
    char * sk_header_value;
    size_t sk_larger;
    struct mail_search_key * sk_not;
    struct mail_search_key * sk_or1;
    struct mail_search_key * sk_or2;
    size_t sk_smaller;
    clist * sk_multiple; /* list of (struct mailimap_search_key *) */
  } sk_data;
};


struct mail_search_key *
mail_search_key_new(int sk_type,
    char * sk_bcc, struct mailimf_date_time * sk_before,
    char * sk_body, char * sk_cc, char * sk_from,
    struct mailimf_date_time * sk_on, struct mailimf_date_time * sk_since,
    char * sk_subject, char * sk_text, char * sk_to,
    char * sk_header_name, char * sk_header_value, size_t sk_larger,
    struct mail_search_key * sk_not, struct mail_search_key * sk_or1,
    struct mail_search_key * sk_or2, size_t sk_smaller,
    clist * sk_multiple);

void mail_search_key_free(struct mail_search_key * key);
#endif

/*
  mail_search_result is a list of message numbers that is returned
  by the mailsession_search_messages function()
*/

#if 0
struct mail_search_result {
  clist * sr_list; /* list of (uint32_t *) */
};

struct mail_search_result * mail_search_result_new(clist * sr_list);

void mail_search_result_free(struct mail_search_result * search_result);
#endif


/*
  There is three kinds of identities :
  - storage
  - folders
  - session

  A storage (struct mailstorage) represents whether a server or
  a main path,

  A storage can be an IMAP server, the root path of a MH or a mbox file.

  Folders (struct mailfolder) are the mailboxes we can
  choose in the server or as sub-folder of the main path.

  Folders for IMAP are the IMAP mailboxes, for MH this is one of the
  folder of the MH storage, for mbox, there is only one folder, the
  mbox file content;

  A mail session (struct mailsession) is whether a connection to a server
  or a path that is open. It is the abstraction lower folders and storage.
  It allow us to send commands.

  We have a session driver for mail session for each kind of storage.

  From a session, we can get a message (struct mailmessage) to read.
  We have a message driver for each kind of storage.
*/

/*
  maildriver is the driver structure for mail sessions

  - name is the name of the driver
  
  - initialize() is the function that will initializes a data structure
      specific to the driver, it returns a value that will be stored
      in the field data of the session.
      The field data of the session is the state of the session,
      the internal data structure used by the driver.
      It is called when creating the mailsession structure with
      mailsession_new().
  
  - uninitialize() frees the structure created with initialize()

  - parameters() implements functions specific to the given mail access
  
  - connect_stream() connects a stream to the session

  - connect_path() notify a main path to the session

  - starttls() changes the current stream to a TLS stream
  
  - login() notifies the user and the password to authenticate to the
      session

  - logout() exits the session and closes the stream

  - noop() does no operation on the session, but it can be
      used to poll for the status of the connection.

  - build_folder_name() will return an allocated string with
      that contains the complete path of the folder to create

  - create_folder() creates the folder that corresponds to the
      given name

  - delete_folder() deletes the folder that corresponds to the
      given name

  - rename_folder() change the name of the folder

  - check_folder() makes a checkpoint of the session

  - examine_folder() selects a mailbox as readonly

  - select_folder() selects a mailbox

  - expunge_folder() deletes all messages marked \Deleted

  - status_folder() queries the status of the folder
      (number of messages, number of recent messages, number of
      unseen messages)

  - messages_number() queries the number of messages in the folder

  - recent_number() queries the number of recent messages in the folder

  - unseen_number() queries the number of unseen messages in the folder

  - list_folders() returns the list of all sub-mailboxes
      of the given mailbox

  - lsub_folders() returns the list of subscribed
      sub-mailboxes of the given mailbox

  - subscribe_folder() subscribes to the given mailbox

  - unsubscribe_folder() unsubscribes to the given mailbox

  - append_message() adds a RFC 2822 message to the current
      given mailbox

  - copy_message() copies a message whose number is given to
       a given mailbox. The mailbox must be accessible from
       the same session.

  - move_message() copies a message whose number is given to
       a given mailbox. The mailbox must be accessible from the
       same session.

  - get_messages_list() returns the list of message numbers
      of the current mailbox.

  - get_envelopes_list() fills the parsed fields in the
      mailmessage structures of the mailmessage_list.

  - remove_message() removes the given message from the mailbox.
      The message is permanently deleted.

  - search_message() returns a list of message numbers that
      corresponds to the given criteria.

  - get_message returns a mailmessage structure that corresponds
      to the given message number.

  - get_message_by_uid returns a mailmessage structure that corresponds
      to the given message unique identifier.

  * mandatory functions are the following :

  - connect_stream() of connect_path()
  - logout()
  - get_messages_list()
  - get_envelopes_list()

  * we advise you to implement these functions :

  - select_folder() (in case a session can access several folders)
  - noop() (to check if the server is responding)
  - check_folder() (to make a checkpoint of the session)
  - status_folder(), messages_number(), recent_number(), unseen_number()
      (to get stat of the folder)
  - append_message() (but can't be done in the case of POP3 at least)
  - login() in a case of an authenticated driver.
  - starttls() in a case of a stream driver, if the procotol supports
      STARTTLS.
  - get_message_by_uid() so that the application can remember the message
      by UID and build its own list of messages.
  - login_sasl() notifies the SASL information to authenticate to the
      session.

  * drivers' specific :

  Everything that is specific to the driver will be implemented in this
  function :

  - parameters()
*/

struct mailsession_driver {
  char * sess_name;

  int (* sess_initialize)(mailsession * session);
  void (* sess_uninitialize)(mailsession * session);

  int (* sess_parameters)(mailsession * session,
      int id, void * value);

  int (* sess_connect_stream)(mailsession * session, mailstream * s);
  int (* sess_connect_path)(mailsession * session, const char * path);

  int (* sess_starttls)(mailsession * session);

  int (* sess_login)(mailsession * session, const char * userid, const char * password);
  int (* sess_logout)(mailsession * session);
  int (* sess_noop)(mailsession * session);

  /* folders operations */

  int (* sess_build_folder_name)(mailsession * session, const char * mb,
      const char * name, char ** result);

  int (* sess_create_folder)(mailsession * session, const char * mb);
  int (* sess_delete_folder)(mailsession * session, const char * mb);
  int (* sess_rename_folder)(mailsession * session, const char * mb,
      const char * new_name);
  int (* sess_check_folder)(mailsession * session);
  int (* sess_examine_folder)(mailsession * session, const char * mb);
  int (* sess_select_folder)(mailsession * session, const char * mb);
  int (* sess_expunge_folder)(mailsession * session);
  int (* sess_status_folder)(mailsession * session, const char * mb,
      uint32_t * result_num, uint32_t * result_recent,
      uint32_t * result_unseen);
  int (* sess_messages_number)(mailsession * session, const char * mb,
      uint32_t * result);
  int (* sess_recent_number)(mailsession * session, const char * mb,
      uint32_t * result);
  int (* sess_unseen_number)(mailsession * session, const char * mb,
      uint32_t * result);

  int (* sess_list_folders)(mailsession * session, const char * mb,
      struct mail_list ** result);
  int (* sess_lsub_folders)(mailsession * session, const char * mb,
      struct mail_list ** result);

  int (* sess_subscribe_folder)(mailsession * session, const char * mb);
  int (* sess_unsubscribe_folder)(mailsession * session, const char * mb);

  /* messages operations */

  int (* sess_append_message)(mailsession * session,
      const char * message, size_t size);
  int (* sess_append_message_flags)(mailsession * session,
      const char * message, size_t size, struct mail_flags * flags);
  int (* sess_copy_message)(mailsession * session,
      uint32_t num, const char * mb);
  int (* sess_move_message)(mailsession * session,
      uint32_t num, const char * mb);

  int (* sess_get_message)(mailsession * session,
      uint32_t num, mailmessage ** result);

  int (* sess_get_message_by_uid)(mailsession * session,
      const char * uid, mailmessage ** result);
  
  int (* sess_get_messages_list)(mailsession * session,
      struct mailmessage_list ** result);
  int (* sess_get_envelopes_list)(mailsession * session,
      struct mailmessage_list * env_list);
  int (* sess_remove_message)(mailsession * session, uint32_t num);
#if 0
  int (* sess_search_messages)(mailsession * session, const char * charset,
      struct mail_search_key * key,
      struct mail_search_result ** result);
#endif
  
  int (* sess_login_sasl)(mailsession * session, const char * auth_type,
      const char * server_fqdn,
      const char * local_ip_port,
      const char * remote_ip_port,
      const char * login, const char * auth_name,
      const char * password, const char * realm);
};


/*
  session is the data structure for a mail session.

  - data is the internal data structure used by the driver
    It is called when initializing the mailsession structure.

  - driver is the driver used for the session
*/

struct mailsession {
  void * sess_data;
  mailsession_driver * sess_driver;
};




/*
  mailmessage_driver is the driver structure to get information from messages.
  
  - name is the name of the driver

  - initialize() is the function that will initializes a data structure
      specific to the driver, it returns a value that will be stored
      in the field data of the mailsession.
      The field data of the session is the state of the session,
      the internal data structure used by the driver.
      It is called when initializing the mailmessage structure with
      mailmessage_init().
  
  - uninitialize() frees the structure created with initialize().
      It will be called by mailmessage_free().

  - flush() will free from memory all temporary structures of the message
      (for example, the MIME structure of the message).

  - fetch_result_free() will free all strings resulted by fetch() or
      any fetch_xxx() functions that returns a string.

  - fetch() returns the content of the message (headers and text).

  - fetch_header() returns the content of the headers.

  - fetch_body() returns the message text (message content without headers)

  - fetch_size() returns the size of the message content.

  - get_bodystructure() returns the MIME structure of the message.

  - fetch_section() returns the content of a given MIME part

  - fetch_section_header() returns the header of the message
      contained by the given MIME part.

  - fetch_section_mime() returns the MIME headers of the
      given MIME part.

  - fetch_section_body() returns the text (if this is a message, this is the
      message content without headers) of the given MIME part.

  - fetch_envelope() returns a mailimf_fields structure, with a list of
      fields chosen by the driver.

  - get_flags() returns a the flags related to the message.
      When you want to get flags of a message, you have to make sure to
      call get_flags() at least once before using directly message->flags.
*/

#define LIBETPAN_MAIL_MESSAGE_CHECK

struct mailmessage_driver {
  char * msg_name;

  int (* msg_initialize)(mailmessage * msg_info);
 
  void (* msg_uninitialize)(mailmessage * msg_info);
  
  void (* msg_flush)(mailmessage * msg_info);

  void (* msg_check)(mailmessage * msg_info);

  void (* msg_fetch_result_free)(mailmessage * msg_info,
			     char * msg);

  int (* msg_fetch)(mailmessage * msg_info,
		char ** result,
		size_t * result_len);
       
  int (* msg_fetch_header)(mailmessage * msg_info,
		       char ** result,
		       size_t * result_len);
  
  int (* msg_fetch_body)(mailmessage * msg_info,
		     char ** result, size_t * result_len);

  int (* msg_fetch_size)(mailmessage * msg_info,
		     size_t * result);
  
  int (* msg_get_bodystructure)(mailmessage * msg_info,
			    struct mailmime ** result);
  
  int (* msg_fetch_section)(mailmessage * msg_info,
			struct mailmime * mime,
			char ** result, size_t * result_len);
  
  int (* msg_fetch_section_header)(mailmessage * msg_info,
			       struct mailmime * mime,
			       char ** result,
			       size_t * result_len);
  
  int (* msg_fetch_section_mime)(mailmessage * msg_info,
			     struct mailmime * mime,
			     char ** result,
			     size_t * result_len);
  
  int (* msg_fetch_section_body)(mailmessage * msg_info,
			     struct mailmime * mime,
			     char ** result,
			     size_t * result_len);

  int (* msg_fetch_envelope)(mailmessage * msg_info,
			 struct mailimf_fields ** result);

  int (* msg_get_flags)(mailmessage * msg_info,
		    struct mail_flags ** result);
};


/*
  mailmessage is a data structure to get information from messages

  - session is the session linked to the given message, it can be NULL

  - driver is the message driver
  
  - index is the message number

  - uid, when it is not NULL, it means that the folder 
      the folder has persistant message numbers, the string is
      the unique message number in the folder.
      uid should be implemented if possible.
      for drivers where we cannot generate real uid,
      a suggestion is "AAAA-IIII" where AAAA is some
      random session number and IIII the content of index field.

  - size, when it is not 0, is the size of the message content.
  
  - fields, when it is not NULL, are the header fields of the message.

  - flags, when it is not NULL, are the flags related to the message.

  - single_fields, when resolved != 0, is filled with the data of fields.

  - mime, when it is not NULL

  - cached is != 0 when the header fields were read from the cache.
  
  - data is data specific to the driver, this is internal data structure,
      some state of the message.
*/

struct mailmessage {
  mailsession * msg_session;
  mailmessage_driver * msg_driver;
  uint32_t msg_index;
  char * msg_uid;

  size_t msg_size;
  struct mailimf_fields * msg_fields;
  struct mail_flags * msg_flags;

  int msg_resolved;
  struct mailimf_single_fields msg_single_fields;
  struct mailmime * msg_mime;

  /* internal data */

  int msg_cached;
  void * msg_data;
  
 /*
   msg_folder field :
   used to reference the mailfolder, this is a workaround due
   to the problem with initial conception, where folder notion
   did not exist.
 */
  void * msg_folder;
  /* user data */
  void * msg_user_data;
};


/*
  mailmessage_tree is a node in the messages tree (thread)
  
  - parent is the parent of the message, it is NULL if the message
      is the root of the message tree.

  - date is the date of the message in number of second elapsed
      since 00:00:00 on January 1, 1970, Coordinated Universal Time (UTC).

  - msg is the message structure that is stored referenced by the node.
      is msg is NULL, this is a dummy node.

  - children is an array that contains all the children of the node.
      children are mailmessage_tree structures.

  - is_reply is != 0 when the message is a reply or a forward

  - base_subject is the extracted subject of the message.

  - index is the message number.
*/

struct mailmessage_tree {
  struct mailmessage_tree * node_parent;
  char * node_msgid;
  time_t node_date;
  mailmessage * node_msg;
  carray * node_children; /* array of (struct mailmessage_tree *) */

  /* private, used for threading */
  int node_is_reply;
  char * node_base_subject;
};

LIBETPAN_EXPORT
struct mailmessage_tree *
mailmessage_tree_new(char * node_msgid, time_t node_date,
    mailmessage * node_msg);

LIBETPAN_EXPORT
void mailmessage_tree_free(struct mailmessage_tree * tree);

/*
  mailmessage_tree_free_recursive

  if you want to release memory of the given tree and all the sub-trees,
  you can use this function.
*/
LIBETPAN_EXPORT
void mailmessage_tree_free_recursive(struct mailmessage_tree * tree);


struct generic_message_t {
  int (* msg_prefetch)(mailmessage * msg_info);
  void (* msg_prefetch_free)(struct generic_message_t * msg);
  int msg_fetched;
  char * msg_message;
  size_t msg_length;
  void * msg_data;
};

LIBETPAN_EXPORT
const char * maildriver_strerror(int err);

/* basic malloc / free functions to be compliant with the library allocations */
LIBETPAN_EXPORT
void *libetpan_malloc(size_t length);

LIBETPAN_EXPORT
void libetpan_free(void* data);

#ifdef __cplusplus
}
#endif

#endif

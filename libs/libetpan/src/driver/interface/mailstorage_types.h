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
 * $Id: mailstorage_types.h,v 1.9 2006/05/22 13:39:40 hoa Exp $
 */

#ifndef MAILSTORAGE_TYPES_H

#define MAILSTORAGE_TYPES_H

#include <libetpan/maildriver_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mailstorage;

typedef struct mailstorage_driver mailstorage_driver;


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
  mailstorage_driver is the driver structure for mail storages

  - name is the name of the driver
  
  - connect() connects the storage to the remote access or to
      the path in the local filesystem.
      
  - get_folder() can have two kinds of behaviour.
      Either it creates a new session and independant from the session
      used by the storage and select the given mailbox or
      it selects the given mailbox in the current session.
      It depends on the efficiency of the mail driver.

  - uninitialize() frees the data created with mailstorage constructor.
*/

struct mailstorage_driver {
  char * sto_name;
  int (* sto_connect)(struct mailstorage * storage);
  int (* sto_get_folder_session)(struct mailstorage * storage,
      char * pathname, mailsession ** result);
  void (* sto_uninitialize)(struct mailstorage * storage);
};

/*
  mailstorage is the data structure for a storage

  - id is the name of the storage, it can be NULL.
  
  - data is the data specific to the driver.
      This is the internal state of the storage.

  - session is the session related to the storage.

  - driver is the driver for the storage.

  - shared_folders is the list of folders returned by the storage.
*/

struct mailstorage {
  char * sto_id;
  void * sto_data;
  mailsession * sto_session;
  mailstorage_driver * sto_driver;
  clist * sto_shared_folders; /* list of (struct mailfolder *) */
  
  void * sto_user_data;
};



/*
  mailfolder is the data structure for a mailbox

  - pathname is the path of the mailbox on the storage

  - virtual_name is the folder identifier, it can be a path,
      a name or NULL.

  - storage is the storage to which the folder belongs to.

  - session is the session related to the folder. It can be
      different of the session of the storage.

  - shared_session is != 0 if the session is the same as the
      session of the storage.

  - pos is the position of the folder in the "shared_folders" field
      of the storage.

  folders can be chained into a tree.

  - parent is the parent of the folder.

  - sibling_index is the index of the folder in the list of children
      of the parent.
      
  - children is the folder.
*/

struct mailfolder {
  char * fld_pathname;
  char * fld_virtual_name;
  
  struct mailstorage * fld_storage;

  mailsession * fld_session;
  int fld_shared_session;
  clistiter * fld_pos;

  struct mailfolder * fld_parent;
  unsigned int fld_sibling_index;
  carray * fld_children; /* array of (struct mailfolder *) */

  void * fld_user_data;
};

/*
  this is the type of socket connection
*/

enum {
  CONNECTION_TYPE_PLAIN,        /* when the connection is plain text */
  CONNECTION_TYPE_STARTTLS,     /* when the connection is first plain,
                                   then, we want to switch to
                                   TLS (secure connection) */
  CONNECTION_TYPE_TRY_STARTTLS, /* the connection is first plain,
                                   then, we will try to switch to TLS */
  CONNECTION_TYPE_TLS,          /* the connection is over TLS */
  CONNECTION_TYPE_COMMAND,      /* the connection is over a shell command */
  CONNECTION_TYPE_COMMAND_STARTTLS, /* the connection is over a shell
                                       command and STARTTLS will be used */
  CONNECTION_TYPE_COMMAND_TRY_STARTTLS, /* the connection is over
                                           a shell command and STARTTLS will
                                           be tried */
  CONNECTION_TYPE_COMMAND_TLS  /* the connection is over a shell
                                  command in TLS */
};

#ifdef __cplusplus
}
#endif

#endif

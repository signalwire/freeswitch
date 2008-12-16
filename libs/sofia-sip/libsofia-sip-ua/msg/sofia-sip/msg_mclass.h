/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef MSG_MCLASS_H
/** Defined when <sofia-sip/msg_mclass.h> has been included. */
#define MSG_MCLASS_H
/**@ingroup msg_parser
 * @file sofia-sip/msg_mclass.h
 *
 * @brief Parser table and message factory object.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Mon Aug 27 15:44:27 2001 ppessi
 */

#ifndef MSG_HEADER_H
#include <sofia-sip/msg_header.h>
#endif

SOFIA_BEGIN_DECLS

enum {
  /** Default size of hash table */
  MC_HASH_SIZE = 127,
  /** Size of short form table */
  MC_SHORT_SIZE = 'Z' - 'A' + 1
};

/**Header reference.
 *
 * A header reference object contains a pointer to a
 * @ref msg_hclass_s "header class"
 * and a offset to the header objects within the @ref msg_pub_t "public
 * message structure".
 *
 * The @a hr_flags field is used to provide classification of headers. For
 * instance, the msg_extract_errors() returns bitwise or of all hr_flags
 * belonging to headers with parsing errors.
 */
struct msg_href_s
{
  msg_hclass_t  *hr_class;	/**< Header class */
  unsigned short hr_offset;	/**< Offset within public message struct. */
  unsigned short hr_flags;	/**< Header flags */
};

/**Factory object for protocol messages.
 *
 * The message class is a kind of a factory object used to create new
 * message objects for the protocol it represents (see msg_create()).
 *
 * The message class object contains all the information needed to parse a
 * message. It used when headers are added or removed from the message. When
 * a message is sent, the message class is used to order message components
 * and print (encode) the message in text format.
 *
 * The message class contains reference objects to headers and other components
 * within the message. Each reference contains a pointer to a @ref
 * msg_hclass_s "header class" and a offset to the header objects within
 * public message structure. The parser engine uses these references when it
 * adds a newly parsed header object to the message structure. The function
 * msg_find_hclass() searches for the reference of the named header.

 * The application can make a copy of existing message class object using
 * the function msg_mclass_clone(). New headers can be added to the message
 * class with the msg_mclass_insert_header() and msg_mclass_insert()
 * functions. The message class of an existing message object can be found
 * out with the function msg_mclass().
 *
 * @sa sip_default_mclass(), http_default_mclass(), msg_create(),
 * msg_mclass(), msg_mclass_clone(), msg_mclass_insert_header(),
 * msg_mclass_insert_with_mask(), msg_mclass_insert().
 */
struct msg_mclass_s
{
  struct msg_hclass_s
                mc_hclass[1];	/**< Recursive header class */
  char const   *mc_name;	/**< Protocol name, e.g., "SIP/2.0" */
  void         *mc_tag;		/**< Protocol-specific tag */
  unsigned      mc_flags;	/**< Default flags */
  unsigned      mc_msize;	/**< Size of public message structure */
  /** Function extracting the message contents. */
  issize_t    (*mc_extract_body)(msg_t *msg, msg_pub_t *pub,
				 char b[], isize_t bsiz, int eos);

  msg_href_t    mc_request[1];	/**< Request line reference */
  msg_href_t    mc_status[1];	/**< Status line reference */
  msg_href_t    mc_separator[1];/**< Separator line reference */
  msg_href_t    mc_payload[1];	/**< Message body reference */
  msg_href_t    mc_unknown[1];	/**< Reference for unknown headers */
  msg_href_t    mc_error[1];	/**< Reference for erroneous header */
  msg_href_t    mc_multipart[1];/**< Multipart body reference */
  msg_href_t const *
                mc_short;	/**< Short forms (or NULL) */
  short         mc_hash_size;	/**< Size of parsing table  */
  short         mc_hash_used;	/**< Number of headers in parsing table */
  /** Hash table for parsing containing reference for each header. */
  msg_href_t    mc_hash[MC_HASH_SIZE];
};

enum { msg_mclass_copy = 0, msg_mclass_empty = 1 };

SOFIAPUBFUN msg_mclass_t *msg_mclass_clone(msg_mclass_t const *old,
					   int newsize, int empty);

SOFIAPUBFUN int msg_mclass_insert(msg_mclass_t *mc, msg_href_t const *hr);

SOFIAPUBFUN
int msg_mclass_insert_header(msg_mclass_t *mc,
			     msg_hclass_t *hc,
			     unsigned short offset);

SOFIAPUBFUN
int msg_mclass_insert_with_mask(msg_mclass_t *mc,
				msg_hclass_t *hc,
				unsigned short offset,
				unsigned short mask);

SOFIAPUBFUN
msg_href_t const *msg_find_hclass(msg_mclass_t const *, char const *, isize_t *);

SOFIAPUBFUN msg_mclass_t const *msg_mclass(msg_t const *);

SOFIA_END_DECLS

#endif /* !defined(MSG_MCLASS_H) */

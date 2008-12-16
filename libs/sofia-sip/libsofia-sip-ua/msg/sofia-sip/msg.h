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

#ifndef MSG_H
/** Defined when <sofia-sip/msg.h> has been included */
#define MSG_H
/**@file sofia-sip/msg.h
 *
 * Base message interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 18 08:54:48 2000 ppessi
 */

#include <sofia-sip/msg_types.h>
#include <sofia-sip/su_alloc.h>

SOFIA_BEGIN_DECLS

SOFIAPUBFUN msg_t *msg_create(msg_mclass_t const *mc, int flags);
SOFIAPUBFUN void msg_destroy(msg_t *);

SOFIAPUBFUN msg_t *msg_copy(msg_t *);
SOFIAPUBFUN msg_t *msg_dup(msg_t const *);

SOFIAPUBFUN msg_t *msg_make(msg_mclass_t const *mc, int flags,
			    void const *data, ssize_t len);
SOFIAPUBFUN char *msg_as_string(su_home_t *home,
				msg_t *msg, msg_pub_t *pub, int flags,
				size_t *return_len);

SOFIAPUBFUN void msg_set_parent(msg_t *kid, msg_t *dad);

SOFIAPUBFUN msg_t *msg_ref_create(msg_t *);
SOFIAPUBFUN void msg_ref_destroy(msg_t *);

SOFIAPUBFUN msg_pub_t *msg_public(msg_t const *msg, void *tag);
SOFIAPUBFUN msg_pub_t *msg_object(msg_t const *msg);
SOFIAPUBFUN msg_mclass_t const *msg_mclass(msg_t const *msg);

SOFIAPUBFUN int msg_extract(msg_t *msg);
SOFIAPUBFUN unsigned msg_extract_errors(msg_t const *msg);
SOFIAPUBFUN int msg_is_complete(msg_t const *msg);
SOFIAPUBFUN int msg_has_error(msg_t const *msg);
SOFIAPUBFUN msg_header_t **msg_chain_head(msg_t const *msg);

SOFIAPUBFUN int msg_serialize(msg_t *msg, msg_pub_t *mo);
SOFIAPUBFUN int msg_prepare(msg_t *msg);
SOFIAPUBFUN void msg_unprepare(msg_t *msg);
SOFIAPUBFUN int msg_is_prepared(msg_t const *msg);

SOFIAPUBFUN usize_t msg_size(msg_t const *msg);
SOFIAPUBFUN usize_t msg_maxsize(msg_t *msg, usize_t maxsize);

/** Cast a #msg_t pointer to a #su_home_t pointer. */
#define msg_home(h) ((su_home_t*)(h))

/** Streaming state of a #msg_t object. */
enum msg_streaming_status {
  /** Disable streaming */
  msg_stop_streaming = 0,
  /** Enable streaming */
  msg_start_streaming = 1
};

SOFIAPUBFUN int msg_is_streaming(msg_t const *msg);
SOFIAPUBFUN void msg_set_streaming(msg_t *msg, enum msg_streaming_status what);

SOFIAPUBFUN unsigned msg_mark_as_complete(msg_t *msg, unsigned mask);

SOFIAPUBFUN unsigned msg_get_flags(msg_t const *msg, unsigned mask);
SOFIAPUBFUN unsigned msg_set_flags(msg_t *msg, unsigned mask);
SOFIAPUBFUN unsigned msg_zap_flags(msg_t *msg, unsigned mask);

/** Flags controlling parser/printer. */
enum msg_flg_user {
  /** Use compact form when printing. */
  MSG_FLG_COMPACT = (1<<0),
  /** Use canonic representation when printing. */
  MSG_FLG_CANONIC = (1<<1),
  /** Cache a copy of headers when parsing. */
  MSG_FLG_EXTRACT_COPY = (1<<2),
  /** Print comma-separated lists instead of separate headers */
  MSG_FLG_COMMA_LISTS = (1<<3),

  /**Use mailbox format when parsing - in mailbox format
   * message has no body unless Content-Length header is present.
   */
  MSG_FLG_MAILBOX = (1<<4),

  /** Use multiple parts for message body */
  MSG_FLG_CHUNKING = (1<<5),

  /** Enable streaming - parser gives completed message fragments when they
   * are ready to upper layers */
  MSG_FLG_STREAMING = (1<<6),

  /** Make messages threadsafe. */
  MSG_FLG_THRDSAFE = (1<<15),

  MSG_FLG_USERMASK = (1<<16) - 1
};

/** Flags used by parser. */
enum  msg_flg_parser {
  /** Extract headers for this message */
  MSG_FLG_HEADERS = (1<<16),
  /** Extract body for this message */
  MSG_FLG_BODY = (1<<17),
  /** Extract chunks for this message */
  MSG_FLG_CHUNKS = (1<<18),
  /** Extract trailers for this message */
  MSG_FLG_TRAILERS = (1<<19),
  /** Extract last component of this message */
  MSG_FLG_FRAGS = (1<<20),
  /** This message has been completely extracted */
  MSG_FLG_COMPLETE = (1<<24),

  /** This message has parsing errors */
  MSG_FLG_ERROR = (1<<25),
  /** This message is too large */
  MSG_FLG_TOOLARGE = (1<<26),
  /** This message is truncated */
  MSG_FLG_TRUNC = (1<<27),
  /** This message has timeout */
  MSG_FLG_TIMEOUT = (1<<28),

  MSG_FLG_PARSERMASK = ((-1) ^ ((1<<16) - 1))
};

#define MSG_DO_COMPACT      MSG_FLG_COMPACT
#define MSG_DO_CANONIC      MSG_FLG_CANONIC
#define MSG_DO_EXTRACT_COPY MSG_FLG_EXTRACT_COPY

/** Test if all the flags in @a v are set in @a f. */
#define MSG_FLAGS(f, v) (((f) & (v)) == v)

#define MSG_IS_COMPACT(f)      MSG_FLAGS((f), MSG_FLG_COMPACT)
#define MSG_IS_CANONIC(f)      MSG_FLAGS((f), MSG_FLG_CANONIC)
#define MSG_IS_EXTRACT_COPY(f) MSG_FLAGS((f), MSG_FLG_EXTRACT_COPY)
#define MSG_IS_COMMA_LISTS(f)  MSG_FLAGS((f), MSG_FLG_COMMA_LISTS)
#define MSG_IS_MAILBOX(f)      MSG_FLAGS((f), MSG_FLG_MAILBOX)

#define MSG_HAS_COMPLETE(f)    MSG_FLAGS((f), MSG_FLG_COMPLETE)
#define MSG_HAS_ERROR(f)       MSG_FLAGS((f), MSG_FLG_ERROR)

#define MSG_IS_COMPLETE(mo) (((mo)->msg_flags & MSG_FLG_COMPLETE) != 0)

SOFIA_END_DECLS

#endif /* MSG_H */

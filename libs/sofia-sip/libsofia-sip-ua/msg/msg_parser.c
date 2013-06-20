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

/**@ingroup msg_parser
 * @CFILE msg_parser.c
 *
 * HTTP-like message parser engine.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Oct  5 14:01:24 2000 ppessi
 *
 */

/*#define NDEBUG*/

#include "config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include <stdarg.h>
#include <sofia-sip/su_tagarg.h>

#include <sofia-sip/su.h>
#include <sofia-sip/su_alloc.h>

#include "msg_internal.h"
#include "sofia-sip/msg_header.h"
#include "sofia-sip/bnf.h"
#include "sofia-sip/msg_parser.h"
#include "sofia-sip/msg_mclass.h"
#include "sofia-sip/msg_mclass_hash.h"
#include "sofia-sip/msg_mime.h"

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "msg_parser";
#endif

static int _msg_header_add_dup_as(msg_t *msg,
				  msg_pub_t *pub,
				  msg_hclass_t *hc,
				  msg_header_t const *src);

static void msg_insert_chain(msg_t *msg, msg_pub_t *pub, int prepend,
			     msg_header_t **head, msg_header_t *h);
static void msg_insert_here_in_chain(msg_t *msg,
				     msg_header_t **prev,
				     msg_header_t *h);
su_inline msg_header_t *msg_chain_remove(msg_t *msg, msg_header_t *h);

#ifndef NDEBUG
static int msg_chain_loop(msg_header_t const *h);
static int msg_chain_errors(msg_header_t const *h);
#endif

/* ====================================================================== */
/* Message properties */

/** Get message flags. */
unsigned msg_get_flags(msg_t const *msg, unsigned mask)
{
  return msg ? msg->m_object->msg_flags & mask : 0;
}

/** Set message flags. */
unsigned msg_set_flags(msg_t *msg, unsigned mask)
{
  return msg ? msg->m_object->msg_flags |= mask : 0;
}

/** Clear message flags. */
unsigned msg_zap_flags(msg_t *msg, unsigned mask)
{
  return msg ? msg->m_object->msg_flags &= ~mask : 0;
}

/** Test if streaming is in progress. */
int msg_is_streaming(msg_t const *msg)
{
  return msg && msg->m_streaming != 0;
}

/** Enable/disable streaming */
void msg_set_streaming(msg_t *msg, enum msg_streaming_status what)
{
  if (msg)
    msg->m_streaming = what != 0;
}

/* ---------------------------------------------------------------------- */

/** Test if header is not in the chain */
#define msg_header_is_removed(h) ((h)->sh_prev == NULL)

su_inline int msg_is_request(msg_header_t const *h)
{
  return h->sh_class->hc_hash == msg_request_hash;
}

su_inline int msg_is_status(msg_header_t const *h)
{
  return h->sh_class->hc_hash == msg_status_hash;
}

/* ====================================================================== */
/* Message buffer management */

/** Allocate a buffer of @a size octets, with slack of #msg_min_size. */
void *msg_buf_alloc(msg_t *msg, usize_t size)
{
  struct msg_mbuffer_s *mb = msg->m_buffer;
  size_t room = mb->mb_size - mb->mb_commit - mb->mb_used;
  size_t target_size;

  if (mb->mb_data && room >= (unsigned)size)
    return mb->mb_data + mb->mb_used + mb->mb_commit;

  target_size =
    msg_min_size * ((size + mb->mb_commit) / msg_min_size + 1) - mb->mb_commit;

  return msg_buf_exact(msg, target_size);
}

/** Allocate a buffer exactly of @a size octets, without any slack. */
void *msg_buf_exact(msg_t *msg, usize_t size)
{
  struct msg_mbuffer_s *mb = msg->m_buffer;
  size_t room = mb->mb_size - mb->mb_commit - mb->mb_used;
  char *buffer;
  int realloc;

  if (mb->mb_data && room >= (unsigned)size)
    return mb->mb_data + mb->mb_used + mb->mb_commit;

  size += mb->mb_commit;

  if (msg->m_maxsize && msg->m_size + size > msg->m_maxsize + 1) {
    msg->m_object->msg_flags |= MSG_FLG_TOOLARGE;
    errno = msg->m_errno = ENOBUFS;
    return NULL;
  }

  realloc = !mb->mb_used && !msg->m_set_buffer;

  if (realloc)
    buffer = su_realloc(msg->m_home, mb->mb_data, size);
  else
    buffer = su_alloc(msg->m_home, size);

  if (!buffer)
    return NULL;

  if (!realloc && mb->mb_commit && mb->mb_data)
    memcpy(buffer, mb->mb_data + mb->mb_used, mb->mb_commit);

  msg->m_set_buffer = 0;

  mb->mb_data = buffer;
  mb->mb_size = size;
  mb->mb_used = 0;

  return buffer + mb->mb_commit;
}

/** Commit data into buffer. */
usize_t msg_buf_commit(msg_t *msg, usize_t size, int eos)
{
  if (msg) {
    struct msg_mbuffer_s *mb = msg->m_buffer;
    assert(mb->mb_used + mb->mb_commit + size <= mb->mb_size);

    mb->mb_commit += size;
    mb->mb_eos = eos;

    if (mb->mb_used == 0 && !msg->m_chunk && !msg->m_set_buffer) {
      size_t slack = mb->mb_size - mb->mb_commit;

      if (eos || slack >= msg_min_size) {
	/* realloc and cut down buffer */
	size_t new_size;
	void *new_data;

	if (eos)
	  new_size = mb->mb_commit + 1;
	else
	  new_size = mb->mb_commit + msg_min_size;

	new_data = su_realloc(msg->m_home, mb->mb_data, new_size);
	if (new_data) {
	  mb->mb_data = new_data, mb->mb_size = new_size;
	}
      }
    }
  }
  return 0;
}

/** Get length of committed data */
usize_t msg_buf_committed(msg_t const *msg)
{
  if (msg)
    return msg->m_buffer->mb_commit;
  else
    return 0;
}

/** Get committed data */
void *msg_buf_committed_data(msg_t const *msg)
{
  return msg && msg->m_buffer->mb_data ?
    msg->m_buffer->mb_data + msg->m_buffer->mb_used
    : NULL;
}

usize_t msg_buf_size(msg_t const *msg)
{
  assert(msg);
  if (msg) {
    struct msg_mbuffer_s const *mb = msg->m_buffer;
    return mb->mb_size - mb->mb_commit - mb->mb_used;
  }
  else
    return 0;
}

su_inline
void msg_buf_used(msg_t *msg, usize_t used)
{
  msg->m_size += used;
  msg->m_buffer->mb_used += used;
  if (msg->m_buffer->mb_commit > used)
    msg->m_buffer->mb_commit -= used;
  else
    msg->m_buffer->mb_commit = 0;
}

/** Set buffer. */
void msg_buf_set(msg_t *msg, void *b, usize_t size)
{
  if (msg) {
    struct msg_mbuffer_s *mb = msg->m_buffer;

    assert(!msg->m_set_buffer);	/* This can be set only once */

    mb->mb_data = b;
    mb->mb_size = size;
    mb->mb_used = 0;
    mb->mb_commit = 0;
    mb->mb_eos  = 0;

    msg->m_set_buffer = 1;
  }
}

/** Move unparsed data from src to dst */
void *msg_buf_move(msg_t *dst, msg_t const *src)
{
  void *retval;
  struct msg_mbuffer_s *db = dst->m_buffer;
  struct msg_mbuffer_s const *sb = src->m_buffer;

  if (!dst || !src)
    return NULL;

  if (sb->mb_eos)
    retval = msg_buf_exact(dst, sb->mb_commit + 1);
  else
    retval = msg_buf_alloc(dst, sb->mb_commit + 1);

  if (retval == NULL)
    return NULL;

  memcpy(retval, sb->mb_data + sb->mb_used, sb->mb_commit);

  db->mb_commit += sb->mb_commit;
  db->mb_eos = sb->mb_eos;

  return retval;
}

/**Obtain I/O vector for receiving the data.
 *
 * @relatesalso msg_s
 *
 * Allocate buffers for receiving @a n bytes
 * of data available from network. Function returns the buffers in the I/O vector
 * @a vec. The @a vec is allocated by the caller, the available length is
 * given as @a veclen. If the protocol is message-oriented like UDP or SCTP
 * and the available data ends at message boundary, the caller should set
 * the @a exact as 1. Otherwise some extra buffer (known as @em slack) is
 * allocated).
 *
 * Currently, the msg_recv_iovec() allocates receive buffers in at most two
 * blocks, so the caller should allocate at least two elements for the I/O
 * vector @a vec.
 *
 * @param[in]  msg     message object
 * @param[out] vec     I/O vector
 * @param[in]  veclen  available length of @a vec
 * @param[in]  n       number of possibly available bytes
 * @param[in]  exact   true if data ends at message boundary
 *
 * @return
 * The length of I/O vector to
 * receive data, 0 if there are not enough buffers, or -1 upon an error.
 *
 * @sa msg_iovec(), su_vrecv()
 */
issize_t msg_recv_iovec(msg_t *msg, msg_iovec_t vec[], isize_t veclen,
			usize_t n, int exact)
{
  size_t i = 0;
  size_t len = 0;
  msg_payload_t *chunk;
  char *buf;

  if (n == 0)
    return 0;

  if (veclen == 0)
    vec = NULL;

  for (chunk = msg->m_chunk; chunk; chunk = MSG_CHUNK_NEXT(chunk)) {
    buf = MSG_CHUNK_BUFFER(chunk);
    len = MSG_CHUNK_AVAIL(chunk);

    if (len == 0)
      continue;
    if (!buf)
      break;

#if SU_HAVE_WINSOCK
    /* WSABUF has u_long */
    if (len > SU_IOVECLEN_MAX)
      len = SU_IOVECLEN_MAX;
#endif
    if (len > n)
      len = n;
    if (vec)
      vec[i].mv_base = buf, vec[i].mv_len = (su_ioveclen_t)len;
    i++;
    if (len == n)
      return i;
    if (i == veclen)
      vec = NULL;
    n -= len;
  }

  if (!chunk && msg->m_chunk && msg_get_flags(msg, MSG_FLG_FRAGS)) {
    /*
     * If the m_chunk is the last fragment for this message,
     * receive rest of the data to the next message
     */
    if (msg->m_next == NULL)
      msg->m_next = msg_create(msg->m_class, msg->m_oflags);
    if (msg->m_next) {
      msg->m_next->m_maxsize = msg->m_maxsize;
      msg_addr_copy(msg->m_next, msg);
    }
    msg = msg->m_next;
    if (msg == NULL)
      return 0;
  }

  if (exact)
    buf = msg_buf_exact(msg, n + 1), len = n;
  else if (chunk && len > n && !msg_get_flags(msg, MSG_FLG_CHUNKING))
    buf = msg_buf_exact(msg, len + 1);
  else
    buf = msg_buf_alloc(msg, n + 1), len = msg_buf_size(msg);

  if (buf == NULL)
    return -1;

  if (vec)
    vec[i].mv_base = buf, vec[i].mv_len = (su_ioveclen_t)n;

  if (chunk) {
    assert(chunk->pl_data == NULL); assert(chunk->pl_common->h_len == 0);

    chunk->pl_common->h_data = chunk->pl_data = buf;

    if (len < MSG_CHUNK_AVAIL(chunk)) {
      msg_header_t *h = (void*)chunk;
      h->sh_succ = msg_header_alloc(msg_home(msg), h->sh_class, 0);
      if (!h->sh_succ)
	return -1;
      h->sh_succ->sh_prev = &h->sh_succ;
      chunk->pl_next = (msg_payload_t *)h->sh_succ;
      chunk->pl_next->pl_len = chunk->pl_len - len;
      chunk->pl_len = len;
    }
    else if (len > MSG_CHUNK_AVAIL(chunk)) {
      len = MSG_CHUNK_AVAIL(chunk);
    }

    msg_buf_used(msg, len);
  }

  return i + 1;

#if 0
  if ((msg->m_ssize || msg->m_stream)
      /* && msg_get_flags(msg, MSG_FLG_BODY) */) {
    /* Streaming */
    msg_buffer_t *b, *b0;

    /* Calculate available size of current buffers */
    for (b = msg->m_stream, len = 0; b && n > len; b = b->b_next)
      len += b->b_avail - b->b_size;

    /* Allocate new buffers */
    if (n > len && msg_buf_external(msg, n, 0) < 0)
      return -1;

    for (b0 = msg->m_stream; b0; b0 = b0->b_next)
      if (b0->b_avail != b0->b_size)
	break;

    for (b = b0; b && n > 0; i++, b = b->b_next) {
      len = b->b_size - b->b_avail;
      len = n < len ? n : len;
      if (vec && i < veclen)
	vec[i].mv_base = b->b_data + b->b_avail, vec[i].mv_len = len;
      else
	vec = NULL;
      n -= len;
    }

    return i + 1;
  }
#endif
}


/** Obtain a buffer for receiving data.
 *
 * @relatesalso msg_s
 */
issize_t msg_recv_buffer(msg_t *msg, void **return_buffer)
{
  void *buffer;

  if (!msg)
    return -1;

  if (return_buffer == NULL)
    return_buffer = &buffer;

  if (msg->m_chunk) {
    msg_payload_t *pl;

    for (pl = msg->m_chunk; pl; pl = pl->pl_next) {
      size_t n = MSG_CHUNK_AVAIL(pl);
      if (n) {
	*return_buffer = MSG_CHUNK_BUFFER(pl);
	return n;
      }
    }

    return 0;
  }

  if (msg_get_flags(msg, MSG_FLG_FRAGS)) {
    /* Message is complete */
    return 0;
  }
  else if ((*return_buffer = msg_buf_alloc(msg, 2))) {
    return msg_buf_size(msg) - 1;
  }
  else {
    return -1;
  }
}



/**Commit @a n bytes of buffers.
 *
 * @relatesalso msg_s
 *
 * The function msg_recv_commit() is called after @a n bytes of data has
 * been received to the message buffers and the parser can extract the
 * received data.
 *
 * @param msg pointer to message object
 * @param n   number of bytes received
 * @param eos true if stream is complete
 *
 * @note The @a eos should be always true for message-based transports. It
 * should also be true when a stram oin stream-based transport ends, for
 * instance, when TCP FIN is received.
 *
 * @retval 0 when successful
 * @retval -1 upon an error.
 */
isize_t msg_recv_commit(msg_t *msg, usize_t n, int eos)
{
  msg_payload_t *pl;

  if (eos)
    msg->m_buffer->mb_eos = 1;

  for (pl = msg->m_chunk; pl; pl = pl->pl_next) {
    size_t len = MSG_CHUNK_AVAIL(pl);

    if (n <= len)
      len = n;

    pl->pl_common->h_len += len;

    n -= len;

    if (n == 0)
      return 0;
  }

  if (msg->m_chunk && msg->m_next)
    msg = msg->m_next;

  return msg_buf_commit(msg, n, eos);
}

/**Get a next message of the stream.
 *
 * @relatesalso msg_s
 *
 * When parsing a transport stream, only the first message in the stream is
 * created with msg_create(). The rest of the messages should be created
 * with msg_next() after previous message has been completely received and
 * parsed.
 *
 */
msg_t *msg_next(msg_t *msg)
{
  msg_t *next;
  usize_t n;

  if (msg && msg->m_next) {
    next = msg->m_next;
    msg->m_next = NULL;
    return next;
  }

  if ((n = msg_buf_committed(msg))) {
    if (msg_buf_move(next = msg_create(msg->m_class, msg->m_oflags), msg)) {
      msg_addr_copy(next, msg);
      return next;
    }
    /* How to indicate error? */
    msg_destroy(next);
  }

  return NULL;
}

/** Set next message of the stream.
 *
 * @relatesalso msg_s
 */
int msg_set_next(msg_t *msg, msg_t *next)
{
  if (!msg || (next && next->m_next))
    return -1;

  if (msg->m_next && next)
    next->m_next = msg->m_next;

  msg->m_next = next;

  return 0;
}

/** Clear committed data.
 *
 * @relatesalso msg_s
 */
void msg_clear_committed(msg_t *msg)
{
  if (msg) {
    usize_t n = msg_buf_committed(msg);

    if (n)
      msg_buf_used(msg, n);
  }
}

#if 0
struct sigcomp_udvm;

struct sigcomp_udvm *msg_get_udvm(msg_t *msg);
struct sigcomp_udvm *msg_set_udvm(msg_t *msg, struct sigcomp_udvm *);

/** Save UDVM. */
struct sigcomp_udvm *msg_set_udvm(msg_t *msg, struct sigcomp_udvm *udvm)
{
  struct sigcomp_udvm *prev = NULL;

  if (msg) {
    prev = msg->m_udvm;
    msg->m_udvm = udvm;
  }

  return prev;
}

/** Get saved UDVM */
struct sigcomp_udvm *msg_get_udvm(msg_t *msg)
{
  return msg ? msg->m_udvm : NULL;
}

#endif

/** Mark message as complete.
 *
 * @relatesalso msg_s
 */
unsigned msg_mark_as_complete(msg_t *msg, unsigned mask)
{
  if (msg) {
    msg->m_streaming = 0;
    return msg->m_object->msg_flags |= mask | MSG_FLG_COMPLETE;
  }
  else {
    return 0;
  }
}

/** Return true if message is complete.
 *
 * @relatesalso msg_s
 */
int msg_is_complete(msg_t const *msg)
{
  return msg && MSG_IS_COMPLETE(msg->m_object);
}

/** Return true if message has parsing errors.
 *
 * @relatesalso msg_s
*/
int msg_has_error(msg_t const *msg)
{
  return msg->m_object->msg_flags & MSG_FLG_ERROR;
}

/**Total size of message.
 *
 * @relatesalso msg_s
 */
usize_t msg_size(msg_t const *msg)
{
  return msg ? msg->m_size : 0;
}

/** Set the maximum size of a message.
 *
 * @relatesalso msg_s
 *
 * The function msg_maxsize() sets the maximum buffer size of a message. It
 * returns the previous maximum size. If the @a maxsize is 0, maximum size
 * is not set, but the current maximum size is returned.
 *
 * If the message size exceeds maxsize, msg_errno() returns ENOBUFS,
 * MSG_FLG_TOOLARGE and MSG_FLG_ERROR flags are set.
 */
usize_t msg_maxsize(msg_t *msg, usize_t maxsize)
{
  usize_t retval = 0;

  if (msg) {
    retval = msg->m_maxsize;
    if (maxsize)
      msg->m_maxsize = maxsize;
  }

  return retval;
}

/**Set the size of next fragment.
 *
 * @relatesalso msg_s
 *
 * The function msg_streaming_size() sets the size of the message body for
 * streaming.
 */
int msg_streaming_size(msg_t *msg, usize_t ssize)
{
  if (!msg)
    return -1;

  msg->m_ssize = ssize;

  return 0;
}

/**Allocate a list of external buffers.
 *
 * @relatesalso msg_s
 *
 * The function msg_buf_external() allocates at most msg_n_fragments
 * external buffers for the message body.
 *
 * @return The function msg_buf_external() returns number of allocated
 * buffers, or -1 upon an error.
 */
issize_t msg_buf_external(msg_t *msg,
			  usize_t N,
			  usize_t blocksize)
{
  msg_buffer_t *ext, *b, **bb;
  size_t i, I;

  assert(N <= 128 * 1024);

  if (msg == NULL)
    return -1;
  if (blocksize == 0)
    blocksize = msg_min_block;
  if (N == 0)
    N = blocksize;
  if (N > blocksize * msg_n_fragments)
    N = blocksize * msg_n_fragments;
  if (N > msg->m_ssize)
    N = msg->m_ssize;

  I = (N + blocksize - 1) / blocksize; assert(I <= msg_n_fragments);

  for (i = 0, bb = &ext; i < I; i++) {
    *bb = su_zalloc(msg_home(msg), sizeof **bb);
    if (!*bb)
      break;
    bb = &(*bb)->b_next;
  }

  if (i == I)
    for (b = ext, i = 0; b; b = b->b_next, i++) {
      b->b_data = su_alloc(msg_home(msg), b->b_size = blocksize);
      if (!b->b_data)
	break;
    }

  if (i == I) {
    /* Successful return */
    for (bb = &msg->m_stream; *bb; bb = &(*bb)->b_next)
      ;

    *bb = ext;

    if (msg->m_ssize != MSG_SSIZE_MAX)
      for (b = ext; b; b = b->b_next) {
	if (msg->m_ssize < b->b_size) {
	  b->b_size = msg->m_ssize;
	}
	msg->m_ssize -= b->b_size;
      }

    return i;
  }

  for (b = ext; b; b = ext) {
    ext = b->b_next;
    su_free(msg_home(msg), b->b_data);
    su_free(msg_home(msg), b);
  }

  return -1;
}

int msg_unref_external(msg_t *msg, msg_buffer_t *b)
{
  if (msg && b) {
    su_free(msg_home(msg), b->b_data);
    su_free(msg_home(msg), b);
    return 0;
  }
  errno = EINVAL;
  return -1;
}

/* ====================================================================== */
/* Parsing messages */

su_inline int extract_incomplete_chunks(msg_t *, int eos);
static issize_t extract_first(msg_t *, msg_pub_t *,
			    char b[], isize_t bsiz, int eos);
su_inline issize_t extract_next(msg_t *, msg_pub_t *, char *, isize_t bsiz,
				  int eos, int copy);
static issize_t extract_header(msg_t *, msg_pub_t*,
			     char b[], isize_t bsiz, int eos, int copy);
static msg_header_t *header_parse(msg_t *, msg_pub_t *, msg_href_t const *,
				  char s[], isize_t slen, int copy_buffer);
static msg_header_t *error_header_parse(msg_t *msg, msg_pub_t *mo,
					msg_href_t const *hr);
su_inline issize_t
extract_trailers(msg_t *msg, msg_pub_t *mo,
		 char *b, isize_t bsiz, int eos, int copy);

/** Calculate length of line ending (0, 1 or 2). @internal */
#define CRLF_TEST(b) ((b)[0] == '\r' ? ((b)[1] == '\n') + 1 : (b)[0] =='\n')

su_inline void
append_parsed(msg_t *msg, msg_pub_t *mo, msg_href_t const *hr, msg_header_t *h,
	      int always_into_chain);

/**Extract and parse a message from internal buffer.
 *
 * @relatesalso msg_s
 *
 * This function parses the internal buffer and adds the parsed fragments to
 * the message object. It marks the successfully parsed data as extracted.
 *
 * @param msg message to be parsed
 *
 * @retval positive if a complete message was parsed
 * @retval 0 if message was incomplete
 * @retval negative if an error occurred
 */
int msg_extract(msg_t *msg)
{
  msg_pub_t *mo = msg_object(msg);
  msg_mclass_t const *mc;
  char *b;
  ssize_t m;
  size_t bsiz;
  int eos;

  if (!msg || !msg->m_buffer->mb_data)
    return -1;

  assert(mo);

  mc = msg->m_class;
  mo = msg->m_object;
  eos = msg->m_buffer->mb_eos;

  if (msg->m_chunk) {
    int incomplete = extract_incomplete_chunks(msg, eos);
    if (incomplete < 1 || MSG_IS_COMPLETE(mo))
      return incomplete;
  }

  if (mo->msg_flags & MSG_FLG_TRAILERS)
    msg_set_streaming(msg, (enum msg_streaming_status)0);

  if (msg->m_buffer->mb_used + msg->m_buffer->mb_commit ==
      msg->m_buffer->mb_size)
    /* Why? When? */
    return 0;

  assert(msg->m_buffer->mb_used + msg->m_buffer->mb_commit <
	 msg->m_buffer->mb_size);

  m = 0;

  b = msg->m_buffer->mb_data + msg->m_buffer->mb_used;
  bsiz = msg->m_buffer->mb_commit;
  b[bsiz] = '\0';

  while (msg->m_buffer->mb_commit > 0) {
    int flags = mo->msg_flags;
    int copy = MSG_IS_EXTRACT_COPY(flags);

    if (flags & MSG_FLG_COMPLETE)
      break;

    if (flags & MSG_FLG_TRAILERS)
      m = extract_trailers(msg, mo, b, bsiz, eos, copy);
    else if (flags & MSG_FLG_BODY)
      m = mc->mc_extract_body(msg, mo, b, bsiz, eos);
    else if (flags & MSG_FLG_HEADERS)
      m = extract_next(msg, mo, b, bsiz, eos, copy);
    else
      m = extract_first(msg, mo, b, bsiz, eos);

    if (m <= 0 || msg->m_chunk)
      break;

    b += m;
    bsiz -= m;

    msg_buf_used(msg, (size_t)m);
  }

  if (eos && bsiz == 0)
    msg_mark_as_complete(msg, 0);

  if (m < 0 || (mo->msg_flags & MSG_FLG_ERROR)) {
    msg_mark_as_complete(msg, MSG_FLG_ERROR);
    return -1;
  }
  else if (!MSG_IS_COMPLETE(mo))
    return 0;
  else if (!(mo->msg_flags & MSG_FLG_HEADERS)) {
    msg_mark_as_complete(msg, MSG_FLG_ERROR);
    return -1;
  }
  else
    return 1;
}

static
issize_t extract_first(msg_t *msg, msg_pub_t *mo, char b[], isize_t bsiz, int eos)
{
  /* First line */
  size_t k, l, m, n, xtra;
  int crlf;
  msg_header_t *h;
  msg_href_t const *hr;
  msg_mclass_t const *mc = msg->m_class;

  for (k = 0; IS_LWS(b[k]); k++) /* Skip whitespace */
    ;
  if (!b[k]) return k;

  /* If first token contains no /, this is request, otherwise status line */
  l = span_token(b + k) + k;
  if (b[l] != '/')
    hr = mc->mc_request;
  else
    hr = mc->mc_status;

  n = span_non_crlf(b + l) + l;
  if (!b[n])
    return eos ? -1 : 0;
  crlf = CRLF_TEST(b + n);

  for (m = n + crlf; IS_WS(b[m]); m++)
    ;
  /* In order to skip possible whitespace after first line, we don't parse
     first line until first non-ws char from next one has been received */
  if (!b[m] && !eos)
    return 0;

  xtra = MSG_IS_EXTRACT_COPY(mo->msg_flags) ? n + 1 - k : 0;
  if (!(h = msg_header_alloc(msg_home(msg), hr->hr_class, xtra)))
    return -1;

  if (xtra) {
    char *bb = memcpy(MSG_HEADER_DATA(h), b, xtra - 1);
    h->sh_data = b, h->sh_len = n + crlf;
    b = bb; n = xtra - 1;
  }
  else {
    b = b + k; n = n - k;
  }

  b[n] = 0;

  if (hr->hr_class->hc_parse(msg_home(msg), h, b, n) < 0)
    return -1;

  assert(hr->hr_offset);

  append_parsed(msg, mo, hr, h, 1);

  mo->msg_flags |= MSG_FLG_HEADERS;

  return m;
}

/* Extract header or message body */
su_inline issize_t
extract_next(msg_t *msg, msg_pub_t *mo, char *b, isize_t bsiz,
	     int eos, int copy)
{
  if (IS_CRLF(b[0]))
    return msg->m_class->mc_extract_body(msg, mo, b, bsiz, eos);
  else
    return extract_header(msg, mo, b, bsiz, eos, copy);
}

/** Extract a header. */
issize_t msg_extract_header(msg_t *msg, msg_pub_t *mo,
			    char b[], isize_t bsiz, int eos)
{
  return extract_header(msg, mo, b, bsiz, eos, 0);
}

/** Extract a header from buffer @a b.
 */
static
issize_t
extract_header(msg_t *msg, msg_pub_t *mo, char *b, isize_t bsiz, int eos,
	       int copy_buffer)
{
  size_t len, m;
  size_t name_len = 0, xtra;
  isize_t n = 0;
  int crlf = 0, name_len_set = 0;
  int error = 0;
  msg_header_t *h;
  msg_href_t const *hr;
  msg_mclass_t const *mc = msg->m_class;

  hr = msg_find_hclass(mc, b, &n); /* Get header name */
  error = n == 0;
  if (hr == NULL)		/* Panic */
    return -1;

  xtra = span_ws(b + n);

  /* Find next crlf which is not followed by whitespace */
  do {
    n += xtra + crlf;
    if (!eos && bsiz == n)
      return 0;
    m = span_non_crlf(b + n);
    if (!name_len_set && m)
      name_len = n, name_len_set = 1; /* First non-ws after COLON */
    n += m;
    crlf = CRLF_TEST(b + n);
    xtra = span_ws(b + n + crlf);
  }
  while (xtra);

  if (!eos && bsiz == n + crlf)
    return 0;

  if (hr->hr_class->hc_hash == msg_unknown_hash)
    name_len = 0, name_len_set = 1;

  if (error) {
    msg->m_extract_err |= hr->hr_flags;
    if (hr->hr_class->hc_critical)
      mo->msg_flags |= MSG_FLG_ERROR;
    hr = mc->mc_error;
    copy_buffer = 1;
    h = error_header_parse(msg, mo, hr);
  }
  else {
    if (!name_len_set)
      /* Empty header - nothing but name, COLON and LWS */
      name_len = n;
    else
      /* Strip extra whitespace at the end of header */
      while (n > name_len && IS_LWS(b[n - 1]))
	n--, crlf++;

    h = header_parse(msg, mo, hr, b + name_len, n - name_len, copy_buffer);
  }

  if (h == NULL)
    return -1;

  len = n + crlf;

  /*
   * If the header contains multiple header fields, set the pointer to the
   * encodeded data correctly
   */
  while (h) {
    if (copy_buffer)
      h->sh_data = b, h->sh_len = len;
    b += len, len = 0;
    if (h->sh_succ)
      assert(&h->sh_succ == h->sh_succ->sh_prev);
    h = h->sh_next;
  }

  return n + crlf;
}

static
msg_header_t *header_parse(msg_t *msg, msg_pub_t *mo,
			   msg_href_t const *hr,
			   char s[], isize_t slen,
			   int copy_buffer)
{
  su_home_t *home = msg_home(msg);
  msg_header_t *h, **hh;
  msg_hclass_t *hc = hr->hr_class;
  int n;
  int add_to_list, clear = 0;

  hh = (msg_header_t **)((char *)mo + hr->hr_offset);

  add_to_list = (hc->hc_kind == msg_kind_list && !copy_buffer && *hh);

  if (add_to_list)
    h = *hh;
  else
    h = msg_header_alloc(home, hc, copy_buffer ? slen + 1 : 0);

  if (!h)
    return NULL;

  if (copy_buffer)
    s = memcpy(MSG_HEADER_DATA(h), s, slen);

  s[slen] = '\0';

  if (hc->hc_kind == msg_kind_list && *hh) {
    n = hc->hc_parse(home, *hh, s, slen);
    /* Clear if adding new header disturbs existing headers */
    clear = *hh != h && !copy_buffer;
    if (clear)
      msg_fragment_clear((*hh)->sh_common);
  }
  else
    n = hc->hc_parse(home, h, s, slen);

  if (n < 0) {
    msg->m_extract_err |= hr->hr_flags;

    if (hc->hc_critical)
      mo->msg_flags |= MSG_FLG_ERROR;

    clear = 0;

    if (!add_to_list) {
      /* XXX - This should be done by msg_header_free_all() */
      msg_header_t *h_next;
      msg_param_t *h_params;
      msg_error_t *er;

      while (h) {
	h_next = h->sh_next;
	if (hc->hc_params) {
	  h_params = *(msg_param_t **)((char *)h + hc->hc_params);
	  if (h_params)
	    su_free(home, h_params);
	}
	su_free(home, h);
	h = h_next;
      }
      /* XXX - This should be done by msg_header_free_all() */
      hr = msg->m_class->mc_error;
      h = msg_header_alloc(home, hr->hr_class, 0);
      er = (msg_error_t *)h;

      if (!er)
	return NULL;

      er->er_name = hc->hc_name;
      hh = (msg_header_t **)((char *)mo + hr->hr_offset);
    }
  }

  if (clear)
    for (hh = &(*hh)->sh_next; *hh; *hh = (*hh)->sh_next)
      msg_chain_remove(msg, *hh);
  else if (h != *hh)
    append_parsed(msg, mo, hr, h, 0);

  return h;
}

static
msg_header_t *error_header_parse(msg_t *msg, msg_pub_t *mo,
				 msg_href_t const *hr)
{
  msg_header_t *h;

  h = msg_header_alloc(msg_home(msg), hr->hr_class, 0);
  if (h)
    append_parsed(msg, mo, hr, h, 0);

  return h;
}


/** Complete this header field and parse next header field.
 *
 * This function completes parsing a multi-field header like @Accept,
 * @Contact, @Via or @Warning. It scans for the next header field and
 * if one is found, it calls the parsing function recursively.
 *
 * @param home 	 memory home used ot allocate
 *             	 new header structures and parameter lists
 * @param prev 	 pointer to header structure already parsed
 * @param s    	 header content to parse; should point to the area after
 *             	 current header field (either end of line or to a comma
 *             	 separating header fields)
 * @param slen 	 ignored
 *
 * @since New in @VERSION_1_12_4.
 *
 * @retval >= 0 when successful
 * @retval -1 upon an error
 */
issize_t msg_parse_next_field(su_home_t *home, msg_header_t *prev,
			      char *s, isize_t slen)
{
  msg_hclass_t *hc = prev->sh_class;
  msg_header_t *h;
  char *end = s + slen;

  if (*s && *s != ',')
    return -1;

  if (msg_header_update_params(prev->sh_common, 0) < 0)
    return -1;

  while (*s == ',') /* Skip comma and following whitespace */
    *s = '\0', s += span_lws(s + 1) + 1;

  if (*s == 0)
    return 0;

  h = msg_header_alloc(home, hc, 0);
  if (!h)
    return -1;

  prev->sh_succ = h, h->sh_prev = &prev->sh_succ;
  prev->sh_next = h;

  return hc->hc_parse(home, h, s, end - s);
}


/** Decode a message header. */
msg_header_t *msg_header_d(su_home_t *home, msg_t const *msg, char const *b)
{
  msg_mclass_t const *mc = msg->m_class;
  msg_href_t const *hr = mc->mc_unknown;
  isize_t n;			/* Length of header contents */
  isize_t name_len, xtra;
  msg_header_t *h;
  char *bb;

  n = strlen(b);
  hr = msg_find_hclass(mc, b, &name_len);
  if (hr == NULL)
    return NULL;

  /* Strip extra whitespace at the end and begin of header */
  while (n > name_len && IS_LWS(b[n - 1]))
    n--;
  if (name_len < n && IS_LWS(b[name_len]))
    name_len++;

  xtra = (n - name_len);
  if (!(h = msg_header_alloc(home, hr->hr_class, xtra + 1)))
    return NULL;

  bb = memcpy(MSG_HEADER_DATA(h), b + name_len, xtra), bb[xtra] = 0;

  if (hr->hr_class->hc_parse(home, h, bb, xtra) >= 0)
    return h;

  hr = mc->mc_unknown;
  su_free(home, h);
  if (!(h = msg_header_alloc(home, hr->hr_class, n + 1)))
    return NULL;
  bb = memcpy(MSG_HEADER_DATA(h), b, n), bb[n] = 0;
  if (hr->hr_class->hc_parse(home, h, bb, n) < 0)
    su_free(home, h), h = NULL;

  return h;
}

/** Extract a separator line */
issize_t msg_extract_separator(msg_t *msg, msg_pub_t *mo,
			       char b[], isize_t bsiz, int eos)
{
  msg_mclass_t const *mc = msg->m_class;
  msg_href_t const *hr = mc->mc_separator;
  int l = CRLF_TEST(b);  /* Separator length */
  msg_header_t *h;

  /* Even if a single CR *may* be a payload separator we cannot be sure */
  if (l == 0 || (!eos && bsiz == 1 && b[0] == '\r'))
    return 0;

  /* Separator */
  if (!(h = msg_header_alloc(msg_home(msg), hr->hr_class, 0)))
    return -1;
  if (hr->hr_class->hc_parse(msg_home(msg), h, b, l) < 0)
    return -1;

  h->sh_data = b, h->sh_len = l;

  append_parsed(msg, mo, hr, h, 0);

  return l;
}

su_inline msg_header_t **msg_chain_tail(msg_t const *msg);

/** Extract a message body of @a body_len bytes.
  */
issize_t msg_extract_payload(msg_t *msg, msg_pub_t *mo,
			     msg_header_t **return_payload,
			     usize_t body_len,
			     char b[], isize_t bsiz,
			     int eos)
{
  msg_mclass_t const *mc;
  msg_href_t const *hr;
  msg_header_t *h, *h0;
  msg_payload_t *pl;
  char *x;

  if (msg == NULL || mo == NULL)
    return -1;

  assert(!msg->m_chunk);
  mc = msg->m_class;
  hr = mc->mc_payload;

  if (return_payload == NULL)
    return_payload = &h0;
  *return_payload = NULL;

  assert(body_len > 0);

  /* Allocate header structure for payload */
  if (!(h = msg_header_alloc(msg_home(msg), hr->hr_class, 0)))
    return -1;

  append_parsed(msg, mo, hr, h, 0);
  pl = (msg_payload_t*)h;
  *return_payload = h;

  if (bsiz >= body_len) {
    /* We have a complete body. */
    h->sh_data = b, h->sh_len = body_len;
    pl->pl_data = b, pl->pl_len = body_len;
    return body_len;
  }

  if (msg->m_maxsize != 0 && body_len > msg->m_maxsize) {
    mo->msg_flags |= MSG_FLG_TOOLARGE;
    return -1;
  }

  assert(msg->m_buffer->mb_commit == bsiz);
  assert(b == msg->m_buffer->mb_data + msg->m_buffer->mb_used);

  if (msg->m_buffer->mb_used + body_len <= msg->m_buffer->mb_size) {
    /* We don't have a complete body, but we have big enough buffer for it. */
    msg->m_chunk = pl;

    h->sh_data = b, h->sh_len  = bsiz;
    pl->pl_data = b, pl->pl_len  = body_len;

    if (msg->m_buffer->mb_used + body_len < msg->m_buffer->mb_size)
      /* NUL-terminate payload */
      b[body_len++] = '\0';

    /* Mark the rest of the body as used in the buffer */
    /* msg_buf_commit(msg, body_len - bsiz, eos); */
    msg_buf_used(msg, body_len);

    return bsiz;
  }

  /* We don't have big enough buffer for body. */

  if (msg_get_flags(msg, MSG_FLG_CHUNKING)) {
    /* Application supports chunking, use multiple chunks for payload */
    usize_t current, rest;

    current = msg->m_buffer->mb_size - msg->m_buffer->mb_used;
    rest = body_len - current;

    /* Use all the data from our current buffer */
    msg_buf_used(msg, current);

    msg->m_chunk = pl;

    h->sh_data = b, h->sh_len = bsiz;
    pl->pl_data = b, pl->pl_len  = current;

    for (;current < body_len; current += rest) {
      msg_header_t *h0 = h;

      /* Allocate header structure for next payload chunk */
      if (!(h = msg_header_alloc(msg_home(msg), hr->hr_class, 0)))
	return -1;
      if (msg->m_chain)
	msg_insert_here_in_chain(msg, msg_chain_tail(msg), h);
      h0->sh_next = h;

      rest = body_len - current;

      if (!msg->m_streaming) {
	x = msg_buf_exact(msg, rest);
	if (x == NULL) {
	  mo->msg_flags |= MSG_FLG_TOOLARGE;
	  return -1;
	}
      }
      else {
	x = NULL;
      }

      if (x) {
	/* Mark the just-allocated buffer as used */
	rest = msg->m_buffer->mb_size - msg->m_buffer->mb_used;
	msg_buf_used(msg, rest);
      }

      pl = h->sh_payload;

      h->sh_len = 0, pl->pl_len = rest;
      h->sh_data = x, pl->pl_data = x;
    }
  }
  else {
    /* No chunking.
     *
     * Allocate a single buffer that contains enough free space for body.
     *
     * msg_buf_exact() also copies committed but un-used data
     * from the old buffer (b[0] .. b[bsiz])
     * to the new buffer (x[-bsiz-1]..b[-1])
     */
    if (!(x = msg_buf_exact(msg, body_len - bsiz + 1))) {
      if (mo->msg_flags & MSG_FLG_TOOLARGE) {
	msg_mark_as_complete(msg, MSG_FLG_TRUNC);
	return bsiz;
      }
      return -1;
    }

    /* Fake un-received data as already received and then use it */
    /* msg_buf_commit(msg, body_len - bsiz + 1, eos); */
    msg_buf_used(msg, body_len + 1);

    msg->m_chunk = h->sh_payload;

    x -= bsiz; /* Start of un-used data */
    x[body_len] = '\0';

    h->sh_data = x, h->sh_len = bsiz;
    pl->pl_data = x, pl->pl_len = body_len;

    assert(MSG_CHUNK_AVAIL(pl) == body_len - bsiz);
  }

  return bsiz;
}

/** Extract incomplete chunks.
 */
su_inline
int extract_incomplete_chunks(msg_t *msg, int eos)
{
  msg_payload_t *chunk;

  for (chunk = msg->m_chunk; chunk; chunk = MSG_CHUNK_NEXT(chunk)) {
    if (MSG_CHUNK_AVAIL(chunk) != 0)
      break;

    /* The incomplete payload fragment is now complete */
    assert(MSG_CHUNK_BUFFER(chunk) == chunk->pl_data + chunk->pl_len);

    msg->m_size += chunk->pl_common->h_len;
  }

  msg->m_chunk = chunk;

  if (chunk) {
    if (eos) {
      msg_mark_as_complete(msg, MSG_FLG_TRUNC);
      return 1;
    }
  }
  else {
    if (msg_get_flags(msg, MSG_FLG_FRAGS))
      msg_mark_as_complete(msg, 0);
  }

  /**@retval 1 when message is complete
   * @retval 0 when message is incomplete
   * @retval -1 upon an error
   */
  return chunk == NULL;
}

/* Extract trailers */
su_inline issize_t
extract_trailers(msg_t *msg, msg_pub_t *mo,
		 char *b, isize_t bsiz, int eos, int copy)
{
  if (IS_CRLF(b[0])) {
    msg_mark_as_complete(msg, MSG_FLG_COMPLETE);
    return CRLF_TEST(b);
  }
  else
    return extract_header(msg, mo, b, bsiz, eos, copy);
}

/* ====================================================================== */
/* Preparing (printing/encoding) a message structure for sending */

/* Internal prototypes */
su_inline size_t
msg_header_name_e(char b[], size_t bsiz, msg_header_t const *h, int flags);
static size_t msg_header_prepare(msg_mclass_t const *, int flags,
				 msg_header_t *h, msg_header_t **return_next,
				 char *b, size_t bsiz);

/**Encode all message fragments.
 *
 * @relatesalso msg_s
 *
 * The function msg_prepare() prepares a message for sending. It encodes all
 * serialized fragments in the message. You have to call msg_serialize()
 * before calling msg_headers_prepare() in order to make sure that all the
 * heades and other message fragments are included in the chain.
 *
 * After encoding, the msg_common_s::h_data field will point to the encoding
 * result of size msg_common_s::h_len bytes in in each fragment.
 *
 * When multiple header fields are represented as a comma-separated list
 * within a single header line, the first fragment in the header will
 * contain all the text belonging to the header. The rest of the header
 * fields will have zero-length encoding with msg_common_s::h_data that
 * points to the end of the line.
 *
 * @return Total size of the encoded message in bytes, or -1 upon an error.
 *
 * @sa msg_extract(), msg_serialize()
 */
int msg_prepare(msg_t *msg)
{
  int total;

  assert(msg->m_chain);
  assert(msg_chain_errors(msg->m_chain) == 0);

  /* Get rid of data that was received but not yet used (parsed) */
  msg_clear_committed(msg);

  total = msg_headers_prepare(msg, msg->m_chain, msg_object(msg)->msg_flags);

  if (total != -1) {
    msg->m_size = total;
    msg->m_prepared = 1;
  }

  return total;
}

/** Clear 'prepared' flag. */
void msg_unprepare(msg_t *msg)
{
  if (msg) msg->m_prepared = 0;
}

/** Return true if message is prepared. */
int msg_is_prepared(msg_t const *msg)
{
  return msg && msg->m_prepared;
}

/**Encode headers in chain.
 *
 * The function msg_headers_prepare() encodes all the headers in the header
 * chain. You have to call msg_serialize() before calling
 * msg_headers_prepare() in order to make sure that all the heades and other
 * message fragments are included in the chain.
 *
 * @return
 * The size of all the headers in chain, or -1 upon an error.
 */
issize_t msg_headers_prepare(msg_t *msg, msg_header_t *headers, int flags)
{
  msg_mclass_t const *mc = msg->m_class;
  msg_header_t *h, *next;
  ssize_t n = 0;
  size_t bsiz = 0, used = 0;
  char *b;
  size_t total = 0;

  b = msg_buf_alloc(msg, msg_min_size);
  bsiz = msg_buf_size(msg);

  if (!b)
    return -1;

  for (h = headers; h;) {

    if (h->sh_data) {
      total += h->sh_len;
      h = h->sh_succ;
      continue;
    }

    for (next = h->sh_succ; next; next = next->sh_succ)
      if (next->sh_class != h->sh_class || next->sh_data)
	break;

    n = msg_header_prepare(mc, flags, h, &next, b, bsiz - used);

    if (n == (ssize_t)-1) {
      errno = EINVAL;
      return -1;
    }

    if (used + n >= bsiz) {
      /* Allocate next buffer */
      if ((b = msg_buf_alloc(msg, n + 1)) == NULL)
	return -1;
      bsiz = msg_buf_size(msg); used = 0;
      continue;
    }

    h->sh_data = b, h->sh_len = n;

    for (h = h->sh_succ; h != next; h = h->sh_succ)
      h->sh_data = b + n, h->sh_len = 0;

    msg_buf_used(msg, n);

    total += n;
    used += n;
    b += n;
  }

  return total;
}

/** Encode a header or a list of headers */
static
size_t msg_header_prepare(msg_mclass_t const *mc, int flags,
			  msg_header_t *h, msg_header_t **return_next,
			  char *b, size_t bsiz)
{
  msg_header_t *h0, *next;
  msg_hclass_t *hc;
  char const *s;
  size_t n; ssize_t m;
  int compact, one_line_list, comma_list;

  assert(h); assert(h->sh_class);

  hc = h->sh_class;
  compact = MSG_IS_COMPACT(flags);
  one_line_list = hc->hc_kind == msg_kind_apndlist;
  comma_list = compact || one_line_list || MSG_IS_COMMA_LISTS(flags);

  for (h0 = h, n = 0; ; h = next) {
    next = h->sh_succ;

    if (h == h0 && hc->hc_name && hc->hc_name[0])
      n += msg_header_name_e(b + n, bsiz >= n ? bsiz - n : 0, h, flags);

    if ((m = hc->hc_print(b + n, bsiz >= n ? bsiz - n : 0, h, flags)) == -1) {
      if (bsiz >= n + 64)
	m = 2 * (bsiz - n);
      else
	m = 128;
    }

    n += m;

    if (hc->hc_name) {
      if (!comma_list || !next || next == *return_next)
	s = CRLF, m = 2;
      /* Else encode continuation */
      else if (compact)
	s = ",", m = 1;
      else if (one_line_list)
	s = ", ", m = 2;
      else
	s = "," CRLF "\t", m = 4;

      if (bsiz > n + m)
	memcpy(b + n, s, m);
      n += m;
    }

    if (!comma_list || !next || next == *return_next)
      break;
  }

  *return_next = next;

  return n;
}

/** Encode a header.
 *
 * The function msg_header_e() encodes a header field in the buffer @a
 * b[]. The encoding includes its name and trailing CRLF.  The function
 * returns the length of the encoding in bytes, excluding the final @c NUL.
 * The buffer @a b must be large enough for whole encoding, including the
 * final @c NUL.
 *
 * The @a flags parameter define how the encoding is done.  If the flags
 * specify @c MSG_DO_COMPACT, the encoding is compact (short form with
 * minimal whitespace).
 */
issize_t msg_header_e(char b[], isize_t bsiz, msg_header_t const *h, int flags)
{
  size_t n, m;

  assert(h); assert(h->sh_class);

  if (h == NULL || h->sh_class == NULL)
    return -1;

  n = msg_header_name_e(b, bsiz, h, flags);
  m = h->sh_class->hc_print(b + n, bsiz > n ? bsiz - n : 0, h, flags);
  if (h->sh_class->hc_name) {
    /* Ordinary header */
    if (bsiz > n + m + strlen(CRLF))
      strcpy(b + n + m, CRLF);
    return n + m + strlen(CRLF);
  }
  else
    return m;
}

/** Encode header name */
su_inline
size_t
msg_header_name_e(char b[], size_t bsiz, msg_header_t const *h, int flags)
{
  int compact = MSG_IS_COMPACT(flags);
  char const *name;
  size_t n, n2;

  if (compact && h->sh_class->hc_short[0])
    name = h->sh_class->hc_short, n = 1;
  else
    name = h->sh_class->hc_name, n = h->sh_class->hc_len;

  if (!name || !name[0])
    return 0;

  n2 = compact ? n + 1 : n + 2;

  if (n2 < bsiz) {
    memcpy(b, name, n);
    b[n++] = ':';
    if (!compact)
      b[n++] = ' ';
    b[n++] = '\0';
  }

  return n2;
}

/** Convert a message to a string.
 *
 * A message is encoded and the encoding result is returned as a string.
 * Because the message may contain binary payload (or NUL in headers), the
 * message length is returned separately in @a *return_len, too.
 *
 * Note that the message is serialized as a side effect.
 *
 * @param home memory home used to allocate the string
 * @param msg  message to encode
 * @param pub  message object to encode (may be NULL)
 * @param flags flags used when encoding
 * @param return_len return-value parameter for encoded message length
 *
 * @return Encoding result as a C string.
 *
 * @since New in @VERSION_1_12_4
 *
 * @sa msg_make(), msg_prepare(), msg_serialize().
 */
char *msg_as_string(su_home_t *home, msg_t *msg, msg_pub_t *pub, int flags,
		    size_t *return_len)
{
  msg_mclass_t const *mc = msg->m_class;
  msg_header_t *h, *next;
  ssize_t n = 0;
  size_t bsiz = 0, used = 0;
  char *b, *b2;

  if (pub == NULL)
    pub = msg->m_object;

  if (msg_serialize(msg, pub) < 0)
    return NULL;

  if (return_len == NULL)
    return_len = &used;

  b = su_alloc(home, bsiz = msg_min_size);

  if (!b)
    return NULL;

  if (pub == msg->m_object)
    h = msg->m_chain;
  else
    h = pub->msg_common->h_succ;

  while (h) {
    for (next = h->sh_succ; next; next = next->sh_succ)
      if (next->sh_class != h->sh_class)
	break;

    n = msg_header_prepare(mc, flags, h, &next, b + used, bsiz - used);

    if (n == -1) {
      errno = EINVAL;
      su_free(home, b);
      return NULL;
    }

    if (bsiz > used + n) {
      used += n;
      h = next;
    }
    else {
      /* Realloc */
      if (h->sh_succ)
	bsiz = (used + n + msg_min_size) / msg_min_size * msg_min_size;
      else
	bsiz = used + n + 1;

      if (bsiz < msg_min_size) {
	errno = ENOMEM;
	su_free(home, b);
	return NULL;
      }

      b2 = su_realloc(home, b, bsiz);

      if (b2 == NULL) {
	errno = ENOMEM;
	su_free(home, b);
	return NULL;
      }

      b = b2;

      continue;
    }
  }

  *return_len = used;

  b[used] = '\0';		/* NUL terminate */

  return su_realloc(home, b, used + 1);
}

/* ====================================================================== */
/* Handling header chain */

su_inline void serialize_first(msg_t *msg, msg_header_t *h);
static msg_header_t **serialize_one(msg_t *msg, msg_header_t *h,
				    msg_header_t **prev);

/** Return head of the fragment chain */
msg_header_t **msg_chain_head(msg_t const *msg)
{
  return msg ? (msg_header_t **)&msg->m_chain : NULL;
}

su_inline msg_header_t **_msg_chain_head(msg_t const *msg)
{
  return msg ? (msg_header_t **)&msg->m_chain : NULL;
}

/** Return tail of the fragment chain */
su_inline msg_header_t **msg_chain_tail(msg_t const *msg)
{
  return msg ? msg->m_tail : NULL;
}

/** Serialize headers into the fragment chain.
 *
 * The msg_serialize() collects the headers and other message components in
 * the fragment chain. It should be called before msg_prepare().
 *
 * @relatesalso msg_s
 *
 * @param msg pointer to message object
 * @param pub public message structure
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int msg_serialize(msg_t *msg, msg_pub_t *pub)
{
  msg_header_t *h, **hh, **end;
  msg_header_t **separator;
  msg_header_t **payload;
  msg_header_t **multipart;
  msg_mclass_t const *mc;
  msg_header_t **tail, ***ptail;

  if (!msg)
    return errno = EINVAL, -1;
  if (pub == NULL)
    pub = msg->m_object;

  /* There must be a first line */
  if (pub->msg_request)
    h = pub->msg_request;
  else if (pub->msg_status)
    h = pub->msg_status;
  else
    return errno = EINVAL, -1;

  serialize_first(msg, h);

  mc = msg->m_class;
  separator = (msg_header_t **)((char *)pub + mc->mc_separator->hr_offset);
  payload = (msg_header_t **)((char *)pub + mc->mc_payload->hr_offset);
  if (mc->mc_multipart->hr_class)
    multipart = (msg_header_t **)((char *)pub + mc->mc_multipart->hr_offset);
  else
    multipart = NULL;

  /* Find place to insert headers: before separator, payload and multipart */
  if (*separator && !msg_header_is_removed(*separator))
    ptail = &(*separator)->sh_prev;
  else if (*payload && !msg_header_is_removed(*payload))
    ptail = &(*payload)->sh_prev;
  else if (multipart && *multipart && !msg_header_is_removed(*multipart))
    ptail = &(*multipart)->sh_prev;
  else
    ptail = &msg->m_tail;

  tail = *ptail;

  end = (msg_header_t **)((char *)pub + pub->msg_size);

  for (hh = pub->msg_headers; hh < end; hh++) {
    if (!*hh)
      continue;
    if (hh == separator || hh == payload || hh == multipart)
      continue;
    tail = serialize_one(msg, *hh, tail);
  }

  /* Serialize separator, payload and multipart last */
  if (*separator)
    tail = serialize_one(msg, *separator, tail);

  *ptail = tail;

  /* Payload comes after separator but before multipart */
  if (ptail != &(*separator)->sh_prev)
    ;
  else if (*payload && !msg_header_is_removed(*payload))
    ptail = &(*payload)->sh_prev;
  else if (multipart && *multipart && !msg_header_is_removed(*multipart))
    ptail = &(*multipart)->sh_prev;
  else
    ptail = &msg->m_tail;

  tail = *ptail;

  if (*payload) {
    tail = serialize_one(msg, *payload, tail);
    *ptail = tail;
  }

  if (multipart && *multipart) {
    msg_header_t *last;

    last = msg_multipart_serialize(tail, (msg_multipart_t *)*multipart);

    msg->m_tail = &last->sh_succ;
  }

  assert(msg->m_chain && msg_chain_errors(msg->m_chain) == 0);

  return 0;
}

su_inline
void serialize_first(msg_t *msg, msg_header_t *h)
{
  if (msg_header_is_removed(h)) {
    if ((h->sh_succ = msg->m_chain))
      h->sh_succ->sh_prev = &h->sh_succ;
    else
      msg->m_tail = &h->sh_succ;
    *(h->sh_prev = &msg->m_chain) = h;
  }
}

static
msg_header_t **serialize_one(msg_t *msg, msg_header_t *h, msg_header_t **prev)
{
  msg_header_t *last;
  msg_header_t *succ = *prev;

  if (msg_header_is_removed(h)) {
    /* Add the first header in the list to the chain */
    *prev = h; h->sh_prev = prev;
    for (last = h; last->sh_succ; last = last->sh_succ) {
      /* Ensure that chain is connected */
      assert(last->sh_next == last->sh_succ);
      assert(last->sh_succ->sh_prev = &last->sh_succ);
    }
    prev = &last->sh_succ;
  }

  if ((h = h->sh_next)) {
    assert(!msg_is_single(h));

    if (msg_is_single(h)) {
      for (; h; h = h->sh_next)
	if (!msg_header_is_removed(h))
	  msg_chain_remove(msg, h);
    }
    /* Add the rest of the headers in the list to the chain */
    else for (; h; h = h->sh_next) {
      if (msg_header_is_removed(h)) {
	*prev = h; h->sh_prev = prev;
	for (;h->sh_succ; h = h->sh_succ)
	  assert(h->sh_succ == h->sh_next);
	prev = &h->sh_succ;
      }
    }
  }

  *prev = succ;

  return prev;
}

/**Fill an I/O vector with message contents.
 *
 * @relatesalso msg_s
 *
 * Calculate number of entries in the I/O vector
 * required to send a message @a msg. It also fills in the I/O vector array,
 * if it is provided by the caller and it is large enough.
 *
 * @param msg   pointer to message object
 * @param vec   I/O vector (may be NULL)
 * @param veclen length of I/O vector in @a vec
 *
 * @return
 * Number of entries of I/O
 * vector required by @a msg, or 0 upon an error.
 *
 * @note The caller should check that the I/O vector @a vec has enough
 * entries. If the @a vec is too short, it should allocate big enough
 * vector and re-invoke msg_iovec().
 *
 * @sa msg_recv_iovec(), su_vsend()
 */
isize_t msg_iovec(msg_t *msg, msg_iovec_t vec[], isize_t veclen)
{
  size_t len = 0, n = 0;
  char const *p = NULL;
  msg_header_t *h;

  size_t total = 0;

  if (veclen <= 0)
    veclen = 0;

  for (h = msg->m_chain; h; h = h->sh_succ) {
    if (h->sh_data != p) {
      p = h->sh_data; len = h->sh_len;

      if (p == NULL)
	return 0;

      if (vec && n != veclen)
	/* new iovec entry */
	vec[n].mv_base = (void *)p, vec[n].mv_len = (su_ioveclen_t)len;
      else
	vec = NULL;

      p += len; n++;
    }
    else {
      /* extend old entry */
      len = h->sh_len;
      if (vec)
	vec[n-1].mv_len += (su_ioveclen_t)len;
      p += len;
    }

    total += len;
  }

  msg->m_size = total;

  return n;
}

/** Insert a header to existing header chain.
 *
 * Headers are either inserted just before the payload, or after the first
 * line, depending on their type.
 *
 * @param[in]     msg  message object
 * @param[in,out] pub  public message structure
 * @param prepend if true, add before same type of headers (instead after them)
 * @param head head of chain
 * @param h    header to insert
 *
 */
static
void msg_insert_chain(msg_t *msg,
		      msg_pub_t *pub,
		      int prepend,
		      msg_header_t **head,
		      msg_header_t *h)
{
  msg_mclass_t const *mc;
  msg_header_t **hh;
  msg_header_t **separator;
  msg_header_t **payload;

  assert(msg && pub && head && h);

  mc = msg->m_class;
  separator = (msg_header_t **)((char *)pub + mc->mc_separator->hr_offset);
  payload = (msg_header_t **)((char *)pub + mc->mc_payload->hr_offset);

  if (msg_is_request(h)) {
    if (pub->msg_status)
      pub->msg_status = NULL;
    hh = head;
  }
  else if (msg_is_status(h)) {
    if (pub->msg_request)
      pub->msg_request = NULL;
    hh = head;
  }
  else if (msg_is_payload(h)) {
    /* Append */
    hh = msg_chain_tail(msg);
  }
  else if (prepend) {
    if (!msg_is_request(*head) && !msg_is_status(*head))
      hh = head;
    else
      hh = &((*head)->sh_succ);
  }
  /* Append headers before separator or payload */
  else if (*separator && (*separator)->sh_prev)
    hh = (*separator)->sh_prev;
  else if (*payload && (*payload)->sh_prev)
    hh = (*payload)->sh_prev;
  else
    hh = msg_chain_tail(msg);

  msg_insert_here_in_chain(msg, hh, h);
}

/** Insert one or more message header to the chain.
 *
 * The function msg_insert_here_in_chain() appends message header to the
 * chain of headers after the given header.
 *
 * @param msg  message
 * @param prev pointer to h_succ of previous fragment in the list
 * @param h    header to be inserted.
 *
 * @return The pointer to the last header inserted.
 */
static
void msg_insert_here_in_chain(msg_t *msg,
			      msg_header_t **prev,
			      msg_header_t *h)
{
  if (h) {
    msg_header_t *last, *next;
    assert(h->sh_prev == NULL);
    assert(prev);
    assert(!msg_chain_errors(h));

    for (last = h; last->sh_succ; last = last->sh_succ)
      ;

    last->sh_succ = next = *prev;
    *prev = h;
    h->sh_prev = prev;
    if (next)
      next->sh_prev = &last->sh_succ;
    else
      msg->m_tail = &last->sh_succ;

    assert(msg->m_chain && msg_chain_errors(msg->m_chain) == 0);
  }
}

/**
 * Remove a message from header chain.
 *
 * The function @c msg_chain_remove() removes a message header from the header
 * chain.
 *
 * @param msg  pointer to the message
 * @param h    pointer to the header in the list to be removed
 *
 * @return The pointer to the header just removed.
 */
su_inline
msg_header_t *msg_chain_remove(msg_t *msg, msg_header_t *h)
{
  if (h) {
    if (h->sh_prev) {
      assert(*h->sh_prev == h);
      assert(h->sh_succ == NULL || h->sh_succ->sh_prev == &h->sh_succ);

      *h->sh_prev = h->sh_succ;
    }

    if (h->sh_succ)
      h->sh_succ->sh_prev = h->sh_prev;
    else if (msg && h->sh_prev)
      msg->m_tail = h->sh_prev;

    h->sh_succ = NULL; h->sh_prev = NULL;

    if (msg)
      assert(msg_chain_errors(msg->m_chain) == 0);
  }
  return h;
}

#ifndef NDEBUG
/**Check if header chain contains any loops.
 *
 * @return
 * Return 0 if no loop, -1 otherwise.
 */
static
int msg_chain_loop(msg_header_t const *h)
{
  msg_header_t const *h2;

  if (!h) return 0;

  for (h2 = h->sh_succ; h && h2 && h2->sh_succ; h = h->sh_succ) {
    if (h == h2 || h == h2->sh_succ)
      return 1;

    h2 = h2->sh_succ->sh_succ;

    if (h == h2)
      return 1;
  }

  return 0;
}

/** Check header chain consistency.
 *
 * @return
 * Return 0 if consistent, number of errors otherwise.
 */
static
int msg_chain_errors(msg_header_t const *h)
{
  if (msg_chain_loop(h))
    return -1;

  for (; h; h = h->sh_succ) {
    if (h->sh_succ && h->sh_succ->sh_prev != &h->sh_succ)
      return -1;
    if (h->sh_prev && h != (*h->sh_prev))
      return -1;
  }

  return 0;
}
#endif

/* ====================================================================== */
/* Handling message structure - allocating, adding and removing headers */

/** Allocate a header structure
 *
 * The msg_header_alloc() function allocates a generic MO header structure
 * and returns a pointer to it.
 *
 * @param home  memory home
 * @param hc    header class
 * @param extra amount of extra memory to be allocated after header structure
 *
 * @return
 * A pointer to the newly created header object, or @c NULL upon an error.
 */
msg_header_t *msg_header_alloc(su_home_t *home,
			       msg_hclass_t *hc,
			       isize_t extra)
{
  isize_t size = hc->hc_size;
  msg_header_t *h = su_alloc(home, size + extra);

  if (h) {
    memset(h, 0, size);
    h->sh_class = hc;
  }

  return h;
}

/**Add a (list of) header(s) to the header structure and fragment chain.
 *
 * The function @c msg_header_add() adds a header or list of headers into
 * the given place within the message structure. It also inserts the headers
 * into the the message fragment chain, if it exists.
 *
 * If the header is a prepend header, the new header is inserted before
 * existing headers of the same class. If the header is an append header,
 * the new header is inserted after existing headers of the same class. If
 * the header is a singleton, existing headers of the same class are
 * removed. If the header is a list header, the values in the new header are
 * added to the existing list.
 *
 * @param msg message owning the fragment chain
 * @param pub public message structure
 * @param hh  place in message structure to which header is added
 * @param h   list of header(s) to be added
 */
int msg_header_add(msg_t *msg,
		   msg_pub_t *pub,
		   msg_header_t **hh,
		   msg_header_t *h)
{
  msg_header_t **head, *old = NULL, *end;

  if (msg == NULL || h == NULL || h == MSG_HEADER_NONE || hh == NULL)
    return -1;
  if (pub == NULL)
    pub = msg->m_object;

  head = _msg_chain_head(msg);

  if (*head) {
    msg_header_t *sh, **prev;

    for (sh = h, prev = NULL; sh; sh = sh->sh_next) {
      sh->sh_succ = sh->sh_next;
      sh->sh_prev = prev;
      prev = &sh->sh_succ;
    }
  }

  switch (h->sh_class->hc_kind) {
  case msg_kind_single:
  case msg_kind_list:
    old = (*hh);
    break;
  case msg_kind_append:
  case msg_kind_apndlist:
    while (*hh)
      hh = &(*hh)->sh_next;
    break;
  case msg_kind_prepend:
    for (end = h; end->sh_next; end = end->sh_next)
      ;
    end->sh_next = *hh;
  }

  if (*head) {
    /* Insert into existing fragment chain */
    msg_insert_chain(msg, pub, msg_is_prepend(h), head, h);

    /* Remove replaced fragment */
    if (old)
      msg_chain_remove(msg, old);
  }

  /* Insert into header list */
  *hh = h;

  return 0;
}

/**Prepend a (list of) header(s) to the header structure and fragment chain.
 *
 * The function @c msg_header_prepend() adds a header or list of headers into
 * the given place within the message structure. It also inserts the headers
 * into the the message fragment chain, if it exists.
 *
 * Unlike msg_header_add(), msg_header_prepend() always inserts header @a h
 * before other headers of the same class. If the header is a singleton,
 * existing headers of the same class are removed. If the header is a list
 * header, the values in the new header are prepended to the existing list.
 *
 * @param msg message owning the fragment chain
 * @param pub public message structure
 * @param hh  place in message structure to which header is added
 * @param h   list of header(s) to be added
 */
int msg_header_prepend(msg_t *msg,
		       msg_pub_t *pub,
		       msg_header_t **hh,
		       msg_header_t *h)
{
  msg_header_t **head, *old = NULL, *end;

  assert(msg && pub);

  if (msg == NULL || h == NULL || h == MSG_HEADER_NONE || hh == NULL)
    return -1;
  if (pub == NULL)
    pub = msg->m_object;

  head = _msg_chain_head(msg);

  if (*head) {
    msg_header_t *sh, **prev;

    for (sh = h, prev = NULL; sh; sh = sh->sh_next) {
      sh->sh_succ = sh->sh_next;
      sh->sh_prev = prev;
      prev = &sh->sh_succ;
    }
  }

  switch (h->sh_class->hc_kind) {
  case msg_kind_single:
  case msg_kind_list:
    old = (*hh);
    break;
  case msg_kind_append:
  case msg_kind_apndlist:
  case msg_kind_prepend:
    for (end = h; end->sh_next; end = end->sh_next)
      ;
    end->sh_next = *hh;
    break;
  }

  if (*head) {
    /* Insert into existing fragment chain */
    msg_insert_chain(msg, pub, 1, head, h);

    /* Remove replaced fragment */
    if (old)
      msg_chain_remove(msg, old);
  }

  /* Insert into header list */
  *hh = h;

  return 0;
}


/** Find place to insert header of the class @a hc. */
msg_header_t **
msg_hclass_offset(msg_mclass_t const *mc, msg_pub_t const *mo, msg_hclass_t *hc)
{
  assert(mc && hc);

  if (mc == NULL || hc == NULL)
    return NULL;

  if (hc->hc_hash > 0) {
    unsigned j, N = mc->mc_hash_size;
    for (j = hc->hc_hash % N; mc->mc_hash[j].hr_class; j = (j + 1) % N)
      if (mc->mc_hash[j].hr_class == hc) {
	return (msg_header_t **)((char *)mo + mc->mc_hash[j].hr_offset);
      }
  } else {
    /* Header has no name. */
    if (hc->hc_hash == mc->mc_request[0].hr_class->hc_hash) return (msg_header_t **)((char *)mo + mc->mc_request[0].hr_offset);
    if (hc->hc_hash == mc->mc_status[0].hr_class->hc_hash) return (msg_header_t **)((char *)mo + mc->mc_status[0].hr_offset);
    if (hc->hc_hash == mc->mc_separator[0].hr_class->hc_hash) return (msg_header_t **)((char *)mo + mc->mc_separator[0].hr_offset);
    if (hc->hc_hash == mc->mc_payload[0].hr_class->hc_hash) return (msg_header_t **)((char *)mo + mc->mc_payload[0].hr_offset);
    if (hc->hc_hash == mc->mc_unknown[0].hr_class->hc_hash) return (msg_header_t **)((char *)mo + mc->mc_unknown[0].hr_offset);
    if (hc->hc_hash == mc->mc_error[0].hr_class->hc_hash) return (msg_header_t **)((char *)mo + mc->mc_error[0].hr_offset);
    if (hc->hc_hash == mc->mc_multipart[0].hr_class->hc_hash) return (msg_header_t **)((char *)mo + mc->mc_multipart[0].hr_offset);
  }

  return NULL;
}

/** Append a parsed header object into the message structure */
su_inline void
append_parsed(msg_t *msg, msg_pub_t *mo, msg_href_t const *hr, msg_header_t *h,
	      int always_into_chain)
{
  msg_header_t **hh;

  assert(msg); assert(hr->hr_offset);

  hh = (msg_header_t **)((char *)mo + hr->hr_offset);

  if (msg->m_chain || always_into_chain)
    msg_insert_here_in_chain(msg, msg_chain_tail(msg), h);

  if (*hh && msg_is_single(h)) {
    /* If there is multiple instances of single headers,
       put the extra headers into the list of erroneous headers */
    msg_error_t **e;

    for (e = &mo->msg_error; *e; e = &(*e)->er_next)
      ;
    *e = (msg_error_t *)h;

    msg->m_extract_err |= hr->hr_flags;
    if (hr->hr_class->hc_critical)
      mo->msg_flags |= MSG_FLG_ERROR;

    return;
  }

  while (*hh)
    hh = &(*hh)->sh_next;
  *hh = h;
}

static int _msg_header_add_list_items(msg_t *msg,
				      msg_header_t **hh,
				      msg_header_t const *src);

/**Duplicate and add a (list of) header(s) to the message.
 *
 * The function @c msg_header_add_dup() duplicates and adds a (list of)
 * header(s) into a message structure.
 *
 * When inserting headers into the fragment chain, a request (or status) is
 * inserted first and replaces the existing request (or status).  Other
 * headers are inserted after the request or status.
 *
 * If the header is a singleton, existing headers with the same class are
 * removed.
 *
 * @param msg message owning the fragment chain
 * @param pub public message structure to which header is added
 * @param src list of header(s) to be added
 */
int msg_header_add_dup(msg_t *msg,
		       msg_pub_t *pub,
		       msg_header_t const *src)
{
  msg_header_t *h, **hh = NULL;
  msg_hclass_t *hc = NULL;

  if (msg == NULL)
    return -1;
  if (src == NULL || src == MSG_HEADER_NONE)
    return 0;
  if (pub == NULL)
    pub = msg->m_object;

  for ( ;src; src = src->sh_next) {
    assert(src->sh_class);

    if (!src->sh_class)
      return -1;

    if (hc != src->sh_class)
      hh = msg_hclass_offset(msg->m_class, pub, hc = src->sh_class);

    if (hh == NULL)
      return -1;

    if (!*hh || hc->hc_kind != msg_kind_list) {
      int size = hc->hc_size;
      isize_t xtra = hc->hc_dxtra(src, size) - size;
      char *end;

      if (!(h = msg_header_alloc(msg_home(msg), hc, xtra)))
	return -1;			/* error */

      if (!(end = hc->hc_dup_one(h, src, (char *)h + size, xtra)))
	return -1;			/* error */

      if (hc->hc_update)
	msg_header_update_params(h->sh_common, 0);

      assert(end == (char *)h + size + xtra);

      if (msg_header_add(msg, pub, hh, h) < 0)
	return -1;

      hh = &h->sh_next;
    }
    else {
      if (_msg_header_add_list_items(msg, hh, src) < 0)
	break;
    }
  }

  if (src)
    return -1;

  return 0;
}

/**Duplicate a header as a given type and add the duplicate into message.
 *
 * The function @c msg_header_add_dup_as() duplicates a header as a instance
 * of the given header class. It adds the new copy into the message.
 *
 * When inserting headers into the fragment chain, a request (or status) is
 * inserted first and replaces the existing request (or status).  Other
 * headers are inserted after the request or status.
 *
 * If the header is a singleton, existing headers with the same class are
 * removed.
 *
 * @param msg message owning the fragment chain
 * @param pub public message structure to which header is added
 * @param hc  header class for header target type
 * @param src list of header(s) to be duplicated and added
 */
int msg_header_add_dup_as(msg_t *msg,
			  msg_pub_t *pub,
			  msg_hclass_t *hc,
			  msg_header_t const *src)
{
  if (msg == NULL || hc == NULL)
    return -1;
  if (src == NULL || src == MSG_HEADER_NONE)
    return 0;
  if (pub == NULL)
    pub = msg->m_object;

  return _msg_header_add_dup_as(msg, pub, hc, src);
}

/** Duplicate and add a (list of) header to a message */
static
int _msg_header_add_dup_as(msg_t *msg,
			   msg_pub_t *pub,
			   msg_hclass_t *hc,
			   msg_header_t const *src)
{
  msg_header_t *h, **hh;

  hh = msg_hclass_offset(msg->m_class, pub, hc);

  if (hh == NULL)
    return -1;

  if (*hh && hc->hc_kind == msg_kind_list)
    return _msg_header_add_list_items(msg, hh, src);

  if (!(h = msg_header_dup_as(msg_home(msg), hc, src)))
    return -1;

  return msg_header_add(msg, pub, hh, h);
}

/* Add list items */
static int _msg_header_add_list_items(msg_t *msg,
				      msg_header_t **hh,
				      msg_header_t const *src)
{
  msg_header_t *h = *hh;
  msg_param_t **s = msg_header_params(src->sh_common);

  if (!s || !*s)
    return 0;

  msg_fragment_clear(h->sh_common);

  /* Remove empty headers */
  for (hh = &h->sh_next; *hh; *hh = (*hh)->sh_next)
    msg_chain_remove(msg, *hh);

  if (msg_header_join_items(msg_home(msg), h->sh_common, src->sh_common, 1)
      < 0)
    return -1;

  return 0;
}

/** Parse a string as a given header field and add result to the message. */
int msg_header_add_make(msg_t *msg,
			msg_pub_t *pub,
			msg_hclass_t *hc,
			char const *s)
{
  msg_header_t *h, **hh;

  if (msg == NULL)
    return -1;
  if (pub == NULL)
    pub = msg->m_object;

  hh = msg_hclass_offset(msg->m_class, pub, hc);

  if (hh == NULL)
    return -1;

  if (!s)
    return 0;

  if (*hh && hc->hc_kind == msg_kind_list) {
    /* Add list items */
    msg_header_t *h = *hh;
    msg_param_t **d;
    char *s0;

    skip_lws(&s);

    d = msg_header_params(h->sh_common); assert(d);

    msg_fragment_clear(h->sh_common);

    /* Remove empty headers */
    for (hh = &h->sh_next; *hh; *hh = (*hh)->sh_next)
      msg_chain_remove(msg, *hh);

    s0 = su_strdup(msg_home(msg), s);

    if (!s0 || msg_commalist_d(msg_home(msg), &s0, d, msg_token_scan) < 0)
      return -1;

    return 0;
  }

  if (!(h = msg_header_make(msg_home(msg), hc, s)))
    return -1;

  return msg_header_add(msg, pub, hh, h);
}

/** Add formatting result to message.
 *
 * Parse result from printf-formatted params as a given header field and add
 * result to the message.
 *
 * @NEW_1_12_10
 */
int msg_header_add_format(msg_t *msg,
			  msg_pub_t *pub,
			  msg_hclass_t *hc,
			  char const *fmt,
			  ...)
{
  msg_header_t *h, **hh;
  va_list va;

  if (msg == NULL)
    return -1;
  if (pub == NULL)
    pub = msg->m_object;

  hh = msg_hclass_offset(msg->m_class, pub, hc);

  if (hh == NULL)
    return -1;

  if (!fmt)
    return 0;

  va_start(va, fmt);
  h = msg_header_vformat(msg_home(msg), hc, fmt, va);
  va_end(va);

  if (!h)
    return -1;

  return msg_header_add(msg, pub, hh, h);
}


/**Add string contents to message.
 *
 * Duplicate a string containing headers (or a message body, if the string
 * starts with linefeed), parse it and add resulting header objects to the
 * message object.
 *
 * @param msg  message object
 * @param pub  message header structure where heades are added (may be NULL)
 * @param str  string to be copied and parsed (not modified, may be NULL)
 *
 * @retval 0 when succesful
 * @retval -1 upon an error
 */
int msg_header_add_str(msg_t *msg,
		       msg_pub_t *pub,
		       char const *str)
{
  char *s;

  if (!msg)
    return -1;
  if (!str)
    return 0;

  s = su_strdup(msg_home(msg), str);

  if (s == NULL)
    return -1;

  return msg_header_parse_str(msg, pub, s);
}

/**Add string to message.
 *
 * Parse a string containing headers (or a message body, if the string
 * starts with linefeed) and add resulting header objects to the message
 * object.
 *
 * @param msg  message object
 * @param pub  message header structure where heades are added (may be NULL)
 * @param s    string to be parsed (and modified)
 *
 * @retval 0 when succesful
 * @retval -1 upon an error
 *
 * @sa msg_header_add_str(), url_headers_as_string()
 *
 * @since New in @VERSION_1_12_4.
 */
int msg_header_parse_str(msg_t *msg,
			 msg_pub_t *pub,
			 char *s)
{
  if (!msg)
    return -1;

  if (pub == NULL)
    pub = msg->m_object;

  if (s) {
    size_t ssiz = strlen(s), used = 0;
    ssize_t n = 1;

    while (ssiz > used) {
      if (IS_CRLF(s[used]))
	break;
      n = msg_extract_header(msg, pub, s + used, ssiz - used, 1);
      if (n <= 0)
	break;
      used += n;
    }

    if (n > 0 && ssiz > used) {
      used += CRLF_TEST(s + used);
      if (ssiz > used)
	msg_extract_payload(msg, pub, NULL, ssiz - used,
			    s + used, ssiz - used, 1);
    }

    if (n <= 0)
      return -1;
  }

  return 0;
}

/** Insert a (list of) header(s) to the fragment chain.
 *
 * The function @c msg_header_insert() inserts header or list of headers
 * into a message structure.  It also inserts them into the the message
 * fragment chain, if it exists.
 *
 * When inserting headers into the fragment chain, a request (or status) is
 * inserted first and replaces the existing request (or status).  Other
 * headers are inserted after the request or status.
 *
 * If there can be only one header field of this type (hc_kind is
 * msg_kind_single), existing header objects with the same class are
 * removed.
 *
 * @param msg message object owning the fragment chain
 * @param pub public message structure to which header is added
 * @param h   list of header(s) to be added
 */
int msg_header_insert(msg_t *msg, msg_pub_t *pub, msg_header_t *h)
{
  msg_header_t **hh;

  assert(msg);

  if (msg == NULL || h == NULL || h == MSG_HEADER_NONE ||
      h->sh_class == NULL)
    return -1;
  if (pub == NULL)
    pub = msg->m_object;

  hh = msg_hclass_offset(msg->m_class, pub, h->sh_class);

  return msg_header_add(msg, pub, hh, h);
}

/**Remove a header from the header structure and fragment chain.
 *
 * The function @c msg_header_remove() removes a header from a message
 * structure.  It also removes the message from the message fragment chain
 * and clears the encoding of other headers objects that share same
 * encoding.
 *
 * @param msg message owning the fragment chain
 * @param pub public message structure to which header is added
 * @param h   header to be removed
 */
int msg_header_remove(msg_t *msg, msg_pub_t *pub, msg_header_t *h)
{
  msg_header_t **hh, **hh0;

  if (msg == NULL || h == NULL || h == MSG_HEADER_NONE ||
      h->sh_class == NULL)
    return -1;
  if (pub == NULL)
    pub = msg->m_object;

  /* First, remove from public structure (msg_pub_t) */
  hh0 = msg_hclass_offset(msg->m_class, pub, h->sh_class);
  if (!hh0)
    return -1;

  for (hh = hh0; *hh; hh = &(*hh)->sh_next) {
    if (*hh == h) {
      *hh = h->sh_next;
      break;
    }
  }

  if (h->sh_data) {
    void const *data = (char *)h->sh_data + h->sh_len;
    for (hh = hh0; *hh; hh = &(*hh)->sh_next) {
      if (data == (char *)(*hh)->sh_data + (*hh)->sh_len) {
	(*hh)->sh_data = NULL, (*hh)->sh_len = 0;
      }
    }
  }

  msg_chain_remove(msg, h);

  return 0;
}


/**Remove a header list from the header structure and fragment chain.
 *
 * The function @c msg_header_remove_all() removes a list of headers from a
 * message structure. It also removes the message from the message fragment
 * chain and clears the encoding of other headers objects that share same
 * encoding.
 *
 * @param msg message owning the fragment chain
 * @param pub public message structure to which header is added
 * @param h   header list to be removed
 */
int msg_header_remove_all(msg_t *msg, msg_pub_t *pub, msg_header_t *h)
{
  msg_header_t **hh, **hh0;
  void const *data;

  if (msg == NULL || h == NULL || h == MSG_HEADER_NONE ||
      h->sh_class == NULL)
    return -1;
  if (pub == NULL)
    pub = msg->m_object;

  hh0 = msg_hclass_offset(msg->m_class, pub, h->sh_class);
  if (!hh0)
    return -1;

  data = (char *)h->sh_data + h->sh_len;

  /* First, remove from public structure (msg_pub_t) */
  for (hh = hh0; *hh; hh = &(*hh)->sh_next) {
    if (*hh == h) {
      break;
    }
    if (data && data == (char *)(*hh)->sh_data + (*hh)->sh_len) {
      h->sh_data = NULL, h->sh_len = 0;
      (*hh)->sh_data = NULL, (*hh)->sh_len = 0;
    }
  }

  /* Remove from header chain */
  while (h) {
    h->sh_data = NULL, h->sh_len = 0;
    msg_chain_remove(msg, h);
    h = h->sh_next;
  }

  *hh = NULL;

  return 0;
}


/** Replace a header item with a (list of) header(s).
 *
 * The function @c msg_header_replace() removes a header structure from
 * message and replaces it with a new one or a list of headers. It inserts
 * the new headers into the the message fragment chain, if it exists.
 *
 * @param msg message object owning the fragment chain
 * @param pub public message structure to which header is added
 * @param replaced   old header to be removed
 * @param h   list of header(s) to be added
 */
int msg_header_replace(msg_t *msg,
		       msg_pub_t *pub,
		       msg_header_t *replaced,
		       msg_header_t *h)
{
  msg_header_t *h0, *last, **hh, **hh0;

  if (msg == NULL || replaced == NULL)
    return -1;
  if (h == NULL || h == MSG_HEADER_NONE || h->sh_class == NULL)
    return msg_header_remove(msg, pub, replaced);
  if (pub == NULL)
    pub = msg->m_object;

  hh = hh0 = msg_hclass_offset(msg->m_class, pub, h->sh_class);
  if (hh == NULL)
    return -1;
  if (replaced == NULL)
    return msg_header_add(msg, pub, hh, h);

  assert(h->sh_prev == NULL);	/* Must not be in existing chain! */

  for (last = h; last->sh_next; last = last->sh_next) {
    if ((last->sh_succ = last->sh_next))
      last->sh_next->sh_prev = &last->sh_succ;
  }

  for (h0 = *hh; h0; hh = &h0->sh_next, h0 = *hh) {
    if (replaced == h0)
      break;
  }

  if (h0 == NULL)
    return -1;

  *hh = h;			/* Replace in list */
  last->sh_next = replaced->sh_next;

  if (replaced->sh_prev) {
    *replaced->sh_prev = h;
    h->sh_prev = replaced->sh_prev;
    if ((last->sh_succ = replaced->sh_succ))
      last->sh_succ->sh_prev = &last->sh_succ;
    if (msg->m_tail == &replaced->sh_succ)
      msg->m_tail = &last->sh_succ;
  }

  assert(msg->m_tail != &replaced->sh_succ);

  replaced->sh_next = NULL;
  replaced->sh_prev = NULL;
  replaced->sh_succ = NULL;

  if (replaced->sh_data) {
    /* Remove cached encoding if it is shared with more than one header fragments */
    int cleared = 0;
    void const *data = (char *)replaced->sh_data + replaced->sh_len;

    for (hh = hh0; *hh; hh = &(*hh)->sh_next) {
      if (data == (char *)(*hh)->sh_data + (*hh)->sh_len) {
	(*hh)->sh_data = NULL, (*hh)->sh_len = 0, cleared = 1;
      }
    }

    if (cleared)
      replaced->sh_data = NULL, replaced->sh_len = 0;
  }

  return 0;
}

/** Free a header structure */
void msg_header_free(su_home_t *home, msg_header_t *h)
{
  su_free(home, h);
}

/** Free a (list of) header structures */
void msg_header_free_all(su_home_t *home, msg_header_t *h)
{
  msg_header_t *h_next;

  while (h) {
    h_next = h->sh_next;
    su_free(home, h);
    h = h_next;
  }
}

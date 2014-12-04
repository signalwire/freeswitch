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

/**@ingroup sdp_printer
 *
 * @CFILE sdp_print.c  Simple SDP printer interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Feb 18 10:25:08 2000 ppessi
 */

#include "config.h"

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_string.h>

#include "sofia-sip/sdp.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

/* ========================================================================= */
/* Printing API */
/* */

#define SDP_BLOCK   (512)

#define CRLF "\015\012"

typedef unsigned longlong ull;

/** @typedef struct sdp_printer_s sdp_printer_t
 *
 * SDP printer handle.
 *
 * @sa #sdp_session_t, sdp_print(), sdp_printing_error(),
 * sdp_message(), sdp_message_size(), sdp_printer_free()
 */

struct sdp_printer_s {
  int        pr_size;
  su_home_t *pr_home;
  char      *pr_buffer;
  size_t     pr_bsiz;
  size_t     pr_used;
  /* various flags */
  unsigned   pr_ok : 1;
  unsigned   pr_strict : 1;
  unsigned   pr_owns_buffer:1;
  unsigned   pr_may_realloc:1;
  unsigned   pr_all_rtpmaps:1;
  unsigned   pr_mode_manual:1;
  unsigned   pr_mode_always:1;
};

static struct sdp_printer_s printer_memory_error = {
  sizeof(printer_memory_error),
  NULL,
  "memory exhausted",
  sizeof(printer_memory_error.pr_buffer),
  sizeof(printer_memory_error.pr_buffer)
};

static void print_session(sdp_printer_t *p, sdp_session_t const *session);
static void printing_error(sdp_printer_t *p, const char *fmt, ...);

/** Print a SDP description.
 *
 * Encode the contents of the SDP session structure #sdp_session_t
 * to the @a msgbuf. The @a msgbuf has size @a msgsize
 * bytes. If @a msgbuf is @c NULL, the sdp_print() function allocates the
 * required buffer from the @a home heap.
 *
 * @param home     Memory home (may be NULL).
 * @param session  SDP session description structure to be encoded.
 * @param msgbuf   Buffer to which encoding is stored (may be NULL).
 * @param msgsize  Size of @a msgbuf.
 * @param flags    Flags specifying the encoding options.
 *
 * The @a flags specify encoding options as follows:
 *
 * @li #sdp_f_strict - Printer should only emit messages conforming strictly
 * to the * specification.
 *
 * @li #sdp_f_realloc - If this flag is specified, and @a msgbuf is too
 * small for the resulting SDP message, @c sdp_print() may allocate a new
 * buffer for it from the heap.
 *
 * @li #sdp_f_print_prefix - The buffer provided by caller already contains
 * valid data. The result will concatenated to the string in the buffer.
 *
 * @li #sdp_f_mode_always - Always add mode attributes to media
 *
 * @li #sdp_f_mode_manual - Do not generate mode attributes
 *
 * @return
 * Always return a handle to an #sdp_printer_t object.
 *
 * @sa #sdp_printer_t, #sdp_session_t, sdp_printing_error(),
 * sdp_message(), sdp_message_size(), sdp_printer_free(),
 * sdp_parse().
 */
sdp_printer_t *sdp_print(su_home_t *home,
			 sdp_session_t const *session,
			 char msgbuf[],
			 isize_t msgsize,
			 int flags)
{
  sdp_printer_t *p = su_salloc(home, sizeof(*p));

  if (p) {
    p->pr_size = sizeof(p);
    p->pr_home = home;
    p->pr_used   = 0;
    if (msgbuf) {
      p->pr_may_realloc = (flags & sdp_f_realloc) != 0;
      p->pr_buffer = msgbuf;
      p->pr_bsiz   = msgsize;
      if (flags & sdp_f_print_prefix)
	p->pr_used = strlen(msgbuf);
    }
    else {
      p->pr_owns_buffer = 1;
      p->pr_buffer = su_alloc(home, SDP_BLOCK);
      p->pr_bsiz   = SDP_BLOCK;
    }
    p->pr_strict = (flags & sdp_f_strict) != 0;
    p->pr_all_rtpmaps = (flags & sdp_f_all_rtpmaps) != 0;
    p->pr_mode_manual = (flags & sdp_f_mode_manual) != 0;
    p->pr_mode_always = (flags & sdp_f_mode_always) != 0;

    if (session)
      print_session(p, session);
    else
      printing_error(p, "NULL session description");

    return p;
  }
  else {
    return &printer_memory_error;
  }
}

/** @brief Get encoding error.
 *
 * Return a message describing the encoding error.
 *
 * @param p Pointer to an #sdp_printer_t object.
 *
 * @return
 * Return a pointer to C string describing printing errors, or NULL if no
 * error was encountered.
 */
char const *sdp_printing_error(sdp_printer_t *p)
{
  if (p)
    if (!p->pr_ok)
      return p->pr_buffer;
    else
      return NULL;
  else
    return "null sdp_printer_t*";
}

/** @brief Get encoded SDP message.
 *
 * Return a pointer to a C string containing the SDP message.
 *
 * @param p Pointer to an #sdp_printer_t object.
 *
 * @return
 * Return a pointer to a C string containing the encoded SDP message, or
 * NULL upon an error.
 */
char const *sdp_message(sdp_printer_t *p)
{
  if (p && p->pr_ok)
    return p->pr_buffer;
  else
    return NULL;
}

/** @brief Get size of encoded SDP message.
 *
 * Return the size of the encoded SDP message.
 *
 * @param p Pointer to an #sdp_printer_t object.
 *
 * @return
 * Number of bytes in SDP message excluding final NUL or 0 upon an error.
 */
isize_t sdp_message_size(sdp_printer_t *p)
{
  if (p && p->pr_ok)
    return p->pr_used;
  else
    return 0;
}

/** Free a SDP printer.
 *
 * Free the printer object @a p and the message buffer possibly associated
 * with it.
 *
 * @param p Pointer to an #sdp_printer_t object.
 */
void sdp_printer_free(sdp_printer_t *p)
{
  if (p && p != &printer_memory_error) {
    if (p->pr_owns_buffer && p->pr_buffer)
      su_free(p->pr_home, p->pr_buffer), p->pr_buffer = NULL;
    su_free(p->pr_home, p);
  }
}

/* ========================================================================= */
static void print_version(sdp_printer_t *p, sdp_version_t const *v);
static void print_origin(sdp_printer_t *p, sdp_origin_t const *o);
static void print_subject(sdp_printer_t *p, sdp_text_t *s);
static void print_information(sdp_printer_t *p, sdp_text_t *i);
static void print_uri(sdp_printer_t *p, sdp_text_t  *u);
static void print_emails(sdp_printer_t *p, sdp_list_t const *e);
static void print_phones(sdp_printer_t *p, sdp_list_t const *ph);
static void print_connection(sdp_printer_t *p, sdp_connection_t const *c);
static void print_connection_list(sdp_printer_t *p, sdp_connection_t const *c);
static void print_connection2(sdp_printer_t *p, sdp_connection_t const *c);
static void print_bandwidths(sdp_printer_t *p, sdp_bandwidth_t const *b);
static void print_time(sdp_printer_t *p, sdp_time_t const *t);
static void print_repeat(sdp_printer_t *p, sdp_repeat_t const *r);
static void print_zone(sdp_printer_t *p, sdp_zone_t const *z);
static void print_typed_time(sdp_printer_t *p, unsigned long t);
static void print_key(sdp_printer_t *p, sdp_key_t const *k);
static void print_attributes(sdp_printer_t *p, sdp_attribute_t const *a);
static void print_charset(sdp_printer_t *p, sdp_text_t *charset);
static void print_media(sdp_printer_t *p, sdp_session_t const *,
			sdp_media_t const *m);

static void print_text_list(sdp_printer_t*,
			    const char *, sdp_list_t const *l);

static void sdp_printf(sdp_printer_t *p, const char *fmt, ...);

static void print_session(sdp_printer_t *p, sdp_session_t const *sdp)
{
  p->pr_ok = 1;

  if (p->pr_ok)
    print_version(p, sdp->sdp_version);
  if (p->pr_ok && sdp->sdp_origin)
    print_origin(p, sdp->sdp_origin);
  if (p->pr_ok && sdp->sdp_subject)
    print_subject(p, sdp->sdp_subject);
  if (p->pr_ok && sdp->sdp_information)
    print_information(p, sdp->sdp_information);
  if (p->pr_ok && sdp->sdp_uri)
    print_uri(p, sdp->sdp_uri);
  if (p->pr_ok && sdp->sdp_emails)
    print_emails(p, sdp->sdp_emails);
  if (p->pr_ok && sdp->sdp_phones)
    print_phones(p, sdp->sdp_phones);
  if (p->pr_ok && sdp->sdp_connection)
    print_connection(p, sdp->sdp_connection);
  if (p->pr_ok && sdp->sdp_bandwidths)
    print_bandwidths(p, sdp->sdp_bandwidths);
  if (p->pr_ok)
    print_time(p, sdp->sdp_time);
  if (p->pr_ok && sdp->sdp_time) {
    if (p->pr_ok && sdp->sdp_time->t_repeat)
      print_repeat(p, sdp->sdp_time->t_repeat);
    if (p->pr_ok && sdp->sdp_time->t_zone)
      print_zone(p, sdp->sdp_time->t_zone);
  }
  if (p->pr_ok && sdp->sdp_key)
    print_key(p, sdp->sdp_key);
  if (p->pr_ok && sdp->sdp_charset)
    print_charset(p, sdp->sdp_charset);
  if (p->pr_ok && sdp->sdp_attributes)
    print_attributes(p, sdp->sdp_attributes);
  if (p->pr_ok && sdp->sdp_media)
    print_media(p, sdp, sdp->sdp_media);
}

static void print_version(sdp_printer_t *p, sdp_version_t const *v)
{
  sdp_printf(p, "v=%lu" CRLF, *v);
}

static void print_origin(sdp_printer_t *p, sdp_origin_t const *o)
{
  if (!o->o_address ||
      !o->o_address->c_address ||
      o->o_address->c_ttl != 0 ||
      o->o_address->c_groups > 1) {
    printing_error(p, "o= address malformed");
    return;
  }

  sdp_printf(p, "o=%s "LLU" "LLU" ",
	     o->o_username,
	     (ull)o->o_id,
	     (ull)o->o_version);

  print_connection2(p, o->o_address);
}

static void print_subject(sdp_printer_t *p, sdp_text_t *subject)
{
  sdp_printf(p, "s=%s" CRLF, subject);
}

static void print_information(sdp_printer_t *p, sdp_text_t *information)
{
  sdp_printf(p, "i=%s" CRLF, information);
}

static void print_uri(sdp_printer_t *p, sdp_text_t *uri)
{
  sdp_printf(p, "u=%s" CRLF, uri);
}

static void print_emails(sdp_printer_t *p, sdp_list_t const *emails)
{
  print_text_list(p, "e=%s" CRLF, emails);
}

static void print_phones(sdp_printer_t *p, sdp_list_t const *phones)
{
  print_text_list(p, "p=%s" CRLF, phones);
}

static void print_connection(sdp_printer_t *p, sdp_connection_t const *c)
{
  sdp_printf(p, "c=");
  print_connection2(p, c);
}

static void print_connection_list(sdp_printer_t *p, sdp_connection_t const *c)
{
  for (; c ; c = c->c_next) {
    sdp_printf(p, "c=");
    print_connection2(p, c);
  }
}

static void print_connection2(sdp_printer_t *p, sdp_connection_t const *c)
{
  const char *nettype;
  const char *addrtype;

  switch (c->c_nettype) {
  case sdp_net_x:
    nettype = NULL;
    break;
  case sdp_net_in:
    nettype = "IN ";
    break;
  default:
    printing_error(p, "unknown nettype %u", c->c_nettype);
    return;
  }

  switch (c->c_addrtype) {
  case sdp_addr_x:
    addrtype = NULL;
    break;
  case sdp_addr_ip4:
    nettype = "IN ";
    addrtype = "IP4 ";
    break;
  case sdp_addr_ip6:
    nettype = "IN ";
    addrtype = "IP6 ";
    break;
  default:
    printing_error(p, "unknown address type %u", c->c_addrtype);
    return;
  }

  if (c->c_address == NULL) {
    printing_error(p, "missing address");
    return;
  }

  if (nettype && addrtype)
    sdp_printf(p, "%s%s%s", nettype, addrtype, c->c_address);
  else if (nettype)
    sdp_printf(p, "%s%s%s", nettype, c->c_address);
  else
    sdp_printf(p, "%s", c->c_address);

  if (c->c_mcast || c->c_ttl) {
    sdp_printf(p, "/%u", c->c_ttl);
    if (c->c_groups > 1)
      sdp_printf(p, "/%u", c->c_groups);
  }
  sdp_printf(p, CRLF);
}

static void print_bandwidths(sdp_printer_t *p, sdp_bandwidth_t const *b)
{
  for (; b ; b = b->b_next) {
    char const *name;

    switch (b->b_modifier) {
    case sdp_bw_ct: name = "CT"; break;
    case sdp_bw_as: name = "AS"; break;
    case sdp_bw_tias: name = "TIAS"; break;
    default:        name = b->b_modifier_name; break;
    }

    sdp_printf(p, "b=%s:%lu" CRLF, name, b->b_value);
  }
}

static void print_time(sdp_printer_t *p, sdp_time_t const *t)
{
  if (t || p->pr_strict)
    sdp_printf(p, "t=%lu %lu" CRLF, t ? t->t_start : 0L, t ? t->t_stop : 0L);
}

static void print_repeat(sdp_printer_t *p, sdp_repeat_t const *r)
{
  int i;

  sdp_printf(p, "r=");
  print_typed_time(p, r->r_interval);
  sdp_printf(p, " ");
  print_typed_time(p, r->r_duration);
  for (i = 0; i < r->r_number_of_offsets; i++) {
    sdp_printf(p, " ");
    print_typed_time(p, r->r_offsets[i]);
  }
  sdp_printf(p, CRLF);
}

static void print_zone(sdp_printer_t *p, sdp_zone_t const *z)
{
  int i;
  sdp_printf(p, "z=");

  for (i = 0; i < z->z_number_of_adjustments; i++) {
    int negative = z->z_adjustments[i].z_offset < 0L;
    sdp_printf(p, "%s%lu %s",
	       i > 0 ? " " : "",
	       z->z_adjustments[i].z_at,
	       negative ? "-" : "");
    if (negative)
      print_typed_time(p, -z->z_adjustments[i].z_offset);
    else
      print_typed_time(p, z->z_adjustments[i].z_offset);
  }

  sdp_printf(p, CRLF);
}

static void  print_typed_time(sdp_printer_t *p, unsigned long t)
{
  if (t % 60 || t == 0) {
    sdp_printf(p, "%lu", t);
  }
  else {
    t /= 60;

    if (t % 60) {
      sdp_printf(p, "%lum", t); /* minutes */
    }
    else {
      t /= 60;

      if (t % 24) {
	sdp_printf(p, "%luh", t); /* hours */
      }
      else {
	t /= 24;

	sdp_printf(p, "%lud", t);	/* days */
      }
    }
  }
}

static void print_key(sdp_printer_t *p, sdp_key_t const *k)
{
  const char *method;
  int have_material = k->k_material != NULL;

  switch (k->k_method) {
  case sdp_key_x:
    method = k->k_method_name; break;
  case sdp_key_clear:
    method = "clear"; break;
  case sdp_key_base64:
    method = "base64"; break;
  case sdp_key_uri:
    method = "uri"; break;
  case sdp_key_prompt:
    method = "prompt"; break;
  default:
    printing_error(p, "unknown key method (%d)", k->k_method);
    return;
  }

  sdp_printf(p, "k=%s%s%s" CRLF, method,
	     have_material ? ":" : "",
	     have_material ? k->k_material : "");
}

static void print_attributes(sdp_printer_t *p, sdp_attribute_t const *a)
{
  for (;a; a = a->a_next) {
    char const *name = a->a_name;
    char const *value = a->a_value;
    sdp_printf(p, "a=%s%s%s" CRLF, name, value ? ":" : "", value ? value : "");
  }
}

static void
print_attributes_without_mode(sdp_printer_t *p, sdp_attribute_t const *a)
{
  for (;a; a = a->a_next) {
    char const *name = a->a_name;
    char const *value = a->a_value;

    if (su_casematch(name, "inactive") ||
	su_casematch(name, "sendonly") ||
	su_casematch(name, "recvonly") ||
	su_casematch(name, "sendrecv"))
      continue;

    sdp_printf(p, "a=%s%s%s" CRLF, name, value ? ":" : "", value ? value : "");
  }
}

static void print_charset(sdp_printer_t *p, sdp_text_t *charset)
{
  sdp_printf(p, "a=charset%s%s" CRLF, charset ? ":" : "", charset ? charset : "");
}

static void print_media(sdp_printer_t *p,
			sdp_session_t const *sdp,
			sdp_media_t const *m)
{
  char const *media, *proto;
  sdp_rtpmap_t *rm;

  sdp_mode_t session_mode = sdp_sendrecv;

  if (!p->pr_mode_manual)
    session_mode = sdp_attribute_mode(sdp->sdp_attributes, sdp_sendrecv);

  for (;m ; m = m->m_next) {
    switch (m->m_type) {
    case sdp_media_audio:       media = "audio"; break;
    case sdp_media_video:       media = "video"; break;
    case sdp_media_application: media = "application"; break;
    case sdp_media_data:        media = "data"; break;
    case sdp_media_control:     media = "control"; break;
    case sdp_media_message:     media = "message"; break;
    case sdp_media_image  :     media = "image"; break;
    default:                    media = m->m_type_name;
    }

    switch (m->m_proto) {
    case sdp_proto_tcp:   proto = "tcp"; break;
    case sdp_proto_udp:   proto = "udp"; break;
    case sdp_proto_rtp:   proto = "RTP/AVP"; break;
    case sdp_proto_srtp:  proto = "RTP/SAVP"; break;
		//case sdp_proto_extended_srtp:  proto = "RTP/SAVPF"; break;
    case sdp_proto_udptl: proto = "udptl"; break;
    case sdp_proto_msrp:  proto = "TCP/MSRP"; break;
    case sdp_proto_msrps:  proto = "TCP/TLS/MSRP"; break;
    case sdp_proto_tls:   proto = "tls"; break;
    default:              proto = m->m_proto_name; break;
    }

    if (m->m_number_of_ports <= 1)
      sdp_printf(p, "m=%s %u %s", media, m->m_port, proto);
    else
      sdp_printf(p, "m=%s %u/%u %s",
		 media, m->m_port, m->m_number_of_ports, proto);

    if (m->m_rtpmaps) {
      for (rm = m->m_rtpmaps; rm; rm = rm->rm_next) {
	if (rm->rm_any)
	  sdp_printf(p, " *");
	else
	  sdp_printf(p, " %u", (unsigned)rm->rm_pt);
      }
    }
    else if (m->m_format) {
      sdp_list_t *l = m->m_format;
      for (; l; l = l->l_next)
	sdp_printf(p, " %s", l->l_text);
    }
    else {
		/* SDP syntax requires at least one format. */
		/* defaults to "19", or "t38" for image */
		if (m->m_type == sdp_media_image) sdp_printf(p, " t38");
		else sdp_printf(p, " 19");
    }


    sdp_printf(p, CRLF);

    if (m->m_information)
      print_information(p, m->m_information);
    if (m->m_connections)
#ifdef nomore
    if (m->m_connections != sdp->sdp_connection)
#endif
      print_connection_list(p, m->m_connections);
    if (m->m_bandwidths)
      print_bandwidths(p, m->m_bandwidths);
    if (m->m_key)
      print_key(p, m->m_key);

    for (rm = m->m_rtpmaps; rm; rm = rm->rm_next) {
		if (rm->rm_encoding && *rm->rm_encoding && (!rm->rm_predef || p->pr_all_rtpmaps)) {
			sdp_printf(p, "a=rtpmap:%u %s/%lu%s%s" CRLF,
					   rm->rm_pt, rm->rm_encoding, rm->rm_rate,
					   rm->rm_params ? "/" : "",
					   rm->rm_params ? rm->rm_params : "");
		}
		if (rm->rm_fmtp) {
			sdp_printf(p, "a=fmtp:%u %s" CRLF,
					   rm->rm_pt, rm->rm_fmtp);
		}
    }

    if (!p->pr_mode_manual && !m->m_rejected &&
	(m->m_mode != (unsigned int)session_mode || p->pr_mode_always)) {
      switch (m->m_mode) {
      case sdp_inactive:
	sdp_printf(p, "a=inactive" CRLF);
	break;
      case sdp_sendonly:
	sdp_printf(p, "a=sendonly" CRLF);
	break;
      case sdp_recvonly:
	sdp_printf(p, "a=recvonly" CRLF);
	break;
      case sdp_sendrecv:
	sdp_printf(p, "a=sendrecv" CRLF);
	break;
      default:
	break;
      }
    }

    if (p->pr_mode_manual)
      print_attributes(p, m->m_attributes);
    else
      print_attributes_without_mode(p, m->m_attributes);
  }
}

static void print_text_list(sdp_printer_t *p,
			    const char *fmt, sdp_list_t const *l)
{
  for (;l; l = l->l_next) {
    sdp_printf(p, fmt, l->l_text);
  }
}

static void printing_error(sdp_printer_t *p, const char *fmt, ...)
{
  va_list ap;

  if (p->pr_ok) {
    va_start(ap, fmt);
    vsnprintf(p->pr_buffer, p->pr_bsiz, fmt, ap);
    va_end(ap);
  }

  p->pr_ok = 0;
}

static void sdp_printf(sdp_printer_t *p, const char *fmt, ...)
{
  va_list ap;

  while (p->pr_ok) {
    int n;

    va_start(ap, fmt);
    n = vsnprintf(p->pr_buffer + p->pr_used, p->pr_bsiz - p->pr_used, fmt, ap);
    va_end(ap);

    if (n > -1 && (size_t)n < p->pr_bsiz - p->pr_used) {
      p->pr_used += n;
      break;
    }
    else {
      if (p->pr_owns_buffer) {
	p->pr_buffer = su_realloc(p->pr_home, p->pr_buffer, 2 * p->pr_bsiz);
	if (p->pr_buffer) {
	  p->pr_bsiz = 2 * p->pr_bsiz;
	  continue;
	}
	p->pr_owns_buffer = 0;
      }
      else if (p->pr_may_realloc) {
	char *buffer;
	size_t size;
	if (p->pr_bsiz < SDP_BLOCK)
	  size = SDP_BLOCK;
	else
	  size = 2 * p->pr_bsiz;
	buffer = su_alloc(p->pr_home, size);
	if (buffer) {
	  p->pr_owns_buffer = 1;
	  p->pr_buffer = memcpy(buffer, p->pr_buffer, p->pr_bsiz);
	  p->pr_bsiz = size;
	  continue;
	}
      }
      p->pr_ok = 0;
      p->pr_buffer = "Memory exhausted";
    }
  }
}

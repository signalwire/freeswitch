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

#ifndef SIP_HEADER_H
/**Defined when <sofia-sip/sip_header.h> has been included.*/
#define SIP_HEADER_H

/**@file sofia-sip/sip_header.h
 *
 * SIP parser library prototypes.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date  Created: Tue Jun 13 02:58:26 2000 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

#ifndef SU_TAG_H
#include <sofia-sip/su_tag.h>
#endif

#ifndef SIP_H
#include <sofia-sip/sip.h>
#endif

#ifndef MSG_HEADER_H
#include <sofia-sip/msg_header.h>
#endif

#ifndef _STRING_H
#include <string.h>
#endif

SOFIA_BEGIN_DECLS

/** Return a built-in SIP parser object. */
SOFIAPUBFUN msg_mclass_t const *sip_default_mclass(void);

SOFIAPUBFUN int sip_update_default_mclass(msg_mclass_t const *mclass);
SOFIAPUBFUN msg_mclass_t *sip_extend_mclass(msg_mclass_t *input);

/** Check that sip_t is a SIP header structure (not MIME or HTTP). @HIDE */
#define sip_is_sip(sip) ((sip) && (sip)->sip_ident == SIP_PROTOCOL_TAG)

/** Initializer for a SIP header structure. @HIDE */
#define SIP_HDR_INIT(name) {{{ 0, 0, sip_##name##_class }}}

/** Initialize a SIP header structure. @HIDE */
#define SIP_HEADER_INIT(h, sip_class, size)	       \
  ((void)memset((h), 0, (size)),		       \
   (void)(((sip_common_t *)(h))->h_class = (sip_class)),	\
   (h))

/** Serialize headers into the fragment chain. */
SOFIAPUBFUN int sip_serialize(msg_t *msg, sip_t *sip);

/** Encode a SIP message. */
SOFIAPUBFUN issize_t sip_e(sip_t const *sip, int flags, char b[], isize_t size);

/** Test if @a header is a pointer to a SIP header object. */
SOFIAPUBFUN int sip_is_header(sip_header_t const *header);

/** Convert the header @a h to a string allocated from @a home. */
SOFIAPUBFUN char *sip_header_as_string(su_home_t *home,
				       sip_header_t const *h);

/** Add a duplicate of header object to a SIP message. */
SOFIAPUBFUN int sip_add_dup(msg_t *, sip_t *, sip_header_t const *);

/** Add a duplicate of header object to the SIP message. */
SOFIAPUBFUN int sip_add_dup_as(msg_t *msg, sip_t *sip,
			       msg_hclass_t *hc, sip_header_t const *o);

/** Add duplicates of headers to the SIP message. */
SOFIAPUBFUN int sip_add_headers(msg_t *msg, sip_t *sip,
				void const *extra, va_list headers);

/** Add duplicates of headers from taglist to the SIP message. */
SOFIAPUBFUN int sip_add_tl(msg_t *msg, sip_t *sip,
			   tag_type_t tag, tag_value_t value, ...);

/** Add duplicates of headers from taglist to the SIP message. */
SOFIAPUBFUN int sip_add_tagis(msg_t *, sip_t *, tagi_t const **inout_list);

/** Parse a string as a header and add it to the SIP message. */
SOFIAPUBFUN int sip_add_make(msg_t *, sip_t *, msg_hclass_t *hc, char const *s);

/** Convert headers from taglist as URL query. */
SOFIAPUBFUN char *sip_headers_as_url_query(su_home_t *home,
					   tag_type_t tag, tag_value_t value,
					   ...);

/** Convert URL query to a tag list. */
SOFIAPUBFUN tagi_t *sip_url_query_as_taglist(su_home_t *home,
					     char const *query,
					     msg_mclass_t const *parser);

/** Complete SIP message. */
SOFIAPUBFUN int sip_complete_message(msg_t *msg);

/** Clear encoded data. @HIDE */
#define sip_fragment_clear(a) ((a)->h_data = NULL, (a)->h_len = 0)

/* Use __attribute__ to allow argument checking for sip_header_format() */
#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

/** Make a SIP header with formatting provided. */
SOFIAPUBFUN sip_header_t *sip_header_format(su_home_t *home,
					    msg_hclass_t *hc,
					    char const *fmt,
					    ...)
  __attribute__((__format__ (printf, 3, 4)));

/** Return current time */
SOFIAPUBFUN sip_time_t sip_now(void);

SOFIAPUBVAR char const sip_method_name_ack[];
SOFIAPUBVAR char const sip_method_name_bye[];
SOFIAPUBVAR char const sip_method_name_cancel[];
SOFIAPUBVAR char const sip_method_name_invite[];
SOFIAPUBVAR char const sip_method_name_options[];
SOFIAPUBVAR char const sip_method_name_register[];
SOFIAPUBVAR char const sip_method_name_info[];
SOFIAPUBVAR char const sip_method_name_prack[];
SOFIAPUBVAR char const sip_method_name_comet[];
SOFIAPUBVAR char const sip_method_name_message[];
SOFIAPUBVAR char const sip_method_name_subscribe[];
SOFIAPUBVAR char const sip_method_name_notify[];
SOFIAPUBVAR char const sip_method_name_refer[];

/** @internal UDP transport version string. */
SOFIAPUBVAR char const sip_transport_udp[];
/** @internal TCP transport version string. */
SOFIAPUBVAR char const sip_transport_tcp[];
/** @internal SCTP transport version string. */
SOFIAPUBVAR char const sip_transport_sctp[];
/** @internal TLS transport version string. */
SOFIAPUBVAR char const sip_transport_tls[];
/** @internal SIP version string. */
SOFIAPUBVAR char const sip_version_2_0[];

#define SIP_VERSION_CURRENT sip_version_2_0

/** SIP parser version */
SOFIAPUBVAR char const sip_parser_version[];

/** Get SIP service name */
#define SIP_PORT(s) ((s) ? (s) : "5060")

/** Get SIPS service name */
#define SIPS_PORT(s) ((s) ? (s) : "5061")

/** Return string corresponding to the method. */
SOFIAPUBFUN char const *sip_method_name(sip_method_t method, char const *name);

/** Return code corresponding to the method code */
SOFIAPUBFUN sip_method_t sip_method_code(char const *name);

SOFIAPUBVAR char const * const sip_method_names[];

#define SIP_METHOD_NAME(method, name) \
 ((method) == sip_method_unknown ? (name) : sip_method_name(method, name))

#define sip_header_make(h, c, s) \
  ((sip_header_t *)msg_header_make((h), (c), (s)))
#define sip_header_vformat(h, c, f, a) \
  ((sip_header_t *)msg_header_vformat((h), (c), (f), (a)))

SOFIA_END_DECLS
#ifndef SIP_PROTOS_H
#include <sofia-sip/sip_protos.h>
#endif
SOFIA_BEGIN_DECLS

/** Create a request line object. */
SOFIAPUBFUN
sip_request_t *sip_request_create(su_home_t *home,
				  sip_method_t method, const char *name,
				  url_string_t const *url,
				  char const *version);

/** Create a status line object. */
SOFIAPUBFUN
sip_status_t *sip_status_create(su_home_t *home,
				unsigned status,
				char const *phrase,
				char const *version);

/** Create a @CallID header object. */
SOFIAPUBFUN sip_call_id_t *sip_call_id_create(su_home_t *home,
					      char const *domain);

/** Create a @CSeq header object.  */
SOFIAPUBFUN sip_cseq_t *sip_cseq_create(su_home_t *, uint32_t seq,
					unsigned method, char const *name);

/** Create a @Contact header object. */
SOFIAPUBFUN sip_contact_t * sip_contact_create(su_home_t *,
					       url_string_t const *url,
					       char const *param,
					       /* char const *params, */
					       ...);

/** Calculate expiration time of a @Contact header. */
SOFIAPUBFUN sip_time_t sip_contact_expires(sip_contact_t const *m,
					   sip_expires_t const *ex,
					   sip_date_t const *date,
					   sip_time_t def,
					   sip_time_t now);

/** Create a @ContentLength header object. */
SOFIAPUBFUN
sip_content_length_t *sip_content_length_create(su_home_t *, uint32_t n);

/** Create an @Date header object. */
SOFIAPUBFUN sip_date_t *sip_date_create(su_home_t *, sip_time_t t);

/** Create an @Expires header object. */
SOFIAPUBFUN sip_expires_t *sip_expires_create(su_home_t *, sip_time_t delta);

/** Create a @Route header object. */
SOFIAPUBFUN sip_route_t *sip_route_create(su_home_t *home, url_t const *url,
					  url_t const *maddr);

/** Create a @RecordRoute header object. */
SOFIAPUBFUN sip_record_route_t *sip_record_route_create(su_home_t *,
							url_t const *rq_url,
							url_t const *m_url);

/** Create a @From header object. */
SOFIAPUBFUN sip_from_t *sip_from_create(su_home_t *, url_string_t const *url);

SOFIAPUBFUN int sip_from_tag(su_home_t *, sip_from_t *from, char const *tag);

/** Create a @To header object. */
SOFIAPUBFUN sip_to_t *sip_to_create(su_home_t *, url_string_t const *url);

SOFIAPUBFUN int sip_to_tag(su_home_t *, sip_to_t *to, char const *tag);

/** Create a @Via object. */
SOFIAPUBFUN sip_via_t *sip_via_create(su_home_t *h,
				      char const *host,
				      char const *port,
				      char const *transport,
				      /* char const *params */
				      ...);

/** Get transport protocol name. */
#if SU_HAVE_INLINE
su_inline char const *sip_via_transport(sip_via_t const *v)
{
  char const *tp = v->v_protocol;
  if (tp) {
    tp = strchr(tp, '/');
    if (tp) {
      tp = strchr(tp + 1, '/');
      if (tp)
	return tp + 1;
    }
  }
  return NULL;
}
#else
char const *sip_via_transport(sip_via_t const *v);
#endif

SOFIAPUBFUN char const *sip_via_port(sip_via_t const *v, int *using_rport);

SOFIAPUBFUN
sip_payload_t *sip_payload_create(su_home_t *, void const *data, isize_t len);

/**@ingroup sip_payload
 *
 * Initialize a SIP payload structure with pointer to data and its length.
 *
 * The SIP_PAYLOAD_INIT2() macro initializes a #sip_payload_t header
 * structure with a pointer to data and its length in octets. For
 * instance,
 * @code
 *  sip_payload_t txt_payload = SIP_PAYLOAD_INIT2(txt, strlen(txt));
 * @endcode
 *
 * The SIP_PAYLOAD_INIT2() macro can be used when creating a new payload
 * from heap is not required, for instance, when the resulting payload
 * structure is immediately copied.
 *
 * @HIDE
 */
#define SIP_PAYLOAD_INIT2(data, length) \
  {{{ 0, 0, sip_payload_class, data, length }, NULL, data, length }}

/** Create a SIP separator line structure. */
SOFIAPUBFUN sip_separator_t *sip_separator_create(su_home_t *home);

/** Check that a required feature is supported. */
SOFIAPUBFUN
sip_unsupported_t *sip_has_unsupported(su_home_t *,
				       sip_supported_t const *support,
				       sip_require_t const *require);

SOFIAPUBFUN
sip_unsupported_t *sip_has_unsupported2(su_home_t *,
					sip_supported_t const *support,
					sip_require_t const *by_require,
					sip_require_t const *require);

SOFIAPUBFUN
sip_unsupported_t *
sip_has_unsupported_any(su_home_t *,
			sip_supported_t const *support,
			sip_require_t const *by_require,
			sip_proxy_require_t const *by_proxy_require,
			sip_require_t const *require,
			sip_require_t const *require2,
			sip_require_t const *require3);

/** Check that a feature is supported. */
SOFIAPUBFUN
int sip_has_supported(sip_supported_t const *support, char const *feature);

/** Check that a feature is in the list. */
SOFIAPUBFUN
int sip_has_feature(msg_list_t const *supported, char const *feature);

/** Return true if the method is listed in @Allow header. */
SOFIAPUBFUN int sip_is_allowed(sip_allow_t const *allow,
			       sip_method_t method, char const *name);

/** Check if the well-known method is listed in @Allow header. @NEW_1_12_6. */
#define SIP_IS_ALLOWED(allow, method) \
  (sip_method_unknown < (method) && (method) < 32 && \
   (allow) && ((allow)->k_bitmap & (1 << (method))) != 0)

/**
 * Bitmasks for header classifications.
 *
 * If parsing of a particular header fails, the error bits in #msg_t are
 * updated. The error bits can be obtained via msg_extract_errors() after
 * parsing. The header-specific bits are stored along with the
 * @ref msg_hclass_t "header class" in the #msg_href_t structure, found in
 * the parser tables of the #msg_mclass_t object.
 *
 * @sa NTATAG_BAD_REQ_MASK(), NTATAG_BAD_RESP_MASK(),
 * #msg_mclass_t, struct #msg_mclass_s, msg_mclass_clone(),
 * msg_mclass_insert_with_mask(),
 * #msg_href_t, struct #msg_href_s, msg_mclass_insert().
 */
enum sip_bad_mask {
  /** Bit marking essential headers in a request message.
   *
   * @ref sip_request \"request line\"", @From, @To, @CSeq, @CallID,
   * @ContentLength, @Via
   */
  sip_mask_request = (1 << 0),

  /** Bit marking essential headers in a response message.
   *
   * @ref sip_status \"status line\"", @From, @To, @CSeq, @CallID,
   * @ContentLength, @Via
   */
  sip_mask_response = (1 << 1),

  /** Bit marking essential headers for User-Agent.
   *
   * @ContentType, @ContentDisposition, @ContentEncoding, @Supported,
   * @Contact, @Require, and @RecordRoute.
   */
  sip_mask_ua = (1 << 2),

  /** Bit marking essential headers for proxy server.
   *
   * @Route, @MaxForwards, @ProxyRequire, @ProxyAuthorization, @Supported,
   * @Contact, and @RecordRoute.
   */
  sip_mask_proxy = (1 << 3),

  /** Bit marking essential headers for registrar server.
   *
   * @MinExpires, @Authorization, @Path, @Supported, @Contact, @Require, and
   * @Expires.
   *
   */
  sip_mask_registrar = (1 << 4),

  /** Bit marking essential headers for 100rel extension.
   *
   * @RAck and @RSeq.
   *
   * @sa @RFC3262.
   */
  sip_mask_100rel = (1 << 5),

  /** Bit marking essential headers for SIP events.
   *
   * @Event, @Expires, and @SubscriptionState.
   *
   * @sa @RFC3265.
   */
  sip_mask_events = (1 << 6),

  /** Bit marking essential headers for session timer extension.
   *
   * @SessionExpires, and @MinSE.
   *
   * @RFC4028
   */
  sip_mask_timer = (1 << 7),

  /** Bit marking essential headers for privacy extension.
   *
   * @Privacy.
   *
   * @sa @RFC3323
   */
  sip_mask_privacy = (1 << 8),

  /** Bit marking essential headers for caller preference extension.
   *
   * @RequestDisposition, @AcceptContact, and @RejectContact.
   *
   * @sa @RFC3841.
   */
  sip_mask_pref = (1 << 9),

  /** Bit marking essential headers for PUBLISH servers and clients.
   *
   * @SIPETag, and @SIPIfMatch.
   *
   * @sa @RFC3903.
   */
  sip_mask_publish = (1 << 10)

  /* NOTE:
   * When adding bits, please update nta_agent_create() and
   * NTATAG_BAD_RESP_MASK()/NTATAG_BAD_REQ_MASK() documentation.
   */
};

/* ------------------------------------------------------------------------- */

/* Here are @deprecated functions and names for compatibility */

/** Encode a SIP header field (name: contents CRLF). */
SOFIAPUBFUN issize_t sip_header_e(char[], isize_t, sip_header_t const *, int);

/** Decode a SIP header string (name: contents CRLF?). */
SOFIAPUBFUN
sip_header_t *sip_header_d(su_home_t *, msg_t const *, char const *);

/** Encode contents of a SIP header field. */
SOFIAPUBFUN issize_t sip_header_field_e(char[], isize_t, sip_header_t const *, int);

/** Decode the string containing header field */
SOFIAPUBFUN issize_t sip_header_field_d(su_home_t *, sip_header_t *, char *, isize_t);

/** Calculate the size of a SIP header and associated memory areas. */
SOFIAPUBFUN isize_t sip_header_size(sip_header_t const *h);

/** Duplicate (deep copy) a SIP header or whole list. */
SOFIAPUBFUN sip_header_t *sip_header_dup(su_home_t *, sip_header_t const *);

/** Copy a SIP header or whole list. */
SOFIAPUBFUN sip_header_t *sip_header_copy(su_home_t *, sip_header_t const *o);

/** Add an event to @AllowEvents header. */
SOFIAPUBFUN int sip_allow_events_add(su_home_t *,
				     sip_allow_events_t *ae,
				     char const *e);

/** Add a parameter to a @Contact header object. */
SOFIAPUBFUN int sip_contact_add_param(su_home_t *, sip_contact_t *,
				      char const *param);

SOFIAPUBFUN int sip_to_add_param(su_home_t *, sip_to_t *, char const *);

SOFIAPUBFUN int sip_from_add_param(su_home_t *, sip_from_t *, char const *);

/** Add a parameter to a @Via header object. */
SOFIAPUBFUN int sip_via_add_param(su_home_t *, sip_via_t *, char const *);

#define sip_from_make_url     sip_from_create
#define sip_to_make_url       sip_to_create
#define sip_params_find       msg_params_find

SOFIA_END_DECLS

#endif

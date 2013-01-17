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

/**@ingroup sip_parser
 * @CFILE sip_parser.c
 *
 * SIP parser.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Thu Oct  5 14:01:24 2000 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include <sofia-sip/su_tagarg.h>
#include <sofia-sip/su_string.h>
#include "sofia-sip/sip_parser.h"
#include <sofia-sip/msg_mclass.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffffU)
#endif

/** Version of the SIP module */
char const sip_parser_version[] = VERSION;

/** SIP version 2.0. */
char const sip_version_2_0[] = "SIP/2.0";

/** Default message class */
extern msg_mclass_t sip_mclass[];

static msg_mclass_t const *_default = sip_mclass;

/** Return a built-in SIP parser object. */
msg_mclass_t const *sip_default_mclass(void)
{
  return _default;
}

/** Update the default SIP parser.
 *
 * Use the extended SIP parser as default one.
 *
 * If the applications want to use headers added after @VERSION_1_12_5,
 * they should call this function before doing any other initialization, e.g.,
 * @code
 *   su_init();
 *   if (sip_update_default_mclass(sip_extend_mclass(NULL)) < 0) {
 *     su_deinit();
 *     exit(2);
 *   }
 * @endcode
 *
 * The default parser is not extended because it may break the old
 * applications looking for extension headers from sip_unknown list.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @sa sip_extend_mclass()
 *
 * @NEW_1_12_7.
 */
int sip_update_default_mclass(msg_mclass_t const *mclass)
{
  if (mclass == NULL)
    return -1;
  _default = mclass;
  return 0;
}

/**Extend SIP parser class with extension headers.
 *
 * Extend given SIP parser class with extension headers. If the given parser
 * (message class) is the default one or NULL, make a clone of default
 * parser before extending it.
 *
 * @param input pointer to a SIP message class (may be NULL)
 *
 * @return Pointer to extended mclass, or NULL upon an error.
 *
 * @sa
 * @AlertInfo,
 * @ReplyTo,
 * @RemotePartyId,
 * @PAssertedIdentity,
 * @PPreferredIdentity,
 * @SuppressBodyIfMatch,
 * @SuppressNotifyIfMatch
 *
 * @NEW_1_12_7.
 */
msg_mclass_t *sip_extend_mclass(msg_mclass_t *input)
{
  msg_mclass_t *mclass;

  if (input == NULL || input == _default)
    mclass = msg_mclass_clone(_default, 0, 0);
  else
    mclass = input;

  if (mclass) {
    extern msg_hclass_t * const sip_extensions[];
    int i;

    for (i = 0; sip_extensions[i]; i++) {
      msg_hclass_t *hclass = sip_extensions[i];
      if (mclass->mc_unknown != msg_find_hclass(mclass, hclass->hc_name, NULL))
	continue;

      if (msg_mclass_insert_header(mclass, hclass, 0) < 0) {
	if (input != mclass)
	  free(mclass);
	return mclass = NULL;
      }
    }
  }

  return mclass;
}

/** Extract the SIP message body, including separator line.
 *
 * @param msg  message object [IN]
 * @param sip  public SIP message structure [IN/OUT]
 * @param b    buffer containing unparsed data [IN]
 * @param bsiz buffer size [IN]
 * @param eos  true if buffer contains whole message [IN]
 *
 * @retval -1 error
 * @retval 0  cannot proceed
 * @retval m
 */
issize_t sip_extract_body(msg_t *msg, sip_t *sip, char b[], isize_t bsiz, int eos)
{
  ssize_t m = 0;
  size_t body_len;

  if (!(sip->sip_flags & MSG_FLG_BODY)) {
    /* We are looking at a potential empty line */
    m = msg_extract_separator(msg, (msg_pub_t *)sip, b, bsiz, eos);
    if (m <= 0)
      return m;
    sip->sip_flags |= MSG_FLG_BODY;
    b += m;
    bsiz -= m;
  }

  if (sip->sip_content_length)
    body_len = sip->sip_content_length->l_length;
  else if (MSG_IS_MAILBOX(sip->sip_flags)) /* message fragments */
    body_len = 0;
  else if (eos)
    body_len = bsiz;
  else if (bsiz == 0)
    return m;
  else
    return -1;

  if (body_len == 0) {
    sip->sip_flags |= MSG_FLG_COMPLETE;
    return m;
  }

  if (m)
    return m;

  if (eos && body_len > bsiz) {
    sip->sip_flags |= MSG_FLG_TRUNC | MSG_FLG_ERROR;
    return bsiz;
  }

  if ((m = msg_extract_payload(msg, (msg_pub_t *)sip,
			       NULL, body_len, b, bsiz, eos)) == -1)
    return -1;

  sip->sip_flags |= MSG_FLG_FRAGS;
  if (bsiz >= body_len)
    sip->sip_flags |= MSG_FLG_COMPLETE;

  return m;
}

/** Parse SIP version.
 *
 * Parse a SIP version string. Update the
 * pointer at @a ss to first non-LWS character after the version string.
 *
 * @param ss   string to be parsed [IN/OUT]
 * @param ver  value result for version [OUT]
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int sip_version_d(char **ss, char const **ver)
{
  char *s = *ss;
  char const *result;
  size_t const version_size = sizeof(sip_version_2_0) - 1;

  if (su_casenmatch(s, sip_version_2_0, version_size) &&
      !IS_TOKEN(s[version_size])) {
    result = sip_version_2_0;
    s += version_size;
  }
  else {
    /* Version consists of two tokens, separated by / */
    size_t l1 = 0, l2 = 0, n;

    result = s;

    l1 = span_token(s);
    for (n = l1; IS_LWS(s[n]); n++)
      {}
    if (s[n] == '/') {
      for (n++; IS_LWS(s[n]); n++)
        {}
      l2 = span_token(s + n);
      n += l2;
    }

    if (l1 == 0 || l2 == 0)
      return -1;

    /* If there is extra ws between tokens, compact version */
    if (n > l1 + 1 + l2) {
      s[l1] = '/';
      memmove(s + l1 + 1, s + n - l2, l2);
      s[l1 + 1 + l2] = 0;

      /* Compare again with compacted version */
      if (su_casematch(s, sip_version_2_0))
	result = sip_version_2_0;
    }

    s += n;
  }

  while (IS_WS(*s)) *s++ = '\0';

  *ss = s;

  if (ver)
    *ver = result;

  return 0;
}

/** Calculate extra space required by version string */
isize_t sip_version_xtra(char const *version)
{
  if (version == SIP_VERSION_CURRENT)
    return 0;
  return MSG_STRING_SIZE(version);
}

/** Duplicate a transport string */
void sip_version_dup(char **pp, char const **dd, char const *s)
{
  if (s == SIP_VERSION_CURRENT)
    *dd = s;
  else
    MSG_STRING_DUP(*pp, *dd, s);
}

char const sip_method_name_invite[] =  	 "INVITE";
char const sip_method_name_ack[] =     	 "ACK";
char const sip_method_name_cancel[] =  	 "CANCEL";
char const sip_method_name_bye[] =     	 "BYE";
char const sip_method_name_options[] = 	 "OPTIONS";
char const sip_method_name_register[] =  "REGISTER";
char const sip_method_name_info[] =      "INFO";
char const sip_method_name_prack[] =     "PRACK";
char const sip_method_name_update[] =    "UPDATE";
char const sip_method_name_message[] =   "MESSAGE";
char const sip_method_name_subscribe[] = "SUBSCRIBE";
char const sip_method_name_notify[] =    "NOTIFY";
char const sip_method_name_refer[] =     "REFER";
char const sip_method_name_publish[] =   "PUBLISH";

/** Well-known SIP method names. */
char const * const sip_method_names[] = {
  "<UNKNOWN>",
  sip_method_name_invite,
  sip_method_name_ack,
  sip_method_name_cancel,
  sip_method_name_bye,
  sip_method_name_options,
  sip_method_name_register,
  sip_method_name_info,
  sip_method_name_prack,
  sip_method_name_update,
  sip_method_name_message,
  sip_method_name_subscribe,
  sip_method_name_notify,
  sip_method_name_refer,
  sip_method_name_publish,
  /* If you add something here, add also them to sip_method_d! */
  NULL
};

/** Get canonic method name. */
char const *sip_method_name(sip_method_t method, char const *name)
{
  const size_t N = sizeof(sip_method_names)/sizeof(sip_method_names[0]);
  if (method > 0 && (size_t)method < N)
    return sip_method_names[method];
  else if (method == 0)
    return name;
  else
    return NULL;
}

/**Parse a SIP method name.
 *
 * Parse a SIP method name and return a code corresponding to the method.
 * The address of the first non-LWS character after method name is stored in
 * @a *ss.
 *
 * @param ss    pointer to pointer to string to be parsed
 * @param return_name  value-result parameter for method name
 *
 * @note
 * If there is no whitespace after method name, the value in @a *return_name
 * may not be NUL-terminated.  The calling function @b must NUL terminate
 * the value by setting the @a **ss to NUL after first examining its value.
 *
 * @return The method code if method
 * was identified, 0 (sip_method_unknown()) if method is not known, or @c -1
 * (sip_method_invalid()) if an error occurred.
 *
 * If the value-result argument @a return_name is not @c NULL,
 * a pointer to the method name is stored to it.
 */
sip_method_t sip_method_d(char **ss, char const **return_name)
{
  char *s = *ss, c = *s;
  char const *name;
  int code = sip_method_unknown;
  size_t n = 0;

#define MATCH(s, m) (strncmp(s, m, n = sizeof(m) - 1) == 0)

  switch (c) {
  case 'A': if (MATCH(s, "ACK")) code = sip_method_ack; break;
  case 'B': if (MATCH(s, "BYE")) code = sip_method_bye; break;
  case 'C':
    if (MATCH(s, "CANCEL"))
      code = sip_method_cancel;
    break;
  case 'I':
    if (MATCH(s, "INVITE"))
      code = sip_method_invite;
    else if (MATCH(s, "INFO"))
      code = sip_method_info;
    break;
  case 'M': if (MATCH(s, "MESSAGE")) code = sip_method_message; break;
  case 'N': if (MATCH(s, "NOTIFY")) code = sip_method_notify; break;
  case 'O': if (MATCH(s, "OPTIONS")) code = sip_method_options; break;
  case 'P':
    if (MATCH(s, "PRACK")) code = sip_method_prack;
    else if (MATCH(s, "PUBLISH")) code = sip_method_publish;
    break;
  case 'R':
    if (MATCH(s, "REGISTER"))
      code = sip_method_register;
    else if (MATCH(s, "REFER"))
      code = sip_method_refer;
    break;
  case 'S':
    if (MATCH(s, "SUBSCRIBE"))
      code = sip_method_subscribe;
    break;
  case 'U':
    if (MATCH(s, "UPDATE"))
      code = sip_method_update;
    break;
  }

#undef MATCH

  if (IS_NON_WS(s[n]))
    /* Unknown method */
    code = sip_method_unknown;

  if (code == sip_method_unknown) {
    name = s;
    for (n = 0; IS_UNRESERVED(s[n]); n++)
      ;
    if (s[n]) {
      if (!IS_LWS(s[n]))
	return sip_method_invalid;
      if (return_name)
	s[n++] = '\0';
    }
  }
  else {
    name = sip_method_names[code];
  }

  while (IS_LWS(s[n]))
    n++;

  *ss = (s + n);
  if (return_name) *return_name = name;

  return (sip_method_t)code;
}

/** Get method enum corresponding to method name */
sip_method_t sip_method_code(char const *name)
{
  /* Note that sip_method_d() does not change string if return_name is NULL */
  return sip_method_d((char **)&name, NULL);
}

char const sip_transport_udp[] = "SIP/2.0/UDP";
char const sip_transport_tcp[] = "SIP/2.0/TCP";
char const sip_transport_sctp[] = "SIP/2.0/SCTP";
char const sip_transport_ws[] = "SIP/2.0/WS";
char const sip_transport_wss[] = "SIP/2.0/WSS";
char const sip_transport_tls[] = "SIP/2.0/TLS";

/** Decode transport */
issize_t sip_transport_d(char **ss, char const **ttransport)
{
  char const *transport;
  char *pn, *pv, *pt;
  size_t pn_len, pv_len, pt_len;
  char *s = *ss;

#define TRANSPORT_MATCH(t) \
  (su_casenmatch(s + 7, t + 7, (sizeof t) - 8) && \
   (!s[sizeof(t) - 1] || IS_LWS(s[sizeof(t) - 1]))	\
   && (transport = t, s += sizeof(t) - 1))

  if (!su_casenmatch(s, "SIP/2.0", 7) ||
      (!TRANSPORT_MATCH(sip_transport_udp) &&
       !TRANSPORT_MATCH(sip_transport_tcp) &&
       !TRANSPORT_MATCH(sip_transport_sctp) &&
       !TRANSPORT_MATCH(sip_transport_ws) &&
       !TRANSPORT_MATCH(sip_transport_wss) &&
       !TRANSPORT_MATCH(sip_transport_tls))) {
    /* Protocol name */
    transport = pn = s;
    skip_token(&s);
    pn_len = s - pn;
    skip_lws(&s);
    if (pn_len == 0 || *s++ != '/') return -1;
    skip_lws(&s);

    /* Protocol version */
    pv = s;
    skip_token(&s);
    pv_len = s - pv;
    skip_lws(&s);
    if (pv_len == 0 || *s++ != '/') return -1;
    skip_lws(&s);

    /* Transport protocol */
    pt = s;
    skip_token(&s);
    pt_len = s - pt;
    if (pt_len == 0) return -1;

    /* Remove whitespace between protocol name and version */
    if (pn + pn_len + 1 != pv) {
      pn[pn_len] = '/';
      pv = memmove(pn + pn_len + 1, pv, pv_len);
    }

    /* Remove whitespace between protocol version and transport */
    if (pv + pv_len + 1 != pt) {
      pv[pv_len] = '/';
      pt = memmove(pv + pv_len + 1, pt, pt_len);
      pt[pt_len] = '\0';

      /* extra whitespace? */
      if (su_casematch(transport, sip_transport_udp))
	transport = sip_transport_udp;
      else if (su_casematch(transport, sip_transport_tcp))
	transport = sip_transport_tcp;
      else if (su_casematch(transport, sip_transport_sctp))
	transport = sip_transport_sctp;
      else if (su_casematch(transport, sip_transport_ws))
	transport = sip_transport_ws;
      else if (su_casematch(transport, sip_transport_wss))
	transport = sip_transport_wss;
      else if (su_casematch(transport, sip_transport_tls))
	transport = sip_transport_tls;
    }
  }

  if (IS_LWS(*s)) { *s++ = '\0'; skip_lws(&s); }
  *ss = s;
  *ttransport = transport;
  return 0;
}

/** Calculate extra space required by sip_transport_dup() */
isize_t sip_transport_xtra(char const *transport)
{
  if (transport == sip_transport_udp ||
      transport == sip_transport_tcp ||
      transport == sip_transport_sctp ||
      transport == sip_transport_ws ||
      transport == sip_transport_wss ||
      transport == sip_transport_tls ||
      su_casematch(transport, sip_transport_udp) ||
      su_casematch(transport, sip_transport_tcp) ||
      su_casematch(transport, sip_transport_sctp) ||
      su_casematch(transport, sip_transport_ws) ||
      su_casematch(transport, sip_transport_wss) ||
      su_casematch(transport, sip_transport_tls))
    return 0;

  return MSG_STRING_SIZE(transport);
}

/** Duplicate a transport string */
void sip_transport_dup(char **pp, char const **dd, char const *s)
{
  if (s == sip_transport_udp)
    *dd = s;
  else if (s == sip_transport_tcp)
    *dd = s;
  else if (s == sip_transport_sctp)
    *dd = s;
  else if (s == sip_transport_tls)
    *dd = s;
  else if (s == sip_transport_ws)
    *dd = s;
  else if (s == sip_transport_wss)
    *dd = s;
  else if (su_casematch(s, sip_transport_udp))
    *dd = sip_transport_udp;
  else if (su_casematch(s, sip_transport_tcp))
    *dd = sip_transport_tcp;
  else if (su_casematch(s, sip_transport_sctp))
    *dd = sip_transport_sctp;
  else if (su_casematch(s, sip_transport_tls))
    *dd = sip_transport_tls;
  else if (su_casematch(s, sip_transport_ws))
    *dd = sip_transport_ws;
  else if (su_casematch(s, sip_transport_wss))
    *dd = sip_transport_wss;
  else
    MSG_STRING_DUP(*pp, *dd, s);
}

/** Parse SIP <word "@" word> construct used in @CallID. */
char *sip_word_at_word_d(char **ss)
{
  char *rv = *ss, *s0 = *ss;

  skip_word(ss);
  if (s0 == *ss)
    return NULL;
  if (**ss == '@') {
    (*ss)++;
    s0 = *ss;
    skip_word(ss);
    if (s0 == *ss)
      return NULL;
  }
  if (IS_LWS(**ss))
    (*ss)++;
  skip_lws(ss);

  return rv;
}

/**Add message separator, then test if message is complete.
 *
 * Add sip_content_length and sip_separator if they are missing.
 * The test that all necessary message components ( @From, @To,
 * @CSeq, @CallID, @ContentLength and message separator are present.
 *
 * @retval 0 when successful
 * @retval -1 upon an error: headers are missing and they could not be added
 */
int sip_complete_message(msg_t *msg)
{
  sip_t *sip = sip_object(msg);
  su_home_t *home = msg_home(msg);
  size_t len = 0;
  ssize_t mplen;

  if (sip == NULL)
    return -1;

  if (!sip->sip_separator)
    sip->sip_separator = sip_separator_create(msg_home(msg));

  if (sip->sip_multipart) {
    sip_content_type_t *c = sip->sip_content_type;
    msg_multipart_t *mp = sip->sip_multipart;
    sip_common_t *head;

    if (!c || msg_multipart_complete(msg_home(msg), c, mp) < 0)
      return -1;

    if (sip->sip_payload)
      head = sip->sip_payload->pl_common;
    else
      head = sip->sip_separator->sep_common;

    if (!head || !msg_multipart_serialize(&head->h_succ, mp))
      return -1;

    mplen = msg_multipart_prepare(msg, mp, sip->sip_flags);
    if (mplen == -1)
      return -1;
    len = (size_t)mplen;
  }

  if (sip->sip_payload)
    len += sip->sip_payload->pl_len;

  if (len > UINT32_MAX)
    return -1;

  if (!sip->sip_content_length) {
    msg_header_insert(msg, (msg_pub_t *)sip, (msg_header_t*)
		      sip_content_length_create(home, (uint32_t)len));
  }
  else {
    if (sip->sip_content_length->l_length != len) {
      sip->sip_content_length->l_length = (uint32_t)len;
      sip_fragment_clear(sip->sip_content_length->l_common);
    }
  }

  if (!sip->sip_cseq ||
      !sip->sip_call_id ||
      !sip->sip_to ||
      !sip->sip_from ||
      !sip->sip_separator ||
      !sip->sip_content_length)
    return -1;

  return 0;
}

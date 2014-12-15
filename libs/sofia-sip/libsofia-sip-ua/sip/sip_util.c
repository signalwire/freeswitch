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

/**@CFILE sip_util.c
 *
 * SIP utility functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Tue Jun 13 02:57:51 2000 ppessi
 */

#include "config.h"

/* Avoid casting sip_t to msg_pub_t and sip_header_t to msg_header_t */
#define MSG_PUB_T       struct sip_s
#define MSG_HDR_T       union sip_header_u

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_strlst.h>
#include <sofia-sip/su_string.h>

#include "sofia-sip/sip_parser.h"
#include <sofia-sip/sip_header.h>
#include <sofia-sip/sip_util.h>
#include <sofia-sip/sip_status.h>

#include <sofia-sip/bnf.h>
#include <sofia-sip/hostdomain.h>


#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>

/**Compare two SIP addresses ( @From or @To headers).
 *
 * @retval nonzero if matching.
 * @retval zero if not matching.
 */
int sip_addr_match(sip_addr_t const *a, sip_addr_t const *b)
{
  return
    (a->a_tag == NULL || b->a_tag == NULL ||
     su_casematch(a->a_tag, b->a_tag))
    &&
    su_casematch(a->a_host, b->a_host)
    &&
    su_strmatch(a->a_user, b->a_user)
    &&
    su_strmatch(a->a_url->url_scheme, b->a_url->url_scheme);
}


/**@ingroup sip_contact
 *
 * Create a contact header.
 *
 * Create a @Contact header object with the given URL and list of parameters.
 *
 * @param home      memory home
 * @param url       URL (string or pointer to url_t)
 * @param p,...     NULL-terminated list of @Contact parameters
 *
 * @return
 * A pointer to newly created @Contact header object when successful or NULL
 * upon an error.
 *
 */
sip_contact_t * sip_contact_create(su_home_t *home,
				   url_string_t const *url,
				   char const *p, ...)
{
  su_strlst_t *l;
  su_home_t *lhome;
  sip_contact_t *m;

  if (url == NULL)
    return NULL;

  l = su_strlst_create_with(NULL, "<", NULL), lhome = su_strlst_home(l);
  if (l == NULL)
    return NULL;

  if (url_is_string(url))
    su_strlst_append(l, (char const *)url);
  else
    su_strlst_append(l, url_as_string(lhome, url->us_url));

  su_strlst_append(l, ">");

  if (p) {
    va_list ap;
    va_start(ap, p);

    for (; p; p = va_arg(ap, char const *)) {
      su_strlst_append(l, ";");
      su_strlst_append(l, p);
    }

    va_end(ap);
  }

  m = sip_contact_make(home, su_strlst_join(l, lhome, ""));

  su_strlst_destroy(l);

  return m;
}

/** Convert a @Via header to @Contact header.
 *
 * The @Contact URI will contain the port number if needed. If transport
 * protocol name starts with "TLS", "SIPS:" URI schema is used. Transport
 * parameter is included in the URI unless the transport protocol is UDP.
 *
 * @param home      memory home
 * @param v         @Via header field structure
 *                  (with <sent-protocol> and <sent-by> parameters)
 * @param user      username for @Contact URI (may be NULL)
 *
 * @retval contact header structure
 * @retval NULL upon an error
 *
 * @sa sip_contact_create_from_via_with_transport(),
 *     sip_contact_string_from_via()
 */
sip_contact_t *
sip_contact_create_from_via(su_home_t *home,
			    sip_via_t const *v,
			    char const *user)
{
  const char *tp;

  if (!v) return NULL;

  tp = v->v_protocol;

  if (tp == sip_transport_udp ||
      su_casematch(tp, sip_transport_udp))  /* Default is UDP */
    tp = NULL;

  return sip_contact_create_from_via_with_transport(home, v, user, tp);
}

/** Convert a @Via header to @Contact header.
 *
 * The @Contact URI will contain the port number and transport parameters if
 * needed. If transport protocol name starts with "TLS", "SIPS:" URI schema
 * is used.
 *
 * @param home      memory home
 * @param v         @Via header field structure
 *                  (with <sent-by> parameter containing host and port)
 * @param user      username for @Contact URI (may be NULL)
 * @param transport transport name for @Contact URI (may be NULL)
 *
 * @retval contact header structure
 * @retval NULL upon an error
 *
 * @sa sip_contact_create_from_via(), sip_contact_string_from_via()
 */
sip_contact_t *
sip_contact_create_from_via_with_transport(su_home_t *home,
					   sip_via_t const *v,
					   char const *user,
					   char const *transport)
{
  char *s = sip_contact_string_from_via(NULL, v, user, transport);
  sip_contact_t *m = sip_contact_make(home, s);
  su_free(NULL, s);
  return m;
}

/** Convert a @Via header to @Contact URL string.
 *
 * The @Contact URI will contain the port number and transport parameters if
 * needed. If transport protocol name starts with "TLS", "SIPS:" URI schema
 * is used.
 *
 * The contact URI string returned will always have angle brackets ("<" and
 * ">") around it.
 *
 * @param home      memory home
 * @param v         @Via header field structure
 *                  (with <sent-by> parameter containing host and port)
 * @param user      username for @Contact URI (may be NULL)
 * @param transport transport name for @Contact URI (may be NULL)
 *
 * @retval string containing Contact URI with angle brackets
 * @retval NULL upon an error
 */
char *
sip_contact_string_from_via(su_home_t *home,
			    sip_via_t const *v,
			    char const *user,
			    char const *transport)
{
  const char *host, *port, *maddr, *comp;
  char const *scheme = "sip:";
  int one = 1;
  char _transport[16];

  if (!v) return NULL;

  host = v->v_host;
  if (v->v_received)
    host = v->v_received;
  port = sip_via_port(v, &one);
  maddr = v->v_maddr;
  comp = v->v_comp;

  if (host == NULL)
    return NULL;

  if (sip_transport_has_tls(v->v_protocol) ||
      sip_transport_has_tls(transport)) {
    scheme = "sips:";
    if (port && strcmp(port, SIPS_DEFAULT_SERV) == 0)
      port = NULL;
    if (port || host_is_ip_address(host))
      transport = NULL;
  }
  else if (port && strcmp(port, SIP_DEFAULT_SERV) == 0 &&
	   (host_is_ip_address(host) || host_has_domain_invalid(host))) {
    port = NULL;
  }

  if (su_casenmatch(transport, "SIP/2.0/", 8))
    transport += 8;

  /* Make transport parameter lowercase */
  if (transport && strlen(transport) < (sizeof _transport)) {
    char *s = strcpy(_transport, transport);
    short c;

    for (s = _transport; (c = *s) && c != ';'; s++)
      if (isupper(c))
	*s = tolower(c);

    transport = _transport;
  }

  return su_strcat_all(home,
		       "<",
		       scheme,
		       user ? user : "", user ? "@" : "",
		       host,
		       SIP_STRLOG(":", port),
		       SIP_STRLOG(";transport=", transport),
		       SIP_STRLOG(";maddr=", maddr),
		       SIP_STRLOG(";comp=", comp),
		       ">",
		       NULL);
}

/** Check if tranport name refers to TLS */
int sip_transport_has_tls(char const *transport_name)
{
  if (!transport_name)
    return 0;

  if (transport_name == sip_transport_tls)
    return 1;

  /* transport name starts with TLS or SIP/2.0/TLS */
  return
    su_casenmatch(transport_name, "TLS", 3) ||
    su_casenmatch(transport_name, sip_transport_tls, 11);
}

/**Perform sanity check on a SIP message
 *
 * Check that the SIP message has all the mandatory fields.
 *
 * @param sip SIP message to be checked
 *
 * @return
 * When the SIP message fulfills the minimum requirements, return zero,
 * otherwise a negative status code.
 */
int
sip_sanity_check(sip_t const *sip)
{
  if (!sip ||
      !((sip->sip_request != NULL) ^ (sip->sip_status != NULL)) ||
      !sip->sip_to ||
      !sip->sip_from ||
      !sip->sip_call_id ||
      !sip->sip_cseq ||
      !sip->sip_via ||
      (sip->sip_flags & MSG_FLG_TRUNC))
    return -1;  /* Bad request */

  if (sip->sip_request) {
    url_t const *ruri = sip->sip_request->rq_url;

    switch (ruri->url_type) {
    case url_invalid:
      return -1;

    case url_sip: case url_sips: case url_im: case url_pres:
      if (!ruri->url_host || strlen(ruri->url_host) == 0)
	return -1;
      break;

    case url_tel:
      if (!ruri->url_user || strlen(ruri->url_user) == 0)
	return -1;
      break;
    }

    if (sip->sip_request->rq_method != sip->sip_cseq->cs_method)
      return -1;

    if (sip->sip_request->rq_method == sip_method_unknown &&
	!su_strmatch(sip->sip_request->rq_method_name,
		     sip->sip_cseq->cs_method_name))
      return -1;
  }

  return 0;
}

/** Decode a string containg header field.
 *
 * The header object is initialized with the contents of the string. The
 * string is modified when parsing. The home is used to allocate extra
 * memory required when parsing, e.g., for parameter list or when there
 * string contains multiple header fields.
 *
 * @deprecated
 * Use msg_header_make() or header-specific make functions, e.g.,
 * sip_via_make().
 *
 * @retval 0 when successful
 * @retval -1 upon an error.
 */
issize_t sip_header_field_d(su_home_t *home, sip_header_t *h, char *s, isize_t slen)
{
  if (h && s && s[slen] == '\0') {
    size_t n = span_lws(s);
    s += n; slen -= n;

    for (n = slen; n >= 1 && IS_LWS(s[n - 1]); n--)
      ;

    s[n] = '\0';

    assert(SIP_HDR_TEST(h));

    return h->sh_class->hc_parse(home, h, s, slen);
  }
  else
    return -1;
}

/** Encode a SIP header contents.
 *
 * @deprecated Use msg_header_field_e() instead.
 */
issize_t sip_header_field_e(char *b, isize_t bsiz, sip_header_t const *h, int flags)
{
  return msg_header_field_e(b, bsiz, h, flags);
}

/** Convert the header @a h to a string allocated from @a home. */
char *sip_header_as_string(su_home_t *home, sip_header_t const *h)
{
  ssize_t len;
  char *rv, s[256];
  ssize_t n;

  if (h == NULL)
    return NULL;

  len = sip_header_field_e(s, sizeof(s), h, 0);

  if (len >= 0 && (size_t)len < sizeof(s))
    return su_strdup(home, s);

  if (len == -1)
    len = 2 * sizeof(s);
  else
    len += 1;

  for (rv = su_alloc(home, len);
       rv;
       rv = su_realloc(home, rv, len)) {
	memset(rv,0,len);
    n = sip_header_field_e(rv, len, h, 0);
    if (n > -1 && n + 1 <= len)
      break;
    if (n > -1)			/* glibc >2.1 */
      len = n + 1;
    else			/* glibc 2.0 */
      len *= 2;
  }

  return rv;
}

/** Calculate size of a SIP header. */
isize_t sip_header_size(sip_header_t const *h)
{
  assert(h == NULL || h == SIP_NONE || h->sh_class);
  if (h == NULL || h == SIP_NONE)
    return 0;
  else
    return h->sh_class->hc_dxtra(h, h->sh_class->hc_size);
}

/** Duplicate a url or make a url out of string.
 * @deprecated Use url_hdup() instead.
 */
url_t *sip_url_dup(su_home_t *home, url_t const *o)
{
  return url_hdup(home, o);
}

/**Calculate Q value.
 *
 * Convert q-value string @a q to numeric value
 * in range (0..1000).  Q values are used, for instance, to describe
 * relative priorities of registered contacts.
 *
 * @param q q-value string <code>("1" | "." 1,3DIGIT)</code>
 *
 * @return An integer in range 0 .. 1000.
 */
unsigned sip_q_value(char const *q)
{
  unsigned value = 0;

  if (!q)
    return 1000;
  if (q[0] != '0' && q[0] != '.' && q[0] != '1')
    return 500; /* Garbage... */
  while (q[0] == '0')
    q++;
  if (q[0] >= '1' && q[0] <= '9')
    return 1000;
  if (q[0] == '\0')
    return 0;
  if (q[0] != '.')
    return 500;    /* Garbage... */

  if (q[1] >= '0' && q[1] <= '9') {
    value = (q[1] - '0') * 100;
    if (q[2] >= '0' && q[2] <= '9') {
      value += (q[2] - '0') * 10;
      if (q[3] >= '0' && q[3] <= '9') {
	value += (q[3] - '0');
	if (q[4] > '5' && q[4] <= '9')
	  /* Round upwards */
	  value += 1;
	else if (q[4] == '5')
	  value += value & 1; /* Round to even */
      }
    }
  }

  return value;
}


/**@ingroup sip_route
 *
 * Get first route header and remove it from its fragment chain.
 *
 */
sip_route_t *sip_route_remove(msg_t *msg, sip_t *sip)
{
  sip_route_t *r;

  if ((r = sip->sip_route))
    msg_header_remove(msg, (msg_pub_t *)sip, (msg_header_t *)r);

  return r;
}

/**@ingroup sip_route
 *
 * Get last route header and remove it from its fragment chain.
 *
 */
sip_route_t *sip_route_pop(msg_t *msg, sip_t *sip)
{
  sip_route_t *r;

  for (r = sip->sip_route; r; r = r->r_next)
    if (r->r_next == NULL) {
      msg_header_remove(msg, (msg_pub_t *)sip, (msg_header_t *)r);
      return r;
    }

  return NULL;
}


/**@ingroup sip_route
 *
 * Get first route header and rewrite the RequestURI.
 */
sip_route_t *sip_route_follow(msg_t *msg, sip_t *sip)
{
  if (sip->sip_route) {
    /* XXX - in case of outbound proxy, route may contain our address */

    sip_route_t *r = sip_route_remove(msg, sip);
    sip_request_t *rq = sip->sip_request;

    rq = sip_request_create(msg_home(msg), rq->rq_method, rq->rq_method_name,
			    (url_string_t const *)r->r_url, rq->rq_version);
    url_strip_transport(rq->rq_url);

    msg_header_insert(msg, (msg_pub_t *)sip, (msg_header_t *)rq);

    return r;
  }
  return NULL;
}

/**@ingroup sip_route
 *
 * Check if route header has lr param.
 *
 * "lr" param can be either URL or header parameter.
 */
int
sip_route_is_loose(sip_route_t const *r)
{
  if (!r)
    return 0;
  if (r->r_url->url_params)
    return url_has_param(r->r_url, "lr");
  else
    return r->r_params && msg_params_find(r->r_params, "lr") != NULL;
}

/**@ingroup sip_route
 *
 * Reverse a route header (@Route, @RecordRoute, @Path, @ServiceRoute).
 */
sip_route_t *sip_route_reverse_as(su_home_t *home,
				  msg_hclass_t *hc,
				  sip_route_t const *route)
{
  sip_route_t *reverse = NULL;
  sip_route_t r[1], *tmp;
  sip_route_init(r);

  r->r_common->h_class = hc;

  for (reverse = NULL; route; route = route->r_next) {
    *r->r_url = *route->r_url;
    /* Fix broken (Record-)Routes without <> */
    if (r->r_url->url_params == NULL
	&& r->r_params
	&& r->r_params[0]
	&& (r->r_params[0][0] == 'l' || r->r_params[0][0] == 'L')
	&& (r->r_params[0][1] == 'r' || r->r_params[0][1] == 'R')
	&& (r->r_params[0][2] == '=' || r->r_params[0][2] == 0))
      r->r_url->url_params = route->r_params[0],
	r->r_params = route->r_params + 1;
    else
      r->r_params = route->r_params;
    tmp = (sip_route_t *)msg_header_dup_as(home, hc, (msg_header_t *)r);
    if (!tmp)
      goto error;
    tmp->r_next = reverse;
    reverse = tmp;
  }

  return reverse;

 error:
  msg_header_free_all(home, (msg_header_t *)reverse);
  return NULL;
}


/**@ingroup sip_route
 *
 * Reverse a @Route header.
 *
 * Reverse A route header like @RecordRoute or @Path.
 */
sip_route_t *sip_route_reverse(su_home_t *home, sip_route_t const *route)
{
  return sip_route_reverse_as(home, sip_route_class, route);
}


/**@ingroup sip_route
 *
 * Fix and duplicate a route header (@Route, @RecordRoute, @Path, @ServiceRoute).
 *
 */
sip_route_t *sip_route_fixdup_as(su_home_t *home,
				 msg_hclass_t *hc,
				 sip_route_t const *route)
{
  sip_route_t *copy = NULL;
  sip_route_t r[1], **rr;
  sip_route_init(r);

  /* Copy the record route as route */
  for (rr = &copy; route; route = route->r_next) {
    *r->r_url = *route->r_url;
    /* Fix broken (Record-)Routes without <> */
    if (r->r_url->url_params == NULL
	&& r->r_params
	&& r->r_params[0]
	&& (r->r_params[0][0] == 'l' || r->r_params[0][0] == 'L')
	&& (r->r_params[0][1] == 'r' || r->r_params[0][1] == 'R')
	&& (r->r_params[0][2] == '=' || r->r_params[0][2] == 0))
      r->r_url->url_params = route->r_params[0],
	r->r_params = route->r_params + 1;
    else
      r->r_params = route->r_params;
    *rr = (sip_route_t *)msg_header_dup_as(home, hc, (msg_header_t *)r);
    if (!*rr) goto error;
    rr = &(*rr)->r_next;
  }

  return copy;

 error:
  msg_header_free_all(home, (msg_header_t *)copy);
  return NULL;
}


/**@ingroup sip_route
 *
 * Fix and duplicate a @Route header.
 *
 * Copy a route header like @RecordRoute or @Path as @Route.
 *
 */
sip_route_t *sip_route_fixdup(su_home_t *home, sip_route_t const *route)
{
  return sip_route_fixdup_as(home, sip_route_class, route);
}

static void sip_fragment_clear_chain(sip_header_t *h)
{
  void const *next;

  for (; h; h = h->sh_succ) {
    next = (char *)h->sh_data + h->sh_len;

    sip_fragment_clear(h->sh_common);

    if (!next ||
	!h->sh_succ ||
	h->sh_next != h->sh_succ ||
	h->sh_succ->sh_data != next ||
	h->sh_succ->sh_len)
      return;
  }
}

/**@ingroup sip_route
 *
 * Fix @Route header.
 */
sip_route_t *sip_route_fix(sip_route_t *route)
{
  sip_route_t *r;
  sip_header_t *h = NULL;
  size_t i;

  for (r = route; r; r = r->r_next) {
    /* Keep track of first header structure on this header line */
    if (!h
	|| (char *)h->sh_data + h->sh_len != r->r_common->h_data
	|| r->r_common->h_len)
      h = (sip_header_t *)r;

    if (r->r_url->url_params == NULL
	&& r->r_params
	&& r->r_params[0]
	&& (r->r_params[0][0] == 'l' || r->r_params[0][0] == 'L')
	&& (r->r_params[0][1] == 'r' || r->r_params[0][1] == 'R')
	&& (r->r_params[0][2] == '=' || r->r_params[0][2] == 0)) {
      r->r_url->url_params = r->r_params[0];

      for (i = 0; r->r_params[i]; i++)
	((char const **)r->r_params)[i] = r->r_params[i + 1];

      sip_fragment_clear_chain(h);
    }
  }

  return route;
}

/**@ingroup sip_via
 *
 * Get first via header and remove it from its fragment chain.
 */
sip_via_t *sip_via_remove(msg_t *msg, sip_t *sip)
{
  sip_via_t *v;

  if (sip == NULL)
    return NULL;

  for (v = sip->sip_via; v; v = v->v_next) {
    sip_fragment_clear(v->v_common);

    if (v->v_next != (void *)v->v_common->h_succ)
      break;
  }

  if ((v = sip->sip_via))
    msg_header_remove(msg, (msg_pub_t *)sip, (msg_header_t *)v);

  return v;
}

/** Serialize payload.
 *
 * The sip_payload_serialize() adds missing headers to MIME multiparty payload,
 * encodes them and orders them in header chain.  It also calculates the total
 * length of the payload.
 */
unsigned long sip_payload_serialize(msg_t *msg, sip_payload_t *pl)
{
  unsigned long total;

  for (total = 0; pl; pl = (sip_payload_t *)pl->pl_next) {
    total += (unsigned)pl->pl_common->h_len;
  }

  return total;
}

/**
 * Remove extra parameters from an AOR URL.
 *
 * The extra parameters listed in the @RFC3261 table 1 include port number,
 * method, maddr, ttl, transport, lr and headers.
 *
 * @note The funtion modifies the @a url and the strings attached to it.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int sip_aor_strip(url_t *url)
{
  if (url == NULL)
    return -1;

  url->url_port = NULL;
  url->url_headers = NULL;

  if (url->url_params)
    url_strip_transport(url);

  if (url->url_params)
    url->url_params =
      url_strip_param_string((char *)url->url_params, "lr");

  return 0;
}

/** Compare @SecurityVerify header with @SecurityServer header. */
int sip_security_verify_compare(sip_security_server_t const *s,
				sip_security_verify_t const *v,
				msg_param_t *return_d_ver)
{
  size_t i, j;
  int retval, digest;
  msg_param_t const *s_params, *v_params, empty[] = { NULL };

  if (return_d_ver)
    *return_d_ver = NULL;

  if (s == NULL)
    return 0;

  for (;;s = s->sa_next, v = v->sa_next) {
    if (s == NULL || v == NULL)
      return (s == NULL) - (v == NULL);

    if ((retval = su_strcmp(s->sa_mec, v->sa_mec)))
      return retval;

    digest = su_casematch(s->sa_mec, "Digest");

    s_params = s->sa_params, v_params = v->sa_params;

    if (digest && s_params == NULL && v_params != NULL)
      s_params = empty;

    if (s_params == NULL || v_params == NULL) {
      if ((retval = (s_params == NULL) - (v_params == NULL)))
	return retval;
      continue;
    }

    for (i = 0, j = 0;; i++, j++) {
      if (digest && v_params[j] &&
	  su_casenmatch(v_params[j], "d-ver=", 6)) {
	if (return_d_ver)
	  *return_d_ver = v_params[j] + strlen("d-ver=");
	j++;
      }

      retval = su_strcmp(s_params[i], v_params[j]);

      if (retval || s_params[i] == NULL || v_params[j] == NULL)
	break;
    }

    if (retval)
      return retval;
  }
}

/** Select best mechanism from @SecurityClient header.
 *
 * @note We assume that @SecurityServer header in @a s is sorted by
 * preference.
 */
sip_security_client_t const *
sip_security_client_select(sip_security_client_t const *client,
			   sip_security_server_t const *server)
{
  sip_security_server_t const *c, *s;

  if (server == NULL || client == NULL)
    return NULL;

  for (s = server; s; s = s->sa_next) {
    for (c = client; c; c = c->sa_next) {
      if (su_strmatch(s->sa_mec, c->sa_mec))
	return c;
    }
  }

  return NULL;
}

/**Checks if the response with given response code terminates dialog or
 * dialog usage.
 *
 * @return -1 if the response with given code terminates whole dialog.
 * @return 1 if the response terminates the dialog usage.
 * @return 0 if the response does not terminate dialog or dialog usage.
 *
 * @return
 * The @a *return_graceful_terminate_usage is set to 1, if application
 * should gracefully terminate its dialog usage. It is set to 0, if no
 * graceful terminate is required. If it is up to application policy to
 * decide whether to gracefully terminate or not, the
 * @a *return_graceful_terminate_usage is left unmodified.
 *
 * @RFC5057
 */
int sip_response_terminates_dialog(int response_code,
				   sip_method_t method,
				   int *return_graceful_terminate_usage)
{
  enum { no_effect, terminate_usage = 1, terminate_dialog = -1 };
  int dummy;

  if (!return_graceful_terminate_usage)
    return_graceful_terminate_usage = &dummy;

  if (response_code < 300)
    return *return_graceful_terminate_usage = 0;

  /*
      3xx responses: Redirection mid-dialog is not well understood in SIP,
      but whatever effect it has impacts the entire dialog and all of
      its usages equally.  In our example scenario, both the
      subscription and the invite usage would be redirected by this
      single response.
  */
  if (response_code < 400)
    return *return_graceful_terminate_usage = 0;

  if (response_code < 500) switch (response_code) {
  default:
  case 400: /** @par 400 and unrecognized 4xx responses

      These responses affect only the NOTIFY transaction, not the
      subscription, the dialog it resides in (beyond affecting the local
      CSeq), or any other usage of that dialog. In general, the response
      is a complaint about this transaction, not the usage or dialog the
      transaction occurs in.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 401:
  case 407: /** @par 401 Unauthorized and 407 Proxy Authentication Required

      This request, not the subscription or dialog, is being challenged. The
      usages and dialog are not terminated.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 402: /** @par 402 Payment Required

      This is a reserved response code. If encountered, it should be
      treated as an unrecognized 4xx.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 403: /** @par 403 Forbidden

      This response terminates the subscription, but has no effect on
      any other usages of the dialog. In our example scenario, the
      invite usage continues to exist. Similarly, if the 403 came in
      response to a re-INVITE, the invite usage would be terminated, but
      not the subscription.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 404: /** @par 404 Not Found

      This response destroys the dialog and all usages sharing it. The
      Request-URI that is being 404ed is the remote target set by the
      @Contact provided by the peer. Getting this response means
      something has gone fundamentally wrong with the dialog state.
    */
    return terminate_dialog;

  case 405: /** @par 405 Method Not Allowed

      In our example scenario, this response destroys the subscription,
      but not the invite usage or the dialog. It's an aberrant case for
      NOTIFYs to receive a 405 since they only come as a result to
      something that creates subscription. In general, a 405 within a
      given usage affects only that usage, but does not affect other
      usages of the dialog.
    */
    switch (method) {
    case sip_method_notify:
    case sip_method_subscribe:
    case sip_method_invite:
      return terminate_usage;
    default:
      *return_graceful_terminate_usage = 0;
      return 0;
    }

  case 406: /** @par 406 Not Acceptable

      These responses concern details of the message in the transaction.
      Subsequent requests in this same usage may succeed. Neither the
      usage nor dialog is terminated, other usages sharing this dialog
      are unaffected.
      */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 408: /** @par 408 Request Timeout

      Receiving a 408 will have the same effect on
      usages and dialogs as a real transaction timeout as described in
      Section 3.2.
    */
    return terminate_usage;

  case 410: /** @par 410 Gone

      This response destroys the dialog and all usages sharing
      it.  The Request-URI that is being rejected is the remote target
      set by the @Contact provided by the peer.  Similar to 404, getting
      this response means something has gone fundamentally wrong with
      the dialog state, its slightly less aberrant in that the other
      endpoint recognizes that this was once a valid URI that it isn't
      willing to respond to anymore.
    */
    return terminate_dialog;

  case 412: /* Conditional Request Failed: */
  case 413: /* Request Entity Too Large: */
  case 414: /* Request-URI Too Long: */
  case 415: /* Unsupported Media Type: */
    /** @par 412, 413, 414 and 415

      These responses concern details of the message in the transaction.
      Subsequent requests in this same usage may succeed. Neither the usage
      nor dialog is terminated, other usages sharing this dialog are
      unaffected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 416: /** @par 416 Unsupported URI Scheme

      Similar to 404 and 410, this response
      came to a request whose Request-URI was provided by the peer in a
      @Contact header field.  Something has gone fundamentally wrong, and
      the dialog and all of its usages are destroyed.
    */
    return terminate_dialog;

  case 417:
    /** @par 417 Uknown Resource-Priority
      The effect of this response on usages
      and dialogs is analgous to that for 420 and 488.  The usage is not
      affected.  The dialog is only affected by a change in its local
      @CSeq.  No other usages of the dialog are affected.
    */

  case 420: /* Bad Extension */
  case 421: /* Extension Required */

    /** @par 420 Bad Extension and 421 Extension Required

      These responses are objecting to the request, not the usage. The
      usage is not affected. The dialog is only affected by a change in
      its local @CSeq. No other usages of the dialog are affected.
    */

  case 422: /** @par 422 Session Interval Too Small

      This response will not be returned to
      a NOTIFY in our example scenario.  This response is non-sensical
      for any mid-usage request.  If it is received, an element in the
      path of the request is violating protocol, and the recipient
      should treat this as it would an unknown 4xx response.  If the
      response came to a request that was attempting to establish a new
      usage in an existing dialog, no new usage is created and existing
      usages are unaffected.
    */

  case 423: /** @par 423 Interval Too Brief

      This response won't happen in our example
      scenario, but if it came in response to a re-SUBSCRIBE, the
      subscribe usage is not destroyed (or otherwise affected).  No
      other usages of the dialog are affected.
    */

  case 428: /** @par 428 Use Identity Header

      This response objects to the request, not
      the usage.  The usage is not affected.  The dialog is only
      affected by a change in its local @CSeq.  No other usages of the
      dialog are affected. */

  case 429: /** @par 429 Provide Referrer Identity

      This response won't be returned to a NOTIFY as in our example
      scenario, but when it is returned to a REFER, it is objecting to
      the REFER request itself, not any usage the REFER occurs within.
      The usage is unaffected. Any other usages sharing this dialog are
      unaffected. The dialog is only affected by a change in its local
      @CSeq.
    */

  case 436: case 437: case 438:
    /** @par 436 Bad Identity-Info, 437 Unsupported Certificate, 438 Invalid \
     *  Identity Header
     *
     * These responses object to the request, not the usage.
     * The usage is not affected.  The dialog is only affected by a
     * change in its local @CSeq.  No other usages of the dialog are
     * affected.
     */
    *return_graceful_terminate_usage = 0;
    return 0;


  case 480: /** @par 480 Temporarily Unavailable

      @RFC3261 is unclear on what this response means for mid-usage
      requests. Clarifications will be made to show that this response
      affects only the usage in which the request occurs. No other usages
      are affected. If the response included a @RetryAfter header field,
      further requests in that usage should not be sent until the indicated
      time has past. Requests in other usages may still be sent at any time.
    */
    return terminate_usage;


  case 481: /** @par 481 Call/Transaction Does Not Exist

      This response indicates that the peer has lost its copy of the dialog
      state. The dialog and any usages sharing it are destroyed.

      The dialog
      itself should not be destroyed unless this was the last usage.
      The effects of a 481 on a dialog and its usages are the most
      ambiguous of any final response.  There are implementations that
      have chosen the meaning recommended here, and others that destroy
      the entire dialog without regard to the number of outstanding
      usages.  Going forward with this clarification will allow those
      deployed implementations that assumed only the usage was destroyed
      to work with a wider number of implementations.  Those that made
      the other choice will continue to function as they do now,
      suffering at most the same extra messages needed for a peer to
      discover that that other usages have gone away that they currently
      do.  However, the necessary clarification to @RFC3261 needs to
      make it very clear that the ability to terminate usages
      independently from the overall dialog using a 481 is not
      justification for designing new applications that count on
      multiple usages in a dialog.
    */
    return terminate_usage;


  case 482: /** @par 482 Loop Detected

      This response is aberrant mid-dialog.  It will
      only occur if the @RecordRoute header field was improperly
      constructed by the proxies involved in setting up the dialog's
      initial usage, or if a mid-dialog request forks and merges (which
      should never happen).  Future requests using this dialog state
      will also fail.  The dialog and any usages sharing it are
      destroyed.
    */
    return terminate_dialog;


  case 483: /** @par 483 Too Many Hops

      Similar to 482, receiving this mid-dialog is
      aberrant.  Unlike 482, recovery may be possible by increasing
      @MaxForwards (assuming that the requester did something strange
      like using a smaller value for @MaxForwards in mid-dialog requests
      than it used for an initial request).  If the request isn't tried
      with an increased @MaxForwards, then the agent should attempt to
      gracefully terminate this usage and all other usages that share
      its dialog.
    */
    *return_graceful_terminate_usage = 1;
    return 0;

  case 484: /* Address Incomplete */
    /** @par 484 Address Incomplete and 485 Ambiguous

      Similar to 404 and 410, these
      responses came to a request whose Request-URI was provided by the
      peer in a @Contact header field.  Something has gone fundamentally
      wrong, and the dialog and all of its usages are destroyed.

      Asterisk (v 1.2.7.1) does response with 484 if a client does send a refer
      with a @ReferTo header to an unknown number.  This is therefore not
      fundamentally wrong and the dialog should not be destroyed!
    */
    if (method == sip_method_refer)
    {
      *return_graceful_terminate_usage = 0;
      return 0;
    }

  case 485: /* Ambiguous */

    return terminate_dialog;

  case 486: /** @par 486 Busy Here

      This response is non-sensical in our example scenario,
      or in any scenario where this response comes inside an established
      usage.  If it occurs in that context, it should be treated as an
      unknown 4xx response.  The usage, and any other usages sharing its
      dialog are unaffected.  The dialog is only affected by the change
      in its local @CSeq.  If this response is to a request that is
      attempting to establish a new usage within an existing dialog
      (such as an INVITE sent within a dialog established by a
      subscription), the request fails, no new usage is created, and no
      other usages are affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 487: /** @par 487 Request Terminated

      This response speaks to the disposition of a
      particular request (transaction).  The usage in which that request
      occurs is not affected by this response (it may be affected by
      another associated request within that usage).  No other usages
      sharing this dialog are affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 488: /** @par 488 Not Acceptable Here

      This response is objecting to the request,
      not the usage.  The usage is not affected.  The dialog is only
      affected by a change in its local @CSeq.  No other usages of the
      dialog are affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 489: /** @par 489 Bad Event

      In our example scenario, @RFC3265 declares that the
      subscription usage in which the NOTIFY is sent is terminated.  The
      invite usage is unaffected and the dialog continues to exist.
      This response is only valid in the context of SUBSCRIBE and
      NOTIFY.  UAC behavior for receiving this response to other methods
      is not specified, but treating it as an unknown 4xx is a
      reasonable practice.
    */
    *return_graceful_terminate_usage = 0;
    return method == sip_method_notify ? terminate_usage : no_effect;

  case 491: /** @par 491 Request Pending

      This response addresses in-dialog request glare.
      Its affect is scoped to the request.  The usage in which the
      request occurs is not affected.  The dialog is only affected by
      the change in its local @CSeq.  No other usages sharing this dialog
      are affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 493: /** @par 493 Undecipherable

      This response objects to the request, not the
      usage.  The usage is not affected.  The dialog is only affected by
      a change in its local @CSeq.  No other usages of the dialog are
      affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 494: /** @par 494 Security Agreement Required

      This response is objecting to the
      request, not the usage.  The usage is not affected.  The dialog is
      only affected by a change in its local @CSeq.  No other usages of
      the dialog are affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;
  }

  if (response_code < 600) switch (response_code) {
  case 500: /* 500 and 5xx unrecognized responses */
  default:
    /** @par 500 and 5xx unrecognized responses

      These responses are complaints against the request (transaction),
      not the usage. If the response contains a @RetryAfter header field
      value, the server thinks the condition is temporary and the
      request can be retried after the indicated interval. This usage,
      and any other usages sharing the dialog are unaffected. If the
      response does not contain a @RetryAfter header field value, the UA
      may decide to retry after an interval of its choosing or attempt
      to gracefully terminate the usage. Whether or not to terminate
      other usages depends on the application. If the UA receives a 500
      (or unrecognized 5xx) in response to an attempt to gracefully
      terminate this usage, it can treat this usage as terminated. If
      this is the last usage sharing the dialog, the dialog is also
      terminated.
    */
    /* Do not change *return_graceful_terminate_usage */
    return 0;

  case 501: /** @par 501 Not Implemented

      This would be a degenerate response in our
      example scenario since the NOTIFY is being sent as part of an
      established subscribe usage.  In this case, the UA knows the
      condition is unrecoverable and should stop attempting to send
      NOTIFYs on this usage.  (It may or may not destroy the usage.  If
      it remembers the bad behavior, it can reject any refresh
      subscription).  In general, this response may or may not affect
      the usage (a 501 to an unknown method or an INFO will not end an
      invite usage).  It will never affect other usages sharing this
      usage's dialog.
    */
    /* Do not change *return_graceful_terminate_usage */
    return 0;

  case 502: /** @par 502 Bad Gateway

      This response is aberrant mid-dialog. It will only occur if the
      @RecordRoute header field was improperly constructed by the
      proxies involved in setting up the dialog's initial usage. Future
      requests using this dialog state will also fail. The dialog and
      any usages sharing it are destroyed.
    */
    return terminate_dialog;

  case 503: /** @par 503 Service Unavailable

      As per @RFC3263, the logic handling locating SIP servers for
      transactions may handle 503 requests (effectively sequentially
      forking at the endpoint based on DNS results). If this process
      does not yield a better response, a 503 may be returned to the
      transaction user. Like a 500 response, the error is a complaint
      about this transaction, not the usage. Because this response
      occurred in the context of an established usage (hence an existing
      dialog), the route-set has already been formed and any opportunity
      to try alternate servers (as recommended in @RFC3261) has been exhausted
      by the @RFC3263 logic. The response should be handled as described
      for 500 earlier in this memo.
    */
    /* Do not change *return_graceful_terminate_usage */
    return 0;

  case 504: /** @par 504 Server Time-out

      It is not obvious under what circumstances this
      response would be returned to a request in an existing dialog.  If
      it occurs it should have the same affect on the dialog and its
      usages as described for unknown 5xx responses.
    */
    /* Do not change *return_graceful_terminate_usage */
    return 0;

  case 505: /* Version Not Supported */
  case 513: /* Message Too Large */
    /** @par 505 Version Not Supported and 513 Message Too Large

      These responses are objecting to the request, not the usage. The
      usage is not affected. The dialog is only affected by a change in
      its local @CSeq. No other usages of the dialog are affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 580: /** @par 580 Precondition Failure

      This response is objecting to the request,
      not the usage.  The usage is not affected.  The dialog is only
      affected by a change in its local @CSeq.  No other usages of the
      dialog are affected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;
  }

  if (response_code < 700) switch (response_code) {
  case 600: /* 600 and 6xx unrecognized responses */
  default:
    /** @par 600 and 6xx unrecognized responses

      Unlike 400 Bad Request, a 600 response code says something about
      the recipient user, not the request that was made. This end user
      is stating an unwillingness to communicate.

      If the response contains a @RetryAfter header field value, the
      user is indicating willingness to communicate later and the
      request can be retried after the indicated interval. This usage,
      and any other usages sharing the dialog are unaffected. If the
      response does not contain a @RetryAfter header field value, the UA
      may decide to retry after an interval of its choosing or attempt
      to gracefully terminate the usage. Whether or not to terminate
      other usages depends on the application. If the UA receives a 600
      (or unrecognized 6xx) in response to an attempt to gracefully
      terminate this usage, it can treat this usage as terminated. If
      this is the last usage sharing the dialog, the dialog is also
      terminated.
    */
    /* Do not change graceful_terminate */
    return 0;

  case 603: /** @par 603 Decline

      This response declines the action indicated by the
      associated request.  It can be used, for example, to decline a
      hold or transfer attempt.  Receiving this response does NOT
      terminate the usage it occurs in.  Other usages sharing the dialog
      are unaffected.
    */
    *return_graceful_terminate_usage = 0;
    return 0;

  case 604: /** @par 604 Does Not Exist Anywhere

      Like 404, this response destroys the
      dialog and all usages sharing it.  The Request-URI that is being
      604ed is the remote target set by the @Contact provided by the
      peer.  Getting this response means something has gone
      fundamentally wrong with the dialog state.
    */
    return terminate_dialog;

  case 606: /** @par 606 Not Acceptable

      This response is objecting to aspects of the
      associated request, not the usage the request appears in.  The
      usage is unaffected.  Any other usages sharing the dialog are
      unaffected.  The only affect on the dialog is the change in the
      local @CSeq.
    */
    *return_graceful_terminate_usage = 0;
    return 0;
  }

  /* Do not change graceful_terminate */

  return 0;
}


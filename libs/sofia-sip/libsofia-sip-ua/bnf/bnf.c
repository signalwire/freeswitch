/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005,2006 Nokia Corporation.
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

/**@CFILE bnf.c
 * @brief Character syntax table for HTTP-like protocols.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <Kai.Vehmanen@nokia.com>
 *
 * @date Created: Thu Jun  8 19:28:55 2000 ppessi
 */

#include "config.h"

#include "sofia-sip/bnf.h"
#include "sofia-sip/su_string.h"

#include <stdio.h>
#include <assert.h>

#define ws    bnf_ws
#define crlf  bnf_crlf
#define alpha bnf_alpha
#define digit bnf_mark|bnf_token0|bnf_safe
#define sep   bnf_separator
#define msep  bnf_mark|bnf_separator
#define psep  bnf_param0|bnf_separator
#define tok   bnf_token0
#define mtok  bnf_mark|bnf_token0
#define smtok bnf_mark|bnf_token0|bnf_safe
#define safe  bnf_safe

/** Table for determining class of a character */
unsigned char const _bnf_table[256] = {
  0,     0,     0,     0,     0,     0,     0,     0,
  0,     ws,    crlf,  0,     0,     crlf,  0,     0,
  0,     0,     0,     0,     0,     0,     0,     0,
  0,     0,     0,     0,     0,     0,     0,     0,
  ws,    mtok,  sep,   0,     safe,  mtok,  0,     mtok,  /*  !"#$%&' */
  msep,  msep,  mtok,  tok,   sep,   smtok, smtok, psep,  /* ()*+,-./ */
  digit, digit, digit, digit, digit, digit, digit, digit, /* 01234567 */
  digit, digit, psep,  sep,   sep,   sep,   sep,   sep,   /* 89:;<=>? */
  sep,   alpha, alpha, alpha, alpha, alpha, alpha, alpha, /* @ABCDEFG */
  alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, /* HIJKLMNO */
  alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, /* PQRSTUVW */
  alpha, alpha, alpha, psep,  sep,   psep,  0,     smtok, /* XYZ[\]^_ */
  tok,   alpha, alpha, alpha, alpha, alpha, alpha, alpha, /* `abcdefg */
  alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, /* hijklmno */
  alpha, alpha, alpha, alpha, alpha, alpha, alpha, alpha, /* pqrstuvw */
  alpha, alpha, alpha, sep,   0,     sep,   mtok,  0,     /* xyz{|}~  */
};

#if 0				/* This escaped lab */

#define BM(c, m00, m32, m64, m96)			   \
  ((c < 64)						   \
   ? ((c < 32)						   \
      ? (m00 & (1 << (31 - c)))				   \
      : (m32 & (1 << (63 - c))))			   \
   : ((c < 96)						   \
      ? (m64 & (1 << (95 - c)))				   \
      : (m96 & (1 << (127 - c)))))

/** Span of a token */
size_t bnf_span_token(char const *s)
{
  char const *e = s;
  unsigned const m32 = 0x4536FFC0U, m64 = 0x7FFFFFE1U, m96 = 0xFFFFFFE2U;

  while (BM(*e, 0, m32, m64, m96))
    e++;

  return e - s;
}

/** Span of a token */
size_t bnf_span_token4(char const *s)
{
  char const *e = s;
  while (_bnf_table[(unsigned char)(*e)] & bnf_token)
    e++;
  return e - s;
}

char * bnf_span_token_end(char const *s)
{
  return (char *)s;
}
#endif

/** Return length of decimal-octet */
su_inline int span_ip4_octet(char const *host)
{
  /*
      decimal-octet =       DIGIT
                            / DIGIT DIGIT
                            / (("0"/"1") 2*(DIGIT))
                            / ("2" ("0"/"1"/"2"/"3"/"4") DIGIT)
                            / ("2" "5" ("0"/"1"/"2"/"3"/"4"/"5"))
  */

  if (!IS_DIGIT(host[0]))
    return 0;

  /* DIGIT */
  if (!IS_DIGIT(host[1]))
    return 1;

  if (host[0] == '2') {
    /* ("2" "5" ("0"/"1"/"2"/"3"/"4"/"5")) */
    if (host[1] == '5' && host[2] >= '0' && host[2] <= '5')
      return 3;

    /* ("2" ("0"/"1"/"2"/"3"/"4") DIGIT) */
    if (host[1] >= '0' && host[1] <= '4' &&
	host[2] >= '0' && host[2] <= '9')
      return 3;
  }
  else if (host[0] == '0' || host[0] == '1') {
    if (IS_DIGIT(host[2]))
      /* ("1" 2*(DIGIT)) ... or "0" 2*(DIGIT) */
      return 3;
  }

  /* POS-DIGIT DIGIT */
  return 2;
}

/** Return length of valid IP4 address */
static
int span_canonic_ip4_address(char const *host, int *return_canonize)
{
  int n, len, canonize = 0;

  if (host == NULL)
    return 0;

  /* IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet */
  len = span_ip4_octet(host);
  if (len == 0 || host[len] != '.')
    return 0;
  if (len > 1 && host[0] == '0')
    canonize = 1;
  n = len + 1;

  len = span_ip4_octet(host + n);
  if (len == 0 || host[n + len] != '.')
    return 0;
  if (len > 1 && host[n] == '0')
    canonize = 1;
  n += len + 1;

  len = span_ip4_octet(host + n);
  if (len == 0 || host[n + len] != '.')
    return 0;
  if (len > 1 && host[n] == '0')
    canonize = 1;
  n += len + 1;

  len = span_ip4_octet(host + n);
  if (len == 0 || IS_DIGIT(host[n + len]) || host[n + len] == '.')
    return 0;
  if (len > 1 && host[n] == '0')
    canonize = 1;
  n += len;

  if (canonize && return_canonize)
    *return_canonize = 1;

  return n;
}

/** Return length of valid IP4 address.
 *
 * Note that we accept here up to two leading zeroes
 * which makes "dotted decimal" notation ambiguous:
 * 127.000.000.001 is interpreted same as 127.0.0.1
 *
 * Note that traditionally IP address octets starting
 * with zero have been interpreted as octal:
 * 172.055.055.001 has been same as 172.45.45.1
 *
 * @b However, we interpret them as @b decimal,
 * 172.055.055.001 is same as 172.55.55.1.
 */
int span_ip4_address(char const *host)
{
  return span_canonic_ip4_address(host, NULL);
}

/** Scan and canonize a valid IP4 address. */
int scan_ip4_address(char **inout_host)
{
  char *src = *inout_host, *dst = src;
  issize_t n;
  int canonize = 0;

  if (src == NULL)
    return -1;

  n = span_canonic_ip4_address(src, &canonize);
  if (n == 0)
    return -1;

  *inout_host += n;

  if (!canonize)
    return n;

  for (;;) {
    char c = *dst++ = *src++;

    if (IS_DIGIT(*src)) {
      if (canonize && c == '0')
	dst--;
      else if (c == '.')
	canonize = 1;
      else
	canonize = 0;
    }
    else if (*src != '.') {
      break;
    }
  }
  *dst = '\0';

  return n;
}

/** Return length of hex4 */
su_inline int span_hex4(char const *host)
{
  if (!IS_HEX(host[0]))
    return 0;
  if (!IS_HEX(host[1]))
    return 1;
  if (!IS_HEX(host[2]))
    return 2;
  if (!IS_HEX(host[3]))
    return 3;
  return 4;
}

/** Return length of valid IP6 address */
su_inline
int span_canonic_ip6_address(char const *host,
			     int *return_canonize,
			     char *hexparts[9])
{
  int n = 0, len, hex4, doublecolon = 0, canonize = 0;

  /*
     IPv6address    =  hexpart [ ":" IPv4address ]
     hexpart        =  hexseq / hexseq "::" [ hexseq ] / "::" [ hexseq ]
     hexseq         =  hex4 *( ":" hex4)
     hex4           =  1*4HEXDIG

     There is at most 8 hex4, 6 hex4 if IPv4address is included.
  */

  if (host == NULL)
    return 0;

  for (hex4 = 0; hex4 < 8; ) {
    len = span_hex4(host + n);

    if (return_canonize) {
      if ((len > 1 && host[n + 1] == '0') || host[n] == '0')
	canonize = 1;
      if (hexparts)
	hexparts[hex4 + doublecolon] = (char *)(host + n);
    }

    if (host[n + len] == ':') {
      if (len != 0) {
	hex4++;
	n += len + 1;
	if (!doublecolon && host[n] == ':') {
	  if (return_canonize && hexparts) {
	    hexparts[hex4] = (char *)(host + n - 1);
	  }
	  doublecolon++, n++;
	}
      }
      else if (n == 0 && host[1] == ':') {
	doublecolon++, n = 2;
      }
      else
	break;
    }
    else if (host[n + len] == '.') {
      len = span_canonic_ip4_address(host + n, return_canonize);

      if (len == 0 || hex4 > 6 || !(doublecolon || hex4 == 6))
	return 0;

      if (canonize && return_canonize)
	*return_canonize = 1;

      return n + len;
    }
    else {
      if (len != 0)
	hex4++;
      n += len;
      break;
    }
  }

  if (hex4 != 8 && !doublecolon)
    return 0;

  if (IS_HEX(host[n]) || host[n] == ':')
    return 0;

  if (canonize && return_canonize)
    *return_canonize = canonize;

  return n;
}

/** Canonize scanned IP6 address.
 *
 * @retval Length of canonized IP6 address.
 */
su_inline
int canonize_ip6_address(char *host, char *hexparts[9])
{
  char *dst, *hex, *ip4 = NULL;
  int i, doublecolon, j, maxparts, maxspan, span, len;

  char buf[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

  /*
    Canonic representation has fewest chars
     - except for mapped/compatible IP4 addresses, like
       ::15.21.117.42 or ::ffff:15.21.117.42 which have non-canonic forms of
       ::f15:752a or ::ffff:f15:752a
    => we just canonize hexparts and ip4part separately
       and select optimal place for doublecolon
       (with expection of ::1 and ::, which are canonized)
  */
  for (i = 0, doublecolon = -1; i < 9; i++) {
    hex = hexparts[i];
    if (!hex)
      break;
    if (hex[0] == ':')
      doublecolon = i;
    while (hex[0] == '0' && IS_HEX(hex[1]))
      hex++;
    hexparts[i] = hex;
  }
  assert(i > 0);

  if (hexparts[i - 1][span_hex4(hexparts[i - 1])] == '.')
    ip4 = hexparts[--i];

  maxparts = ip4 ? 6 : 8;

  if (doublecolon >= 0) {
    /* Order at most 8 (or 6) hexparts */
    assert(i <= maxparts + 1);

    if (i == maxparts + 1) {
      /* There is an extra doublecolon */
      for (j = doublecolon; j + 1 < i; j++)
	hexparts[j] = hexparts[j + 1];
      i--;
    }
    else {
      for (j = maxparts; i > doublecolon + 1; )
	hexparts[--j] = hexparts[--i];
      for (;j > doublecolon;)
	hexparts[--j] = "0:";
      i = maxparts;
    }
  }
  assert(i == maxparts);

  /* Scan for optimal place for "::" */
  for (i = 0, maxspan = 0, span = 0, doublecolon = 0; i < maxparts; i++) {
    if (hexparts[i][0] == '0')
      span++;
    else if (span > maxspan)
      doublecolon = i - span, maxspan = span, span = 0;
    else
      span = 0;
  }

  if (span > maxspan)
    doublecolon = i - span, maxspan = span;

  dst = buf;

  for (i = 0; i < maxparts; i++) {
    if (i == doublecolon)
      hex = i == 0 ? "::" : ":", len = 1;
    else if (i > doublecolon && i < doublecolon + maxspan)
      continue;
    else
      hex = hexparts[i], len = span_hex4(hex);
    if (hex[len] == ':')
      len++;
    memcpy(dst, hex, len);
    dst += len;
  }

  if (ip4) {
    hex = ip4;
    len = scan_ip4_address(&hex); assert(len > 0);

    /* Canonize :: and ::1 */
    if (doublecolon == 0 && maxspan == 6) {
      if (len == 7 && strncmp(ip4, "0.0.0.0", len) == 0)
	ip4 = "", len = 0;
      else if (len == 7 && strncmp(ip4, "0.0.0.1", len) == 0)
	ip4 = "1", len = 1;
    }

    memcpy(dst, ip4, len);
    dst += len;
  }

  len = dst - buf;

  memcpy(host, buf, len);

  return len;
}

/** Return length of valid IP6 address */
int span_ip6_address(char const *host)
{
  return span_canonic_ip6_address(host, NULL, NULL);
}

/** Scan and canonize valid IP6 address.
 *
 * @param inout_host input pointer to string to scan
 *                   output pointer to first character after valid IP6 address
 *
 * @retval Length of valid IP6 address or -1 upon an error.
 *
 * @note Scanned IP6 is not always NUL-terminated.
 */
int scan_ip6_address(char **inout_host)
{
  int n, canonize = 0;
  char *host = *inout_host;
  char *hexparts[9] = { NULL };

  n = span_canonic_ip6_address(host, &canonize, hexparts);

  if (n == 0)
    return -1;

  *inout_host += n;

  if (canonize) {
    int len = canonize_ip6_address(host, hexparts);
    assert(len <= n);
    if (len < n)
      host[len] = '\0';
  }

  return n;
}

/** Return length of valid IP6 reference. */
int span_ip6_reference(char const *host)
{
  /* IPv6reference  =  "[" IPv6address "]" */

  if (host && host[0] == '[') {
    int n = span_ip6_address(host + 1);
    if (n > 0 && host[n + 1] == ']')
      return n + 2;
  }

  return 0;
}

/** Scan valid IP6 reference. */
int scan_ip6_reference(char **inout_host)
{
  int n, canonize = 0;
  char *host = *inout_host;
  char *hexparts[9] = { NULL };

  /* IPv6reference  =  "[" IPv6address "]" */

  if (host == NULL ||
      host[0] != '[' ||
      (n = span_canonic_ip6_address(host + 1, &canonize, hexparts)) == 0 ||
      host[n + 1] != ']')
    return -1;

  *inout_host += n + 2;

  if (canonize) {
    int len = canonize_ip6_address(host + 1, hexparts);

    assert(len <= n);

    host[len + 1] = ']';
    if (len + 2 < n + 2)
      host[len + 2] = '\0';
  }

  return n + 2;
}

/** Return length of valid IP4 or IP6 address. */
int span_ip_address(char const *host)
{
  if (!host || !host[0])
    return 0;

  /* IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet */
  if (IS_DIGIT(host[0])) {
    int n = span_ip4_address(host);
    if (n)
      return n;
  }

  if (host[0] == '[')
    return span_ip6_reference(host);
  else
    return span_ip6_address(host);
}

/** Scan valid IP4/IP6 address. */
int scan_ip_address(char **inout_host)
{
  char *host = *inout_host;

  if (host == NULL)
    return -1;

  /* IPv6reference  =  "[" IPv6address "]" */
  if (host[0] == '[')
    return scan_ip6_reference(inout_host);

  if (IS_DIGIT(host[0])) {
    int n = scan_ip4_address(inout_host);
    if (n > 0)
      return n;
  }

  return scan_ip6_address(inout_host);
}

/** Return length of a valid domain label */
su_inline
size_t span_domain_label(char const *label)
{
  /* domainlabel =  alphanum / alphanum *( alphanum / "-" ) alphanum */
  if (IS_ALPHANUM(*label)) {
    size_t n;
    for (n = 1; IS_ALPHANUM(label[n]) || label[n] == '-'; n++)
      ;
    if (IS_ALPHANUM(label[n - 1]))
      return n;
  }

  return 0;
}

/** Scan valid domain name and count number of labels in it. */
su_inline
size_t span_domain_labels(char const *host, size_t *return_labels)
{
  size_t len, n, labels;
  int c;

  if (!host || !host[0])
    return 0;

  for (n = 0, labels = 0; ; n += len) {
    len = span_domain_label(host + n);
    if (len == 0)
      return 0;

    labels++;

    if (host[n + len] != '.')
      break;
    len++;
    if (!IS_ALPHANUM(host[n + len]))
      break;
  }

  /* Check that last label does not start with number */
  if (!IS_ALPHA(host[n]))
    return 0;

  c = host[n + len];
  if (IS_ALPHANUM(c) || c == '-' || c == '.')
    return 0;

  if (return_labels)
    *return_labels = labels;

  return n + len;
}

/** Return length of a valid domain name.
 *
 * @code
 * hostname         =  *( domainlabel "." ) toplabel [ "." ]
 * domainlabel      =  alphanum
 *                     / alphanum *( alphanum / "-" ) alphanum
 * toplabel         =  ALPHA / ALPHA *( alphanum / "-" ) alphanum
 * @endcode
 */
isize_t span_domain(char const *host)
{
  return span_domain_labels(host, NULL);
}

/** Scan valid domain name. */
issize_t scan_domain(char **inout_host)
{
  char *host;
  size_t n, labels;

  n = span_domain_labels(host = *inout_host, &labels);
  if (n == 0)
    return -1;

  /* Remove extra dot at the end of hostname */
  if (labels > 1 && host[n - 1] == '.')
    host[n - 1] = '\0';

  *inout_host += n;

  return n;
}

/** Return length of a valid domain name or IP address. */
isize_t span_host(char const *host)
{
 if (!host || !host[0])
    return 0;

  if (host[0] == '[')
    return span_ip6_reference(host);

  if (IS_DIGIT(host[0])) {
    int n = span_ip4_address(host);
    if (n)
      return (isize_t)n;
  }

  return span_domain(host);
}

/** Scan valid domain name or IP address. */
issize_t scan_host(char **inout_host)
{
  char *host = *inout_host;

  if (host == NULL)
    return -1;

  /* IPv6reference  =  "[" IPv6address "]" */
  if (host[0] == '[')
    return scan_ip6_reference(inout_host);

  if (IS_DIGIT(host[0])) {
    int n = scan_ip4_address(inout_host);
    if (n > 0)
      return (issize_t)n;
  }

  return scan_domain(inout_host);
}

#include <sofia-sip/hostdomain.h>

/** Return true if @a string is valid IP4 address in dot-notation.
 *
 * @note Only 4-octet form is accepted, e.g., @c 127.1 is not considered
 * valid IP4 address.
 */
int host_is_ip4_address(char const *string)
{
  int n = span_ip4_address(string);
  return n > 0 && string[n] == '\0';
}

/** Return true if @a string is valid IP6 address in hex notation.
 *
 * E.g., fe80::1 is a valid IP6 address.
 */
int host_is_ip6_address(char const *string)
{
  int n = span_ip6_address(string);
  return n > 0 && string[n] == '\0';
}

int host_ip6_reference(char const *string)
{
  return host_is_ip6_reference(string);
}

/** Return true if @a string is valid IP6 reference,
 *  i.e. hex notation in square brackets.
 *
 * E.g., [::1] is a valid IP6 reference.
 */
int host_is_ip6_reference(char const *string)
{
  int n = span_ip6_reference(string);
  return n > 0 && string[n] == '\0';
}

/** Return true if @a string is valid IP address.
 *
 * Valid IP address is either a IP4 adddress in quad-octet notation,
 * IP6 hex address or IP6 reference in square brackets ([]).
 */
int host_is_ip_address(char const *string)
{
  int n = span_ip_address(string);
  return n > 0 && string[n] == '\0';
}

/** Return true if @a string is valid a domain name.
 *
 * Valid domain name consists of alphanumeric labels separated with
 * dot ("."). There can be a "-" in the middle of label.
 * The last label must start with a letter.
 *
 * @code
 * hostname         =  *( domainlabel "." ) toplabel [ "." ]
 * domainlabel      =  alphanum
 *                     / alphanum *( alphanum / "-" ) alphanum
 * toplabel         =  ALPHA / ALPHA *( alphanum / "-" ) alphanum
 * @endcode
 */
int host_is_domain(char const *string)
{
  size_t n = string ? span_domain(string) : 0;
  return string && n > 0 && string[n] == '\0';
}

/** Return true if @a string is valid a host name.
 *
 * Check if the @a string is a domain name, IP address or IP6 reference.
 */
int host_is_valid(char const *string)
{
  size_t n = span_host(string);
  return n > 0 && string[n] == '\0';
}

/** Returns true if @a string is describing a local address.
 *
 * Uses the definitions of local addresses found in RFC1700 and
 * RFC4291.
 */
int host_is_local(char const *host)
{
  size_t n;

  if (host_is_ip6_reference(host))
    return (strcmp(host, "[::1]") == 0);
  else if (host_is_ip6_address(host))
    return (strcmp(host, "::1") == 0);
  else if (host_is_ip4_address(host))
    return (strncmp(host, "127.", 4) == 0);

  n = span_domain(host);

  return
    n >= 9 /* strlen("localhost") */ &&
    su_casenmatch(host, "localhost", 9) &&
    (n == 9 ||
     ((n == 10 || /* localhost. */
       n == 21 || /* strlen("localhost.localdomain") */
       n == 22) && /* strlen("localhost.localdomain.") */
      su_casenmatch(host + 9, ".localdomain.", n - 9)));
}

/** Return true if @a string has domain name in "invalid." domain.
 *
 */
int host_has_domain_invalid(char const *string)
{
  size_t n = span_domain(string);

  if (n >= 7 && string[n] == '\0') {
    static char const invalid[] = ".invalid";
    if (string[n - 1] == '.')	/* .invalid. perhaps? */
      n--;
    if (n == 7 /* strlen("invalid") */)
      return su_casenmatch(string, invalid + 1, 7);
    else
      return su_casenmatch(string + n - 8, invalid, 8);
  }

  return 0;
}

#include <sofia-sip/su.h>

static size_t convert_ip_address(char const *s,
				 uint8_t addr[16],
				 size_t *return_addrlen)
{
  size_t len;
  int canonize = 0;
  char buf[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];

#if SU_HAVE_IN6

  len = span_ip6_reference(s);
  if (len) {
    assert(len - 2 < sizeof buf); assert(len > 2);

    if (s[len])
      return 0;

    len = len - 2;
    s = memcpy(buf, s + 1, len), buf[len] = '\0';
  }
  else
    len = span_ip6_address(s);

  if (len) {
    if (s[len] == '\0' && su_inet_pton(AF_INET6, s, addr) == 1) {
      if (SU_IN6_IS_ADDR_V4MAPPED(addr) ||
	  SU_IN6_IS_ADDR_V4COMPAT(addr)) {
	memcpy(addr, addr + 12, 4);
	return (void)(*return_addrlen = 4), len;
      }
      return (void)(*return_addrlen = 16), len;
    }
  }
  else
#endif
  len = span_canonic_ip4_address(s, &canonize);

  if (len) {
    if (canonize) {
      char *tmp = buf;
      s = memcpy(tmp, s, len + 1);
      scan_ip4_address(&tmp);
    }
    if (s[len] == '\0' && su_inet_pton(AF_INET, s, addr) == 1)
      return (void)(*return_addrlen = 4), len;
  }

  return 0;
}

/** Compare two host names or IP addresses
 *
 * Converts valid IP addresses to the binary format before comparing them.
 * Note that IP6-mapped IP4 addresses and IP6-compatible IP4 addresses are
 * compared as IP4 addresses; that is, ::ffff:127.0.0.1, ::127.0.0.1 and
 * 127.0.0.1 all are all equal.
 *
 * @param a IP address or domain name
 * @param b IP address or domain name
 *
 * @retval -1 if a < b
 * @retval 0 if a == b
 * @retval 1 if a > b
 *
 * @since New in @VERSION_1_12_4.
 */
int host_cmp(char const *a, char const *b)
{
  uint8_t a6[16], b6[16];
  size_t alen, blen, asize = 0, bsize = 0;
  int retval;

  if (a == NULL || b == NULL) {
    retval = (a != NULL) - (b != NULL);
  }
  else {
    alen = convert_ip_address(a, a6, &asize);
    blen = convert_ip_address(b, b6, &bsize);

    if (alen > 0 && blen > 0) {
      if (asize < bsize)
	retval = -1;
      else if (asize > bsize)
	retval = 1;
      else
	retval = memcmp(a6, b6, asize);
    }
    else {
      retval = su_strcasecmp(a, b);
    }
  }

  return retval;
}

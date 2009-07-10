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

/**@CFILE url.c
 *
 * Implementation of basic URL parsing and handling.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun 29 22:44:37 2000 ppessi
 */

#include "config.h"

#include <sofia-sip/su_alloc.h>
#include <sofia-sip/bnf.h>
#include <sofia-sip/hostdomain.h>
#include <sofia-sip/url.h>

#include <sofia-sip/string0.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>

/**@def URL_PRINT_FORMAT
 * Format string used when printing url with printf().
 *
 * The macro URL_PRINT_FORMAT is used in format string of printf() or
 * similar printing functions.  A URL can be printed like this:
 * @code
 *   printf("%s received URL " URL_PRINT_FORMAT "\n",
 *          my_name, URL_PRINT_ARGS(url));
 * @endcode
 */

/** @def URL_PRINT_ARGS(u)
 * Argument list used when printing url with printf().
 *
 * The macro URL_PRINT_ARGS() is used to create a stdarg list for printf()
 * or similar printing functions.  Using it, a URL can be printed like this:
 *
 * @code
 *   printf("%s received URL " URL_PRINT_FORMAT "\n",
 *          my_name, URL_PRINT_ARGS(url));
 * @endcode
 */

#define RESERVED        ";/?:@&=+$,"
#define DELIMS          "<>#%\""
#define UNWISE		"{}|\\^[]`"

#define EXCLUDED	RESERVED DELIMS UNWISE

#define UNRESERVED    	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
                      	"abcdefghijklmnopqrstuvwxyz" \
                      	"0123456789" \
                      	"-_.!~*'()"

#define IS_EXCLUDED(u, m32, m64, m96)			\
  (u <= ' '						\
   || u >= '\177'					\
   || (u < 64 ? (m32 & (1 << (63 - u)))			\
       : (u < 96 ? (m64 & (1 << (95 - u)))		\
	  : /*u < 128*/ (m96 & (1 << (127 - u))))) != 0)

#define MASKS_WITH_RESERVED(reserved, m32, m64, m96)		\
  if (reserved == NULL) {					\
    m32 = 0xbe19003f, m64 = 0x8000001e, m96 = 0x8000001d;	\
  } else do {							\
    m32 = 0xb400000a, m64 = 0x0000001e, m96 = 0x8000001d;	\
    								\
    for (;reserved[0]; reserved++) {				\
      unsigned r = reserved[0];					\
      RESERVE(r, m32, m64, m96);				\
    }								\
  } while (0)

#define RESERVE(reserved, m32, m64, m96)				\
  if (r < 32)								\
    ;									\
  else if (r < 64)							\
    m32 |= 1U << (63 - r);						\
  else if (r < 96)							\
    m64 |= 1U << (95 - r);						\
  else if (r < 128)							\
    m96 |= 1U << (127 - r)

#define MASKS_WITH_ALLOWED(allowed, mask32, mask64, mask96)	\
  do {								\
    if (allowed) {						\
      for (;allowed[0]; allowed++) {				\
	unsigned a = allowed[0];				\
	ALLOW(a, mask32, mask64, mask96);			\
      }								\
    }								\
  } while (0)

#define ALLOW(a, mask32, mask64, mask96)	\
  if (a < 32)					\
    ;						\
  else if (a < 64)				\
    mask32 &= ~(1U << (63 - a));		\
  else if (a < 96)				\
    mask64 &= ~(1U << (95 - a));		\
  else if (a < 128)				\
    mask96 &= ~(1U << (127 - a))

#define NUL '\0'
#define NULNULNUL '\0', '\0', '\0'

#define RMASK1 0xbe19003f
#define RMASK2 0x8000001e
#define RMASK3 0x8000001d

#define RESERVED_MASK 0xbe19003f, 0x8000001e, 0x8000001d
#define URIC_MASK     0xb400000a, 0x0000001e, 0x8000001d

#define IS_EXCLUDED_MASK(u, m) IS_EXCLUDED(u, m)

/* Internal prototypes */
static char *url_canonize(char *d, char const *s, size_t n,
			  unsigned syn33,
			  char const allowed[]);
static char *url_canonize2(char *d, char const *s, size_t n,
			   unsigned syn33,
			   unsigned m32, unsigned m64, unsigned m96);
static int url_tel_cmp_numbers(char const *A, char const *B);

/**Test if string contains excluded or url-reserved characters.
 *
 *
 *
 * @param s  string to be searched
 *
 * @retval 0 if no reserved characters were found.
 * @retval l if a reserved character was found.
 */
int url_reserved_p(char const *s)
{
  if (s)
    while (*s) {
      unsigned char u = *s++;

      if (IS_EXCLUDED(u, RMASK1, RMASK2, RMASK3))
	return 1;
    }

  return 0;
}

/** Calculate length of string when escaped with %-notation.
 *
 * Calculate the length of string @a s when the excluded or reserved
 * characters in it have been escaped.
 *
 * @param s         String with reserved URL characters. [IN
 * @param reserved  Optional array of reserved characters [IN]
 *
 * @return
 * The number of characters in corresponding but escaped string.
 *
 * You can handle a part of URL with reserved characters like this:
 * @code
 * if (url_reserved_p(s))  {
 *   n = malloc(url_esclen(s, NULL) + 1);
 *   if (n) url_escape(n, s);
 * } else {
 *   n = malloc(strlen(s) + 1);
 *   if (n) strcpy(n, s);
 * }
 * @endcode
 */
isize_t url_esclen(char const *s, char const reserved[])
{
  size_t n;
  unsigned mask32, mask64, mask96;

  MASKS_WITH_RESERVED(reserved, mask32, mask64, mask96);

  for (n = 0; s && *s; n++) {
    unsigned char u = *s++;

    if (IS_EXCLUDED(u, mask32, mask64, mask96))
      n += 2;
  }

  return (isize_t)n;
}

/** Escape a string.
 *
 * The function url_escape() copies the string pointed by @a s to the array
 * pointed by @a d, @b excluding the terminating \\0 character.  All reserved
 * characters in @a s are copied in hexadecimal format, for instance, @c
 * "$%#" is copied as @c "%24%25%23".  The destination array @a d must be
 * large enough to receive the escaped copy.
 *
 * @param d         Destination buffer [OUT]
 * @param s         String to be copied [IN]
 * @param reserved  Array of reserved characters [IN]
 *
 * @return Pointer to the destination array.
 */
char *url_escape(char *d, char const *s, char const reserved[])
{
  char *retval = d;
  unsigned mask32, mask64, mask96;

  MASKS_WITH_RESERVED(reserved, mask32, mask64, mask96);

  while (s && *s) {
    unsigned char u = *s++;

    if (IS_EXCLUDED(u, mask32, mask64, mask96)) {
#     define URL_HEXIFY(u) ((u) + '0' + ((u) >= 10 ? 'A' - '0' - 10 : 0))

      *d++ = '%';
      *d++ = URL_HEXIFY(u >> 4);
      *d++ = URL_HEXIFY(u & 15);

#     undef URL_HEXIFY
    }
    else {
      *d++ = u;
    }
  }

  *d = '\0';

  return retval;
}


/**Unescape url-escaped string fragment.
 *
 * Unescape @a n characters from string @a s to the buffer @a d, including
 * the terminating \\0 character. All %-escaped triplets in @a s are
 * unescaped, for instance, @c "%40%25%23" is copied as @c "@%#". The
 * destination array @a d must be large enough to receive the escaped copy
 * (@a n bytes is always enough).
 *
 * @param d  destination buffer
 * @param s  string to be unescaped
 * @param n  maximum number of characters to unescape
 *
 * @return Length of unescaped string
 *
 * @NEW_1_12_4.
 */
size_t url_unescape_to(char *d, char const *s, size_t n)
{
  size_t i = 0, j = 0;

  if (s == NULL)
    return 0;

  i = j = strncspn(s, n, "%");

  if (d && d != s)
    memmove(d, s, i);

  for (; i < n;) {
    char c = s[i++];

    if (c == '\0')
      break;

    if (c == '%' && i + 1 < n && IS_HEX(s[i]) && IS_HEX(s[i + 1])) {
#define   UNHEX(a) (a - (a >= 'a' ? 'a' - 10 : (a >= 'A' ? 'A' - 10 : '0')))
      c = (UNHEX(s[i]) << 4) | UNHEX(s[i + 1]);
#undef    UNHEX
      i += 2;
    }

    if (d)
      d[j] = c;
    j++;
  }

  return j;
}

/**Unescape url-escaped string.
 *
 * Unescape string @a s to the buffer @a d, including the terminating \\0
 * character. All %-escaped triplets in @a s are unescaped, for instance, @c
 * "%40%25%23" is copied as @c "@%#". The destination array @a d must be
 * large enough to receive the escaped copy.
 *
 * @param d  destination buffer
 * @param s  string to be copied
 *
 * @return Pointer to the destination buffer.
 */
char *url_unescape(char *d, char const *s)
{
  size_t n = url_unescape_to(d, s, SIZE_MAX);
  if (d)
    d[n] = '\0';
  return d;
}

/** Canonize a URL component */
static
char *url_canonize(char *d, char const *s, size_t n,
		   unsigned syn33,
		   char const allowed[])
{
  unsigned mask32 = 0xbe19003f, mask64 = 0x8000001e, mask96 = 0x8000001d;

  MASKS_WITH_ALLOWED(allowed, mask32, mask64, mask96);

  return url_canonize2(d, s, n, syn33, mask32, mask64, mask96);
}

#define SYN33(c) (1U << (c - 33))
#define IS_SYN33(syn33, c) ((syn33 & (1U << (c - 33))) != 0)

/** Canonize a URL component (with precomputed mask) */
static
char *url_canonize2(char *d, char const * const s, size_t n,
		    unsigned syn33,
		    unsigned m32, unsigned m64, unsigned m96)
{
  size_t i = 0;

  if (d == s)
    for (;s[i] && i < n; d++, i++)
      if (s[i] == '%')
	break;

  for (;s[i] && i < n; d++, i++) {
    unsigned char c = s[i], h1, h2;

    if (c != '%') {
      if (!IS_SYN33(syn33, c) && IS_EXCLUDED(c, m32, m64, m96))
	return NULL;
      *d = c;
      continue;
    }

    h1 = s[i + 1], h2 = s[i + 2];

    if (!IS_HEX(h1) || !IS_HEX(h2)) {
      *d = '\0';
      return NULL;
    }

#define UNHEX(a) (a - (a >= 'a' ? 'a' - 10 : (a >= 'A' ? 'A' - 10 : '0')))
    c = (UNHEX(h1) << 4) | UNHEX(h2);

    if (!IS_EXCLUDED(c, m32, m64, m96)) {
      /* Convert hex to normal character */
      *d = c, i += 2;
      continue;
    }

    /* Convert hex to uppercase */
    if (h1 >= 'a' /* && h1 <= 'f' */)
      h1 = h1 - 'a' + 'A';
    if (h2 >= 'a' /* && h2 <= 'f' */)
      h2 = h2 - 'a' + 'A';

    d[0] = '%', d[1] = h1, d[2] = h2;

    d +=2, i += 2;
#undef    UNHEX
  }

  *d = '\0';

  return d;
}


/** Canonize a URL component (with precomputed mask).
 *
 * This version does not flag error if *s contains character that should
 * be escaped.
 */
static
char *url_canonize3(char *d, char const * const s, size_t n,
		    unsigned m32, unsigned m64, unsigned m96)
{
  size_t i = 0;

  if (d == s)
    for (;s[i] && i < n; d++, i++)
      if (s[i] == '%')
	break;

  for (;s[i] && i < n; d++, i++) {
    unsigned char c = s[i], h1, h2;

    if (c != '%') {
      *d = c;
      continue;
    }

    h1 = s[i + 1], h2 = s[i + 2];

    if (!IS_HEX(h1) || !IS_HEX(h2)) {
      *d = '\0';
      return NULL;
    }

#define UNHEX(a) (a - (a >= 'a' ? 'a' - 10 : (a >= 'A' ? 'A' - 10 : '0')))
    c = (UNHEX(h1) << 4) | UNHEX(h2);

    if (!IS_EXCLUDED(c, m32, m64, m96)) {
      *d = c, i += 2;
      continue;
    }

    /* Convert hex to uppercase */
    if (h1 >= 'a' /* && h1 <= 'f' */)
      h1 = h1 - 'a' + 'A';
    if (h2 >= 'a' /* && h2 <= 'f' */)
      h2 = h2 - 'a' + 'A';

    d[0] = '%', d[1] = h1, d[2] = h2;

    d +=2, i += 2;
#undef    UNHEX
  }

  *d = '\0';

  return d;
}


/** Get URL scheme. */
char const* url_scheme(enum url_type_e url_type)
{
  switch (url_type) {
  case url_any:    return "*";
  case url_sip:    return "sip";
  case url_sips:   return "sips";
  case url_tel:    return "tel";
  case url_fax:    return "fax";
  case url_modem:  return "modem";
  case url_http:   return "http";
  case url_https:  return "https";
  case url_ftp:    return "ftp";
  case url_file:   return "file";
  case url_rtsp:   return "rtsp";
  case url_rtspu:  return "rtspu";
  case url_mailto: return "mailto";
  case url_im:     return "im";
  case url_pres:   return "pres";
  case url_cid:    return "cid";
  case url_msrp:   return "msrp";
  case url_msrps:  return "msrps";
  case url_wv:     return "wv";
  default:
    assert(url_type == url_unknown);
    return NULL;
  }
}

su_inline
int url_type_is_opaque(enum url_type_e url_type)
{
  return
    url_type == url_invalid ||
    url_type == url_tel ||
    url_type == url_modem ||
    url_type == url_fax ||
    url_type == url_cid;
}

/** Init an url as given type */
void url_init(url_t *url, enum url_type_e type)
{
  memset(url, 0, sizeof(*url));
  url->url_type = type;
  if (type > url_unknown) {
    char const *scheme = url_scheme((enum url_type_e)url->url_type);
    if (scheme)
      url->url_scheme = scheme;
  }
}

/** Get url type */
su_inline
enum url_type_e url_get_type(char const *scheme, size_t len)
{
#define test_scheme(s) \
   if (len == strlen(#s) && !strncasecmp(scheme, #s, len)) return url_##s

  switch (scheme[0]) {
  case '*': if (strcmp(scheme, "*") == 0) return url_any;
  case 'c': case 'C':
    test_scheme(cid); break;
  case 'f': case 'F':
    test_scheme(ftp); test_scheme(file); test_scheme(fax); break;
  case 'h': case 'H':
    test_scheme(http); test_scheme(https); break;
  case 'i': case 'I':
    test_scheme(im); break;
  case 'm': case 'M':
    test_scheme(mailto); test_scheme(modem);
    test_scheme(msrp); test_scheme(msrps); break;
  case 'p': case 'P':
    test_scheme(pres); break;
  case 'r': case 'R':
    test_scheme(rtsp); test_scheme(rtspu); break;
  case 's': case 'S':
    test_scheme(sip); test_scheme(sips); break;
  case 't': case 'T':
    test_scheme(tel); break;
  case 'w': case 'W':
    test_scheme(wv); break;


  default: break;
  }

#undef test_scheme

  if (len != span_unreserved(scheme))
    return url_invalid;
  else
    return url_unknown;
}

/**
 * Decode a URL.
 *
 * This function decodes a (SIP) URL string to a url_t structure.
 *
 * @param url structure to store the parsing result
 * @param s   NUL-terminated string to be parsed
 *
 * @note The parsed string @a s will be modified when parsing it.
 *
 * @retval 0 if successful,
 * @retval -1 otherwise.
 */
static
int _url_d(url_t *url, char *s)
{
  size_t n, p;
  char *s0, rest_c, *host, *user;
  int have_authority = 1;

  memset(url, 0, sizeof(*url));

  if (strcmp(s, "*") == 0) {
    url->url_type = url_any;
    url->url_scheme = "*";
    return 0;
  }

  s0 = s;

  n = strcspn(s, ":/?#");

  if (n && s[n] == ':') {
    char *scheme;
    url->url_scheme = scheme = s; s[n] = '\0'; s = s + n + 1;

    if (!(scheme = url_canonize(scheme, scheme, SIZE_MAX, 0, "+")))
      return -1;

    n = scheme - url->url_scheme;

    url->url_type = url_get_type(url->url_scheme, n);

    have_authority = !url_type_is_opaque((enum url_type_e)url->url_type);
  }
  else {
    url->url_type = url_unknown;
  }

  user = NULL, host = s;

  if (url->url_type == url_sip || url->url_type == url_sips) {
    /* SIP URL may have /;? in user part but no path */
    /* user-unreserved  =  "&" / "=" / "+" / "$" / "," / ";" / "?" / "/" */
    /* Some #*@#* phones include unescaped # there, too */
    n = strcspn(s, "@/;?#");
    p = strcspn(s + n, "@");
    if (s[n + p] == '@') {
      n += p;
      user = s;
      host = s + n + 1;
    }

    n += strcspn(s + n, "/;?#");
  }
  else if (have_authority) {
    if (url->url_type == url_wv) {
      /* WV URL may have / in user part */
      n = strcspn(s, "@#?;");
      if (s[n] == '@') {
	user = s;
	host = s + n + 1;
	n += strcspn(s + n, ";?#");
      }
    }
    else if (host[0] == '/' && host[1] != '/') {
      /* foo:/bar or /bar - no authority, just path */
      url->url_root = '/';	/* Absolute path */
      host = NULL, n = 0;
    }
    else {
      if (host[0] == '/' && host[1] == '/') {
	/* We have authority, / / foo or foo */
	host += 2; s += 2, url->url_root = '/';
	n = strcspn(s, "/?#@[]");
      }
      else
	n = strcspn(s, "@;/?#");

      if (s[n] == '@')
	user = host, host = user + n + 1;

      n += strcspn(s + n, ";/?#");	/* Find path, query and/or fragment */
    }
  }
  else /* !have_authority */ {
    user = host, host = NULL;
    if (url->url_type != url_invalid)
      n = strcspn(s, "/;?#");	/* Find params, query and/or fragment */
    else
      n = strcspn(s, "#");
  }

  rest_c = s[n]; s[n] = 0; s = rest_c ? s + n + 1 : NULL;

  if (user) {
    if (host) host[-1] = '\0';
    url->url_user = user;
    if (url->url_type != url_unknown) {
      n = strcspn(user, ":");
      if (user[n]) {
	user[n] = '\0';
	url->url_password = user + n + 1;
      }
    }
  }

  if (host) {
    url->url_host = host;
    /* IPv6 (and in some cases, IPv4) addresses are quoted with [] */
    if (host[0] == '[') {
      n = strcspn(host, "]");
      if (host[n] && (host[n + 1] == '\0' || host[n + 1] == ':'))
	n++;
      else
	n = 0;
    }
    else {
      n = strcspn(host, ":");
    }

    /* We allow empty host by default */
    if (n == 0) switch (url->url_type) {
    case url_sip:
    case url_sips:
    case url_im:
    case url_pres:
      return -1;
    default:
      break;
    }

    if (host[n] == ':') {
      char *port = host + n + 1;
      url->url_port = port;
      switch (url->url_type) {
      case url_any:
      case url_sip:
      case url_sips:
      case url_http:
      case url_https:
      case url_ftp:
      case url_file:
      case url_rtsp:
      case url_rtspu:
	if (!url_canonize2(port, port, SIZE_MAX, 0, RESERVED_MASK))
	  return -1;

	/* Check that port is really numeric or wildcard */
	/* Port can be *digit, empty string or "*" */
	while (*port >= '0' && *port <= '9')
	  port++;

	if (port != url->url_port) {
	  if (port[0] != '\0')
	    return -1;
	}
	else if (port[0] == '\0')
	  /* empty string */;
	else if (port[0] == '*' && port[1] == '\0')
	  /* wildcard */;
	else
	  return -1;
      }
      host[n] = 0;
    }
  }

  if (rest_c == '/') {
    url->url_path = s; n = strcspn(s, "?#");
    rest_c = s[n]; s[n] = 0; s = rest_c ? s + n + 1 : NULL;
  }
  if (rest_c == ';') {
    url->url_params = s; n = strcspn(s, "?#");
    rest_c = s[n]; s[n] = 0; s = rest_c ? s + n + 1 : NULL;
  }
  if (rest_c == '?') {
    url->url_headers = s; n = strcspn(s, "#");
    rest_c = s[n]; s[n] = 0; s = rest_c ? s + n + 1 : NULL;
  }
  if (rest_c == '#') {
    url->url_fragment = s;
    rest_c = '\0';
  }
  if (rest_c)
    return -1;

  return 0;
}

/* Unreserved things */

/**
 * Decode a URL.
 *
 * This function decodes a URL string to a url_t structure.
 *
 * @param url structure to store the parsing result
 * @param s   NUL-terminated string to be parsed
 *
 * @note The parsed string @a s will be modified when parsing it.
 *
 * @retval 0 if successful,
 * @retval -1 otherwise.
 */
int url_d(url_t *url, char *s)
{
  if (url == NULL || _url_d(url, s) < 0)
    return -1;

  /* Canonize  URL */
  /* scheme is canonized by _url_d() */
  if (url->url_type == url_sip || url->url_type == url_sips) {

#   define SIP_USER_UNRESERVED "&=+$,;?/"
    s = (char *)url->url_user;
    if (s && !url_canonize(s, s, SIZE_MAX, 0, SIP_USER_UNRESERVED))
      return -1;

    /* Having different charset in user and password does not make sense */
    /* but that is how it is defined in RFC 3261 */
#   define SIP_PASS_UNRESERVED "&=+$,"
    s = (char *)url->url_password;
    if (s && !url_canonize(s, s, SIZE_MAX, 0, SIP_PASS_UNRESERVED))
      return -1;

  }
  else {

#   define USER_UNRESERVED "&=+$,;"
    s = (char *)url->url_user;
    if (s && !url_canonize(s, s, SIZE_MAX, 0, USER_UNRESERVED))
      return -1;

#   define PASS_UNRESERVED "&=+$,;:"
    s = (char *)url->url_password;
    if (s && !url_canonize(s, s, SIZE_MAX, 0, PASS_UNRESERVED))
      return -1;
  }

  s = (char *)url->url_host;
  if (s && !url_canonize2(s, s, SIZE_MAX, 0, RESERVED_MASK))
    return -1;

  /* port is canonized by _url_d() */
  s = (char *)url->url_path;
  if (s && !url_canonize(s, s, SIZE_MAX,
			 /* Allow all URI characters but ? */
			 /* Allow unescaped /;?@, - but do not convert */
			 SYN33('/') | SYN33(';') | SYN33('=') | SYN33('@') |
			 SYN33(','),
			 /* Convert escaped :&+$ to unescaped */
			 ":&+$"))
    return -1;

  s = (char *)url->url_params;
  if (s && !url_canonize(s, s, SIZE_MAX,
			 /* Allow all URI characters but ? */
			 /* Allow unescaped ;=@, - but do not convert */
			 SYN33(';') | SYN33('=') | SYN33('@') | SYN33(','),
			 /* Convert escaped /:&+$ to unescaped */
			 "/:&+$"))
    return -1;

  /* Unhex alphanumeric and unreserved URI characters */
  s = (char *)url->url_headers;
  if (s && !url_canonize3(s, s, SIZE_MAX, RESERVED_MASK))
    return -1;

  /* Allow all URI characters (including reserved ones) */
  s = (char *)url->url_fragment;
  if (s && !url_canonize2(s, s, SIZE_MAX, 0, URIC_MASK))
    return -1;

  return 0;
}

/** Encode an URL.
 *
 * The function url_e() combines a URL from substrings in url_t structure
 * according the @ref url_syntax "URL syntax" presented above.  The encoded
 * @a url is stored in a @a buffer of @a n bytes.
 *
 * @param buffer memory area to store the encoded @a url.
 * @param n      size of @a buffer.
 * @param url    URL to be encoded.
 *
 * @return
 * Return the number of bytes in the encoding.
 *
 * @note The function follows the convention set by C99 snprintf().  Even if
 * the result does not fit into the @a buffer and it is truncated, the
 * function returns the number of bytes in an untruncated encoding.
 */
issize_t url_e(char buffer[], isize_t n, url_t const *url)
{
  size_t i;
  char *b = buffer;
  size_t m = n;
  int do_copy = n > 0;

  if (url == NULL)
    return -1;

  if (URL_STRING_P(url)) {
    char const *u = (char *)url;
    i = strlen(u);
    if (!buffer)
      return i;

    if (i >= n) {
      memcpy(buffer, u, n - 2);
      buffer[n - 1] = '\0';
    } else {
      memcpy(buffer, u, i + 1);
    }

    return i;
  }


  if (url->url_type == url_any) {
    if (b && m > 0) {
      if (m > 1) strcpy(b, "*"); else b[0] = '\0';
    }
    return 1;
  }

  if (url->url_scheme && url->url_scheme[0]) {
    i = strlen(url->url_scheme) + 1;
    if (do_copy && (do_copy = i <= n)) {
      memcpy(b, url->url_scheme, i - 1);
      b[i - 1] = ':';
    }
    b += i; n -= i;
  }

  if (url->url_root && (url->url_host || url->url_user)) {
    if (do_copy && (do_copy = 2 <= n))
      memcpy(b, "//", 2);
    b += 2; n -= 2;
  }

  if (url->url_user) {
    i = strlen(url->url_user);
    if (do_copy && (do_copy = i <= n))
      memcpy(b, url->url_user, i);
    b += i; n -= i;

    if (url->url_password) {
      if (do_copy && (do_copy = 1 <= n))
	*b = ':';
      b++; n--;
      i = strlen(url->url_password);
      if (do_copy && (do_copy = i <= n))
	memcpy(b, url->url_password, i);
      b += i; n -= i;
    }

    if (url->url_host) {
      if (do_copy && (do_copy = 1 <= n))
	*b = '@';
      b++; n--;
    }
  }

  if (url->url_host) {
    i = strlen(url->url_host);
    if (do_copy && (do_copy = i <= n))
      memcpy(b, url->url_host, i);
    b += i; n -= i;

    if (url->url_port) {
      i = strlen(url->url_port) + 1;
      if (do_copy && (do_copy = i <= n)) {
	b[0] = ':';
	memcpy(b + 1, url->url_port, i - 1);
      }
      b += i; n -= i;
    }
  }

  if (url->url_path) {
    if (url->url_root) {
      if (do_copy && (do_copy = 1 <= n))
	b[0] = '/';
      b++, n--;
    }
    i = strlen(url->url_path);
    if (do_copy && (do_copy = i < n))
      memcpy(b, url->url_path, i);
    b += i; n -= i;
  }

  {
    static char const sep[] = ";?#";
    char const *pp[3];
    size_t j;

    pp[0] = url->url_params;
    pp[1] = url->url_headers;
    pp[2] = url->url_fragment;

    for (j = 0; j < 3; j++) {
      char const *p = pp[j];
      if (!p) continue;
      i = strlen(p) + 1;
      if (do_copy && (do_copy = i <= n)) {
	*b = sep[j];
	memcpy(b + 1, p, i - 1);
      }
      b += i; n -= i;
    }
  }

  if (do_copy && (do_copy = 1 <= n))
    *b = '\0';
  else if (buffer && m > 0)
    buffer[m - 1] = '\0';

  assert((size_t)(b - buffer) == (size_t)(m - n));

  /* This follows the snprintf(C99) return value,
   * Number of characters written (excluding NUL)
   */
  return b - buffer;
}


/** Calculate the length of URL when encoded.
 *
 */
isize_t url_len(url_t const * url)
{
  size_t rv = 0;

  if (url->url_scheme) rv += strlen(url->url_scheme) + 1; /* plus ':' */
  if (url->url_user) {
    rv += strlen(url->url_user);
    if (url->url_password)
      rv += strlen(url->url_password) + 1;   /* plus ':' */
    rv += url->url_host != NULL;  /* plus '@' */
  }
  if (url->url_host) rv += strlen(url->url_host);
  if (url->url_port) rv += strlen(url->url_port) + 1;	        /* plus ':' */
  if (url->url_path) rv += strlen(url->url_path) + 1;     /* plus initial / */
  if (url->url_params) rv += strlen(url->url_params) + 1; /* plus initial ; */
  if (url->url_headers) rv += strlen(url->url_headers) + 1;	/* plus '?' */
  if (url->url_fragment) rv += strlen(url->url_fragment) + 1;   /* plus '#' */

  return rv;
}

/**@def URL_E(buf, end, url)
 * Encode an URL: use @a buf up to @a end.
 * @hideinitializer
 */

/**
 * Calculate the size of strings associated with a #url_t sructure.
 *
 * @param url pointer to a #url_t structure or string
 * @return Number of bytes for URL
 */
isize_t url_xtra(url_t const *url)
{
  size_t xtra;

  if (URL_STRING_P(url)) {
    xtra = strlen((char const *)url) + 1;
  }
  else {
    size_t len_scheme, len_user, len_password,
      len_host, len_port, len_path, len_params,
      len_headers, len_fragment;

    len_scheme = (url->url_type <= url_unknown && url->url_scheme) ?
      strlen(url->url_scheme) + 1 : 0;
    len_user = url->url_user ? strlen(url->url_user) + 1 : 0;
    len_password = url->url_password ? strlen(url->url_password) + 1 : 0;
    len_host = url->url_host ? strlen(url->url_host) + 1 : 0;
    len_port = url->url_port ? strlen(url->url_port) + 1 : 0;
    len_path = url->url_path ? strlen(url->url_path) + 1 : 0;
    len_params = url->url_params ? strlen(url->url_params) + 1 : 0;
    len_headers = url->url_headers ? strlen(url->url_headers) + 1 : 0;
    len_fragment = url->url_fragment ? strlen(url->url_fragment) + 1 : 0;

    xtra =
      len_scheme + len_user + len_password + len_host + len_port +
      len_path + len_params + len_headers + len_fragment;
  }

  return xtra;
}

su_inline
char *copy(char *buf, char *end, char const *src)
{
#if HAVE_MEMCCPY
  char *b = memccpy(buf, src, '\0', end - buf);
  if (b)
    return b;
  else
    return end + strlen(src + (end - buf)) + 1;
#else
  for (; buf < end && (*buf = *src); buf++, src++)
    ;

  if (buf >= end)
    while (*src++)
      buf++;

  return buf + 1;
#endif
}

/**
 * Duplicate the url.
 *
 * The function url_dup() copies the url structure @a src and the strings
 * attached to it to @a url.  The non-constant strings in @a src are copied
 * to @a buf.  If the size of duplicated strings exceed @a bufsize, the
 * corresponding string fields in @a url are set to NULL.
 *
 * The calling function can calculate the size of buffer required by calling
 * url_dup() with zero as @a bufsize and NULL as @a dst.

 * @param buf     Buffer for non-constant strings copied from @a src.
 * @param bufsize Size of @a buf.
 * @param dst     Destination URL structure.
 * @param src     Source URL structure.
 *
 * @return Number of characters required for
 * duplicating the strings in @a str, or -1 if an error
 * occurred.
 */
issize_t url_dup(char *buf, isize_t bufsize, url_t *dst, url_t const *src)
{
  if (!src && !dst)
    return -1;
  else if (URL_STRING_P(src)) {
    size_t n = strlen((char *)src) + 1;
    if (n > bufsize || dst == NULL)
      return n;

    strcpy(buf, (char *)src);
    memset(dst, 0, sizeof(*dst));
    if (url_d(dst, buf) < 0)
      return -1;

    return n;
  }
  else {
    char *b = buf;
    char *end = b + bufsize;
    char const **dstp;
    char const * const *srcp;
    url_t dst0[1];

    if (dst == NULL)
      dst = dst0;

    memset(dst, 0, sizeof(*dst));

    if (!src)
      return 0;

    memset(dst->url_pad, 0, sizeof dst->url_pad);
    dst->url_type = src->url_type;
    dst->url_root = src->url_root;

    dstp = &dst->url_scheme;
    srcp = &src->url_scheme;

    if (dst->url_type > url_unknown)
      *dstp = url_scheme((enum url_type_e)dst->url_type);

    if (*dstp != NULL)
      dstp++, srcp++;	/* Skip scheme if it is constant */

    if (dst != dst0 && buf != NULL && bufsize != 0)
      for (; srcp <= &src->url_fragment; srcp++, dstp++)
	if (*srcp) {
	  char *next = copy(b, end, *srcp);

	  if (next > end)
	    break;

	  *dstp = b, b = next;
	}

    for (; srcp <= &src->url_fragment; srcp++)
      if (*srcp) {
	b += strlen(*srcp) + 1;
      }

    return b - buf;
  }
}

/**@def URL_DUP(buf, end, dst, src)
 *  Duplicate the url: use @a buf up to @a end. @HI
 *
 * The macro URL_DUP() duplicates the url.  The non-constant strings in @a
 * src are copied to @a buf.  However, no strings are copied past @a end.
 * In other words, the size of buffer is @a end - @a buf.
 *
 * The macro updates the buffer pointer @a buf, so that it points to the
 * first unused byte in the buffer.  The buffer pointer @a buf is updated,
 * even if the buffer is too small for the duplicated strings.
 *
 * @param buf     Buffer for non-constant strings copied from @a src.
 * @param end     End of @a buf.
 * @param dst     Destination URL structure.
 * @param src     Source URL structure.
 *
 * @return
 * The macro URL_DUP() returns pointer to first unused byte in the
 * buffer @a buf.
 */

/** Duplicate the url to memory allocated via home.
 *
 * The function url_hdup() duplicates (deep copies) an #url_t structure.
 * Alternatively, it can be passed a string; string is then copied and
 * parsed to the #url_t structure.
 *
 * The function url_hdup() allocates the destination structure from @a home
 * as a single memory block. It is possible to free the copied url structure
 * and all the associated strings using a single call to su_free().
 *
 * @param home memory home used to allocate new url object
 * @param src  pointer to URL (or string)
 *
 * @return
 * The function url_hdup() returns a pointer to the newly allocated #url_t
 * structure, or NULL upon an error.
 */
url_t *url_hdup(su_home_t *home, url_t const *src)
{
  if (src) {
    size_t len = sizeof(*src) + url_xtra(src);
    url_t *dst = su_alloc(home, len);
    if (dst) {
      ssize_t actual;
      actual = url_dup((char *)(dst + 1), len - sizeof(*src), dst, src);
      if (actual < 0)
	su_free(home, dst), dst = NULL;
      else
	assert(len == sizeof(*src) + actual);
    }
    return dst;
  }
  else
    return NULL;
}


/** Convert an string to an url */
url_t *url_make(su_home_t *h, char const *str)
{
  return url_hdup(h, URL_STRING_MAKE(str)->us_url);
}

/** Print an URL */
url_t *url_format(su_home_t *h, char const *fmt, ...)
{
  url_t *url;
  char *us;
  va_list ap;

  va_start(ap, fmt);

  us = su_vsprintf(h, fmt, ap);

  va_end(ap);

  if (us == NULL)
    return NULL;

  url = url_hdup(h, URL_STRING_MAKE(us)->us_url);

  su_free(h, us);

  return url;
}


/** Convert @a url to a string allocated from @a home.
 *
 * @param home memory home to allocate the new string
 * @param url  url to convert to string
 *
 * The @a url can be a string, too.
 *
 * @return Newly allocated conversion result, or NULL upon an error.
 */
char *url_as_string(su_home_t *home, url_t const *url)
{
  if (url) {
    int len = url_e(NULL, 0, url);
    char *b = su_alloc(home, len + 1);
    url_e(b, len + 1, url);
    return b;
  } else {
    return NULL;
  }
}


/** Test if param @a tag matches to parameter string @a p.
 */
#define URL_PARAM_MATCH(p, tag) \
 (strncasecmp(p, tag, strlen(tag)) == 0 && \
  (p[strlen(tag)] == '\0' || p[strlen(tag)] == ';' || p[strlen(tag)] == '='))

/**
 * Search for a parameter.
 *
 * This function searches for a parameter from a parameter list.
 *
 * If you want to test if there is parameter @b user=phone,
 * call this function like
 * @code if (url_param(url->url_param, "user=phone", NULL, 0))
 * @endcode
 *
 * @param params URL parameter string (excluding first semicolon)
 * @param tag    parameter name
 * @param value  string to which the parameter value is copied
 * @param vlen   length of string reserved for value
 *
 * @retval positive length of parameter value (including final NUL) if found
 * @retval zero     if not found.
 */
isize_t url_param(char const *params,
		  char const *tag,
		  char value[], isize_t vlen)
{
  size_t n, tlen, flen;
  char *p;

  if (!params)
    return 0;

  tlen = strlen(tag);
  if (tlen && tag[tlen - 1] == '=')
    tlen--;

  for (p = (char *)params; *p; p += n + 1) {
    n = strcspn(p, ";");
    if (n < tlen) {
      if (p[n]) continue; else break;
    }
    if (strncasecmp(p, tag, tlen) == 0) {
      if (n == tlen) {
	if (vlen > 0)
	  value[0] = '\0';
	return 1;
      }
      if (p[tlen] != '=')
	continue;
      flen = n - tlen - 1;
      if (flen >= (size_t)vlen)
	return flen + 1;
      memcpy(value, p + tlen + 1, flen);
      value[flen] = '\0';
      return flen + 1;
    }
    if (!p[n])
      break;
  }

  return 0;
}

/** Check for a parameter.
 *
 * @deprecated
 * Bad grammar. Use url_has_param().
 */
isize_t url_have_param(char const *params, char const *tag)
{
  return url_param(params, tag, NULL, 0);
}

/** Check for a parameter. */
int url_has_param(url_t const *url, char const *tag)
{
  return url && url->url_params && url_param(url->url_params, tag, NULL, 0);
}

/** Add an parameter. */
int url_param_add(su_home_t *h, url_t *url, char const *param)
{
  /* XXX - should remove existing parameters with same name? */
  size_t n = url->url_params ? strlen(url->url_params) + 1: 0;
  size_t nn = strlen(param) + 1;
  char *s = su_alloc(h, n + nn);

  if (!s)
    return -1;

  if (url->url_params)
    strcpy(s, url->url_params)[n - 1] = ';';
  strcpy(s + n, param);
  url->url_params = s;

  return 0;
}

/** Remove a named parameter from url_param string.
 *
 * Remove a named parameter and its possible value from the URL parameter
 * string (url_s##url_param).
 *
 * @return Pointer to modified string, or NULL if nothing is left in there.
 */
char *url_strip_param_string(char *params, char const *name)
{
  if (params && name) {
    size_t i, n = strlen(name), remove, rest;

    for (i = 0; params[i];) {
      if (strncasecmp(params + i, name, n) ||
	  (params[i + n] != '=' && params[i + n] != ';' && params[i + n])) {
	i = i + strcspn(params + i, ";");
	if (!params[i++])
	  break;
	continue;
      }
      remove = n + strcspn(params + i + n, ";");
      if (params[i + remove] == ';')
	remove++;

      if (i == 0) {
	params += remove;
	continue;
      }

      rest = strlen(params + i + remove);
      if (!rest) {
	if (i == 0)
	  return NULL;		/* removed everything */
	params[i - 1] = '\0';
	break;
      }
      memmove(params + i, params + i + remove, rest + 1);
    }

    if (!params[0])
      return NULL;
  }

  return params;
}

int url_string_p(url_string_t const *url)
{
  return URL_STRING_P(url);
}

int url_is_string(url_string_t const *url)
{
  return URL_IS_STRING(url);
}

/** Strip transport-specific stuff. */
static
int url_strip_transport2(url_t *url, int modify)
{
  char *p, *d;
  size_t n;
  int semi;

  if (url->url_type != url_sip && url->url_type != url_sips)
    return 0;

  if (url->url_port != NULL) {
    if (!modify)
      return 1;
    url->url_port = NULL;
  }

  if (!url->url_params)
    return 0;

  for (d = p = (char *)url->url_params; *p; p += n + semi) {
    n = strcspn(p, ";");
    semi = (p[n] != '\0');

    if (modify && n == 0)
      continue;
    if (URL_PARAM_MATCH(p, "method"))
      continue;
    if (URL_PARAM_MATCH(p, "maddr"))
      continue;
    if (URL_PARAM_MATCH(p, "ttl"))
      continue;
    if (URL_PARAM_MATCH(p, "transport"))
      continue;

    if (p != d) {
      if (d != url->url_params)
	d++;
      if (p != d) {
	if (!modify)
	  return 1;
	memmove(d, p, n + 1);
      }
    }
    d += n;
  }

  if (d == p)
    return 0;
  else if (d + 1 == p)		/* empty param */
    return 0;
  else if (!modify)
    return 1;

  if (d != url->url_params)
    *d = '\0';
  else
    url->url_params = NULL;

  return 1;
}

/** Strip transport-specific stuff.
 *
 * The function url_strip_transport() removes transport-specific parameters
 * from a SIP or SIPS URI.  These parameters include:
 * - the port number
 * - "maddr=" parameter
 * - "transport=" parameter
 * - "ttl=" parameter
 * - "method=" parameter
 *
 * @note
 * The @a url must be a pointer to a URL structure. It is stripped in-place.
 *
 * @note
 * If the parameter string contains empty parameters, they are stripped, too.
 *
 * @return
 * The function url_strip_transport() returns @e true, if the URL was
 * modified, @e false otherwise.
 */
int url_strip_transport(url_t *url)
{
  return url_strip_transport2(url, 1);
}

/** Check for transport-specific stuff.
 *
 * The function url_have_transport() tests if there are transport-specific
 * parameters in a SIP or SIPS URI. These parameters include:
 * - the port number
 * - "maddr=" parameters
 * - "transport=" parameters
 *
 * @note
 * The @a url must be a pointer to a URL structure.
 *
 * @return The function url_have_transport() returns @e true, if the URL
 * contains transport parameters, @e false otherwise.
 */
int url_have_transport(url_t const *url)
{
  return url_strip_transport2((url_t *)url, 0);
}

/**Lazily compare two URLs.
 *
 * Compare essential parts of URLs: schema, host, port, and username.
 *
 * any_url compares 0 with any other URL.
 *
 * pres: and im: URIs compares 0 with SIP URIs.
 *
 * @note
 * The @a a and @a b must be pointers to URL structures.
 *
 * @note Currently, the url parameters are not compared. This is because the
 * url_cmp() is used to sort URLs: taking parameters into account makes that
 * impossible.
 */
int url_cmp(url_t const *a, url_t const *b)
{
  int rv;
  int url_type;

  if ((a && a->url_type == url_any) || (b && b->url_type == url_any))
    return 0;

  if (!a || !b)
    return (a != NULL) - (b != NULL);

  if ((rv = a->url_type - b->url_type)) {
#if 0
    /* presence and instant messaging URLs match magically with SIP */
    enum url_type_e a_type = a->url_type;
    enum url_type_e b_type = b->url_type;

    if (a_type == url_im || a_type == url_pres)
      a_type = url_sip;

    if (b_type == url_im || b_type == url_pres)
      b_type = url_sip;

    if (a_type != b_type)
#endif
      return rv;
  }

  url_type = a->url_type;	/* Or b->url_type, they are equal! */

  if (url_type <= url_unknown &&
      ((rv = !a->url_scheme - !b->url_scheme) ||
       (a->url_scheme && b->url_scheme &&
	(rv = strcasecmp(a->url_scheme, b->url_scheme)))))
    return rv;

  if ((rv = host_cmp(a->url_host, b->url_host)))
    return rv;

  if (a->url_port != b->url_port) {
    char const *a_port;
    char const *b_port;

    if (url_type != url_sip && url_type != url_sips)
      a_port = b_port = url_port_default((enum url_type_e)url_type);
    else if (host_is_ip_address(a->url_host))
      a_port = b_port = url_port_default((enum url_type_e)url_type);
    else
      a_port = b_port = "";

    if (a->url_port) a_port = a->url_port;
    if (b->url_port) b_port = b->url_port;

    if ((rv = strcmp(a_port, b_port)))
      return rv;
  }

  if (a->url_user != b->url_user) {
    if (a->url_user == NULL) return -1;
    if (b->url_user == NULL) return +1;
    switch (url_type) {
    case url_tel: case url_modem: case url_fax:
      rv = url_tel_cmp_numbers(a->url_user, b->url_user);
      break;
    default:
      rv = strcmp(a->url_user, b->url_user);
      break;
    }
    if (rv)
      return rv;
  }

#if 0
  if (a->url_path != b->url_path) {
    if (a->url_path == NULL) return -1;
    if (b->url_path == NULL) return +1;
    if ((rv = strcmp(a->url_path, b->url_path)))
      return rv;
  }
#endif

  return 0;
}

static
int url_tel_cmp_numbers(char const *A, char const *B)
{
  short a, b;
  int rv;

  while (*A && *B) {
    #define UNHEX(a) (a - (a >= 'a' ? 'a' - 10 : (a >= 'A' ? 'A' - 10 : '0')))
    /* Skip visual-separators */
    do {
      a = *A++;
      if (a == '%' && IS_HEX(A[0]) && IS_HEX(A[1]))
	a = (UNHEX(A[0]) << 4) | UNHEX(A[1]), A +=2;
    } while (a == ' ' || a == '-' || a == '.' || a == '(' || a == ')');

    if (isupper(a))
      a = tolower(a);

    do {
      b = *B++;
      if (b == '%' && IS_HEX(B[0]) && IS_HEX(B[1]))
	b = (UNHEX(B[0]) << 4) | UNHEX(B[1]), B +=2;
    } while (b == ' ' || b == '-' || b == '.' || b == '(' || b == ')');

    if (isupper(b))
      b = tolower(b);

    if ((rv = a - b))
      return rv;
  }

  return (int)*A - (int)*B;
}

/**Conservative comparison of urls.
 *
 * Compare all parts of URLs.
 *
 * @note
 * The @a a and @a b must be pointers to URL structures.
 *
 */
int url_cmp_all(url_t const *a, url_t const *b)
{
  int rv, url_type;

  if (!a || !b)
    return (a != NULL) - (b != NULL);

  if ((rv = a->url_type - b->url_type))
    return rv;

  url_type = a->url_type;	/* Or b->url_type, they are equal! */

  if (url_type <= url_unknown &&
      ((rv = !a->url_scheme - !b->url_scheme) ||
       (a->url_scheme && b->url_scheme &&
	(rv = strcasecmp(a->url_scheme, b->url_scheme)))))
    return rv;

  if ((rv = a->url_root - b->url_root))
    return rv;

  if ((rv = host_cmp(a->url_host, b->url_host)))
    return rv;

  if (a->url_port != b->url_port) {
    char const *a_port;
    char const *b_port;

    if (url_type != url_sip && url_type != url_sips)
      a_port = b_port = url_port_default((enum url_type_e)url_type);
    else if (host_is_ip_address(a->url_host))
      a_port = b_port = url_port_default((enum url_type_e)url_type);
    else
      a_port = b_port = "";

    if (a->url_port) a_port = a->url_port;
    if (b->url_port) b_port = b->url_port;

    if ((rv = strcmp(a_port, b_port)))
      return rv;
  }

  if (a->url_user != b->url_user) {
    if (a->url_user == NULL) return -1;
    if (b->url_user == NULL) return +1;

    switch (url_type) {
    case url_tel: case url_modem: case url_fax:
      rv = url_tel_cmp_numbers(a->url_user, b->url_user);
      break;
    default:
      rv = strcmp(a->url_user, b->url_user);
      break;
    }
    if (rv)
      return rv;
  }

  if (a->url_path != b->url_path) {
    if (a->url_path == NULL) return -1;
    if (b->url_path == NULL) return +1;
    if ((rv = strcmp(a->url_path, b->url_path)))
      return rv;
  }

  if (a->url_params != b->url_params) {
    if (a->url_params == NULL) return -1;
    if (b->url_params == NULL) return +1;
    if ((rv = strcmp(a->url_params, b->url_params)))
      return rv;
  }

  if (a->url_headers != b->url_headers) {
    if (a->url_headers == NULL) return -1;
    if (b->url_headers == NULL) return +1;
    if ((rv = strcmp(a->url_headers, b->url_headers)))
      return rv;
  }

  if (a->url_headers != b->url_headers) {
    if (a->url_headers == NULL) return -1;
    if (b->url_headers == NULL) return +1;
    if ((rv = strcmp(a->url_headers, b->url_headers)))
      return rv;
  }

  if (a->url_fragment != b->url_fragment) {
    if (a->url_fragment == NULL) return -1;
    if (b->url_fragment == NULL) return +1;
    if ((rv = strcmp(a->url_fragment, b->url_fragment)))
      return rv;
  }

  return 0;
}

/** Return default port number corresponding to the url type */
char const *url_port_default(enum url_type_e url_type)
{
  switch (url_type) {
  case url_sip:			/* "sip:" */
    return "5060";
  case url_sips:		/* "sips:" */
    return "5061";
  case url_http:		/* "http:" */
    return "80";
  case url_https:		/* "https:" */
    return "443";
  case url_ftp:			/* "ftp:" */
  case url_file:		/* "file:" */
    return "21";
  case url_rtsp:		/* "rtsp:" */
  case url_rtspu:		/* "rtspu:" */
    return "554";
  case url_mailto:		/* "mailto:" */
    return "25";

  case url_any:			/* "*" */
    return "*";

  case url_msrp:
  case url_msrps:
    return "9999";		/* XXXX */

  case url_tel:
  case url_fax:
  case url_modem:
  case url_im:
  case url_pres:
  case url_cid:
  case url_wv:

  default:			/* Unknown scheme */
    return "";
  }
}

/** Return default transport name corresponding to the url type */
char const *url_tport_default(enum url_type_e url_type)
{
  switch (url_type) {
  case url_sip:
    return "*";
  case url_sips:
    return "tls";
  case url_http:
    return "tcp";
  case url_https:
    return "tls";
  case url_ftp:
  case url_file:
    return "tcp";
  case url_rtsp:
    return "tcp";
  case url_rtspu:
    return "udp";
  case url_mailto:
    return "tcp";
  case url_msrp:
    return "tcp";
  case url_msrps:
    return "tls";

  case url_any:			/* "*" */
  case url_tel:
  case url_fax:
  case url_modem:
  case url_im:
  case url_pres:
  case url_cid:
  case url_wv:

  default:			/* Unknown scheme */
    return "*";
  }
}


/** Return the URL port string */
char const *url_port(url_t const *u)
{
  if (!u)
    return "";
  else if (u->url_port && u->url_port[0])
    return u->url_port;

  if (u->url_type == url_sips || u->url_type == url_sip)
    if (!host_is_ip_address(u->url_host))
      return "";

  return url_port_default((enum url_type_e)u->url_type);
}

/** Sanitize URL.
 *
 * The function url_sanitize() adds a scheme to an incomplete URL.  It
 * modifies its parameter structure @a url.  Currently, the function follows
 * simple heuristics:
 *
 * - URL with host name starting with @c ftp. is an FTP URL
 * - URL with host name starting with @c www. is an HTTP URL
 * - URL with host and path, e.g., @c host/foo;bar, is an HTTP URL
 * - URL with host name, no path is a SIP URL.
 *
 * @param url pointer to URL struct to be sanitized (IN/OUT)
 *
 * @return
 * The function url_sanitize() returns 0 if it considers URL to be
 * sane, and -1 otherwise.
 */
int url_sanitize(url_t *url)
{
  if (!url)
    return -1;
  else if (url->url_scheme != NULL)
    /* xyzzy */;
  else if (url->url_host == NULL)
    return -1;
  else if (strncasecmp(url->url_host, "ftp.", strlen("ftp.")) == 0)
    url->url_type = url_ftp, url->url_scheme = "ftp", url->url_root = '/';
  else if (strncasecmp(url->url_host, "www.", strlen("www.")) == 0
	   || url->url_path)
    url->url_type = url_http, url->url_scheme = "http", url->url_root = '/';
  else
    url->url_type = url_sip, url->url_scheme = "sip";

  return 0;
}

#include <sofia-sip/su_md5.h>

static
void canon_update(su_md5_t *md5, char const *s, size_t n, char const *allow)
{
  size_t i, j;

  for (i = 0, j = 0; i < n && s[i]; i++) {
    char c;

    if (s[i] == '%' && i + 2 < n && IS_HEX(s[i+1]) && IS_HEX(s[i+2])) {
#define   UNHEX(a) (a - (a >= 'a' ? 'a' - 10 : (a >= 'A' ? 'A' - 10 : '0')))
      c = (UNHEX(s[i+1]) << 4) | UNHEX(s[i+2]);
#undef    UNHEX
      if (c != '%' && c > ' ' && c < '\177' &&
	  (!strchr(EXCLUDED, c) || strchr(allow, c))) {
	if (i != j)
	  su_md5_iupdate(md5, s + j, i - j);
	su_md5_iupdate(md5, &c, 1);
	j = i + 3;
      }
      i += 2;
    }
  }

  if (i != j)
    su_md5_iupdate(md5, s + j, i - j);
}

/** Update MD5 sum with url-string contents */
static
void url_string_update(su_md5_t *md5, char const *s)
{
  size_t n, p;
  int have_authority = 1;
  enum url_type_e type = url_any;
  char const *at, *colon;
  char schema[48];

  if (s == NULL || strlen(s) == 0 || strcmp(s, "*") == 0) {
    su_md5_update(md5, "*\0\0*", 4);
    return;
  }

  n = strcspn(s, ":/?#");
  if (n >= sizeof schema) {
    su_md5_update(md5, ":", 1);
  }
  else if (n && s[n] == ':' ) {
    at = url_canonize(schema, s, n, 0, "+");

    type = url_get_type(schema, at - schema);
    su_md5_iupdate(md5, schema, at - schema);

    have_authority = !url_type_is_opaque(type);
    s += n + 1;
  }
  else {
    su_md5_update(md5, "", 1);
  }

  if (type == url_sip || type == url_sips) {
    /* SIP URL may have /;? in user part but no path */
    /* user-unreserved  =  "&" / "=" / "+" / "$" / "," / ";" / "?" / "/" */
    /* Some #*@#* phones include unescaped # there, too */
    n = strcspn(s, "@/;?#");
    p = strcspn(s + n, "@");
    if (s[n + p] == '@') {
      n += p;
      /* Ignore password in hash */
      colon = memchr(s, ':', n);
      p = colon ? (size_t)(colon - s) : n;
      canon_update(md5, s, p, SIP_USER_UNRESERVED);
      s += n + 1; n = 0;
    }
    else
      su_md5_iupdate(md5, "", 1);	/* user */
    n += strcspn(s + n, "/;?#");
  }
  else if (have_authority) {
    if (type == url_wv) {    /* WV URL may have / in user part */
      n = strcspn(s, "@;?#");
    }
    else if (type != url_wv && s[0] == '/' && s[1] != '/') {
      /* foo:/bar */
      su_md5_update(md5, "\0\0", 2); /* user, host */
      su_md5_striupdate(md5, url_port_default(type));
      return;
    }
    else if (s[0] == '/' && s[1] == '/') {
      /* We have authority, / / foo or foo */
      s += 2;
      n = strcspn(s, "/?#@[]");
    }
    else
      n = strcspn(s, "@;/?#");

    if (s[n] == '@') {
      /* Ignore password in hash */
      colon = type != url_unknown ? memchr(s, ':', n) : NULL;
      p = colon ? (size_t)(colon - s) : n;
      canon_update(md5, s, p, SIP_USER_UNRESERVED);
      s += n + 1;
      n = strcspn(s, "/;?#");	/* Until path, query or fragment */
    }
    else {
      su_md5_iupdate(md5, "", 1);	/* user */
      n += strcspn(s + n, "/;?#");	/* Until path, query or fragment */
    }
  }
  else /* if (!have_authority) */ {
    n = strcspn(s, ":/;?#");	/* Until pass, path, query or fragment */

    canon_update(md5, s, n, ""); /* user */
    su_md5_update(md5, "\0", 1); /* host, no port */
    su_md5_striupdate(md5, url_port_default(type));
    return;
  }

  if (n > 0 && s[0] == '[') {	/* IPv6reference */
    colon = memchr(s, ']', n);
    if (colon == NULL || ++colon == s + n || *colon != ':')
      colon = NULL;
  }
  else
    colon = memchr(s, ':', n);

  if (colon) {
    canon_update(md5, s, colon - s, ""); /* host */
    canon_update(md5, colon + 1, (s + n) - (colon + 1), "");
  }
  else {
    canon_update(md5, s, n, ""); /* host */
    su_md5_strupdate(md5, url_port_default(type));	/* port */
  }

  /* ignore parameters/path/headers.... */
}


/** Update md5 digest with contents of URL.
 *
 */
void url_update(su_md5_t *md5, url_t const *url)
{
  if (url_string_p((url_string_t *)url)) {
    url_string_update(md5, (char const *)url);
  }
  else {
    SU_MD5_STRI0UPDATE(md5, url->url_scheme);
    SU_MD5_STRI0UPDATE(md5, url->url_user);
    SU_MD5_STRI0UPDATE(md5, url->url_host);
    su_md5_striupdate(md5, URL_PORT(url));
    /* XXX - parameters/path.... */
    /* SU_MD5_STRI0UPDATE(md5, url->url_path); */
  }
}

/** Calculate a digest from URL contents. */
void url_digest(void *hash, int hsize, url_t const *url, char const *key)
{
  su_md5_t md5[1];
  uint8_t digest[SU_MD5_DIGEST_SIZE];

  su_md5_init(md5);
  if (key) su_md5_strupdate(md5, key);
  url_update(md5, url);
  su_md5_digest(md5, digest);

  if (hsize > SU_MD5_DIGEST_SIZE) {
    memset((char *)hash + SU_MD5_DIGEST_SIZE, 0, hsize - SU_MD5_DIGEST_SIZE);
    hsize = SU_MD5_DIGEST_SIZE;
  }

  memcpy(hash, digest, hsize);
}

/** Convert a URL query to a header string.
 *
 * URL query is converted by replacing each "=" in header name "=" value
 * pair with semicolon (":"), and the "&" separating header-name-value pairs
 * with line feed ("\n"). The "body" pseudoheader is moved last in the
 * string. The %-escaping is removed. Note that if the @a query contains %00,
 * the resulting string will be truncated.
 *
 * @param home memory home used to alloate string (if NULL, malloc() it)
 * @param query query part from SIP URL
 *
 * The result string is allocated from @a home, and it can be used as
 * argument to msg_header_parse_str(), msg_header_add_str() or
 * SIPTAG_HEADER_STR().
 *
 * @sa msg_header_add_str(), SIPTAG_HEADER_STR(),
 * sip_headers_as_url_query(), sip_url_query_as_taglist(),
 * @RFC3261 section 19.1.1 "Headers", #url_t, url_s#url_headers,
 * url_unescape(), url_unescape_to()
 *
 * @since New in @VERSION_1_12_4.
 */
char *url_query_as_header_string(su_home_t *home,
				 char const *query)
{
  size_t i, j, n, b_start = 0, b_len = 0;
  char *s = su_strdup(home, query);

  if (!s)
    return NULL;

  for (i = 0, j = 0; s[i];) {
    n = strcspn(s + i, "=");
    if (!s[i + n])
      break;
    if (n == 4 && strncasecmp(s + i, "body", 4) == 0) {
      if (b_start)
	break;
      b_start = i + n + 1, b_len = strcspn(s + b_start, "&");
      i = b_start + b_len + 1;
      continue;
    }
    if (i != j)
      memmove(s + j, s + i, n);
    s[j + n] = ':';
    i += n + 1, j += n + 1;
    n = strcspn(s + i, "&");
    j += url_unescape_to(s + j, s + i, n);
    i += n;
    if (s[i]) {
      s[j++] = '\n', i++;
    }
  }

  if (s[i])
    return (void)su_free(home, s), NULL;

  if (b_start) {
    s[j++] = '\n', s[j++] = '\n';
    j += url_unescape_to(s + j, query + b_start, b_len);
  }
  s[j] = '\0'; assert(j <= i);

  return s;
}

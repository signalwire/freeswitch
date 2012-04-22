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

#ifndef BNF_H  /** Defined when <sofia-sip/bnf.h> has been included. */
#define BNF_H

/**@file sofia-sip/bnf.h
 *
 * Parsing macros and prototypes for HTTP-like protocols.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Jun 06 10:59:34 2000 ppessi
 *
 */

#include <sofia-sip/su_types.h>

#include <string.h>

SOFIA_BEGIN_DECLS

/* Parsing tokens */
/** Control characters. */
#define CTL   "\001\002\003\004\005\006\007" \
    "\010\011\012\013\014\015\016\017" \
    "\020\021\022\023\024\025\026\027" \
    "\030\031\032\033\034\035\036\037" "\177" "\0"
/** Space */
#define SP      " "
/** Horizontal tab */
#define HT      "\t"
/** Carriage return */
#define CR      "\r"
/** Line feed */
#define LF      "\n"
/** Line-ending characters */
#define CRLF     CR LF
/** Whitespace */
#define WS       SP HT
/** Linear whitespace */
#define LWS      SP HT CR LF
/** Lower-case alphabetic characters */
#define LOALPHA "abcdefghijklmnopqrstuvwxyz"
/** Upper-case alphabetic characters */
#define UPALPHA "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
/** Alphabetic characters */
#define ALPHA    LOALPHA UPALPHA
/** Digits */
#define DIGIT   "0123456789"
/** RTSP safe characters */
#define SAFE    "$-_." /* RTSP stuff */
#define ALPHANUM DIGIT ALPHA
#define HEX      DIGIT "ABCDEF" "abcdef"

/** SIP token characters.
 * @note $|&^# were token chars in RFC 2543, but no more in RFC 3261.
 */
#define SIP_TOKEN  ALPHANUM "-.!%*_+`'~"
/** SIP separator characters */
#define SIP_SEPARATOR  "()<>@,;:\\\"/[]?={}" SP HT

/** SIP Word characters (that are not token characters) */
#define SIP_WORD "()<>:\\\"/[]?{}"

/** Skip whitespace (SP HT) */
#define skip_ws(ss) (*(ss) += span_ws(*(ss)))

/** Skip linear whitespace (SP HT CR LF) */
#define skip_lws(ss) (*(ss) += span_lws(*(ss)))

/** Skip [a-zA-Z] */
#define skip_alpha(ss) (*(ss) += span_alpha(*(ss)))

/** Skip digits */
#define skip_digit(ss) (*(ss) += span_digit(*(ss)))

/** Skip characters belonging to an RTSP token. */
#define skip_alpha_digit_safe(ss) (*(ss) += span_alpha_digit_safe(*(ss)))

/** Skip characters belonging to a SIP token. */
#define skip_token(ss)  (*(ss) += span_token(*(ss)))

/** Skip characters belonging to a SIP parameter value. */
#define skip_param(ss) (*(ss) += span_param(*(ss)))

/** Skip characters belonging to a SIP word. */
#define skip_word(ss) (*(ss) += span_word(*(ss)))

/** Test if @c is CR or LF */
#define IS_CRLF(c)       ((c) == '\r' || (c) == '\n')
/** Test if @c is linear whitespace */
#define IS_LWS(c)  	 ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
/*#define IS_LWS(c)  	 ((_bnf_table[(unsigned char)(c)] & bnf_lws))*/
/** Test if @c is normal whitespace */
#define IS_WS(c)   	 ((c) == ' ' || (c) == '\t')
/** Test if @c is not whitespace (and not NUL). */
#define IS_NON_WS(c)     (c && !IS_WS(c))
/*#define IS_NON_WS(c)     (c && !(_bnf_table[(unsigned char)c] & bnf_ws))*/
/** Test if @c is not linear whitespace (and not NUL). */
#define IS_NON_LWS(c)    (c && !IS_LWS(c))
/*#define IS_NON_LWS(c)    (c && !(_bnf_table[(unsigned char)c] & bnf_lws))*/
/** Test if @c is a digit. */
#define IS_DIGIT(c)   	 ((c) >= '0' && (c) <= '9')
/** Test if @c is alphabetic. */
#define IS_ALPHA(c)      (c && ((_bnf_table[(unsigned char)c] & bnf_alpha)))
/** Test if @c is alphanumeric. */
#define IS_ALPHANUM(c)   (c && (IS_DIGIT(c) || IS_ALPHA(c)))
/** Test if @c is URL-unreserved. */
#define IS_UNRESERVED(c) ((_bnf_table[(unsigned char)c] & bnf_unreserved))
/** Test if @c is URL-reserved. */
#define IS_RESERVED(c)   (c && !(_bnf_table[(unsigned char)c] & bnf_unreserved))
/** Test if @c is valid in tokens. */
#define IS_TOKEN(c)      ((_bnf_table[(unsigned char)c] & bnf_token))
/** Test if @c is valid for SIP parameter value. */
#define IS_PARAM(c)      ((_bnf_table[(unsigned char)c] & (bnf_token|bnf_param)))
/** Test if @c is a hex digit. */
#define IS_HEX(c)        (((c) >= '0' && (c) <= '9') || ((c) >= 'A' && (c) <= 'F') || ((c) >= 'a' && (c) <= 'f'))
/** Test if @c is a linear whitespace or valid in tokens. */
#define IS_TOKENLWS(c)   ((_bnf_table[(unsigned char)c] & (bnf_token|bn_lws)))

enum {
  bnf_ws = 1,					/**< Whitespace character  */
  bnf_crlf = 2,					/**< Line end character */
  bnf_lws = 3,					/**< Linear whitespace */
  bnf_alpha = 4,				/**< Alphabetic */
  bnf_safe = 8,					/**< RTSP safe */
  bnf_mark = 16,				/**< URL mark */
  bnf_unreserved = bnf_alpha | bnf_mark,	/**< URL unreserved */
  bnf_separator = 32,				/**< SIP separator */
  /** SIP token, not alphabetic (0123456789-.!%*_+`'~) */
  bnf_token0 = 64 | bnf_safe,
  bnf_token = bnf_token0 | bnf_alpha,		/**< SIP token */
  bnf_param0 = 128,				/**< SIP parameter, not token */
  bnf_param = bnf_token | bnf_param0 /**< SIP/HTTP parameter */
};

/** Table for determining class of a character. */
SOFIAPUBVAR unsigned char const _bnf_table[256];

/** Get number of characters before CRLF */
#define span_non_crlf(s) strcspn(s, CR LF)

/** Get number of characters before whitespace */
#define span_non_ws(s) strcspn(s, WS)

/** Get number of whitespace characters */
#define span_ws(s) strspn(s, WS)

/** Get number of characters before linear whitespace */
#define span_non_lws(s) strcspn(s, LWS)

/** Calculate span of a linear whitespace.
 * LWS = [*WSP CRLF] 1*WSP
 */
su_inline isize_t span_lws(char const *s)
{
  char const *e = s;
  int i = 0;
  e += strspn(s, WS);
  if (e[i] == '\r') i++;
  if (e[i] == '\n') i++;
  if (IS_WS(e[i]))
    e += i + strspn(e + i, WS);
  return e - s;
}

/** Calculate span of a token or linear whitespace characters.  */
su_inline isize_t span_token_lws(char const *s)
{
  char const *e = s;
  while (_bnf_table[(unsigned char)(*e)] & (bnf_token | bnf_lws))
    e++;
  return e - s;
}

/** Calculate span of a token characters.  */
su_inline isize_t span_token(char const *s)
{
  char const *e = s;
  while (_bnf_table[(unsigned char)(*e)] & bnf_token)
    e++;
  return e - s;
}

/** Calculate span of a alphabetic characters.  */
su_inline isize_t span_alpha(char const *s)
{
  char const *e = s;
  while (_bnf_table[(unsigned char)(*e)] & bnf_alpha)
    e++;
  return e - s;
}

/** Calculate span of a digits.  */
su_inline isize_t span_digit(char const *s)
{
  char const *e = s;
  while (*e >= '0' && *e <= '9')
    e++;
  return e - s;
}

/** Calculate span of a hex.  */
su_inline isize_t span_hexdigit(char const *s)
{
  char const *e = s;
  while (IS_HEX(*e))
    e++;
  return e - s;
}

/** Calculate span of characters belonging to an RTSP token */
su_inline isize_t span_alpha_digit_safe(char const *s)
{
  char const *e = s;
  while (_bnf_table[(unsigned char)(*e)] & (bnf_alpha | bnf_safe))
    e++;
  return e - s;
}

/** Calculate span of a characters valid in parameters.  */
su_inline isize_t span_param(char const *s)
{
  char const *e = s;
  while (IS_PARAM(*e))
    e++;
  return e - s;
}

/** Calculate span of a SIP word.  */
su_inline isize_t span_word(char const *s)
{
  char const *e = s;
  while (*e && (IS_TOKEN(*e) || strchr(SIP_WORD, *e)))
    e++;
  return e - s;
}

/** Calculate span of a unreserved characters.  */
su_inline isize_t span_unreserved(char const *s)
{
  char const *e = s;
  while (IS_UNRESERVED(*e))
    e++;
  return e - s;
}

/** Calculate span of a double quoted string (with escaped chars inside) */
su_inline isize_t span_quoted(char const *s)
{
  char const *b = s;

  if (*s++ != '"')
    return 0;

  for (;;) {
    s += strcspn(s, "\\\"");
    if (!*s)
      return 0;
    if (*s++ == '"')
      return s - b;
    if (!*s++)
      return 0;
  }
}

/* RFC 2396 defines URL chars */
/** Reserved in URLs */
#define URL_RESERVED        ";/?:=+$,"

/** Non-alphanumeric characters without syntactical meaning. */
#define URL_MARK            "-_.!~*'()"

/** Unreserved characters. */
#define URL_UNRESERVED ALPHANUM URL_MARK

/** URL hex escape. */
#define URL_ESCAPED    "%"
#define URL_DELIMS     "<>#%\""
#define URL_UNWISE     "{}|\\^[]`"
#define URL_SCHEME     ALPHANUM "+-."

/** Get number of characters belonging to url scheme */
#define span_url_scheme(s) strspn(s, URL_SCHEME)

SOFIAPUBFUN int span_ip4_address(char const *host);
SOFIAPUBFUN int span_ip6_address(char const *host);
SOFIAPUBFUN int span_ip6_reference(char const *host);
SOFIAPUBFUN int span_ip_address(char const *host);
SOFIAPUBFUN isize_t span_domain(char const *host);
SOFIAPUBFUN isize_t span_host(char const *host);

SOFIAPUBFUN int scan_ip4_address(char **inout_host);
SOFIAPUBFUN int scan_ip6_address(char **inout_host);
SOFIAPUBFUN int scan_ip6_reference(char **inout_host);
SOFIAPUBFUN int scan_ip_address(char **inout_host);
SOFIAPUBFUN issize_t scan_domain(char **inout_host);
SOFIAPUBFUN issize_t scan_host(char **inout_host);

SOFIA_END_DECLS

#endif /* !defined BNF_H */

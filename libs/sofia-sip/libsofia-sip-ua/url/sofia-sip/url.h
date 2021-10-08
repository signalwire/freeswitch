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

/**@file sofia-sip/url.h
 *
 * URL struct and helper functions.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun  8 19:28:55 2000 ppessi
 */

#ifndef URL_H_TYPES
#define URL_H_TYPES

/** Recognized URL schemes (value of url_t.url_type).
 *
 * @sa &lt;<a href="http://www.iana.org/assignments/uri-schemes.html">http://www.iana.org/assignments/uri-schemes.html</a>&gt;
 */
enum url_type_e {
  url_invalid = -2,	/**< Invalid url. */
  url_unknown = -1,	/**< Unknown scheme. */
  url_any = 0,		/**< "*" */
  url_sip,		/**< "sip:". @sa @RFC3261 */
  url_sips,		/**< "sips:". @sa @RFC3261 */
  url_tel,		/**< "tel:" @sa RFC3966 */
  url_fax,		/**< "fax:". @note Obsolete. @sa @RFC2806 */
  url_modem,		/**< "modem:". @note Obsolete. @sa @RFC2806  */
  url_http,		/**< "http:". @sa @RFC2616, @RFC3986 */
  url_https,		/**< "https:". @sa @RFC2618, @RFC3986 */
  url_ftp,		/**< "ftp:". @sa @RFC1738 */
  url_file,		/**< "file:" @sa @RFC1738 */
  url_rtsp,		/**< "rtsp:" @sa @RFC2326 */
  url_rtspu,		/**< "rtspu:" @sa @RFC2326 */
  url_mailto,		/**< "mailto:" @sa @RFC2368 */
  url_im,		/**< "im:" (simple instant messaging). @sa @RFC3860 */
  url_pres,		/**< "pres:" (simple presence). @sa @RFC3859  */
  url_cid,		/**< "cid:" (Content-ID). @sa @RFC2392 */
  url_msrp,		/**< "msrp:" (message session relay)  */
  url_msrps,		/**< "msrps:" (new in @VERSION_1_12_2) */
  url_wv,		/**< "wv:" (Wireless village) */
  _url_none
};

/** URL structure.
 *
 * This structure is used to present a parsed URL.
 */
typedef struct {
  char                url_pad[sizeof(void *) - 2];
				    /**< Zero pad for URL_STRING_P(). */
  signed char         url_type;	    /**< URL type (url_type_e). */
  char                url_root;	    /**< Nonzero if root "//" */
  char const         *url_scheme;   /**< URL type as string. */
  char const         *url_user;	    /**< User part */
  char const         *url_password; /**< Password */
  char const         *url_host;     /**< Host part */
  char const         *url_port;     /**< Port */
  char const         *url_path;     /**< Path part, starts with "/" */
  char const         *url_params;   /**< Parameters (separated by ;) */
  char const         *url_headers;  /**< Headers (separated by ? and &) */
  char const         *url_fragment; /**< Fragment (separated by #) */
} url_t;

enum {
  /** Maximum size of a URL. */
  URL_MAXLEN = 65536
};

/** Type to present either a parsed URL or string.
 *
 * The union type url_string_t is used to pass a parsed URL or string as a
 * parameter. The URL_STRING_P() checks if a passed pointer points to a
 * string or a parsed URL. Testing requires that the first character of the
 * string is nonzero. Use URL_STRING_MAKE() to properly cast a string
 * pointer as a pointer to url_string_t.
 */
typedef union {
  char  us_str[URL_MAXLEN];	/**< URL as a string. */
  url_t us_url[1];		/**< Parsed URL. */
} url_string_t;

#endif

#ifndef URL_H
/** Defined when <sofia-sip/url.h> has been included. */
#define URL_H

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

SOFIA_BEGIN_DECLS

/** Convert a string to a url struct. */
SOFIAPUBFUN url_t *url_make(su_home_t *h, char const *str);

/** Convert a string formatting result to a url struct. */
SOFIAPUBFUN url_t *url_format(su_home_t *h, char const *fmt, ...);

/** Convert #url_t to a string allocated from @a home */
SOFIAPUBFUN char *url_as_string(su_home_t *home, url_t const *url);

/** Duplicate the url to memory allocated via home */
SOFIAPUBFUN url_t *url_hdup(su_home_t *h, url_t const *src);

/** Sanitize a URL. */
SOFIAPUBFUN int url_sanitize(url_t *u);

/** Get URL scheme by type. */
SOFIAPUBFUN char const *url_scheme(enum url_type_e type);

/* ---------------------------------------------------------------------- */
/* URL comparison */

/** Compare two URLs lazily. */
SOFIAPUBFUN int url_cmp(url_t const *a, url_t const *b);

/** Compare two URLs conservatively. */
SOFIAPUBFUN int url_cmp_all(url_t const *a, url_t const *b);

/* ---------------------------------------------------------------------- */
/* Parameter handling */

/** Search for a parameter. */
SOFIAPUBFUN isize_t url_param(char const *params, char const *tag,
			      char value[], isize_t vlen);

/** Check for a parameter. */
SOFIAPUBFUN int url_has_param(url_t const *url, char const *name);

/** Check for a presence of a parameter in string. */
SOFIAPUBFUN isize_t url_have_param(char const *params, char const *tag);

/** Add a parameter. */
SOFIAPUBFUN int url_param_add(su_home_t *h, url_t *url, char const *param);

/** Strip transport-specific stuff away from URI. */
SOFIAPUBFUN int url_strip_transport(url_t *u);

/** Strip parameter away from URI. */
SOFIAPUBFUN char *url_strip_param_string(char *params, char const *name);

/** Test if url has any transport-specific stuff. */
SOFIAPUBFUN int url_have_transport(url_t const *u);

/* ---------------------------------------------------------------------- */
/* Query handling */

/** Convert a URL query to a header string. */
SOFIAPUBFUN char *url_query_as_header_string(su_home_t *home,
					     char const *query);

/* ---------------------------------------------------------------------- */
/* Handling url-escque strings */

/** Test if string contains url-reserved characters. */
SOFIAPUBFUN int url_reserved_p(char const *s);

/** Escape a string. */
SOFIAPUBFUN char *url_escape(char *d, char const *s, char const reserved[]);

/** Calculate length of string when escaped. */
SOFIAPUBFUN isize_t url_esclen(char const *s, char const reserved[]);

/** Unescape characters from string */
SOFIAPUBFUN size_t url_unescape_to(char *d, char const *s, size_t n);

/** Unescape a string */
SOFIAPUBFUN char *url_unescape(char *d, char const *s);

#define URL_RESERVED_CHARS ";/?:@&=+$,"

/* ---------------------------------------------------------------------- */
/* Initializing */

/** Initializer for an #url_t structure. @HI
 *
 * The macro URL_INIT_AS() is used to initialize a #url_t structure with a
 * known url type:
 * @code
 *   url_t urls[2] = { URL_INIT_AS(sip), URL_INIT_AS(http) };
 * @endcode
 */
#define URL_INIT_AS(type)  \
  { "\0", url_##type, 0, url_##type != url_any ? #type : "*" }

/** Init a url structure as given type */
SOFIAPUBFUN void url_init(url_t *url, enum url_type_e type);

/* ---------------------------------------------------------------------- */
/* Resolving helpers */

/** Return default port number corresponding to the url type. */
SOFIAPUBFUN char const *url_port_default(enum url_type_e url_type);

/** Return default transport name corresponding to the url type */
SOFIAPUBFUN char const *url_tport_default(enum url_type_e url_type);

/** Return the URL port string, using default port if not present. */
SOFIAPUBFUN char const *url_port(url_t const *u);

/** Return the URL port string, using default port if none present. */
#define URL_PORT(u) \
  ((u) && (u)->url_port ? (u)->url_port : \
  url_port_default((u) ? (enum url_type_e)(u)->url_type : url_any))

/* ---------------------------------------------------------------------- */
/* url_string_t handling */

/** Test if a pointer to #url_string_t is a string
 * (not a pointer to a #url_t structure). */
#define URL_STRING_P(u) ((u) && *((url_string_t*)(u))->us_str != 0)

/** Test if a pointer to #url_string_t is a string
 * (not a pointer to a #url_t structure). */
#define URL_IS_STRING(u) ((u) && *((url_string_t*)(u))->us_str != 0)

/** Test if a pointer to #url_string_t is a string
 * (not a pointer to a #url_t structure). */
SOFIAPUBFUN int url_string_p(url_string_t const * url);

/** Test if a pointer to #url_string_t is a string
 * (not a pointer to a #url_t structure). */
SOFIAPUBFUN int url_is_string(url_string_t const * url);

/** Cast a string to a #url_string_t. @HI */
#define URL_STRING_MAKE(s) \
  ((url_string_t *)((s) && *((char *)(s)) ? (s) : NULL))

/* ---------------------------------------------------------------------- */
/* Printing URL */

/** Format string used when printing url with printf(). @HI */
#define URL_PRINT_FORMAT "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
#define URL_FORMAT_STRING URL_PRINT_FORMAT

/** Argument list used when printing url with printf(). @HI */
#define URL_PRINT_ARGS(u) \
  (u)->url_scheme ? (u)->url_scheme : "",	\
  (u)->url_type != url_any && (u)->url_scheme && (u)->url_scheme[0] \
    ? ":" : "", \
  (u)->url_root && ((u)->url_host || (u)->url_user) ? "//" : "", \
  (u)->url_user ? (u)->url_user : "", \
  (u)->url_user && (u)->url_password ? ":" : "", \
  (u)->url_user && (u)->url_password ? (u)->url_password : "", \
  (u)->url_user && (u)->url_host ? "@" : "", \
  (u)->url_host ? (u)->url_host : "", \
  (u)->url_host && (u)->url_port ? ":" : "", \
  (u)->url_host && (u)->url_port ? (u)->url_port : "", \
  (u)->url_root && (u)->url_path ? "/" : "", \
  (u)->url_path ? (u)->url_path : "", \
  (u)->url_params ? ";" : "", (u)->url_params ? (u)->url_params : "", \
  (u)->url_headers ? "?" : "", (u)->url_headers ? (u)->url_headers : "", \
  (u)->url_fragment ? "#" : "", (u)->url_fragment ? (u)->url_fragment : ""

/* ---------------------------------------------------------------------- */
/* URL digests */

struct su_md5_t;

/** Update MD5 sum with URL contents. */
SOFIAPUBFUN void url_update(struct su_md5_t *md5, url_t const *url);

/** Calculate a digest from URL contents. */
SOFIAPUBFUN void url_digest(void *hash, int hsize,
			    url_t const *, char const *key);

/* ---------------------------------------------------------------------- */
/* Parsing and manipulating URLs */

/** Decode a URL. */
SOFIAPUBFUN int url_d(url_t *url, char *s);

/** Calculate the encoding length of URL. */
SOFIAPUBFUN isize_t url_len(url_t const * url);

/** Encode a URL. */
SOFIAPUBFUN issize_t url_e(char buffer[], isize_t n, url_t const *url);

/** Encode a URL: use @a buf up to @a end. @HI */
#define URL_E(buf, end, url) \
  (buf) += url_e((buf), (buf) < (end) ? (end) - (buf) : 0, (url))

/** Calculate the size of srings attached to the url. */
SOFIAPUBFUN isize_t url_xtra(url_t const * url);

/** Duplicate the url in the provided memory area. */
SOFIAPUBFUN issize_t url_dup(char *, isize_t , url_t *dst, url_t const *src);

/** Duplicate the url: use @a buf up to @a end. @HI */
#define URL_DUP(buf, end, dst, src) \
  (buf) += url_dup((buf), (isize_t)((buf) < (end) ? (end) - (buf) : 0), (dst), (src))

SOFIA_END_DECLS
#endif


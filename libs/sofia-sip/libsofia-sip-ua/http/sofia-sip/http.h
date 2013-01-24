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

#ifndef HTTP_H
/** Defined when <sofia-sip/http.h> has been included. */
#define HTTP_H

/**@file sofia-sip/http.h
 *
 * HTTP message, methods, headers.
 *
 * @sa <a href="http://www.ietf.org/rfc/rfc2616.txt">RFC 2616</a>
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created      : Thu Jun  8 19:28:55 2000 ppessi
 */

#ifndef MSG_H
#include <sofia-sip/msg.h>
#endif
#ifndef URL_H
#include <sofia-sip/url.h>
#endif
#ifndef MSG_MIME_H
#include <sofia-sip/msg_mime.h>
#endif

SOFIA_BEGIN_DECLS

/* ----------------------------------------------------------------------
 * 1) Constants
 */

#define HTTP_NONE ((http_header_t *)MSG_HEADER_NONE)
#define HTTP_DEFAULT_PORT (80)
#define HTTP_DEFAULT_SERV "80"

/** HTTP protocol identifier */
#ifndef _MSC_VER
#define HTTP_PROTOCOL_TAG   ((void *)0x48545450)	/* 'HTTP' */
#else
#define HTTP_PROTOCOL_TAG   ((void *)(UINT_PTR)0x48545450)	/* 'HTTP' */
#endif

/** HTTP parser flags */
enum {
  HTTP_FLG_NO_BODY = (1 << 15)
};

/** IDs for well-known HTTP methods. */
typedef enum {
  http_method_invalid = -1,	/**< Invalid method name */
  http_method_unknown = 0,	/**< Unknown method, use @c method_name */
  http_method_get,		/**< GET */
  http_method_post,		/**< POST */
  http_method_head,		/**< HEAD */
  http_method_options,		/**< OPTIONS */
  http_method_put,		/**< PUT */
  http_method_delete,		/**< DELETE */
  http_method_trace,		/**< TRACE */
  http_method_connect,		/**< CONNECT */
} http_method_t;

#define HTTP_METHOD(s)          http_method_unknown, #s
#define HTTP_NO_METHOD          http_method_unknown, NULL
#define HTTP_METHOD_GET	        http_method_get, "GET"
#define HTTP_METHOD_POST	http_method_post, "POST"
#define HTTP_METHOD_HEAD	http_method_head, "HEAD"
#define HTTP_METHOD_OPTIONS	http_method_options, "OPTIONS"
#define HTTP_METHOD_PUT	        http_method_put, "PUT"
#define HTTP_METHOD_DELETE	http_method_delete, "DELETE"
#define HTTP_METHOD_TRACE	http_method_trace, "TRACE"
#define HTTP_METHOD_CONNECT	http_method_connect, "CONNECT"

/* ----------------------------------------------------------------------
 * 2) Type declarations
 */

/** HTTP message object. */
typedef struct http_s               http_t;

/** Any HTTP header - union of all possible HTTP headers. */
typedef union http_header_u         http_header_t;

typedef struct http_request_s 	    http_request_t;
typedef struct http_status_s  	    http_status_t;
typedef msg_accept_t                http_accept_t;
typedef msg_accept_charset_t        http_accept_charset_t;
typedef msg_accept_encoding_t       http_accept_encoding_t;
typedef msg_accept_language_t       http_accept_language_t;
typedef msg_list_t                  http_accept_ranges_t;
typedef msg_numeric_t               http_age_t;
typedef msg_list_t                  http_allow_t;
typedef msg_auth_info_t             http_authentication_info_t;
typedef msg_auth_t                  http_authorization_t;
typedef msg_list_t                  http_cache_control_t;
typedef msg_list_t                  http_connection_t;
typedef msg_content_encoding_t      http_content_encoding_t;
typedef msg_content_language_t      http_content_language_t;
typedef msg_content_length_t        http_content_length_t;
typedef msg_content_location_t      http_content_location_t;
typedef msg_generic_t               http_content_md5_t;
typedef struct http_content_range_s http_content_range_t;
typedef msg_content_type_t          http_content_type_t;
typedef struct http_date_s          http_date_t;
typedef msg_generic_t               http_etag_t;
typedef msg_generic_t               http_expect_t;
typedef http_date_t                 http_expires_t;
typedef msg_generic_t               http_from_t;
typedef struct http_host_s          http_host_t;
typedef msg_list_t                  http_if_match_t;
typedef http_date_t                 http_if_modified_since_t;
typedef msg_list_t                  http_if_none_match_t;
typedef struct http_if_range_s      http_if_range_t;
typedef http_date_t                 http_if_unmodified_since_t;
typedef http_date_t                 http_last_modified_t;
typedef struct http_location_s      http_location_t;
typedef struct http_max_forwards_s  http_max_forwards_t;
typedef msg_generic_t               http_mime_version_t;
typedef msg_list_t                  http_pragma_t;
typedef msg_auth_t                  http_proxy_authenticate_t;
typedef msg_auth_t                  http_proxy_authorization_t;
typedef struct http_range_s         http_range_t;
typedef struct http_location_s      http_referer_t;
typedef struct http_retry_after_s   http_retry_after_t;
typedef msg_generic_t               http_server_t;
typedef struct http_te_s            http_te_t;
typedef msg_list_t                  http_trailer_t;
typedef msg_list_t                  http_transfer_encoding_t;
typedef msg_list_t                  http_upgrade_t;
typedef msg_generic_t               http_user_agent_t;
typedef msg_list_t                  http_vary_t;
typedef struct http_via_s           http_via_t;
typedef msg_warning_t               http_warning_t;
typedef msg_auth_t                  http_www_authenticate_t;

typedef msg_list_t                  http_proxy_connection_t;

typedef msg_generic_t                  http_sec_websocket_key_t;
typedef msg_generic_t                  http_origin_t;
typedef msg_generic_t                  http_sec_websocket_protocol_t;
typedef msg_generic_t                  http_sec_websocket_version_t;

typedef struct http_set_cookie_s    http_set_cookie_t;
typedef struct http_cookie_s        http_cookie_t;

/** Erroneous header. */
typedef msg_error_t                 http_error_t;
/** Unknown header. */
typedef msg_generic_t               http_unknown_t;
/** Separator line between headers and message contents */
typedef msg_separator_t             http_separator_t;
/** Entity-body */
typedef msg_payload_t               http_payload_t;
/** Time in seconds since 01-Jan-1900. */
typedef unsigned long               http_time_t;
/** Range offset. */
typedef unsigned long               http_off_t;


/* ----------------------------------------------------------------------
 * 3) Structure definitions
 */

/** HTTP request line */
struct http_request_s {
  msg_common_t      rq_common[1];
  http_error_t     *rq_next;
  http_method_t     rq_method;	    	/** Method enum. */
  char const       *rq_method_name; 	/** Method name. */
  url_t             rq_url[1];	    	/** URL. */
  char const       *rq_version;     	/** Protocol version. */
};

/** HTTP status line */
struct http_status_s {
  msg_common_t      st_common[1];
  http_error_t     *st_next;
  char const       *st_version;
  int               st_status;
  char const       *st_phrase;
};

/**@ingroup http_authentication_info
 * @brief Structure for @b Authentication-Info header.
 *
 * @deprecated Use struct msg_auth_info_s instead.
 */
struct http_authentication_info_s
{
  msg_common_t        ai_common[1]; /**< Common fragment info */
  msg_error_t        *ai_next;	    /**< Dummy link to next */
  msg_param_t const  *ai_params;    /**< List of authentication info */
};

/** Content-Range */
struct http_content_range_s {
  msg_common_t      cr_common[1];
  http_error_t     *cr_next;
  http_off_t        cr_first;	/**< First-byte-pos */
  http_off_t        cr_last;	/**< Last-byte-pos */
  http_off_t        cr_length;	/**< Instance-length */
};

/** Date, Expires, If-Modified-Since, If-Unmodified-Since, Last-Modified */
struct http_date_s {
  msg_common_t      d_common[1];
  http_error_t     *d_next;
  http_time_t       d_time;	/**< Seconds since Jan 1, 1900 */
};

/** Host */
struct http_host_s {
  msg_common_t         h_common[1];
  http_error_t        *h_next;
  char const          *h_host;
  char const          *h_port;
};

/** If-Range */
struct http_if_range_s {
  msg_common_t         ifr_common[1];
  http_error_t        *ifr_next;
  char const          *ifr_tag;	 /**< Tag */
  http_time_t          ifr_time; /**< Timestamp */
};

/** Location, Referer */
struct http_location_s {
  msg_common_t         loc_common[1];
  http_error_t        *loc_next;
  url_t                loc_url[1];
};

/** Max-Forwards */
struct http_max_forwards_s {
  msg_common_t         mf_common[1];
  http_error_t        *mf_next;
  unsigned long        mf_count;
};

/** Range */
struct http_range_s
{
  msg_common_t         rng_common[1];
  http_error_t        *rng_next;
  char const          *rng_unit;
  char const         **rng_specs;
};

/** Retry-After. */
struct http_retry_after_s {
  msg_common_t         ra_common[1];  	/**< Common fragment info */
  http_error_t        *ra_next;	      	/**< Link to next (dummy) */
  http_time_t          ra_date;       	/**< When to retry */
  http_time_t          ra_delta;        /**< Seconds to before retry */
};

/** TE */
struct http_te_s {
  msg_common_t         te_common[1];	/**< Common fragment info */
  http_te_t           *te_next;		/**< Link to next t-coding */
  char const          *te_extension;	/**< Transfer-Extension */
  msg_param_t const   *te_params;	/**< List of parameters */
  char const          *te_q;		/**< Q-value */
};

/** Via */
struct http_via_s {
  msg_common_t         v_common[1];
  http_via_t          *v_next;
  char const          *v_version;
  char const          *v_host;
  char const          *v_port;
  char const          *v_comment;
};

/** Cookie */
struct http_cookie_s {
  msg_common_t         c_common[1];
  http_cookie_t       *c_next;
  msg_param_t const   *c_params;
  char const          *c_version;
  char const          *c_name;
  char const          *c_domain;
  char const          *c_path;
};

/** Set-Cookie */
struct http_set_cookie_s {
  msg_common_t         sc_common[1];
  http_set_cookie_t   *sc_next;
  msg_param_t const   *sc_params;
  char const          *sc_name;
  char const          *sc_version;
  char const          *sc_domain;
  char const          *sc_path;
  char const          *sc_comment;
  char const          *sc_max_age;
  unsigned             sc_secure;
};

/**HTTP message object.
 *
 * This structure contains a HTTP message object.  It is used to access the
 * headers and payload within the message.  The generic transport aspects of
 * the message, like network address, is accessed using a @b msg_t object
 * directly.
 */
struct http_s {
  msg_common_t               http_common[1];    /**< For recursive inclusion */
  msg_pub_t                 *http_next;	     /**< Dummy pointer to next part */
  void                      *http_user;	               /**< Application data */
  unsigned                   http_size;	         /**< Size of this structure */
  int                        http_flags;                          /**< Flags */
  http_error_t              *http_error;              /**< Erroneous headers */

  http_request_t            *http_request;                /**< Request line  */
  http_status_t             *http_status;                   /**< Status line */

  /* === Headers start here */
  http_accept_t             *http_accept;                        /**< Accept */
  http_accept_charset_t     *http_accept_charset;        /**< Accept-Charset */
  http_accept_encoding_t    *http_accept_encoding;      /**< Accept-Encoding */
  http_accept_language_t    *http_accept_language;      /**< Accept-Language */
  http_accept_ranges_t      *http_accept_ranges;          /**< Accept-Ranges */
  http_allow_t              *http_allow;                          /**< Allow */
  http_authentication_info_t*http_authentication_info;/**<Authentication-Info*/
  http_authorization_t      *http_authorization;          /**< Authorization */
  http_age_t                *http_age;                              /**< Age */
  http_cache_control_t      *http_cache_control;          /**< Cache-Control */
  http_connection_t         *http_connection;                /**< Connection */
  http_date_t               *http_date;                            /**< Date */
  http_etag_t               *http_etag;                            /**< ETag */
  http_expect_t             *http_expect;                        /**< Expect */
  http_expires_t            *http_expires;                      /**< Expires */
  http_from_t               *http_from;                            /**< From */
  http_host_t               *http_host;                            /**< Host */
  http_if_match_t           *http_if_match;                    /**< If-Match */
  http_if_modified_since_t  *http_if_modified_since;  /**< If-Modified-Since */
  http_if_none_match_t      *http_if_none_match;          /**< If-None-Match */
  http_if_range_t           *http_if_range;                    /**< If-Range */
  http_if_unmodified_since_t*http_if_unmodified_since;/**<If-Unmodified-Since*/
  http_last_modified_t      *http_last_modified;          /**< Last-Modified */
  http_location_t           *http_location;                    /**< Location */
  http_max_forwards_t       *http_max_forwards;            /**< Max-Forwards */
  http_pragma_t             *http_pragma;                        /**< Pragma */
  http_proxy_authenticate_t *http_proxy_authenticate;/**< Proxy-Authenticate */
  http_proxy_authorization_t*http_proxy_authorization;/**<Proxy-Authorization*/
  http_range_t              *http_range;                          /**< Range */
  http_referer_t            *http_referer;                      /**< Referer */
  http_retry_after_t        *http_retry_after;              /**< Retry-After */
  http_server_t             *http_server;                        /**< Server */
  http_te_t                 *http_te;                                /**< TE */
  http_trailer_t            *http_trailer;                      /**< Trailer */
  http_transfer_encoding_t  *http_transfer_encoding;  /**< Transfer-Encoding */
  http_upgrade_t            *http_upgrade;                      /**< Upgrade */
  http_user_agent_t         *http_user_agent;                /**< User-Agent */
  http_vary_t               *http_vary;                            /**< Vary */
  http_via_t                *http_via;                              /**< Via */
  http_warning_t            *http_warning;                      /**< Warning */
  http_www_authenticate_t   *http_www_authenticate;    /**< WWW-Authenticate */

  http_proxy_connection_t   *http_proxy_connection;    /**< Proxy-Connection */
  http_set_cookie_t         *http_set_cookie;                /**< Set-Cookie */
  http_cookie_t             *http_cookie;                        /**< Cookie */

  http_sec_websocket_key_t        *http_sec_websocket_key; /**< Sec-Websocket-Key */
  http_origin_t                   *http_origin;  /**< Origin */
  http_sec_websocket_protocol_t   *http_sec_websocket_protocol; /**< Sec-Websocket-Protocol */
  http_sec_websocket_version_t    *http_sec_websocket_version; /**< Sec-Websocket-Version */

  http_mime_version_t       *http_mime_version;            /**< MIME-Version */
  http_content_encoding_t   *http_content_encoding;    /**< Content-Encoding */
  http_content_language_t   *http_content_language;    /**< Content-Language */
  http_content_length_t     *http_content_length;        /**< Content-Length */
  http_content_location_t   *http_content_location;    /**< Content-Location */
  http_content_md5_t        *http_content_md5;              /**< Content-MD5 */
  http_content_range_t      *http_content_range;          /**< Content-Range */
  http_content_type_t       *http_content_type;            /**< Content-Type */

  /* === Headers end here */
  http_header_t             *http_unknown;             /**< Unknown headers. */
  http_separator_t          *http_separator;
				  /**< Separator between message and payload */
  http_payload_t            *http_payload;	    /**< Message entity-body */
};

/**Union representing any HTTP header.
 *
 * Each different header is an array of size 1.
 *
 * @deprecated
 */
union http_header_u {
  msg_common_t                sh_common[1];
  struct {
    msg_common_t              shn_common;
    http_header_t            *shn_next;
  }                           sh_header_next[1];

  msg_auth_t                  sh_auth[1];
  msg_generic_t               sh_generic[1];
  msg_numeric_t               sh_numeric[1];

  http_request_t              sh_request[1];
  http_status_t               sh_status[1];
  http_error_t                sh_error[1];
  http_unknown_t              sh_unknown[1];
  http_separator_t            sh_separator[1];
  http_payload_t              sh_payload[1];

  /* Proper headers */
  http_via_t                 sh_via[1];
  http_host_t                sh_host[1];
  http_from_t                sh_from[1];
  http_referer_t             sh_referer[1];
  http_connection_t          sh_connection[1];

  http_accept_t              sh_accept[1];
  http_accept_charset_t      sh_accept_charset[1];
  http_accept_encoding_t     sh_accept_encoding[1];
  http_accept_language_t     sh_accept_language[1];
  http_accept_ranges_t       sh_accept_ranges[1];
  http_allow_t               sh_allow[1];
  http_te_t                  sh_te[1];

  http_authentication_info_t sh_authentication_info[1];
  http_authorization_t       sh_authorization[1];
  http_www_authenticate_t    sh_www_authenticate[1];
  http_proxy_authenticate_t  sh_proxy_authenticate[1];
  http_proxy_authorization_t sh_proxy_authorization[1];

  http_age_t                 sh_age[1];
  http_cache_control_t       sh_cache_control[1];
  http_date_t                sh_date[1];
  http_expires_t             sh_expires[1];
  http_if_match_t            sh_if_match[1];
  http_if_modified_since_t   sh_if_modified_since[1];
  http_if_none_match_t       sh_if_none_match[1];
  http_if_range_t            sh_if_range[1];
  http_if_unmodified_since_t sh_if_unmodified_since[1];

  http_etag_t                sh_etag[1];
  http_expect_t              sh_expect[1];
  http_last_modified_t       sh_last_modified[1];
  http_location_t            sh_location[1];
  http_max_forwards_t        sh_max_forwards[1];
  http_pragma_t              sh_pragma[1];
  http_range_t               sh_range[1];
  http_retry_after_t         sh_retry_after[1];
  http_trailer_t             sh_trailer[1];
  http_upgrade_t             sh_upgrade[1];
  http_vary_t                sh_vary[1];
  http_warning_t             sh_warning[1];

  http_user_agent_t          sh_user_agent[1];
  http_server_t              sh_server[1];

  http_mime_version_t        sh_mime_version[1];
  http_content_language_t    sh_content_language[1];
  http_content_location_t    sh_content_location[1];
  http_content_md5_t         sh_content_md5[1];
  http_content_range_t       sh_content_range[1];
  http_content_encoding_t    sh_content_encoding[1];
  http_transfer_encoding_t   sh_transfer_encoding[1];
  http_content_type_t        sh_content_type[1];
  http_content_length_t      sh_content_length[1];

};

SOFIA_END_DECLS

#endif /* !defined(HTTP_H) */

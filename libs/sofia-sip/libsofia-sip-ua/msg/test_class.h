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

#ifndef TEST_CLASS_H
/** Defined when <test_class.h> has been included. */
#define TEST_CLASS_H

/**@ingroup test_msg
 * @file test_class.h
 * @brief Message and header classes for testing.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jun 29 15:58:06 2000 ppessi
 */

#ifndef URL_H
#include <sofia-sip/url.h>
#endif
#ifndef MSG_H
#include <sofia-sip/msg.h>
#endif
#ifndef MSG_HEADER_H
#include <sofia-sip/msg_header.h>
#endif
#ifndef MSG_MIME_H
#include <sofia-sip/msg_mime.h>
#endif
#ifndef MSG_MCLASS_H
#include <sofia-sip/msg_mclass.h>
#endif

SOFIA_BEGIN_DECLS

#define MSG_TEST_PROTOCOL_TAG ((void *)(size_t)0xdeadbeef)

#define MSG_TEST_VERSION_CURRENT "msg/1.0"

extern msg_mclass_t const msg_test_mclass[1];

extern msg_href_t const msg_content_length_href[1];

typedef struct msg_request_s msg_request_t;
typedef struct msg_status_s  msg_status_t;

/** Request line. */
struct msg_request_s {
  msg_common_t     rq_common[1];   /**< Common fragment info */
  msg_header_t    *rq_next;	   /**< Link to next header */
  char const      *rq_method_name; /**< Method name */
  url_t            rq_url[1];	   /**< RequestURI */
  char const      *rq_version;     /**< Protocol version */
};

/** Status line. */
struct msg_status_s {
  msg_common_t   st_common[1];	/**< Common fragment info */
  msg_header_t *st_next;	/**< Link to next (dummy) */
  char const    *st_version;	/**< Protocol version */
  int            st_status;	/**< Status code */
  char const    *st_phrase;	/**< Status phrase */
};

/** Message object for tests. */
typedef struct msg_test_s {
  msg_common_t        msg_common[1]; /**< For recursive inclusion */
  msg_pub_t          *msg_next;
  void               *msg_user;	     /**< User data */
  unsigned            msg_size;
  unsigned            msg_flags;
  msg_error_t        *msg_error;

  msg_request_t      *msg_request;
  msg_status_t       *msg_status;

  /* === Headers start here */
  msg_content_type_t     *msg_content_type;     /**< Content-Type */
  msg_content_disposition_t *msg_content_disposition;
                                                /**< Content-Disposition */
  msg_content_location_t *msg_content_location; /**< Content-Location */
  msg_content_language_t *msg_content_language; /**< Content-Language */

  msg_accept_t           *msg_accept;           /**< Accept */
  msg_accept_charset_t   *msg_accept_charset;	/**< Accept-Charset */
  msg_accept_encoding_t  *msg_accept_encoding;	/**< Accept-Encoding */
  msg_accept_language_t  *msg_accept_language;	/**< Accept-Language */
  msg_mime_version_t     *msg_mime_version;	/**< MIME-Version */
  msg_content_md5_t      *msg_content_md5;	/**< Content-MD5 */
  msg_content_encoding_t *msg_content_encoding;
						/**< Content-Encoding */
  msg_content_length_t   *msg_content_length;	/**< Content-Length */

  msg_auth_t             *msg_auth; 		/**< Auth (testing) */
  msg_numeric_t          *msg_numeric;		/**< Numeric (testing) */
  /* === Headers end here */

  msg_unknown_t      *msg_unknown;
  msg_separator_t    *msg_separator;
  msg_payload_t      *msg_payload;
  msg_multipart_t    *msg_multipart;
} msg_test_t;

union msg_test_u
{
  msg_common_t    sh_common[1];
  struct {
    msg_common_t  shn_common;
    msg_header_t *shn_next;
  }               sh_header_next[1];

  msg_request_t             sh_request[1];
  msg_status_t              sh_status[1];
  msg_accept_t              sh_accept[1];
  msg_accept_charset_t      sh_accept_charset[1];
  msg_accept_encoding_t     sh_accept_encoding[1];
  msg_accept_language_t     sh_accept_language[1];
  msg_content_disposition_t sh_content_disposition[1];
  msg_content_encoding_t    sh_content_encoding[1];
  msg_content_id_t          sh_content_id[1];
  msg_content_md5_t         sh_content_md5[1];
  msg_content_language_t    sh_content_language[1];
  msg_content_length_t      sh_content_length[1];
  msg_content_location_t    sh_content_location[1];
  msg_content_type_t        sh_content_type[1];
  msg_mime_version_t        sh_mime_version[1];

  msg_generic_t   sh_generic[1];
  msg_numeric_t   sh_numeric[1];
  msg_list_t      sh_list[1];
  msg_auth_t      sh_auth[1];
  msg_separator_t sh_separator[1];
  msg_payload_t   sh_payload[1];
  msg_unknown_t   sh_unknown[1];
};

issize_t msg_test_extract_body(msg_t *, msg_pub_t *,
			       char b[], isize_t bsiz, int eos);

su_inline
msg_test_t *msg_test_public(msg_t *msg)
{
  return (msg_test_t *)msg_public(msg, MSG_TEST_PROTOCOL_TAG);
}

#define msg_auth_class test_auth_class

#define msg_numeric_class test_numeric_class

enum {
  msg_auth_hash = 22894,
  msg_numeric_hash = 24435
};

extern msg_hclass_t test_auth_class[1], test_numeric_class[1];

SOFIA_END_DECLS

#endif /* !defined(TEST_CLASS_H) */

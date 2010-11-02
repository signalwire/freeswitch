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

#ifndef MSG_MIME_H
/** Defined when <sofia-sip/msg_mime.h> has been included. */
#define MSG_MIME_H

/**@ingroup msg_mime
 * @file sofia-sip/msg_mime.h
 *
 * MIME headers and multipart messages (@RFC2045).
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri Aug 16 19:18:26 EEST 2002 ppessi
 *
 */

#ifndef URL_H
#include <sofia-sip/url.h>
#endif

#ifndef MSG_TYPES_H
#include <sofia-sip/msg_types.h>
#endif
#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

typedef struct msg_accept_any_s     msg_accept_any_t;

typedef struct msg_accept_s  	    msg_accept_t;

typedef msg_accept_any_t            msg_accept_charset_t;
typedef msg_accept_any_t      	    msg_accept_encoding_t;
typedef msg_accept_any_t     	    msg_accept_language_t;

typedef struct msg_content_disposition_s
                                    msg_content_disposition_t;
typedef msg_list_t	      	    msg_content_encoding_t;
typedef msg_generic_t               msg_content_id_t;
typedef struct msg_content_length_s msg_content_length_t;
typedef msg_generic_t               msg_content_location_t;
typedef msg_list_t                  msg_content_language_t;
typedef msg_generic_t               msg_content_md5_t;
typedef msg_generic_t	     	    msg_content_transfer_encoding_t;
typedef struct msg_content_type_s   msg_content_type_t;
typedef msg_generic_t	     	    msg_mime_version_t;
typedef struct msg_warning_s	    msg_warning_t;

/** Multipart body object. */
typedef struct msg_multipart_s      msg_multipart_t;

/**@ingroup msg_accept
 * @brief Structure for @b Accept header.
 */
struct msg_accept_s
{
  msg_common_t        ac_common[1]; /**< Common fragment info */
  msg_accept_t       *ac_next;	    /**< Pointer to next Accept header */
  char const         *ac_type;	    /**< Pointer to type/subtype */
  char const         *ac_subtype;   /**< Points after first slash in type */
  msg_param_t const  *ac_params;    /**< List of parameters */
  char const         *ac_q;	    /**< Value of q parameter */
};

/**@ingroup msg_accept_encoding
 * @brief Structure for @b Accept-Charset, @b Accept-Encoding and
 * @b Accept-Language headers.
 */
struct msg_accept_any_s
{
  msg_common_t        aa_common[1]; /**< Common fragment info */
  msg_accept_any_t   *aa_next;	    /**< Pointer to next Accept-* header */
  char const         *aa_value;	    /**< Token */
  msg_param_t const  *aa_params;    /**< List of parameters */
  char const         *aa_q;	    /**< Value of q parameter */
};

/**@ingroup msg_content_disposition
 * @brief Structure for @b Content-Disposition header.
 */
struct msg_content_disposition_s
{
  msg_common_t       cd_common[1];  /**< Common fragment info */
  msg_error_t       *cd_next;	    /**< Link to next (dummy) */
  char const        *cd_type;	    /**< Disposition type */
  msg_param_t const *cd_params;	    /**< List of parameters */
  char const        *cd_handling;   /**< Value of @b handling parameter */
  unsigned           cd_required:1; /**< True if handling=required */
  unsigned           cd_optional:1; /**< True if handling=optional */
  unsigned           :0;	    /* pad */
};

/**@ingroup msg_content_length
 * @brief Structure for Content-Length header.
 */
struct msg_content_length_s
{
  msg_common_t   l_common[1];	    /**< Common fragment info */
  msg_error_t   *l_next;	    /**< Link to next (dummy) */
  unsigned long  l_length;	    /**< Digits */
};


/**@ingroup msg_content_type
 * @brief Structure for Content-Type header.
 */
struct msg_content_type_s
{
  msg_common_t        c_common[1];  /**< Common fragment info */
  msg_error_t        *c_next;	    /**< Dummy link to next */
  char const         *c_type;	    /**< Pointer to type/subtype */
  char const         *c_subtype;    /**< Points after first slash in type */
  msg_param_t const  *c_params;	    /**< List of parameters */
};


/**@ingroup sip_warning
 * @brief Structure for @b Warning header.
 */
struct msg_warning_s
{
  msg_common_t        w_common[1];  /**< Common fragment info */
  msg_warning_t      *w_next;	    /**< Link to next Warning header */
  unsigned            w_code;       /**< Warning code */
  char const         *w_host;	    /**< Hostname or pseudonym */
  char const         *w_port;	    /**< Port number */
  char const         *w_text;       /**< Warning text */
};


/**@ingroup msg_multipart
 *
 * @brief Structure for a part in MIME multipart message.
 */
struct msg_multipart_s
{
  msg_common_t            mp_common[1];	/**< Common fragment information */
  msg_multipart_t        *mp_next;      /**< Next part in multipart body */
  /* Preamble for this part */
  char                   *mp_data;	/**< Boundary string. */
  unsigned                mp_len;	/**< Length of boundary (mp_data).*/
  unsigned                mp_flags;
  msg_error_t            *mp_error;

  /* === Headers start here */
  msg_content_type_t     *mp_content_type;	/**< Content-Type (c) */
  msg_content_disposition_t *mp_content_disposition;
                                                /**< Content-Disposition */
  msg_content_location_t *mp_content_location;	/**< Content-Location */
  msg_content_id_t       *mp_content_id;        /**< Content-ID */
  msg_content_language_t *mp_content_language;	/**< Content-Language */
  msg_content_encoding_t *mp_content_encoding;	/**< Content-Encoding (e) */
  msg_content_transfer_encoding_t *mp_content_transfer_encoding;
                                        /**< Content-Transfer-Encoding */
#if 0
  /* === Hash headers end here */
  /* These MIME headers are here for msg_parser.awk */
  msg_accept_t           *mp_accept;		/**< Accept */
  msg_accept_charset_t   *mp_accept_charset;	/**< Accept-Charset */
  msg_accept_encoding_t  *mp_accept_encoding;	/**< Accept-Encoding */
  msg_accept_language_t  *mp_accept_language;	/**< Accept-Language */
  msg_mime_version_t     *mp_mime_version;	/**< MIME-Version */
  msg_content_md5_t      *mp_content_md5;	/**< Content-MD5 */
  msg_content_length_t   *mp_content_length;	/**< Content-Length */
  msg_multipart_t        *mp_multipart;		/**< Recursive multipart */
  msg_warning_t          *mp_warning;           /**< Warning */
#endif
  /* === Headers end here */

  /** Unknown and extra headers. */
  msg_unknown_t          *mp_unknown;           /**< Unknown headers */

  msg_separator_t        *mp_separator;	        /**< Separator */
  msg_payload_t          *mp_payload;	        /**< Body part */

  msg_multipart_t        *mp_multipart;		/**< Recursive multipart */

  msg_payload_t          *mp_close_delim;       /**< Closing delimiter */
};

SOFIAPUBFUN
msg_multipart_t *msg_multipart_create(su_home_t *home,
				      char const *content_type,
				      void const *data,
				      isize_t dlen);
SOFIAPUBFUN
msg_multipart_t *msg_multipart_parse(su_home_t *home,
				     msg_content_type_t const *c,
				     msg_payload_t *pl);
SOFIAPUBFUN
int msg_multipart_complete(su_home_t *home,
			   msg_content_type_t *c,
			   msg_multipart_t *mp);
SOFIAPUBFUN msg_header_t *msg_multipart_serialize(msg_header_t **head0,
						  msg_multipart_t *mp);

SOFIAPUBFUN issize_t msg_multipart_prepare(msg_t *msg, msg_multipart_t *mp, int flags);

SOFIAPUBFUN isize_t msg_accept_any_dup_xtra(msg_header_t const *h, isize_t offset);

SOFIAPUBFUN char *msg_accept_any_dup_one(msg_header_t *dst,
					 msg_header_t const *src,
					 char *b, isize_t xtra);

SOFIAPUBFUN
msg_content_length_t *msg_content_length_create(su_home_t *home, uint32_t n);

/** MIME multipart protocol name. @HIDE */
#define MSG_MULTIPART_VERSION_CURRENT msg_mime_version_1_0
SOFIAPUBVAR char const msg_mime_version_1_0[];

/** MIME multipart parser table identifier. @HIDE */
#ifndef _MSC_VER
#define MSG_MULTIPART_PROTOCOL_TAG   ((void *)0x4d494d45)	/* 'MIME' */
#else
#define MSG_MULTIPART_PROTOCOL_TAG   ((void *)(UINT_PTR)0x4d494d45)	/* 'MIME' */
#endif

SOFIA_END_DECLS

#endif /** MSG_MIME_H */

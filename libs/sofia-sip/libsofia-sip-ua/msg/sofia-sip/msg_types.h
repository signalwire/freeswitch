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

#ifndef MSG_TYPES_H
/** Defined when <sofia-sip/msg_types.h> has been included. */
#define MSG_TYPES_H

/**@file sofia-sip/msg_types.h
 * @brief Types for messages and common headers
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Jan 23 15:43:17 2003 ppessi
 *
 */

#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

/** Message class. */
typedef struct msg_mclass_s       msg_mclass_t;

/** Header class. */
typedef struct msg_hclass_s const msg_hclass_t;

/** Header reference. */
typedef struct msg_href_s         msg_href_t;

/** Message object. */
typedef struct msg_s              msg_t;

#ifndef MSG_TIME_T_DEFINED
#define MSG_TIME_T_DEFINED
/** Time in seconds since epoch (1900-Jan-01 00:00:00). */
typedef unsigned long msg_time_t;
#endif

#ifndef MSG_TIME_MAX
/** Latest time that can be expressed with msg_time_t. @HIDE */
#define MSG_TIME_MAX ((msg_time_t)ULONG_MAX)
#endif

#ifndef MSG_PUB_T
#ifdef MSG_OBJ_T
#define MSG_PUB_T MSG_OBJ_T
#else
#define MSG_PUB_T struct msg_pub_s
#endif
#endif

/**Public protocol-specific message structure for accessing the message.
 *
 * This type can be either #sip_t, #http_t, or #msg_multipart_t, depending
 * on the message. The base structure used by msg module is defined in
 * struct #msg_pub_s.
 */
typedef MSG_PUB_T msg_pub_t;

#ifndef MSG_HDR_T
#define MSG_HDR_T union msg_header_u
#endif
/** Any protocol-specific header object */
typedef MSG_HDR_T msg_header_t;

typedef struct msg_common_s         msg_common_t;

typedef struct msg_separator_s      msg_separator_t;
typedef struct msg_payload_s        msg_payload_t;
typedef struct msg_unknown_s        msg_unknown_t;
typedef struct msg_error_s          msg_error_t;

typedef msg_common_t msg_frg_t;

typedef char const            	   *msg_param_t;
typedef struct msg_numeric_s  	    msg_numeric_t;
typedef struct msg_generic_s  	    msg_generic_t;
typedef struct msg_list_s     	    msg_list_t;
typedef struct msg_auth_s     	    msg_auth_t;
typedef struct msg_auth_info_s      msg_auth_info_t;

#define MSG_HEADER_N 16377

/** Common part of the header objects (or message fragments).
 *
 * This structure is also known as #msg_common_t or #sip_common_t.
 */
struct msg_common_s {
  msg_header_t       *h_succ;	/**< Pointer to succeeding fragment. */
  msg_header_t      **h_prev;	/**< Pointer to preceeding fragment. */
  msg_hclass_t       *h_class;	/**< Header class. */
  void const         *h_data;	/**< Fragment data */
  usize_t             h_len;    /**< Fragment length (including CRLF) */
};


/** Message object, common view */
struct msg_pub_s {
  msg_common_t        msg_common[1]; /**< Recursive */
  msg_pub_t          *msg_next;
  void               *msg_user;
  unsigned            msg_size;
  unsigned            msg_flags;
  msg_error_t        *msg_error;
  msg_header_t       *msg_request;
  msg_header_t       *msg_status;
  msg_header_t       *msg_headers[MSG_HEADER_N];
};

#define msg_ident msg_common->h_class

/** Numeric header.
 *
 * A numeric header has value range of a 32-bit, 0..4294967295. The @a
 * x_value field is unsigned long, however.
 */
struct msg_numeric_s {
  msg_common_t   x_common[1];	    /**< Common fragment info */
  msg_numeric_t *x_next;	    /**< Link to next header */
  unsigned long  x_value;	    /**< Numeric header value */
};

/** Generic header.
 *
 * A generic header does not have any internal structure. Its value is
 * represented as a string.
 */
struct msg_generic_s {
  msg_common_t   g_common[1];	    /**< Common fragment info */
  msg_generic_t *g_next;	    /**< Link to next header */
  char const    *g_string;	    /**< Header value */
};

/** List header.
 *
 * A list header consists of comma-separated list of tokens.
 */
struct msg_list_s {
  msg_common_t       k_common[1];   /**< Common fragment info */
  msg_list_t        *k_next;	    /**< Link to next header */
  msg_param_t       *k_items;	    /**< List of items */
};

/** Authentication header.
 *
 * An authentication header has authentication scheme name and
 * comma-separated list of parameters as its value.
 */
struct msg_auth_s {
  msg_common_t       au_common[1];  /**< Common fragment info */
  msg_auth_t        *au_next;	    /**< Link to next header */
  char const        *au_scheme;	    /**< Auth-scheme like Basic or Digest */
  msg_param_t const *au_params;	    /**< Comma-separated parameters */
};

/**Authentication-Info header
 *
 * An Authentication-Info header has comma-separated list of parameters as its value.
 */
struct msg_auth_info_s
{
  msg_common_t        ai_common[1]; /**< Common fragment info */
  msg_error_t        *ai_next;	    /**< Dummy link to next */
  msg_param_t const  *ai_params;    /**< List of ainfo */
};

/** Unknown header. */
struct msg_unknown_s {
  msg_common_t    un_common[1];  /**< Common fragment info */
  msg_unknown_t  *un_next;	 /**< Link to next unknown header */
  char const     *un_name;	 /**< Header name */
  char const     *un_value;	 /**< Header field value */
};

/** Erroneus header. */
struct msg_error_s {
  msg_common_t    er_common[1];  /**< Common fragment info */
  msg_error_t    *er_next;	 /**< Link to next header */
  char const     *er_name;       /**< Name of bad header (if any). */
};


/** Separator. */
struct msg_separator_s {
  msg_common_t    sep_common[1]; /**< Common fragment info */
  msg_error_t    *sep_next;	 /**< Dummy link to next header */
  char            sep_data[4];	 /**< NUL-terminated separator */
};

/** Message payload. */
struct msg_payload_s {
  msg_common_t    pl_common[1];	    /**< Common fragment info */
  msg_payload_t  *pl_next;	    /**< Next payload chunk */
  char           *pl_data;	    /**< Data - may contain NUL */
  usize_t         pl_len;	    /**< Length of message payload */
};

/** Any header. */
union msg_header_u {
  msg_common_t    sh_common[1];	    /**< Common fragment info */
  struct {
    msg_common_t  shn_common;
    msg_header_t *shn_next;
  }               sh_header_next[1];
#define sh_next   sh_header_next->shn_next
#define sh_class  sh_common->h_class
#define sh_succ   sh_common->h_succ
#define sh_prev   sh_common->h_prev
#define sh_data   sh_common->h_data
#define sh_len    sh_common->h_len

  msg_generic_t   sh_generic[1];
  msg_numeric_t   sh_numeric[1];
  msg_list_t      sh_list[1];
  msg_auth_t      sh_auth[1];
  msg_separator_t sh_separator[1];
  msg_payload_t   sh_payload[1];
  msg_unknown_t   sh_unknown[1];
  msg_error_t     sh_error[1];
};

/* ====================================================================== */

/**Define how to handle existing headers
 * when a new header is added to a message.
 */
typedef enum {
  msg_kind_single,		/**< Only one header is allowed */
  msg_kind_append,		/**< New header is appended */
  msg_kind_list,		/**< A token list header,
				 * new header is combined with old one. */
  msg_kind_apndlist,		/**< A complex list header. */
  msg_kind_prepend		/**< New header is prepended */
} msg_header_kind_t;

struct su_home_s;

typedef issize_t msg_parse_f(struct su_home_s *, msg_header_t *, char *, isize_t);
typedef issize_t msg_print_f(char buf[], isize_t bufsiz,
			     msg_header_t const *, int flags);
typedef char *msg_dup_f(msg_header_t *dst, msg_header_t const *src,
			char *buf, isize_t bufsiz);
typedef isize_t msg_xtra_f(msg_header_t const *h, isize_t offset);

typedef int msg_update_f(msg_common_t *, char const *name, isize_t namelen,
			 char const *value);

/** Factory object for a header.
 *
 * The #msg_hclass_t object, "header class", defines how a header is
 * handled. It has parsing and printing functions, functions used to copy
 * header objects, header name and other information used when parsing,
 * printing, removing, adding and replacing headers within a message.
 */
struct msg_hclass_s
{
  /* XXX size of header class missing. Someone has saved bits in wrong place. */
  int               hc_hash;	/**< Header name hash or ID */
  msg_parse_f      *hc_parse;	/**< Parse header. */
  msg_print_f      *hc_print;	/**< Print header. */
  msg_xtra_f       *hc_dxtra;	/**< Calculate extra size for dup */
  msg_dup_f        *hc_dup_one;	/**< Duplicate one header. */
  msg_update_f     *hc_update;	/**< Update parameter(s) */
  char const 	   *hc_name;	/**< Full name. */
  short             hc_len;	/**< Length of hc_name. */
  char              hc_short[2];/**< Short name, if any. */
  unsigned char     hc_size;	/**< Size of header structure. */
  unsigned char     hc_params;	/**< Offset of parameter list */
  unsigned          hc_kind:3;	/**< Kind of header (#msg_header_kind_t):
				 * single, append, list, apndlist, prepend. */
  unsigned          hc_critical:1; /**< True if header is critical */
  unsigned          /*pad*/:0;
};

#define HC_LEN_MAX SHRT_MAX

SOFIA_END_DECLS

#endif /* !defined MSG_TYPES_H */

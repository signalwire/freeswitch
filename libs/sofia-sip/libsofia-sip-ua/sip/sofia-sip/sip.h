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

#ifndef SIP_H
/** Defined when <sofia-sip/sip.h> has been included. */
#define SIP_H

/**@file sofia-sip/sip.h
 *
 * SIP objects.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created      : Thu Jun  8 19:28:55 2000 ppessi
 */

#ifndef MSG_TYPES_H
#include <sofia-sip/msg_types.h>
#endif
#ifndef MSG_MIME_H
#include <sofia-sip/msg_mime.h>
#endif

SOFIA_BEGIN_DECLS

/** IDs for well-known SIP methods. */
typedef enum {
  sip_method_invalid = -1,	/**< Invalid method name */
  sip_method_unknown = 0,	/**< Unknown method, use @c method_name */
  sip_method_invite,		/**< INVITE */
  sip_method_ack,		/**< ACK */
  sip_method_cancel,		/**< CANCEL */
  sip_method_bye,		/**< BYE */
  sip_method_options,		/**< OPTIONS */
  sip_method_register,		/**< REGISTER */
  sip_method_info,		/**< INFO */
  sip_method_prack,		/**< PRACK */
  sip_method_update,		/**< UPDATE */
  sip_method_message,		/**< MESSAGE */
  sip_method_subscribe,		/**< SUBSCRIBE */
  sip_method_notify,		/**< NOTIFY */
  sip_method_refer,		/**< REFER */
  sip_method_publish		/**< PUBLISH */
} sip_method_t;

#define SIP_METHOD(s)         sip_method_unknown, #s
#define SIP_METHOD_ACK        sip_method_ack, "ACK"
#define SIP_METHOD_CANCEL     sip_method_cancel, "CANCEL"
#define SIP_METHOD_BYE        sip_method_bye, "BYE"
#define SIP_METHOD_INVITE     sip_method_invite, "INVITE"
#define SIP_METHOD_OPTIONS    sip_method_options, "OPTIONS"
#define SIP_METHOD_REGISTER   sip_method_register, "REGISTER"
#define SIP_METHOD_INFO       sip_method_info, "INFO"
#define SIP_METHOD_PRACK      sip_method_prack, "PRACK"
#define SIP_METHOD_UPDATE     sip_method_update, "UPDATE"
#define SIP_METHOD_MESSAGE    sip_method_message, "MESSAGE"
#define SIP_METHOD_SUBSCRIBE  sip_method_subscribe, "SUBSCRIBE"
#define SIP_METHOD_NOTIFY     sip_method_notify, "NOTIFY"
#define SIP_METHOD_REFER      sip_method_refer, "REFER"
#define SIP_METHOD_PUBLISH    sip_method_publish, "PUBLISH"

/** Magic pointer value - never valid for SIP headers. @HI */
#define SIP_NONE ((void const *)-1L)

/** SIP protocol identifier @HIDE */
#define SIP_PROTOCOL_TAG   ((void *)0x53495020)	/* 'SIP'20 */

enum {
  /** Default port for SIP as integer */
 SIP_DEFAULT_PORT = 5060,
#define SIP_DEFAULT_PORT SIP_DEFAULT_PORT

/** Default port for SIP as string */
#define SIP_DEFAULT_SERV "5060"

 /** Default port for SIPS as integer */
 SIPS_DEFAULT_PORT = 5061
#define SIPS_DEFAULT_PORT SIPS_DEFAULT_PORT
 /** Default port for SIPS as string */
#define SIPS_DEFAULT_SERV "5061"
};

/** Time in seconds since Jan 01 1900.  */
typedef msg_time_t sip_time_t;

/** Latest time that can be expressed with #sip_time_t. @HIDE */
#define SIP_TIME_MAX ((sip_time_t)MSG_TIME_MAX)

/** Structure for accessing parsed SIP headers. */
typedef struct sip_s                sip_t;

/** Any SIP header - union of all possible SIP headers. */
typedef union sip_header_u          sip_header_t;

/** Type of a generic SIP header. */
typedef struct msg_generic_s        sip_generic_t;
#define g_value g_string

/** Common part of all the header structures. */
typedef msg_common_t                sip_common_t;

/** SIP parameter string. */
typedef msg_param_t                 sip_param_t;

/** @To or @From header. */
typedef struct sip_addr_s           sip_addr_t;

/** @Authorization, @ProxyAuthenticate, @WWWAuthenticate */
typedef msg_auth_t                  sip_auth_t;

typedef struct sip_request_s 	    sip_request_t;
typedef struct sip_status_s  	    sip_status_t;
typedef msg_error_t                 sip_error_t;
typedef msg_unknown_t               sip_unknown_t;
typedef msg_separator_t             sip_separator_t;
typedef msg_payload_t               sip_payload_t;

typedef struct sip_accept_s  	    sip_accept_t;
typedef msg_accept_any_t      	    sip_accept_encoding_t;
typedef msg_accept_any_t      	    sip_accept_language_t;
typedef struct sip_allow_s	    sip_allow_t;
typedef struct msg_auth_info_s      sip_authentication_info_t;
typedef struct msg_auth_s 	    sip_authorization_t;
typedef struct sip_call_id_s 	    sip_call_id_t;
typedef struct sip_call_info_s      sip_call_info_t;
typedef struct sip_contact_s 	    sip_contact_t;
typedef struct sip_cseq_s    	    sip_cseq_t;
typedef struct msg_generic_s        sip_etag_t;
typedef struct msg_generic_s        sip_if_match_t;
typedef msg_content_disposition_t   sip_content_disposition_t;
typedef msg_list_t	      	    sip_content_encoding_t;
typedef msg_list_t	      	    sip_content_language_t;
typedef struct sip_content_length_s sip_content_length_t;
typedef struct msg_content_type_s   sip_content_type_t;
typedef struct msg_generic_s        sip_mime_version_t;
typedef struct sip_date_s	    sip_date_t;
typedef struct sip_error_info_s     sip_error_info_t;
typedef struct sip_expires_s        sip_expires_t;
typedef struct sip_addr_s  	    sip_from_t;
typedef msg_list_t                  sip_in_reply_to_t;
typedef struct sip_max_forwards_s   sip_max_forwards_t;
typedef struct sip_min_expires_s    sip_min_expires_t;
typedef struct sip_min_se_s         sip_min_se_t;
typedef struct msg_generic_s        sip_organization_t;
typedef struct msg_generic_s        sip_priority_t;
typedef struct msg_auth_s 	    sip_proxy_authenticate_t;
typedef struct msg_auth_info_s      sip_proxy_authentication_info_t;
typedef struct msg_auth_s     	    sip_proxy_authorization_t;
typedef msg_list_t	     	    sip_proxy_require_t;
typedef struct sip_rack_s           sip_rack_t;
typedef struct sip_reason_s         sip_reason_t;
typedef struct sip_route_s     	    sip_record_route_t;

typedef struct sip_refer_to_s       sip_refer_to_t;
typedef struct sip_referred_by_s    sip_referred_by_t;
typedef struct sip_replaces_s       sip_replaces_t;

typedef struct sip_request_disposition_s sip_request_disposition_t;

typedef struct sip_caller_prefs_s   sip_caller_prefs_t;
typedef struct sip_caller_prefs_s   sip_accept_contact_t;
typedef struct sip_caller_prefs_s   sip_reject_contact_t;

typedef msg_list_t	     	    sip_require_t;
typedef struct sip_retry_after_s    sip_retry_after_t;
typedef struct sip_route_s     	    sip_route_t;
typedef struct sip_rseq_s           sip_rseq_t;
typedef struct msg_generic_s        sip_server_t;
typedef struct sip_session_expires_s
                                    sip_session_expires_t;
typedef struct msg_generic_s        sip_subject_t;
typedef struct sip_subscription_state_s
                                    sip_subscription_state_t;
typedef msg_list_t	     	    sip_supported_t;
typedef struct sip_timestamp_s 	    sip_timestamp_t;
typedef struct sip_addr_s           sip_to_t;
typedef msg_list_t	     	    sip_unsupported_t;
typedef struct msg_generic_s        sip_user_agent_t;
typedef struct sip_via_s 	    sip_via_t;
typedef msg_warning_t	            sip_warning_t;
typedef struct msg_auth_s 	    sip_www_authenticate_t;

typedef struct sip_event_s          sip_event_t;
typedef msg_list_t                  sip_allow_events_t;

/* RFC 3323 - @Privacy */
typedef struct sip_privacy_s sip_privacy_t;

/* RFC 3327 - @Path */
typedef struct sip_route_s     	    sip_path_t;

/* RFC 3329 - Security Mechanism Agreement */
typedef struct sip_security_agree_s sip_security_client_t;
typedef struct sip_security_agree_s sip_security_server_t;
typedef struct sip_security_agree_s sip_security_verify_t;

/* RFC 3608 - Service Route */
typedef struct sip_route_s     	    sip_service_route_t;


/**SIP message object.
 *
 * This structure contains a parsed SIP message. The struct is usually
 * referred with typedef #sip_t. It is used to access the headers and
 * payload within the SIP message. The generic transport aspects of the
 * message, like network address, is accessed using the #msg_t object
 * directly.
 */
struct sip_s {
  msg_common_t               sip_common[1];     /**< For recursive inclusion */
  msg_pub_t                 *sip_next;          /**< Dummy link to msgfrag */
  void                      *sip_user;	        /**< Application data */
  unsigned                   sip_size;          /**< Size of structure */
  int                        sip_flags;	        /**< Parser flags */

  sip_error_t               *sip_error;	        /**< Erroneous headers */

  /* Pseudoheaders */
  sip_request_t             *sip_request;       /**< Request line  */
  sip_status_t              *sip_status;        /**< Status line */

  /* === Headers start here */
  sip_via_t        	    *sip_via;		/**< Via (v) */
  sip_route_t               *sip_route;		/**< Route */
  sip_record_route_t        *sip_record_route;	/**< Record-Route */
  sip_max_forwards_t        *sip_max_forwards;	/**< Max-Forwards */
  sip_proxy_require_t       *sip_proxy_require;	/**< Proxy-Require */

  sip_from_t       	    *sip_from;		/**< From (f) */
  sip_to_t         	    *sip_to;		/**< To (t) */
  sip_call_id_t             *sip_call_id;	/**< Call-ID (i) */
  sip_cseq_t       	    *sip_cseq;		/**< CSeq */
  sip_contact_t             *sip_contact;	/**< Contact (m) */
  sip_rseq_t                *sip_rseq;          /**< RSeq */
  sip_rack_t                *sip_rack;          /**< RAck */

  /* Caller Preferences */
  sip_request_disposition_t *sip_request_disposition;
                                                /**< Request-Disposition (d) */
  sip_accept_contact_t      *sip_accept_contact;/**< Accept-Contact (a) */
  sip_reject_contact_t      *sip_reject_contact;/**< Reject-Contact (j) */

  sip_expires_t             *sip_expires;	/**< Expires */
  sip_date_t                *sip_date;		/**< Date */
  sip_retry_after_t         *sip_retry_after;	/**< Retry-After */
  sip_timestamp_t           *sip_timestamp;	/**< Timestamp */
  sip_min_expires_t         *sip_min_expires;   /**< Min-Expires */

  sip_subject_t    	    *sip_subject;	/**< Subject (s) */
  sip_priority_t            *sip_priority;	/**< Priority */

  sip_call_info_t           *sip_call_info;	/**< Call-Info */
  sip_organization_t        *sip_organization;	/**< Organization */
  sip_server_t              *sip_server;	/**< Server */
  sip_user_agent_t          *sip_user_agent;	/**< User-Agent */
  sip_in_reply_to_t         *sip_in_reply_to;   /**< In-Reply-To */

  sip_accept_t              *sip_accept;	/**< Accept */
  sip_accept_encoding_t     *sip_accept_encoding; /**< Accept-Encoding */
  sip_accept_language_t     *sip_accept_language; /**< Accept-Language */

  sip_allow_t               *sip_allow;		/**< Allow */
  sip_require_t             *sip_require;	/**< Require */
  sip_supported_t           *sip_supported;	/**< Supported (k) */
  sip_unsupported_t         *sip_unsupported;	/**< Unsupported */

  /* RFC 3265 */
  sip_event_t               *sip_event;	        /**< Event (o) */
  sip_allow_events_t        *sip_allow_events;  /**< Allow-Events (u) */
  sip_subscription_state_t  *sip_subscription_state;
				/**< Subscription-State */

  sip_proxy_authenticate_t  *sip_proxy_authenticate;
				/**< Proxy-Authenticate */
  sip_proxy_authentication_info_t *sip_proxy_authentication_info;
				/**< Proxy-Authentication-Info */
  sip_proxy_authorization_t *sip_proxy_authorization;
				/**< Proxy-Authorization */
  sip_authorization_t       *sip_authorization;
				/**< Authorization */
  sip_www_authenticate_t    *sip_www_authenticate;
				/**< WWW-Authenticate */
  sip_authentication_info_t *sip_authentication_info;
                                /**< Authentication-Info */
  sip_error_info_t          *sip_error_info;    /**< Error-Info */
  sip_warning_t             *sip_warning;	/**< Warning */

  /* RFC 3515 */
  sip_refer_to_t            *sip_refer_to;      /**< Refer-To (r) */
  sip_referred_by_t         *sip_referred_by;   /**< Referred-By (b) */
  sip_replaces_t            *sip_replaces;      /**< Replaces */

  /* draft-ietf-sip-session-timer */
  sip_session_expires_t     *sip_session_expires;
				/**< Session-Expires (x) */
  sip_min_se_t              *sip_min_se;        /**< Min-SE */

  sip_path_t                *sip_path;        /**< Path */
  sip_service_route_t       *sip_service_route; /**< Service-Route */

  sip_reason_t              *sip_reason;        /**< Reason */

  sip_security_client_t     *sip_security_client; /**< Security-Client */
  sip_security_server_t     *sip_security_server; /**< Security-Server */
  sip_security_verify_t     *sip_security_verify; /**< Security-Verify */

  sip_privacy_t             *sip_privacy; /**< Privacy */

  sip_etag_t                *sip_etag;          /**< SIP-ETag */
  sip_if_match_t            *sip_if_match;      /**< SIP-If-Match */

  /* Entity headers */
  sip_mime_version_t        *sip_mime_version;	/**< MIME-Version */
  sip_content_type_t        *sip_content_type;	/**< Content-Type (c) */
  sip_content_encoding_t    *sip_content_encoding;
				/**< Content-Encoding (e) */
  sip_content_language_t    *sip_content_language; /**< Content-Language */
  sip_content_disposition_t *sip_content_disposition;
				/**< Content-Disposition */
  sip_content_length_t      *sip_content_length;/**< Content-Length (l) */

  /* === Headers end here */

  sip_unknown_t             *sip_unknown;       /**< Unknown headers */
  sip_separator_t           *sip_separator;
				/**< Separator between headers and payload */
  sip_payload_t             *sip_payload;	/**< Message payload */
  msg_multipart_t           *sip_multipart;     /**< Multipart MIME payload */
};


/** @ingroup sip_request
 * @brief Structure for @ref sip_request "SIP request line".
 */
struct sip_request_s
{
  sip_common_t     rq_common[1];   /**< Common fragment info */
  sip_error_t     *rq_next;	   /**< Link to next (dummy) */
  sip_method_t     rq_method;	   /**< Method enum */
  char const      *rq_method_name; /**< Method name */
  url_t            rq_url[1];	   /**< RequestURI */
  char const      *rq_version;     /**< Protocol version */
};

/**@ingroup sip_status
 * @brief Structure for @ref sip_status "SIP status line".
 */
struct sip_status_s
{
  sip_common_t   st_common[1];	/**< Common fragment info */
  sip_error_t   *st_next;	/**< Link to next (dummy) */
  char const    *st_version;	/**< Protocol version */
  int            st_status;	/**< Status code */
  char const    *st_phrase;	/**< Status phrase */
};

/**@ingroup sip_from
 * @brief Structure for @From and @To headers.
 */
struct sip_addr_s
{
  sip_common_t       a_common[1];   /**< Common fragment info */
  sip_error_t       *a_next;
  char const        *a_display;	    /**< Display name */
  url_t              a_url[1];	    /**< URL */
  msg_param_t const *a_params;	    /**< Parameter table  */
  char const        *a_comment;	    /**< Comment */

  char const        *a_tag;	    /**< Tag parameter */
};

#define a_user a_url->url_user
#define a_host a_url->url_host

/**@ingroup sip_accept
 * @brief Structure for @Accept header field.
 */
struct sip_accept_s
{
  sip_common_t        ac_common[1]; /**< Common fragment info */
  sip_accept_t       *ac_next;	    /**< Pointer to next @Accept value */
  char const         *ac_type;	    /**< Pointer to type/subtype */
  char const         *ac_subtype;   /**< Points after first slash in type */
  msg_param_t const  *ac_params;    /**< List of parameters */
  char const         *ac_q;	    /**< Value of q parameter */
};

/**@ingroup sip_allow
 * @brief Structure for @Allow header field.
 *
 * @NEW_1_12_5. (Before used struct msg_list_s with @Allow).
 */
struct sip_allow_s
{
  msg_common_t       k_common[1];   /**< Common fragment info */
  msg_list_t        *k_next;	    /**< Link to next */
  msg_param_t       *k_items;	    /**< List of allowed items */
  uint32_t           k_bitmap;	    /**< Bitmap of allowed methods.
				       @NEW_1_12_5. */
};

/**@ingroup sip_authentication_info
 * @brief Structure for @AuthenticationInfo header.
 *
 * @deprecated Use struct msg_auth_info_s instead.
 */
struct sip_authentication_info_s
{
  sip_common_t        ai_common[1]; /**< Common fragment info */
  sip_error_t        *ai_next;	    /**< Dummy link to next */
  msg_param_t const  *ai_params;    /**< List of authentication info */
};

/**@ingroup sip_call_id
 * @brief Structure for @CallID (and @InReplyTo) header fields.
 */
struct sip_call_id_s {
  sip_common_t   i_common[1];	    /**< Common fragment info */
  sip_call_id_t *i_next;	    /**< Link to next (In-Reply-To) */
  char const    *i_id;		    /**< ID value */
  uint32_t       i_hash;	    /**< Hash value (always nonzero) */
};

/**@ingroup sip_call_info
 * @brief Structure for @CallInfo header.
 */
struct sip_call_info_s
{
  sip_common_t        ci_common[1]; /**< Common fragment info */
  sip_call_info_t    *ci_next;	    /**< Link to next @CallInfo */
  url_t               ci_url[1];    /**< URI to call info  */
  msg_param_t const  *ci_params;    /**< List of parameters */
  char const         *ci_purpose;   /**< Value of @b purpose parameter */
};

/**@ingroup sip_cseq
 * @brief Structure for @CSeq header.
 */
struct sip_cseq_s
{
  sip_common_t   cs_common[1];	    /**< Common fragment info */
  sip_error_t   *cs_next;	    /**< Link to next (dummy) */
  uint32_t       cs_seq;	    /**< Sequence number */
  sip_method_t   cs_method;	    /**< Method enum */
  char const    *cs_method_name;    /**< Method name */
};

/**@ingroup sip_contact
 * @brief Structure for @Contact header field.
 */
struct sip_contact_s
{
  sip_common_t        m_common[1];  /**< Common fragment info */
  sip_contact_t      *m_next;	    /**< Link to next @Contact header */
  char const         *m_display;    /**< Display name */
  url_t               m_url[1];	    /**< SIP URL */
  msg_param_t const  *m_params;	    /**< List of contact-params */
  char const         *m_comment;    /**< Comment */

  char const         *m_q;	    /**< @Priority */
  char const         *m_expires;    /**< Expiration time */
};

/**@ingroup sip_content_length
 * @brief Structure for @ContentLength header.
 */
struct sip_content_length_s
{
  sip_common_t   l_common[1];	    /**< Common fragment info */
  sip_error_t   *l_next;	    /**< Link to next (dummy) */
  uint32_t       l_length;	    /**< Length in bytes */
};

#if DOCUMENTATION_ONLY
/**@ingroup sip_content_type
 *
 * @brief Structure for @ContentType header.
 */
struct sip_content_type_s
{
  sip_common_t        c_common[1];  /**< Common fragment info */
  sip_error_t        *c_next;	    /**< Dummy link to next */
  char const         *c_type;	    /**< Pointer to type/subtype */
  char const         *c_subtype;    /**< Points after first slash in type */
  msg_param_t const  *c_params;	    /**< List of parameters */
};
#endif

/**@ingroup sip_date
 * @brief Structure for @Date header.
 */
struct sip_date_s
{
  sip_common_t   d_common[1];	    /**< Common fragment info */
  sip_date_t    *d_next;	    /**< Link to next (dummy) */
  sip_time_t     d_time;	    /**< Seconds since Jan 1, 1900 */
};

/**@ingroup sip_error_info
 * @brief Structure for @ErrorInfo header.
 */
struct sip_error_info_s
{
  sip_common_t        ei_common[1]; /**< Common fragment info */
  sip_call_info_t    *ei_next;	    /**< Link to next @ErrorInfo */
  url_t               ei_url[1];    /**< URI to error description */
  msg_param_t const  *ei_params;    /**< List of parameters */
};

/**@ingroup sip_event
 * @brief Structure for @Event header.
 */
struct sip_event_s
{
  sip_common_t        o_common[1];  /**< Common fragment info */
  sip_error_t        *o_next;	    /**< Link to next (dummy) */
  char const *        o_type;	    /**< @Event type */
  msg_param_t const  *o_params;	    /**< List of parameters */
  char const         *o_id;	    /**< @Event ID */
};

/**@ingroup sip_expires
 * @brief Structure for @Expires header.
 */
struct sip_expires_s
{
  sip_common_t        ex_common[1]; /**< Common fragment info */
  sip_error_t        *ex_next;	    /**< Link to next (dummy) */
  sip_time_t          ex_date;	    /**< Seconds since Jan 1, 1900 */
# define ex_time ex_date
  sip_time_t          ex_delta;	    /**< Delta seconds */
};

/**@ingroup sip_max_forwards
 * @brief Structure for @MaxForwards header.
 */
struct sip_max_forwards_s
{
  sip_common_t        mf_common[1]; /**< Common fragment info */
  sip_error_t        *mf_next;	    /**< Link to next (dummy) */
  unsigned long       mf_count;	    /**< Forwarding count */
};

/**@ingroup sip_min_expires
 * @brief Structure for @MinExpires header.
 */
struct sip_min_expires_s
{
  sip_common_t        me_common[1]; /**< Common fragment info */
  sip_error_t        *me_next;	    /**< Link to next (dummy) */
  unsigned long       me_delta;	    /**< Seconds */
};

/**@ingroup sip_rack
 * @brief Structure for @RAck header.
 */
struct sip_rack_s
{
  sip_common_t        ra_common;        /**< Common fragment info */
  sip_error_t        *ra_next;		/**< Dummy link to next */
  uint32_t            ra_response;	/**< Sequence number of response */
  uint32_t            ra_cseq;		/**< Sequence number of request  */
  sip_method_t        ra_method;	/**< Original request method */
  char const         *ra_method_name;	/**< Original request method name */
};

/**@ingroup sip_refer_to
 * @brief Structure for @ReferTo header.
 */
struct sip_refer_to_s
{
  sip_common_t        r_common[1];  /**< Common fragment info */
  sip_error_t        *r_next;	    /**< Link to next (dummy) */
  char const         *r_display;
  url_t               r_url[1];	    /**< URI to reference */
  msg_param_t const  *r_params;	    /**< List of parameters */
};

/**@ingroup sip_referred_by
 * @brief Structure for @ReferredBy header.
 */
struct sip_referred_by_s
{
  sip_common_t        b_common[1];  /**< Common fragment info */
  sip_error_t        *b_next;	    /**< Link to next (dummy) */
  char const         *b_display;
  url_t               b_url[1];	    /**< Referrer-URI */
  msg_param_t const  *b_params;	    /**< List of parameters */
  char const         *b_cid;	    /**< The cid parameter */
};


/**@ingroup sip_replaces
 * @brief Structure for @Replaces header.
 */
struct sip_replaces_s
{
  sip_common_t        rp_common[1];   /**< Common fragment info */
  sip_error_t        *rp_next;	      /**< Link to next (dummy) */
  char const         *rp_call_id;     /**< @CallID of dialog to replace */
  msg_param_t const  *rp_params;      /**< List of parameters */
  char const         *rp_to_tag;      /**< Value of "to-tag" parameter */
  char const         *rp_from_tag;    /**< Value of "from-tag" parameter */
  unsigned            rp_early_only;  /**< early-only parameter */
};


/**@ingroup sip_retry_after
 * @brief Structure for @RetryAfter header.
 */
struct sip_retry_after_s {
  sip_common_t        af_common[1]; /**< Common fragment info */
  sip_error_t        *af_next;	    /**< Link to next (dummy) */
  sip_time_t          af_delta;	    /**< Seconds to before retry */
  char const         *af_comment;   /**< Comment string */
  msg_param_t const  *af_params;    /**< List of parameters */
  char const         *af_duration;  /**< Value of "duration" parameter */
};

/**@ingroup sip_request_disposition
 * @brief Structure for @RequestDisposition header.
 */
struct sip_request_disposition_s
{
  sip_common_t        rd_common[1]; /**< Common fragment info */
  sip_error_t        *rd_next;	    /**< Link to next (dummy) */
  msg_param_t        *rd_items;     /**< List of directives */
};

/**@ingroup sip_caller_preferences
 * @brief Structure for @AcceptContact and @RejectContact header fields.
 */
struct sip_caller_prefs_s
{
  sip_common_t        cp_common[1];   /**< Common fragment info */
  sip_caller_prefs_t *cp_next;	      /**< Link to next (dummy) */
  msg_param_t const  *cp_params;      /**< List of parameters */
  char const         *cp_q;           /**< @Priority */
  unsigned            cp_require :1;  /**< Value of "require" parameter */
  unsigned            cp_explicit :1; /**< Value of "explicit" parameter */
};

/**@ingroup sip_reason
 * @brief Structure for @Reason header field.
 */
struct sip_reason_s
{
  sip_common_t        re_common[1]; /**< Common fragment info */
  sip_reason_t       *re_next;	    /**< Link to next */
  char const         *re_protocol;  /**< Protocol */
  msg_param_t const  *re_params;    /**< List of reason parameters */
  char const         *re_cause;	    /**< Value of cause parameter */
  char const         *re_text;	    /**< Value of text parameter */
};

/**@ingroup sip_route
 * @brief Structure for @Route and @RecordRoute header fields.
 */
struct sip_route_s
{
  sip_common_t        r_common[1];  /**< Common fragment info */
  sip_route_t        *r_next;	    /**< Link to next */
  char const         *r_display;    /**< Display name */
  url_t               r_url[1];	    /**< @Route URL */
  msg_param_t const  *r_params;	    /**< List of route parameters */
};

/**@ingroup sip_rseq
 * @brief Structure for @RSeq header.
 */
struct sip_rseq_s
{
  sip_common_t        rs_common[1];	/**< Common fragment info */
  sip_error_t        *rs_next;		/**< Dummy link to next */
  unsigned long       rs_response;	/**< Sequence number of response */
};

/**@ingroup sip_session_expires
 * @brief Structure for @SessionExpires header.
 */
struct sip_session_expires_s
{
  sip_common_t        x_common[1];	/**< Common fragment info */
  sip_error_t        *x_next;		/**< Dummy link to next */
  unsigned long       x_delta;		/**< Delta-seconds */
  msg_param_t const  *x_params;		/**< List of parameters */
  char const         *x_refresher;	/**< Value of "refresher"
					 * parameter: UAS or UAC */
};

/**@ingroup sip_min_se
 * @brief Structure for @MinSE header.
 */
struct sip_min_se_s
{
  sip_common_t        min_common[1];	/**< Common fragment info */
  sip_error_t        *min_next;		/**< Dummy link to next */
  unsigned long       min_delta;	/**< Delta-seconds */
  msg_param_t const  *min_params;	/**< List of extension parameters */
};

/**@ingroup sip_subscription_state
 * @brief Structure for @SubscriptionState header.
 */
struct sip_subscription_state_s
{
  sip_common_t        ss_common[1];   /**< Common fragment info */
  sip_error_t        *ss_next;	      /**< Dummy link to next */
  /** Subscription state: "pending", "active" or "terminated" */
  char const         *ss_substate;
  msg_param_t const  *ss_params;      /**< List of parameters */
  char const         *ss_reason;      /**< Reason for termination  */
  char const         *ss_expires;     /**< Subscription lifetime */
  char const         *ss_retry_after; /**< Value of retry-after parameter */
};

/**@ingroup sip_timestamp
 * @brief Structure for @Timestamp header.
 */
struct sip_timestamp_s
{
  sip_common_t        ts_common[1]; /**< Common fragment info */
  sip_error_t        *ts_next;	    /**< Dummy link to next */
  char const         *ts_stamp;	    /**< Original timestamp */
  char const         *ts_delay;	    /**< Delay at UAS */
};

/**@ingroup sip_via
 * @brief Structure for @Via header field.
 */
struct sip_via_s
{
  sip_common_t        v_common[1];  /**< Common fragment info */
  sip_via_t          *v_next;	    /**< Link to next @Via header */
  char const         *v_protocol;   /**< Application and transport protocol */
  char const         *v_host;	    /**< Hostname */
  char const         *v_port;	    /**< Port number */
  msg_param_t const  *v_params;	    /**< List of via-params */
  char const         *v_comment;    /**< Comment */
  char const         *v_ttl;	    /**< Value of "ttl" parameter */
  char const         *v_maddr;	    /**< Value of "maddr" parameter */
  char const         *v_received;   /**< Value of "received" parameter*/
  char const         *v_branch;	    /**< Value of "branch" parameter */
  char const         *v_rport;	    /**< Value of "rport" parameter */
  char const         *v_comp;	    /**< Value of "comp" parameter */
};

/**@ingroup sip_security_client
 * @brief Structure for @SecurityClient, @SecurityServer, and
 * @SecurityVerify headers.
 */
struct sip_security_agree_s
{
  sip_common_t        sa_common[1]; /**< Common fragment info */
  struct sip_security_agree_s
                     *sa_next;	    /**< Link to next mechanism */
  char const         *sa_mec;	    /**< Security mechanism */
  msg_param_t const  *sa_params;    /**< List of mechanism parameters */
  char const         *sa_q;	    /**< Value of q (preference) parameter */
  char const         *sa_d_alg;	    /**< Value of d-alg parameter */
  char const         *sa_d_qop;	    /**< Value of d-qop parameter */
  char const         *sa_d_ver;	    /**< Value of d-ver parameter */
};

/**@ingroup sip_privacy
 * @brief Structure for @Privacy header.
 */
struct sip_privacy_s
{
  sip_common_t       priv_common[1];/**< Common fragment info */
  sip_error_t       *priv_next;	    /**< Dummy link */
  msg_param_t const *priv_values;   /**< @Privacy values */
};

/* union representing any SIP header
 * these are arrays of size 1 for easy casting
 */
union sip_header_u
{
  sip_common_t               sh_common[1];
  struct
  {
    sip_common_t             shn_common;
    sip_header_t            *shn_next;
  }                          sh_header_next[1];
#define sh_next              sh_header_next->shn_next
#define sh_class sh_common->h_class
#define sh_succ  sh_common->h_succ
#define sh_prev  sh_common->h_prev
#define sh_data  sh_common->h_data
#define sh_len   sh_common->h_len

  sip_addr_t                 sh_addr[1];
  sip_auth_t                 sh_auth[1];
  sip_generic_t              sh_generic[1];

  sip_request_t              sh_request[1];
  sip_status_t               sh_status[1];

  sip_error_t                sh_error[1];

  sip_via_t                  sh_via[1];
  sip_route_t                sh_route[1];
  sip_record_route_t         sh_record_route[1];
  sip_max_forwards_t         sh_max_forwards[1];

  sip_from_t                 sh_from[1];
  sip_to_t                   sh_to[1];
  sip_contact_t              sh_contact[1];
  sip_call_id_t              sh_call_id[1];
  sip_cseq_t                 sh_cseq[1];
  sip_rseq_t                 sh_rseq[1];
  sip_rack_t                 sh_rack[1];

  sip_subject_t              sh_subject[1];
  sip_priority_t             sh_priority[1];

  sip_date_t                 sh_date[1];
  sip_retry_after_t          sh_retry_after[1];
  sip_timestamp_t            sh_timestamp[1];
  sip_expires_t              sh_expires[1];
  sip_min_expires_t          sh_min_expires[1];

  sip_call_info_t            sh_call_info[1];
  sip_organization_t         sh_organization[1];
  sip_server_t               sh_server[1];
  sip_user_agent_t           sh_user_agent[1];
  sip_in_reply_to_t          sh_in_reply_to[1];

  sip_accept_t               sh_accept[1];
  sip_accept_encoding_t      sh_accept_encoding[1];
  sip_accept_language_t      sh_accept_language[1];

  sip_allow_t                sh_allow[1];
  sip_require_t              sh_require[1];
  sip_proxy_require_t        sh_proxy_require[1];
  sip_supported_t            sh_supported[1];
  sip_unsupported_t          sh_unsupported[1];

  sip_event_t                sh_event[1];
  sip_allow_events_t         sh_allow_events[1];
  sip_subscription_state_t   sh_subscription_state[1];

  sip_proxy_authenticate_t   sh_proxy_authenticate[1];
  sip_proxy_authentication_info_t sh_proxy_authentication_info[1];
  sip_proxy_authorization_t  sh_proxy_authorization[1];

  sip_authorization_t        sh_authorization[1];
  sip_www_authenticate_t     sh_www_authenticate[1];
  sip_authentication_info_t  sh_authentication_info[1];

  sip_error_info_t           sh_error_info[1];
  sip_warning_t              sh_warning[1];

  sip_refer_to_t             sh_refer_to[1];
  sip_referred_by_t          sh_referred_by[1];
  sip_replaces_t             sh_replaces[1];

  /* Caller-Preferences */
  sip_caller_prefs_t         sh_caller_prefs[1];
  sip_request_disposition_t  sh_request_disposition[1];
  sip_accept_contact_t       sh_accept_contact[1];
  sip_reject_contact_t       sh_reject_contact[1];

  sip_session_expires_t      sh_session_expires[1];
  sip_min_se_t               sh_min_se[1];

  sip_path_t                 sh_path[1];
  sip_service_route_t        sh_service_route[1];

  sip_reason_t               sh_reason[1];

  sip_security_client_t      sh_security_client[1];
  sip_security_server_t      sh_security_server[1];
  sip_security_verify_t      sh_security_verify[1];

  sip_privacy_t              sh_privacy[1];

  sip_etag_t                 sh_etag[1];
  sip_if_match_t             sh_if_match[1];

  /* Entity headers */
  sip_mime_version_t         sh_mime_version[1];
  sip_content_type_t         sh_content_type[1];
  sip_content_encoding_t     sh_content_encoding[1];
  sip_content_language_t     sh_content_language[1];
  sip_content_length_t       sh_content_length[1];
  sip_content_disposition_t  sh_content_disposition[1];

  sip_unknown_t              sh_unknown[1];

  sip_separator_t            sh_separator[1];
  sip_payload_t              sh_payload[1];
};

SOFIA_END_DECLS

#endif /* !defined(SIP_H) */

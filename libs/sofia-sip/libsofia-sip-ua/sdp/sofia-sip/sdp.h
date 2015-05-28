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

#ifndef SDP_H
#define SDP_H
/**@file sofia-sip/sdp.h  Simple SDP (RFC 2327) Interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Fri Feb 18 08:54:48 2000 ppessi
 */

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif
#ifndef SU_TYPES_H
#include <sofia-sip/su_types.h>
#endif

SOFIA_BEGIN_DECLS

/** SDP session description */
typedef struct sdp_session_s     sdp_session_t;
/** SDP version "v=" line */
typedef unsigned long            sdp_version_t;
/** SDP origin "o=" line */
typedef struct sdp_origin_s      sdp_origin_t;
/** SDP connection "c=" line */
typedef struct sdp_connection_s  sdp_connection_t;
/** SDP bandwidth "b=" line */
typedef struct sdp_bandwidth_s   sdp_bandwidth_t;
/** SDP time "t=" line */
typedef struct sdp_time_s        sdp_time_t;
/** SDP repeat "r=" line */
typedef struct sdp_repeat_s      sdp_repeat_t;
/** SDP timezone "z=" line */
typedef struct sdp_zone_s        sdp_zone_t;
/** SDP encryption key "k=" line */
typedef struct sdp_key_s         sdp_key_t;
/** SDP attribute "a=" line */
typedef struct sdp_attribute_s   sdp_attribute_t;
/** SDP media "m=" line */
typedef struct sdp_media_s       sdp_media_t;
/** SDP list ("e=", "p=" lines) */
typedef struct sdp_list_s        sdp_list_t;
/** SDP rtpmap attribute */
typedef struct sdp_rtpmap_s      sdp_rtpmap_t;

/** Message text */
typedef char const               sdp_text_t;

#define SDP_MIME_TYPE "application/sdp"

enum {
  SDP_CURRENT_VERSION = 0
};

/** Session description */
struct sdp_session_s
{
  int                sdp_size;		/**< sizeof sdp_session_t */
  sdp_session_t     *sdp_next;	        /**< Next description in list */
  sdp_version_t      sdp_version[1];	/**< SDP version */
  sdp_origin_t      *sdp_origin;	/**< Owner/creator and session ID */
  sdp_text_t        *sdp_subject;	/**< Session name */
  sdp_text_t        *sdp_information;	/**< Session information  */
  sdp_text_t        *sdp_uri;		/**< URi of description */
  sdp_list_t   	    *sdp_emails;	/**< E-mail address(s) */
  sdp_list_t   	    *sdp_phones;	/**< Phone number(s)  */
  sdp_connection_t  *sdp_connection;	/**< Group (or member) address */
  sdp_bandwidth_t   *sdp_bandwidths;	/**< Session bandwidth */
  sdp_time_t        *sdp_time;		/**< Session active time */
  sdp_key_t         *sdp_key;	        /**< Session key */
  sdp_attribute_t   *sdp_attributes;    /**< Session attributes */
  sdp_text_t        *sdp_charset;       /**< SDP charset (default is UTF8) */
  sdp_media_t       *sdp_media;         /**< Media descriptors */
};

/** Session description identification */
struct sdp_origin_s
{
  int               o_size;		/**< sizeof sdp_origin_t */
  sdp_text_t       *o_username;		/**< Username of originator */
  uint64_t          o_id;		/**< Session identification  */
  uint64_t          o_version;		/**< Version of session description */
  sdp_connection_t *o_address;		/**< Address of originator */
};

/** Network type */
typedef enum
{
  sdp_net_x = 0,			/**< Unknown network type */
  sdp_net_in = 1		        /**< Internet */
} sdp_nettype_e;

/** Address type */
typedef enum
{
  sdp_addr_x   = 0,			/**< Unknown address type */
  sdp_addr_ip4 = 1,			/**< IPv4 address */
  sdp_addr_ip6 = 2,			/**< IPv6 address */
} sdp_addrtype_e;

/** SDP connection - host or group address */
struct sdp_connection_s
{
  int               c_size;		/**< Size fo sdp_connection_t */
  sdp_connection_t *c_next;		/**< Next connection in list */
  sdp_nettype_e     c_nettype;		/**< Network type */
  sdp_addrtype_e    c_addrtype;		/**< Address type */
  sdp_text_t       *c_address;		/**< Host or group address */
  unsigned          c_ttl : 8;		/**< Time to live (scope) */
  unsigned          c_mcast : 1;        /**< True if multicast */
  unsigned : 0;
  unsigned          c_groups;		/**< Number of groups (if multiple) */
};

/** Bandwdith type */
typedef enum
{
  sdp_bw_x,				/**< Unknown bandwidth type */
  sdp_bw_ct,				/**< Conference total */
  sdp_bw_as,				/**< Application-specific */
  sdp_bw_tias,				/**< Application-specific */
} sdp_bandwidth_e;

/** Session or media bandwidth. */
struct sdp_bandwidth_s
{
  int              b_size;		/**< Size fo sdp_bandwidth_t */
  sdp_bandwidth_t *b_next;		/**< Next bw description in list */
  sdp_bandwidth_e  b_modifier;		/**< Meaning of value
					     (total, or per application).  */
  sdp_text_t      *b_modifier_name;     /**< Modifier if not well-known */
  unsigned long    b_value;		/**< Bandwidth in kilobits per second */
};

/** Active time description. */
struct sdp_time_s
{
  int            t_size;		/**< sizeof sdp_time_t in bytes */
  sdp_time_t    *t_next;		/**< Next time description in list */
  unsigned long  t_start;		/**< Start time (seconds since 1900) */
  unsigned long  t_stop;		/**< Stop time (seconds since 1900) */
  sdp_repeat_t  *t_repeat;		/**< Repeat information */
  sdp_zone_t    *t_zone;		/**< Time Zone infromation */
};

/** Description of repetition. */
struct sdp_repeat_s
{
  int           r_size;			/**< Size of structure including
					 * r_offsets[r_number_of_offsets]
					 */
  int           r_number_of_offsets;	/**< Number of offsets in list */
  unsigned long r_interval;		/**< Time between activations */
  unsigned long r_duration;		/**< Duration of activation */
  unsigned long r_offsets[1];	        /**< List of offsets from start-time */
};

/** Timezone */
struct sdp_zone_s
{
  /** Size of structure including z_adjustments[z_number_of_adjustments] */
  int z_size;
  int z_number_of_adjustments;		/**< Number of adjustments in list  */
  struct {
    unsigned long z_at;			/**< Adjustment time  */
    long          z_offset;		/**< Adjustment offset  */
  } z_adjustments[1];		        /**< List of timezone adjustments */
};

/** Mechanism to be used to obtain session key */
typedef enum {
  sdp_key_x,				/**< Unknown mechanism */
  sdp_key_clear,			/**< Key is included untransformed */
  sdp_key_base64,			/**< Key is encoded with base64 */
  sdp_key_uri,				/**< URI used to obtain a key */
  sdp_key_prompt			/**< No key is included,
					     prompt user for key */
} sdp_key_method_e;

/** Session key */
struct sdp_key_s
{
  int              k_size;		/**< sizeof sdp_key_t  */
  sdp_key_method_e k_method;		/**< Mechanism used to obtain key */
  sdp_text_t      *k_method_name;	/**< Mechanism if not known  */
  sdp_text_t      *k_material;		/**< Encryption key  */
};

/** Session or media attribute */
struct sdp_attribute_s {
  int              a_size;		/**< sizeof sdp_attribute_t  */
  sdp_attribute_t *a_next;		/**< Next attribute in list */
  sdp_text_t      *a_name;		/**< Attribute name */
  sdp_text_t      *a_value;		/**< Attribute value */
};

/** Media type @sa RFC2327 page 18. */
typedef enum
{
  sdp_media_x = 0,			/**< Unknown media */
  sdp_media_any,		        /**< * wildcard */
  sdp_media_audio,			/**< Audio */
  sdp_media_video,			/**< Video */
  sdp_media_application,		/**< Conferencing */
  sdp_media_data,			/**< Bulk data transfer */
  sdp_media_control,			/**< Additional conference control */
  sdp_media_message,			/**< Messaging sessions*/
  sdp_media_image,			/**< Image browsing sessions,
					 *   e.g., JPIP or T.38. */
  sdp_media_red				/**< Redundancy. @NEW_1_12_4. */
} sdp_media_e;

/** Media transport protocol. */
typedef enum
{
  sdp_proto_x = 0,			/**< Unknown transport  */
  sdp_proto_tcp = 6,			/**< TCP  */
  sdp_proto_udp = 17,			/**< Plain UDP */
  sdp_proto_rtp = 256,			/**< RTP/AVP */
  sdp_proto_srtp = 257,			/**< RTP/SAVP  */
  sdp_proto_udptl = 258,		/**< UDPTL. @NEW_1_12_4. */
  sdp_proto_msrp =  259,		/**< TCP/MSRP @NEW_MSRP*/
  sdp_proto_msrps = 260,		/**< TCP/TLS/MSRP @NEW_MSRP*/
  sdp_proto_extended_srtp = 261, /** WEBRTC SAVPF */
  sdp_proto_extended_rtp = 262, /** WEBRTC AVPF */
  sdp_proto_tls = 511,			/**< TLS over TCP */
  sdp_proto_any = 512		        /**< * wildcard */
} sdp_proto_e;

/** Session mode. @note Identical to rtp_mode_t. */
typedef enum {
  sdp_inactive = 0,
  sdp_sendonly = 1,
  sdp_recvonly = 2,
  sdp_sendrecv = sdp_sendonly | sdp_recvonly
} sdp_mode_t;

/** Media announcement.
 *
 * This structure describes one media type, e.g., audio.  The description
 * contains the transport address (IP address and port) used for the group,
 * the transport protocol used, the media formats or RTP payload types, and
 * optionally media-specific bandwidth specification, encryption key and
 * attributes.
 *
 * There is a pointer (m_user) for the application data, too.
 */
struct sdp_media_s
{
  int               m_size;		/**< sizeof sdp_media_t  */
  sdp_media_t      *m_next;		/**< Next media announcement  */
  sdp_session_t    *m_session;          /**< Back-pointer to session level */

  sdp_media_e       m_type;		/**< Media type  */
  sdp_text_t       *m_type_name;	/**< Media type name */
  unsigned long     m_port;		/**< Transport port number */
  unsigned long     m_number_of_ports;	/**< Number of ports (if multiple) */
  sdp_proto_e       m_proto;		/**< Transport protocol  */
  sdp_text_t       *m_proto_name;	/**< Transport protocol name */
  sdp_list_t       *m_format;		/**< List of media formats */
  sdp_rtpmap_t     *m_rtpmaps;		/**< List of RTP maps */
  sdp_text_t       *m_information;	/**< Media information */
  sdp_connection_t *m_connections;	/**< List of addresses used */
  sdp_bandwidth_t  *m_bandwidths;	/**< Bandwidth specification */
  sdp_key_t        *m_key;		/**< Media key */
  sdp_attribute_t  *m_attributes;	/**< Media attributes */

  void             *m_user;	        /**< User data. */

  /** Rejected media */
  unsigned          m_rejected : 1;
  /** Inactive, recvonly, sendonly, sendrecv */
  /* sdp_mode_t */ unsigned m_mode : 2;
  unsigned          : 0;
};

/** Text list */
struct sdp_list_s
{
  int              l_size;		/**< sizeof sdp_list_t  */
  sdp_list_t      *l_next;		/**< Next text entry in list */
  sdp_text_t      *l_text;	        /**< Text as C string */
};

/** Mapping from RTP payload to codec.
 *
 * The sdp_rtpmap_t() structure defines a mapping from an RTP payload to a
 * particular codec.  In case of well-known payloads, the sdp_rtpmap_t()
 * structure may be predefined, that is, generated by SDP parser without
 * corresponding "a" line in the SDP.  The sdp_rtpmap_t() structure may also
 * contain the @c fmtp attribute, which is used to convey format-specific
 * parameters.
 */
struct sdp_rtpmap_s {
  int            rm_size;		/**< sizeof sdp_rtpmap_t  */
  sdp_rtpmap_t  *rm_next;		/**< Next RTP map entry  */
  sdp_text_t    *rm_encoding;		/**< Codec name */
  unsigned long  rm_rate;		/**< Sampling rate */
  sdp_text_t    *rm_params;		/**< Format-specific parameters  */
  sdp_text_t    *rm_fmtp;	        /**< Contents of fmtp */
  unsigned       rm_predef : 1;	        /**< is this entry well-known? */
  unsigned       rm_pt : 7;		/**< Payload type */
  unsigned       rm_any : 1;	        /**< Wildcard entry */
  unsigned       :0;
};

SOFIAPUBVAR sdp_rtpmap_t const * const sdp_rtpmap_well_known[128];

/** Duplicate an SDP session description structure. */
SOFIAPUBFUN sdp_session_t *sdp_session_dup(su_home_t *, sdp_session_t const *);

/** Duplicate an SDP origin structure. */
SOFIAPUBFUN
sdp_origin_t    *sdp_origin_dup(su_home_t *, sdp_origin_t const *);

/** Duplicate an SDP connection structure. */
SOFIAPUBFUN
sdp_connection_t *sdp_connection_dup(su_home_t *, sdp_connection_t const *);

/** Duplicate an SDP bandwidth structure. */
SOFIAPUBFUN
sdp_bandwidth_t  *sdp_bandwidth_dup(su_home_t *, sdp_bandwidth_t const *);

/** Duplicate an SDP time structure. */
SOFIAPUBFUN
sdp_time_t       *sdp_time_dup(su_home_t *, sdp_time_t const *);

/** Duplicate an SDP repeat structure. */
SOFIAPUBFUN
sdp_repeat_t     *sdp_repeat_dup(su_home_t *, sdp_repeat_t const *);

/** Duplicate an SDP timezone structure. */
SOFIAPUBFUN
sdp_zone_t       *sdp_zone_dup(su_home_t *, sdp_zone_t const *);

/** Duplicate an SDP key structure. */
SOFIAPUBFUN
sdp_key_t        *sdp_key_dup(su_home_t *, sdp_key_t const *);

/** Duplicate an SDP attribute structure. */
SOFIAPUBFUN
sdp_attribute_t  *sdp_attribute_dup(su_home_t *, sdp_attribute_t const *);

/** Duplicate an SDP media description structure. */
SOFIAPUBFUN
sdp_media_t *sdp_media_dup(su_home_t *, sdp_media_t const *,
			   sdp_session_t *);

/** Duplicate a list of SDP media description structures. */
SOFIAPUBFUN
sdp_media_t *sdp_media_dup_all(su_home_t *, sdp_media_t const *,
			       sdp_session_t *);

/** Duplicate a list structure. */
SOFIAPUBFUN
sdp_list_t       *sdp_list_dup(su_home_t *, sdp_list_t const *);

/** Duplicate an rtpmap structure. */
SOFIAPUBFUN
sdp_rtpmap_t     *sdp_rtpmap_dup(su_home_t *, sdp_rtpmap_t const *);

/** Compare two session descriptions. */
SOFIAPUBFUN int sdp_session_cmp(sdp_session_t const *a,
				sdp_session_t const *b);

/** Compare two origin fields */
SOFIAPUBFUN int sdp_origin_cmp(sdp_origin_t const *a,
			       sdp_origin_t const *b);

/** Compare two connection fields */
SOFIAPUBFUN int sdp_connection_cmp(sdp_connection_t const *,
				   sdp_connection_t const *b);

/** Compare two bandwidth (b=) fields */
SOFIAPUBFUN int sdp_bandwidth_cmp(sdp_bandwidth_t const *a,
				  sdp_bandwidth_t const *b);

/** Compare two time fields */
SOFIAPUBFUN int sdp_time_cmp(sdp_time_t const *a, sdp_time_t const *b);

/* Compare two repeat (r=) fields */
SOFIAPUBFUN int sdp_repeat_cmp(sdp_repeat_t const *a, sdp_repeat_t const *b);

/* Compare two zone (z=) fields */
SOFIAPUBFUN int sdp_zone_cmp(sdp_zone_t const *a, sdp_zone_t const *b);

/** Compare two key (k=) fields. */
SOFIAPUBFUN int sdp_key_cmp(sdp_key_t const *a, sdp_key_t const *b);

/** Compare two attribute (a=) fields */
SOFIAPUBFUN int sdp_attribute_cmp(sdp_attribute_t const *,
				  sdp_attribute_t const *);

/** Compare two media (m=) descriptions */
SOFIAPUBFUN int sdp_media_cmp(sdp_media_t const *, sdp_media_t const *);

/** Compare two rtpmap structures. */
SOFIAPUBFUN int sdp_rtpmap_cmp(sdp_rtpmap_t const *a, sdp_rtpmap_t const *b);

/** Compare two text lists */
SOFIAPUBFUN int sdp_list_cmp(sdp_list_t const *a, sdp_list_t const *b);

/** Get connections of a media description */
SOFIAPUBFUN sdp_connection_t *sdp_media_connections(sdp_media_t const *m);

/** Check if media uses RTP as its transport protocol  */
SOFIAPUBFUN int sdp_media_has_rtp(sdp_media_t const *m);

/** Set media type */
SOFIAPUBFUN void sdp_media_type(sdp_media_t *m, char const *s);

/** Set transport protocol */
SOFIAPUBFUN void sdp_media_transport(sdp_media_t *m, char const *s);

/** Find named attribute from given list. */
SOFIAPUBFUN sdp_attribute_t  *sdp_attribute_find(sdp_attribute_t const *a,
						 char const *name);

/** Find named attribute from given lists. */
SOFIAPUBFUN sdp_attribute_t *sdp_attribute_find2(sdp_attribute_t const *a,
						 sdp_attribute_t const *a2,
						 char const *name);

/** Get session mode from attribute list. */
SOFIAPUBFUN sdp_mode_t sdp_attribute_mode(sdp_attribute_t const *a,
					  sdp_mode_t defmode);

/** Get session mode from attribute list. */
SOFIAPUBFUN sdp_attribute_t *sdp_attribute_by_mode(su_home_t *,
						   sdp_mode_t mode);

/** Find a mapped attribute. */
SOFIAPUBFUN
sdp_attribute_t *sdp_attribute_mapped_find(sdp_attribute_t const *a,
					   char const *name,
					   int pt, char **return_result);

/** Append a attribute to a list of attributes. */
SOFIAPUBFUN void sdp_attribute_append(sdp_attribute_t **list,
				      sdp_attribute_t const *a);

/** Replace a attribute within a list of attributes. */
SOFIAPUBFUN int sdp_attribute_replace(sdp_attribute_t **list,
				      sdp_attribute_t *a,
				      sdp_attribute_t **return_replaced);

/** Remove a named attribute from a list of attributes. */
SOFIAPUBFUN sdp_attribute_t *sdp_attribute_remove(sdp_attribute_t **list,
						  char const *name);

/* Return 1 if m= line struct matches with given type and name */
SOFIAPUBFUN unsigned sdp_media_match(sdp_media_t const *m,
				     sdp_media_e type,
				     sdp_text_t *type_name,
				     sdp_proto_e proto,
				     sdp_text_t *proto_name);

SOFIAPUBFUN unsigned sdp_media_match_with(sdp_media_t const *a,
					  sdp_media_t const *b);

/** Count media lines in SDP. */
SOFIAPUBFUN unsigned sdp_media_count(sdp_session_t const *sdp,
				     sdp_media_e type,
				     sdp_text_t *type_name,
				     sdp_proto_e proto,
				     sdp_text_t *proto_name);

SOFIAPUBFUN unsigned sdp_media_count_with(sdp_session_t const *sdp,
					  sdp_media_t const *m0);

/** Return true if media uses RTP */
SOFIAPUBFUN int sdp_media_uses_rtp(sdp_media_t const *m);

/** Check if payload type, rtp rate and parameters match in rtpmaps*/
SOFIAPUBFUN int sdp_rtpmap_match(sdp_rtpmap_t const *, sdp_rtpmap_t const *);

/** Search for matching rtpmap from list */
SOFIAPUBFUN sdp_rtpmap_t *sdp_rtpmap_find_matching(sdp_rtpmap_t const *list,
						   sdp_rtpmap_t const *rm);

/* ======================================================================== */

/** Flags given to sdp_parse()/sdp_print(). */
enum sdp_parse_flags_e {
  /** Accept only conforming SDP */
  sdp_f_strict = 1,
  /** Accept any network type. */
  sdp_f_anynet = 2,
  /** Reallocate message. */
  sdp_f_realloc = 4,
  /** Include well-known rtpmaps in message, too */
  sdp_f_all_rtpmaps = 8,
  /** Print buffer already contains a valid prefix */
  sdp_f_print_prefix = 16,
  /** Connection line with INADDR_ANY is considered equal to sendonly */
  sdp_f_mode_0000 = 32,
  /** Don't run sanity check */
  sdp_f_insane = 64,
  /** Don't require c= for each media line */
  sdp_f_c_missing = 128,
  /** Parse SDP config files */
  sdp_f_config = 256,
  /** Do not generate or parse SDP mode */
  sdp_f_mode_manual = 512,
  /** Always generate media-level mode attributes */
  sdp_f_mode_always = 1024
};

/** SDP parser handle. */
typedef struct sdp_parser_s sdp_parser_t;
typedef sdp_parser_t  *sdp_parser;

SOFIAPUBFUN sdp_parser_t *sdp_parse(su_home_t *,
				    char const msg[], issize_t msgsize,
				    int flags);
SOFIAPUBFUN char const *sdp_parsing_error(sdp_parser_t *p);
SOFIAPUBFUN sdp_session_t *sdp_session(sdp_parser_t *p);
SOFIAPUBFUN void sdp_parser_free(sdp_parser_t *p);

SOFIAPUBFUN int sdp_sanity_check(sdp_parser_t *);

SOFIAPUBFUN su_home_t *sdp_parser_home(sdp_parser_t *);

/* ======================================================================== */

/** SDP printer handle */
typedef struct sdp_printer_s sdp_printer_t;
typedef sdp_printer_t *sdp_printer;

SOFIAPUBFUN sdp_printer_t *sdp_print(su_home_t *, sdp_session_t const *,
				     char msgbuf[], isize_t maxmsgsize,
				     int flags);
SOFIAPUBFUN char const *sdp_printing_error(sdp_printer_t *p);
SOFIAPUBFUN char const *sdp_message(sdp_printer_t *p);
SOFIAPUBFUN isize_t sdp_message_size(sdp_printer_t *p);
SOFIAPUBFUN void sdp_printer_free(sdp_printer_t *p);

#define sdp_mapped_attribute_find sdp_attribute_mapped_find
#define sdp_free_parser  sdp_parser_free
#define sdp_free_printer sdp_printer_free

SOFIA_END_DECLS

#endif /* SDP_H */

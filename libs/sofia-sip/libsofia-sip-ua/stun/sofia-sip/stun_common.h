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

#ifndef STUN_COMMON_H
/** Defined when <sofia-sip/stun_common.h> has been included. */
#define STUN_COMMON_H

/**
 * @file sofia-sip/stun_common.h
 * @brief
 *
 * @author Tat Chan <Tat.Chan@nokia.com>
 *
 * @date Created: Fri Oct  3 13:39:55 2003 ppessi
 *
 */

#include <sofia-sip/su_localinfo.h>

SOFIA_BEGIN_DECLS

/* Define Message Types */
#define BINDING_REQUEST               0x0001
#define BINDING_RESPONSE              0x0101
#define BINDING_ERROR_RESPONSE        0x0111
#define SHARED_SECRET_REQUEST         0x0002
#define SHARED_SECRET_RESPONSE        0x0102
#define SHARED_SECRET_ERROR_RESPONSE  0x0112

/* Define Attribute Types */
#define MAPPED_ADDRESS                0x0001
#define RESPONSE_ADDRESS              0x0002
#define CHANGE_REQUEST                0x0003
#define SOURCE_ADDRESS                0x0004
#define CHANGED_ADDRESS               0x0005
#define USERNAME                      0x0006
#define PASSWORD                      0x0007
#define MESSAGE_INTEGRITY             0x0008
#define ERROR_CODE                    0x0009
#define UNKNOWN_ATTRIBUTES            0x000a
#define REFLECTED_FROM                0x000b
#define STUN_A_REALM                  0x0014 /* XXX: check value in 3489bis-05+ */
#define STUN_A_NONCE                  0x0015 /* XXX: check value in 3489bis-05+ */
#define STUN_A_XOR_MAPPED_ADDRESS     0x0020
#define STUN_A_FINGERPRINT            0x0023
#define STUN_A_SERVER                 0x8022
#define STUN_A_ALTERNATE_SERVER       0x8023
#define STUN_A_REFRESH_INTERVAL       0x8024

/* Defines for mandatory and optional attributes */
#define STUN_A_LAST_MANDATORY         0x0023 /**< largest attribute in the current
						spec (see above for exceptions
						for buggy servers) */
#define STUN_A_OPTIONAL               0x7fff

/* Compability attribute types */
#define STUN_A_ALTERNATE_SERVER_DEP   0x000e /**< historic from early fc3489bis drafts */
#define STUN_A_BUGGYSERVER_XORONLY    0x0021 /**< workaround for stund-0.94 and older */
#define STUN_A_BUGGYSERVER_SERVER     0x0022 /**< workaround for stund-0.94 and older */
#define LARGEST_ATTRIBUTE             STUN_A_LAST_MANDATORY /**< deprecated API */
#define OPTIONAL_ATTRIBUTE            STUN_A_OPTIONAL /**< deprecated API */

/* Stun response codes */
#define STUN_400_BAD_REQUEST             400
#define STUN_401_UNAUTHORIZED            401
#define STUN_420_UNKNOWN_ATTRIBUTE       420
#define STUN_430_STALE_CREDENTIALS       430
#define STUN_431_INTEGRITY_CHECK_FAILURE 431
#define STUN_432_MISSING_USERNAME        432
#define STUN_433_USE_TLS                 433
#define STUN_500_SERVER_ERROR            500
#define STUN_600_GLOBAL_FAILURE          600

/* flags for CHANGE_REQUEST */
#define STUN_CR_CHANGE_IP               0x0004
#define STUN_CR_CHANGE_PORT             0x0002

/* mask for ERROR_CODE */
#define STUN_EC_CLASS                   0x0070
#define STUN_EC_NUM                     0x000F

#define RAND_MAX_16                     65535

#define STUN_TID_BYTES                  16

/* other protocol specific parameters */
#define STUN_MAX_RETRX                  5 /* should be 8? */
#define STUN_MAX_RETRX_INT              1600  /**< max retrx interval in
						   millisec */
#define STUN_DEFAULT_PORT               3478  /**< from RFC3489 */

/*
 * STUN header format
 */
  /*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         message type          |       message length          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                       Transaction ID                          |
   |                                                               |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  */
struct stun_buffer_s {
  unsigned char *data;      /**< Pointer to data */
  unsigned size;            /**< Size of buffer */
};

typedef struct stun_buffer_s stun_buffer_t;

typedef struct {
  uint16_t msg_type;        /**< message type */
  uint16_t msg_len;         /**< message length */
  uint8_t tran_id[16];      /**< transaction id, 128 bits */
} stun_hdr_t;

typedef struct stun_attr_s {
  uint16_t attr_type;       /**< attribute type */
  void *pattr;              /**< pointer to corresponding attribute */
  stun_buffer_t enc_buf;    /**< encoded attribue */
  struct stun_attr_s *next; /**< next attribute */
} stun_attr_t;

typedef struct {
  stun_hdr_t stun_hdr;
  stun_attr_t *stun_attr;
  stun_buffer_t enc_buf;    /**< to store already encoded stun msg */
} stun_msg_t;

/* stun attribute definition */
/* stun_sockaddr_t is used for:
   MAPPED_ADDRESS
   RESPONSE_ADDRESS
   SOURCE_ADDRESS
   CHANGED_ADDRESS
   REFLECTED_FROM
*/
typedef struct sockaddr_in stun_attr_sockaddr_t;

/* CHANGE_REQUEST attribute */
typedef struct stun_attr_uint32_s {
  uint32_t value;
} stun_attr_uint32_t;

typedef stun_attr_uint32_t stun_attr_changerequest_t;

/* ERROR_CODE attribute */
typedef struct {
  int code;
  char *phrase;
} stun_attr_errorcode_t;

/* USERNAME attribute */
/* typedef struct {
  stun_buffer_t *uname;
} stun_attr_username_t;
*/
typedef stun_buffer_t stun_attr_username_t;

/* PASSWORD attribute */
typedef stun_buffer_t stun_attr_password_t;

/* UNKNOWN_ATTRIBUTES attribute */
typedef struct stun_attr_unknownattributes_s{
  uint16_t attr_type[2];
  struct stun_attr_unknownattributes_s *next;
} stun_attr_unknownattributes_t;

/* Common functions */
int stun_parse_message(stun_msg_t *msg);
int stun_parse_attribute(stun_msg_t *msg, unsigned char *p);
int stun_parse_attr_address(stun_attr_t *attr, const unsigned char *p, unsigned len);
int stun_parse_attr_error_code(stun_attr_t *attr, const unsigned char *p, unsigned len);
int stun_parse_attr_unknown_attributes(stun_attr_t *attr, const unsigned char *p, unsigned len);
int stun_parse_attr_uint32(stun_attr_t *attr, const unsigned char *p, unsigned len);
int stun_parse_attr_buffer(stun_attr_t *attr, const unsigned char *p, unsigned len);

stun_attr_t *stun_get_attr(stun_attr_t *attr, uint16_t attr_type);

int stun_encode_address(stun_attr_t *attr);
int stun_encode_uint32(stun_attr_t *attr);
int stun_encode_buffer(stun_attr_t *attr);
int stun_encode_error_code(stun_attr_t *attr);
int stun_encode_message_integrity(stun_attr_t *attr, unsigned char *buf, int len, stun_buffer_t *pwd);
int stun_encode_type_len(stun_attr_t *attr, uint16_t len);
int stun_encode_response_address(stun_attr_t *attr);

int stun_validate_message_integrity(stun_msg_t *msg, stun_buffer_t *pwd);

int stun_copy_buffer(stun_buffer_t *p, stun_buffer_t *p2);
void stun_init_buffer(stun_buffer_t *p);
int stun_free_buffer(stun_buffer_t *p);
int stun_free_message(stun_msg_t *msg);

int stun_init_message(stun_msg_t *msg);
/* int stun_send_message(int sockfd, struct sockaddr_in *to_addr, stun_msg_t *msg, stun_buffer_t *pwd); */
int stun_encode_message(stun_msg_t *msg, stun_buffer_t *pwd);

char const *stun_response_phrase(int status);
void debug_print(stun_buffer_t *buf);
char const *stun_attr_phrase(uint16_t type);

/**Determines and returns local IP address
 *
 * Address is determined using su_getlocalinfo() function.
 *
 * @param family        network address family in use
 * @return local ip address
 */
char *stun_determine_ip_address(int family);

SOFIA_END_DECLS

#endif /* !defined STUN_COMMON_H */

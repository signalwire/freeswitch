/* 
 * libDingaLing XMPP Jingle Library
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is libDingaLing XMPP Jingle Library
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 * stun_parser.h STUN packet manipulation
 *
 */
/*! \file stun_parser.h
    \brief STUN packet manipulation
*/

/*!
  \defgroup stun1 libDingaLing Stun Parser
  \ingroup LIBDINGALING
  \{
*/
#ifndef _STUN_PARSER_H
#define _STUN_PARSER_H

#define STUN_PACKET_MIN_LEN 20

typedef enum {
	STUN_BINDING_REQUEST					= 0x0001,
	STUN_BINDING_RESPONSE					= 0x0101,
	STUN_BINDING_ERROR_RESPONSE				= 0x0111,
	STUN_SHARED_SECRET_REQUEST				= 0x0002,
	STUN_SHARED_SECRET_RESPONSE				= 0x0102,
	STUN_SHARED_SECRET_ERROR_RESPONSE 		= 0x0112,
	STUN_ALLOCATE_REQUEST					= 0x0003,
	STUN_ALLOCATE_RESPONSE					= 0x0103,
	STUN_ALLOCATE_ERROR_RESPONSE			= 0x0113,
	STUN_SEND_REQUEST						= 0x0004,
	STUN_SEND_RESPONSE						= 0x0104,
	STUN_SEND_ERROR_RESPONSE				= 0x0114,
	STUN_DATA_INDICATION					= 0x0115
} stun_message_t;

typedef enum {
	STUN_ATTR_MAPPED_ADDRESS				= 0x0001, /* Address */
	STUN_ATTR_RESPONSE_ADDRESS				= 0x0002, /* Address */
	STUN_ATTR_CHANGE_REQUEST				= 0x0003, /* UInt32 */
	STUN_ATTR_SOURCE_ADDRESS				= 0x0004, /* Address */
	STUN_ATTR_CHANGED_ADDRESS				= 0x0005, /* Address */
	STUN_ATTR_USERNAME						= 0x0006, /* ByteString, multiple of 4 bytes */
	STUN_ATTR_PASSWORD						= 0x0007, /* ByteString, multiple of 4 bytes */
	STUN_ATTR_MESSAGE_INTEGRITY				= 0x0008, /* ByteString, 20 bytes */
	STUN_ATTR_ERROR_CODE					= 0x0009, /* ErrorCode */
	STUN_ATTR_UNKNOWN_ATTRIBUTES			= 0x000a, /* UInt16List */
	STUN_ATTR_REFLECTED_FROM				= 0x000b, /* Address */
	STUN_ATTR_TRANSPORT_PREFERENCES 		= 0x000c, /* TransportPrefs */
	STUN_ATTR_LIFETIME						= 0x000d, /* UInt32 */
	STUN_ATTR_ALTERNATE_SERVER				= 0x000e, /* Address */
	STUN_ATTR_MAGIC_COOKIE					= 0x000f, /* ByteString, 4 bytes */
	STUN_ATTR_BANDWIDTH						= 0x0010, /* UInt32 */
	STUN_ATTR_DESTINATION_ADDRESS			= 0x0011, /* Address */
	STUN_ATTR_SOURCE_ADDRESS2				= 0x0012, /* Address */
	STUN_ATTR_DATA							= 0x0013, /* ByteString */
	STUN_ATTR_OPTIONS						= 0x8001  /* UInt32 */
} stun_attribute_t;

typedef enum {
	STUN_ERROR_BAD_REQUEST					= 400,
	STUN_ERROR_UNAUTHORIZED					= 401,
	STUN_ERROR_UNKNOWN_ATTRIBUTE			= 420,
	STUN_ERROR_STALE_CREDENTIALS			= 430,
	STUN_ERROR_INTEGRITY_CHECK_FAILURE		= 431,
	STUN_ERROR_MISSING_USERNAME				= 432,
	STUN_ERROR_USE_TLS						= 433,
	STUN_ERROR_SERVER_ERROR					= 500,
	STUN_ERROR_GLOBAL_FAILURE				= 600
} stun_error_t;

typedef enum {
	STUN_TYPE_PACKET_TYPE,
	STUN_TYPE_ATTRIBUTE,
	STUN_TYPE_ERROR
} stun_type_t;

typedef struct {
	int16_t type;
	int16_t length;
	char id[16];
} stun_packet_header_t;

typedef struct {
	int16_t type;
	uint16_t length;
	char value[0];
} stun_packet_attribute_t;

typedef struct {
	stun_packet_header_t header;
	stun_packet_attribute_t first_attribute;
} stun_packet_t;

typedef struct {
	int8_t wasted;
	int8_t family;
	int16_t port;
	int32_t address;
} stun_ip_t;


/*!
  \brief Writes random characters into a buffer
  \param buf the buffer
  \param len the length of the data
  \param set the set of chars to use (NULL for auto)
*/
void stun_random_string(char *buf, uint16_t len, char *set);

/*!
  \brief Prepare a raw packet for parsing
  \param buf the raw data
  \param len the length of the data
  \return a stun packet pointer to buf to use as an access point
*/
stun_packet_t *stun_packet_parse(uint8_t *buf, uint32_t len);

/*!
  \brief Obtain a printable string form of a given value
  \param type the type of message
  \param value the value to look up
  \return a sring version of value
*/
const char *stun_value_to_name(int32_t type, int32_t value);


/*!
  \brief Extract a mapped address (IP:PORT) from a packet attribute
  \param attribute the attribute from which to extract
  \param ipstr a buffer to write the string representation of the ip
  \param port the port
  \return true or false
*/
uint8_t stun_packet_attribute_get_mapped_address(stun_packet_attribute_t *attribute, char *ipstr, uint16_t *port);

/*!
  \brief Extract a username from a packet attribute
  \param attribute the attribute from which to extract
  \param username a buffer to write the string representation of the username
  \param len the maximum size of the username buffer
  \return a pointer to the username or NULL
*/
char *stun_packet_attribute_get_username(stun_packet_attribute_t *attribute, char *username, uint16_t len);


/*!
  \brief Prepare a new outbound packet of a certian type and id
  \param id id to use (NULL for an auto generated id)
  \param type the stun packet type
  \param buf a pointer to data to use for the packet
  \return a pointer to a ready-to-use stun packet
*/
stun_packet_t *stun_packet_build_header(stun_message_t type,
										char *id,
										uint8_t *buf
										);

/*!
  \brief Add a username packet attribute
  \param packet the packet to add the attribute to
  \param username the string representation of the username
  \param ulen the length of the username
  \return true or false
*/
uint8_t stun_packet_attribute_add_username(stun_packet_t *packet, char *username, uint16_t ulen);


/*!
  \brief Add a binded address packet attribute
  \param packet the packet to add the attribute to
  \param ipstr the string representation of the ip
  \param port the port of the mapped address
  \return true or false
*/
uint8_t stun_packet_attribute_add_binded_address(stun_packet_t *packet, char *ipstr,  uint16_t port);

/*!
  \brief set a stun_packet_attribute_t pointer to point at the first attribute in a packet
  \param packet the packet in question
  \param attribute the pointer to set up
*/
#define stun_packet_first_attribute(packet, attribute) 	attribute = &packet->first_attribute;

/*!
  \brief Increment an attribute pointer to the next attribute in it's packet
  \param attribute the pointer to increment
  \return true or false depending on if there are any more attributes
*/
#define stun_packet_next_attribute(attribute) (attribute = (stun_packet_attribute_t *) (attribute->value + attribute->length)) && attribute->length

/*!
  \brief Obtain the correct length in bytes of a stun packet
  \param packet the packet in question
  \return the size in bytes (host order) of the entire packet
*/
#define stun_packet_length(packet) ntohs(packet->header.length) + sizeof(stun_packet_header_t)
///\}
#endif

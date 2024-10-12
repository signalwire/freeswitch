/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_stun.h STUN (Simple Traversal of UDP over NAT)
 *
 */
/*!
  \defgroup stun1 STUN code
  \ingroup core1
  \{
*/
#ifndef FREESWITCH_STUN_PARSER_H
#define FREESWITCH_STUN_PARSER_H

SWITCH_BEGIN_EXTERN_C
#define SWITCH_STUN_DEFAULT_PORT 3478
#define SWITCH_STUN_PACKET_MIN_LEN 20
#define SWITCH_STUN_ATTRIBUTE_MIN_LEN 8
	typedef enum {
	SWITCH_STUN_BINDING_REQUEST = 0x0001,
	SWITCH_STUN_BINDING_RESPONSE = 0x0101,
	SWITCH_STUN_BINDING_ERROR_RESPONSE = 0x0111,
	SWITCH_STUN_SHARED_SECRET_REQUEST = 0x0002,
	SWITCH_STUN_SHARED_SECRET_RESPONSE = 0x0102,
	SWITCH_STUN_SHARED_SECRET_ERROR_RESPONSE = 0x0112,
	SWITCH_STUN_ALLOCATE_REQUEST = 0x0003,
	SWITCH_STUN_ALLOCATE_RESPONSE = 0x0103,
	SWITCH_STUN_ALLOCATE_ERROR_RESPONSE = 0x0113,
	SWITCH_STUN_SEND_REQUEST = 0x0004,
	SWITCH_STUN_SEND_RESPONSE = 0x0104,
	SWITCH_STUN_SEND_ERROR_RESPONSE = 0x0114,
	SWITCH_STUN_DATA_INDICATION = 0x0115
} switch_stun_message_t;

#define STUN_MAGIC_COOKIE 0x2112A442

typedef enum {
	SWITCH_STUN_ATTR_MAPPED_ADDRESS = 0x0001,	/* Address */
	SWITCH_STUN_ATTR_RESPONSE_ADDRESS = 0x0002,	/* Address */
	SWITCH_STUN_ATTR_CHANGE_REQUEST = 0x0003,	/* UInt32 */
	SWITCH_STUN_ATTR_SOURCE_ADDRESS = 0x0004,	/* Address */
	SWITCH_STUN_ATTR_CHANGED_ADDRESS = 0x0005,	/* Address */
	SWITCH_STUN_ATTR_USERNAME = 0x0006,	/* ByteString, multiple of 4 bytes */
	SWITCH_STUN_ATTR_PASSWORD = 0x0007,	/* ByteString, multiple of 4 bytes */
	SWITCH_STUN_ATTR_MESSAGE_INTEGRITY = 0x0008,	/* ByteString, 20 bytes */
	SWITCH_STUN_ATTR_ERROR_CODE = 0x0009,	/* ErrorCode */
	SWITCH_STUN_ATTR_UNKNOWN_ATTRIBUTES = 0x000a,	/* UInt16List */
	SWITCH_STUN_ATTR_REFLECTED_FROM = 0x000b,	/* Address */
	SWITCH_STUN_ATTR_TRANSPORT_PREFERENCES = 0x000c,	/* TransportPrefs */
	SWITCH_STUN_ATTR_LIFETIME = 0x000d,	/* UInt32 */
	SWITCH_STUN_ATTR_ALTERNATE_SERVER = 0x000e,	/* Address */
	SWITCH_STUN_ATTR_MAGIC_COOKIE = 0x000f,	/* ByteString, 4 bytes */
	SWITCH_STUN_ATTR_BANDWIDTH = 0x0010,	/* UInt32 */
	SWITCH_STUN_ATTR_DESTINATION_ADDRESS = 0x0011,	/* Address */
	SWITCH_STUN_ATTR_SOURCE_ADDRESS2 = 0x0012,	/* Address */
	SWITCH_STUN_ATTR_DATA = 0x0013,	/* ByteString */
	SWITCH_STUN_ATTR_OPTIONS = 0x8001,	/* UInt32 */
	SWITCH_STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020,   /* Address */

	/* ice crap */

	SWITCH_STUN_ATTR_PRIORITY           = 0x0024,
	SWITCH_STUN_ATTR_USE_CAND           = 0x0025,
	SWITCH_STUN_ATTR_PADDING            = 0x0026,
	SWITCH_STUN_ATTR_RESP_PORT          = 0x0027,
	SWITCH_STUN_ATTR_SOFTWARE           = 0x8022,
	SWITCH_STUN_ATTR_ALT_SERVER         = 0x8023,
	SWITCH_STUN_ATTR_FINGERPRINT        = 0x8028,
	SWITCH_STUN_ATTR_CONTROLLED         = 0x8029,
	SWITCH_STUN_ATTR_CONTROLLING        = 0x802a,
	SWITCH_STUN_ATTR_RESP_ORIGIN        = 0x802b,
	SWITCH_STUN_ATTR_OTHER_ADDR         = 0x802c

} switch_stun_attribute_t;

typedef enum {
	SWITCH_STUN_ERROR_BAD_REQUEST = 400,
	SWITCH_STUN_ERROR_UNAUTHORIZED = 401,
	SWITCH_STUN_ERROR_UNKNOWN_ATTRIBUTE = 420,
	SWITCH_STUN_ERROR_STALE_CREDENTIALS = 430,
	SWITCH_STUN_ERROR_INTEGRITY_CHECK_FAILURE = 431,
	SWITCH_STUN_ERROR_MISSING_USERNAME = 432,
	SWITCH_STUN_ERROR_USE_TLS = 433,
	SWITCH_STUN_ERROR_SERVER_ERROR = 500,
	SWITCH_STUN_ERROR_GLOBAL_FAILURE = 600
} switch_stun_error_t;

typedef enum {
	SWITCH_STUN_TYPE_PACKET_TYPE,
	SWITCH_STUN_TYPE_ATTRIBUTE,
	SWITCH_STUN_TYPE_ERROR
} switch_stun_type_t;

typedef struct {
	uint16_t type;
	uint16_t length;
	uint32_t cookie;
	char id[12];
} switch_stun_packet_header_t;

typedef struct {
	uint16_t type;
	uint16_t length;
	char value[];
} switch_stun_packet_attribute_t;

typedef struct {
	switch_stun_packet_header_t header;
	uint8_t first_attribute[];
} switch_stun_packet_t;

typedef struct {
	uint8_t wasted;
	uint8_t family;
	uint16_t port;
	uint32_t address;
} switch_stun_ip_t;

typedef struct {
	uint8_t wasted;
	uint8_t family;
	uint16_t port;
	uint8_t address[16];
} switch_stun_ipv6_t;

#if SWITCH_BYTE_ORDER == __BIG_ENDIAN

typedef struct {
	unsigned padding:21;
	unsigned code:3;
	unsigned number:8;
	char reason[764];
} switch_stun_error_code_t;

#else

typedef struct {
	unsigned number:8;
	unsigned code:3;
	unsigned padding:21;
	char reason[764];
} switch_stun_error_code_t;

#endif


/*!
  \brief Writes random characters into a buffer
  \param buf the buffer
  \param len the length of the data
  \param set the set of chars to use (NULL for auto)
*/
SWITCH_DECLARE(void) switch_stun_random_string(char *buf, uint16_t len, char *set);

/*!
  \brief Prepare a raw packet for parsing
  \param buf the raw data
  \param len the length of the data
  \return a stun packet pointer to buf to use as an access point
*/
SWITCH_DECLARE(switch_stun_packet_t *) switch_stun_packet_parse(uint8_t *buf, uint32_t len);

/*!
  \brief Obtain a printable string form of a given value
  \param type the type of message
  \param value the value to look up
  \return a sring version of value
*/
SWITCH_DECLARE(const char *) switch_stun_value_to_name(int32_t type, uint32_t value);

SWITCH_DECLARE(char *) switch_stun_host_lookup(const char *host, switch_memory_pool_t *pool);

/*!
  \brief Extract a mapped address (IP:PORT) from a packet attribute
  \param attribute the attribute from which to extract
  \param ipstr a buffer to write the string representation of the ip
  \param port the port
  \return true or false
*/
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_get_mapped_address(switch_stun_packet_attribute_t *attribute, char *ipstr, switch_size_t iplen, uint16_t *port);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_get_xor_mapped_address(switch_stun_packet_attribute_t *attribute, switch_stun_packet_header_t *header, char *ipstr, switch_size_t iplen, uint16_t *port);


/*!
  \brief Extract a username from a packet attribute
  \param attribute the attribute from which to extract
  \param username a buffer to write the string representation of the username
  \param len the maximum size of the username buffer
  \return a pointer to the username or NULL
*/
SWITCH_DECLARE(char *) switch_stun_packet_attribute_get_username(switch_stun_packet_attribute_t *attribute, char *username, uint16_t len);


/*!
  \brief Prepare a new outbound packet of a certian type and id
  \param id id to use (NULL for an auto generated id)
  \param type the stun packet type
  \param buf a pointer to data to use for the packet
  \return a pointer to a ready-to-use stun packet
*/
SWITCH_DECLARE(switch_stun_packet_t *) switch_stun_packet_build_header(switch_stun_message_t type, char *id, uint8_t *buf);

/*!
  \brief Add a username packet attribute
  \param packet the packet to add the attribute to
  \param username the string representation of the username
  \param ulen the length of the username
  \return true or false
*/
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_username(switch_stun_packet_t *packet, char *username, uint16_t ulen);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_password(switch_stun_packet_t *packet, char *password, uint16_t ulen);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_software(switch_stun_packet_t *packet, char *software, uint16_t ulen);

/*!
  \brief Add a binded address packet attribute
  \param packet the packet to add the attribute to
  \param ipstr the string representation of the ip
  \param port the port of the mapped address
  \return true or false
*/
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_binded_address(switch_stun_packet_t *packet, char *ipstr, uint16_t port, int family);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_xor_binded_address(switch_stun_packet_t *packet, char *ipstr, uint16_t port, int family);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_integrity(switch_stun_packet_t *packet, const char *pass);
SWITCH_DECLARE(uint32_t) switch_crc32_8bytes(const void* data, size_t length);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_fingerprint(switch_stun_packet_t *packet);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_use_candidate(switch_stun_packet_t *packet);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_controlling(switch_stun_packet_t *packet);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_controlled(switch_stun_packet_t *packet);
SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_priority(switch_stun_packet_t *packet, uint32_t priority);

/*!
  \brief Perform a stun lookup
  \param ip the local ip to use (replaced with stun results)
  \param port the local port to use (replaced with stun results)
  \param stunip the ip of the stun server
  \param stunport the port of the stun server
  \param err a pointer to describe errors
  \param pool the memory pool to use
  \return SUCCESS or FAIL
*/
SWITCH_DECLARE(switch_status_t) switch_stun_lookup(char **ip,
												   switch_port_t *port, char *stunip, switch_port_t stunport, char **err, switch_memory_pool_t *pool);

/*!
  \brief Perform a stun ip lookup
  \param external_ip replaced with stun results
  \param sourceip stun:, host: or an ip
  \param external_pool the memory pool to use
  \return SUCCESS or FAIL
*/
SWITCH_DECLARE(switch_status_t) switch_stun_ip_lookup(char **external_ip, const char *sourceip, switch_memory_pool_t *external_pool);


/*!
  \brief Obtain the padded length of an attribute's value
  \param attribute the attribute
  \return the padded size in bytes
*/
#define switch_stun_attribute_padded_length(attribute) (int16_t)((attribute->length & 0x3) ? 0x4 + (attribute->length & ~0x3) : attribute->length)
#define switch_stun_attribute_padded_length_hbo(attribute) (int16_t)((ntohs(attribute->length) & 0x3) ? 0x4 + (ntohs(attribute->length) & ~0x3) : ntohs(attribute->length))


/*!
  \brief set a switch_stun_packet_attribute_t pointer to point at the first attribute in a packet
  \param packet the packet in question
  \param attribute the pointer to set up
*/
#define switch_stun_packet_first_attribute(packet, attribute) 	attribute = (switch_stun_packet_attribute_t *)(&packet->first_attribute);

/*!
  \brief Increment an attribute pointer to the next attribute in it's packet
  \param attribute the pointer to increment
  \param end pointer to the end of the buffer
  \return true or false depending on if there are any more attributes
*/

#define switch_stun_packet_next_attribute(attribute, end) (attribute && (attribute = (switch_stun_packet_attribute_t *) (attribute->value +  switch_stun_attribute_padded_length(attribute))) && ((void *)attribute < end) && attribute->type && (((switch_byte_t *)attribute +  switch_stun_attribute_padded_length(attribute)) < (switch_byte_t *)end))

#define switch_stun_packet_next_attribute_hbo(attribute, end) (attribute && (attribute = (switch_stun_packet_attribute_t *) (attribute->value +  switch_stun_attribute_padded_length_hbo(attribute))) && ((void *)attribute < end) && attribute->type && (((switch_byte_t *)attribute +  switch_stun_attribute_padded_length_hbo(attribute)) < (switch_byte_t *)end))

/*!
  \brief Obtain the correct length in bytes of a stun packet
  \param packet the packet in question
  \return the size in bytes (host order) of the entire packet
*/
#define switch_stun_packet_length(packet) ntohs(packet->header.length) + (sizeof(switch_stun_packet_header_t))
///\}

SWITCH_END_EXTERN_C
#endif
/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

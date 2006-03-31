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
 * stun_parser.c STUN packet manipulation
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef WIN32
#include <Winsock2.h>
#else
#include <stdint.h>
#include <netinet/in.h>
#endif

#include "ldl_compat.h"
#include "stun_parser.h"

struct value_mapping {
	const uint32_t value;
	const char *name;
};

static const struct value_mapping PACKET_TYPES[] = {
	{ STUN_BINDING_REQUEST, "BINDING_REQUEST" },
	{ STUN_BINDING_RESPONSE, "BINDING_RESPONSE" },
	{ STUN_BINDING_ERROR_RESPONSE, "BINDING_ERROR_RESPONSE" },
	{ STUN_SHARED_SECRET_REQUEST, "SHARED_SECRET_REQUEST" },
	{ STUN_SHARED_SECRET_RESPONSE, "SHARED_SECRET_RESPONSE" },
	{ STUN_SHARED_SECRET_ERROR_RESPONSE, "SHARED_SECRET_ERROR_RESPONSE" },
	{ STUN_ALLOCATE_REQUEST, "ALLOCATE_REQUEST" },
	{ STUN_ALLOCATE_RESPONSE, "ALLOCATE_RESPONSE" },
	{ STUN_ALLOCATE_ERROR_RESPONSE, "ALLOCATE_ERROR_RESPONSE" },
	{ STUN_SEND_REQUEST, "SEND_REQUEST" },
	{ STUN_SEND_RESPONSE, "SEND_RESPONSE" },
	{ STUN_SEND_ERROR_RESPONSE, "SEND_ERROR_RESPONSE" },
	{ STUN_DATA_INDICATION , "DATA_INDICATION"},
	{ 0, 0} };

static const struct value_mapping ATTR_TYPES[] = {
	{ STUN_ATTR_MAPPED_ADDRESS, "MAPPED_ADDRESS" },
	{ STUN_ATTR_RESPONSE_ADDRESS, "RESPONSE_ADDRESS" },
	{ STUN_ATTR_CHANGE_REQUEST, "CHANGE_REQUEST" },
	{ STUN_ATTR_SOURCE_ADDRESS, "SOURCE_ADDRESS" },
	{ STUN_ATTR_CHANGED_ADDRESS, "CHANGED_ADDRESS" },
	{ STUN_ATTR_USERNAME, "USERNAME" },
	{ STUN_ATTR_PASSWORD, "PASSWORD" },
	{ STUN_ATTR_MESSAGE_INTEGRITY, "MESSAGE_INTEGRITY" },
	{ STUN_ATTR_ERROR_CODE, "ERROR_CODE" },
	{ STUN_ATTR_UNKNOWN_ATTRIBUTES, "UNKNOWN_ATTRIBUTES" },
	{ STUN_ATTR_REFLECTED_FROM, "REFLECTED_FROM" },
	{ STUN_ATTR_TRANSPORT_PREFERENCES, "TRANSPORT_PREFERENCES" },
	{ STUN_ATTR_LIFETIME, "LIFETIME" },
	{ STUN_ATTR_ALTERNATE_SERVER, "ALTERNATE_SERVER" },
	{ STUN_ATTR_MAGIC_COOKIE, "MAGIC_COOKIE" },
	{ STUN_ATTR_BANDWIDTH, "BANDWIDTH" },
	{ STUN_ATTR_DESTINATION_ADDRESS, "DESTINATION_ADDRESS" },
	{ STUN_ATTR_SOURCE_ADDRESS2, "SOURCE_ADDRESS2" },
	{ STUN_ATTR_DATA, "DATA" },
	{ STUN_ATTR_OPTIONS, "OPTIONS" },
	{ 0, 0} };

static const struct value_mapping ERROR_TYPES[] = {
	{ STUN_ERROR_BAD_REQUEST, "BAD_REQUEST" },
	{ STUN_ERROR_UNAUTHORIZED, "UNAUTHORIZED" },
	{ STUN_ERROR_UNKNOWN_ATTRIBUTE, "UNKNOWN_ATTRIBUTE" },
	{ STUN_ERROR_STALE_CREDENTIALS, "STALE_CREDENTIALS" },
	{ STUN_ERROR_INTEGRITY_CHECK_FAILURE, "INTEGRITY_CHECK_FAILURE" },
	{ STUN_ERROR_MISSING_USERNAME, "MISSING_USERNAME" },
	{ STUN_ERROR_USE_TLS, "USE_TLS" },
	{ STUN_ERROR_SERVER_ERROR, "SERVER_ERROR" },
	{ STUN_ERROR_GLOBAL_FAILURE, "GLOBAL_FAILURE" }, 
	{ 0, 0 }};

void stun_random_string(char *buf, uint16_t len, char *set)
{
	char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int max;
	uint8_t x;

	if (!set) {
		set = chars;
	}

	max = (int)strlen(set) - 1;
	
	for(x = 0; x < len; x++) {
		int j = 1+(int)(max*1.0*rand()/(RAND_MAX+1.0));
		buf[x] = set[j];
	}
}



stun_packet_t *stun_packet_parse(uint8_t *buf, uint32_t len)
{
	stun_packet_t *packet;
	stun_packet_attribute_t *attr;

	if (len < STUN_PACKET_MIN_LEN) {
		return NULL;
	}

	packet = (stun_packet_t *) buf;
	packet->header.type = ntohs(packet->header.type);
	packet->header.length = ntohs(packet->header.length);
	attr = &packet->first_attribute;
	stun_packet_first_attribute(packet, attr);
	do {
		attr->length = ntohs(attr->length);
		attr->type = ntohs(attr->type);
		if (!attr->length) {
			break;
		}
		switch(attr->type) {
		case STUN_ATTR_MAPPED_ADDRESS:
			if (attr->type) {
				stun_ip_t *ip;
				ip = (stun_ip_t *) attr->value;
				ip->port = ntohs(ip->port);

			}
			break;
		}
	} while (stun_packet_next_attribute(attr));
	return packet;
}

const char *stun_value_to_name(int32_t type, int32_t value)

{
	uint32_t x = 0;
	const struct value_mapping *map = NULL;
	switch (type) {
	case STUN_TYPE_PACKET_TYPE:
		map = PACKET_TYPES;
		break;
	case STUN_TYPE_ATTRIBUTE:
		map = ATTR_TYPES;
		break;
	case STUN_TYPE_ERROR:
		map = ERROR_TYPES;
		break;
	default:
		map = NULL;
		break;
	}

	if (map) {
		for(x = 0; map[x].value; x++) {
			if (map[x].value == value) {
				return map[x].name;
			}
		}
	}
	
	return "INVALID";
}

uint8_t stun_packet_attribute_get_mapped_address(stun_packet_attribute_t *attribute, char *ipstr, uint16_t *port)
{
	stun_ip_t *ip;
	uint8_t x, *i;
	char *p = ipstr;
	
	ip = (stun_ip_t *) attribute->value;
	i = (uint8_t *) &ip->address;
	*ipstr = 0;
	for(x =0; x < 4; x++) {
		sprintf(p, "%u%s", i[x], x == 3 ? "" : ".");
		p = ipstr + strlen(ipstr);
	}
	*port = ip->port;
	return 1;
}

char *stun_packet_attribute_get_username(stun_packet_attribute_t *attribute, char *username, uint16_t len)
{
	uint16_t cpylen;

	cpylen = attribute->length > len ? attribute->length : len;
	return memcpy(username, attribute->value, cpylen);
}

stun_packet_t *stun_packet_build_header(stun_message_t type,
										char *id,
										uint8_t *buf
										)
{
	stun_packet_header_t *header;

	
	header = (stun_packet_header_t *) buf;
	header->type = htons(type);
	header->length = 0;
	
	if (id) {
		memcpy(header->id, id, 16);
	} else {
		stun_random_string(header->id, 16, NULL);
	}

	return (stun_packet_t *) buf;
}

uint8_t stun_packet_attribute_add_binded_address(stun_packet_t *packet, char *ipstr,  uint16_t port)
{
	stun_packet_attribute_t *attribute;
	stun_ip_t *ip;
	uint8_t *i, x;
	char *p = ipstr;

	attribute = (stun_packet_attribute_t *) ((uint8_t *) &packet->first_attribute + ntohs(packet->header.length));
	attribute->type = htons(STUN_ATTR_MAPPED_ADDRESS);
	attribute->length = htons(8);
	ip = (stun_ip_t *) attribute->value;

	ip->port = htons(port);
	ip->family = 1;
	i = (uint8_t *) &ip->address;

	for(x = 0; x < 4 ; x++) {
		i[x] = atoi(p);
		if ((p = strchr(p, '.'))) {
			p++;
		} else {
			break;
		}
	}

	packet->header.length += htons(sizeof(stun_packet_attribute_t)) + attribute->length;
	return 1;
}

uint8_t stun_packet_attribute_add_username(stun_packet_t *packet, char *username, uint16_t ulen)
{
	stun_packet_attribute_t *attribute;

	if (ulen % 4 != 0) {
		return 0;
	}
	attribute = (stun_packet_attribute_t *) ((uint8_t *) &packet->first_attribute + ntohs(packet->header.length));
	attribute->type = htons(STUN_ATTR_USERNAME);
	attribute->length = htons(ulen);
	if (username) {
		memcpy(attribute->value, username, ulen);
	} else {
		stun_random_string(attribute->value, ulen, NULL);
	} 

	packet->header.length += htons(sizeof(stun_packet_attribute_t)) + attribute->length;
	return 1;
}

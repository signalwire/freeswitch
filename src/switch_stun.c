/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Fanzhou Zhao <fanzhou@gmail.com> 2006-08-22 (Bugfix 2357-2358)
 *
 *
 * switch_stun.c STUN (Simple Traversal of UDP over NAT)
 *
 */

#include <switch.h>
#include <switch_stun.h>

struct value_mapping {
	const uint32_t value;
	const char *name;
};

static const struct value_mapping PACKET_TYPES[] = {
	{SWITCH_STUN_BINDING_REQUEST, "BINDING_REQUEST"},
	{SWITCH_STUN_BINDING_RESPONSE, "BINDING_RESPONSE"},
	{SWITCH_STUN_BINDING_ERROR_RESPONSE, "BINDING_ERROR_RESPONSE"},
	{SWITCH_STUN_SHARED_SECRET_REQUEST, "SHARED_SECRET_REQUEST"},
	{SWITCH_STUN_SHARED_SECRET_RESPONSE, "SHARED_SECRET_RESPONSE"},
	{SWITCH_STUN_SHARED_SECRET_ERROR_RESPONSE, "SHARED_SECRET_ERROR_RESPONSE"},
	{SWITCH_STUN_ALLOCATE_REQUEST, "ALLOCATE_REQUEST"},
	{SWITCH_STUN_ALLOCATE_RESPONSE, "ALLOCATE_RESPONSE"},
	{SWITCH_STUN_ALLOCATE_ERROR_RESPONSE, "ALLOCATE_ERROR_RESPONSE"},
	{SWITCH_STUN_SEND_REQUEST, "SEND_REQUEST"},
	{SWITCH_STUN_SEND_RESPONSE, "SEND_RESPONSE"},
	{SWITCH_STUN_SEND_ERROR_RESPONSE, "SEND_ERROR_RESPONSE"},
	{SWITCH_STUN_DATA_INDICATION, "DATA_INDICATION"},
	{0, 0}
};

static const struct value_mapping ATTR_TYPES[] = {
	{SWITCH_STUN_ATTR_MAPPED_ADDRESS, "MAPPED_ADDRESS"},
	{SWITCH_STUN_ATTR_RESPONSE_ADDRESS, "RESPONSE_ADDRESS"},
	{SWITCH_STUN_ATTR_CHANGE_REQUEST, "CHANGE_REQUEST"},
	{SWITCH_STUN_ATTR_SOURCE_ADDRESS, "SOURCE_ADDRESS"},
	{SWITCH_STUN_ATTR_CHANGED_ADDRESS, "CHANGED_ADDRESS"},
	{SWITCH_STUN_ATTR_USERNAME, "USERNAME"},
	{SWITCH_STUN_ATTR_PASSWORD, "PASSWORD"},
	{SWITCH_STUN_ATTR_MESSAGE_INTEGRITY, "MESSAGE_INTEGRITY"},
	{SWITCH_STUN_ATTR_ERROR_CODE, "ERROR_CODE"},
	{SWITCH_STUN_ATTR_UNKNOWN_ATTRIBUTES, "UNKNOWN_ATTRIBUTES"},
	{SWITCH_STUN_ATTR_REFLECTED_FROM, "REFLECTED_FROM"},
	{SWITCH_STUN_ATTR_TRANSPORT_PREFERENCES, "TRANSPORT_PREFERENCES"},
	{SWITCH_STUN_ATTR_LIFETIME, "LIFETIME"},
	{SWITCH_STUN_ATTR_ALTERNATE_SERVER, "ALTERNATE_SERVER"},
	{SWITCH_STUN_ATTR_MAGIC_COOKIE, "MAGIC_COOKIE"},
	{SWITCH_STUN_ATTR_BANDWIDTH, "BANDWIDTH"},
	{SWITCH_STUN_ATTR_DESTINATION_ADDRESS, "DESTINATION_ADDRESS"},
	{SWITCH_STUN_ATTR_SOURCE_ADDRESS2, "SOURCE_ADDRESS2"},
	{SWITCH_STUN_ATTR_DATA, "DATA"},
	{SWITCH_STUN_ATTR_OPTIONS, "OPTIONS"},
	{0, 0}
};

static const struct value_mapping ERROR_TYPES[] = {
	{SWITCH_STUN_ERROR_BAD_REQUEST, "BAD_REQUEST"},
	{SWITCH_STUN_ERROR_UNAUTHORIZED, "UNAUTHORIZED"},
	{SWITCH_STUN_ERROR_UNKNOWN_ATTRIBUTE, "UNKNOWN_ATTRIBUTE"},
	{SWITCH_STUN_ERROR_STALE_CREDENTIALS, "STALE_CREDENTIALS"},
	{SWITCH_STUN_ERROR_INTEGRITY_CHECK_FAILURE, "INTEGRITY_CHECK_FAILURE"},
	{SWITCH_STUN_ERROR_MISSING_USERNAME, "MISSING_USERNAME"},
	{SWITCH_STUN_ERROR_USE_TLS, "USE_TLS"},
	{SWITCH_STUN_ERROR_SERVER_ERROR, "SERVER_ERROR"},
	{SWITCH_STUN_ERROR_GLOBAL_FAILURE, "GLOBAL_FAILURE"},
	{0, 0}
};

SWITCH_DECLARE(void) switch_stun_random_string(char *buf, uint16_t len, char *set)
{
	char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	int max;
	uint16_t x;

	if (!set) {
		set = chars;
	}

	max = (int) strlen(set);

	srand((unsigned int) switch_micro_time_now());

	for (x = 0; x < len; x++) {
		int j = (int) (max * 1.0 * rand() / (RAND_MAX + 1.0));
		buf[x] = set[j];
	}
}


SWITCH_DECLARE(switch_stun_packet_t *) switch_stun_packet_parse(uint8_t *buf, uint32_t len)
{
	switch_stun_packet_t *packet;
	switch_stun_packet_attribute_t *attr;
	uint32_t bytes_left = len;
	void *end_buf = buf + len;

	if (len < SWITCH_STUN_PACKET_MIN_LEN) {
		return NULL;
	}

	packet = (switch_stun_packet_t *) buf;
	packet->header.type = ntohs(packet->header.type);
	packet->header.length = ntohs(packet->header.length);
	bytes_left -= packet->header.length + 20;

	/*
	 * Check packet type (RFC3489(bis?) values)
	 */
	switch (packet->header.type) {
	case SWITCH_STUN_BINDING_REQUEST:
	case SWITCH_STUN_BINDING_RESPONSE:
	case SWITCH_STUN_BINDING_ERROR_RESPONSE:
	case SWITCH_STUN_SHARED_SECRET_REQUEST:
	case SWITCH_STUN_SHARED_SECRET_RESPONSE:
	case SWITCH_STUN_SHARED_SECRET_ERROR_RESPONSE:
	case SWITCH_STUN_ALLOCATE_REQUEST:
	case SWITCH_STUN_ALLOCATE_RESPONSE:
	case SWITCH_STUN_ALLOCATE_ERROR_RESPONSE:
	case SWITCH_STUN_SEND_REQUEST:
	case SWITCH_STUN_SEND_RESPONSE:
	case SWITCH_STUN_SEND_ERROR_RESPONSE:
	case SWITCH_STUN_DATA_INDICATION:
		/* Valid */
		break;

	default:
		/* Invalid value */
		return NULL;
	}

	/*
	 * Check for length overflow
	 */
	if (bytes_left <= 0) {
		/* Invalid */
		return NULL;
	}

	/*
	 * No payload?
	 */
	if (packet->header.length == 0) {
		/* Invalid?! */
		return NULL;
	}

	/* check if we have enough bytes left for an attribute */
	if (bytes_left < SWITCH_STUN_ATTRIBUTE_MIN_LEN) {
		return NULL;
	}

	switch_stun_packet_first_attribute(packet, attr);
	do {
		attr->length = ntohs(attr->length);
		attr->type = ntohs(attr->type);
		bytes_left -= 4;		/* attribute header consumed */

		if (!attr->length || switch_stun_attribute_padded_length(attr) > bytes_left) {
			/*
			 * Note we simply don't "break" here out of the loop anymore because
			 * we don't want the upper layers to have to deal with attributes without a value
			 * (or worse: invalid length)
			 */
			return NULL;
		}

		/*
		 * Handle STUN attributes
		 */
		switch (attr->type) {
		case SWITCH_STUN_ATTR_MAPPED_ADDRESS:	/* Address, we only care about this one, but parse the others too */
		case SWITCH_STUN_ATTR_RESPONSE_ADDRESS:
		case SWITCH_STUN_ATTR_SOURCE_ADDRESS:
		case SWITCH_STUN_ATTR_CHANGED_ADDRESS:
		case SWITCH_STUN_ATTR_REFLECTED_FROM:
		case SWITCH_STUN_ATTR_ALTERNATE_SERVER:
		case SWITCH_STUN_ATTR_DESTINATION_ADDRESS:
		case SWITCH_STUN_ATTR_SOURCE_ADDRESS2:
			{
				switch_stun_ip_t *ip;
				uint32_t addr_length = 0;
				ip = (switch_stun_ip_t *) attr->value;

				switch (ip->family) {
				case 0x01:		/* IPv4 */
					addr_length = 4;
					break;

				case 0x02:		/* IPv6 */
					addr_length = 16;
					break;

				default:		/* Invalid */
					return NULL;
				}

				/* attribute payload length must be == address length + size of other payload fields (family...) */
				if (attr->length != addr_length + 4) {
					/* Invalid */
					return NULL;
				}

				ip->port = ntohs(ip->port);
			}
			break;

		case SWITCH_STUN_ATTR_CHANGE_REQUEST:	/* UInt32 */
		case SWITCH_STUN_ATTR_LIFETIME:
		case SWITCH_STUN_ATTR_BANDWIDTH:
		case SWITCH_STUN_ATTR_OPTIONS:
			{
				uint32_t *val = (uint32_t *) attr->value;

				if (attr->length != sizeof(uint32_t)) {
					/* Invalid */
					return NULL;
				}

				*val = ntohl(*val);	/* should we do this here? */
			}
			break;

		case SWITCH_STUN_ATTR_USERNAME:	/* ByteString, multiple of 4 bytes */
		case SWITCH_STUN_ATTR_PASSWORD:	/* ByteString, multiple of 4 bytes */
			if (attr->length % 4 != 0) {
				/* Invalid */
				return NULL;
			}
			break;

		case SWITCH_STUN_ATTR_DATA:	/* ByteString */
		case SWITCH_STUN_ATTR_ERROR_CODE:	/* ErrorCode */
		case SWITCH_STUN_ATTR_TRANSPORT_PREFERENCES:	/* TransportPrefs */
			/*
			 * No length checking here, since we already checked against the padded length
			 * before
			 */
			break;

		case SWITCH_STUN_ATTR_MESSAGE_INTEGRITY:	/* ByteString, 20 bytes */
			if (attr->length != 20) {
				/* Invalid */
				return NULL;
			}
			break;

		case SWITCH_STUN_ATTR_MAGIC_COOKIE:	/* ByteString, 4 bytes */
			if (attr->length != 4) {
				/* Invalid */
				return NULL;
			}
			break;

		case SWITCH_STUN_ATTR_UNKNOWN_ATTRIBUTES:	/* UInt16List (= multiple of 2 bytes) */
			if (attr->length % 2 != 0) {
				return NULL;
			}
			break;

		default:
			/* Mandatory attribute range? => invalid */
			if (attr->type <= 0x7FFF) {
				return NULL;
			}
			break;
		}
		bytes_left -= switch_stun_attribute_padded_length(attr);	/* attribute value consumed, substract padded length */

	} while (bytes_left >= SWITCH_STUN_ATTRIBUTE_MIN_LEN && switch_stun_packet_next_attribute(attr, end_buf));

	if ((uint32_t) (packet->header.length + 20) > (uint32_t) (len - bytes_left)) {
		/*
		 * the packet length is longer than the length of all attributes?
		 * for now simply decrease the packet size
		 */
		packet->header.length = (uint16_t) ((len - bytes_left) - 20);
	}

	return packet;
}

SWITCH_DECLARE(const char *) switch_stun_value_to_name(int32_t type, uint32_t value)
{
	uint32_t x = 0;
	const struct value_mapping *map = NULL;
	switch (type) {
	case SWITCH_STUN_TYPE_PACKET_TYPE:
		map = PACKET_TYPES;
		break;
	case SWITCH_STUN_TYPE_ATTRIBUTE:
		map = ATTR_TYPES;
		break;
	case SWITCH_STUN_TYPE_ERROR:
		map = ERROR_TYPES;
		break;
	default:
		map = NULL;
		break;
	}

	if (map) {
		for (x = 0; map[x].value; x++) {
			if (map[x].value == value) {
				return map[x].name;
			}
		}
	}

	return "INVALID";
}

SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_get_mapped_address(switch_stun_packet_attribute_t *attribute, char *ipstr, uint16_t *port)
{
	switch_stun_ip_t *ip;
	uint8_t x, *i;
	char *p = ipstr;

	ip = (switch_stun_ip_t *) attribute->value;
	i = (uint8_t *) & ip->address;
	*ipstr = 0;
	for (x = 0; x < 4; x++) {
		sprintf(p, "%u%s", i[x], x == 3 ? "" : ".");
		p = ipstr + strlen(ipstr);
	}
	*port = ip->port;
	return 1;
}

SWITCH_DECLARE(char *) switch_stun_packet_attribute_get_username(switch_stun_packet_attribute_t *attribute, char *username, uint16_t len)
{
	uint16_t cpylen;

	cpylen = attribute->length < len ? attribute->length : len;
	return memcpy(username, attribute->value, cpylen);
}

SWITCH_DECLARE(switch_stun_packet_t *) switch_stun_packet_build_header(switch_stun_message_t type, char *id, uint8_t *buf)
{
	switch_stun_packet_header_t *header;


	header = (switch_stun_packet_header_t *) buf;
	header->type = htons(type);
	header->length = 0;

	if (id) {
		memcpy(header->id, id, 16);
	} else {
		switch_stun_random_string(header->id, 16, NULL);
	}

	return (switch_stun_packet_t *) buf;
}

SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_binded_address(switch_stun_packet_t *packet, char *ipstr, uint16_t port)
{
	switch_stun_packet_attribute_t *attribute;
	switch_stun_ip_t *ip;
	uint8_t *i, x;
	char *p = ipstr;

	attribute = (switch_stun_packet_attribute_t *) ((uint8_t *) & packet->first_attribute + ntohs(packet->header.length));
	attribute->type = htons(SWITCH_STUN_ATTR_MAPPED_ADDRESS);
	attribute->length = htons(8);
	ip = (switch_stun_ip_t *) attribute->value;

	ip->port = htons(port);
	ip->family = 1;
	i = (uint8_t *) & ip->address;

	for (x = 0; x < 4; x++) {
		i[x] = (uint8_t) atoi(p);
		if ((p = strchr(p, '.'))) {
			p++;
		} else {
			break;
		}
	}

	packet->header.length += htons(sizeof(switch_stun_packet_attribute_t)) + attribute->length;
	return 1;
}

SWITCH_DECLARE(uint8_t) switch_stun_packet_attribute_add_username(switch_stun_packet_t *packet, char *username, uint16_t ulen)
{
	switch_stun_packet_attribute_t *attribute;

	if (ulen % 4 != 0) {
		return 0;
	}
	attribute = (switch_stun_packet_attribute_t *) ((uint8_t *) & packet->first_attribute + ntohs(packet->header.length));
	attribute->type = htons(SWITCH_STUN_ATTR_USERNAME);
	attribute->length = htons(ulen);
	if (username) {
		memcpy(attribute->value, username, ulen);
	} else {
		switch_stun_random_string(attribute->value, ulen, NULL);
	}

	packet->header.length += htons(sizeof(switch_stun_packet_attribute_t)) + attribute->length;
	return 1;
}

SWITCH_DECLARE(char *) switch_stun_host_lookup(const char *host, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *addr = NULL;
	char buf[30];

	switch_sockaddr_info_get(&addr, host, SWITCH_UNSPEC, 0, 0, pool);
	return switch_core_strdup(pool, switch_str_nil(switch_get_addr(buf, sizeof(buf), addr)));

}

SWITCH_DECLARE(switch_status_t) switch_stun_lookup(char **ip,
												   switch_port_t *port, char *stunip, switch_port_t stunport, char **err, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *local_addr = NULL, *remote_addr = NULL, *from_addr = NULL;
	switch_socket_t *sock = NULL;
	uint8_t buf[260] = { 0 };
	uint8_t *start = buf;
	void *end_buf;
	switch_stun_packet_t *packet;
	switch_stun_packet_attribute_t *attr;
	switch_size_t bytes = 0;
	char username[33] = { 0 };
	char rip[16] = { 0 };
	uint16_t rport = 0;
	switch_time_t started = 0;
	unsigned int elapsed = 0;
	int funny = 0;
	int size = sizeof(buf);

	switch_assert(err);

	if (*err && !strcmp(*err, "funny")) {
		funny = 1;
	}

	*err = "Success";

	switch_sockaddr_info_get(&from_addr, NULL, SWITCH_UNSPEC, 0, 0, pool);

	if (switch_sockaddr_info_get(&local_addr, *ip, SWITCH_UNSPEC, *port, 0, pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Local Address Error!";
		return SWITCH_STATUS_FALSE;
	}

	if (switch_sockaddr_info_get(&remote_addr, stunip, SWITCH_UNSPEC, stunport, 0, pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Remote Address Error!";
		return SWITCH_STATUS_FALSE;
	}

	if (switch_socket_create(&sock, AF_INET, SOCK_DGRAM, 0, pool) != SWITCH_STATUS_SUCCESS) {
		*err = "Socket Error!";
		return SWITCH_STATUS_FALSE;
	}

	if (switch_socket_bind(sock, local_addr) != SWITCH_STATUS_SUCCESS) {
		*err = "Bind Error!";
		return SWITCH_STATUS_FALSE;
	}

	if (funny) {
		*start++ = 0;
		*start++ = 0;
		*start++ = 0x22;
		*start++ = 0x22;
	}

	switch_socket_opt_set(sock, SWITCH_SO_NONBLOCK, TRUE);
	packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, start);
	switch_stun_random_string(username, 32, NULL);
	switch_stun_packet_attribute_add_username(packet, username, 32);
	bytes = switch_stun_packet_length(packet);

	if (funny) {
		packet = (switch_stun_packet_t *) buf;
		bytes += 4;
		buf[bytes++] = 0;
		buf[bytes++] = 0;
		buf[bytes++] = 0;
		buf[bytes++] = 0;
	}

	switch_socket_sendto(sock, remote_addr, 0, (void *) packet, &bytes);
	started = switch_micro_time_now();

	*ip = NULL;
	*port = 0;


	for (;;) {
		bytes = sizeof(buf);
		if (switch_socket_recvfrom(from_addr, sock, 0, (char *) &buf, &bytes) == SWITCH_STATUS_SUCCESS && bytes > 0) {
			break;
		}

		if ((elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000)) > 5000) {
			*err = "Timeout";
			switch_socket_shutdown(sock, SWITCH_SHUTDOWN_READWRITE);
			switch_socket_close(sock);
			return SWITCH_STATUS_TIMEOUT;
		}
		switch_cond_next();
	}
	switch_socket_close(sock);

	if (funny) {
		size -= 4;
	}

	packet = switch_stun_packet_parse(start, size);
	if (!packet) {
		*err = "Invalid STUN/ICE packet";
		return SWITCH_STATUS_FALSE;
	}
	end_buf = buf + ((sizeof(buf) > packet->header.length) ? packet->header.length : sizeof(buf));

	switch_stun_packet_first_attribute(packet, attr);
	do {
		switch (attr->type) {
		case SWITCH_STUN_ATTR_MAPPED_ADDRESS:
			if (attr->type) {
				if (funny) {
					switch_stun_ip_t *tmp = (switch_stun_ip_t *) attr->value;
					tmp->address ^= ntohl(0xabcdabcd);
				}
				switch_stun_packet_attribute_get_mapped_address(attr, rip, &rport);
			}
			break;
		case SWITCH_STUN_ATTR_USERNAME:
			if (attr->type) {
				switch_stun_packet_attribute_get_username(attr, username, 32);
			}
			break;
		}
	} while (switch_stun_packet_next_attribute(attr, end_buf));

	if (packet->header.type == SWITCH_STUN_BINDING_RESPONSE) {
		*ip = switch_core_strdup(pool, rip);
		*port = rport;
		return SWITCH_STATUS_SUCCESS;
	} else {
		*err = "Invalid Reply";
	}

	return SWITCH_STATUS_FALSE;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2026, Anthony Minessale II <anthm@freeswitch.org>
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
* Dmitry Verenitsin <morbit85@gmail.com>
*
* switch_stun.c -- tests STUN (https://www.rfc-editor.org/rfc/rfc5389).
*/


#include <switch.h>
#include <switch_stun.h>
#include <test/switch_test.h>

FST_CORE_BEGIN("./conf_stun")
{
FST_SUITE_BEGIN(switch_stun)
{
FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

	FST_TEST_BEGIN(test_stun_add_binded_address_ipv6)
	{
		/*
		 * Encode an IPv6 XOR-MAPPED-ADDRESS attribute and verify the
		 * attribute type, length, address family, and the raw 16-byte
		 * address payload at its expected offset inside the value.
		 */
		uint8_t buf[512];
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *attr;
		const char *ipv6_str = "2001:db8::dead:beef";
		uint8_t expected[16];
		uint8_t *value_bytes;

		memset(buf, 0, sizeof(buf));
		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, NULL, buf);
		fst_xcheck(inet_pton(AF_INET6, ipv6_str, expected) == 1, "test IPv6 literal parses");

		switch_stun_packet_attribute_add_binded_address(packet, (char *)ipv6_str, 12345, AF_INET6);

		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		fst_xcheck(ntohs(attr->type) == SWITCH_STUN_ATTR_XOR_MAPPED_ADDRESS, "attribute type is XOR_MAPPED_ADDRESS");
		fst_xcheck(ntohs(attr->length) == 20, "attribute length is 20 for IPv6");

		/* Attribute value layout: wasted(1) + family(1) + port(2) + address(16). */
		value_bytes = (uint8_t *)attr->value;
		fst_xcheck(value_bytes[1] == 2, "attribute family byte is 2 for IPv6");
		fst_xcheck(memcmp(value_bytes + 4, expected, 16) == 0, "16-byte IPv6 address written at offset 4 of attribute value");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_add_xor_binded_address_ipv6)
	{
		/*
		 * Encode then decode an IPv6 XOR-MAPPED-ADDRESS attribute and
		 * confirm the round-trip recovers the original IPv6 string —
		 * the write path must XOR the address with the transaction ID
		 * symmetrically to the read path.
		 */
		uint8_t buf[512];
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *attr;
		const char *ipv6_str = "2001:db8::dead:beef";
		char out_ip[64] = { 0 };
		uint16_t out_port = 0;

		memset(buf, 0, sizeof(buf));
		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, NULL, buf);

		switch_stun_packet_attribute_add_xor_binded_address(packet, (char *)ipv6_str, 12345, AF_INET6);

		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		fst_xcheck(ntohs(attr->type) == SWITCH_STUN_ATTR_XOR_MAPPED_ADDRESS, "attribute type is XOR_MAPPED_ADDRESS");
		fst_xcheck(ntohs(attr->length) == 20, "attribute length is 20 for IPv6");

		switch_stun_packet_attribute_get_xor_mapped_address(attr, &packet->header, out_ip, sizeof(out_ip), &out_port);
		fst_check_string_equals(out_ip, ipv6_str);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_add_binded_address_ipv4)
	{
		/*
		 * Encode an IPv4 XOR-MAPPED-ADDRESS attribute and verify the
		 * attribute type, length, address family, and the raw 4-byte
		 * address payload at its expected offset inside the value.
		 */
		uint8_t buf[512];
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *attr;
		const char *ipv4_str = "192.0.2.42";
		uint8_t expected[4];
		uint8_t *value_bytes;

		memset(buf, 0, sizeof(buf));
		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, NULL, buf);
		fst_xcheck(inet_pton(AF_INET, ipv4_str, expected) == 1, "test IPv4 literal parses");

		switch_stun_packet_attribute_add_binded_address(packet, (char *)ipv4_str, 12345, AF_INET);

		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		fst_xcheck(ntohs(attr->type) == SWITCH_STUN_ATTR_XOR_MAPPED_ADDRESS, "attribute type is XOR_MAPPED_ADDRESS");
		fst_xcheck(ntohs(attr->length) == 8, "attribute length is 8 for IPv4");

		/* Attribute value layout: wasted(1) + family(1) + port(2) + address(4). */
		value_bytes = (uint8_t *)attr->value;
		fst_xcheck(value_bytes[1] == 1, "attribute family byte is 1 for IPv4");
		fst_xcheck(memcmp(value_bytes + 4, expected, 4) == 0, "4-byte IPv4 address written at offset 4 of attribute value");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_add_xor_binded_address_ipv4)
	{
		/*
		 * Encode then decode an IPv4 XOR-MAPPED-ADDRESS attribute and
		 * confirm the round-trip recovers the original IPv4 string —
		 * the write path must XOR the address with the magic cookie
		 * symmetrically to the read path.
		 */
		uint8_t buf[512];
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *attr;
		const char *ipv4_str = "192.0.2.42";
		char out_ip[64] = { 0 };
		uint16_t out_port = 0;

		memset(buf, 0, sizeof(buf));
		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, NULL, buf);

		switch_stun_packet_attribute_add_xor_binded_address(packet, (char *)ipv4_str, 12345, AF_INET);

		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		fst_xcheck(ntohs(attr->type) == SWITCH_STUN_ATTR_XOR_MAPPED_ADDRESS, "attribute type is XOR_MAPPED_ADDRESS");
		fst_xcheck(ntohs(attr->length) == 8, "attribute length is 8 for IPv4");

		switch_stun_packet_attribute_get_xor_mapped_address(attr, &packet->header, out_ip, sizeof(out_ip), &out_port);
		fst_check_string_equals(out_ip, ipv4_str);
	}
	FST_TEST_END()
}
FST_SUITE_END()
}
FST_CORE_END()


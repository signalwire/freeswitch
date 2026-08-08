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

	FST_TEST_BEGIN(test_stun_verify_integrity_accepts_valid_hmac)
	{
		/* A packet signed with add_integrity under a given key must verify against that same key,
		   including when a leading attribute precedes MESSAGE-INTEGRITY (it is part of the HMAC input). */
		uint8_t buf[512] = { 0 };
		char software[] = "sw";
		switch_stun_packet_t *packet;
		uint32_t len;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
		switch_stun_packet_attribute_add_software(packet, software, (uint16_t)strlen(software));
		switch_stun_packet_attribute_add_integrity(packet, "secret");
		len = (uint32_t)switch_stun_packet_length(packet);

		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "secret") == SWITCH_STATUS_SUCCESS,
				   "valid MESSAGE-INTEGRITY verifies against the signing key");
		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "wrong") == SWITCH_STATUS_FALSE,
				   "MESSAGE-INTEGRITY does not verify against a different key");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_verify_integrity_rejects_zeroed_hmac)
	{
		/* An all-zero MESSAGE-INTEGRITY value must not verify: this is the shape a sender produces when
		   it fills the field with zeros instead of computing the HMAC. MI is the first attribute, so its
		   20-byte value sits at offset 24 (20-byte header plus 4-byte attribute header). */
		uint8_t buf[512] = { 0 };
		switch_stun_packet_t *packet;
		uint32_t len;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
		switch_stun_packet_attribute_add_integrity(packet, "secret");
		len = (uint32_t)switch_stun_packet_length(packet);
		memset(buf + SWITCH_STUN_PACKET_MIN_LEN + 4, 0, 20);

		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "secret") == SWITCH_STATUS_FALSE,
				   "a zeroed MESSAGE-INTEGRITY value is rejected");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_verify_integrity_trailing_fingerprint)
	{
		/* MESSAGE-INTEGRITY followed by FINGERPRINT (the layout our own responses use) must still verify:
		   the HMAC input's length field reads as if the message ended right after MESSAGE-INTEGRITY, so the
		   trailing FINGERPRINT is excluded from the computation. */
		uint8_t buf[512] = { 0 };
		switch_stun_packet_t *packet;
		uint32_t len;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, NULL, buf);
		switch_stun_packet_attribute_add_integrity(packet, "secret");
		switch_stun_packet_attribute_add_fingerprint(packet);
		len = (uint32_t)switch_stun_packet_length(packet);

		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "secret") == SWITCH_STATUS_SUCCESS,
				   "MESSAGE-INTEGRITY verifies with a trailing FINGERPRINT present");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_verify_integrity_absent)
	{
		/* A packet with no MESSAGE-INTEGRITY attribute reports NOTFOUND, distinct from a mismatch, so the
		   caller can apply its own present-or-absent policy. */
		uint8_t buf[512] = { 0 };
		char software[] = "sw";
		switch_stun_packet_t *packet;
		uint32_t len;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
		switch_stun_packet_attribute_add_software(packet, software, (uint16_t)strlen(software));
		len = (uint32_t)switch_stun_packet_length(packet);

		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "secret") == SWITCH_STATUS_NOTFOUND,
				   "a packet with no MESSAGE-INTEGRITY reports NOTFOUND");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_verify_integrity_rejects_attr_after_mi)
	{
		/* Only MESSAGE-INTEGRITY-SHA256 and FINGERPRINT may follow MESSAGE-INTEGRITY. A USE-CANDIDATE
		   appended after MI leaves the HMAC prefix - and therefore the signature - valid, so it must be
		   rejected outright rather than verified and then acted on. */
		uint8_t buf[512] = { 0 };
		switch_stun_packet_t *packet;
		uint32_t len;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
		switch_stun_packet_attribute_add_integrity(packet, "secret");
		switch_stun_packet_attribute_add_use_candidate(packet);	/* lands after MESSAGE-INTEGRITY */
		len = (uint32_t)switch_stun_packet_length(packet);

		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "secret") == SWITCH_STATUS_FALSE,
				   "a non-FINGERPRINT attribute after MESSAGE-INTEGRITY is rejected");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_verify_integrity_oversized_attr_length)
	{
		/* An attribute length with the high bit set must not walk the cursor backward or read out of
		   bounds: the walk treats the padded length as unsigned and stops once it exceeds the bytes that
		   remain. With a leading oversized attribute, MESSAGE-INTEGRITY is never reached (NOTFOUND) and no
		   out-of-bounds access occurs (ASAN would catch a regression here). */
		uint8_t buf[64] = { 0 };
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *attr;
		uint32_t len;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = htons(0x8000);
		packet->header.length = htons(4);	/* declare just the 4-byte attribute header */
		len = SWITCH_STUN_PACKET_MIN_LEN + 4;

		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "secret") == SWITCH_STATUS_NOTFOUND,
				   "an attribute length with the high bit set stops the walk without an out-of-bounds read");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_verify_integrity_allows_sha256_after_mi)
	{
		/* RFC 8489 permits MESSAGE-INTEGRITY-SHA256 to follow MESSAGE-INTEGRITY. It is inert (carries no
		   ICE state) and outside the SHA-1 HMAC's coverage, so a dual-hash sender's packet must still
		   verify on its SHA-1 MESSAGE-INTEGRITY rather than be rejected as a disallowed trailing attribute. */
		uint8_t buf[512] = { 0 };
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *sha256;
		uint32_t off;
		uint32_t len;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_REQUEST, NULL, buf);
		switch_stun_packet_attribute_add_integrity(packet, "secret");

		/* Append a MESSAGE-INTEGRITY-SHA256 attribute (32-byte value, left zero: the SHA-1 verifier does
		   not inspect it) immediately after MESSAGE-INTEGRITY. */
		off = SWITCH_STUN_PACKET_MIN_LEN + ntohs(packet->header.length);
		sha256 = (switch_stun_packet_attribute_t *)(buf + off);
		sha256->type = htons(SWITCH_STUN_ATTR_MESSAGE_INTEGRITY_SHA256);
		sha256->length = htons(32);
		packet->header.length = htons((uint16_t)(ntohs(packet->header.length) + 4 + 32));
		len = (uint32_t)switch_stun_packet_length(packet);

		fst_xcheck(switch_stun_packet_verify_integrity(buf, len, "secret") == SWITCH_STATUS_SUCCESS,
				   "a MESSAGE-INTEGRITY-SHA256 attribute after MESSAGE-INTEGRITY is tolerated");
	}
	FST_TEST_END()
}
FST_SUITE_END()
}
FST_CORE_END()


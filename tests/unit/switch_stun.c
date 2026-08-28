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

	FST_TEST_BEGIN(test_stun_get_xor_mapped_address_short_ipv6)
	{
		/*
		 * An XOR-MAPPED-ADDRESS attribute whose family byte claims IPv6 (2)
		 * but whose value is only the 8-byte IPv4 size must be rejected:
		 * switch_stun_packet_attribute_get_xor_mapped_address must not read
		 * or XOR a 20-byte IPv6 address out of the 8-byte value. The packet
		 * is routed through switch_stun_packet_parse first so the attribute
		 * length is in host byte order, as the live callers see it.
		 */
		uint8_t buf[512] = { 0 };
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *attr;
		char out_ip[64];
		uint16_t out_port = 0xffff;
		uint8_t ret;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, NULL, buf);

		/* Value layout: wasted(1) + family(1) + port(2) + address(4) = 8 bytes. */
		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		attr->type = htons(SWITCH_STUN_ATTR_XOR_MAPPED_ADDRESS);
		attr->length = htons(8);
		attr->value[0] = 0;	/* wasted */
		attr->value[1] = 2;	/* family byte claims IPv6 */
		attr->value[2] = 0;
		attr->value[3] = 0;
		attr->value[4] = 0x01;
		attr->value[5] = 0x02;
		attr->value[6] = 0x03;
		attr->value[7] = 0x04;
		packet->header.length = htons(4 + 8);

		packet = switch_stun_packet_parse(buf, 20 + 4 + 8);
		fst_requires(packet != NULL);

		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		fst_xcheck(attr->length == 8, "parse accepts the 8-byte family-2 XOR-MAPPED-ADDRESS attribute");

		memset(out_ip, 'x', sizeof(out_ip));
		ret = switch_stun_packet_attribute_get_xor_mapped_address(attr, &packet->header, out_ip, sizeof(out_ip), &out_port);
		fst_xcheck(ret == 0, "short IPv6 XOR-MAPPED-ADDRESS attribute is rejected");
		fst_xcheck(out_ip[0] == '\0', "output address string left empty on reject");
		fst_xcheck(out_port == 0, "output port left defined on reject");
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

	FST_TEST_BEGIN(test_stun_get_username_terminates)
	{
		/* get_username must always NUL-terminate within the caller's buffer, even when the
		   attribute value is as long as or longer than the buffer: callers use the result as a C string. */
		uint8_t abuf[128] = { 0 };
		switch_stun_packet_attribute_t *attr = (switch_stun_packet_attribute_t *)abuf;
		char dst[32];
		char *ret;
		int i;

		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = 64;	/* host order: the accessor reads attribute->length directly, as post-parse callers do */
		for (i = 0; i < 64; i++) {
			attr->value[i] = 'A';
		}

		memset(dst, 'x', sizeof(dst));
		ret = switch_stun_packet_attribute_get_username(attr, dst, sizeof(dst));
		fst_xcheck(ret == dst, "get_username returns the destination buffer");
		fst_xcheck(dst[sizeof(dst) - 1] == '\0', "over-long USERNAME is NUL-terminated at the last byte");
		fst_xcheck(strlen(dst) == sizeof(dst) - 1, "over-long USERNAME is truncated to len-1");

		attr->length = 5;
		memcpy(attr->value, "abcde", 5);
		memset(dst, 'x', sizeof(dst));
		switch_stun_packet_attribute_get_username(attr, dst, sizeof(dst));
		fst_xcheck(strlen(dst) == 5, "short USERNAME is copied and terminated at its own length");
		fst_check_string_equals(dst, "abcde");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_next_attribute_walks_whole_attributes)
	{
		/* Positive test: the iterator visits each whole attribute and stops exactly at the end of the
		   last one, including an attribute whose value ends on the buffer boundary. Guards against a
		   future change breaking normal iteration; it does not distinguish the header/TLV bounds fix
		   (a naive iterator passes it too). */
		uint8_t buf[8] = { 0 };
		switch_stun_packet_attribute_t *attr;
		void *end = buf + sizeof(buf);

		attr = (switch_stun_packet_attribute_t *)buf;
		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = 0;
		attr = (switch_stun_packet_attribute_t *)(buf + 4);
		attr->type = htons(SWITCH_STUN_ATTR_PRIORITY);
		attr->length = 0;

		attr = (switch_stun_packet_attribute_t *)buf;
		fst_xcheck(switch_stun_packet_next_attribute(attr, end) != 0, "iterator advances to the second whole attribute");
		fst_xcheck((uint8_t *)attr == buf + 4, "iterator lands exactly on the second attribute");
		fst_xcheck(switch_stun_packet_next_attribute(attr, end) == 0, "iterator stops after the last whole attribute");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_next_attribute_truncated_header)
	{
		/* After the first attribute only 2 bytes remain before end, so the next attribute's 4-byte
		   header does not fit. The iterator must confirm the header is fully in-bounds before reading
		   it and stop; reading attribute->length here would run past the buffer. The trailing bytes are
		   non-zero so the type sentinel does not stop the walk first, so the out-of-bounds read (if the
		   header check is missing) is exercised and caught under ASAN. */
		uint8_t buf[6] = { 0 };
		switch_stun_packet_attribute_t *attr = (switch_stun_packet_attribute_t *)buf;
		void *end = buf + sizeof(buf);

		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = 0;
		buf[4] = 0xff;	/* non-zero type for the truncated trailing header */
		buf[5] = 0xff;

		fst_xcheck(switch_stun_packet_next_attribute(attr, end) == 0, "iterator stops at a truncated trailing attribute header without reading past end");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_next_attribute_hbo)
	{
		/* Positive test for the _hbo iterator, which reads attribute lengths in network byte order
		   (ntohs) for callers walking raw on-the-wire packets that have not been through
		   switch_stun_packet_parse, since parse byte-swaps type and length in place: it must advance by
		   the network-order length and stop at the end. The plain macro would misread these
		   network-order lengths; this confirms the _hbo variant walks normal attributes, not a bounds
		   regression. */
		uint8_t buf[16] = { 0 };
		switch_stun_packet_attribute_t *attr;
		void *end = buf + sizeof(buf);

		attr = (switch_stun_packet_attribute_t *)buf;
		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = htons(4);	/* network order: _hbo applies ntohs, the plain macro would misread this */
		attr = (switch_stun_packet_attribute_t *)(buf + 8);
		attr->type = htons(SWITCH_STUN_ATTR_PRIORITY);
		attr->length = htons(4);

		attr = (switch_stun_packet_attribute_t *)buf;
		fst_xcheck(switch_stun_packet_next_attribute_hbo(attr, end) != 0, "hbo iterator advances to the second attribute");
		fst_xcheck((uint8_t *)attr == buf + 8, "hbo iterator lands on the second attribute using the network-order length");
		fst_xcheck(switch_stun_packet_next_attribute_hbo(attr, end) == 0, "hbo iterator stops after the last attribute");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_next_attribute_value_overruns)
	{
		/* An attribute whose 4-byte header fits before end but whose declared value extends past end
		   (more value bytes than remain) must be rejected, so the caller never reads the value out of
		   bounds. This exercises the value-length bound distinctly from a truncated header. */
		uint8_t buf[12] = { 0 };
		switch_stun_packet_attribute_t *attr;
		void *end = buf + 10;	/* ends inside the second attribute's declared value */

		attr = (switch_stun_packet_attribute_t *)buf;
		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = 0;	/* host order: empty leading attribute */
		attr = (switch_stun_packet_attribute_t *)(buf + 4);
		attr->type = htons(SWITCH_STUN_ATTR_PRIORITY);
		attr->length = 4;	/* value would occupy buf[8..11], past end (buf+10) */

		attr = (switch_stun_packet_attribute_t *)buf;
		fst_xcheck(switch_stun_packet_next_attribute(attr, end) == 0, "attribute whose value overruns end is rejected");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_stun_walk_reaches_trailing_attribute)
	{
		/* Positive test: parse + walk over a well-formed multi-attribute packet reaches every attribute,
		   including a trailing one whose value ends on the last byte of the message. end_buf is computed
		   here rather than taken from switch_stun_lookup or handle_ice, so how those callers derive it is
		   not covered. */
		uint8_t buf[512] = { 0 };
		switch_stun_packet_t *packet;
		switch_stun_packet_attribute_t *attr;
		void *end_buf;
		int count;

		packet = switch_stun_packet_build_header(SWITCH_STUN_BINDING_RESPONSE, NULL, buf);

		attr = (switch_stun_packet_attribute_t *)packet->first_attribute;
		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = htons(4);
		memcpy(attr->value, "abcd", 4);

		attr = (switch_stun_packet_attribute_t *)(packet->first_attribute + 8);
		attr->type = htons(SWITCH_STUN_ATTR_USERNAME);
		attr->length = htons(4);
		memcpy(attr->value, "efgh", 4);

		packet->header.length = htons(8 + 8);

		packet = switch_stun_packet_parse(buf, SWITCH_STUN_PACKET_MIN_LEN + 8 + 8);
		fst_requires(packet != NULL);

		/* Same end_buf the iterator's callers use: the 20-byte header plus the attribute section. */
		end_buf = buf + SWITCH_STUN_PACKET_MIN_LEN + packet->header.length;

		switch_stun_packet_first_attribute(packet, attr);
		count = 1;
		while (switch_stun_packet_next_attribute(attr, end_buf)) {
			count++;
		}
		fst_xcheck(count == 2, "iterator reaches the trailing attribute in the last bytes of the message");
	}
	FST_TEST_END()
}
FST_SUITE_END()
}
FST_CORE_END()


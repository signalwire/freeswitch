/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2018, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <seven@signalwire.com>
 * Windy Wang <xiaofengcanyuexp@163.com>
 *
 * switch_utils.c -- tests switch_utils
 *
 */

#include <switch.h>
#include <test/switch_test.h>

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(switch_hash)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(benchmark)
{
    char encoded[1024];
    char *s = "ABCD";

    switch_url_encode(s, encoded, sizeof(encoded));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "encoded: [%s]\n", encoded);
    fst_check_string_equals(encoded, "ABCD");

    s = "&bryän#!杜金房";
    switch_url_encode(s, encoded, sizeof(encoded));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "encoded: [%s]\n", encoded);
    fst_check_string_equals(encoded, "%26bry%C3%A4n%23!%E6%9D%9C%E9%87%91%E6%88%BF");
}
FST_TEST_END()

FST_TEST_BEGIN(b64)
{
    switch_size_t size;
    char *str = "ABC";
    unsigned char b64_str[6];
    char decoded_str[4];
    switch_status_t status = switch_b64_encode((unsigned char *)str, strlen(str), b64_str, sizeof(b64_str));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "b64_str: %s\n", b64_str);
    fst_check(status == SWITCH_STATUS_SUCCESS);
    fst_check_string_equals((const char *)b64_str, "QUJD");

    size = switch_b64_decode((const char *)b64_str, decoded_str, sizeof(decoded_str));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "decoded_str: %s\n", decoded_str);
    fst_check_string_equals(decoded_str, str);
    fst_check(size == 4);
}
FST_TEST_END()

FST_TEST_BEGIN(b64_pad2)
{
    switch_size_t size;
    char str[] = {0, 0, 0, 0};
    unsigned char b64_str[128];
    char decoded_str[128];
	int i;
    switch_status_t status = switch_b64_encode((unsigned char *)str, sizeof(str), b64_str, sizeof(b64_str));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "b64_str: %s\n", b64_str);
    fst_check(status == SWITCH_STATUS_SUCCESS);
    fst_check_string_equals((const char *)b64_str, "AAAAAA==");

    size = switch_b64_decode((const char *)b64_str, decoded_str, sizeof(decoded_str));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "decoded_str: %s\n", decoded_str);
    fst_check_string_equals(decoded_str, str);
    fst_check(size == sizeof(str) + 1);
	for (i = 0; i < sizeof(str); i++) {
		fst_check(decoded_str[i] == str[i]);
	}
}
FST_TEST_END()

FST_TEST_BEGIN(b64_pad1)
{
    switch_size_t size;
    char str[] = {0, 0, 0, 0, 0};
    unsigned char b64_str[128];
    char decoded_str[128];
	int i;
    switch_status_t status = switch_b64_encode((unsigned char *)str, sizeof(str), b64_str, sizeof(b64_str));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "b64_str: %s\n", b64_str);
    fst_check(status == SWITCH_STATUS_SUCCESS);
    fst_check_string_equals((const char *)b64_str, "AAAAAAA=");

    size = switch_b64_decode((const char *)b64_str, decoded_str, sizeof(decoded_str));
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "decoded_str: %s\n", decoded_str);
    fst_check_string_equals(decoded_str, str);
    fst_check(size == sizeof(str) + 1);
		for (i = 0; i < sizeof(str); i++) {
		fst_check(decoded_str[i] == str[i]);
	}
}
FST_TEST_END()

FST_TEST_BEGIN(b64_roundtrip)
{
	/* Encode then decode inputs covering all three padding cases; the base64 output must
	   match the known value and decode back to the original bytes. Unlike b64_pad1/b64_pad2
	   (all-zero input), these push non-zero bytes through the padded final group. */
	struct {
		const char *plain;
		const char *encoded;
	} cases[] = {
		{ "Man",           "TWFu" },					/* no padding */
		{ "Ma",            "TWE=" },					/* one pad byte */
		{ "M",             "TQ==" },					/* two pad bytes */
		{ "Hello, World!", "SGVsbG8sIFdvcmxkIQ==" }
	};
	int i;

	for (i = 0; i < (int) (sizeof(cases) / sizeof(cases[0])); i++) {
		unsigned char encoded[64];
		char decoded[64];
		switch_size_t plain_len = strlen(cases[i].plain);
		switch_size_t decoded_len;
		switch_status_t status = switch_b64_encode((unsigned char *) cases[i].plain, plain_len, encoded, sizeof(encoded));

		fst_xcheck(status == SWITCH_STATUS_SUCCESS, "encode must succeed");
		fst_check_string_equals((const char *) encoded, cases[i].encoded);

		decoded_len = switch_b64_decode((const char *) encoded, decoded, sizeof(decoded));
		fst_xcheck(decoded_len == plain_len + 1, "decode must return the plaintext length plus the trailing NUL");
		fst_check_string_equals(decoded, cases[i].plain);
	}
}
FST_TEST_END()

FST_TEST_BEGIN(b64_decode_output_bounds)
{
	/* The 0xAA sentinel across the destination catches any write outside the region
	   the decode call is allowed to touch. */
	unsigned char guarded[32];
	switch_size_t size;
	int i;

	/* Decode with olen == 0: no room even for the trailing NUL, so the decoder must
	   write nothing and return 0. */
	memset(guarded, 0xAA, sizeof(guarded));
	size = switch_b64_decode("QUJDQUJDQUJDQUJDQUJDQUJDQUJDQUJD", (char *) guarded, 0);
	fst_xcheck(size == 0, "olen==0 decode must return 0");
	for (i = 0; i < (int) sizeof(guarded); i++) {
		fst_xcheck(guarded[i] == 0xAA, "olen==0 decode must not write any output byte");
	}

	/* Decode with olen == 1: room only for the terminating NUL at index 0; no decoded
	   data byte may be written. */
	memset(guarded, 0xAA, sizeof(guarded));
	size = switch_b64_decode("QUJDQUJDQUJDQUJD", (char *) guarded, 1);
	fst_xcheck(size == 1, "olen==1 decode must return 1 (NUL only)");
	fst_xcheck(guarded[0] == '\0', "olen==1 decode must store the NUL at index 0");
	for (i = 1; i < (int) sizeof(guarded); i++) {
		fst_xcheck(guarded[i] == 0xAA, "olen==1 decode must not write past index 0");
	}

	/* Decode with a small olen: up to olen-1 decoded bytes, then the trailing NUL at
	   index olen-1, and nothing beyond. "QUJD" decodes to "ABC". */
	memset(guarded, 0xAA, sizeof(guarded));
	size = switch_b64_decode("QUJD", (char *) guarded, 2);
	fst_xcheck(size == 2, "bounded decode must return olen");
	fst_xcheck(guarded[0] == 'A', "first decoded byte must be written");
	fst_xcheck(guarded[1] == '\0', "trailing NUL must be at index olen-1");
	for (i = 2; i < (int) sizeof(guarded); i++) {
		fst_xcheck(guarded[i] == 0xAA, "bounded decode must not write past index olen-1");
	}
}
FST_TEST_END()

FST_TEST_BEGIN(b64_decode_non_alphabet_bytes)
{
	/* Bytes outside the base64 alphabet, including those >= 0x80, are skipped and never used
	   as a lookup-table index. */
	char decoded[8];
	switch_size_t size;

	/* "QUJD" ("ABC") with a non-alphabet 0x80 byte spliced in. */
	size = switch_b64_decode("QU\x80" "JD", decoded, sizeof(decoded));
	fst_xcheck(size == 4, "non-alphabet byte must be skipped, leaving 3 data bytes plus the NUL");
	fst_check_string_equals(decoded, "ABC");
}
FST_TEST_END()

#define test_uri_count 6

/* Currently tests only clear_uri() */
FST_TEST_BEGIN(test_switch_http_parse_header)
{
	int i = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_http_request_t request = {0};
	char bad_uris[][200] = {
		"/t/o/o/_/l/o/n/g/_/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/2/3/4",
		"without_a_slash/",
	};
	char raw_uris[test_uri_count][200] = {
		"/////////uri1",
		"/././././uri2",
		"/uri3/uri3_1/.//uri3_2/../../uri3_3",
		"/../../../uri4",
		"/uri5/uri5_1/",
		"/uri6/uri6_1",
	};
	const char clear_uris[test_uri_count][200] = {
		"/uri1",
		"/uri2",
		"/uri3/uri3_3",
		"/uri4",
		"/uri5/uri5_1",
		"/uri6/uri6_1",
	};

	for (i = 0; i < (sizeof(bad_uris) / sizeof(bad_uris[0])); i++) {
		char bad_header[256];
		const char *bad_uri = bad_uris[i];

		/* Use precision specifier to suppress false-positive "format-truncation" warning.  */
		snprintf(bad_header, sizeof(bad_header), "GET %.199s HTTP/1.1\r\n\r\nBODY", bad_uri);

		fst_check((status = switch_http_parse_header(bad_header, sizeof(bad_header), &request)) == SWITCH_STATUS_FALSE);

		if (status == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad uri parsed [%d]: [%s]\n", i, request.uri);
			switch_http_free_request(&request);
		}
	}

	for (i = 0; i < test_uri_count; i++) {
		char raw_header[256];
		const char *clear_uri = clear_uris[i];
		const char *raw_uri = raw_uris[i];

		/* Use precision specifier to suppress false-positive "format-truncation" warning.  */
		snprintf(raw_header, sizeof(raw_header), "GET %.199s HTTP/1.1\r\n\r\nBODY", raw_uri);

		fst_check((status = switch_http_parse_header(raw_header, sizeof(raw_header), &request)) == SWITCH_STATUS_SUCCESS);
		fst_check_string_equals(clear_uri, request.uri);

		if (status == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "URI [%d]: [%s] => [%s]\n", i, raw_uri, request.uri);
			switch_http_free_request(&request);
		}
	}
}
FST_TEST_END()

FST_SUITE_END()

FST_MINCORE_END()

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

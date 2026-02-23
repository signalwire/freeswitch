#include <switch.h>
#include <test/switch_test.h>

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(switch_cjson)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(test_char_escaping_bug_0x80)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 1: Invalid UTF-8 byte 0x80 should be escaped */
	unsigned char test_string_80[] = { 0x80, 0x00 };
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_80);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test 0x80: %s\n", json_str);

	/* Byte 0x80 alone is invalid UTF-8 (continuation byte without leading byte)
	 * It should be escaped as \u0080
	 * This is useful for Windows-1252 encoded data where 0x80 = Euro sign
	 * Expected: {"test":"\u0080"}
	 */
	fst_check_string_equals(json_str, "{\"test\":\"\\u0080\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_bug_0xff)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 2: Invalid UTF-8 byte 0xFF should be escaped */
	unsigned char test_string_ff[] = { 0xFF, 0x00 };
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_ff);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test 0xFF: %s\n", json_str);

	/* Byte 0xFF alone is invalid UTF-8 (not a valid leading byte)
	 * It should be escaped as \u00ff
	 * This is useful for Windows-1252 encoded data where 0xFF = ÿ
	 * Expected: {"test":"\u00ff"}
	 */
	fst_check_string_equals(json_str, "{\"test\":\"\\u00ff\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_valid_utf8)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 3: Valid UTF-8 sequence should pass through unescaped */
	unsigned char test_string_utf8[] = { 0xC3, 0xA9, 0x00 }; /* "é" in UTF-8 */
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_utf8);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test valid UTF-8: %s\n", json_str);

	/* Valid UTF-8 sequences should be passed through without escaping
	 * Expected: {"test":"é"} (UTF-8 bytes 0xC3 0xA9 passed through)
	 * This is correct behavior - JSON strings can contain UTF-8 directly
	 */
	fst_check_string_equals(json_str, "{\"test\":\"é\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_invalid_utf8)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 3b: Invalid/incomplete UTF-8 sequence should be escaped */
	unsigned char test_string_invalid[] = { 0xC3, 0x41, 0x00 }; /* 0xC3 expects continuation byte, but 0x41 is 'A' */
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_invalid);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test invalid UTF-8: %s\n", json_str);

	/* Invalid UTF-8 sequences should be escaped byte by byte
	 * 0xC3 is invalid (missing continuation byte), so it gets escaped as \u00c3
	 * 0x41 is 'A' which passes through
	 * Expected: {"test":"\u00c3A"}
	 */
	fst_check_string_equals(json_str, "{\"test\":\"\\u00c3A\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_valid_utf8_3byte)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 3c: Valid 3-byte UTF-8 sequence should pass through unescaped */
	unsigned char test_string_3byte[] = { 0xE8, 0xAF, 0xAD, 0x00 }; /* "语" (Chinese) in UTF-8 */
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_3byte);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test valid 3-byte UTF-8: %s\n", json_str);

	/* Valid 3-byte UTF-8 sequences should be passed through without escaping
	 * Expected: {"test":"语"} (UTF-8 bytes 0xE8 0xAF 0xAD passed through)
	 */
	fst_check_string_equals(json_str, "{\"test\":\"语\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_chinese_key_value)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 3d: Valid UTF-8 in both key and value */
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	/* "中文" = "Chinese" (2 characters, 6 bytes in UTF-8)
	 * "也行" = "also OK" (2 characters, 6 bytes in UTF-8)
	 */
	cJSON_AddStringToObject(json, "中文", "也行");
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test Chinese key-value: %s\n", json_str);

	/* Valid UTF-8 in both keys and values should pass through without escaping
	 * Expected: {"中文":"也行"}
	 */
	fst_check_string_equals(json_str, "{\"中文\":\"也行\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_control_chars_ok)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 4: Control characters < 0x20 should work correctly (no bug here) */
	unsigned char test_string_control[] = { 0x01, 0x1F, 0x00 };
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_control);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test control chars: %s\n", json_str);

	/* This should work correctly even with the bug */
	fst_check_string_equals(json_str, "{\"test\":\"\\u0001\\u001f\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_boundary_0x7f)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 5: Boundary test at 0x7F (last ASCII character, should not be escaped) */
	unsigned char test_string_7f[] = { 0x7F, 0x00 };
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_7f);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test 0x7F: %s\n", json_str);

	/* 0x7F is DEL character, typically not escaped but it's printable in JSON context
	 * This demonstrates the boundary between characters that work (0x7F) and those with the bug (0x80+)
	 */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "0x7F boundary test - should contain raw 0x7F byte\n");

	cJSON_Delete(json);
	cJSON_free(json_str);
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

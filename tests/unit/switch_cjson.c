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

	/* Test case 1: Character 0x80 (first byte >= 0x80) */
	unsigned char test_string_80[] = { 0x80, 0x00 };
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_80);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test 0x80: %s\n", json_str);

	/* Bug: Characters >= 0x80 should be escaped as \uXXXX in JSON
	 * Expected: {"test":"\u0080"}
	 * Actual (bug): {"test":"�"} or raw byte 0x80
	 *
	 * The bug is in cJSON.c line 909:
	 * if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\'))
	 *
	 * When input_pointer is 'unsigned char*' but *input_pointer is treated as signed char,
	 * 0x80 becomes -128 in signed comparison, so (*input_pointer > 31) is false,
	 * causing the character to pass through unescaped instead of being encoded as \u0080
	 */

	/* This test will FAIL with the bug, demonstrating the issue */
	fst_check_string_equals(json_str, "{\"test\":\"\\u0080\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_bug_0xff)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 2: Character 0xFF (highest byte value) */
	unsigned char test_string_ff[] = { 0xFF, 0x00 };
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_ff);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test 0xFF: %s\n", json_str);

	/* Expected: {"test":"\u00ff"}
	 * Actual (bug): {"test":"ÿ"} or raw byte 0xFF
	 */
	fst_check_string_equals(json_str, "{\"test\":\"\\u00ff\"}");

	cJSON_Delete(json);
	cJSON_free(json_str);
}
FST_TEST_END()

FST_TEST_BEGIN(test_char_escaping_bug_utf8_like)
{
	cJSON *json = NULL;
	char *json_str = NULL;

	/* Test case 3: UTF-8-like sequence (multi-byte pattern) */
	unsigned char test_string_utf8[] = { 0xC3, 0xA9, 0x00 }; /* Would be "é" in UTF-8 */
	json = cJSON_CreateObject();
	fst_requires(json != NULL);

	cJSON_AddStringToObject(json, "test", (const char *)test_string_utf8);
	json_str = cJSON_PrintUnformatted(json);
	fst_requires(json_str != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Test UTF-8-like: %s\n", json_str);

	/* Expected: {"test":"\u00c3\u00a9"}
	 * Actual (bug): {"test":"é"} or raw bytes
	 *
	 * Note: cJSON doesn't handle UTF-8 sequences, it should escape each byte >= 0x80
	 */
	fst_check_string_equals(json_str, "{\"test\":\"\\u00c3\\u00a9\"}");

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

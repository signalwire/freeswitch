/*
 * Created by damir@telnyx.com 09.12.2025.
 *
 * test_string_stream_mem_leak.c -- tests memory leaks in switch_string_stream_write function
 *
 */

#include <switch.h>
#include <test/switch_test.h>

SWITCH_DECLARE_NONSTD(switch_status_t) switch_string_stream_write(switch_stream_handle_t *handle, const char *fmt, ...);

FST_CORE_EX_BEGIN("./conf", SCF_VG | SCF_USE_SQL)

FST_MODULE_BEGIN(mod_sofia, sofia_string_stream)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(test_switch_string_stream_write_basic)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	
	SWITCH_STANDARD_STREAM(stream);
	
	status = switch_string_stream_write(&stream, "Hello %s %d", "World", 123);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	fst_check(stream.data != NULL);
	fst_check_string_equals((char*)stream.data, "Hello World 123");
	
	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_TEST_BEGIN(test_switch_string_stream_write_empty_string)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	
	SWITCH_STANDARD_STREAM(stream);
	
	status = switch_string_stream_write(&stream, "");
	fst_check(status == SWITCH_STATUS_SUCCESS);
	fst_check(stream.data != NULL);
	fst_check_string_equals((char*)stream.data, "");
	
	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_TEST_BEGIN(test_switch_string_stream_write_large_string)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	char large_format[2048];
	char expected[4096];
	int i;
	
	SWITCH_STANDARD_STREAM(stream);
	
	for (i = 0; i < 2000; i++) {
		large_format[i] = 'A';
	}
	large_format[2000] = '\0';
	
	strcpy(expected, large_format);
	
	status = switch_string_stream_write(&stream, "%s", large_format);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	fst_check(stream.data != NULL);
	fst_check_string_equals((char*)stream.data, expected);
	
	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_TEST_BEGIN(test_switch_string_stream_write_multiple_calls)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	
	SWITCH_STANDARD_STREAM(stream);
	
	status = switch_string_stream_write(&stream, "First call %d", 1);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	
	status = switch_string_stream_write(&stream, " Second call %d", 2);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	
	status = switch_string_stream_write(&stream, " Third call %d", 3);
	fst_check(status == SWITCH_STATUS_SUCCESS);
	
	fst_check(stream.data != NULL);
	fst_check_string_equals((char*)stream.data, "First call 1 Second call 2 Third call 3");
	
	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_TEST_BEGIN(test_switch_string_stream_write_memory_stress)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	int i;
	
	SWITCH_STANDARD_STREAM(stream);
	
	for (i = 0; i < 100; i++) {
		status = switch_string_stream_write(&stream, "Iteration %d with some text to make it longer ", i);
		fst_check(status == SWITCH_STATUS_SUCCESS);
	}
	
	fst_check(stream.data != NULL);
	fst_check(strlen((char*)stream.data) > 0);
	fst_check(strstr((char*)stream.data, "Iteration 0") != NULL);
	fst_check(strstr((char*)stream.data, "Iteration 99") != NULL);
	
	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_TEST_BEGIN(test_switch_string_stream_write_format_specifiers)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	
	SWITCH_STANDARD_STREAM(stream);
	
	status = switch_string_stream_write(&stream, "String: %s, Int: %d, Float: %.2f, Char: %c", 
		"test", 42, 3.14, 'X');
	fst_check(status == SWITCH_STATUS_SUCCESS);
	fst_check(stream.data != NULL);
	fst_check(strstr((char*)stream.data, "String: test") != NULL);
	fst_check(strstr((char*)stream.data, "Int: 42") != NULL);
	fst_check(strstr((char*)stream.data, "Float: 3.14") != NULL);
	fst_check(strstr((char*)stream.data, "Char: X") != NULL);
	
	switch_safe_free(stream.data);
}
FST_TEST_END()

FST_TEST_BEGIN(test_switch_string_stream_write_memory_leak_detection)
{
	switch_stream_handle_t stream = { 0 };
	switch_status_t status;
	int i;
	
	/* This test specifically checks for memory leaks by calling the function
	 * many times and ensuring proper cleanup. Run with ASAN to detect leaks.
	 */
	for (i = 0; i < 1000; i++) {
		SWITCH_STANDARD_STREAM(stream);
		
		status = switch_string_stream_write(&stream, "Test iteration %d with some data", i);
		fst_check(status == SWITCH_STATUS_SUCCESS);
		fst_check(stream.data != NULL);
		
		switch_safe_free(stream.data);
		
		/* Reset stream for next iteration */
		memset(&stream, 0, sizeof(stream));
	}
}
FST_TEST_END()

FST_MODULE_END()

FST_CORE_END()

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

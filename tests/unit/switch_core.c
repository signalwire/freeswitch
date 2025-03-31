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
 * Chris Rienzo <chris@signalwire.com>
 * Seven Du <dujinfang@gmail.com>
 *
 *
 * switch_core.c -- tests core functions
 *
 */
#include <switch.h>
#include <test/switch_test.h>

#include <string.h>
// #include <time.h>
#include <uuid/uuid.h>
#include <switch_uuidv7.h>

#if defined(HAVE_OPENSSL)
#include <openssl/ssl.h>
#endif

#define ENABLE_SNPRINTFV_TESTS 0 /* Do not turn on for CI as this requires a lot of RAM */

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_core)
	{
		FST_SETUP_BEGIN()
		{
			switch_core_set_variable("spawn_instead_of_system", "false");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(test_switch_rand)
		{
			int i, c = 0;

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "\nLet's generate a few random numbers.\n");

			for (i = 0; i < 10; i++) {
				uint32_t rnd = switch_rand();

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Random number %d\n", rnd);

				if (rnd == 1) {
					c++;
				}
			}

			/* We do not expect all random numbers to be 1 all 10 times. That would mean we have an error OR we are lucky to have 10 random ones! */
			fst_check(c < 10);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_uint31_t_overflow)
		{
			switch_uint31_t x;
			uint32_t overflow;

			x.value = 0x7fffffff;
			x.value++;

			fst_check_int_equals(x.value, 0);
			x.value++;
			fst_check_int_equals(x.value, 1);
			x.value -= 2;
			fst_check_int_equals(x.value, 0x7fffffff);

			overflow = (uint32_t)0x7fffffff + 1;
			x.value = overflow;
			fst_check_int_equals(x.value, 0);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_parse_cidr_v6)
		{
			ip_t ip, mask;
			uint32_t bits;

			fst_check(!switch_parse_cidr("fe80::/10", &ip, &mask, &bits));
			fst_check_int_equals(bits, 10);
			fst_check_int_equals(ip.v6.s6_addr[0], 0xfe);
			fst_check_int_equals(ip.v6.s6_addr[1], 0x80);
			fst_check_int_equals(ip.v6.s6_addr[2], 0);
			fst_check_int_equals(mask.v6.s6_addr[0], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[1], 0xc0);
			fst_check_int_equals(mask.v6.s6_addr[2], 0);

			fst_check(!switch_parse_cidr("::/0", &ip, &mask, &bits));
			fst_check_int_equals(bits, 0);
			fst_check_int_equals(ip.v6.s6_addr[0], 0);
			fst_check_int_equals(ip.v6.s6_addr[1], 0);
			fst_check_int_equals(ip.v6.s6_addr[2], 0);
			fst_check_int_equals(mask.v6.s6_addr[0], 0);
			fst_check_int_equals(mask.v6.s6_addr[1], 0);
			fst_check_int_equals(mask.v6.s6_addr[2], 0);

			fst_check(!switch_parse_cidr("::1/128", &ip, &mask, &bits));
			fst_check_int_equals(bits, 128);
			fst_check_int_equals(ip.v6.s6_addr[0], 0);
			fst_check_int_equals(ip.v6.s6_addr[1], 0);
			fst_check_int_equals(ip.v6.s6_addr[2], 0);
			fst_check_int_equals(ip.v6.s6_addr[3], 0);
			fst_check_int_equals(ip.v6.s6_addr[4], 0);
			fst_check_int_equals(ip.v6.s6_addr[5], 0);
			fst_check_int_equals(ip.v6.s6_addr[6], 0);
			fst_check_int_equals(ip.v6.s6_addr[7], 0);
			fst_check_int_equals(ip.v6.s6_addr[8], 0);
			fst_check_int_equals(ip.v6.s6_addr[9], 0);
			fst_check_int_equals(ip.v6.s6_addr[10], 0);
			fst_check_int_equals(ip.v6.s6_addr[11], 0);
			fst_check_int_equals(ip.v6.s6_addr[12], 0);
			fst_check_int_equals(ip.v6.s6_addr[13], 0);
			fst_check_int_equals(ip.v6.s6_addr[14], 0);
			fst_check_int_equals(ip.v6.s6_addr[15], 1);
			fst_check_int_equals(mask.v6.s6_addr[0], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[1], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[2], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[3], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[4], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[5], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[6], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[7], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[8], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[9], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[10], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[11], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[12], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[13], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[14], 0xff);
			fst_check_int_equals(mask.v6.s6_addr[15], 0xff);
		}
		FST_TEST_END()

#if ENABLE_SNPRINTFV_TESTS
		FST_TEST_BEGIN(test_snprintfv_1)
		{
			size_t src_buf_size = 0x100000001;
			char* src = calloc(1, src_buf_size);

			if (!src) {
				printf("bad allocation\n");

				return -1;
			}

			src[0] = '\xc0';
			memset(src + 1, '\x80', 0xffffffff);

			char dst[256];
			switch_snprintfv(dst, 256, "'%!q'", src);
			free(src);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_snprintfv_2)
		{
#define STR_LEN ((0x100000001 - 3) / 2)

				char* src = calloc(1, STR_LEN + 1); /* Account for NULL byte. */

				if (!src) { return -1; }

				memset(src, 'a', STR_LEN);

				char* dst = calloc(1, STR_LEN + 3); /* Account for extra quotes and NULL byte */
				if (!dst) { return -1; }

				switch_snprintfv(dst, 2 * STR_LEN + 3, "'%q'", src);

				free(src);
				free(dst);
		}
		FST_TEST_END()
#endif

		FST_TEST_BEGIN(test_switch_is_number_in_range)
		{
			fst_check_int_equals(switch_is_uint_in_range("x5", 0, 10), SWITCH_FALSE);
			fst_check_int_equals(switch_is_uint_in_range("0", 1, 10), SWITCH_FALSE);
			fst_check_int_equals(switch_is_uint_in_range("-11", -10, 10), SWITCH_FALSE);
			fst_check_int_equals(switch_is_uint_in_range("-10", -10, 10), SWITCH_FALSE);
			fst_check_int_equals(switch_is_uint_in_range("-5", -10, 10), SWITCH_FALSE);
			fst_check_int_equals(switch_is_uint_in_range("-5", -10, 10), SWITCH_FALSE);
			fst_check_int_equals(switch_is_uint_in_range("5", -10, 10), SWITCH_FALSE);
			fst_check_int_equals(switch_is_uint_in_range("0", 0, 10), SWITCH_TRUE);
			fst_check_int_equals(switch_is_uint_in_range("10", 0, 10), SWITCH_TRUE);
			fst_check_int_equals(switch_is_uint_in_range("11", 0, 10), SWITCH_FALSE);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_md5)
		{
			char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };
			char test_string[] = "test";
			switch_status_t status;

			status = switch_md5_string(digest, (void *)test_string, strlen(test_string));

			fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
			fst_check_string_equals(digest, "098f6bcd4621d373cade4e832627b4f6");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_event_add_header_leak)
		{
			switch_event_t* event;

			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_CALLSTATE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-Call-State-Number[0]", "1");
				switch_event_fire(&event);
			}

			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_CALLSTATE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Channel-Call-State-Number[5000]", "12");
				switch_event_fire(&event);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_xml_free_attr)
		{
			switch_xml_t parent_xml = switch_xml_new("xml");
			switch_xml_t xml = switch_xml_add_child_d(parent_xml, "test", 1);
			switch_xml_set_attr(xml, "a1", "v1");
			switch_xml_set_attr_d(xml, "a2", "v2");
			switch_xml_free(parent_xml);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_xml_set_attr)
		{
			switch_xml_t parent_xml = switch_xml_new("xml");
			switch_xml_t xml = switch_xml_add_child_d(parent_xml, "test", 1);
			switch_xml_set_attr_d(xml, "test1", "1");
			switch_xml_set_attr(xml, "a1", "v1");
			switch_xml_set_attr_d(xml, "a2", "v2");
			switch_xml_set_attr(xml, "test1", NULL);
			switch_xml_set_attr_d(xml, "test2", "2");
			switch_xml_set_attr_d(xml, "a3", "v3");
			switch_xml_set_attr(xml, "test2", NULL);
			switch_xml_set_attr(xml, "a1", NULL);
			switch_xml_set_attr(xml, "a2", NULL);
			switch_xml_set_attr(xml, "a3", NULL);
			switch_xml_free(parent_xml);
		}
		FST_TEST_END()

#ifdef HAVE_OPENSSL
		FST_TEST_BEGIN(test_md5)
		{
			char *digest_name = "md5";
			char *digest_str = NULL;
			const char *str = "test data";
			unsigned int outputlen;

			switch_status_t status = switch_digest_string(digest_name, &digest_str, str, strlen(str), &outputlen);
			fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
			fst_check_string_equals(digest_str, "eb733a00c0c9d336e65691a37ab54293");
			switch_safe_free(digest_str);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_sha256)
		{
			char *digest_name = "sha256";
			char *digest_str = NULL;
			const char *str = "test data";
			unsigned int outputlen;

			switch_status_t status = switch_digest_string(digest_name, &digest_str, str, strlen(str), &outputlen);
			fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
			fst_check_string_equals(digest_str, "916f0027a575074ce72a331777c3478d6513f786a591bd892da1a577bf2335f9");
			switch_safe_free(digest_str);
		}
		FST_TEST_END()
#endif

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
		FST_TEST_BEGIN(test_sha512_256)
		{
			char *digest_name = "sha512-256";
			char *digest_str = NULL;
			const char *str = "test data";
			unsigned int outputlen;

			switch_status_t status = switch_digest_string(digest_name, &digest_str, str, strlen(str), &outputlen);
			fst_check_int_equals(status, SWITCH_STATUS_SUCCESS);
			fst_check_string_equals(digest_str, "9fe875600168548c1954aed4f03974ce06b3e17f03a70980190da2d7ef937a43");
			switch_safe_free(digest_str);
		}
		FST_TEST_END()
#endif

#ifndef WIN32
		FST_TEST_BEGIN(test_fork)
		{
			switch_stream_handle_t exec_result = { 0 };
			SWITCH_STANDARD_STREAM(exec_result);
			fst_requires(switch_stream_system_fork("ip ad sh", &exec_result) == 0);
			fst_requires(!zstr(exec_result.data));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s\n", (char *)exec_result.data);

			fst_requires(switch_stream_system_fork("ip ad sh | grep link", &exec_result) == 0);
			fst_requires(!zstr(exec_result.data));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s\n", (char *)exec_result.data);

			switch_safe_free(exec_result.data);
		}
		FST_TEST_END()
#endif

		FST_TEST_BEGIN(test_non_fork_exec_set)
		{
			char *var_test = switch_core_get_variable_dup("test");
			char *var_default_password = switch_core_get_variable_dup("default_password");

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "global_getvar test: %s\n", switch_str_nil(var_test));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "global_getvar default_password: %s\n", switch_str_nil(var_default_password));

			fst_check_string_not_equals(var_test, "");
			fst_check_string_not_equals(var_default_password, "");
			fst_check_string_equals(var_test, var_default_password);

			switch_safe_free(var_test);
			switch_safe_free(var_default_password);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_sockaddr_new)
		{
			int type = SOCK_DGRAM;
			switch_port_t port = 12044;
			const char *ip = "127.0.0.1";

			switch_memory_pool_t *pool = NULL;
			switch_sockaddr_t *local_addr = NULL;
			switch_socket_t *sock = NULL;
			switch_bool_t r = SWITCH_FALSE;

			if (switch_core_new_memory_pool(&pool) == SWITCH_STATUS_SUCCESS) {
				if (switch_sockaddr_new(&local_addr, ip, port, pool) == SWITCH_STATUS_SUCCESS) {
					if (switch_socket_create(&sock, switch_sockaddr_get_family(local_addr), type, 0, pool) == SWITCH_STATUS_SUCCESS) {
						if (switch_socket_bind(sock, local_addr) == SWITCH_STATUS_SUCCESS) {
							r = SWITCH_TRUE;
						}
						switch_socket_close(sock);
					}
				}

				switch_core_destroy_memory_pool(&pool);
			}

			fst_check_int_equals(r, SWITCH_TRUE);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_spawn)
		{
#ifdef __linux__
			int status;
			switch_stream_handle_t stream = { 0 };

			status = switch_spawn("echo CHECKING_BAD_FILE_DESCRIPTOR", SWITCH_TRUE);
			fst_check_int_equals(status, 0);

			SWITCH_STANDARD_STREAM(stream);
			status = switch_stream_spawn("echo DEADBEEF", SWITCH_FALSE, SWITCH_TRUE, &stream);
			fst_check_int_equals(status, 0);
			fst_check_string_equals(stream.data, "DEADBEEF\n");
			switch_safe_free(stream.data);

			SWITCH_STANDARD_STREAM(stream);
			status = switch_stream_spawn("echo DEADBEEF", SWITCH_FALSE, SWITCH_FALSE, &stream);
			fst_check_int_equals(status, 0);
			fst_check_string_equals(stream.data, "DEADBEEF\n");
			switch_safe_free(stream.data);

			printf("\nExpected warning check ... ");
			status = switch_spawn("false", SWITCH_TRUE);
			fct_chk_neq_int(status, 0);

			status = switch_spawn("false", SWITCH_FALSE);
			fct_chk_eq_int(status, 0);

			status = switch_spawn("true", SWITCH_TRUE);
			fct_chk_eq_int(status, 0);
#endif
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_spawn_instead_of_system)
		{
#ifdef __linux__
			int status;
			char file_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
			const char *filename = NULL;
			const char *cmd = NULL;

			// tell FS core to use posix_spawn() instead of popen() and friends
			switch_core_set_variable("spawn_instead_of_system", "true");

			// echo text to a file using shell redirection- this will ensure the command was executed in a shell, as expected
			switch_uuid_str(file_uuid, sizeof(file_uuid));
			filename = switch_core_sprintf(fst_pool, "%s" SWITCH_PATH_SEPARATOR "%s", SWITCH_GLOBAL_dirs.temp_dir, file_uuid);
			cmd = switch_core_sprintf(fst_pool, "echo test_switch_spawn_instead_of_system with spaces > %s", filename);
			status = switch_system(cmd, SWITCH_TRUE);

			fst_check_int_equals(status, 0);
			fst_xcheck(status == 0, "Expect switch_system() command to return 0");
			fst_xcheck(switch_file_exists(filename, fst_pool) == SWITCH_STATUS_SUCCESS, "Expect switch_system() to use shell to create file via > redirection");
			unlink(filename);

			// verify exec-set works- see conf/freeswitch.xml for test setup of shell_exec_set_test global variable
			fst_check_string_equals(switch_core_get_variable("shell_exec_set_test"), "usr");
#endif
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_safe_atoXX)
		{
			fst_check_int_equals(switch_safe_atoi("1", 0), 1);
			fst_check_int_equals(switch_safe_atoi("", 2), 0);
			fst_check_int_equals(switch_safe_atoi(0, 3), 3);

			fst_check_int_equals(switch_safe_atol("9275806", 0), 9275806);
			fst_check_int_equals(switch_safe_atol("", 2), 0);
			fst_check_int_equals(switch_safe_atol(0, 3), 3);

			fst_check_int_equals(switch_safe_atoll("9275806", 0), 9275806);
			fst_check_int_equals(switch_safe_atoll("", 2), 0);
			fst_check_int_equals(switch_safe_atoll(0, 3), 3);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_core_hash_insert_dup)
		{
			char *magicnumber = malloc(9);
			switch_hash_index_t *hi;
			switch_hash_t *hash = NULL;
			void *hash_val;
			switch_core_hash_init(&hash);
			fst_requires(hash);

			snprintf(magicnumber, 9, "%s", "DEADBEEF");
			switch_core_hash_insert_dup(hash, "test", (const char *)magicnumber);
			snprintf(magicnumber, 9, "%s", "BAADF00D");

			hi = switch_core_hash_first(hash);
			switch_core_hash_this(hi, NULL, NULL, &hash_val);
			fst_check_string_equals(hash_val, "DEADBEEF");
			switch_safe_free(hash_val);
			free(magicnumber);
			free(hi);
			switch_core_hash_destroy(&hash);
			fst_requires(hash == NULL);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_core_hash_insert_alloc)
		{
			char *item;
			switch_hash_index_t *hi;
			switch_hash_t *hash = NULL;
			void *hash_val;
			switch_core_hash_init(&hash);
			fst_requires(hash);

			item = switch_core_hash_insert_alloc(hash, "test", 10);
			fst_requires(item);
			snprintf(item, 9, "%s", "DEADBEEF");

			hi = switch_core_hash_first(hash);
			switch_core_hash_this(hi, NULL, NULL, &hash_val);
			fst_check_string_equals(hash_val, "DEADBEEF");
			free(hi);
			switch_core_hash_destroy(&hash);
			fst_requires(hash == NULL);
			free(item);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_core_hash_insert_pointer)
		{
			int i, sum = 0;
			switch_hash_index_t *hi;
			switch_hash_t *hash = NULL;
			switch_core_hash_init(&hash);
			fst_requires(hash);

			for (i = 0; i < 10; i++) {
				int *num = malloc(sizeof(int));
				*num = i;
				fst_check_int_equals(switch_core_hash_insert_pointer(hash, (void*)num), SWITCH_STATUS_SUCCESS);
			}

			i = 0;
			for (hi = switch_core_hash_first(hash); hi; hi = switch_core_hash_next(&hi)) {
				void *hash_val;
				switch_core_hash_this(hi, NULL, NULL, &hash_val);
				sum += *(int*)hash_val;
				free(hash_val);
				i++;
			}

			fst_check_int_equals(i, 10);
			fst_check_int_equals(sum, 45);

			switch_core_hash_destroy(&hash);
			fst_requires(hash == NULL);
		}
		FST_TEST_END()

		FST_SESSION_BEGIN(test_switch_channel_get_variable_strdup)
		{
			const char *val;
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);

			fst_check(channel);

			switch_channel_set_variable(channel, "test_var", "test_value");

			fst_check(!switch_channel_get_variable_strdup(channel, "test_var_does_not_exist"));

			val = switch_channel_get_variable_strdup(channel, "test_var");

			fst_check(val);
			fst_check_string_equals(val, "test_value");

			free((char *)val);
		}
		FST_SESSION_END()

		FST_SESSION_BEGIN(test_switch_channel_get_variable_buf)
		{
			char buf[16] = { 0 };
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);

			fst_check(channel);

			switch_channel_set_variable(channel, "test_var", "test_value");

			fst_check(switch_channel_get_variable_buf(channel, "test_var", buf, sizeof(buf)) == SWITCH_STATUS_SUCCESS);
			fst_check_string_equals(buf, "test_value");

			fst_check(switch_channel_get_variable_buf(channel, "test_var_does_not_exist", buf, sizeof(buf)) == SWITCH_STATUS_FALSE);
		}
		FST_SESSION_END()

		FST_TEST_BEGIN(test_create_uuid)
		{
			switch_uuid_t uuid;
			switch_uuid_t uuid2;
			char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "test_create_uuid:\n");
			uuidv7_new(uuid.data);
			switch_uuid_format(uuid_str, &uuid);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "uuidv7: %s\n", uuid_str);
			uuidv7_new(uuid.data);
			switch_uuid_format(uuid_str, &uuid);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "uuidv7: %s\n", uuid_str);
			fst_check(0 != memcmp(uuid.data, uuid2.data, sizeof(uuid.data)));
			uuid_generate(uuid.data);
			switch_uuid_format(uuid_str, &uuid);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "uuidv4: %s\n", uuid_str);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_create_uuid_speed)
		{
			int n;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "test_create_uuid_speed:\n");
			for (n = 1; n < 4; n++) {
				clock_t start = 0, end = 0;
				switch_uuid_t uuid;
				double cpu_time_used = 0;
				start = clock();
				for (int i = 0; i < 1000 * pow(10, n); i++) {
					uuidv7_new(uuid.data);
				}
				end = clock();
				cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%d uuidv7_new running time: %f seconds\n", 1000 * (int)pow(10, n), cpu_time_used);

				start = clock();
				for (long long int i = 0; i < 1000 * pow(10, n); i++) {
					uuid_generate(uuid.data);
				}
				end = clock();
				cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%d uuid_generate running time: %f seconds\n", 1000 * (int)pow(10, n), cpu_time_used);
			}
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

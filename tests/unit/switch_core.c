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

#if defined(HAVE_OPENSSL)
#include <openssl/ssl.h>
#endif

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
	}
	FST_SUITE_END()
}
FST_CORE_END()

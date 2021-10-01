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
#include <stdlib.h>

#include <test/switch_test.h>

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_ivr_originate)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

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
	}
	FST_SUITE_END()
}
FST_CORE_END()

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
 * Andrey Volk <andywolk@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Andrey Volk <andywolk@gmail.com>
 *
 * mod_commands_test -- mod_commands tests
 *
 */

#include <test/switch_test.h>

FST_CORE_BEGIN("conf")
{
	FST_MODULE_BEGIN(mod_commands, mod_commands_test)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(spawn_test)
		{
#ifdef __linux__
			switch_stream_handle_t stream = { 0 };

			SWITCH_STANDARD_STREAM(stream);
			switch_api_execute("bg_spawn", "echo TEST_BG_SPAWN", NULL, &stream);
			fst_check_string_equals(stream.data, "+OK\n");
			switch_safe_free(stream.data);

			SWITCH_STANDARD_STREAM(stream);
			switch_api_execute("spawn_stream", "echo DEADBEEF", NULL, &stream);
			fst_check_string_equals(stream.data, "DEADBEEF\n");
			switch_safe_free(stream.data);

			SWITCH_STANDARD_STREAM(stream);
			switch_api_execute("spawn", "echo TEST_NO_OUTPUT", NULL, &stream);
			fst_check_string_equals(stream.data, "+OK\n");
			switch_safe_free(stream.data);
#endif
		}
		FST_TEST_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()
	}
	FST_MODULE_END()
}
FST_CORE_END()

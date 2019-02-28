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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * test_mod_lua -- mod_lua test
 *
 */

#include <test/switch_test.h>

FST_CORE_BEGIN("conf")
{
	FST_MODULE_BEGIN(mod_lua, mod_lua_test)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_lua");
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(json_test)
		{
			switch_stream_handle_t stream = { 0 };

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("lua", "test_json.lua", NULL, &stream);

			if (stream.data) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "LUA DATA: %s\n", (char *)stream.data);
				fst_check(strstr(stream.data, "+OK") == stream.data);
				free(stream.data);
			}
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

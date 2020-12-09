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
 * Hailin Zhou <zhouhailin555@aliyun.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * test_mod_commands -- mod_commands test
 *
 */

#include <switch.h>
#include <test/switch_test.h>
#include <stdlib.h>


FST_CORE_BEGIN("conf")

FST_MODULE_BEGIN(mod_commands, mod_commands_test)

FST_SETUP_BEGIN()
{
	fst_requires_module("mod_commands");
	fst_requires_module("mod_tone_stream");
	fst_requires_module("mod_sndfile");
	fst_requires_module("mod_dptools");
	fst_requires_module("mod_test");
	fst_requires_module("mod_loopback");
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()


FST_SESSION_BEGIN(uuid_speak_test)
{
	switch_stream_handle_t stream = { 0 };

	SWITCH_STANDARD_STREAM(stream);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_speak %s test:laihu-tts|siqi|'Hello World'\n", switch_core_session_get_uuid(fst_session));
	switch_api_execute("uuid_speak", switch_core_session_sprintf(fst_session, "%s test:laihu-tts|siqi|'Hello World'", switch_core_session_get_uuid(fst_session)), NULL, &stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_speak %s test:laihu-tts||'Hello World'\n", switch_core_session_get_uuid(fst_session));
	switch_api_execute("uuid_speak", switch_core_session_sprintf(fst_session, "%s test:laihu-tts||'Hello World'", switch_core_session_get_uuid(fst_session)), NULL, &stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_setvar %s tts_engine test:laihu-tts\n", switch_core_session_get_uuid(fst_session));
	switch_api_execute("uuid_setvar", switch_core_session_sprintf(fst_session, "%s tts_engine test:laihu-tts", switch_core_session_get_uuid(fst_session)), NULL, &stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_speak %s 'Hello World'\n", switch_core_session_get_uuid(fst_session));
	switch_api_execute("uuid_speak", switch_core_session_sprintf(fst_session, "%s 'Hello World'", switch_core_session_get_uuid(fst_session)), NULL, &stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_setvar %s tts_voice siqi\n", switch_core_session_get_uuid(fst_session));
	switch_api_execute("uuid_setvar", switch_core_session_sprintf(fst_session, "%s tts_voice siqi", switch_core_session_get_uuid(fst_session)), NULL, &stream);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_speak %s 'Hello World'\n", switch_core_session_get_uuid(fst_session));
	switch_api_execute("uuid_speak", switch_core_session_sprintf(fst_session, "%s 'Hello World'", switch_core_session_get_uuid(fst_session)), NULL, &stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_speak\n");
	switch_api_execute("uuid_speak", NULL, NULL, &stream);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid_speak %s\n", switch_core_session_get_uuid(fst_session));
	switch_api_execute("uuid_speak", switch_core_session_sprintf(fst_session, "%s", switch_core_session_get_uuid(fst_session)), NULL, &stream);

	if (stream.data) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "LUA DATA: %s\n", (char *)stream.data);
		fst_check(strstr(stream.data, "+OK") == stream.data);
		free(stream.data);
	}
}
FST_SESSION_END()


FST_MODULE_END()

FST_CORE_END()

/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2021, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * test_tts_format.c -- tests for tts:// file format
 *
 */
#include <switch.h>
#include <test/switch_test.h>

FST_CORE_BEGIN(".")

FST_MODULE_BEGIN(mod_ssml, test_tts_format)

FST_SETUP_BEGIN()
{
	fst_requires_module("mod_test");
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
	switch_core_set_variable("mod_test_tts_must_have_channel_uuid", "false");
}
FST_TEARDOWN_END()

FST_SESSION_BEGIN(tts_channel_uuid)
{
	char *tts_without_channel_uuid = "tts://test||This is a test";
	char *tts_with_channel_uuid = switch_core_session_sprintf(fst_session, "{channel-uuid=%s}tts://test||This is a test", switch_core_session_get_uuid(fst_session));
	switch_status_t status;
	switch_core_set_variable("mod_test_tts_must_have_channel_uuid", "true");
	status = switch_ivr_play_file(fst_session, NULL, tts_without_channel_uuid, NULL);
	fst_xcheck(status != SWITCH_STATUS_SUCCESS, "Expect channel UUID not to be delivered to TTS module");

	status = switch_ivr_play_file(fst_session, NULL, tts_with_channel_uuid, NULL);
	fst_xcheck(status == SWITCH_STATUS_SUCCESS, "Expect channel UUID to be delivered to TTS module");
}
FST_SESSION_END()

FST_MODULE_END()

FST_CORE_END()

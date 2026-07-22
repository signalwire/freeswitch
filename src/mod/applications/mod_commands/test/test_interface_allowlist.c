/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2024, Anthony Minessale II <anthm@freeswitch.org>
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
 * Contributor(s):
 * Chris Rienzo <chris@signalwire.com>
 *
 * test_interface_allowlist -- tests the switch.conf.xml <interface-allowlist>
 *
 * The core is booted with conf_interface_allowlist/freeswitch.xml, whose allowlist permits only
 * a couple of mod_commands interfaces. mod_commands is then loaded and we verify that unlisted /
 * dangerous commands are refused registration while listed ones load, that a "module.name.type"
 * entry gates by interface type, and that interface_allowlist_dump prints the config format.
 *
 */

#include <test/switch_test.h>
#include <string.h>

FST_CORE_BEGIN("conf_interface_allowlist")
{
	FST_MODULE_BEGIN(mod_commands, interface_allowlist_test)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		/* Listed commands register and run; unlisted (including the shell-exec commands) are refused.
		   A refused command is not in the api hash, so switch_api_execute returns FALSE without ever
		   invoking the command function (empty arg means system/spawn would only print usage anyway). */
		FST_TEST_BEGIN(allowlist_blocks_unlisted_apis)
		{
			switch_stream_handle_t stream = { 0 };

			/* allowed: mod_commands.status.api */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("status", "", NULL, &stream) == SWITCH_STATUS_SUCCESS);
			switch_safe_free(stream.data);

			/* allowed: mod_commands.version */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("version", "", NULL, &stream) == SWITCH_STATUS_SUCCESS);
			switch_safe_free(stream.data);

			/* blocked: the shell-exec API this whole feature exists to disable */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("system", "", NULL, &stream) == SWITCH_STATUS_FALSE);
			switch_safe_free(stream.data);

			/* blocked: spawn family */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("spawn", "", NULL, &stream) == SWITCH_STATUS_FALSE);
			switch_safe_free(stream.data);

			/* blocked: an ordinary unlisted command, proving default-deny once the list is active */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("uptime", "", NULL, &stream) == SWITCH_STATUS_FALSE);
			switch_safe_free(stream.data);
		}
		FST_TEST_END()

		/* "mod_commands.status.api" permits only the API type; the JSON API of the same name stays blocked. */
		FST_TEST_BEGIN(allowlist_type_qualified_entry)
		{
			switch_stream_handle_t stream = { 0 };
			cJSON *jcmd, *reply = NULL;
			switch_status_t status;

			/* API "status" is permitted. */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("status", "", NULL, &stream) == SWITCH_STATUS_SUCCESS);
			switch_safe_free(stream.data);

			/* JSON API "status" is NOT permitted (only the .api type was allowed) -> not found. */
			jcmd = cJSON_Parse("{\"command\":\"status\",\"data\":\"\"}");
			fst_requires(jcmd);
			status = switch_json_api_execute(jcmd, NULL, &reply);
			fst_check(status == SWITCH_STATUS_FALSE);
			if (reply) {
				cJSON_Delete(reply);
			}
			cJSON_Delete(jcmd);
		}
		FST_TEST_END()

		/* interface_allowlist_dump emits keys in the config format. It reflects module capabilities
		   (every interface the module registered), independent of what enforcement blocked -- that is
		   what makes it useful for generating a starting allowlist. */
		FST_TEST_BEGIN(interface_allowlist_dump_format)
		{
			switch_stream_handle_t stream = { 0 };

			/* Default: XML, one fully qualified entry per interface. */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("interface_allowlist_dump", "", NULL, &stream) == SWITCH_STATUS_SUCCESS);
			fst_requires(stream.data);
			fst_check(strstr((char *) stream.data, "<interface-allowlist>") != NULL);
			fst_check(strstr((char *) stream.data, "mod_commands.status.api") != NULL);
			fst_check(strstr((char *) stream.data, "mod_commands.status.json_api") != NULL);
			/* system is present in the dump even though it was blocked from registering. */
			fst_check(strstr((char *) stream.data, "mod_commands.system.api") != NULL);
			switch_safe_free(stream.data);

			/* "modules" variant: one entry per module. */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("interface_allowlist_dump", "modules", NULL, &stream) == SWITCH_STATUS_SUCCESS);
			fst_requires(stream.data);
			fst_check(strstr((char *) stream.data, "<allow name=\"mod_commands\"/>") != NULL);
			switch_safe_free(stream.data);

			/* "plain" variant: raw keys, no XML wrapper. */
			SWITCH_STANDARD_STREAM(stream);
			fst_check(switch_api_execute("interface_allowlist_dump", "plain", NULL, &stream) == SWITCH_STATUS_SUCCESS);
			fst_requires(stream.data);
			fst_check(strstr((char *) stream.data, "<interface-allowlist>") == NULL);
			fst_check(strstr((char *) stream.data, "mod_commands.version.api") != NULL);
			switch_safe_free(stream.data);
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

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
 * Seven Du <seven@signalwire.com>
 *
 *
 * test_sofia_funcs.c -- tests sofia functions
 *
 */

#include <switch.h>
#include <test/switch_test.h>
#include "../mod_sofia.c"

FST_MINCORE_BEGIN("./conf")

FST_SUITE_BEGIN(switch_hash)

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(test_protect_url)
{
	int ret;
	switch_caller_profile_t cp = { 0 };
	switch_core_new_memory_pool(&cp.pool);

	cp.destination_number = switch_core_strdup(cp.pool, "1234");
	ret = protect_dest_uri(&cp);
	fst_check(ret == 0);
	fst_check_string_equals(cp.destination_number, "1234");

	cp.destination_number = switch_core_strdup(cp.pool, "1234@ip");
	ret = protect_dest_uri(&cp);
	fst_check(ret == 0);
	fst_check_string_equals(cp.destination_number, "1234@ip");

	cp.destination_number = switch_core_strdup(cp.pool, "external/1234@ip");
	ret = protect_dest_uri(&cp);
	fst_check(ret == 0);
	fst_check_string_equals(cp.destination_number, "external/1234@ip");

	cp.destination_number = switch_core_strdup(cp.pool, "external/sip:1234@ip");
	ret = protect_dest_uri(&cp);
	fst_check(ret == 0);
	fst_check_string_equals(cp.destination_number, "external/1234@ip");

	cp.destination_number = switch_core_strdup(cp.pool, "external/sips:1234@ip");
	ret = protect_dest_uri(&cp);
	fst_check(ret == 0);
	fst_check_string_equals(cp.destination_number, "external/1234@ip");

	cp.destination_number = switch_core_strdup(cp.pool, "external/bryän&!杜金房@freeswitch-testing:9080");
	ret = protect_dest_uri(&cp);
	fst_check(ret == 1);
	fst_check_string_equals(cp.destination_number, "external/bry%C3%A4n%26!%E6%9D%9C%E9%87%91%E6%88%BF@freeswitch-testing:9080");

	cp.destination_number = switch_core_strdup(cp.pool, "external/" SWITCH_URL_UNSAFE "@freeswitch-testing:9080");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "before: [%s]\n", cp.destination_number);
	ret = protect_dest_uri(&cp);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "after: [%s]\n", cp.destination_number);
	fst_check(ret == 1);
	fst_check_string_equals(cp.destination_number, "external/%0D%0A%20%23%25%26%2B%3A%3B%3C%3D%3E%3F@[\\]^`{|}\"@freeswitch-testing:9080");

	switch_core_destroy_memory_pool(&cp.pool);
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

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
 * Andrey Volk <andrey@signalwire.com>
 *
 *
 * switch_core_db.c -- tests core db functions
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

FST_CORE_DB_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_core_db)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(test_switch_cache_db_execute_sql2str)
		{
			switch_cache_db_handle_t *dbh = NULL;
			char *dsn = "test_switch_cache_db_execute_sql2str.db";
			char res1[20] = "test";
			char res2[20] = "test";

			if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) == SWITCH_STATUS_SUCCESS) {

				switch_cache_db_execute_sql2str(dbh, "SELECT 1", (char *)&res1, 20, NULL);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SELECT 1: %s\n", switch_str_nil(res1));

				switch_cache_db_execute_sql2str(dbh, "SELECT NULL", (char *)&res2, 20, NULL);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SELECT NULL: %s\n", switch_str_nil(res2));

			}

			fst_check_string_equals(res1, "1");
			fst_check_string_equals(res2, "");
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

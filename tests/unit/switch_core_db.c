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

int max_rows = 150;

int status = 0;

int table_count_func(void *pArg, int argc, char **argv, char **columnNames){
	if (argc > 0) {
		status = atoi(argv[0]);
	}

	return -1;
}

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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SELECT 1: %s\n", res1);

				switch_cache_db_execute_sql2str(dbh, "SELECT NULL", (char *)&res2, 20, NULL);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SELECT NULL: %s\n", res2);

			}

			fst_check_string_equals(res1, "1");
			fst_check_string_equals(res2, "");
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_cache_db_queue_manager_race)
		{
			int i;
			switch_sql_queue_manager_t *qm = NULL;

			switch_sql_queue_manager_init_name("TEST",
				&qm,
				4,
				"test_switch_cache_db_queue_manager_race",
				SWITCH_MAX_TRANS,
				NULL, NULL, NULL, NULL);

			switch_sql_queue_manager_start(qm);

			switch_sql_queue_manager_push_confirm(qm, "DROP TABLE IF EXISTS t;", 0, SWITCH_TRUE);
			switch_sql_queue_manager_push_confirm(qm, "CREATE TABLE t (col1 INT);", 0, SWITCH_TRUE);

			for (i = 0; i < max_rows; i++) {
				switch_sql_queue_manager_push(qm, "INSERT INTO t (col1) VALUES (1);", 0, SWITCH_TRUE);
			}

			switch_sleep(1 * 1000 * 1000);

			switch_sql_queue_manager_execute_sql_callback(qm, "SELECT COUNT(col1) FROM t;", table_count_func, NULL);

			while (switch_sql_queue_manager_size(qm, 0)) {
				switch_cond_next();
			}

			switch_sql_queue_manager_stop(qm);
			switch_sql_queue_manager_destroy(&qm);

			fst_check_int_equals(status, max_rows);
		}
		FST_TEST_END()


	}
	FST_SUITE_END()
}
FST_CORE_END()

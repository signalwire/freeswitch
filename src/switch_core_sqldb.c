/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_sqldb.c -- Main Core Library (statistics tracker)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

static struct {
	switch_core_db_t *db;
	switch_core_db_t *event_db;
	switch_queue_t *sql_queue;
	switch_memory_pool_t *memory_pool;
	int thread_running;
} sql_manager;

static switch_status_t switch_core_db_persistant_execute_trans(switch_core_db_t *db, char *sql, uint32_t retries)
{
	char *errmsg;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;
	unsigned begin_retries = 100;
	uint8_t again = 0;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

again:

	while (begin_retries > 0) {
		again = 0;

		switch_core_db_exec(db, "begin transaction", NULL, NULL, &errmsg);

		if (errmsg) {
			begin_retries--;
			if (strstr(errmsg, "cannot start a transaction within a transaction")) {
				again = 1;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL Retry [%s]\n", errmsg);
			}
			switch_core_db_free(errmsg);
			errmsg = NULL;

			if (again) {
				switch_core_db_exec(db, "end transaction", NULL, NULL, NULL);
				goto again;
			}

			switch_yield(100000);

			if (begin_retries == 0) {
				goto done;
			}
		} else {
			break;
		}

	}

	while (retries > 0) {
		switch_core_db_exec(db, sql, NULL, NULL, &errmsg);
		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", errmsg);
			switch_core_db_free(errmsg);
			errmsg = NULL;
			switch_yield(100000);
			retries--;
			if (retries == 0 && forever) {
				retries = 1000;
				continue;
			}
		} else {
			status = SWITCH_STATUS_SUCCESS;
			break;
		}
	}

done:

	switch_core_db_exec(db, "end transaction", NULL, NULL, NULL);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_db_persistant_execute(switch_core_db_t *db, char *sql, uint32_t retries)
{
	char *errmsg;
	switch_status_t status = SWITCH_STATUS_FALSE;
	uint8_t forever = 0;

	if (!retries) {
		forever = 1;
		retries = 1000;
	}

	while (retries > 0) {
		switch_core_db_exec(db, sql, NULL, NULL, &errmsg);
		if (errmsg) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR [%s]\n", errmsg);
			switch_core_db_free(errmsg);
			switch_yield(100000);
			retries--;
			if (retries == 0 && forever) {
				retries = 1000;
				continue;
			}
		} else {
			status = SWITCH_STATUS_SUCCESS;
			break;
		}
	}

	return status;
}

#define SQLLEN 1024 * 64
static void *SWITCH_THREAD_FUNC switch_core_sql_thread(switch_thread_t * thread, void *obj)
{
	void *pop;
	uint32_t itterations = 0;
	uint8_t trans = 0, nothing_in_queue = 0;
	uint32_t target = 1000;
	switch_size_t len = 0, sql_len = SQLLEN;
	char *tmp, *sqlbuf = (char *) malloc(sql_len);
	char *sql;
	switch_size_t newlen;
	int lc = 0;

	switch_assert(sqlbuf);

	if (!sql_manager.event_db) {
		sql_manager.event_db = switch_core_db_handle();
	}

	sql_manager.thread_running = 1;

	for (;;) {
		if (switch_queue_trypop(sql_manager.sql_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			sql = (char *) pop;

			if (sql) {
				newlen = strlen(sql) + 2;

				if (itterations == 0) {
					trans = 1;
				}

				/* ignore abnormally large strings sql strings as potential buffer overflow */
				if (newlen < SQLLEN) {
					itterations++;
					if (len + newlen > sql_len) {
						sql_len = len + SQLLEN;
						if (!(tmp = realloc(sqlbuf, sql_len))) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL thread ending on mem err\n");
							abort();
							break;
						}
						sqlbuf = tmp;
					}
					sprintf(sqlbuf + len, "%s;\n", sql);
					len += newlen;

				}
				switch_core_db_free(sql);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "SQL thread ending\n");
				break;
			}
		} else {
			nothing_in_queue = 1;
		}


		if (trans && ((itterations == target) || nothing_in_queue)) {
			if (switch_core_db_persistant_execute_trans(sql_manager.event_db, sqlbuf, 100) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "SQL thread unable to commit transaction, records lost!\n");
			}
			itterations = 0;
			trans = 0;
			nothing_in_queue = 0;
			len = 0;
			*sqlbuf = '\0';
		}

		if (++lc == 300000) {
			switch_core_db_exec(sql_manager.db, "vacuum;", NULL, NULL, NULL);
			lc = 0;
		}

		if (nothing_in_queue) {
			switch_cond_next();
		}
	}

	while (switch_queue_trypop(sql_manager.sql_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		free(pop);
	}

	free(sqlbuf);
	return NULL;
}

static void core_event_handler(switch_event_t *event)
{
	char *sql = NULL;

	switch_assert(event);

	switch (event->event_id) {
	case SWITCH_EVENT_ADD_SCHEDULE:
		sql = switch_mprintf("insert into tasks values('%q','%q','%q','%q')",
							 switch_event_get_header_nil(event, "task-id"),
							 switch_event_get_header_nil(event, "task-desc"),
							 switch_event_get_header_nil(event, "task-group"), 
							 switch_event_get_header_nil(event, "task-sql_manager")
							 );
		break;
	case SWITCH_EVENT_DEL_SCHEDULE:
	case SWITCH_EVENT_EXE_SCHEDULE:
		sql = switch_mprintf("delete from tasks where task_id=%q", switch_event_get_header_nil(event, "task-id"));
		break;
	case SWITCH_EVENT_RE_SCHEDULE:
		sql = switch_mprintf("update tasks set task_sql_manager='%q' where task_id=%q",
							 switch_event_get_header_nil(event, "task-sql_manager"), switch_event_get_header_nil(event, "task-id"));
		break;
	case SWITCH_EVENT_CHANNEL_DESTROY:
		sql = switch_mprintf("delete from channels where uuid='%q'", switch_event_get_header_nil(event, "unique-id"));
		break;
	case SWITCH_EVENT_CHANNEL_UUID:
		{
			sql = switch_mprintf(
								 "update channels set uuid='%q' where uuid='%q';"
								 "update calls set caller_uuid='%q' where caller_uuid='%q';"
								 "update calls set callee_uuid='%q' where callee_uuid='%q'",
								 switch_event_get_header_nil(event, "unique-id"),
								 switch_event_get_header_nil(event, "old-unique-id"),
								 switch_event_get_header_nil(event, "unique-id"),
								 switch_event_get_header_nil(event, "old-unique-id"),
								 switch_event_get_header_nil(event, "unique-id"),
								 switch_event_get_header_nil(event, "old-unique-id")
								 );
			break;
		}
	case SWITCH_EVENT_CHANNEL_CREATE:
		sql = switch_mprintf("insert into channels (uuid,created,created_epoch, name,state,dialplan,context) values('%q','%q','%ld','%q','%q','%q','%q')",
							 switch_event_get_header_nil(event, "unique-id"),
							 switch_event_get_header_nil(event, "event-date-local"),
							 (long)switch_timestamp(NULL),							 
							 switch_event_get_header_nil(event, "channel-name"),
							 switch_event_get_header_nil(event, "channel-state"),
							 switch_event_get_header_nil(event, "caller-dialplan"),
							 switch_event_get_header_nil(event, "caller-context")
			);
		break;
	case SWITCH_EVENT_CODEC:
		sql =
			switch_mprintf
			("update channels set read_codec='%q',read_rate='%q',write_codec='%q',write_rate='%q' where uuid='%q'",
			 switch_event_get_header_nil(event, "channel-read-codec-name"), switch_event_get_header_nil(event,
																								"channel-read-codec-rate"),
			 switch_event_get_header_nil(event, "channel-write-codec-name"), switch_event_get_header_nil(event,
																								 "channel-write-codec-rate"),
			 switch_event_get_header_nil(event, "unique-id"));
		break;
	case SWITCH_EVENT_CHANNEL_EXECUTE:
		sql = switch_mprintf("update channels set application='%q',application_data='%q' where uuid='%q'",
							 switch_event_get_header_nil(event, "application"),
							 switch_event_get_header_nil(event, "application-data"), switch_event_get_header_nil(event, "unique-id")
			);
		break;
	case SWITCH_EVENT_CHANNEL_STATE:
		{
			char *state = switch_event_get_header_nil(event, "channel-state-number");
			switch_channel_state_t state_i = CS_DONE;

			if (!switch_strlen_zero(state)) {
				state_i = atoi(state);
			}

			switch (state_i) {
			case CS_HANGUP:
			case CS_DONE:
				break;
			case CS_ROUTING:
				sql = switch_mprintf("update channels set state='%s',cid_name='%q',cid_num='%q',ip_addr='%s',dest='%q',dialplan='%q',context='%q' "
									 "where uuid='%s'",
									 switch_event_get_header_nil(event, "channel-state"),
									 switch_event_get_header_nil(event, "caller-caller-id-name"),
									 switch_event_get_header_nil(event, "caller-caller-id-number"),
									 switch_event_get_header_nil(event, "caller-network-addr"),
									 switch_event_get_header_nil(event, "caller-destination-number"), 
									 switch_event_get_header_nil(event, "caller-dialplan"), 
									 switch_event_get_header_nil(event, "caller-context"), 
									 switch_event_get_header_nil(event, "unique-id"));
				break;
			default:
				sql = switch_mprintf("update channels set state='%s' where uuid='%s'",
									 switch_event_get_header_nil(event, "channel-state"), switch_event_get_header_nil(event, "unique-id"));
				break;
			}
			break;
		}
	case SWITCH_EVENT_CHANNEL_BRIDGE:
		sql = switch_mprintf("insert into calls values ('%s', '%ld', '%s','%q','%q','%q','%q','%s','%q','%q','%q','%q','%s')",
							 switch_event_get_header_nil(event, "event-date-local"),
							 (long)switch_timestamp(NULL),
							 switch_event_get_header_nil(event, "event-calling-function"),
							 switch_event_get_header_nil(event, "caller-caller-id-name"),
							 switch_event_get_header_nil(event, "caller-caller-id-number"),
							 switch_event_get_header_nil(event, "caller-destination-number"),
							 switch_event_get_header_nil(event, "caller-channel-name"),
							 switch_event_get_header_nil(event, "caller-unique-id"),
							 switch_event_get_header_nil(event, "Other-Leg-caller-id-name"),
							 switch_event_get_header_nil(event, "Other-Leg-caller-id-number"),
							 switch_event_get_header_nil(event, "Other-Leg-destination-number"),
							 switch_event_get_header_nil(event, "Other-Leg-channel-name"), switch_event_get_header_nil(event, "Other-Leg-unique-id")
			);
		break;
	case SWITCH_EVENT_CHANNEL_UNBRIDGE:
		sql = switch_mprintf("delete from calls where caller_uuid='%s'", switch_event_get_header_nil(event, "caller-unique-id"));
		break;
	case SWITCH_EVENT_SHUTDOWN:
		sql = switch_mprintf("delete from channels;delete from interfaces;delete from calls");
		break;
	case SWITCH_EVENT_LOG:
		return;
	case SWITCH_EVENT_MODULE_LOAD:
		{
			const char *type = switch_event_get_header_nil(event, "type");
			const char *name = switch_event_get_header_nil(event, "name");
			const char *description = switch_event_get_header_nil(event, "description");
			const char *syntax = switch_event_get_header_nil(event, "syntax");
			if (!switch_strlen_zero(type) && !switch_strlen_zero(name)) {
				sql =
					switch_mprintf("insert into interfaces (type,name,description,syntax) values('%q','%q','%q','%q')",
								   type, name, switch_str_nil(description), switch_str_nil(syntax)
					);
			}
			break;
		}
	case SWITCH_EVENT_MODULE_UNLOAD:
		{
			const char *type = switch_event_get_header_nil(event, "type");
			const char *name = switch_event_get_header_nil(event, "name");
			if (!switch_strlen_zero(type) && !switch_strlen_zero(name)) {
				sql = switch_mprintf("delete from interfaces where type='%q' and name='%q'", type, name);
			}
			break;
		}
	default:
		break;
	}

	if (sql) {
		switch_queue_push(sql_manager.sql_queue, sql);
		sql = NULL;
	}
}

void switch_core_sqldb_start(switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;;

	sql_manager.memory_pool = pool;

	/* Activate SQL database */
	if ((sql_manager.db = switch_core_db_handle()) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB!\n");
		switch_clear_flag((&runtime), SCF_USE_SQL);
	} else {
		char create_complete_sql[] =
			"CREATE TABLE complete (\n"
			"   sticky  INTEGER,\n" 
			"   a1  VARCHAR(255),\n" 
			"   a2  VARCHAR(255),\n" 
			"   a3  VARCHAR(255),\n" 
			"   a4  VARCHAR(255),\n" 
			"   a5  VARCHAR(255),\n" 
			"   a6  VARCHAR(255),\n" 
			"   a7  VARCHAR(255),\n" 
			"   a8  VARCHAR(255),\n" 
			"   a9  VARCHAR(255),\n" 
			"   a10 VARCHAR(255)\n" 
			");\n";

		char create_alias_sql[] =
			"CREATE TABLE aliases (\n"
			"   sticky  INTEGER,\n" 
			"   alias  VARCHAR(255),\n" 
			"   command  VARCHAR(255)\n" 
			");\n";

		char create_channels_sql[] =
			"CREATE TABLE channels (\n"
			"   uuid  VARCHAR(255),\n"
			"   created  VARCHAR(255),\n"
			"   created_epoch  INTEGER,\n"
			"   name  VARCHAR(255),\n"
			"   state  VARCHAR(255),\n"
			"   cid_name  VARCHAR(255),\n"
			"   cid_num  VARCHAR(255),\n"
			"   ip_addr  VARCHAR(255),\n"
			"   dest  VARCHAR(255),\n"
			"   application  VARCHAR(255),\n"
			"   application_data  VARCHAR(255),\n"
			"   dialplan VARCHAR(255),\n"
			"   context VARCHAR(255),\n"
			"   read_codec  VARCHAR(255),\n" 
			"   read_rate  VARCHAR(255),\n" 
			"   write_codec  VARCHAR(255),\n" 
			"   write_rate  VARCHAR(255)\n" 
			");\n";
		char create_calls_sql[] =
			"CREATE TABLE calls (\n"
			"   created  VARCHAR(255),\n"
			"   created_epoch  INTEGER,\n"
			"   function  VARCHAR(255),\n"
			"   caller_cid_name  VARCHAR(255),\n"
			"   caller_cid_num   VARCHAR(255),\n"
			"   caller_dest_num  VARCHAR(255),\n"
			"   caller_chan_name VARCHAR(255),\n"
			"   caller_uuid      VARCHAR(255),\n"
			"   callee_cid_name  VARCHAR(255),\n"
			"   callee_cid_num   VARCHAR(255),\n"
			"   callee_dest_num  VARCHAR(255),\n" 
			"   callee_chan_name VARCHAR(255),\n" 
			"   callee_uuid      VARCHAR(255)\n" 
			");\n";
		char create_interfaces_sql[] =
			"CREATE TABLE interfaces (\n"
			"   type             VARCHAR(255),\n"
			"   name             VARCHAR(255),\n" 
			"   description      VARCHAR(255),\n" 
			"   syntax           VARCHAR(255)\n" 
			");\n";
		char create_tasks_sql[] =
			"CREATE TABLE tasks (\n"
			"   task_id             INTEGER(4),\n"
			"   task_desc           VARCHAR(255),\n" 
			"   task_group          VARCHAR(255),\n" 
			"   task_sql_manager    INTEGER(8)\n" 
			");\n";

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening DB\n");
		switch_core_db_exec(sql_manager.db, "drop table channels", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "drop table calls", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "drop table interfaces", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "drop table tasks", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "PRAGMA synchronous=OFF;", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "PRAGMA count_changes=OFF;", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "PRAGMA cache_size=8000", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "PRAGMA temp_store=MEMORY;", NULL, NULL, NULL);

		switch_core_db_test_reactive(sql_manager.db, "select sticky from complete", "DROP TABLE complete", create_complete_sql);
		switch_core_db_test_reactive(sql_manager.db, "select sticky from aliases", "DROP TABLE aliases", create_alias_sql);
		switch_core_db_exec(sql_manager.db, "delete from complete where sticky=0", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, "delete from aliases where sticky=0", NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, create_channels_sql, NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, create_calls_sql, NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, create_interfaces_sql, NULL, NULL, NULL);
		switch_core_db_exec(sql_manager.db, create_tasks_sql, NULL, NULL, NULL);
		if (switch_event_bind("core_db", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, core_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event handler!\n");
		}
	}

	switch_queue_create(&sql_manager.sql_queue, SWITCH_SQL_QUEUE_LEN, sql_manager.memory_pool);

	switch_threadattr_create(&thd_attr, sql_manager.memory_pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, switch_core_sql_thread, NULL, sql_manager.memory_pool);
	while (!sql_manager.thread_running) {
		switch_yield(10000);
	}
}

void switch_core_sqldb_stop(void)
{
	switch_queue_push(sql_manager.sql_queue, NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Waiting for unfinished SQL transactions\n");
	while (switch_queue_size(sql_manager.sql_queue) > 0) {
		switch_yield(10000);
	}

	switch_core_db_close(sql_manager.db);
	switch_core_db_close(sql_manager.event_db);

}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * 
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 *
 * mod_callcenter.c -- Call Center Module
 *
 */
#include <switch.h>

#define CALLCENTER_EVENT "callcenter::info"

#define CC_AGENT_TYPE_CALLBACK "Callback"

/* TODO
   drop caller if no agent login
   dont allow new caller
 */
/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_callcenter_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_callcenter_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_callcenter_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_callcenter, mod_callcenter_load, mod_callcenter_shutdown, NULL);

static const char *global_cf = "callcenter.conf";

typedef enum {
	CC_STATUS_SUCCESS,
	CC_STATUS_FALSE,
	CC_STATUS_AGENT_NOT_FOUND,
	CC_STATUS_QUEUE_NOT_FOUND,
	CC_STATUS_AGENT_ALREADY_EXIST,
	CC_STATUS_AGENT_INVALID_TYPE,
	CC_STATUS_AGENT_INVALID_STATUS,
	CC_STATUS_AGENT_INVALID_STATE,
	CC_STATUS_TIER_ALREADY_EXIST,
	CC_STATUS_TIER_NOT_FOUND,
	CC_STATUS_TIER_INVALID_STATE,
	CC_STATUS_INVALID_KEY
} cc_status_t;

typedef enum {
	CC_TIER_STATE_UNKNOWN = 0,
	CC_TIER_STATE_NO_ANSWER = 1,
	CC_TIER_STATE_READY = 2,
	CC_TIER_STATE_OFFERING = 3,
	CC_TIER_STATE_ACTIVE_INBOUND = 4,
	CC_TIER_STATE_STANDBY = 5
} cc_tier_state_t;

struct cc_tier_state_table {
	const char *name;
	cc_tier_state_t state;
};

static struct cc_tier_state_table STATE_CHART[] = {
	{"Unknown", CC_TIER_STATE_UNKNOWN},
	{"No Answer", CC_TIER_STATE_NO_ANSWER},
	{"Ready", CC_TIER_STATE_READY},
	{"Offering", CC_TIER_STATE_OFFERING},
	{"Active Inbound", CC_TIER_STATE_ACTIVE_INBOUND},
	{"Standby", CC_TIER_STATE_STANDBY},
	{NULL, 0}

};

typedef enum {
	CC_AGENT_STATUS_UNKNOWN = 0,
	CC_AGENT_STATUS_LOGGED_OUT = 1,
	CC_AGENT_STATUS_AVAILABLE = 2,
	CC_AGENT_STATUS_AVAILABLE_ON_DEMAND = 3,
	CC_AGENT_STATUS_ON_BREAK = 4
} cc_agent_status_t;

struct cc_agent_status_table {
	const char *name;
	cc_agent_status_t status;
};

static struct cc_agent_status_table AGENT_STATUS_CHART[] = {
	{"Unknown", CC_AGENT_STATUS_UNKNOWN},
	{"Logged Out", CC_AGENT_STATUS_LOGGED_OUT},
	{"Available", CC_AGENT_STATUS_AVAILABLE},
	{"Available (On Demand)", CC_AGENT_STATUS_AVAILABLE_ON_DEMAND},
	{"On Break", CC_AGENT_STATUS_ON_BREAK},
	{NULL, 0}

};

typedef enum {
	CC_AGENT_STATE_UNKNOWN = 0,
	CC_AGENT_STATE_WAITING = 1,
	CC_AGENT_STATE_RECEIVING = 2,
	CC_AGENT_STATE_IN_A_QUEUE_CALL = 3,
	CC_AGENT_STATE_IDLE = 4
} cc_agent_state_t;

struct cc_agent_state_table {
	const char *name;
	cc_agent_state_t state;
};

static struct cc_agent_state_table AGENT_STATE_CHART[] = {
	{"Unknown", CC_AGENT_STATE_UNKNOWN},
	{"Waiting", CC_AGENT_STATE_WAITING},
	{"Receiving", CC_AGENT_STATE_RECEIVING},
	{"In a queue call", CC_AGENT_STATE_IN_A_QUEUE_CALL},
	{"Idle", CC_AGENT_STATE_IDLE},
	{NULL, 0}

};

typedef enum {
	CC_MEMBER_STATE_UNKNOWN = 0,
	CC_MEMBER_STATE_WAITING = 1,
	CC_MEMBER_STATE_TRYING = 2,
	CC_MEMBER_STATE_ANSWERED = 3,
} cc_member_state_t;

struct cc_member_state_table {
	const char *name;
	cc_member_state_t state;
};

static struct cc_member_state_table MEMBER_STATE_CHART[] = {
	{"Unknown", CC_MEMBER_STATE_UNKNOWN},
	{"Waiting", CC_MEMBER_STATE_WAITING},
	{"Trying", CC_MEMBER_STATE_TRYING},
	{"Answered", CC_MEMBER_STATE_ANSWERED},
	{NULL, 0}

};

/*static char queues_sql[] =
  "CREATE TABLE queues (\n"
  "   name	  VARCHAR(255)\n" ");\n";
 */
static char members_sql[] =
"CREATE TABLE members (\n"
"   queue	   VARCHAR(255),\n"
"   system	   VARCHAR(255),\n"
"   uuid	   VARCHAR(255) NOT NULL DEFAULT '',\n"
"   caller_number  VARCHAR(255),\n"
"   caller_name    VARCHAR(255),\n"
"   system_epoch   INTEGER NOT NULL DEFAULT 0,\n"
"   joined_epoch   INTEGER NOT NULL DEFAULT 0,\n"
"   bridge_epoch   INTEGER NOT NULL DEFAULT 0,\n"
"   base_score     INTEGER NOT NULL DEFAULT 0,\n"
"   skill_score    INTEGER NOT NULL DEFAULT 0,\n"
"   serving_agent  VARCHAR(255),\n"
"   serving_system VARCHAR(255),\n"
"   state	   VARCHAR(255)\n" ");\n";
/* Member State 
   Waiting
   Answered
 */

static char agents_sql[] =
"CREATE TABLE agents (\n"
"   name      VARCHAR(255),\n"
"   system    VARCHAR(255),\n"
"   uuid      VARCHAR(255),\n"
"   type      VARCHAR(255),\n" // Callback , Dial in...
"   contact   VARCHAR(255),\n"
"   status    VARCHAR(255),\n"
/*User Personal Status
  Available
  On Break
  Logged Out
 */
"   state   VARCHAR(255),\n"
/* User Personal State
   Waiting
   Receiving
   In a queue call
 */

"   max_no_answer INTEGER NOT NULL DEFAULT 0,\n"
"   wrap_up_time INTEGER NOT NULL DEFAULT 0,\n"
"   last_bridge_start INTEGER NOT NULL DEFAULT 0,\n"
"   last_bridge_end INTEGER NOT NULL DEFAULT 0,\n"
"   last_offered_call INTEGER NOT NULL DEFAULT 0,\n" 
"   last_status_change INTEGER NOT NULL DEFAULT 0,\n"
"   no_answer_count INTEGER NOT NULL DEFAULT 0,\n"
"   calls_answered  INTEGER NOT NULL DEFAULT 0,\n"
"   talk_time  INTEGER NOT NULL DEFAULT 0\n"
");\n";


static char tiers_sql[] =
"CREATE TABLE tiers (\n"
"   queue    VARCHAR(255),\n"
"   agent    VARCHAR(255),\n"
"   state    VARCHAR(255),\n"
/*
   Agent State: 
   Ready
   Active inbound
   Wrap-up inbound
   Standby
   No Answer
   Offering
 */
"   level    INTEGER NOT NULL DEFAULT 1,\n"
"   position INTEGER NOT NULL DEFAULT 1\n" ");\n";


const char * cc_tier_state2str(cc_tier_state_t state)
{
	uint8_t x;
	const char *str = "Unknown";

	for (x = 0; x < (sizeof(STATE_CHART) / sizeof(struct cc_tier_state_table)) - 1; x++) {
		if (STATE_CHART[x].state == state) {
			str = STATE_CHART[x].name;
			break;
		}
	}

	return str;
}

cc_tier_state_t cc_tier_str2state(const char *str)
{
	uint8_t x;
	cc_tier_state_t state = CC_TIER_STATE_UNKNOWN;

	for (x = 0; x < (sizeof(STATE_CHART) / sizeof(struct cc_tier_state_table)) - 1 && STATE_CHART[x].name; x++) {
		if (!strcasecmp(STATE_CHART[x].name, str)) {
			state = STATE_CHART[x].state;
			break;
		}
	}
	return state;
}

const char * cc_agent_status2str(cc_agent_status_t status)
{
	uint8_t x;
	const char *str = "Unknown";

	for (x = 0; x < (sizeof(AGENT_STATUS_CHART) / sizeof(struct cc_agent_status_table)) - 1; x++) {
		if (AGENT_STATUS_CHART[x].status == status) {
			str = AGENT_STATUS_CHART[x].name;
			break;
		}
	}

	return str;
}

cc_agent_status_t cc_agent_str2status(const char *str)
{
	uint8_t x;
	cc_agent_status_t status = CC_AGENT_STATUS_UNKNOWN;

	for (x = 0; x < (sizeof(AGENT_STATUS_CHART) / sizeof(struct cc_agent_status_table)) - 1 && AGENT_STATUS_CHART[x].name; x++) {
		if (!strcasecmp(AGENT_STATUS_CHART[x].name, str)) {
			status = AGENT_STATUS_CHART[x].status;
			break;
		}
	}
	return status;
}

const char * cc_agent_state2str(cc_agent_state_t state)
{
	uint8_t x;
	const char *str = "Unknown";

	for (x = 0; x < (sizeof(AGENT_STATE_CHART) / sizeof(struct cc_agent_state_table)) - 1; x++) {
		if (AGENT_STATE_CHART[x].state == state) {
			str = AGENT_STATE_CHART[x].name;
			break;
		}
	}

	return str;
}

cc_agent_state_t cc_agent_str2state(const char *str)
{
	uint8_t x;
	cc_agent_state_t state = CC_AGENT_STATE_UNKNOWN;

	for (x = 0; x < (sizeof(AGENT_STATE_CHART) / sizeof(struct cc_agent_state_table)) - 1 && AGENT_STATE_CHART[x].name; x++) {
		if (!strcasecmp(AGENT_STATE_CHART[x].name, str)) {
			state = AGENT_STATE_CHART[x].state;
			break;
		}
	}
	return state;
}

const char * cc_member_state2str(cc_member_state_t state)
{
	uint8_t x;
	const char *str = "Unknown";

	for (x = 0; x < (sizeof(MEMBER_STATE_CHART) / sizeof(struct cc_member_state_table)) - 1; x++) {
		if (MEMBER_STATE_CHART[x].state == state) {
			str = MEMBER_STATE_CHART[x].name;
			break;
		}
	}

	return str;
}

cc_member_state_t cc_member_str2state(const char *str)
{
	uint8_t x;
	cc_member_state_t state = CC_MEMBER_STATE_UNKNOWN;

	for (x = 0; x < (sizeof(MEMBER_STATE_CHART) / sizeof(struct cc_member_state_table)) - 1 && MEMBER_STATE_CHART[x].name; x++) {
		if (!strcasecmp(MEMBER_STATE_CHART[x].name, str)) {
			state = MEMBER_STATE_CHART[x].state;
			break;
		}
	}
	return state;
}


typedef enum {
	PFLAG_DESTROY = 1 << 0
} cc_flags_t;

static struct {
	switch_hash_t *queue_hash;
	int debug;
	int32_t threads;
	int32_t running;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
} globals;

#define CC_QUEUE_CONFIGITEM_COUNT 100

struct cc_queue {
	char *name;

	char *odbc_dsn;
	char *odbc_user;
	char *odbc_pass;

	char *strategy;
	char *moh;
	char *record_template;
	char *time_base_score;
	switch_mutex_t *mutex;

	switch_thread_rwlock_t *rwlock;
	switch_memory_pool_t *pool;
	uint32_t flags;

	switch_xml_config_item_t config[CC_QUEUE_CONFIGITEM_COUNT];
	switch_xml_config_string_options_t config_str_pool;

};
typedef struct cc_queue cc_queue_t;

static void free_queue(cc_queue_t *queue)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying Profile %s\n", queue->name);
	switch_core_destroy_memory_pool(&queue->pool);
}

static void queue_rwunlock(cc_queue_t *queue)
{
	switch_thread_rwlock_unlock(queue->rwlock);
	if (switch_test_flag(queue, PFLAG_DESTROY)) {
		if (switch_thread_rwlock_trywrlock(queue->rwlock) == SWITCH_STATUS_SUCCESS) {
			switch_thread_rwlock_unlock(queue->rwlock);
			free_queue(queue);
		}
	}
}

static void destroy_queue(const char *queue_name, switch_bool_t block)
{
	cc_queue_t *queue = NULL;
	switch_mutex_lock(globals.mutex);
	if ((queue = switch_core_hash_find(globals.queue_hash, queue_name))) {
		switch_core_hash_delete(globals.queue_hash, queue_name);
	}
	switch_mutex_unlock(globals.mutex);

	if (!queue) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[%s] Invalid queue\n", queue_name);
		return;
	}

	if (block) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] Waiting for write lock\n", queue->name);
		switch_thread_rwlock_wrlock(queue->rwlock);
	} else {
		if (switch_thread_rwlock_trywrlock(queue->rwlock) != SWITCH_STATUS_SUCCESS) {
			/* Lock failed, set the destroy flag so it'll be destroyed whenever its not in use anymore */
			switch_set_flag(queue, PFLAG_DESTROY);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] queue is in use, memory will be freed whenever its no longer in use\n",
					queue->name);
			return;
		}
	}

	free_queue(queue);
}


switch_cache_db_handle_t *cc_get_db_handle(cc_queue_t *queue)
{
	switch_cache_db_connection_options_t options = { {0} };
	switch_cache_db_handle_t *dbh = NULL;

	if (queue && !zstr(queue->odbc_dsn)) {
		options.odbc_options.dsn = queue->odbc_dsn;
		options.odbc_options.user = queue->odbc_user;
		options.odbc_options.pass = queue->odbc_pass;

		if (switch_cache_db_get_db_handle(&dbh, SCDB_TYPE_ODBC, &options) != SWITCH_STATUS_SUCCESS)
			dbh = NULL;
		return dbh;
	} else {
		options.core_db_options.db_path = "callcenter";
		if (switch_cache_db_get_db_handle(&dbh, SCDB_TYPE_CORE_DB, &options) != SWITCH_STATUS_SUCCESS)
			dbh = NULL;
		return dbh;
	}
}
/*!
 * \brief Sets the queue's configuration instructions 
 */
cc_queue_t *queue_set_config(cc_queue_t *queue)
{
	int i = 0;

	queue->config_str_pool.pool = queue->pool;

	/*
	   SWITCH _CONFIG_SET_ITEM(item, "key", type, flags, 
	   pointer, default, options, help_syntax, help_description)
	 */
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "strategy", SWITCH_CONFIG_STRING, 0, &queue->strategy, "longest-idle-agent", &queue->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "moh-sound", SWITCH_CONFIG_STRING, 0, &queue->moh, NULL, &queue->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "record-template", SWITCH_CONFIG_STRING, 0, &queue->record_template, NULL, &queue->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "time-base-score", SWITCH_CONFIG_STRING, 0, &queue->time_base_score, "queue", &queue->config_str_pool, NULL, NULL);

	switch_assert(i < CC_QUEUE_CONFIGITEM_COUNT);

	return queue;

}


char *cc_execute_sql2str(cc_queue_t *queue, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;

	switch_cache_db_handle_t *dbh = NULL;

	if (!(dbh = cc_get_db_handle(queue))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		return NULL;
	}

	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, NULL);

	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	switch_cache_db_release_db_handle(&dbh);

	return ret;
}

static switch_status_t cc_execute_sql(cc_queue_t *queue, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	if (!(dbh = cc_get_db_handle(queue))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	return status;
}

static switch_bool_t cc_execute_sql_callback(cc_queue_t *queue, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	if (!(dbh = cc_get_db_handle(queue))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	return ret;
}

static cc_queue_t *load_queue(const char *queue_name)
{
	cc_queue_t *queue = NULL;
	switch_xml_t x_queues, x_queue, cfg, xml;
	switch_event_t *event = NULL;
	switch_cache_db_handle_t *dbh = NULL;
	char *sql = NULL;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return queue;
	}
	if (!(x_queues = switch_xml_child(cfg, "queues"))) {
		goto end;
	}

	if ((x_queue = switch_xml_find_child(x_queues, "queue", "name", queue_name))) {
		switch_memory_pool_t *pool;
		int count;

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
			goto end;
		}

		if (!(queue = switch_core_alloc(pool, sizeof(cc_queue_t)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
			switch_core_destroy_memory_pool(&pool);
			goto end;
		}

		queue->pool = pool;
		queue_set_config(queue);

		/* Add the params to the event structure */
		count = switch_event_import_xml(switch_xml_child(x_queue, "param"), "name", "value", &event);

		if (switch_xml_config_parse_event(event, count, SWITCH_FALSE, queue->config) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to process configuration\n");
			switch_core_destroy_memory_pool(&pool);
			goto end;
		}

		switch_thread_rwlock_create(&queue->rwlock, pool);
		queue->name = switch_core_strdup(pool, queue_name);

		if (!zstr(queue->odbc_dsn)) {
			if ((queue->odbc_user = strchr(queue->odbc_dsn, ':'))) {
				*(queue->odbc_user++) = '\0';
				if ((queue->odbc_pass = strchr(queue->odbc_user, ':'))) {
					*(queue->odbc_pass++) = '\0';
				}
			}
		}

		if (!(dbh = cc_get_db_handle(queue))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot open DB!\n");
			goto end;
		}

		//		switch_cache_db_test_reactive(dbh, "select count(name) from queues", "drop table queues", queues_sql);
		switch_cache_db_test_reactive(dbh, "select count(system_epoch) from members", "drop table members", members_sql);
		switch_cache_db_test_reactive(dbh, "select count(max_no_answer) from agents", "drop table agents", agents_sql);
		switch_cache_db_test_reactive(dbh, "select count(queue) from tiers", "drop table tiers" , tiers_sql);
		switch_mutex_init(&queue->mutex, SWITCH_MUTEX_NESTED, queue->pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added queue %s\n", queue->name);
		switch_core_hash_insert(globals.queue_hash, queue->name, queue);

		/* Reset a unclean shutdown */
		sql = switch_mprintf("UPDATE agents SET state = 'Waiting', uuid = '' WHERE system = 'single_box';"
				"UPDATE tiers SET state = 'Ready' WHERE agent IN (SELECT name FROM agents WHERE system = 'single_box');"
				"DELETE FROM members WHERE system = 'single_box';");
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);
	}

end:

	if (xml) {
		switch_xml_free(xml);
	}
	if (event) {
		switch_event_destroy(&event);
	}
	return queue;
}

static cc_queue_t *get_queue(const char *queue_name)
{
	cc_queue_t *queue = NULL;

	switch_mutex_lock(globals.mutex);
	if (!(queue = switch_core_hash_find(globals.queue_hash, queue_name))) {
		queue = load_queue(queue_name);
	}
	if (queue) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "[%s] rwlock\n", queue->name);

		switch_thread_rwlock_rdlock(queue->rwlock);
	}
	switch_mutex_unlock(globals.mutex);

	return queue;
}

struct call_helper {
	const char *member_uuid;
	const char *queue;
	const char *queue_strategy;
	const char *member_joined_epoch;
	const char *member_caller_name;
	const char *member_caller_number;
	const char *agent_name;
	const char *agent_system;
	const char *agent_status;
	const char *originate_string;
	const char *record_template;
	int no_answer_count;
	int max_no_answer;
	switch_memory_pool_t *pool;
};

int cc_queue_count(const char *queue)
{
	char *sql;
	int count = 0;
	char res[256] = "0";
	const char *event_name = "Single-Queue";
	switch_event_t *event;

	if (!switch_strlen_zero(queue)) {
		if (queue[0] == '*') {
			event_name = "All-Queues";
			sql = switch_mprintf("SELECT count(*) FROM members WHERE state = '%q' OR state = '%q'",
					cc_member_state2str(CC_MEMBER_STATE_WAITING), cc_member_state2str(CC_MEMBER_STATE_TRYING));
		} else {
			sql = switch_mprintf("SELECT count(*) FROM members WHERE queue = '%q' AND (state = '%q' OR state = '%q')",
					queue, cc_member_state2str(CC_MEMBER_STATE_WAITING), cc_member_state2str(CC_MEMBER_STATE_TRYING));
		}
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);
		count = atoi(res);

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", queue);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "members-count");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Count", res);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Selection", event_name);
			switch_event_fire(&event);
		}
	}	

	return count;
}

cc_status_t cc_agent_add(const char *agent, const char *type)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;

	if (!strcasecmp(type, CC_AGENT_TYPE_CALLBACK)) {
		char res[256] = "";
		/* Check to see if agent already exist */
		sql = switch_mprintf("SELECT count(*) FROM agents WHERE name = '%q'", agent);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);

		if (atoi(res) != 0) {
			result = CC_STATUS_AGENT_ALREADY_EXIST;
			goto done;
		}
		/* Add Agent */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding Agent %s with type %s with default status %s\n", 
				agent, type, cc_agent_status2str(CC_AGENT_STATUS_LOGGED_OUT));
		sql = switch_mprintf("INSERT INTO agents (name, system, type, status, state) VALUES('%q', 'single_box', '%q', '%q', '%q');", 
				agent, type, cc_agent_status2str(CC_AGENT_STATUS_LOGGED_OUT), cc_agent_state2str(CC_AGENT_STATE_WAITING));
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);
	} else {
		result = CC_STATUS_AGENT_INVALID_TYPE;
		goto done;
	}
done:		
	return result;
}

cc_status_t cc_agent_del(const char *agent)
{
	cc_status_t result = CC_STATUS_SUCCESS;

	char *sql;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deleted Agent %s\n", agent);
	sql = switch_mprintf("DELETE FROM agents WHERE name = '%q';"
			"DELETE FROM tiers WHERE agent = '%q';",
			agent, agent);
	cc_execute_sql(NULL, sql, NULL);
	switch_safe_free(sql);
	return result;
}

cc_agent_status_t cc_agent_get(const char *key, const char *agent, char *ret_result, size_t ret_result_size)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;
	switch_event_t *event;
	char res[256];

	/* Check to see if agent already exist */
	sql = switch_mprintf("SELECT count(*) FROM agents WHERE name = '%q'", agent);
	cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
	switch_safe_free(sql);

	if (atoi(res) == 0) {
		result = CC_STATUS_AGENT_NOT_FOUND;
		goto done;
	}

	if (!strcasecmp(key, "status") ) { 
		/* Check to see if agent already exist */
		sql = switch_mprintf("SELECT %q FROM agents WHERE name = '%q'", key, agent);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);
		switch_snprintf(ret_result, ret_result_size, "%s", res);
		result = CC_STATUS_SUCCESS;

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", agent);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "agent-status-get");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-Status", res);
			switch_event_fire(&event);
		}

	} else {
		result = CC_STATUS_INVALID_KEY;
		goto done;

	}

done:   
	if (result == CC_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Get Info Agent %s set %s = %s\n", agent, key, res);
	}

	return result;
}

cc_status_t cc_agent_update(const char *key, const char *value, const char *agent)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;
	char res[256];
	switch_event_t *event;

	/* Check to see if agent already exist */
	sql = switch_mprintf("SELECT count(*) FROM agents WHERE name = '%q'", agent);
	cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
	switch_safe_free(sql);

	if (atoi(res) == 0) {
		result = CC_STATUS_AGENT_NOT_FOUND;
		goto done;
	}

	if (!strcasecmp(key, "status")) {
		if (cc_agent_str2status(value) != CC_AGENT_STATUS_UNKNOWN) {
			/* Reset values on available only */
			if (cc_agent_str2status(value) == CC_AGENT_STATUS_AVAILABLE) {
				sql = switch_mprintf("UPDATE agents SET status = '%q', last_status_change = '%ld', talk_time = 0, calls_answered = 0, no_answer_count = 0"
						" WHERE name = '%q' AND NOT status = '%q'",
						value, (long) switch_epoch_time_now(NULL),
						agent, value);
			} else {
				sql = switch_mprintf("UPDATE agents SET status = '%q', last_status_change = '%ld' WHERE name = '%q'",
						value, (long) switch_epoch_time_now(NULL), agent);
			}
			cc_execute_sql(NULL, sql, NULL);
			switch_safe_free(sql);


			/* Used to stop any active callback */
			if (cc_agent_str2status(value) != CC_AGENT_STATUS_AVAILABLE) {
				sql = switch_mprintf("SELECT uuid FROM members WHERE serving_agent = '%q' AND serving_system = 'single_box' AND NOT state = 'Answered'", agent);
				cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
				switch_safe_free(sql);
				if (!switch_strlen_zero(res)) {
					switch_core_session_hupall_matching_var("cc_member_uuid", res, SWITCH_CAUSE_ORIGINATOR_CANCEL);
				}
			}


			result = CC_STATUS_SUCCESS;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", agent);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "agent-status-change");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-Status", value);
				switch_event_fire(&event);
			}

		} else {
			result = CC_STATUS_AGENT_INVALID_STATUS;
			goto done;
		}
	} else if (!strcasecmp(key, "state")) {
		if (cc_agent_str2state(value) != CC_AGENT_STATE_UNKNOWN) {
			if (cc_agent_str2state(value) != CC_AGENT_STATE_RECEIVING) {
				sql = switch_mprintf("UPDATE agents SET state = '%q' WHERE name = '%q'", value, agent);
			} else {
				sql = switch_mprintf("UPDATE agents SET state = '%q', last_offered_call = '%ld' WHERE name = '%q'",
						value, (long) switch_epoch_time_now(NULL), agent);
			}
			cc_execute_sql(NULL, sql, NULL);
			switch_safe_free(sql);

			result = CC_STATUS_SUCCESS;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", agent);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "agent-state-change");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-State", value);
				switch_event_fire(&event);
			}

		} else {
			result = CC_STATUS_AGENT_INVALID_STATE;
			goto done;
		}

	} else if (!strcasecmp(key, "contact")) {
		sql = switch_mprintf("UPDATE agents SET contact = '%q', system = 'single_box' WHERE name = '%q'", value, agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;

	} else if (!strcasecmp(key, "type")) {
		if (strcasecmp(value, CC_AGENT_TYPE_CALLBACK)) {
			result = CC_STATUS_AGENT_INVALID_TYPE;
			goto done;
		}

		sql = switch_mprintf("UPDATE agents SET type = '%q' WHERE name = '%q'", value, agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;

	} else if (!strcasecmp(key, "max_no_answer")) {
		sql = switch_mprintf("UPDATE agents SET max_no_answer = '%d', system = 'single_box' WHERE name = '%q'", atoi(value), agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;

	} else if (!strcasecmp(key, "wrap_up_time")) {
		sql = switch_mprintf("UPDATE agents SET wrap_up_time = '%d', system = 'single_box' WHERE name = '%q'", atoi(value), agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;

	} else {
		result = CC_STATUS_INVALID_KEY;
		goto done;

	}

done:
	if (result == CC_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Updated Agent %s set %s = %s\n", agent, key, value);
	}

	return result;
}

cc_status_t cc_tier_add(const char *queue_name, const char *agent, const char *state, int level, int position)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;
	cc_queue_t *queue = NULL;
	if (!(queue = get_queue(queue_name))) {
		result = CC_STATUS_QUEUE_NOT_FOUND;
		goto done;
	} else {
		queue_rwunlock(queue);
	}

	if (cc_tier_str2state(state) != CC_TIER_STATE_UNKNOWN) {
		char res[256] = "";
		/* Check to see if agent already exist */
		sql = switch_mprintf("SELECT count(*) FROM agents WHERE name = '%q'", agent);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);

		if (atoi(res) == 0) {
			result = CC_STATUS_AGENT_NOT_FOUND;
			goto done;
		}

		/* Check to see if tier already exist */
		sql = switch_mprintf("SELECT count(*) FROM tiers WHERE agent = '%q' AND queue = '%q'", agent, queue_name);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);

		if (atoi(res) != 0) {
			result = CC_STATUS_TIER_ALREADY_EXIST;
			goto done;
		}

		/* Add Agent in tier */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding Tier on Queue %s for Agent %s, level %d, position %d\n", queue_name, agent, level, position);
		sql = switch_mprintf("INSERT INTO tiers (queue, agent, state, level, position) VALUES('%q', '%q', '%q', '%d', '%d');",
				queue_name, agent, state, level, position);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else {
		result = CC_STATUS_TIER_INVALID_STATE;
		goto done;

	}

done:		
	return result;
}

cc_status_t cc_tier_update(const char *key, const char *value, const char *queue_name, const char *agent)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;
	char res[256];
	cc_queue_t *queue = NULL;

	/* Check to see if tier already exist */
	sql = switch_mprintf("SELECT count(*) FROM tiers WHERE agent = '%q' AND queue = '%q'", agent, queue_name);
	cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
	switch_safe_free(sql);

	if (atoi(res) == 0) {
		result = CC_STATUS_TIER_NOT_FOUND;
		goto done;
	}

	/* Check to see if agent already exist */
	sql = switch_mprintf("SELECT count(*) FROM agents WHERE name = '%q'", agent);
	cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
	switch_safe_free(sql);

	if (atoi(res) == 0) {
		result = CC_STATUS_AGENT_NOT_FOUND;
		goto done;
	}

	if (!(queue = get_queue(queue_name))) {
		result = CC_STATUS_QUEUE_NOT_FOUND;
		goto done;
	} else {
		queue_rwunlock(queue);
	}

	if (!strcasecmp(key, "state")) {
		if (cc_tier_str2state(value) != CC_TIER_STATE_UNKNOWN) {
			sql = switch_mprintf("UPDATE tiers SET state = '%q' WHERE queue = '%q' AND agent = '%q'", value, queue_name, agent);
			cc_execute_sql(NULL, sql, NULL);
			switch_safe_free(sql);
			result = CC_STATUS_SUCCESS;
		} else {
			result = CC_STATUS_TIER_INVALID_STATE;
			goto done;
		}
	} else if (!strcasecmp(key, "level")) {
		sql = switch_mprintf("UPDATE tiers SET level = '%d' WHERE queue = '%q' AND agent = '%q'", atoi(value), queue_name, agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;

	} else if (!strcasecmp(key, "position")) {
		sql = switch_mprintf("UPDATE tiers SET position = '%d' WHERE queue = '%q' AND agent = '%q'", atoi(value), queue_name, agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else {
		result = CC_STATUS_INVALID_KEY;
		goto done;
	}	
done:
	if (result == CC_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Updated tier: Agent %s in Queue %s set %s = %s\n", agent, queue_name, key, value);
	}
	return result;
}

cc_status_t cc_tier_del(const char *queue, const char *agent)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deleted tier Agent %s in Queue %s\n", agent, queue);
	sql = switch_mprintf("DELETE FROM tiers WHERE queue = '%q' AND agent = '%q';", queue, agent);
	cc_execute_sql(NULL, sql, NULL);
	switch_safe_free(sql);

	result = CC_STATUS_SUCCESS;

	return result;
}

static switch_status_t load_agent(const char *agent_name)
{
	switch_xml_t x_agents, x_agent, cfg, xml;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return SWITCH_STATUS_FALSE;
	}
	if (!(x_agents = switch_xml_child(cfg, "agents"))) {
		goto end;
	}

	if ((x_agent = switch_xml_find_child(x_agents, "agent", "name", agent_name))) {
		const char *type = switch_xml_attr(x_agent, "type");
		const char *contact = switch_xml_attr(x_agent, "contact"); 
		const char *status = switch_xml_attr(x_agent, "status");
		const char *max_no_answer = switch_xml_attr(x_agent, "max-no-answer");
		const char *wrap_up_time = switch_xml_attr(x_agent, "wrap-up-time");

		if (type) {
			cc_status_t res = cc_agent_add(agent_name, type);
			if (res == CC_STATUS_SUCCESS || res == CC_STATUS_AGENT_ALREADY_EXIST) {
				if (contact) {
					cc_agent_update("contact", contact, agent_name);
				}
				if (status) {
					cc_agent_update("status", status, agent_name);
				}
				if (wrap_up_time) {
					cc_agent_update("wrap_up_time", wrap_up_time, agent_name);
				}
				if (max_no_answer) {
					cc_agent_update("max_no_answer", max_no_answer, agent_name);
				}

				if (type && res == CC_STATUS_AGENT_ALREADY_EXIST) {
					cc_agent_update("type", type, agent_name);
				}

			}
		}
	}

end:

	if (xml) {
		switch_xml_free(xml);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t load_config(void)
{
	switch_xml_t cfg, xml, settings, param, x_queues, x_queue, x_agents, x_agent, x_tiers, x_tier;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_lock(globals.mutex);
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				globals.debug = atoi(val);
			}
		}
	}

	if ((x_queues = switch_xml_child(cfg, "queues"))) {
		for (x_queue = switch_xml_child(x_queues, "queue"); x_queue; x_queue = x_queue->next) {
			load_queue(switch_xml_attr_soft(x_queue, "name"));
		}
	}

	if ((x_agents = switch_xml_child(cfg, "agents"))) {
		for (x_agent = switch_xml_child(x_agents, "agent"); x_agent; x_agent = x_agent->next) {
			const char *agent = switch_xml_attr(x_agent, "name");
			if (agent) {
				load_agent(agent);
			}
		}
	}

	if ((x_tiers = switch_xml_child(cfg, "tiers"))) {
		for (x_tier = switch_xml_child(x_tiers, "tier"); x_tier; x_tier = x_tier->next) {
			const char *agent = switch_xml_attr(x_tier, "agent");
			const char *queue_name = switch_xml_attr(x_tier, "queue");
			const char *level = switch_xml_attr(x_tier, "level");
			const char *position = switch_xml_attr(x_tier, "position");
			if (agent && queue_name) {
				/* Hack to check if an tier already exist */
				if (cc_tier_update("unknown", "unknown", queue_name, agent) == CC_STATUS_TIER_NOT_FOUND) {
					if (level && position) {
						cc_tier_add(queue_name, agent, cc_tier_state2str(CC_TIER_STATE_READY), atoi(level), atoi(position));
					} else {
						/* default to level 1 and position 1 within the level */
						cc_tier_add(queue_name, agent, cc_tier_state2str(CC_TIER_STATE_READY), 1, 1);
					}
				} else {
					if (level) {
						cc_tier_update("level", level, queue_name, agent);
					}
					if (position) {
						cc_tier_update("position", position, queue_name, agent);
					}
				}
			}
		}
	}

	switch_mutex_unlock(globals.mutex);

	switch_xml_free(xml);

	return SWITCH_STATUS_SUCCESS;
}

static void *SWITCH_THREAD_FUNC outbound_agent_thread_run(switch_thread_t *thread, void *obj)
{
	struct call_helper *h = (struct call_helper *) obj;
	switch_core_session_t *agent_session = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char *sql = NULL;
	char *dialstr = NULL;
	cc_tier_state_t tiers_state = CC_TIER_STATE_READY;
	switch_core_session_t *member_session = switch_core_session_locate(h->member_uuid);
	switch_event_t *ovars;
	switch_time_t t_agent_called = 0;
	switch_time_t t_agent_answered = 0;
	switch_time_t t_member_called = atoi(h->member_joined_epoch);

	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);

	/* member is gone before we could process it */
	if (!member_session) {
		sql = switch_mprintf("DELETE FROM members WHERE system = 'single_box' AND uuid = '%q'", h->member_uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);
		goto done;
	}
	switch_event_create(&ovars, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_member_uuid", "%s", h->member_uuid);
	switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_member_pre_answer_uuid", "%s", h->member_uuid);
	switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "ignore_early_media", "true");

	t_agent_called = switch_epoch_time_now(NULL);
	dialstr = switch_mprintf("%s", h->originate_string);
	status = switch_ivr_originate(NULL, &agent_session, &cause, dialstr, 60, NULL, h->member_caller_name, h->member_caller_number, NULL, ovars, SOF_NONE, NULL);
	switch_safe_free(dialstr);

	switch_event_destroy(&ovars);

	if (status == SWITCH_STATUS_SUCCESS) {
		/* Agent Answered */
		const char *agent_uuid = switch_core_session_get_uuid(agent_session);
		switch_channel_t *member_channel = switch_core_session_get_channel(member_session);
		switch_channel_t *agent_channel = switch_core_session_get_channel(agent_session);
		switch_event_t *event;

		switch_channel_set_variable(agent_channel, "cc_member_pre_answer_uuid", NULL);

		if (!strcasecmp(h->queue_strategy,"ring-all")) {
			char res[256];
			/* Map the Agent to the member */
			sql = switch_mprintf("UPDATE members SET serving_agent = '%q', serving_system = 'single_box', state = '%q'"
					" WHERE state = '%q' AND uuid = '%q' AND system = 'single_box' AND serving_agent = 'ring-all'",
					h->agent_name, cc_member_state2str(CC_MEMBER_STATE_TRYING),
					cc_member_state2str(CC_MEMBER_STATE_TRYING), h->member_uuid);
			cc_execute_sql(NULL, sql, NULL);

			switch_safe_free(sql);

			/* Check if we won the race to get the member to our selected agent (Used for Multi system purposes) */
			sql = switch_mprintf("SELECT count(*) FROM members"
					" WHERE serving_agent = '%q' AND serving_system = 'single_box' AND uuid = '%q' AND system = 'single_box'",
					h->agent_name, h->member_uuid);
			cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
			switch_safe_free(sql);

			if (atoi(res) == 0) {
				goto done;
			}
			switch_core_session_hupall_matching_var("cc_member_pre_answer_uuid", h->member_uuid, SWITCH_CAUSE_ORIGINATOR_CANCEL);

		}
		t_agent_answered = switch_epoch_time_now(NULL);

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(agent_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", h->queue);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "bridge-agent-start");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", h->agent_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-System", h->agent_system);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-UUID", agent_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-UUID", h->member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Name", h->member_caller_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Number", h->member_caller_number);
			switch_event_fire(&event);
		}
		/* for xml_cdr needs */
		switch_channel_set_variable(member_channel, "cc_agent", h->agent_name);
		switch_channel_set_variable_printf(member_channel, "cc_queue_answered_epoch", "%ld", (long) switch_epoch_time_now(NULL)); 

		/* Set UUID of the Agent channel */
		sql = switch_mprintf("UPDATE agents SET uuid = '%q', last_bridge_start = '%ld', calls_answered = calls_answered + 1, no_answer_count = 0"
				" WHERE name = '%q' AND system = '%q'",
				agent_uuid, (long) switch_epoch_time_now(NULL),
				h->agent_name, h->agent_system);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Change the agents Status in the tiers */
		sql = switch_mprintf("UPDATE tiers SET state = '%q' WHERE agent = '%q' AND queue = '%q'",
				cc_tier_state2str(CC_TIER_STATE_ACTIVE_INBOUND), h->agent_name, h->queue);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		cc_agent_update("state", cc_agent_state2str(CC_AGENT_STATE_IN_A_QUEUE_CALL), h->agent_name);

		/* Record session if record-template is provided */
		if (h->record_template) {
			char *expanded = switch_channel_expand_variables(member_channel, h->record_template);
			switch_channel_set_variable(member_channel, "cc_record_filename", expanded);
			switch_ivr_record_session(member_session, expanded, 0, NULL);
			if (expanded != h->record_template) {
				switch_safe_free(expanded);
			}					
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Agent %s answered \"%s\" (%s) from queue %s%s\n",
				h->agent_name, h->member_caller_name, h->member_caller_number, h->queue, (h->record_template?" (Recorded)":""));
		switch_ivr_uuid_bridge(switch_core_session_get_uuid(agent_session), h->member_uuid);

		/* Wait until the member hangup or the agent hangup.  This will quit also if the agent transfer the call */
		while(switch_channel_up(member_channel) && switch_channel_up(agent_channel) && globals.running) {
			switch_yield(100000);
		}
		tiers_state = CC_TIER_STATE_READY;		

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(agent_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", h->queue);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "bridge-agent-end");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", h->agent_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-System", h->agent_system);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-UUID", agent_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-UUID", h->member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Name", h->member_caller_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Number", h->member_caller_number);
			switch_event_fire(&event);
		}
		/* for xml_cdr needs */
		switch_channel_set_variable_printf(member_channel, "cc_queue_terminated_epoch", "%ld", (long) switch_epoch_time_now(NULL));

		/* Update Agents Items */
		sql = switch_mprintf("UPDATE agents SET uuid = '', last_bridge_end = %ld, talk_time = talk_time + (%ld-last_bridge_start) WHERE name = '%q' AND system = '%q';"
				, (long) switch_epoch_time_now(NULL), (long) switch_epoch_time_now(NULL), h->agent_name, h->agent_system);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Remove the member entry from the db (Could become optional to support latter processing) */
		sql = switch_mprintf("DELETE FROM members WHERE system = 'single_box' AND uuid = '%q'", h->member_uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Caller off event */
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(member_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", h->queue);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "member-queue-end");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Cause", "Terminated");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Answer-Time", "%ld",  (long) (t_agent_answered - t_agent_called));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Wait-Time", "%ld",  (long) (t_agent_answered - t_member_called));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Talk-Time", "%ld",  (long) (switch_epoch_time_now(NULL) - t_agent_answered));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Total-Time", "%ld",  (long) (switch_epoch_time_now(NULL) - t_member_called));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-UUID", h->member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Name", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Number", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")));
			switch_event_fire(&event);
		}


	} else {
		/* Agent didn't answer or originate failed */
		sql = switch_mprintf("UPDATE members SET state = '%q', serving_agent = '', serving_system = ''"
				" WHERE serving_agent = '%q' AND serving_system = '%q' AND uuid = '%q' AND system = 'single_box'",
				cc_member_state2str(CC_MEMBER_STATE_WAITING),
				h->agent_name, h->agent_system, h->member_uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Agent %s Origination Canceled : %s\n",h->agent_name, switch_channel_cause2str(cause));

		switch (cause) {
			case SWITCH_CAUSE_CALL_REJECTED:
			case SWITCH_CAUSE_ORIGINATOR_CANCEL:
				break;
			default:
				tiers_state = CC_TIER_STATE_NO_ANSWER;

				/* Update Agent NO Answer count */
				sql = switch_mprintf("UPDATE agents SET no_answer_count = no_answer_count + 1 WHERE name = '%q' AND system = '%q';",
						h->agent_name, h->agent_system);
				cc_execute_sql(NULL, sql, NULL);
				switch_safe_free(sql);

				/* Put Agent on break because he didn't answer often */
				if (h->max_no_answer > 0 && (h->no_answer_count + 1) >= h->max_no_answer) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Agent %s reach maximum no answer of %d, Putting agent on break\n",
							h->agent_name, h->max_no_answer);
					cc_agent_update("status", cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), h->agent_name);
				}
		}

	}

done:
	/* Make Agent Available Again */
	sql = switch_mprintf(
			"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND queue = '%q' AND (state = '%q' OR state = '%q' OR state = '%q');"
			"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND NOT queue = '%q' AND state = '%q'"
			, cc_tier_state2str(tiers_state), h->agent_name, h->queue, cc_tier_state2str(CC_TIER_STATE_ACTIVE_INBOUND), cc_tier_state2str(CC_TIER_STATE_STANDBY), cc_tier_state2str(CC_TIER_STATE_OFFERING),
			cc_tier_state2str(CC_TIER_STATE_READY), h->agent_name, h->queue, cc_tier_state2str(CC_TIER_STATE_STANDBY));
	cc_execute_sql(NULL, sql, NULL);
	switch_safe_free(sql);

	/* If we are in Status Available On Demand, set state to Idle so we do not receive another call until state manually changed to Waiting */
	if (!strcasecmp(cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE_ON_DEMAND), h->agent_status)) {
		cc_agent_update("state", cc_agent_state2str(CC_AGENT_STATE_IDLE), h->agent_name);
	} else {
		cc_agent_update("state", cc_agent_state2str(CC_AGENT_STATE_WAITING), h->agent_name);
	}

	if (agent_session) {
		switch_core_session_rwunlock(agent_session);
	}
	if (member_session) {
		switch_core_session_rwunlock(member_session);
	}

	switch_core_destroy_memory_pool(&h->pool);

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	switch_mutex_unlock(globals.mutex);

	return NULL;
}

struct agent_callback {
	const char *queue;
	const char *system;
	const char *uuid;
	const char *caller_number;
	const char *caller_name;
	const char *joined_epoch;
	const char *strategy;
	const char *record_template;
	int tier;
};
typedef struct agent_callback agent_callback_t;

static int agents_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	agent_callback_t *cbt = (agent_callback_t *) pArg;
	char *sql = NULL;
	char res[256];

	/* More Advanced rules based on On Break still being available for near future call */
	if (!strcasecmp(argv[2], cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK))) {
		return 0; /* Skip this agent for the moment */
	}

	/* If agent isn't on this box */
	if (strcasecmp(argv[0],"single_box" /* SELF */)) {
		if (!strcasecmp(cbt->strategy, "ring-all")) {
			return 1; /* Abort finding agent for member if we found a match but for a different Server */
		} else {
			return 0; /* Skip this Agents only, so we can ring the other one */
		}
	}

	if (!strcasecmp(cbt->strategy,"ring-all")) {
		/* Check if member is a ring-all mode */
		sql = switch_mprintf("SELECT count(*) FROM members WHERE serving_agent = 'ring-all' AND uuid = '%q' AND system = 'single_box'", cbt->uuid);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));

		switch_safe_free(sql);
	} else {
		/* Map the Agent to the member */
		sql = switch_mprintf("UPDATE members SET serving_agent = '%q', serving_system = 'single_box', state = '%q'"
				" WHERE state = '%q' AND uuid = '%q' AND system = 'single_box'", 
				argv[1], cc_member_state2str(CC_MEMBER_STATE_TRYING),
				cc_member_state2str(CC_MEMBER_STATE_WAITING), cbt->uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Check if we won the race to get the member to our selected agent (Used for Multi system purposes) */
		sql = switch_mprintf("SELECT count(*) FROM members WHERE serving_agent = '%q' AND serving_system = 'single_box' AND uuid = '%q' AND system = 'single_box'",
				argv[1], cbt->uuid);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);
	}

	switch (atoi(res)) {
		case 0: /* Ok, someone else took it, or user hanged up already */
			return 1;
			/* We default to default even if more entry is returned... Should never happen	anyway */
		default: /* Go ahead, start thread to try to bridge these 2 caller */
			{
				switch_thread_t *thread;
				switch_threadattr_t *thd_attr = NULL;
				switch_memory_pool_t *pool;
				struct call_helper *h;

				switch_core_new_memory_pool(&pool);
				h = switch_core_alloc(pool, sizeof(*h));
				h->pool = pool;
				h->member_uuid = switch_core_strdup(h->pool, cbt->uuid);
				h->queue_strategy = switch_core_strdup(h->pool, cbt->strategy);
				h->originate_string = switch_core_strdup(h->pool, argv[3]);
				h->agent_name = switch_core_strdup(h->pool, argv[1]);
				h->agent_system = switch_core_strdup(h->pool, "single_box");
				h->agent_status = switch_core_strdup(h->pool, argv[2]);
				h->member_joined_epoch = switch_core_strdup(h->pool, cbt->joined_epoch); 
				h->member_caller_name = switch_core_strdup(h->pool, cbt->caller_name);
				h->member_caller_number = switch_core_strdup(h->pool, cbt->caller_number);
				h->queue = switch_core_strdup(h->pool, cbt->queue);
				h->record_template = switch_core_strdup(h->pool, cbt->record_template);
				h->no_answer_count = atoi(argv[4]);
				h->max_no_answer = atoi(argv[5]);

				cc_agent_update("state", cc_agent_state2str(CC_AGENT_STATE_RECEIVING), h->agent_name);

				sql = switch_mprintf(
						"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND queue = '%q';"
						"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND NOT queue = '%q' AND state = '%q';",
						cc_tier_state2str(CC_TIER_STATE_OFFERING), h->agent_name, h->queue,
						cc_tier_state2str(CC_TIER_STATE_STANDBY), h->agent_name, h->queue, cc_tier_state2str(CC_TIER_STATE_READY));
				cc_execute_sql(NULL, sql, NULL);
				switch_safe_free(sql);

				switch_threadattr_create(&thd_attr, h->pool);
				switch_threadattr_detach_set(thd_attr, 1);
				switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
				switch_thread_create(&thread, thd_attr, outbound_agent_thread_run, h, h->pool);
			}

			if (!strcasecmp(cbt->strategy,"ring-all")) {
				return 0;
			} else {
				return 1;
			}
	}

	return 0;
}

static int members_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	cc_queue_t *queue = NULL;
	char *sql = NULL;
	char *sql_order_by = NULL;
	char *queue_name = NULL;
	char *queue_strategy = NULL;
	char *queue_record_template = NULL;
	agent_callback_t cbt;

	if (!argv[0] || !(queue = get_queue(argv[0]))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Queue %s not found locally, skip this member\n", argv[0]);
		goto end;
	} else {
		queue_name = strdup(queue->name);
		queue_strategy = strdup(queue->strategy);

		if (queue->record_template) {
			queue_record_template = strdup(queue->record_template);
		}

		queue_rwunlock(queue);
	}

	memset(&cbt, 0, sizeof(cbt));
	cbt.tier = 1;
	cbt.uuid = argv[1];
	cbt.caller_number = argv[2];
	cbt.caller_name = argv[3];
	cbt.joined_epoch = argv[4];
	cbt.queue = argv[0];
	cbt.strategy = queue_strategy;
	cbt.record_template = queue_record_template;
	
	if (!strcasecmp(queue->strategy, "longest-idle-agent") || !strcasecmp(queue_strategy, "roundrobin") /* TODO TMP backward compatibility for taxi dispatch setup */) {
		sql_order_by = switch_mprintf("level, agents.last_offered_call, position");
	} else if (!strcasecmp(queue_strategy, "agent-with-least-talk-time")) {
		sql_order_by = switch_mprintf("level, agents.talk_time, position");
	} else if (!strcasecmp(queue_strategy, "agent-with-fewest-calls")) {
		sql_order_by = switch_mprintf("level, agents.calls_answered, position");
	} else if (!strcasecmp(queue_strategy, "ring-all")) {
		sql = switch_mprintf("UPDATE members SET state = '%q' WHERE state = '%q' AND uuid = '%q' AND system = 'single_box'",
				cc_member_state2str(CC_MEMBER_STATE_TRYING), cc_member_state2str(CC_MEMBER_STATE_WAITING), cbt.uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);
		sql_order_by = switch_mprintf("level, position");
	} else if(!strcasecmp(queue_strategy, "sequentially-by-agent-order")) {
		sql_order_by = switch_mprintf("level, position");
	} else {
		/* If the strategy doesn't exist, just fallback to the following */
		sql_order_by = switch_mprintf("level, position");
	}

	sql = switch_mprintf("SELECT system, name, status, contact, no_answer_count, max_no_answer  FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent)"
			" WHERE tiers.queue = '%q'"
			" AND (tiers.state = '%q' OR tiers.state = '%q')"
			" AND (agents.status = '%q' OR agents.status = '%q' OR agents.status = '%q')"
			" AND agents.state = '%q' AND last_bridge_end < (%ld - wrap_up_time)"
			" ORDER BY %q",
			queue_name,
			cc_tier_state2str(CC_TIER_STATE_READY), cc_tier_state2str(CC_TIER_STATE_NO_ANSWER),
			cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE), cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE_ON_DEMAND),
			cc_agent_state2str(CC_AGENT_STATE_WAITING), (long) switch_epoch_time_now(NULL),
			sql_order_by);

	cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, agents_callback, &cbt /* Call back variables */);

	switch_safe_free(sql);
	switch_safe_free(sql_order_by);

end:
	switch_safe_free(queue_name);
	switch_safe_free(queue_strategy);
	switch_safe_free(queue_record_template);

	return 0;
}

static int AGENT_DISPATCH_THREAD_RUNNING = 0;
static int AGENT_DISPATCH_THREAD_STARTED = 0;

void *SWITCH_THREAD_FUNC cc_agent_dispatch_thread_run(switch_thread_t *thread, void *obj)
{
	int done = 0;

	switch_mutex_lock(globals.mutex);
	if (!AGENT_DISPATCH_THREAD_RUNNING) {
		AGENT_DISPATCH_THREAD_RUNNING++;
		globals.threads++;
	} else {
		done = 1;
	}
	switch_mutex_unlock(globals.mutex);

	if (done) {
		return NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Agent Dispatch Thread Started\n");

	while (globals.running == 1) {
		char *sql = NULL;
		sql = switch_mprintf("SELECT queue,uuid,caller_number,caller_name,joined_epoch,(%ld-joined_epoch)+base_score+skill_score AS score FROM members"
				" WHERE state = '%q' OR (serving_agent != 'ring-all' AND state = '%q') ORDER BY score DESC",
				(long) switch_epoch_time_now(NULL),
				cc_member_state2str(CC_MEMBER_STATE_WAITING), cc_member_state2str(CC_MEMBER_STATE_TRYING));

		cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, members_callback, NULL /* Call back variables */);
		switch_safe_free(sql);
		switch_yield(100000);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Agent Dispatch Thread Ended\n");

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	AGENT_DISPATCH_THREAD_RUNNING = AGENT_DISPATCH_THREAD_STARTED = 0;
	switch_mutex_unlock(globals.mutex);

	return NULL;
}


void cc_agent_dispatch_thread_start(void)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	int done = 0;

	switch_mutex_lock(globals.mutex);

	if (!AGENT_DISPATCH_THREAD_STARTED) {
		AGENT_DISPATCH_THREAD_STARTED++;
	} else {
		done = 1;
	}
	switch_mutex_unlock(globals.mutex);

	if (done) {
		return;
	}

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_increase(thd_attr);
	switch_thread_create(&thread, thd_attr, cc_agent_dispatch_thread_run, NULL, globals.pool);
}

struct member_helper {
	const char *uuid;
	int running;
	switch_memory_pool_t *pool;
};

void *SWITCH_THREAD_FUNC cc_member_thread_run(switch_thread_t *thread, void *obj)
{
	struct member_helper *m = (struct member_helper *) obj;
	switch_core_session_t *member_session = switch_core_session_locate(m->uuid);
	switch_channel_t *channel = NULL;

	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);

	if (member_session) {
		channel = switch_core_session_get_channel(member_session);
	} else {
		switch_core_destroy_memory_pool(&m->pool);
		return NULL;
	}

	while(switch_channel_ready(channel) && m->running && globals.running) {
		/* TODO Go thought the list of phrases */
		/* SAMPLE CODE to playback something over the MOH

		   switch_event_t *event;
		   if (switch_event_create(&event, SWITCH_EVENT_COMMAND) == SWITCH_STATUS_SUCCESS) {
		   switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
		   switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "execute-app-name", "playback");
		   switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "execute-app-arg", "tone_stream://%(200,0,500,600,700)");
		   switch_core_session_queue_private_event(member_session, &event, SWITCH_TRUE);
		   }
		 */

		/* If Agent Logoff, we might need to recalculare score based on skill */
		/* Play Announcement in order */
		//		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "MEMBER WAITING QUEUE EXECUTE\n");
		switch_yield(100000);
	}
	switch_core_session_rwunlock(member_session);
	switch_core_destroy_memory_pool(&m->pool);

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	switch_mutex_unlock(globals.mutex);

	return NULL; 
}

#define CC_DESC "callcenter"
#define CC_USAGE "queue_name"

SWITCH_STANDARD_APP(callcenter_function)
{
	int argc = 0;
	char *argv[6] = { 0 };
	char *mydata = NULL;
	cc_queue_t *queue = NULL;
	const char *queue_name = NULL;
	switch_channel_t *member_channel = switch_core_session_get_channel(session);
	char *sql = NULL;
	char *uuid = switch_core_session_get_uuid(session);
	switch_input_args_t args = { 0 };
	struct member_helper *h = NULL;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	int cc_base_score_int = 0;
	switch_channel_timetable_t *times = NULL;
	const char *cc_base_score = switch_channel_get_variable(member_channel, "cc_base_score");
	const char *cc_moh_override = switch_channel_get_variable(member_channel, "cc_moh_override");
	const char *cur_moh = NULL;
	char start_epoch[64];
	switch_event_t *event;
	switch_time_t t_member_called = switch_epoch_time_now(NULL);

	if (!zstr(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No Queue name provided\n");
		goto end;
	}

	if (argv[0]) {
		queue_name = argv[0];
	}

	if (!queue_name || !(queue = get_queue(queue_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Queue %s not found\n", queue_name);
		goto end;
	}

	/* Make sure we answer the channel before getting the switch_channel_time_table_t answer time */
	switch_channel_answer(member_channel);

	/* Grab the start epoch of a channel */
	times = switch_channel_get_timetable(member_channel);
	switch_snprintf(start_epoch, sizeof(start_epoch), "%" SWITCH_TIME_T_FMT, times->answered);

	/* Add manually imported score */
	if (cc_base_score) {
		cc_base_score_int += atoi(cc_base_score);
	}

	/* If system, will add the total time the session is up to the base score */
	if (!switch_strlen_zero(start_epoch) && !strcasecmp("system", queue->time_base_score)) {
		cc_base_score_int += (switch_epoch_time_now(NULL) - atoi(start_epoch));
	}

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(member_channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", queue_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "member-queue-start");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-UUID", uuid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Name", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Number", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")));
		switch_event_fire(&event);
	}
	/* for xml_cdr needs */
	switch_channel_set_variable_printf(member_channel, "cc_queue_joined_epoch", "%ld", (long) switch_epoch_time_now(NULL));
	switch_channel_set_variable(member_channel, "cc_queue", queue_name);

	/* Add the caller to the member queue */
	sql = switch_mprintf("INSERT INTO members"
			" (queue,system,uuid,system_epoch,joined_epoch,base_score,skill_score, caller_number, caller_name, serving_agent, serving_system, state)"
			" VALUES('%q','single_box', '%q', '%q', '%ld','%d','%d','%q','%q','%q','','%q')", 
			queue_name,
			uuid,
			start_epoch,
			(long) switch_epoch_time_now(NULL),
			cc_base_score_int,
			0 /*TODO SKILL score*/,
			switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")),
			switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")),
			(!strcasecmp(queue->strategy,"ring-all")?"ring-all":""),
			cc_member_state2str(CC_MEMBER_STATE_WAITING));
	cc_execute_sql(queue, sql, NULL);
	switch_safe_free(sql);

	/* Send Event with queue count */
	cc_queue_count(queue_name);

	/* Start Thread that will playback different prompt to the channel */
	switch_core_new_memory_pool(&pool);
	h = switch_core_alloc(pool, sizeof(*h));
	h->pool = pool;
	h->uuid = switch_core_strdup(h->pool, uuid);
	h->running = 1;
	switch_threadattr_create(&thd_attr, h->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, cc_member_thread_run, h, h->pool);

	/* Playback MOH */
	/* TODO Add DTMF callback support */
	/* TODO add MOH infitite loop */	
	if (cc_moh_override) {
		cur_moh = switch_core_session_strdup(session, cc_moh_override);
	} else {
		cur_moh = switch_core_session_strdup(session, queue->moh);
	}
	queue_rwunlock(queue);

	if (cur_moh) {
		switch_ivr_play_file(session, NULL, cur_moh, &args);
	} else {
		switch_ivr_collect_digits_callback(session, &args, 0, 0);
	}

	/* Stop Member Thread */
	if (h) {
		h->running = 0;
	}

	/* Hangup any agents been callback */
	if (!switch_channel_up(member_channel)) { /* If channel is still up, it mean that the member didn't hangup, so we should leave the agent alone */
		switch_core_session_hupall_matching_var("cc_member_uuid", uuid, SWITCH_CAUSE_ORIGINATOR_CANCEL);
		sql = switch_mprintf("DELETE FROM members WHERE system = 'single_box' AND uuid = '%q'", uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(member_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", queue_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "member-queue-end");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Wait-Time", "%ld", (long) (switch_epoch_time_now(NULL) - t_member_called));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Cause", "Abort");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-UUID", uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Name", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Caller-CID-Number", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")));
			switch_event_fire(&event);
		}
		/* for xml_cdr needs */
		switch_channel_set_variable_printf(member_channel, "cc_queue_canceled_epoch", "%ld", (long) switch_epoch_time_now(NULL));


		/* Send Event with queue count */
		cc_queue_count(queue_name);

	} else {
		sql = switch_mprintf("UPDATE members SET state = '%q', bridge_epoch = '%ld' WHERE system = 'single_box' AND uuid = '%q'",
				cc_member_state2str(CC_MEMBER_STATE_ANSWERED), (long) switch_epoch_time_now(NULL), uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Send Event with queue count */
		cc_queue_count(queue_name);

	}

end:

	return;
}

struct list_result {
	const char *name;
	const char *format;
	int row_process;
	switch_stream_handle_t *stream;

};
static int list_result_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct list_result *cbt = (struct list_result *) pArg;
	int i = 0;

	cbt->row_process++;

	if (cbt->row_process == 1) {
		for ( i = 0; i < argc; i++) {
			cbt->stream->write_function(cbt->stream,"%s", columnNames[i]);
			if (i < argc - 1) {
				cbt->stream->write_function(cbt->stream,"|");
			}
		}  
		cbt->stream->write_function(cbt->stream,"\n");

	}
	for ( i = 0; i < argc; i++) {
		cbt->stream->write_function(cbt->stream,"%s", argv[i]);
		if (i < argc - 1) {
			cbt->stream->write_function(cbt->stream,"|");
		}
	}
	cbt->stream->write_function(cbt->stream,"\n");
	return 0;
}

#define CC_CONFIG_API_SYNTAX "callcenter_config agent add [name] [type] | " \
"callcenter_config agent del [name] | " \
"callcenter_config agent set status [agent_name] [status] | " \
"callcenter_config agent set state [agent_name] [state] | " \
"callcenter_config agent set contact [agent_name] [contact] | " \
"callcenter_config agent get status [agent_name] | " \
"callcenter_config tier add [queue_name] [agent_name] [level] [position] | " \
"callcenter_config tier set state [queue_name] [agent_name] [state] | " \
"callcenter_config tier set level [queue_name] [agent_name] [level] | " \
"callcenter_config tier set position [queue_name] [agent_name] [position] | " \
"callcenter_config tier del [queue_name] [agent_name] | " \
"callcenter_config queue load [queue_name] | " \
"callcenter_config queue unload [queue_name] | " \
"callcenter_config queue reload [queue_name] | " \
"callcenter_config tier list [queue_name] | " \
"callcenter_config queue list [queue_name] | " \
"callcenter_config queue count [queue_name]"

SWITCH_STANDARD_API(cc_config_api_function)
{
	char *mydata = NULL, *argv[8] = { 0 };
	const char *section = NULL;
	const char *action = NULL;
	char *sql;
	int initial_argc = 2;

	int argc;
	if (!globals.running) {
		return SWITCH_STATUS_FALSE;
	}
	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", CC_CONFIG_API_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 2) {
		stream->write_function(stream, "%s", "-ERR Invalid!\n");
		goto done;
	}

	section = argv[0];
	action = argv[1];

	if (section && !strcasecmp(section, "agent")) {
		if (action && !strcasecmp(action, "add")) {
			if (argc-initial_argc < 2) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *name = argv[0 + initial_argc];
				const char *type = argv[1 + initial_argc];
				switch (cc_agent_add(name, type)) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", "+OK\n");
						break;
					case CC_STATUS_AGENT_ALREADY_EXIST:
						stream->write_function(stream, "%s", "-ERR Agent already exist!\n");
						goto done;
					case CC_STATUS_AGENT_INVALID_TYPE:
						stream->write_function(stream, "%s", "-ERR Agent type invalid!\n");
						goto done;

					default:
						stream->write_function(stream, "%s", "-ERR Unknown Error!\n");
						goto done;

				}
			}

		} else if (action && !strcasecmp(action, "del")) {
			if (argc-initial_argc < 1) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *agent = argv[0 + initial_argc];
				switch (cc_agent_del(agent)) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", "+OK\n");
						break;
					default:
						stream->write_function(stream, "%s", "-ERR Unknown Error!\n");
						goto done;
				}
			}

		} else if (action && !strcasecmp(action, "set")) {
			if (argc-initial_argc < 3) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *key = argv[0 + initial_argc];
				const char *agent = argv[1 + initial_argc];
				const char *value = argv[2 + initial_argc];

				switch (cc_agent_update(key, value, agent)) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", "+OK\n");
						break;
					case CC_STATUS_AGENT_INVALID_STATUS:
						stream->write_function(stream, "%s", "-ERR Invalid Agent Status!\n");
						goto done;
					case CC_STATUS_AGENT_INVALID_STATE:
						stream->write_function(stream, "%s", "-ERR Invalid Agent State!\n");
						goto done;
					case CC_STATUS_AGENT_INVALID_TYPE:
						stream->write_function(stream, "%s", "-ERR Invalid Agent Type!\n");
						goto done;
					case CC_STATUS_INVALID_KEY:
						stream->write_function(stream, "%s", "-ERR Invalid Agent Update KEY!\n");
						goto done;
					case CC_STATUS_AGENT_NOT_FOUND:	
						stream->write_function(stream, "%s", "-ERR Agent not found!\n");
						goto done;
					default:
						stream->write_function(stream, "%s", "-ERR Unknown Error!\n");
						goto done;
				}

			}
		} else if (action && !strcasecmp(action, "get")) {
			if (argc-initial_argc < 2) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *key = argv[0 + initial_argc];
				const char *agent = argv[1 + initial_argc];
				switch (cc_agent_get(key, agent, NULL, 0)) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", "+OK\n");
						break;
					case CC_STATUS_INVALID_KEY:
						stream->write_function(stream, "%s", "-ERR Invalid Agent Update KEY!\n");
						goto done;
					case CC_STATUS_AGENT_NOT_FOUND:
						stream->write_function(stream, "%s", "-ERR Agent not found!\n");
						goto done;
					default:
						stream->write_function(stream, "%s", "-ERR Unknown Error!\n");
						goto done;


				}
			}
		} else if (action && !strcasecmp(action, "list")) {
			struct list_result cbt;
			cbt.row_process = 0;
			cbt.stream = stream;
			sql = switch_mprintf("SELECT * FROM agents");
			cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, list_result_callback, &cbt /* Call back variables */);
			switch_safe_free(sql);
			stream->write_function(stream, "%s", "+OK\n");
		}

	} else if (section && !strcasecmp(section, "tier")) {
		if (action && !strcasecmp(action, "add")) {
			// queue, agent, level, position, state
			if (argc-initial_argc < 4) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *queue_name = argv[0 + initial_argc];
				const char *agent = argv[1 + initial_argc];
				const char *level = argv[2 + initial_argc];
				const char *position = argv[3 + initial_argc];

				switch(cc_tier_add(queue_name, agent, cc_tier_state2str(CC_TIER_STATE_READY), atoi(level), atoi(position))) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", "+OK\n");
						break;
					case CC_STATUS_QUEUE_NOT_FOUND:
						stream->write_function(stream, "%s", "-ERR Queue not found!\n");
						goto done;
					case CC_STATUS_TIER_INVALID_STATE:
						stream->write_function(stream, "%s", "-ERR Invalid Tier State!\n");
						goto done;
					case CC_STATUS_AGENT_NOT_FOUND:
						stream->write_function(stream, "%s", "-ERR Agent not found!\n");
						goto done;
					case CC_STATUS_TIER_ALREADY_EXIST:
						stream->write_function(stream, "%s", "-ERR Tier already exist!\n");
						goto done;
					default:
						stream->write_function(stream, "%s", "-ERR Unknown Error!\n");
						goto done;
				}
			}

		} else if (action && !strcasecmp(action, "set")) {
			if (argc-initial_argc < 4) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *key = argv[0 + initial_argc];
				const char *queue_name = argv[1 + initial_argc];
				const char *agent = argv[2 + initial_argc];
				const char *value = argv[3 + initial_argc];

				switch(cc_tier_update(key, value, queue_name, agent)) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", "+OK\n");
						break;
					case CC_STATUS_AGENT_INVALID_STATUS:
						stream->write_function(stream, "%s", "-ERR Invalid Agent Status!\n");
						goto done;
					case CC_STATUS_TIER_INVALID_STATE:
						stream->write_function(stream, "%s", "-ERR Invalid Tier State!\n");
						goto done;
					case CC_STATUS_INVALID_KEY:
						stream->write_function(stream, "%s", "-ERR Invalid Tier Update KEY!\n");
						goto done;
					case CC_STATUS_AGENT_NOT_FOUND:
						stream->write_function(stream, "%s", "-ERR Agent not found!\n");
						goto done;
					case CC_STATUS_QUEUE_NOT_FOUND:
						stream->write_function(stream, "%s", "-ERR Agent not found!\n");
						goto done;

					default:
						stream->write_function(stream, "%s", "-ERR Unknown Error!\n");
						goto done;
				}
			}
		} else if (action && !strcasecmp(action, "del")) {
			if (argc-initial_argc < 2) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done; 
			} else {
				const char *queue = argv[0 + initial_argc];
				const char *agent = argv[1 + initial_argc];
				switch (cc_tier_del(queue, agent)) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", "+OK\n");
						break;
					default:
						stream->write_function(stream, "%s", "-ERR Unknown Error!\n");
						goto done;

				}
			}

		} else if (action && !strcasecmp(action, "list")) {
			if (argc-initial_argc < 1) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *queue = argv[0 + initial_argc];
				struct list_result cbt;
				cbt.row_process = 0;
				cbt.stream = stream;
				sql = switch_mprintf("SELECT * FROM tiers WHERE queue = '%q' ORDER BY level, position", queue);
				cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, list_result_callback, &cbt /* Call back variables */);
				switch_safe_free(sql);
				stream->write_function(stream, "%s", "+OK\n");
			}
		}
	} else if (section && !strcasecmp(section, "queue")) {
		if (action && !strcasecmp(action, "load")) {
			if (argc-initial_argc < 1) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *queue_name = argv[0 + initial_argc];
				cc_queue_t *queue = NULL;
				if ((queue = get_queue(queue_name))) {
					queue_rwunlock(queue);
				}
				stream->write_function(stream, "%s", "+OK\n");
			}
		} else if (action && !strcasecmp(action, "unload")) {
			if (argc-initial_argc < 1) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *queue_name = argv[0 + initial_argc];
				destroy_queue(queue_name, SWITCH_FALSE);
				stream->write_function(stream, "%s", "+OK\n");

			}
		} else if (action && !strcasecmp(action, "reload")) {
			if (argc-initial_argc < 1) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *queue_name = argv[0 + initial_argc];
				cc_queue_t *queue = NULL;
				destroy_queue(queue_name, SWITCH_FALSE);
				if ((queue = get_queue(queue_name))) {
					queue_rwunlock(queue);
				}
				stream->write_function(stream, "%s", "+OK\n");
			}
		} else if (action && !strcasecmp(action, "list")) {
			if (argc-initial_argc < 1) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *queue_name = argv[0 + initial_argc];

				struct list_result cbt;
				cbt.row_process = 0;
				cbt.stream = stream;
				sql = switch_mprintf("SELECT * FROM members WHERE queue = '%q'", queue_name);
				cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, list_result_callback, &cbt /* Call back variables */);
				switch_safe_free(sql);
				stream->write_function(stream, "%s", "+OK\n");
			}
		} else if (action && !strcasecmp(action, "count")) {
			if (argc-initial_argc < 1) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else {
				const char *queue_name = argv[0 + initial_argc];
				char res[256] = "";
				switch_event_t *event;
				/* Check to see if agent already exist */
				sql = switch_mprintf("SELECT count(*) FROM members WHERE queue = '%q'", queue_name);
				cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
				switch_safe_free(sql);
				stream->write_function(stream, "%d\n", atoi(res));

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Name", queue_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "members-count");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Count", res);
					switch_event_fire(&event);
				}
			}
		}
	}

	goto done;
done:

	free(mydata);

	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_callcenter_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_callcenter_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	switch_core_hash_init(&globals.queue_hash, globals.pool);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	switch_mutex_lock(globals.mutex);
	globals.running = 1;
	switch_mutex_unlock(globals.mutex);

	load_config();

	if (!AGENT_DISPATCH_THREAD_STARTED) {
		cc_agent_dispatch_thread_start();
	}

	SWITCH_ADD_APP(app_interface, "callcenter", "CallCenter", CC_DESC, callcenter_function, CC_USAGE, SAF_NONE);
	SWITCH_ADD_API(api_interface, "callcenter_config", "Config of callcenter", cc_config_api_function, CC_CONFIG_API_SYNTAX);

	switch_console_set_complete("add callcenter_config agent add");
	switch_console_set_complete("add callcenter_config agent del");
	switch_console_set_complete("add callcenter_config agent set status");
	switch_console_set_complete("add callcenter_config agent set state");
	switch_console_set_complete("add callcenter_config agent set contact");
	switch_console_set_complete("add callcenter_config agent get status");
	switch_console_set_complete("add callcenter_config agent list");


	switch_console_set_complete("add callcenter_config tier add");
	switch_console_set_complete("add callcenter_config tier del");
	switch_console_set_complete("add callcenter_config tier set state");
	switch_console_set_complete("add callcenter_config tier set level");
	switch_console_set_complete("add callcenter_config tier set position");
	switch_console_set_complete("add callcenter_config tier list");

	switch_console_set_complete("add callcenter_config queue load");
	switch_console_set_complete("add callcenter_config queue unload");
	switch_console_set_complete("add callcenter_config queue reload");
	switch_console_set_complete("add callcenter_config queue list");
	switch_console_set_complete("add callcenter_config queue count");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
   Called when the system shuts down
   Macro expands to: switch_status_t mod_callcenter_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_callcenter_shutdown)
{
	switch_hash_index_t *hi;
	cc_queue_t *queue;
	void *val = NULL;
	const void *key;
	switch_ssize_t keylen;
	int sanity = 0;

	switch_mutex_lock(globals.mutex);
	if (globals.running == 1) {
		globals.running = 0;
	}
	switch_mutex_unlock(globals.mutex);

	while (globals.threads) {
		switch_cond_next();
		if (++sanity >= 60000) {
			break;
		}
	}

	switch_mutex_lock(globals.mutex);
	while ((hi = switch_hash_first(NULL, globals.queue_hash))) {
		switch_hash_this(hi, &key, &keylen, &val);
		queue = (cc_queue_t *) val;

		switch_core_hash_delete(globals.queue_hash, queue->name);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for write lock (queue %s)\n", queue->name);
		switch_thread_rwlock_wrlock(queue->rwlock);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying queue %s\n", queue->name);

		switch_core_destroy_memory_pool(&queue->pool);
		queue = NULL;
	}
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */

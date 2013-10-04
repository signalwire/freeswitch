/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 *
 * mod_callcenter.c -- Call Center Module
 *
 */
#include <switch.h>

#define CALLCENTER_EVENT "callcenter::info"

#define CC_AGENT_TYPE_CALLBACK "Callback"
#define CC_AGENT_TYPE_UUID_STANDBY "uuid-standby"
#define CC_SQLITE_DB_NAME "callcenter"


/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_callcenter_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_callcenter_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_callcenter_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_callcenter, mod_callcenter_load, mod_callcenter_shutdown, NULL);

static const char *global_cf = "callcenter.conf";

struct cc_status_table {
	const char *name;
	int status;
};

struct cc_state_table {
	const char *name;
	int state;
};

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

static struct cc_state_table STATE_CHART[] = {
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

static struct cc_status_table AGENT_STATUS_CHART[] = {
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

static struct cc_state_table AGENT_STATE_CHART[] = {
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
	CC_MEMBER_STATE_ABANDONED = 4
} cc_member_state_t;

static struct cc_state_table MEMBER_STATE_CHART[] = {
	{"Unknown", CC_MEMBER_STATE_UNKNOWN},
	{"Waiting", CC_MEMBER_STATE_WAITING},
	{"Trying", CC_MEMBER_STATE_TRYING},
	{"Answered", CC_MEMBER_STATE_ANSWERED},
	{"Abandoned", CC_MEMBER_STATE_ABANDONED},
	{NULL, 0}

};

struct cc_member_cancel_reason_table {
	const char *name;
	int reason;
};

typedef enum {
	CC_MEMBER_CANCEL_REASON_NONE,
	CC_MEMBER_CANCEL_REASON_TIMEOUT,
	CC_MEMBER_CANCEL_REASON_NO_AGENT_TIMEOUT,
	CC_MEMBER_CANCEL_REASON_BREAK_OUT
} cc_member_cancel_reason_t;

static struct cc_member_cancel_reason_table MEMBER_CANCEL_REASON_CHART[] = {
	{"NONE", CC_MEMBER_CANCEL_REASON_NONE},
	{"TIMEOUT", CC_MEMBER_CANCEL_REASON_TIMEOUT},
	{"NO_AGENT_TIMEOUT", CC_MEMBER_CANCEL_REASON_NO_AGENT_TIMEOUT},
	{"BREAK_OUT", CC_MEMBER_CANCEL_REASON_BREAK_OUT},
	{NULL, 0}
};

static char members_sql[] =
"CREATE TABLE members (\n"
"   queue	     VARCHAR(255),\n"
"   system	     VARCHAR(255),\n"
"   uuid	     VARCHAR(255) NOT NULL DEFAULT '',\n"
"   session_uuid     VARCHAR(255) NOT NULL DEFAULT '',\n"
"   cid_number	     VARCHAR(255),\n"
"   cid_name	     VARCHAR(255),\n"
"   system_epoch     INTEGER NOT NULL DEFAULT 0,\n"
"   joined_epoch     INTEGER NOT NULL DEFAULT 0,\n"
"   rejoined_epoch   INTEGER NOT NULL DEFAULT 0,\n"
"   bridge_epoch     INTEGER NOT NULL DEFAULT 0,\n"
"   abandoned_epoch  INTEGER NOT NULL DEFAULT 0,\n"
"   base_score       INTEGER NOT NULL DEFAULT 0,\n"
"   skill_score      INTEGER NOT NULL DEFAULT 0,\n"
"   serving_agent    VARCHAR(255),\n"
"   serving_system   VARCHAR(255),\n"
"   state	     VARCHAR(255)\n" ");\n";
/* Member State 
   Waiting
   Answered
 */

static char agents_sql[] =
"CREATE TABLE agents (\n"
"   name      VARCHAR(255),\n"
"   system    VARCHAR(255),\n"
"   uuid      VARCHAR(255),\n"
"   type      VARCHAR(255),\n" /* Callback , Dial in...*/
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
"   reject_delay_time INTEGER NOT NULL DEFAULT 0,\n"
"   busy_delay_time INTEGER NOT NULL DEFAULT 0,\n"
"   no_answer_delay_time INTEGER NOT NULL DEFAULT 0,\n"
"   last_bridge_start INTEGER NOT NULL DEFAULT 0,\n"
"   last_bridge_end INTEGER NOT NULL DEFAULT 0,\n"
"   last_offered_call INTEGER NOT NULL DEFAULT 0,\n" 
"   last_status_change INTEGER NOT NULL DEFAULT 0,\n"
"   no_answer_count INTEGER NOT NULL DEFAULT 0,\n"
"   calls_answered  INTEGER NOT NULL DEFAULT 0,\n"
"   talk_time  INTEGER NOT NULL DEFAULT 0,\n"
"   ready_time INTEGER NOT NULL DEFAULT 0\n"
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

static switch_xml_config_int_options_t config_int_0_86400 = { SWITCH_TRUE, 0, SWITCH_TRUE, 86400 };

/* TODO This is temporary until we either move it to the core, or use it differently in the module */
switch_time_t local_epoch_time_now(switch_time_t *t)
{
	switch_time_t now = switch_micro_time_now() / 1000000; /* APR_USEC_PER_SEC */
	if (t) {
		*t = now;
	}
	return now;
}

const char * cc_tier_state2str(cc_tier_state_t state)
{
	uint8_t x;
	const char *str = "Unknown";

	for (x = 0; x < (sizeof(STATE_CHART) / sizeof(struct cc_state_table)) - 1; x++) {
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

	for (x = 0; x < (sizeof(STATE_CHART) / sizeof(struct cc_state_table)) - 1 && STATE_CHART[x].name; x++) {
		if (!strcasecmp(STATE_CHART[x].name, str)) {
			state = STATE_CHART[x].state;
			break;
		}
	}
	return state;
}

const char * cc_member_cancel_reason2str(cc_member_cancel_reason_t reason)
{
	uint8_t x;
	const char *str = "NONE";

	for (x = 0; x < (sizeof(MEMBER_CANCEL_REASON_CHART) / sizeof(struct cc_member_cancel_reason_table)) - 1; x++) {
		if (MEMBER_CANCEL_REASON_CHART[x].reason == reason) {
			str = MEMBER_CANCEL_REASON_CHART[x].name;
			break;
		}
	}

	return str;
}

cc_member_cancel_reason_t cc_member_cancel_str2reason(const char *str)
{
	uint8_t x;
	cc_member_cancel_reason_t reason = CC_MEMBER_CANCEL_REASON_NONE;

	for (x = 0; x < (sizeof(MEMBER_CANCEL_REASON_CHART) / sizeof(struct cc_member_cancel_reason_table)) - 1 && MEMBER_CANCEL_REASON_CHART[x].name; x++) {
		if (!strcasecmp(MEMBER_CANCEL_REASON_CHART[x].name, str)) {
			reason = MEMBER_CANCEL_REASON_CHART[x].reason;
			break;
		}
	}
	return reason;
}

const char * cc_agent_status2str(cc_agent_status_t status)
{
	uint8_t x;
	const char *str = "Unknown";

	for (x = 0; x < (sizeof(AGENT_STATUS_CHART) / sizeof(struct cc_status_table)) - 1; x++) {
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

	for (x = 0; x < (sizeof(AGENT_STATUS_CHART) / sizeof(struct cc_status_table)) - 1 && AGENT_STATUS_CHART[x].name; x++) {
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

	for (x = 0; x < (sizeof(AGENT_STATE_CHART) / sizeof(struct cc_state_table)) - 1; x++) {
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

	for (x = 0; x < (sizeof(AGENT_STATE_CHART) / sizeof(struct cc_state_table)) - 1 && AGENT_STATE_CHART[x].name; x++) {
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

	for (x = 0; x < (sizeof(MEMBER_STATE_CHART) / sizeof(struct cc_state_table)) - 1; x++) {
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

	for (x = 0; x < (sizeof(MEMBER_STATE_CHART) / sizeof(struct cc_state_table)) - 1 && MEMBER_STATE_CHART[x].name; x++) {
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
	char *odbc_dsn;
	char *dbname;
	int32_t threads;
	int32_t running;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
} globals;

#define CC_QUEUE_CONFIGITEM_COUNT 100

struct cc_queue {
	char *name;

	char *strategy;
	char *moh;
	char *record_template;
	char *time_base_score;

	switch_bool_t tier_rules_apply;
	uint32_t tier_rule_wait_second;
	switch_bool_t tier_rule_wait_multiply_level;
	switch_bool_t tier_rule_no_agent_no_wait;

	uint32_t discard_abandoned_after;
	switch_bool_t abandoned_resume_allowed;

	uint32_t max_wait_time;
	uint32_t max_wait_time_with_no_agent;
	uint32_t max_wait_time_with_no_agent_time_reached;

	switch_mutex_t *mutex;

	switch_thread_rwlock_t *rwlock;
	switch_memory_pool_t *pool;
	uint32_t flags;

	switch_time_t last_agent_exist;
	switch_time_t last_agent_exist_check;

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


switch_cache_db_handle_t *cc_get_db_handle(void)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(globals.odbc_dsn)) {
		dsn = globals.odbc_dsn;
	} else {
		dsn = globals.dbname;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;

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

	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "tier-rules-apply", SWITCH_CONFIG_BOOL, 0, &queue->tier_rules_apply, SWITCH_FALSE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "tier-rule-wait-second", SWITCH_CONFIG_INT, 0, &queue->tier_rule_wait_second, 0, &config_int_0_86400, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "tier-rule-wait-multiply-level", SWITCH_CONFIG_BOOL, 0, &queue->tier_rule_wait_multiply_level, SWITCH_FALSE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "tier-rule-no-agent-no-wait", SWITCH_CONFIG_BOOL, 0, &queue->tier_rule_no_agent_no_wait, SWITCH_TRUE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "discard-abandoned-after", SWITCH_CONFIG_INT, 0, &queue->discard_abandoned_after, 60, &config_int_0_86400, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "abandoned-resume-allowed", SWITCH_CONFIG_BOOL, 0, &queue->abandoned_resume_allowed, SWITCH_FALSE, NULL, NULL, NULL);

	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "max-wait-time", SWITCH_CONFIG_INT, 0, &queue->max_wait_time, 0, &config_int_0_86400, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "max-wait-time-with-no-agent", SWITCH_CONFIG_INT, 0, &queue->max_wait_time_with_no_agent, 0, &config_int_0_86400, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(queue->config[i++], "max-wait-time-with-no-agent-time-reached", SWITCH_CONFIG_INT, 0, &queue->max_wait_time_with_no_agent_time_reached, 5, &config_int_0_86400, NULL, NULL);

	switch_assert(i < CC_QUEUE_CONFIGITEM_COUNT);

	return queue;

}


char *cc_execute_sql2str(cc_queue_t *queue, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;

	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	if (!(dbh = cc_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, NULL);

end:
	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

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

	if (!(dbh = cc_get_db_handle())) {
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

	if (!(dbh = cc_get_db_handle())) {
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

		queue->last_agent_exist = 0;
		queue->last_agent_exist_check = 0;

		switch_mutex_init(&queue->mutex, SWITCH_MUTEX_NESTED, queue->pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added queue %s\n", queue->name);
		switch_core_hash_insert(globals.queue_hash, queue->name, queue);

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
	const char *member_session_uuid;
	const char *queue_name;
	const char *queue_strategy;
	const char *member_joined_epoch;
	const char *member_cid_name;
	const char *member_cid_number;
	const char *agent_name;
	const char *agent_system;
	const char *agent_status;
	const char *agent_type;
	const char *agent_uuid;
	const char *originate_string;
	const char *record_template;
	int no_answer_count;
	int max_no_answer;
	int reject_delay_time;
	int busy_delay_time;
	int no_answer_delay_time;

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
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", queue);
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

	if (!strcasecmp(type, CC_AGENT_TYPE_CALLBACK) || !strcasecmp(type, CC_AGENT_TYPE_UUID_STANDBY)) {
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

cc_status_t cc_agent_get(const char *key, const char *agent, char *ret_result, size_t ret_result_size)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;
	switch_event_t *event;
	char res[256];

	/* Check to see if agent already exists */
	sql = switch_mprintf("SELECT count(*) FROM agents WHERE name = '%q'", agent);
	cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
	switch_safe_free(sql);

	if (atoi(res) == 0) {
		result = CC_STATUS_AGENT_NOT_FOUND;
		goto done;
	}

	if (!strcasecmp(key, "status") || !strcasecmp(key, "state") || !strcasecmp(key, "uuid") ) { 
		/* Check to see if agent already exists */
		sql = switch_mprintf("SELECT %q FROM agents WHERE name = '%q'", key, agent);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);
		switch_snprintf(ret_result, ret_result_size, "%s", res);
		result = CC_STATUS_SUCCESS;

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			char tmpname[256];
			if (!strcasecmp(key, "uuid")) {
				switch_snprintf(tmpname, sizeof(tmpname), "CC-Agent-UUID");	
			} else {
				switch_snprintf(tmpname, sizeof(tmpname), "CC-Agent-%c%s", (char) switch_toupper(key[0]), key+1);
			}
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", agent);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Action", "agent-%s-get", key);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, tmpname, res);
			switch_event_fire(&event);
		}

	} else {
		result = CC_STATUS_INVALID_KEY;
		goto done;

	}

done:   
	if (result == CC_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Get Info Agent %s %s = %s\n", agent, key, res);
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
				sql = switch_mprintf("UPDATE agents SET status = '%q', last_status_change = '%" SWITCH_TIME_T_FMT "', talk_time = 0, calls_answered = 0, no_answer_count = 0"
						" WHERE name = '%q' AND NOT status = '%q'",
						value, local_epoch_time_now(NULL),
						agent, value);
			} else {
				sql = switch_mprintf("UPDATE agents SET status = '%q', last_status_change = '%" SWITCH_TIME_T_FMT "' WHERE name = '%q'",
						value, local_epoch_time_now(NULL), agent);
			}
			cc_execute_sql(NULL, sql, NULL);
			switch_safe_free(sql);


			/* Used to stop any active callback */
			if (cc_agent_str2status(value) != CC_AGENT_STATUS_AVAILABLE) {
				sql = switch_mprintf("SELECT uuid FROM members WHERE serving_agent = '%q' AND serving_system = 'single_box' AND NOT state = 'Answered'", agent);
				cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
				switch_safe_free(sql);
				if (!switch_strlen_zero(res)) {
					switch_core_session_hupall_matching_var("cc_member_pre_answer_uuid", res, SWITCH_CAUSE_ORIGINATOR_CANCEL);
				}
			}


			result = CC_STATUS_SUCCESS;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", agent);
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
				sql = switch_mprintf("UPDATE agents SET state = '%q', last_offered_call = '%" SWITCH_TIME_T_FMT "' WHERE name = '%q'",
						value, local_epoch_time_now(NULL), agent);
			}
			cc_execute_sql(NULL, sql, NULL);
			switch_safe_free(sql);

			result = CC_STATUS_SUCCESS;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", agent);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "agent-state-change");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-State", value);
				switch_event_fire(&event);
			}

		} else {
			result = CC_STATUS_AGENT_INVALID_STATE;
			goto done;
		}
	} else if (!strcasecmp(key, "uuid")) {
		sql = switch_mprintf("UPDATE agents SET uuid = '%q', system = 'single_box' WHERE name = '%q'", value, agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else if (!strcasecmp(key, "contact")) {
		sql = switch_mprintf("UPDATE agents SET contact = '%q', system = 'single_box' WHERE name = '%q'", value, agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else if (!strcasecmp(key, "ready_time")) {
		sql = switch_mprintf("UPDATE agents SET ready_time = '%ld', system = 'single_box' WHERE name = '%q'", atol(value), agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else if (!strcasecmp(key, "busy_delay_time")) {
		sql = switch_mprintf("UPDATE agents SET busy_delay_time = '%ld', system = 'single_box' WHERE name = '%q'", atol(value), agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else if (!strcasecmp(key, "reject_delay_time")) {
		sql = switch_mprintf("UPDATE agents SET reject_delay_time = '%ld', system = 'single_box' WHERE name = '%q'", atol(value), agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else if (!strcasecmp(key, "no_answer_delay_time")) {
		sql = switch_mprintf("UPDATE agents SET no_answer_delay_time = '%ld', system = 'single_box' WHERE name = '%q'", atol(value), agent);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		result = CC_STATUS_SUCCESS;
	} else if (!strcasecmp(key, "type")) {
		if (strcasecmp(value, CC_AGENT_TYPE_CALLBACK) && strcasecmp(value, CC_AGENT_TYPE_UUID_STANDBY)) {
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

cc_status_t cc_tier_del(const char *queue_name, const char *agent)
{
	cc_status_t result = CC_STATUS_SUCCESS;
	char *sql;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deleted tier Agent %s in Queue %s\n", agent, queue_name);
	sql = switch_mprintf("DELETE FROM tiers WHERE queue = '%q' AND agent = '%q';", queue_name, agent);
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
		const char *reject_delay_time = switch_xml_attr(x_agent, "reject-delay-time");
		const char *busy_delay_time = switch_xml_attr(x_agent, "busy-delay-time");
		const char *no_answer_delay_time = switch_xml_attr(x_agent, "no-answer-delay-time");

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
				if (reject_delay_time) {
					cc_agent_update("reject_delay_time", reject_delay_time, agent_name);
				}
				if (busy_delay_time) {
					cc_agent_update("busy_delay_time", busy_delay_time, agent_name);
				}
				if (no_answer_delay_time) {
					cc_agent_update("no_answer_delay_time", no_answer_delay_time, agent_name);
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
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t cfg, xml, settings, param, x_queues, x_queue, x_agents, x_agent, x_tiers, x_tier;
	switch_cache_db_handle_t *dbh = NULL;
	char *sql = NULL;
	
	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		status = SWITCH_STATUS_TERM;
		goto end;
	}

	switch_mutex_lock(globals.mutex);
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcasecmp(var, "dbname")) {
				globals.dbname = strdup(val);
			} else if (!strcasecmp(var, "odbc-dsn")) {
				globals.odbc_dsn = strdup(val);
			}
		}
	}
	if (!globals.dbname) {
		globals.dbname = strdup(CC_SQLITE_DB_NAME);
	}

	/* Initialize database */
	if (!(dbh = cc_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot open DB!\n");
		status = SWITCH_STATUS_TERM;
		goto end;
	}
	switch_cache_db_test_reactive(dbh, "select count(session_uuid) from members", "drop table members", members_sql);
	switch_cache_db_test_reactive(dbh, "select count(ready_time) from agents", NULL, "alter table agents add ready_time integer not null default 0;"
									   "alter table agents add reject_delay_time integer not null default 0;"
									   "alter table agents add busy_delay_time  integer not null default 0;");
	switch_cache_db_test_reactive(dbh, "select count(no_answer_delay_time) from agents", NULL, "alter table agents add no_answer_delay_time integer not null default 0;");
	switch_cache_db_test_reactive(dbh, "select count(ready_time) from agents", "drop table agents", agents_sql);
	switch_cache_db_test_reactive(dbh, "select count(queue) from tiers", "drop table tiers" , tiers_sql);

	switch_cache_db_release_db_handle(&dbh);

	/* Reset a unclean shutdown */
	sql = switch_mprintf("update agents set state = 'Waiting', uuid = '' where system = 'single_box';"
						 "update tiers set state = 'Ready' where agent IN (select name from agents where system = 'single_box');"
						 "update members set state = '%q', session_uuid = '' where system = 'single_box';",
						 cc_member_state2str(CC_MEMBER_STATE_ABANDONED));
	cc_execute_sql(NULL, sql, NULL);
	switch_safe_free(sql);

	/* Loading queue into memory struct */
	if ((x_queues = switch_xml_child(cfg, "queues"))) {
		for (x_queue = switch_xml_child(x_queues, "queue"); x_queue; x_queue = x_queue->next) {
			load_queue(switch_xml_attr_soft(x_queue, "name"));
		}
	}

	/* Importing from XML config Agents */
	if ((x_agents = switch_xml_child(cfg, "agents"))) {
		for (x_agent = switch_xml_child(x_agents, "agent"); x_agent; x_agent = x_agent->next) {
			const char *agent = switch_xml_attr(x_agent, "name");
			if (agent) {
				load_agent(agent);
			}
		}
	}

	/* Importing from XML config Agent Tiers */
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
						cc_tier_add(queue_name, agent, cc_tier_state2str(CC_TIER_STATE_READY), 0, 0);
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

end:
	switch_mutex_unlock(globals.mutex);

	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

static void playback_array(switch_core_session_t *session, const char *str) {
	if (str && !strncmp(str, "ARRAY::", 7)) {
		char *i = (char*) str + 7, *j = i;
		while (1) {
			if ((j = strstr(i, "::"))) {
				*j = 0;
			}
			switch_ivr_play_file(session, NULL, i, NULL);
			if (!j) break;
			i = j + 2;
		}
	} else {
		switch_ivr_play_file(session, NULL, str, NULL);
	}
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
	switch_core_session_t *member_session = switch_core_session_locate(h->member_session_uuid);
	switch_event_t *ovars;
	switch_time_t t_agent_called = 0;
	switch_time_t t_agent_answered = 0;
	switch_time_t t_member_called = atoi(h->member_joined_epoch);
	switch_event_t *event = NULL;

	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);

	/* member is gone before we could process it */
	if (!member_session) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Member %s <%s> with uuid %s in queue %s is gone just before we assigned an agent\n", h->member_cid_name, h->member_cid_number, h->member_session_uuid, h->queue_name);
	
	
		 sql = switch_mprintf("UPDATE members SET state = '%q', session_uuid = '', abandoned_epoch = '%" SWITCH_TIME_T_FMT "' WHERE system = 'single_box' AND uuid = '%q' AND state != '%q'",
				cc_member_state2str(CC_MEMBER_STATE_ABANDONED), local_epoch_time_now(NULL), h->member_uuid, cc_member_state2str(CC_MEMBER_STATE_ABANDONED));

		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);
		goto done;
	}

	/* Proceed contact the agent to offer the member */
	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_t *member_channel = switch_core_session_get_channel(member_session);
		switch_caller_profile_t *member_profile = switch_channel_get_caller_profile(member_channel);
		const char *member_dnis = member_profile->rdnis;

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", h->queue_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "agent-offering");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", h->agent_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-Type", h->agent_type);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-System", h->agent_system);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-UUID", h->member_uuid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-Session-UUID", h->member_session_uuid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Name", h->member_cid_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Number", h->member_cid_number);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-DNIS", member_dnis);
		switch_event_fire(&event);
	}

	/* CallBack Mode */
	if (!strcasecmp(h->agent_type, CC_AGENT_TYPE_CALLBACK)) {
		switch_channel_t *member_channel = switch_core_session_get_channel(member_session);
		char *cid_name = NULL;
		const char *cid_name_prefix = NULL;
		if ((cid_name_prefix = switch_channel_get_variable(member_channel, "cc_outbound_cid_name_prefix"))) {
			cid_name = switch_mprintf("%s%s", cid_name_prefix, h->member_cid_name);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Setting outbound caller_id_name to: %s\n", cid_name);
		}

		switch_event_create(&ovars, SWITCH_EVENT_REQUEST_PARAMS);
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_queue", "%s", h->queue_name);
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_member_uuid", "%s", h->member_uuid);
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_member_session_uuid", "%s", h->member_session_uuid);
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_member_pre_answer_uuid", "%s", h->member_uuid);
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_agent", "%s", h->agent_name);
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_agent_type", "%s", h->agent_type);
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "cc_side", "%s", "agent");
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "loopback_bowout", "false");
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "loopback_bowout_on_execute", "false");
		switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "ignore_early_media", "true");

		switch_channel_process_export(member_channel, NULL, ovars, "cc_export_vars");

		t_agent_called = local_epoch_time_now(NULL);

		dialstr = switch_channel_expand_variables(member_channel, h->originate_string);
		status = switch_ivr_originate(NULL, &agent_session, &cause, dialstr, 60, NULL, cid_name ? cid_name : h->member_cid_name, h->member_cid_number, NULL, ovars, SOF_NONE, NULL);
		if (dialstr != h->originate_string) {
			switch_safe_free(dialstr);
		}
		switch_safe_free(cid_name);

		switch_event_destroy(&ovars);
	/* UUID Standby Mode */
	} else if (!strcasecmp(h->agent_type, CC_AGENT_TYPE_UUID_STANDBY)) {
		agent_session = switch_core_session_locate(h->agent_uuid);
		if (agent_session) {
			switch_channel_t *agent_channel = switch_core_session_get_channel(agent_session);
			switch_event_t *event;
			const char *cc_warning_tone = switch_channel_get_variable(agent_channel, "cc_warning_tone");

			switch_channel_set_variable(agent_channel, "cc_side", "agent");
			switch_channel_set_variable(agent_channel, "cc_queue", h->queue_name);
			switch_channel_set_variable(agent_channel, "cc_agent", h->agent_name);
			switch_channel_set_variable(agent_channel, "cc_agent_type", h->agent_type);
			switch_channel_set_variable(agent_channel, "cc_member_uuid", h->member_uuid);
			switch_channel_set_variable(agent_channel, "cc_member_session_uuid", h->member_session_uuid);

			/* Playback this to the agent */
			if (cc_warning_tone && switch_event_create(&event, SWITCH_EVENT_COMMAND) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "call-command", "execute");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "execute-app-name", "playback");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "execute-app-arg", cc_warning_tone);
				switch_core_session_queue_private_event(agent_session, &event, SWITCH_TRUE);
			}

			status = SWITCH_STATUS_SUCCESS;
		} else {
			cc_agent_update("status", cc_agent_status2str(CC_AGENT_STATUS_LOGGED_OUT), h->agent_name);
			cc_agent_update("uuid", "", h->agent_name);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Invalid agent type '%s' for agent '%s', aborting member offering", h->agent_type, h->agent_name);
		cause = SWITCH_CAUSE_USER_NOT_REGISTERED;
	}

	/* Originate/Bridge is not finished, processing the return value */
	if (status == SWITCH_STATUS_SUCCESS) {
		/* Agent Answered */
		const char *agent_uuid = switch_core_session_get_uuid(agent_session);
		switch_channel_t *member_channel = switch_core_session_get_channel(member_session);
		switch_channel_t *agent_channel = switch_core_session_get_channel(agent_session);
		const char *other_loopback_leg_uuid = switch_channel_get_variable(agent_channel, "other_loopback_leg_uuid");
		const char *o_announce = NULL;

		switch_channel_set_variable(agent_channel, "cc_member_pre_answer_uuid", NULL);

		/* Our agent channel is a loopback. Try to find if a real channel is bridged to it in order
		   to use it as our new agent channel.
		   - Locate the loopback-b channel using 'other_loopback_leg_uuid' variable
		   - Locate the real agent channel using 'signal_bond' variable from loopback-b
		*/
		if (other_loopback_leg_uuid) {
			switch_core_session_t *other_loopback_session = NULL;

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Agent '%s' is a loopback channel. Searching for real channel...\n", h->agent_name);

			if ((other_loopback_session = switch_core_session_locate(other_loopback_leg_uuid))) {
				switch_channel_t *other_loopback_channel = switch_core_session_get_channel(other_loopback_session);
				const char *real_uuid = NULL;

				/* Wait for the real channel to be fully bridged */
				switch_channel_wait_for_flag(other_loopback_channel, CF_BRIDGED, SWITCH_TRUE, 5000, member_channel);

				real_uuid = switch_channel_get_partner_uuid(other_loopback_channel);
				switch_channel_set_variable(other_loopback_channel, "cc_member_pre_answer_uuid", NULL);

				/* Switch the agent session */
				if (real_uuid) {
					switch_core_session_rwunlock(agent_session);
					if (!(agent_session = switch_core_session_locate(real_uuid))) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Real session is already gone (agent '%s')\n", h->agent_name);
						sql = switch_mprintf("UPDATE members SET state = '%q', serving_agent = '', serving_system = ''"
											 " WHERE serving_agent = '%q' AND serving_system = '%q' AND uuid = '%q' AND system = 'single_box'",
											 cc_member_state2str(CC_MEMBER_STATE_WAITING), h->agent_name, h->agent_system, h->member_uuid);
						cc_execute_sql(NULL, sql, NULL);
						switch_safe_free(sql);
						goto done;
					}
					agent_uuid = switch_core_session_get_uuid(agent_session);
					agent_channel = switch_core_session_get_channel(agent_session);

					if (!switch_channel_test_flag(agent_channel, CF_BRIDGED)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Timeout waiting for real channel to be bridged (agent '%s')\n", h->agent_name);
					} else {
						switch_channel_set_variable(agent_channel, "cc_queue", h->queue_name);
						switch_channel_set_variable(agent_channel, "cc_agent", h->agent_name);
						switch_channel_set_variable(agent_channel, "cc_agent_type", h->agent_type);
						switch_channel_set_variable(agent_channel, "cc_member_uuid", h->member_uuid);
						switch_channel_set_variable(agent_channel, "cc_member_session_uuid", h->member_session_uuid);

						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Real channel found behind loopback agent '%s'\n", h->agent_name);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Failed to find a real channel behind loopback agent '%s'\n", h->agent_name);
				}

				switch_core_session_rwunlock(other_loopback_session);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Failed to locate loopback-b channel of agent '%s'\n", h->agent_name);
			}
		}


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
		t_agent_answered = local_epoch_time_now(NULL);

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_caller_profile_t *member_profile = switch_channel_get_caller_profile(member_channel);
			const char *member_dnis = member_profile->rdnis;

			switch_channel_event_set_data(agent_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", h->queue_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "bridge-agent-start");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", h->agent_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-System", h->agent_system);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-UUID", agent_uuid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Called-Time", "%" SWITCH_TIME_T_FMT, t_agent_called);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Answered-Time", "%" SWITCH_TIME_T_FMT, t_agent_answered);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Member-Joined-Time", "%" SWITCH_TIME_T_FMT, t_member_called);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-UUID", h->member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-Session-UUID", h->member_session_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Name", h->member_cid_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Number", h->member_cid_number);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-DNIS", member_dnis);
			switch_event_fire(&event);
		}
		/* for xml_cdr needs */
		switch_channel_set_variable(member_channel, "cc_agent", h->agent_name);
		switch_channel_set_variable_printf(member_channel, "cc_queue_answered_epoch", "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL)); 

		/* Set UUID of the Agent channel */
		sql = switch_mprintf("UPDATE agents SET uuid = '%q', last_bridge_start = '%" SWITCH_TIME_T_FMT "', calls_answered = calls_answered + 1, no_answer_count = 0"
				" WHERE name = '%q' AND system = '%q'",
				agent_uuid, local_epoch_time_now(NULL),
				h->agent_name, h->agent_system);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Change the agents Status in the tiers */
		cc_tier_update("state", cc_tier_state2str(CC_TIER_STATE_ACTIVE_INBOUND), h->queue_name, h->agent_name);
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

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Agent %s answered \"%s\" <%s> from queue %s%s\n",
				h->agent_name, h->member_cid_name, h->member_cid_number, h->queue_name, (h->record_template?" (Recorded)":""));

		if ((o_announce = switch_channel_get_variable(member_channel, "cc_outbound_announce"))) {
			playback_array(agent_session, o_announce);
		}

		switch_ivr_uuid_bridge(h->member_session_uuid, switch_core_session_get_uuid(agent_session));

		switch_channel_set_variable(member_channel, "cc_agent_uuid", agent_uuid);

		/* This is used for the waiting caller to quit waiting for a agent */
		switch_channel_set_variable(member_channel, "cc_agent_found", "true");

		/* Wait until the member hangup or the agent hangup.  This will quit also if the agent transfer the call */
		while(switch_channel_up(member_channel) && switch_channel_up(agent_channel) && globals.running) {
			switch_yield(100000);
		}
		tiers_state = CC_TIER_STATE_READY;

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(agent_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", h->queue_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "bridge-agent-end");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Hangup-Cause", switch_channel_cause2str(cause));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", h->agent_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-System", h->agent_system);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-UUID", agent_uuid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Called-Time", "%" SWITCH_TIME_T_FMT, t_agent_called);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Answered-Time", "%" SWITCH_TIME_T_FMT, t_agent_answered);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Member-Joined-Time", "%" SWITCH_TIME_T_FMT,  t_member_called);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Bridge-Terminated-Time", "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-UUID", h->member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-Session-UUID", h->member_session_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Name", h->member_cid_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Number", h->member_cid_number);
			switch_event_fire(&event);
		}
		/* for xml_cdr needs */
		switch_channel_set_variable_printf(member_channel, "cc_queue_terminated_epoch", "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL));

		/* Update Agents Items */
		/* Do not remove uuid of the agent if we are a standby agent */
		sql = switch_mprintf("UPDATE agents SET %s last_bridge_end = %" SWITCH_TIME_T_FMT ", talk_time = talk_time + (%" SWITCH_TIME_T_FMT "-last_bridge_start) WHERE name = '%q' AND system = '%q';"
				, (strcasecmp(h->agent_type, CC_AGENT_TYPE_UUID_STANDBY)?"uuid = '',":""), local_epoch_time_now(NULL), local_epoch_time_now(NULL), h->agent_name, h->agent_system);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Remove the member entry from the db (Could become optional to support latter processing) */
		sql = switch_mprintf("DELETE FROM members WHERE system = 'single_box' AND uuid = '%q'", h->member_uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Caller off event */
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(member_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", h->queue_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "member-queue-end");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Hangup-Cause", switch_channel_cause2str(cause));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Cause", "Terminated");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", h->agent_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-System", h->agent_system);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-UUID", agent_uuid);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Called-Time", "%" SWITCH_TIME_T_FMT, t_agent_called);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Answered-Time", "%" SWITCH_TIME_T_FMT, t_agent_answered);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Member-Leaving-Time", "%" SWITCH_TIME_T_FMT,  local_epoch_time_now(NULL));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Member-Joined-Time", "%" SWITCH_TIME_T_FMT, t_member_called);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-UUID", h->member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-Session-UUID", h->member_session_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Name", h->member_cid_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Number", h->member_cid_number);
			switch_event_fire(&event);
		}


	} else {
		/* Agent didn't answer or originate failed */
		int delay_next_agent_call = 0;
		sql = switch_mprintf("UPDATE members SET state = '%q', serving_agent = '', serving_system = ''"
				" WHERE serving_agent = '%q' AND serving_system = '%q' AND uuid = '%q' AND system = 'single_box'",
				cc_member_state2str(CC_MEMBER_STATE_WAITING),
				h->agent_name, h->agent_system, h->member_uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Agent %s Origination Canceled : %s\n", h->agent_name, switch_channel_cause2str(cause));

		switch (cause) {
			/* When we hang-up agents that did not answer in ring-all strategy */
			case SWITCH_CAUSE_ORIGINATOR_CANCEL:
				break;
			/* Busy: Do Not Disturb, Circuit congestion */
			case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
			case SWITCH_CAUSE_USER_BUSY:
				delay_next_agent_call = (h->busy_delay_time > delay_next_agent_call? h->busy_delay_time : delay_next_agent_call);
				break;
			/* Reject: User rejected the call */
			case SWITCH_CAUSE_CALL_REJECTED:
				delay_next_agent_call = (h->reject_delay_time > delay_next_agent_call? h->reject_delay_time : delay_next_agent_call);
				break;
			/* Protection againts super fast loop due to unregistrer */			
			case SWITCH_CAUSE_USER_NOT_REGISTERED:
				delay_next_agent_call = 5;
				break;
			/* No answer: Destination does not answer for some other reason */
			default:
				delay_next_agent_call = (h->no_answer_delay_time > delay_next_agent_call? h->no_answer_delay_time : delay_next_agent_call);

				tiers_state = CC_TIER_STATE_NO_ANSWER;

				/* Update Agent NO Answer count */
				sql = switch_mprintf("UPDATE agents SET no_answer_count = no_answer_count + 1 WHERE name = '%q' AND system = '%q';",
						h->agent_name, h->agent_system);
				cc_execute_sql(NULL, sql, NULL);
				switch_safe_free(sql);

				/* Put Agent on break because he didn't answer often */
				if (h->max_no_answer > 0 && (h->no_answer_count + 1) >= h->max_no_answer) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Agent %s reach maximum no answer of %d, Putting agent on break\n",
							h->agent_name, h->max_no_answer);
					cc_agent_update("status", cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), h->agent_name);
				}
				break;
		}

		/* Put agent to sleep for some time if necessary */
		if (delay_next_agent_call > 0) {
			char ready_epoch[64];
			switch_snprintf(ready_epoch, sizeof(ready_epoch), "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL) + delay_next_agent_call);
			cc_agent_update("ready_time", ready_epoch , h->agent_name);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Agent %s sleeping for %d seconds\n", h->agent_name, delay_next_agent_call);
		}

		/* Fire up event when contact agent fails */
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", h->queue_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "bridge-agent-fail");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Hangup-Cause", switch_channel_cause2str(cause));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent", h->agent_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Agent-System", h->agent_system);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Called-Time", "%" SWITCH_TIME_T_FMT, t_agent_called);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Agent-Aborted-Time", "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-UUID", h->member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-Session-UUID", h->member_session_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Name", h->member_cid_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Number", h->member_cid_number);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Member-Joined-Time", "%" SWITCH_TIME_T_FMT, t_member_called);
			switch_event_fire(&event);
		}

	}

done:
	/* Make Agent Available Again */
	sql = switch_mprintf(
			"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND queue = '%q' AND (state = '%q' OR state = '%q' OR state = '%q');"
			"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND NOT queue = '%q' AND state = '%q'"
			, cc_tier_state2str(tiers_state), h->agent_name, h->queue_name, cc_tier_state2str(CC_TIER_STATE_ACTIVE_INBOUND), cc_tier_state2str(CC_TIER_STATE_STANDBY), cc_tier_state2str(CC_TIER_STATE_OFFERING),
			cc_tier_state2str(CC_TIER_STATE_READY), h->agent_name, h->queue_name, cc_tier_state2str(CC_TIER_STATE_STANDBY));
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
	const char *queue_name;
	const char *system;
	const char *member_uuid;
	const char *member_session_uuid;
	const char *member_cid_number;
	const char *member_cid_name;
	const char *member_joined_epoch;
	const char *member_score;
	const char *strategy;
	const char *record_template;
	switch_bool_t tier_rules_apply;
	uint32_t tier_rule_wait_second;
	switch_bool_t tier_rule_wait_multiply_level;
	switch_bool_t tier_rule_no_agent_no_wait;
	switch_bool_t agent_found;

	int tier;
	int tier_agent_available;
};
typedef struct agent_callback agent_callback_t;

static int agents_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	agent_callback_t *cbt = (agent_callback_t *) pArg;
	char *sql = NULL;
	char res[256];
	const char *agent_system = argv[0];
	const char *agent_name = argv[1];
	const char *agent_status = argv[2];
	const char *agent_originate_string = argv[3];
	const char *agent_no_answer_count = argv[4];
	const char *agent_max_no_answer = argv[5];
	const char *agent_reject_delay_time = argv[6];
	const char *agent_busy_delay_time = argv[7];
	const char *agent_no_answer_delay_time = argv[8];
	const char *agent_tier_state = argv[9];
	const char *agent_last_bridge_end = argv[10];
	const char *agent_wrap_up_time = argv[11];
	const char *agent_state = argv[12];
	const char *agent_ready_time = argv[13];
	const char *agent_tier_position = argv[14];
	const char *agent_tier_level = argv[15];
	const char *agent_type = argv[16];
	const char *agent_uuid = argv[17];

	switch_bool_t contact_agent = SWITCH_TRUE;

	cbt->agent_found = SWITCH_TRUE;

	/* Check if we switch to a different tier, if so, check if we should continue further for that member */

	if (cbt->tier_rules_apply == SWITCH_TRUE && atoi(agent_tier_level) > cbt->tier) {
		/* Continue if no agent was logged in in the previous tier and noagent = true */
		if (cbt->tier_rule_no_agent_no_wait == SWITCH_TRUE && cbt->tier_agent_available == 0) {
			cbt->tier = atoi(agent_tier_level);
			/* Multiple the tier level by the tier wait time */
		} else if (cbt->tier_rule_wait_multiply_level == SWITCH_TRUE && (long) local_epoch_time_now(NULL) - atol(cbt->member_joined_epoch) >= atoi(agent_tier_level) * cbt->tier_rule_wait_second) {
			cbt->tier = atoi(agent_tier_level);
			cbt->tier_agent_available = 0;
			/* Just check if joined is bigger than next tier wait time */
		} else if (cbt->tier_rule_wait_multiply_level == SWITCH_FALSE && (long) local_epoch_time_now(NULL) - atol(cbt->member_joined_epoch) >= cbt->tier_rule_wait_second) {
			cbt->tier = atoi(agent_tier_level);
			cbt->tier_agent_available = 0;
		} else {
			/* We are not allowed to continue to the next tier of agent */
			return 1;
		}
	}
	cbt->tier_agent_available++;

	/* If Agent is not in a acceptable tier state, continue */
	if (! (!strcasecmp(agent_tier_state, cc_tier_state2str(CC_TIER_STATE_NO_ANSWER)) || !strcasecmp(agent_tier_state, cc_tier_state2str(CC_TIER_STATE_READY)))) {
		contact_agent = SWITCH_FALSE;
	}
	if (! (!strcasecmp(agent_state, cc_agent_state2str(CC_AGENT_STATE_WAITING)))) {
		contact_agent = SWITCH_FALSE;
	}
	if (! (atol(agent_last_bridge_end) < ((long) local_epoch_time_now(NULL) - atol(agent_wrap_up_time)))) {
		contact_agent = SWITCH_FALSE;
	}
	if (! (atol(agent_ready_time) <= (long) local_epoch_time_now(NULL))) {
		contact_agent = SWITCH_FALSE;
	}
	if (! (strcasecmp(agent_status, cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK)))) {
		contact_agent = SWITCH_FALSE;
	}

	if (contact_agent == SWITCH_FALSE) {
		return 0; /* Continue to next Agent */
	}

	/* If agent isn't on this box */
	if (strcasecmp(agent_system,"single_box" /* SELF */)) {
		if (!strcasecmp(cbt->strategy, "ring-all")) {
			return 1; /* Abort finding agent for member if we found a match but for a different Server */
		} else {
			return 0; /* Skip this Agents only, so we can ring the other one */
		}
	}

	if (!strcasecmp(cbt->strategy,"ring-all")) {
		/* Check if member is a ring-all mode */
		sql = switch_mprintf("SELECT count(*) FROM members WHERE serving_agent = 'ring-all' AND uuid = '%q' AND system = 'single_box'", cbt->member_uuid);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));

		switch_safe_free(sql);
	} else {
		/* Map the Agent to the member */
		sql = switch_mprintf("UPDATE members SET serving_agent = '%q', serving_system = 'single_box', state = '%q'"
				" WHERE state = '%q' AND uuid = '%q' AND system = 'single_box'", 
				agent_name, cc_member_state2str(CC_MEMBER_STATE_TRYING),
				cc_member_state2str(CC_MEMBER_STATE_WAITING), cbt->member_uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Check if we won the race to get the member to our selected agent (Used for Multi system purposes) */
		sql = switch_mprintf("SELECT count(*) FROM members WHERE serving_agent = '%q' AND serving_system = 'single_box' AND uuid = '%q' AND system = 'single_box'",
				agent_name, cbt->member_uuid);
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
				h->member_uuid = switch_core_strdup(h->pool, cbt->member_uuid);
				h->member_session_uuid = switch_core_strdup(h->pool, cbt->member_session_uuid);
				h->queue_strategy = switch_core_strdup(h->pool, cbt->strategy);
				h->originate_string = switch_core_strdup(h->pool, agent_originate_string);
				h->agent_name = switch_core_strdup(h->pool, agent_name);
				h->agent_system = switch_core_strdup(h->pool, "single_box");
				h->agent_status = switch_core_strdup(h->pool, agent_status);
				h->agent_type = switch_core_strdup(h->pool, agent_type);
				h->agent_uuid = switch_core_strdup(h->pool, agent_uuid);
				h->member_joined_epoch = switch_core_strdup(h->pool, cbt->member_joined_epoch); 
				h->member_cid_name = switch_core_strdup(h->pool, cbt->member_cid_name);
				h->member_cid_number = switch_core_strdup(h->pool, cbt->member_cid_number);
				h->queue_name = switch_core_strdup(h->pool, cbt->queue_name);
				h->record_template = switch_core_strdup(h->pool, cbt->record_template);
				h->no_answer_count = atoi(agent_no_answer_count);
				h->max_no_answer = atoi(agent_max_no_answer);
				h->reject_delay_time = atoi(agent_reject_delay_time);
				h->busy_delay_time = atoi(agent_busy_delay_time);
				h->no_answer_delay_time = atoi(agent_no_answer_delay_time);

				if (!strcasecmp(cbt->strategy, "top-down")) {
					switch_core_session_t *member_session = switch_core_session_locate(cbt->member_session_uuid);
					if (member_session) {
						switch_channel_t *member_channel = switch_core_session_get_channel(member_session);
						switch_channel_set_variable(member_channel, "cc_last_agent_tier_position", agent_tier_position);
						switch_channel_set_variable(member_channel, "cc_last_agent_tier_level", agent_tier_level);
						switch_core_session_rwunlock(member_session);
					}
				}
				cc_agent_update("state", cc_agent_state2str(CC_AGENT_STATE_RECEIVING), h->agent_name);

				sql = switch_mprintf(
						"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND queue = '%q';"
						"UPDATE tiers SET state = '%q' WHERE agent = '%q' AND NOT queue = '%q' AND state = '%q';",
						cc_tier_state2str(CC_TIER_STATE_OFFERING), h->agent_name, h->queue_name,
						cc_tier_state2str(CC_TIER_STATE_STANDBY), h->agent_name, h->queue_name, cc_tier_state2str(CC_TIER_STATE_READY));
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
	switch_bool_t tier_rules_apply;
	uint32_t tier_rule_wait_second;
	switch_bool_t tier_rule_wait_multiply_level;
	switch_bool_t tier_rule_no_agent_no_wait;
	uint32_t discard_abandoned_after;
	agent_callback_t cbt;
	const char *member_state = NULL;
	const char *member_abandoned_epoch = NULL;
	memset(&cbt, 0, sizeof(cbt));

	cbt.queue_name = argv[0];
	cbt.member_uuid = argv[1];
	cbt.member_session_uuid = argv[2];
	cbt.member_cid_number = argv[3];
	cbt.member_cid_name = argv[4];
	cbt.member_joined_epoch = argv[5];
	cbt.member_score = argv[6];
	member_state = argv[7];
	member_abandoned_epoch = argv[8];

	if (!cbt.queue_name || !(queue = get_queue(cbt.queue_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Queue %s not found locally, skip this member\n", cbt.queue_name);
		goto end;
	} else {
		queue_name = strdup(queue->name);
		queue_strategy = strdup(queue->strategy);
		tier_rules_apply = queue->tier_rules_apply;
		tier_rule_wait_second = queue->tier_rule_wait_second;
		tier_rule_wait_multiply_level = queue->tier_rule_wait_multiply_level;
		tier_rule_no_agent_no_wait = queue->tier_rule_no_agent_no_wait;
		discard_abandoned_after = queue->discard_abandoned_after;

		if (queue->record_template) {
			queue_record_template = strdup(queue->record_template);
		}

		queue_rwunlock(queue);
	}

	/* Checking for cleanup Abandonded calls from the db */
	if (!strcasecmp(member_state, cc_member_state2str(CC_MEMBER_STATE_ABANDONED))) {
		switch_time_t abandoned_epoch = atoll(member_abandoned_epoch);
		if (abandoned_epoch == 0) {
			abandoned_epoch = atoll(cbt.member_joined_epoch);
		}
		/* Once we pass a certain point, we want to get rid of the abandoned call */
		if (abandoned_epoch + discard_abandoned_after < local_epoch_time_now(NULL)) {
			sql = switch_mprintf("DELETE FROM members WHERE system = 'single_box' AND uuid = '%q' AND (abandoned_epoch = '%" SWITCH_TIME_T_FMT "' OR joined_epoch = '%q')", cbt.member_uuid, abandoned_epoch, cbt.member_joined_epoch);
			cc_execute_sql(NULL, sql, NULL);
			switch_safe_free(sql);
		}
		/* Skip this member */
		goto end;
	} 

	/* Check if member is in the queue waiting */
	if (zstr(cbt.member_session_uuid)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Member %s <%s> in Queue %s have no session uuid, skip this member\n", cbt.member_cid_name, cbt.member_cid_number, cbt.queue_name);
	}

	cbt.tier = 0;
	cbt.tier_agent_available = 0;

	cbt.tier_rules_apply = tier_rules_apply;
	cbt.tier_rule_wait_second = tier_rule_wait_second;
	cbt.tier_rule_wait_multiply_level = tier_rule_wait_multiply_level;
	cbt.tier_rule_no_agent_no_wait = tier_rule_no_agent_no_wait;

	cbt.strategy = queue_strategy;
	cbt.record_template = queue_record_template;
	cbt.agent_found = SWITCH_FALSE;

	if (!strcasecmp(queue->strategy, "top-down")) {
		/* WARNING this use channel variable to help dispatch... might need to be reviewed to save it in DB to make this multi server prooft in the future */
		switch_core_session_t *member_session = switch_core_session_locate(cbt.member_session_uuid);
		int position = 0, level = 0;
		const char *last_agent_tier_position, *last_agent_tier_level;
		if (member_session) {
			switch_channel_t *member_channel = switch_core_session_get_channel(member_session);

			if ((last_agent_tier_position = switch_channel_get_variable(member_channel, "cc_last_agent_tier_position"))) {
				position = atoi(last_agent_tier_position);
			}
			if ((last_agent_tier_level = switch_channel_get_variable(member_channel, "cc_last_agent_tier_level"))) {
				level = atoi(last_agent_tier_level);
			}
			switch_core_session_rwunlock(member_session);
		}

		sql = switch_mprintf("SELECT system, name, status, contact, no_answer_count, max_no_answer, reject_delay_time, busy_delay_time, no_answer_delay_time, tiers.state, agents.last_bridge_end, agents.wrap_up_time, agents.state, agents.ready_time, tiers.position as tiers_position, tiers.level as tiers_level, agents.type, agents.uuid, agents.last_offered_call as agents_last_offered_call, 1 as dyn_order FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent)"
				" WHERE tiers.queue = '%q'"
				" AND (agents.status = '%q' OR agents.status = '%q' OR agents.status = '%q')"
				" AND tiers.position > %d"
				" AND tiers.level = %d"
				" UNION "
				"SELECT system, name, status, contact, no_answer_count, max_no_answer, reject_delay_time, busy_delay_time, no_answer_delay_time, tiers.state, agents.last_bridge_end, agents.wrap_up_time, agents.state, agents.ready_time, tiers.position as tiers_position, tiers.level as tiers_level, agents.type, agents.uuid, agents.last_offered_call as agents_last_offered_call, 2 as dyn_order FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent)"
				" WHERE tiers.queue = '%q'"
				" AND (agents.status = '%q' OR agents.status = '%q' OR agents.status = '%q')"
				" ORDER BY dyn_order asc, tiers_level, tiers_position, agents_last_offered_call",
				queue_name,
				cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE), cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE_ON_DEMAND),
				position,
				level,
				queue_name,
				cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE), cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE_ON_DEMAND)
				);
	} else if (!strcasecmp(queue->strategy, "round-robin")) {
		sql = switch_mprintf("SELECT system, name, status, contact, no_answer_count, max_no_answer, reject_delay_time, busy_delay_time, no_answer_delay_time, tiers.state, agents.last_bridge_end, agents.wrap_up_time, agents.state, agents.ready_time, tiers.position as tiers_position, tiers.level as tiers_level, agents.type, agents.uuid, agents.last_offered_call as agents_last_offered_call, 1 as dyn_order FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent)"
				" WHERE tiers.queue = '%q'"
				" AND (agents.status = '%q' OR agents.status = '%q' OR agents.status = '%q')"
				" AND tiers.position > (SELECT tiers.position FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent) WHERE tiers.queue = '%q' AND agents.last_offered_call > 0 ORDER BY agents.last_offered_call DESC LIMIT 1)"
				" AND tiers.level = (SELECT tiers.level FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent) WHERE tiers.queue = '%q' AND agents.last_offered_call > 0 ORDER BY agents.last_offered_call DESC LIMIT 1)"
				" UNION "
				"SELECT system, name, status, contact, no_answer_count, max_no_answer, reject_delay_time, busy_delay_time, no_answer_delay_time, tiers.state, agents.last_bridge_end, agents.wrap_up_time, agents.state, agents.ready_time, tiers.position as tiers_position, tiers.level as tiers_level, agents.type, agents.uuid, agents.last_offered_call as agents_last_offered_call, 2 as dyn_order FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent)"
				" WHERE tiers.queue = '%q'"
				" AND (agents.status = '%q' OR agents.status = '%q' OR agents.status = '%q')"
				" ORDER BY dyn_order asc, tiers_level, tiers_position, agents_last_offered_call",
				queue_name,
				cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE), cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE_ON_DEMAND),
				queue_name,
				queue_name,
				queue_name,
				cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE), cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE_ON_DEMAND)
				);

	} else {

		if (!strcasecmp(queue->strategy, "longest-idle-agent")) {
			sql_order_by = switch_mprintf("level, agents.last_offered_call, position");
		} else if (!strcasecmp(queue_strategy, "agent-with-least-talk-time")) {
			sql_order_by = switch_mprintf("level, agents.talk_time, position");
		} else if (!strcasecmp(queue_strategy, "agent-with-fewest-calls")) {
			sql_order_by = switch_mprintf("level, agents.calls_answered, position");
		} else if (!strcasecmp(queue_strategy, "ring-all")) {
			sql = switch_mprintf("UPDATE members SET state = '%q' WHERE state = '%q' AND uuid = '%q' AND system = 'single_box'",
					cc_member_state2str(CC_MEMBER_STATE_TRYING), cc_member_state2str(CC_MEMBER_STATE_WAITING), cbt.member_uuid);
			cc_execute_sql(NULL, sql, NULL);
			switch_safe_free(sql);
			sql_order_by = switch_mprintf("level, position");
		} else if(!strcasecmp(queue_strategy, "random")) {
			sql_order_by = switch_mprintf("level, random()");
		} else if(!strcasecmp(queue_strategy, "sequentially-by-agent-order")) {
			sql_order_by = switch_mprintf("level, position, agents.last_offered_call"); /* Default to last_offered_call, let add new strategy if needing it differently */
		} else {
			/* If the strategy doesn't exist, just fallback to the following */
			sql_order_by = switch_mprintf("level, position, agents.last_offered_call");
		}

		sql = switch_mprintf("SELECT system, name, status, contact, no_answer_count, max_no_answer, reject_delay_time, busy_delay_time, no_answer_delay_time, tiers.state, agents.last_bridge_end, agents.wrap_up_time, agents.state, agents.ready_time, tiers.position, tiers.level, agents.type, agents.uuid FROM agents LEFT JOIN tiers ON (agents.name = tiers.agent)"
				" WHERE tiers.queue = '%q'"
				" AND (agents.status = '%q' OR agents.status = '%q' OR agents.status = '%q')"
				" ORDER BY %q",
				queue_name,
				cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE), cc_agent_status2str(CC_AGENT_STATUS_ON_BREAK), cc_agent_status2str(CC_AGENT_STATUS_AVAILABLE_ON_DEMAND),
				sql_order_by);
		switch_safe_free(sql_order_by);

	}

	cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, agents_callback, &cbt /* Call back variables */);

	switch_safe_free(sql);

	/* We update a field in the queue struct so we can kick caller out if waiting for too long with no agent */
	if (!cbt.queue_name || !(queue = get_queue(cbt.queue_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Queue %s not found locally, skip this member\n", cbt.queue_name);
		goto end;
	} else {
		queue->last_agent_exist_check = local_epoch_time_now(NULL);
		if (cbt.agent_found) {
			queue->last_agent_exist = queue->last_agent_exist_check;
		}
		queue_rwunlock(queue);
	}

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
		sql = switch_mprintf("SELECT queue,uuid,session_uuid,cid_number,cid_name,joined_epoch,(%" SWITCH_TIME_T_FMT "-joined_epoch)+base_score+skill_score AS score, state, abandoned_epoch FROM members"
				" WHERE state = '%q' OR state = '%q' OR (serving_agent = 'ring-all' AND state = '%q') ORDER BY score DESC",
				local_epoch_time_now(NULL),
				cc_member_state2str(CC_MEMBER_STATE_WAITING), cc_member_state2str(CC_MEMBER_STATE_ABANDONED), cc_member_state2str(CC_MEMBER_STATE_TRYING));

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
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
	switch_thread_create(&thread, thd_attr, cc_agent_dispatch_thread_run, NULL, globals.pool);
}

struct member_thread_helper {
	const char *queue_name;
	const char *member_uuid;
	const char *member_session_uuid;
	const char *member_cid_name;
	const char *member_cid_number;
	switch_time_t t_member_called;
	cc_member_cancel_reason_t member_cancel_reason;

	int running;
	switch_memory_pool_t *pool;
};

void *SWITCH_THREAD_FUNC cc_member_thread_run(switch_thread_t *thread, void *obj)
{
	struct member_thread_helper *m = (struct member_thread_helper *) obj;
	switch_core_session_t *member_session = switch_core_session_locate(m->member_session_uuid);
	switch_channel_t *member_channel = NULL;

	if (member_session) {
		member_channel = switch_core_session_get_channel(member_session);
	} else {
		switch_core_destroy_memory_pool(&m->pool);
		return NULL;
	}

	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);

	while(switch_channel_ready(member_channel) && m->running && globals.running) {
		cc_queue_t *queue = NULL;

		if (!m->queue_name || !(queue = get_queue(m->queue_name))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_WARNING, "Queue %s not found\n", m->queue_name);
			break;
		}
		/* Make the Caller Leave if he went over his max wait time */
		if (queue->max_wait_time > 0 && queue->max_wait_time <= local_epoch_time_now(NULL) - m->t_member_called) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member %s <%s> in queue '%s' reached max wait time\n", m->member_cid_name, m->member_cid_number, m->queue_name);
			m->member_cancel_reason = CC_MEMBER_CANCEL_REASON_TIMEOUT;
			switch_channel_set_flag_value(member_channel, CF_BREAK, 2);
		}

		if (queue->max_wait_time_with_no_agent > 0 && queue->last_agent_exist_check > queue->last_agent_exist) {
			if (queue->last_agent_exist_check - queue->last_agent_exist >= queue->max_wait_time_with_no_agent) {
				if (queue->max_wait_time_with_no_agent_time_reached > 0) {
					if (queue->last_agent_exist_check - m->t_member_called >= queue->max_wait_time_with_no_agent + queue->max_wait_time_with_no_agent_time_reached) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member %s <%s> in queue '%s' reached max wait of %d sec. with no agent plus join grace period of %d sec.\n", m->member_cid_name, m->member_cid_number, m->queue_name, queue->max_wait_time_with_no_agent, queue->max_wait_time_with_no_agent_time_reached);
						m->member_cancel_reason = CC_MEMBER_CANCEL_REASON_NO_AGENT_TIMEOUT;
						switch_channel_set_flag_value(member_channel, CF_BREAK, 2);

					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member %s <%s> in queue '%s' reached max wait of %d sec. with no agent\n", m->member_cid_name, m->member_cid_number, m->queue_name, queue->max_wait_time_with_no_agent);
					m->member_cancel_reason = CC_MEMBER_CANCEL_REASON_NO_AGENT_TIMEOUT;
					switch_channel_set_flag_value(member_channel, CF_BREAK, 2);

				} 
			}
		}

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

		queue_rwunlock(queue);

		switch_yield(500000);
	}

	switch_core_session_rwunlock(member_session);
	switch_core_destroy_memory_pool(&m->pool);

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	switch_mutex_unlock(globals.mutex);

	return NULL; 
}

struct moh_dtmf_helper {
	const char *queue_name;
	const char *exit_keys;
	char dtmf;
};

static switch_status_t moh_on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen) {
	struct moh_dtmf_helper *h = (struct moh_dtmf_helper *) buf;

	switch (itype) {
		case SWITCH_INPUT_TYPE_DTMF:
			if (h->exit_keys && *(h->exit_keys)) {
				switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
				if (strchr(h->exit_keys, dtmf->digit)) {
					h->dtmf = dtmf->digit;
					return SWITCH_STATUS_BREAK;
				}
			}
			break;
		default:
			break;
	}

	return SWITCH_STATUS_SUCCESS;
}

#define CC_DESC "callcenter"
#define CC_USAGE "queue_name"

SWITCH_STANDARD_APP(callcenter_function)
{
	char *argv[6] = { 0 };
	char *mydata = NULL;
	cc_queue_t *queue = NULL;
	const char *queue_name = NULL;
	switch_core_session_t *member_session = session;
	switch_channel_t *member_channel = switch_core_session_get_channel(member_session);
	char *sql = NULL;
	char *member_session_uuid = switch_core_session_get_uuid(member_session);
	struct member_thread_helper *h = NULL;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	switch_channel_timetable_t *times = NULL;
	const char *cc_moh_override = switch_channel_get_variable(member_channel, "cc_moh_override");
	const char *cc_base_score = switch_channel_get_variable(member_channel, "cc_base_score");
	int cc_base_score_int = 0;
	const char *cur_moh = NULL;
	char start_epoch[64];
	switch_event_t *event;
	switch_time_t t_member_called = local_epoch_time_now(NULL);
	long abandoned_epoch = 0;
	switch_uuid_t smember_uuid;
	char member_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";
	switch_bool_t agent_found = SWITCH_FALSE;
	switch_bool_t moh_valid = SWITCH_TRUE;
	const char *p;

	if (!zstr(data)) {
		mydata = switch_core_session_strdup(member_session, data);
		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_WARNING, "No Queue name provided\n");
		goto end;
	}

	if (argv[0]) {
		queue_name = argv[0];
	}

	if (!queue_name || !(queue = get_queue(queue_name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_WARNING, "Queue %s not found\n", queue_name);
		goto end;
	}

	/* Make sure we answer the channel before getting the switch_channel_time_table_t answer time */
	switch_channel_answer(member_channel);

	/* Grab the start epoch of a channel */
	times = switch_channel_get_timetable(member_channel);
	switch_snprintf(start_epoch, sizeof(start_epoch), "%" SWITCH_TIME_T_FMT, times->answered / 1000000);

	/* Check if we support and have a queued abandoned member we can resume from */
	if (queue->abandoned_resume_allowed == SWITCH_TRUE) {
		char res[256];

		/* Check to see if agent already exist */
		sql = switch_mprintf("SELECT uuid FROM members WHERE queue = '%q' AND cid_number = '%q' AND state = '%q' ORDER BY abandoned_epoch DESC",
				queue_name, switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")), cc_member_state2str(CC_MEMBER_STATE_ABANDONED));
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);
		strncpy(member_uuid, res, sizeof(member_uuid));
		
		if (!zstr(member_uuid)) {
			sql = switch_mprintf("SELECT abandoned_epoch FROM members WHERE uuid = '%q'", member_uuid);
			cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
			switch_safe_free(sql);
			abandoned_epoch = atol(res);
		}
	}

	/* If no existing uuid is restored, let create a new one */
	if (abandoned_epoch == 0) {
		switch_uuid_get(&smember_uuid);
		switch_uuid_format(member_uuid, &smember_uuid);
	}

	switch_channel_set_variable(member_channel, "cc_side", "member");
	switch_channel_set_variable(member_channel, "cc_member_uuid", member_uuid);

	/* Add manually imported score */
	if (cc_base_score) {
		cc_base_score_int += atoi(cc_base_score);
	}

	/* If system, will add the total time the session is up to the base score */
	if (!switch_strlen_zero(start_epoch) && !strcasecmp("system", queue->time_base_score)) {
		cc_base_score_int += ((long) local_epoch_time_now(NULL) - atol(start_epoch));
	}

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(member_channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", queue_name);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Action", "member-queue-%s", (abandoned_epoch==0?"start":"resume"));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-UUID", member_uuid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-Session-UUID", member_session_uuid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Name", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Number", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")));
		switch_event_fire(&event);
	}
	/* for xml_cdr needs */
	switch_channel_set_variable_printf(member_channel, "cc_queue_joined_epoch", "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL));
	switch_channel_set_variable(member_channel, "cc_queue", queue_name);

	if (abandoned_epoch == 0) {
		/* Add the caller to the member queue */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member %s <%s> joining queue %s\n", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")), switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")), queue_name);

		sql = switch_mprintf("INSERT INTO members"
				" (queue,system,uuid,session_uuid,system_epoch,joined_epoch,base_score,skill_score,cid_number,cid_name,serving_agent,serving_system,state)"
				" VALUES('%q','single_box','%q','%q','%q','%" SWITCH_TIME_T_FMT "','%d','%d','%q','%q','%q','','%q')", 
				queue_name,
				member_uuid,
				member_session_uuid,
				start_epoch,
				local_epoch_time_now(NULL),
				cc_base_score_int,
				0 /*TODO SKILL score*/,
				switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")),
				switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")),
				(!strcasecmp(queue->strategy,"ring-all")?"ring-all":""),
				cc_member_state2str(CC_MEMBER_STATE_WAITING));
		cc_execute_sql(queue, sql, NULL);
		switch_safe_free(sql);
	} else {
		char res[256];

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member %s <%s> restoring it previous position in queue %s\n", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")), switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")), queue_name);

		/* Update abandoned member */
		sql = switch_mprintf("UPDATE members SET session_uuid = '%q', state = '%q', rejoined_epoch = '%" SWITCH_TIME_T_FMT "' WHERE uuid = '%q' AND state = '%q'",
				member_session_uuid, cc_member_state2str(CC_MEMBER_STATE_WAITING), local_epoch_time_now(NULL), member_uuid, cc_member_state2str(CC_MEMBER_STATE_ABANDONED)); 
		cc_execute_sql(queue, sql, NULL);
		switch_safe_free(sql);

		/* Confirm we took that member in */
		sql = switch_mprintf("SELECT abandoned_epoch FROM members WHERE uuid = '%q' AND session_uuid = '%q' AND state = '%q' AND queue = '%q'", member_uuid, member_session_uuid, cc_member_state2str(CC_MEMBER_STATE_WAITING), queue_name);
		cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
		switch_safe_free(sql);

		if (atol(res) == 0) {
			/* Failed to get the member !!! */
			/* TODO Loop back to just create a uuid and add the member as a new member */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_ERROR, "Member %s <%s> restoring action failed in queue %s, exiting\n", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")), switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")), queue_name);
			queue_rwunlock(queue);
			goto end;
		}

	}

	/* Send Event with queue count */
	cc_queue_count(queue_name);

	/* Start Thread that will playback different prompt to the channel */
	switch_core_new_memory_pool(&pool);
	h = switch_core_alloc(pool, sizeof(*h));

	h->pool = pool;
	h->member_uuid = switch_core_strdup(h->pool, member_uuid);
	h->member_session_uuid = switch_core_strdup(h->pool, member_session_uuid);
	h->member_cid_name = switch_core_strdup(h->pool, switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")));
	h->member_cid_number = switch_core_strdup(h->pool, switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")));
	h->queue_name = switch_core_strdup(h->pool, queue_name);
	h->t_member_called = t_member_called;
	h->member_cancel_reason = CC_MEMBER_CANCEL_REASON_NONE;
	h->running = 1;

	switch_threadattr_create(&thd_attr, h->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, cc_member_thread_run, h, h->pool);

	/* Playback MOH */
	if (cc_moh_override) {
		cur_moh = switch_core_session_strdup(member_session, cc_moh_override);
	} else {
		cur_moh = switch_core_session_strdup(member_session, queue->moh);
	}
	queue_rwunlock(queue);

	while (switch_channel_ready(member_channel)) {
		switch_input_args_t args = { 0 };
		struct moh_dtmf_helper ht;

		ht.exit_keys = switch_channel_get_variable(member_channel, "cc_exit_keys");
		ht.dtmf = '\0';
		args.input_callback = moh_on_dtmf;
		args.buf = (void *) &ht;
		args.buflen = sizeof(h);

		/* An agent was found, time to exit and let the bridge do it job */
		if ((p = switch_channel_get_variable(member_channel, "cc_agent_found")) && (agent_found = switch_true(p))) {
			break;
		}
		/* If the member thread set a different reason, we monitor it so we can quit the wait */
		if (h->member_cancel_reason != CC_MEMBER_CANCEL_REASON_NONE) {
			break;
		}

		switch_core_session_flush_private_events(member_session);

		if (moh_valid && cur_moh) {
			switch_status_t status = switch_ivr_play_file(member_session, NULL, cur_moh, &args);

			if (status == SWITCH_STATUS_FALSE /* Invalid Recording */ && SWITCH_READ_ACCEPTABLE(status)) { 
				/* Sadly, there doesn't seem to be a return to switch_ivr_play_file that tell you the file wasn't found.  FALSE also mean that the channel got switch to BRAKE state, so we check for read acceptable */
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_WARNING, "Couldn't play file '%s', continuing wait with no audio\n", cur_moh);
				moh_valid = SWITCH_FALSE;
			} else if (status == SWITCH_STATUS_BREAK) {
				char buf[2] = { ht.dtmf, 0 };
				switch_channel_set_variable(member_channel, "cc_exit_key", buf);
				break;
			} else if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}
		} else {
			if ((switch_ivr_collect_digits_callback(member_session, &args, 0, 0)) == SWITCH_STATUS_BREAK) {
				char buf[2] = { ht.dtmf, 0 };
				switch_channel_set_variable(member_channel, "cc_exit_key", buf);
				break;
			}
		}
		switch_yield(1000);
	}

	/* Make sure an agent was found, as we might break above without setting it */
	if (!agent_found && (p = switch_channel_get_variable(member_channel, "cc_agent_found"))) {
		agent_found = switch_true(p);
	}

	/* Stop member thread */
	if (h) {
		h->running = 0;
	}

	/* Check if we were removed because FS Core(BREAK) asked us to */
	if (h->member_cancel_reason == CC_MEMBER_CANCEL_REASON_NONE && !agent_found) {
		h->member_cancel_reason = CC_MEMBER_CANCEL_REASON_BREAK_OUT;
	}

	switch_channel_set_variable(member_channel, "cc_agent_found", NULL);
	/* Canceled for some reason */
	if (!switch_channel_up(member_channel) || h->member_cancel_reason != CC_MEMBER_CANCEL_REASON_NONE) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member %s <%s> abandoned waiting in queue %s\n", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")), switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")), queue_name);

		/* Update member state */
		sql = switch_mprintf("UPDATE members SET state = '%q', session_uuid = '', abandoned_epoch = '%" SWITCH_TIME_T_FMT "' WHERE system = 'single_box' AND uuid = '%q'",
				cc_member_state2str(CC_MEMBER_STATE_ABANDONED), local_epoch_time_now(NULL), member_uuid);
				cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Hangup any callback agents  */
		switch_core_session_hupall_matching_var("cc_member_pre_answer_uuid", member_uuid, SWITCH_CAUSE_ORIGINATOR_CANCEL);

		/* Generate an event */
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CALLCENTER_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(member_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Queue", queue_name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Action", "member-queue-end");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Member-Leaving-Time", "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "CC-Member-Joined-Time", "%" SWITCH_TIME_T_FMT, t_member_called);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Cause", "Cancel");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Cancel-Reason", cc_member_cancel_reason2str(h->member_cancel_reason));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-UUID", member_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-Session-UUID", member_session_uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Name", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CC-Member-CID-Number", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")));
			switch_event_fire(&event);
		}

		/* Update some channel variables for xml_cdr needs */
		switch_channel_set_variable_printf(member_channel, "cc_queue_canceled_epoch", "%" SWITCH_TIME_T_FMT, local_epoch_time_now(NULL));
		switch_channel_set_variable_printf(member_channel, "cc_cause", "%s", "cancel");
		switch_channel_set_variable_printf(member_channel, "cc_cancel_reason", "%s", cc_member_cancel_reason2str(h->member_cancel_reason));

		/* Print some debug log information */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member \"%s\" <%s> exit queue %s due to %s\n",
						  switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")),
						  switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")),
						  queue_name, cc_member_cancel_reason2str(h->member_cancel_reason));

	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(member_session), SWITCH_LOG_DEBUG, "Member %s <%s> is answered by an agent in queue %s\n", switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_name")), switch_str_nil(switch_channel_get_variable(member_channel, "caller_id_number")), queue_name);

		/* Update member state */
		sql = switch_mprintf("UPDATE members SET state = '%q', bridge_epoch = '%" SWITCH_TIME_T_FMT "' WHERE system = 'single_box' AND uuid = '%q'",
				cc_member_state2str(CC_MEMBER_STATE_ANSWERED), local_epoch_time_now(NULL), member_uuid);
		cc_execute_sql(NULL, sql, NULL);
		switch_safe_free(sql);

		/* Update some channel variables for xml_cdr needs */
		switch_channel_set_variable_printf(member_channel, "cc_cause", "%s", "answered");

	}

	/* Send Event with queue count */
	cc_queue_count(queue_name);

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

#define CC_CONFIG_API_SYNTAX "callcenter_config <target> <args>,\n"\
"\tcallcenter_config agent add [name] [type] | \n" \
"\tcallcenter_config agent del [name] | \n" \
"\tcallcenter_config agent set status [agent_name] [status] | \n" \
"\tcallcenter_config agent set state [agent_name] [state] | \n" \
"\tcallcenter_config agent set contact [agent_name] [contact] | \n" \
"\tcallcenter_config agent set ready_time [agent_name] [wait till epoch] | \n"\
"\tcallcenter_config agent set reject_delay_time [agent_name] [wait second] | \n"\
"\tcallcenter_config agent set busy_delay_time [agent_name] [wait second] | \n"\
"\tcallcenter_config agent set no_answer_delay_time [agent_name] [wait second] | \n"\
"\tcallcenter_config agent get status [agent_name] | \n" \
"\tcallcenter_config agent get state [agent_name] | \n" \
"\tcallcenter_config agent get uuid [agent_name] | \n" \
"\tcallcenter_config agent list [[agent_name]] | \n" \
"\tcallcenter_config tier add [queue_name] [agent_name] [level] [position] | \n" \
"\tcallcenter_config tier set state [queue_name] [agent_name] [state] | \n" \
"\tcallcenter_config tier set level [queue_name] [agent_name] [level] | \n" \
"\tcallcenter_config tier set position [queue_name] [agent_name] [position] | \n" \
"\tcallcenter_config tier del [queue_name] [agent_name] | \n" \
"\tcallcenter_config tier list | \n" \
"\tcallcenter_config queue load [queue_name] | \n" \
"\tcallcenter_config queue unload [queue_name] | \n" \
"\tcallcenter_config queue reload [queue_name] | \n" \
"\tcallcenter_config queue list | \n" \
"\tcallcenter_config queue list agents [queue_name] [status] | \n" \
"\tcallcenter_config queue list members [queue_name] | \n" \
"\tcallcenter_config queue list tiers [queue_name] | \n" \
"\tcallcenter_config queue count | \n" \
"\tcallcenter_config queue count agents [queue_name] [status] | \n" \
"\tcallcenter_config queue count members [queue_name] | \n" \
"\tcallcenter_config queue count tiers [queue_name]"

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
		stream->write_function(stream, "-USAGE: \n%s\n", CC_CONFIG_API_SYNTAX);
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
				char ret[64];
				switch (cc_agent_get(key, agent, ret, sizeof(ret))) {
					case CC_STATUS_SUCCESS:
						stream->write_function(stream, "%s", ret);
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
			if ( argc-initial_argc > 1 ) {
				stream->write_function(stream, "%s", "-ERR Invalid!\n");
				goto done;
			} else if ( argc-initial_argc == 1 ) {
				sql = switch_mprintf("SELECT * FROM agents WHERE name='%q'", argv[0 + initial_argc]);
			} else {
				sql = switch_mprintf("SELECT * FROM agents");
			}
			cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, list_result_callback, &cbt /* Call back variables */);
			switch_safe_free(sql);
			stream->write_function(stream, "%s", "+OK\n");
		}

	} else if (section && !strcasecmp(section, "tier")) {
		if (action && !strcasecmp(action, "add")) {
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
			struct list_result cbt;
			cbt.row_process = 0;
			cbt.stream = stream;
			sql = switch_mprintf("SELECT * FROM tiers ORDER BY level, position");
			cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, list_result_callback, &cbt /* Call back variables */);
			switch_safe_free(sql);
			stream->write_function(stream, "%s", "+OK\n");
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
					stream->write_function(stream, "%s", "+OK\n");
				} else {
					stream->write_function(stream, "%s", "-ERR Invalid Queue not found!\n");
				}
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
					stream->write_function(stream, "%s", "+OK\n");
				} else {
					stream->write_function(stream, "%s", "-ERR Invalid Queue not found!\n");
				}
			}

		} else if (action && !strcasecmp(action, "list")) {
			/* queue list */
			if (argc-initial_argc < 1) {
				switch_hash_index_t *hi;
				stream->write_function(stream, "%s", "name|strategy|moh_sound|time_base_score|tier_rules_apply|tier_rule_wait_second|tier_rule_wait_multiply_level|tier_rule_no_agent_no_wait|discard_abandoned_after|abandoned_resume_allowed|max_wait_time|max_wait_time_with_no_agent|max_wait_time_with_no_agent_time_reached|record_template\n");
				switch_mutex_lock(globals.mutex);
				for (hi = switch_hash_first(NULL, globals.queue_hash); hi; hi = switch_hash_next(hi)) {
					void *val = NULL;
					const void *key;
					switch_ssize_t keylen;
					cc_queue_t *queue;
					switch_hash_this(hi, &key, &keylen, &val);
					queue = (cc_queue_t *) val;
					stream->write_function(stream, "%s|%s|%s|%s|%s|%d|%s|%s|%d|%s|%d|%d|%d|%s\n", queue->name, queue->strategy, queue->moh, queue->time_base_score, (queue->tier_rules_apply?"true":"false"), queue->tier_rule_wait_second, (queue->tier_rule_wait_multiply_level?"true":"false"), (queue->tier_rule_no_agent_no_wait?"true":"false"), queue->discard_abandoned_after, (queue->abandoned_resume_allowed?"true":"false"), queue->max_wait_time, queue->max_wait_time_with_no_agent, queue->max_wait_time_with_no_agent_time_reached, queue->record_template);
					queue = NULL;
				}
				switch_mutex_unlock(globals.mutex);
				stream->write_function(stream, "%s", "+OK\n");
				goto done;
			} else {
				const char *sub_action = argv[0 + initial_argc];
				const char *queue_name = argv[1 + initial_argc];
				const char *status = NULL;
				struct list_result cbt;

				/* queue list agents */
				if (sub_action && !strcasecmp(sub_action, "agents")) {
					if (argc-initial_argc > 2) {
						status = argv[2 + initial_argc];
					}
					if (status)	{
						sql = switch_mprintf("SELECT agents.* FROM agents,tiers WHERE tiers.agent = agents.name AND tiers.queue = '%q' AND agents.status = '%q'", queue_name, status);
					} else {
						sql = switch_mprintf("SELECT agents.* FROM agents,tiers WHERE tiers.agent = agents.name AND tiers.queue = '%q'", queue_name);
					}
				/* queue list members */
				} else if (sub_action && !strcasecmp(sub_action, "members")) {
					sql = switch_mprintf("SELECT * FROM members WHERE queue = '%q';", queue_name);
				/* queue list tiers */
				} else if (sub_action && !strcasecmp(sub_action, "tiers")) {
					sql = switch_mprintf("SELECT * FROM tiers WHERE queue = '%q';", queue_name);
				} else {
					stream->write_function(stream, "%s", "-ERR Invalid!\n");
					goto done;
				}

				cbt.row_process = 0;
				cbt.stream = stream;
				cc_execute_sql_callback(NULL /* queue */, NULL /* mutex */, sql, list_result_callback, &cbt /* Call back variables */);
				switch_safe_free(sql);
				stream->write_function(stream, "%s", "+OK\n");
			}

		} else if (action && !strcasecmp(action, "count")) {
			/* queue count */
			if (argc-initial_argc < 1) {
				switch_hash_index_t *hi;
				int queue_count = 0;
				switch_mutex_lock(globals.mutex);
				for (hi = switch_hash_first(NULL, globals.queue_hash); hi; hi = switch_hash_next(hi)) {
					queue_count++;
				}
				switch_mutex_unlock(globals.mutex);
				stream->write_function(stream, "%d\n", queue_count);
				goto done;
			} else {
				const char *sub_action = argv[0 + initial_argc];
				const char *queue_name = argv[1 + initial_argc];
				const char *status = NULL;
				char res[256] = "";

				/* queue count agents */
				if (sub_action && !strcasecmp(sub_action, "agents")) {
					if (argc-initial_argc > 2) {
						status = argv[2 + initial_argc];
					}
					if (status)	{
						sql = switch_mprintf("SELECT count(*) FROM agents,tiers WHERE tiers.agent = agents.name AND tiers.queue = '%q' AND agents.status = '%q'", queue_name, status);
					} else {
						sql = switch_mprintf("SELECT count(*) FROM agents,tiers WHERE tiers.agent = agents.name AND tiers.queue = '%q'", queue_name);
					}
				/* queue count members */
				} else if (sub_action && !strcasecmp(sub_action, "members")) {
					sql = switch_mprintf("SELECT count(*) FROM members WHERE queue = '%q';", queue_name);
				/* queue count tiers */
				} else if (sub_action && !strcasecmp(sub_action, "tiers")) {
					sql = switch_mprintf("SELECT count(*) FROM tiers WHERE queue = '%q';", queue_name);
				} else {
					stream->write_function(stream, "%s", "-ERR Invalid!\n");
					goto done;
				}

				cc_execute_sql2str(NULL, NULL, sql, res, sizeof(res));
				switch_safe_free(sql);
				stream->write_function(stream, "%d\n", atoi(res));
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
	switch_status_t status;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	switch_core_hash_init(&globals.queue_hash, globals.pool);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_mutex_lock(globals.mutex);
	globals.running = 1;
	switch_mutex_unlock(globals.mutex);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (!AGENT_DISPATCH_THREAD_STARTED) {
		cc_agent_dispatch_thread_start();
	}

	SWITCH_ADD_APP(app_interface, "callcenter", "CallCenter", CC_DESC, callcenter_function, CC_USAGE, SAF_NONE);
	SWITCH_ADD_API(api_interface, "callcenter_config", "Config of callcenter", cc_config_api_function, CC_CONFIG_API_SYNTAX);

	switch_console_set_complete("add callcenter_config agent add");
	switch_console_set_complete("add callcenter_config agent del");
	switch_console_set_complete("add callcenter_config agent set status");
	switch_console_set_complete("add callcenter_config agent set state");
	switch_console_set_complete("add callcenter_config agent set uuid");
	switch_console_set_complete("add callcenter_config agent set contact");
	switch_console_set_complete("add callcenter_config agent set ready_time");
	switch_console_set_complete("add callcenter_config agent set reject_delay_time");
	switch_console_set_complete("add callcenter_config agent set busy_delay_time");
	switch_console_set_complete("add callcenter_config agent set no_answer_delay_time");
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
	switch_console_set_complete("add callcenter_config queue list agents");
	switch_console_set_complete("add callcenter_config queue list members");
	switch_console_set_complete("add callcenter_config queue list tiers");
	switch_console_set_complete("add callcenter_config queue count");
	switch_console_set_complete("add callcenter_config queue count agents");
	switch_console_set_complete("add callcenter_config queue count members");
	switch_console_set_complete("add callcenter_config queue count tiers");

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

	switch_safe_free(globals.odbc_dsn);
	switch_safe_free(globals.dbname);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */

/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Travis Cross <tc@traviscross.com>
 *
 * mod_fifo.c -- FIFO
 *
 */
#include <switch.h>
#define FIFO_APP_KEY "mod_fifo"

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fifo_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_fifo_load);
SWITCH_MODULE_DEFINITION(mod_fifo, mod_fifo_load, mod_fifo_shutdown, NULL);

/*!\file
 * # Theory of operation
 *
 * ## Kinds of things
 *
 * The fifo systems deals in various kinds of things: /fifos nodes/,
 * /queues/, /(inbound) callers/, /outbound callers/, /consumers/, and
 * /(outbound) members/.
 *
 * /fifo nodes/ accept callers and work to deliver those callers to
 * consumers and members.  The nodes contain an array of /queues/
 * indexed by a priority value.
 *
 * /queues/ contain an array of callers treated as a list.
 *
 * /callers/ are the channels placed into a fifo node's queue for
 * eventual delivery to consumers and members.
 *
 * /outbound callers/ are persons waiting to be called back via a
 * dial string.
 *
 * /consumers/ are channels for agents who have dialed in to one or
 * more fifos and will have callers connected to them.
 *
 * /members/ are agents who we'll place calls to via a dial string to
 * attempt to deliver callers.
 *
 * An /agent/ may refer to either a /consumer/ or an /member/.
 *
 * ## Outbound Strategies
 *
 * An outbound strategy defines the way in which we attempt to deliver
 * callers to members.
 *
 * The default strategy, /ringall/, preserves the caller ID of the
 * caller being delivered.  Because this means we must choose a caller
 * before we place the call to the member, this impacts the order in
 * which calls are delivered and the rate at which we can deliver
 * those calls.
 *
 * The /enterprise/ outbound strategy does not preserve the caller ID
 * of the caller thereby allowing deliver of callers to agents at the
 * fastest possible rate.
 *
 * ## Manual calls
 *
 * The fifo system provides a way to prevent members on non-fifo calls
 * from receiving a call from the fifo system.  We do this by tracking
 * non-fifo calls in a special fifo named `manual_calls`.  When
 * creating a channel for an agent we set the channel variable
 * `fifo_outbound_uuid` to an arbitrary unique value for that agent,
 * then call `fifo_track_call`.  For the corresponding member we must
 * also set `{fifo_outbound_uuid=}` to the same value.
 *
 * ## Importance
 *
 * Importance is a value 0-9 that can be associated with a fifo.  The
 * default value is 0.  If the fifo is being dynamically created the
 * importance of the fifo can be set when calling the `fifo`
 * application.  If the fifo already exists the importance value
 * passed to the fifo application will be ignored.
 */

#define MANUAL_QUEUE_NAME "manual_calls"
#define FIFO_EVENT "fifo::info"

static switch_status_t load_config(int reload, int del_all);
#define MAX_PRI 10

typedef enum {
	NODE_STRATEGY_INVALID = -1,
	NODE_STRATEGY_RINGALL = 0,
	NODE_STRATEGY_ENTERPRISE
} outbound_strategy_t;

static outbound_strategy_t default_strategy = NODE_STRATEGY_RINGALL;

static int marker = 1;

typedef struct {
	int nelm;
	int idx;
	switch_event_t **data;
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
} fifo_queue_t;

typedef enum {
	FIFO_APP_BRIDGE_TAG = (1 << 0),
	FIFO_APP_TRACKING = (1 << 1),
	FIFO_APP_DID_HOOK = (1 << 2)
} fifo_app_flag_t;



static int check_caller_outbound_call(const char *key);
static void add_caller_outbound_call(const char *key, switch_call_cause_t *cancel_cause);
static void del_caller_outbound_call(const char *key);
static void cancel_caller_outbound_call(const char *key, switch_call_cause_t cause);
static int check_consumer_outbound_call(const char *key);
static void add_consumer_outbound_call(const char *key, switch_call_cause_t *cancel_cause);
static void del_consumer_outbound_call(const char *key);
static void cancel_consumer_outbound_call(const char *key, switch_call_cause_t cause);


static int check_bridge_call(const char *key);
static void add_bridge_call(const char *key);
static void del_bridge_call(const char *key);


switch_status_t fifo_queue_create(fifo_queue_t **queue, int size, switch_memory_pool_t *pool)
{
	fifo_queue_t *q;

	q = switch_core_alloc(pool, sizeof(*q));
	q->pool = pool;
	q->nelm = size - 1;
	q->data = switch_core_alloc(pool, size * sizeof(switch_event_t *));
	switch_mutex_init(&q->mutex, SWITCH_MUTEX_NESTED, pool);

	*queue = q;

	return SWITCH_STATUS_SUCCESS;
}


static void change_pos(switch_event_t *event, int pos)
{
	const char *uuid = switch_event_get_header(event, "unique-id");
	switch_core_session_t *session;
	switch_channel_t *channel;
	char tmp[30] = "";

	if (zstr(uuid)) return;

	if (!(session = switch_core_session_locate(uuid))) {
		return;
	}

	channel = switch_core_session_get_channel(session);

	switch_snprintf(tmp, sizeof(tmp), "%d", pos);
	switch_channel_set_variable(channel, "fifo_position", tmp);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "fifo_position", tmp);

	switch_core_session_rwunlock(session);


}

static switch_status_t fifo_queue_push(fifo_queue_t *queue, switch_event_t *ptr)
{
	switch_mutex_lock(queue->mutex);

	if (queue->idx == queue->nelm) {
		switch_mutex_unlock(queue->mutex);
		return SWITCH_STATUS_FALSE;
	}

	queue->data[queue->idx++] = ptr;

	switch_mutex_unlock(queue->mutex);

	return SWITCH_STATUS_SUCCESS;

}

static int fifo_queue_size(fifo_queue_t *queue)
{
	int s;
	switch_mutex_lock(queue->mutex);
	s = queue->idx;
	switch_mutex_unlock(queue->mutex);

	return s;
}

static switch_status_t fifo_queue_pop(fifo_queue_t *queue, switch_event_t **pop, int remove)
{
	int i, j;

	switch_mutex_lock(queue->mutex);

	if (queue->idx == 0) {
		switch_mutex_unlock(queue->mutex);
		return SWITCH_STATUS_FALSE;
	}

	for (j = 0; j < queue->idx; j++) {
		const char *uuid = switch_event_get_header(queue->data[j], "unique-id");
		if (uuid && (remove == 2 || !check_caller_outbound_call(uuid))) {
			if (remove) {
				*pop = queue->data[j];
			} else {
				switch_event_dup(pop, queue->data[j]);
			}
			break;
		}
	}

	if (j == queue->idx) {
		switch_mutex_unlock(queue->mutex);
		return SWITCH_STATUS_FALSE;
	}

	if (remove) {
		for (i = j+1; i < queue->idx; i++) {
			queue->data[i-1] = queue->data[i];
			queue->data[i] = NULL;
			change_pos(queue->data[i-1], i);
		}

		queue->idx--;
	}

	switch_mutex_unlock(queue->mutex);

	return SWITCH_STATUS_SUCCESS;

}


static switch_status_t fifo_queue_pop_nameval(fifo_queue_t *queue, const char *name, const char *val, switch_event_t **pop, int remove)
{
	int i, j, force = 0;

	switch_mutex_lock(queue->mutex);

	if (name && *name == '+') {
		name++;
		force = 1;
	}

	if (remove == 2) {
		force = 1;
	}

	if (queue->idx == 0 || zstr(name) || zstr(val)) {
		switch_mutex_unlock(queue->mutex);
		return SWITCH_STATUS_FALSE;
	}

	for (j = 0; j < queue->idx; j++) {
		const char *j_val = switch_event_get_header(queue->data[j], name);
		const char *uuid = switch_event_get_header(queue->data[j], "unique-id");
		if (j_val && val && !strcmp(j_val, val) && (force || !check_caller_outbound_call(uuid))) {

			if (remove) {
				*pop = queue->data[j];
			} else {
				switch_event_dup(pop, queue->data[j]);
			}
			break;
		}
	}

	if (j == queue->idx) {
		switch_mutex_unlock(queue->mutex);
		return SWITCH_STATUS_FALSE;
	}

	if (remove) {
		for (i = j+1; i < queue->idx; i++) {
			queue->data[i-1] = queue->data[i];
			queue->data[i] = NULL;
			change_pos(queue->data[i-1], i);
		}

		queue->idx--;
	}

	switch_mutex_unlock(queue->mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t fifo_queue_popfly(fifo_queue_t *queue, const char *uuid)
{
	int i, j;

	switch_mutex_lock(queue->mutex);

	if (queue->idx == 0 || zstr(uuid)) {
		switch_mutex_unlock(queue->mutex);
		return SWITCH_STATUS_FALSE;
	}

	for (j = 0; j < queue->idx; j++) {
		const char *j_uuid = switch_event_get_header(queue->data[j], "unique-id");
		if (j_uuid && !strcmp(j_uuid, uuid)) {
			switch_event_destroy(&queue->data[j]);
			break;
		}
	}

	if (j == queue->idx) {
		switch_mutex_unlock(queue->mutex);
		return SWITCH_STATUS_FALSE;
	}

	for (i = j+1; i < queue->idx; i++) {
		queue->data[i-1] = queue->data[i];
		queue->data[i] = NULL;
		change_pos(queue->data[i-1], i);
	}

	queue->idx--;

	switch_mutex_unlock(queue->mutex);

	return SWITCH_STATUS_SUCCESS;

}



struct fifo_node {
	char *name;
	switch_mutex_t *mutex;
	switch_mutex_t *update_mutex;
	fifo_queue_t *fifo_list[MAX_PRI];
	switch_hash_t *consumer_hash;
	int outbound_priority;
	int caller_count;
	int consumer_count;
	int ring_consumer_count;
	int member_count;
	switch_time_t start_waiting;
	uint32_t importance;
	switch_thread_rwlock_t *rwlock;
	switch_memory_pool_t *pool;
	int has_outbound;
	int ready;
	long busy;
	int is_static;
	int outbound_per_cycle;
	char *outbound_name;
	outbound_strategy_t outbound_strategy;
	int ring_timeout;
	int default_lag;
	char *domain_name;
	int retry_delay;
	struct fifo_node *next;
};

typedef struct fifo_node fifo_node_t;

static void fifo_caller_add(fifo_node_t *node, switch_core_session_t *session);
static void fifo_caller_del(const char *uuid);



struct callback {
	char *buf;
	size_t len;
	int matches;
};
typedef struct callback callback_t;

static const char *strat_parse(outbound_strategy_t s)
{
	switch (s) {
	case NODE_STRATEGY_RINGALL:
		return "ringall";
	case NODE_STRATEGY_ENTERPRISE:
		return "enterprise";
	default:
		break;
	}

	return "invalid";
}

static outbound_strategy_t parse_strat(const char *name)
{
	if (!strcasecmp(name, "ringall")) {
		return NODE_STRATEGY_RINGALL;
	}

	if (!strcasecmp(name, "enterprise")) {
		return NODE_STRATEGY_ENTERPRISE;
	}

	return NODE_STRATEGY_INVALID;
}

static int sql2str_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	callback_t *cbt = (callback_t *) pArg;

	switch_copy_string(cbt->buf, argv[0], cbt->len);
	cbt->matches++;
	return 0;
}

static switch_bool_t match_key(const char *caller_exit_key, char key)
{
	while (caller_exit_key && *caller_exit_key) {
		if (*caller_exit_key++ == key) {
			return SWITCH_TRUE;
		}
	}
	return SWITCH_FALSE;
}

static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch_core_session_t *bleg = (switch_core_session_t *) buf;

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			switch_channel_t *bchan = switch_core_session_get_channel(bleg);
			switch_channel_t *channel = switch_core_session_get_channel(session);

			const char *consumer_exit_key = switch_channel_get_variable(channel, "fifo_consumer_exit_key");

			if (switch_channel_test_flag(switch_core_session_get_channel(session), CF_BRIDGE_ORIGINATOR)) {
				if (consumer_exit_key && dtmf->digit == *consumer_exit_key) {
					switch_channel_hangup(bchan, SWITCH_CAUSE_NORMAL_CLEARING);
					return SWITCH_STATUS_BREAK;
				} else if (!consumer_exit_key && dtmf->digit == '*') {
					switch_channel_hangup(bchan, SWITCH_CAUSE_NORMAL_CLEARING);
					return SWITCH_STATUS_BREAK;
				} else if (dtmf->digit == '0') {
					const char *moh_a = NULL, *moh_b = NULL;

					if (!(moh_b = switch_channel_get_variable(bchan, "fifo_music"))) {
						moh_b = switch_channel_get_hold_music(bchan);
					}

					if (!(moh_a = switch_channel_get_variable(channel, "fifo_hold_music"))) {
						if (!(moh_a = switch_channel_get_variable(channel, "fifo_music"))) {
							moh_a = switch_channel_get_hold_music(channel);
						}
					}

					switch_ivr_soft_hold(session, "0", moh_a, moh_b);
					return SWITCH_STATUS_IGNORE;
				}
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t moh_on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			switch_channel_t *channel = switch_core_session_get_channel(session);
			const char *caller_exit_key = switch_channel_get_variable(channel, "fifo_caller_exit_key");

			if (match_key(caller_exit_key, dtmf->digit)) {
				char *bp = buf;
				*bp = dtmf->digit;
				return SWITCH_STATUS_BREAK;
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

#define check_string(s) if (!zstr(s) && !strcasecmp(s, "undef")) { s = NULL; }

static int node_caller_count(fifo_node_t *node)
{
	int i, len = 0;

	for (i = 0; i < MAX_PRI; i++) {
		len += fifo_queue_size(node->fifo_list[i]);
	}

	return len;
}

static void node_remove_uuid(fifo_node_t *node, const char *uuid)
{
	int i = 0;

	for (i = 0; i < MAX_PRI; i++) {
		fifo_queue_popfly(node->fifo_list[i], uuid);
	}

	if (!node_caller_count(node)) {
		node->start_waiting = 0;
	}

	fifo_caller_del(uuid);

	return;
}

#define MAX_CHIME 25
struct fifo_chime_data {
	char *list[MAX_CHIME];
	int total;
	int index;
	time_t next;
	int freq;
	int abort;
	time_t orbit_timeout;
	int do_orbit;
	char *orbit_exten;
	char *orbit_dialplan;
	char *orbit_context;
	char *exit_key;
};

typedef struct fifo_chime_data fifo_chime_data_t;

static switch_status_t chime_read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	fifo_chime_data_t *cd = (fifo_chime_data_t *) user_data;

	if (cd && cd->orbit_timeout && switch_epoch_time_now(NULL) >= cd->orbit_timeout) {
		cd->do_orbit = 1;
		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t caller_read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	fifo_chime_data_t *cd = (fifo_chime_data_t *) user_data;

	if (!cd) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (cd->total && switch_epoch_time_now(NULL) >= cd->next) {
		if (cd->index == MAX_CHIME || cd->index == cd->total || !cd->list[cd->index]) {
			cd->index = 0;
		}

		if (cd->list[cd->index]) {
			switch_input_args_t args = { 0 };
			char buf[25] = "";
			switch_status_t status;

			args.input_callback = moh_on_dtmf;
			args.buf = buf;
			args.buflen = sizeof(buf);
			args.read_frame_callback = chime_read_frame_callback;
			args.user_data = user_data;
			
			status = switch_ivr_play_file(session, NULL, cd->list[cd->index], &args);
			
			if (match_key(cd->exit_key, *buf)) {
				cd->abort = 1;
				return SWITCH_STATUS_BREAK;
			}
			
			if (status != SWITCH_STATUS_SUCCESS) {
				return SWITCH_STATUS_BREAK;
			}

			cd->next = switch_epoch_time_now(NULL) + cd->freq;
			cd->index++;
		}
	}

	return chime_read_frame_callback(session, frame, user_data);
}

static switch_status_t consumer_read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	fifo_node_t *node, **node_list = (fifo_node_t **) user_data;
	int x = 0, total = 0, i = 0;

	for (i = 0;; i++) {
		if (!(node = node_list[i])) {
			break;
		}
		for (x = 0; x < MAX_PRI; x++) {
			total += fifo_queue_size(node->fifo_list[x]);
		}
	}

	if (total) {
		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}

struct fifo_node;

static struct {
	switch_hash_t *caller_orig_hash;
	switch_hash_t *consumer_orig_hash;
	switch_hash_t *bridge_hash;
	switch_hash_t *use_hash;
	switch_mutex_t *use_mutex;
	switch_mutex_t *caller_orig_mutex;
	switch_mutex_t *consumer_orig_mutex;
	switch_mutex_t *bridge_mutex;
	switch_hash_t *fifo_hash;
	switch_mutex_t *mutex;
	switch_mutex_t *sql_mutex;
	switch_memory_pool_t *pool;
	int running;
	switch_event_node_t *node;
	char hostname[256];
	char *dbname;
	char odbc_dsn[1024];
	int node_thread_running;
	switch_odbc_handle_t *master_odbc;
	int threads;
	switch_thread_t *node_thread;
	int debug;
	struct fifo_node *nodes;
	char *pre_trans_execute;
	char *post_trans_execute;
	char *inner_pre_trans_execute;
	char *inner_post_trans_execute;	
	switch_sql_queue_manager_t *qm;
	int allow_transcoding;
} globals;



static int fifo_dec_use_count(const char *outbound_id)
{
	int r = 0, *count;


	switch_mutex_lock(globals.use_mutex);
	if ((count = (int *) switch_core_hash_find(globals.use_hash, outbound_id))) {
		if (*count > 0) {
			r = --(*count);
		}
	}
	switch_mutex_unlock(globals.use_mutex);
	
	return r;
}

static int fifo_get_use_count(const char *outbound_id) 
{
	int r = 0, *count;

	switch_mutex_lock(globals.use_mutex);
	if ((count = (int *) switch_core_hash_find(globals.use_hash, outbound_id))) {
		r = *count;
	}
	switch_mutex_unlock(globals.use_mutex);
	
	return r;
}


static int fifo_inc_use_count(const char *outbound_id) 
{
	int r = 0, *count;

	switch_mutex_lock(globals.use_mutex);
	if (!(count = (int *) switch_core_hash_find(globals.use_hash, outbound_id))) {
		count = switch_core_alloc(globals.pool, sizeof(int));
		switch_core_hash_insert(globals.use_hash, outbound_id, count);
	}

	r = ++(*count);

	switch_mutex_unlock(globals.use_mutex);
	
	return r;
}

static void fifo_init_use_count(void) 
{
	switch_mutex_lock(globals.use_mutex);
	if (globals.use_hash) {
		switch_core_hash_destroy(&globals.use_hash);
	}
	switch_core_hash_init(&globals.use_hash);
	switch_mutex_unlock(globals.use_mutex);
}




static int check_caller_outbound_call(const char *key)
{
	int x = 0;

	if (!key) return x;

	switch_mutex_lock(globals.caller_orig_mutex);
	x = !!switch_core_hash_find(globals.caller_orig_hash, key);
	switch_mutex_unlock(globals.caller_orig_mutex);
	return x;

}


static void add_caller_outbound_call(const char *key, switch_call_cause_t *cancel_cause)
{
	if (!key) return;

	switch_mutex_lock(globals.caller_orig_mutex);
	switch_core_hash_insert(globals.caller_orig_hash, key, cancel_cause);
	switch_mutex_unlock(globals.caller_orig_mutex);
}

static void del_caller_outbound_call(const char *key)
{
	if (!key) return;

	switch_mutex_lock(globals.caller_orig_mutex);
	switch_core_hash_delete(globals.caller_orig_hash, key);
	switch_mutex_unlock(globals.caller_orig_mutex);
}

static void cancel_caller_outbound_call(const char *key, switch_call_cause_t cause)
{
	switch_call_cause_t *cancel_cause = NULL;

	if (!key) return;

	switch_mutex_lock(globals.caller_orig_mutex);
	if ((cancel_cause = (switch_call_cause_t *) switch_core_hash_find(globals.caller_orig_hash, key))) {
		*cancel_cause = cause;
	}
	switch_mutex_unlock(globals.caller_orig_mutex);
}



static int check_bridge_call(const char *key)
{
	int x = 0;

	if (!key) return x;

	switch_mutex_lock(globals.bridge_mutex);
	x = !!switch_core_hash_find(globals.bridge_hash, key);
	switch_mutex_unlock(globals.bridge_mutex);
	return x;

}


static void add_bridge_call(const char *key)
{
	if (!key) return;

	switch_mutex_lock(globals.bridge_mutex);
	switch_core_hash_insert(globals.bridge_hash, key, (void *)&marker);
	switch_mutex_unlock(globals.bridge_mutex);
}

static void del_bridge_call(const char *key)
{
	switch_mutex_lock(globals.bridge_mutex);
	switch_core_hash_delete(globals.bridge_hash, key);
	switch_mutex_unlock(globals.bridge_mutex);
}


static int check_consumer_outbound_call(const char *key)
{
	int x = 0;

	if (!key) return x;

	switch_mutex_lock(globals.consumer_orig_mutex);
	x = !!switch_core_hash_find(globals.consumer_orig_hash, key);
	switch_mutex_unlock(globals.consumer_orig_mutex);
	return x;

}

static void add_consumer_outbound_call(const char *key, switch_call_cause_t *cancel_cause)
{
	if (!key) return;

	switch_mutex_lock(globals.consumer_orig_mutex);
	switch_core_hash_insert(globals.consumer_orig_hash, key, cancel_cause);
	switch_mutex_unlock(globals.consumer_orig_mutex);
}

static void del_consumer_outbound_call(const char *key)
{
	if (!key) return;

	switch_mutex_lock(globals.consumer_orig_mutex);
	switch_core_hash_delete(globals.consumer_orig_hash, key);
	switch_mutex_unlock(globals.consumer_orig_mutex);
}

static void cancel_consumer_outbound_call(const char *key, switch_call_cause_t cause)
{
	switch_call_cause_t *cancel_cause = NULL;

	if (!key) return;

	switch_mutex_lock(globals.consumer_orig_mutex);
	if ((cancel_cause = (switch_call_cause_t *) switch_core_hash_find(globals.consumer_orig_hash, key))) {
		*cancel_cause = cause;
	}
	switch_mutex_unlock(globals.consumer_orig_mutex);

}



switch_cache_db_handle_t *fifo_get_db_handle(void)
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

static switch_status_t fifo_execute_sql_queued(char **sqlp, switch_bool_t sql_already_dynamic, switch_bool_t block)
{
	int index = 1;
	char *sql;

	switch_assert(sqlp && *sqlp);
	sql = *sqlp;	


	if (switch_stristr("insert", sql)) {
		index = 0;
	}

	if (block) {
		switch_sql_queue_manager_push_confirm(globals.qm, sql, index, !sql_already_dynamic);
	} else {
		switch_sql_queue_manager_push(globals.qm, sql, index, !sql_already_dynamic);
	}

	if (sql_already_dynamic) {
		*sqlp = NULL;
	}

	return SWITCH_STATUS_SUCCESS;

}
#if 0
static switch_status_t fifo_execute_sql(char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = fifo_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	if (globals.debug > 1) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "sql: %s\n", sql);

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

  end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}
#endif

static switch_bool_t fifo_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = fifo_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	if (globals.debug > 1) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "sql: %s\n", sql);

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

  end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}

static fifo_node_t *create_node(const char *name, uint32_t importance, switch_mutex_t *mutex)
{
	fifo_node_t *node;
	int x = 0;
	switch_memory_pool_t *pool;
	char outbound_count[80] = "";
	callback_t cbt = { 0 };
	char *sql = NULL;
	char *domain_name = NULL;
	
	if (!globals.running) {
		return NULL;
	}


	switch_core_new_memory_pool(&pool);

	node = switch_core_alloc(pool, sizeof(*node));
	node->pool = pool;
	node->outbound_strategy = default_strategy;
	node->name = switch_core_strdup(node->pool, name);

	if (!strchr(name, '@')) {
		domain_name = switch_core_get_domain(SWITCH_TRUE);
		node->domain_name = switch_core_strdup(node->pool, domain_name);
	}

	for (x = 0; x < MAX_PRI; x++) {
		fifo_queue_create(&node->fifo_list[x], 1000, node->pool);
		switch_assert(node->fifo_list[x]);
	}

	switch_core_hash_init(&node->consumer_hash);
	switch_thread_rwlock_create(&node->rwlock, node->pool);
	switch_mutex_init(&node->mutex, SWITCH_MUTEX_NESTED, node->pool);
	switch_mutex_init(&node->update_mutex, SWITCH_MUTEX_NESTED, node->pool);
	cbt.buf = outbound_count;
	cbt.len = sizeof(outbound_count);
	sql = switch_mprintf("select count(*) from fifo_outbound where fifo_name = '%q'", name);
	fifo_execute_sql_callback(mutex, sql, sql2str_callback, &cbt);
    node->member_count = atoi(outbound_count);
	if (node->member_count > 0) {
		node->has_outbound = 1;
	} else {
        node->has_outbound = 0;
    }
	switch_safe_free(sql);

	node->importance = importance;

	switch_mutex_lock(globals.mutex);

	switch_core_hash_insert(globals.fifo_hash, name, node);
	node->next = globals.nodes;
	globals.nodes = node;
	switch_mutex_unlock(globals.mutex);

	switch_safe_free(domain_name);

	return node;
}

static int node_idle_consumers(fifo_node_t *node)
{
	switch_hash_index_t *hi;
	void *val;
	const void *var;
	switch_core_session_t *session;
	switch_channel_t *channel;
	int total = 0;

	switch_mutex_lock(node->mutex);
	for (hi = switch_core_hash_first(node->consumer_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &var, NULL, &val);
		session = (switch_core_session_t *) val;
		channel = switch_core_session_get_channel(session);
		if (!switch_channel_test_flag(channel, CF_BRIDGED)) {
			total++;
		}
	}
	switch_mutex_unlock(node->mutex);

	return total;

}

struct call_helper {
	char *uuid;
	char *node_name;
	char *originate_string;
	int timeout;
	switch_memory_pool_t *pool;
};

#define MAX_ROWS 250
struct callback_helper {
	int need;
	switch_memory_pool_t *pool;
	struct call_helper *rows[MAX_ROWS];
	int rowcount;
	int ready;
};

static void do_unbridge(switch_core_session_t *consumer_session, switch_core_session_t *caller_session)
{
	switch_channel_t *consumer_channel = switch_core_session_get_channel(consumer_session);
	switch_channel_t *caller_channel = NULL;

	if (caller_session) {
		caller_channel = switch_core_session_get_channel(caller_session);
	}

	if (switch_channel_test_app_flag_key(FIFO_APP_KEY, consumer_channel, FIFO_APP_BRIDGE_TAG)) {
		char date[80] = "";
		switch_time_exp_t tm;
		switch_time_t ts = switch_micro_time_now();
		switch_size_t retsize;
		long epoch_start = 0, epoch_end = 0;
		const char *epoch_start_a = NULL;
		char *sql;
		switch_event_t *event;
		const char *outbound_id = NULL;
		int use_count = 0;

		switch_channel_clear_app_flag_key(FIFO_APP_KEY, consumer_channel, FIFO_APP_BRIDGE_TAG);
		switch_channel_set_variable(consumer_channel, "fifo_bridged", NULL);

		if ((outbound_id = switch_channel_get_variable(consumer_channel, "fifo_outbound_uuid"))) {
			use_count = fifo_get_use_count(outbound_id);
		}
		
		ts = switch_micro_time_now();
		switch_time_exp_lt(&tm, ts);
		switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);

		sql = switch_mprintf("delete from fifo_bridge where consumer_uuid='%q'", switch_core_session_get_uuid(consumer_session));
		fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_FALSE);


		switch_channel_set_variable(consumer_channel, "fifo_status", "WAITING");
		switch_channel_set_variable(consumer_channel, "fifo_timestamp", date);

		if (caller_channel) {
			switch_channel_set_variable(caller_channel, "fifo_status", "DONE");
			switch_channel_set_variable(caller_channel, "fifo_timestamp", date);
		}

		if ((epoch_start_a = switch_channel_get_variable(consumer_channel, "fifo_epoch_start_bridge"))) {
			epoch_start = atol(epoch_start_a);
		}

		epoch_end = (long)switch_epoch_time_now(NULL);

		switch_channel_set_variable_printf(consumer_channel, "fifo_epoch_stop_bridge", "%ld", epoch_end);
		switch_channel_set_variable_printf(consumer_channel, "fifo_bridge_seconds", "%d", epoch_end - epoch_start);

		if (caller_channel) {
			switch_channel_set_variable_printf(caller_channel, "fifo_epoch_stop_bridge", "%ld", epoch_end);
			switch_channel_set_variable_printf(caller_channel, "fifo_bridge_seconds", "%d", epoch_end - epoch_start);
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(consumer_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", MANUAL_QUEUE_NAME);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-consumer-stop");
			if (outbound_id) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", outbound_id);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", use_count);
			}
			switch_event_fire(&event);
		}

		if (caller_channel) {
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(caller_channel, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", MANUAL_QUEUE_NAME);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-caller-stop");
				switch_event_fire(&event);
			}
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(consumer_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", MANUAL_QUEUE_NAME);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_stop");
			switch_event_fire(&event);
		}
	}
}


static switch_status_t messagehook (switch_core_session_t *session, switch_core_session_message_t *msg)
{
	switch_event_t *event;
	switch_core_session_t *caller_session = NULL, *consumer_session = NULL;
	switch_channel_t *caller_channel = NULL, *consumer_channel = NULL;
	const char *outbound_id;
	char *sql;

	consumer_session = session;
	consumer_channel = switch_core_session_get_channel(consumer_session);
	outbound_id = switch_channel_get_variable(consumer_channel, "fifo_outbound_uuid");

	if (!outbound_id) return SWITCH_STATUS_SUCCESS;

	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		if (msg->numeric_arg == 42) {
			goto end;
		}
		if ((caller_session = switch_core_session_locate(msg->string_arg))) {
			caller_channel = switch_core_session_get_channel(caller_session);
			if (msg->message_id == SWITCH_MESSAGE_INDICATE_BRIDGE) {
				cancel_consumer_outbound_call(outbound_id, SWITCH_CAUSE_ORIGINATOR_CANCEL);
				switch_core_session_soft_lock(caller_session, 5);
			} else {
				switch_core_session_soft_unlock(caller_session);
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_DISPLAY:
		sql = switch_mprintf("update fifo_bridge set caller_caller_id_name='%q', caller_caller_id_number='%q' where consumer_uuid='%q'",
							 switch_str_nil(msg->string_array_arg[0]),
							 switch_str_nil(msg->string_array_arg[1]),
							 switch_core_session_get_uuid(session));
		fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_FALSE);
		goto end;
	default:
		goto end;
	}


	switch (msg->message_id) {
	case SWITCH_MESSAGE_INDICATE_BRIDGE:
		{

			long epoch_start = 0;
			char date[80] = "";
			switch_time_t ts;
			switch_time_exp_t tm;
			switch_size_t retsize;
			const char *ced_name, *ced_number, *cid_name, *cid_number, *outbound_id;

			if (switch_channel_test_app_flag_key(FIFO_APP_KEY, consumer_channel, FIFO_APP_BRIDGE_TAG)) {
				goto end;
			}

			switch_channel_set_app_flag_key(FIFO_APP_KEY, consumer_channel, FIFO_APP_BRIDGE_TAG);

			switch_channel_set_variable(consumer_channel, "fifo_bridged", "true");
			switch_channel_set_variable(consumer_channel, "fifo_manual_bridge", "true");
			switch_channel_set_variable(consumer_channel, "fifo_role", "consumer");
			outbound_id = switch_channel_get_variable(consumer_channel, "fifo_outbound_uuid");

			if (caller_channel) {
				switch_channel_set_variable(caller_channel, "fifo_role", "caller");
				switch_process_import(consumer_session, caller_channel, "fifo_caller_consumer_import",
									  switch_channel_get_variable(consumer_channel, "fifo_import_prefix"));
				switch_process_import(caller_session, consumer_channel, "fifo_consumer_caller_import",
									  switch_channel_get_variable(caller_channel, "fifo_import_prefix"));
			}

			ced_name = switch_channel_get_variable(consumer_channel, "callee_id_name");
			ced_number = switch_channel_get_variable(consumer_channel, "callee_id_number");

			cid_name = switch_channel_get_variable(consumer_channel, "caller_id_name");
			cid_number = switch_channel_get_variable(consumer_channel, "caller_id_number");

			if (switch_channel_direction(consumer_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
				if (zstr(ced_name) || !strcmp(ced_name, cid_name)) {
					ced_name = ced_number;
				}

				if (zstr(ced_number) || !strcmp(ced_number, cid_number)) {
					ced_name = switch_channel_get_variable(consumer_channel, "destination_number");
					ced_number = ced_name;
				}
			} else {
				ced_name = cid_name;
				ced_number = cid_number;
			}

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(consumer_channel, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", MANUAL_QUEUE_NAME);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-consumer-start");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-CID-Name", ced_name);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-CID-Number", ced_number);
				if (outbound_id) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", outbound_id);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", fifo_get_use_count(outbound_id));
				}
				switch_event_fire(&event);
			}

			if (caller_channel) {
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(caller_channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", MANUAL_QUEUE_NAME);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-caller-start");
					switch_event_fire(&event);
				}

				sql = switch_mprintf("insert into fifo_bridge "
									 "(fifo_name,caller_uuid,caller_caller_id_name,caller_caller_id_number,consumer_uuid,consumer_outgoing_uuid,bridge_start) "
									 "values ('%q','%q','%q','%q','%q','%q',%ld)",
									 MANUAL_QUEUE_NAME,
									 switch_core_session_get_uuid(caller_session),
									 ced_name,
									 ced_number,
									 switch_core_session_get_uuid(session),
									 switch_str_nil(outbound_id),
									 (long) switch_epoch_time_now(NULL)
									 );
			} else {
				sql = switch_mprintf("insert into fifo_bridge "
									 "(fifo_name,caller_uuid,caller_caller_id_name,caller_caller_id_number,consumer_uuid,consumer_outgoing_uuid,bridge_start) "
									 "values ('%q','%q','%q','%q','%q','%q',%ld)",
									 MANUAL_QUEUE_NAME,
									 (msg->string_arg && strchr(msg->string_arg, '-')) ? msg->string_arg : "00000000-0000-0000-0000-000000000000",
									 ced_name,
									 ced_number,
									 switch_core_session_get_uuid(session),
									 switch_str_nil(outbound_id),
									 (long) switch_epoch_time_now(NULL)
									 );
			}

			fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_FALSE);


			epoch_start = (long)switch_epoch_time_now(NULL);

			ts = switch_micro_time_now();
			switch_time_exp_lt(&tm, ts);
			epoch_start = (long)switch_epoch_time_now(NULL);
			switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
			switch_channel_set_variable(consumer_channel, "fifo_status", "TALKING");
			if (caller_session) {
				switch_channel_set_variable(consumer_channel, "fifo_target", switch_core_session_get_uuid(caller_session));
			}
			switch_channel_set_variable(consumer_channel, "fifo_timestamp", date);
			switch_channel_set_variable_printf(consumer_channel, "fifo_epoch_start_bridge", "%ld", epoch_start);
			switch_channel_set_variable(consumer_channel, "fifo_role", "consumer");

			if (caller_channel) {
				switch_channel_set_variable(caller_channel, "fifo_status", "TALKING");
				switch_channel_set_variable(caller_channel, "fifo_timestamp", date);
				switch_channel_set_variable_printf(caller_channel, "fifo_epoch_start_bridge", "%ld", epoch_start);
				switch_channel_set_variable(caller_channel, "fifo_target", switch_core_session_get_uuid(session));
				switch_channel_set_variable(caller_channel, "fifo_role", "caller");
			}
		}
		break;
	case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		{
			do_unbridge(consumer_session, caller_session);
		}
		break;
	default:
		break;
	}

 end:

	if (caller_session) {
		switch_core_session_rwunlock(caller_session);
	}

	return SWITCH_STATUS_SUCCESS;
}


static void *SWITCH_THREAD_FUNC ringall_thread_run(switch_thread_t *thread, void *obj)
{
	struct callback_helper *cbh = (struct callback_helper *) obj;
	char *node_name;
	int i = 0;
	int timeout = 0;
	switch_stream_handle_t stream = { 0 };
	switch_stream_handle_t stream2 = { 0 };
	fifo_node_t *node = NULL;
	char *originate_string = NULL;
	switch_event_t *ovars = NULL;
	switch_status_t status;
	switch_core_session_t *session = NULL;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	char *app_name = NULL, *arg = NULL;
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel;
	char *caller_id_name = NULL, *cid_num = NULL, *id = NULL;
	switch_event_t *pop = NULL, *pop_dup = NULL;
	fifo_queue_t *q = NULL;
	int x = 0;
	switch_event_t *event;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	switch_call_cause_t cancel_cause = 0;
	char *uuid_list = NULL;
	int total = 0;
	const char *codec;
	struct call_helper *rows[MAX_ROWS] = { 0 };
	int rowcount = 0;
	switch_memory_pool_t *pool;
	char *export = NULL;

	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);

	if (!globals.running) goto dpool;

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	if (!cbh->rowcount) {
		goto end;
	}

	node_name = cbh->rows[0]->node_name;

	switch_mutex_lock(globals.mutex);
	if ((node = switch_core_hash_find(globals.fifo_hash, node_name))) {
		switch_thread_rwlock_rdlock(node->rwlock);
	}
	switch_mutex_unlock(globals.mutex);

	if (!node) {
		goto end;
	}

	for (i = 0; i < cbh->rowcount; i++) {
		struct call_helper *h = cbh->rows[i];

		if (check_consumer_outbound_call(h->uuid) || check_bridge_call(h->uuid)) {
			continue;
		}

		rows[rowcount++] = h;
		add_consumer_outbound_call(h->uuid, &cancel_cause);
		total++;
	}

	for (i = 0; i < rowcount; i++) {
		struct call_helper *h = rows[i];
		cbh->rows[i] = h;
	}

	cbh->rowcount = rowcount;

	cbh->ready = 1;

	if (!total) {
		goto end;
	}


	if (node) {
		switch_mutex_lock(node->update_mutex);
		node->busy = 0;
		node->ring_consumer_count = 1;
		switch_mutex_unlock(node->update_mutex);
	} else {
		goto end;
	}

	SWITCH_STANDARD_STREAM(stream);
	SWITCH_STANDARD_STREAM(stream2);

	switch_event_create(&ovars, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(ovars);


	for (i = 0; i < cbh->rowcount; i++) {
		struct call_helper *h = cbh->rows[i];
		char *parsed = NULL;
		int use_ent = 0;
		char *expanded_originate_string = switch_event_expand_headers(ovars, h->originate_string);


		if (strstr(expanded_originate_string, "user/")) {
			switch_event_create_brackets(expanded_originate_string, '<', '>', ',', &ovars, &parsed, SWITCH_TRUE);
			use_ent = 1;
		} else {
			switch_event_create_brackets(expanded_originate_string, '{', '}', ',', &ovars, &parsed, SWITCH_TRUE);
		}

		switch_event_del_header(ovars, "fifo_outbound_uuid");

		if (!h->timeout) h->timeout = node->ring_timeout;
		if (timeout < h->timeout) timeout = h->timeout;

		if (use_ent) {
			stream.write_function(&stream, "{ignore_early_media=true,outbound_redirect_fatal=true,leg_timeout=%d,fifo_outbound_uuid=%s,fifo_name=%s}%s%s",
								  h->timeout, h->uuid, node->name, 
								  parsed ? parsed : expanded_originate_string, (i == cbh->rowcount - 1) ? "" : SWITCH_ENT_ORIGINATE_DELIM);
		} else {
			stream.write_function(&stream, "[leg_timeout=%d,fifo_outbound_uuid=%s,fifo_name=%s]%s,",
								  h->timeout, h->uuid, node->name, parsed ? parsed : expanded_originate_string);
		}

		stream2.write_function(&stream2, "%s,", h->uuid);
		switch_safe_free(parsed);

		if (expanded_originate_string && expanded_originate_string != h->originate_string) {
			switch_safe_free(expanded_originate_string);
		}

	}

	originate_string = (char *) stream.data;

	uuid_list = (char *) stream2.data;

	if (uuid_list) {
		end_of(uuid_list) = '\0';
	}

	if (!timeout) timeout = 60;

	pop = pop_dup = NULL;

	for (x = 0; x < MAX_PRI; x++) {
		q = node->fifo_list[x];
		if (fifo_queue_pop_nameval(q, "variable_fifo_vip", "true", &pop_dup, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS && pop_dup) {
			pop = pop_dup;
			break;
		}
	}

	if (!pop) {
		for (x = 0; x < MAX_PRI; x++) {
			q = node->fifo_list[x];
			if (fifo_queue_pop(node->fifo_list[x], &pop_dup, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS && pop_dup) {
				pop = pop_dup;
				break;
			}
		}
	}

	if (!pop) {
		goto end;
	}

	if (!switch_event_get_header(ovars, "origination_caller_id_name")) {
		if ((caller_id_name = switch_event_get_header(pop, "caller-caller-id-name"))) {
			if (!zstr(node->outbound_name)) {
				if ( node->outbound_name[0] == '=' ) {
					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "origination_caller_id_name", "%s", node->outbound_name + 1);
				} else {
					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "origination_caller_id_name", "(%s) %s", node->outbound_name, caller_id_name);
				}
			} else {
				switch_event_add_header_string(ovars, SWITCH_STACK_BOTTOM, "origination_caller_id_name", caller_id_name);
			}
		}
	}

	if (!switch_event_get_header(ovars, "origination_caller_id_number")) {
		if ((cid_num = switch_event_get_header(pop, "caller-caller-id-number"))) {
			switch_event_add_header_string(ovars, SWITCH_STACK_BOTTOM, "origination_caller_id_number", cid_num);
		}
	}

	if ((id = switch_event_get_header(pop, "unique-id"))) {
		switch_event_add_header_string(ovars, SWITCH_STACK_BOTTOM, "fifo_bridge_uuid", id);
	}

	switch_event_add_header_string(ovars, SWITCH_STACK_BOTTOM, "fifo_originate_uuid", uuid_str);


	if ((export = switch_event_get_header(pop, "variable_fifo_export"))) {
		int argc;
		char *argv[100] = { 0 };
		char *mydata = strdup(export);
		char *tmp;

		argc = switch_split(mydata, ',', argv);

		for (x = 0; x < argc; x++) {
			char *name = switch_mprintf("variable_%s", argv[x]);

			if ((tmp = switch_event_get_header(pop, name))) {
				switch_event_add_header_string(ovars, SWITCH_STACK_BOTTOM, argv[x], tmp);
			}

			free(name);
		}

		switch_safe_free(mydata);
	}


	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_core_session_t *session;
		if (id && (session = switch_core_session_locate(id))) {
			switch_channel_t *channel = switch_core_session_get_channel(session);

			switch_channel_set_variable(channel, "fifo_originate_uuid", uuid_str);
			switch_channel_event_set_data(channel, event);
			switch_core_session_rwunlock(session);
		}

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", node->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "pre-dial");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "outbound-strategy", "ringall");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "caller-uuid", id);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "originate_string", originate_string);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Outbound-UUID-List", uuid_list);

		switch_event_fire(&event);
	}

	for (i = 0; i < cbh->rowcount; i++) {
		struct call_helper *h = cbh->rows[i];
		char *sql = switch_mprintf("update fifo_outbound set ring_count=ring_count+1 where uuid='%s'", h->uuid);

		fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

	}

	if (!total) goto end;

	if (!globals.allow_transcoding && !switch_true(switch_event_get_header(pop, "variable_fifo_allow_transcoding")) && 
		(codec = switch_event_get_header(pop, "variable_rtp_use_codec_name"))) {
		const char *rate = switch_event_get_header(pop, "variable_rtp_use_codec_rate");
		const char *ptime = switch_event_get_header(pop, "variable_rtp_use_codec_ptime");
		char nstr[256] = "";

		if (strcasecmp(codec, "PCMU") && strcasecmp(codec, "PCMA")) {
			switch_snprintf(nstr, sizeof(nstr), "%s@%si@%sh,PCMU@%si,PCMA@%si", codec, ptime, rate, ptime, ptime);
		} else {
			switch_snprintf(nstr, sizeof(nstr), "%s@%si@%sh", codec, ptime, rate);
		}

		switch_event_add_header_string(ovars, SWITCH_STACK_BOTTOM, "absolute_codec_string", nstr);
	}

	add_caller_outbound_call(id, &cancel_cause);

	if (globals.debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s dialing: %s\n", node->name, originate_string);

	status = switch_ivr_originate(NULL, &session, &cause, originate_string, timeout, NULL, NULL, NULL, NULL, ovars, SOF_NONE, &cancel_cause);

	del_caller_outbound_call(id);


	if (status != SWITCH_STATUS_SUCCESS || cause != SWITCH_CAUSE_SUCCESS) {
		const char *acceptable = "false";

		switch (cause) {
		case SWITCH_CAUSE_ORIGINATOR_CANCEL:
		case SWITCH_CAUSE_PICKED_OFF:
			{
				acceptable = "true";

				for (i = 0; i < cbh->rowcount; i++) {
					struct call_helper *h = cbh->rows[i];
					char *sql = switch_mprintf("update fifo_outbound set ring_count=ring_count-1 "
											   "where uuid='%q' and ring_count > 0", h->uuid);
					fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);
				}

			}
			break;
		default:
			{
				for (i = 0; i < cbh->rowcount; i++) {
					struct call_helper *h = cbh->rows[i];
					char *sql = switch_mprintf("update fifo_outbound set ring_count=ring_count-1, "
											   "outbound_fail_count=outbound_fail_count+1, "
											   "outbound_fail_total_count = outbound_fail_total_count+1, "
											   "next_avail=%ld + lag + 1 where uuid='%q' and ring_count > 0",
											   (long) switch_epoch_time_now(NULL) + node->retry_delay, h->uuid);
					fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

				}
			}
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", node->name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "post-dial");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "outbound-strategy", "ringall");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "caller-uuid", id);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "result", "failure");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "acceptable", acceptable);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "cause", switch_channel_cause2str(cause));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "originate_string", originate_string);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Outbound-UUID-List", uuid_list);
			switch_event_fire(&event);
		}

		goto end;
	}

	channel = switch_core_session_get_channel(session);

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", node->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "post-dial");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "outbound-strategy", "ringall");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "caller-uuid", id);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Outbound-UUID", switch_channel_get_variable(channel, "fifo_outbound_uuid"));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Outbound-UUID-List", uuid_list);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "result", "success");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "originate_string", originate_string);
		switch_event_fire(&event);
	}


	switch_channel_set_variable(channel, "fifo_pop_order", NULL);

	app_name = "fifo";
	arg = switch_core_session_sprintf(session, "%s out nowait", node_name);
	extension = switch_caller_extension_new(session, app_name, arg);
	switch_caller_extension_add_application(session, extension, app_name, arg);
	switch_channel_set_caller_extension(channel, extension);
	switch_channel_set_state(channel, CS_EXECUTE);
	switch_channel_wait_for_state(channel, NULL, CS_EXECUTE);
	switch_channel_wait_for_flag(channel, CF_BRIDGED, SWITCH_TRUE, 5000, NULL);

	switch_core_session_rwunlock(session);




	for (i = 0; i < cbh->rowcount; i++) {
		struct call_helper *h = cbh->rows[i];
		char *sql = switch_mprintf("update fifo_outbound set ring_count=ring_count-1 where uuid='%q' and ring_count > 0",  h->uuid);
		fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);
	}

  end:

	cbh->ready = 1;

	if (node) {
		switch_mutex_lock(node->update_mutex);
		node->ring_consumer_count = 0;
		node->busy = 0;
		switch_mutex_unlock(node->update_mutex);
		switch_thread_rwlock_unlock(node->rwlock);
	}


	for (i = 0; i < cbh->rowcount; i++) {
		struct call_helper *h = cbh->rows[i];
		del_consumer_outbound_call(h->uuid);
	}

	switch_safe_free(originate_string);
	switch_safe_free(uuid_list);

	if (ovars) {
		switch_event_destroy(&ovars);
	}

	if (pop_dup) {
		switch_event_destroy(&pop_dup);
	}

 dpool:

	pool = cbh->pool;
	switch_core_destroy_memory_pool(&pool);

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	switch_mutex_unlock(globals.mutex);

	return NULL;
}

static void *SWITCH_THREAD_FUNC o_thread_run(switch_thread_t *thread, void *obj)
{
	struct call_helper *h = (struct call_helper *) obj;

	switch_core_session_t *session = NULL;
	switch_channel_t *channel;
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	switch_caller_extension_t *extension = NULL;
	char *app_name, *arg = NULL, *originate_string = NULL;
	const char *member_wait = NULL;
	fifo_node_t *node = NULL;
	switch_event_t *ovars = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;
	char *sql = NULL;
	char *expanded_originate_string = NULL;

	if (!globals.running) return NULL;

	switch_mutex_lock(globals.mutex);
	globals.threads++;
	switch_mutex_unlock(globals.mutex);


	switch_mutex_lock(globals.mutex);
	node = switch_core_hash_find(globals.fifo_hash, h->node_name);
	switch_thread_rwlock_rdlock(node->rwlock);
	switch_mutex_unlock(globals.mutex);

	if (node) {
		switch_mutex_lock(node->update_mutex);
		node->ring_consumer_count++;
		node->busy = 0;
		switch_mutex_unlock(node->update_mutex);
	}

	switch_event_create(&ovars, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(ovars);
	switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "originate_timeout", "%d", h->timeout);

	expanded_originate_string = switch_event_expand_headers(ovars, h->originate_string);

	if (node && switch_stristr("origination_caller", expanded_originate_string)) {
		originate_string = switch_mprintf("{execute_on_answer='unset fifo_hangup_check',fifo_name='%q',fifo_hangup_check='%q'}%s",
										  node->name, node->name, expanded_originate_string);
	} else {
		if (node && !zstr(node->outbound_name)) {
			originate_string = switch_mprintf("{execute_on_answer='unset fifo_hangup_check',fifo_name='%q',fifo_hangup_check='%q',"
											  "origination_caller_id_name=Queue,origination_caller_id_number='Queue: %q'}%s",
											  node->name, node->name,  node->outbound_name, expanded_originate_string);
		} else if (node) {
			originate_string = switch_mprintf("{execute_on_answer='unset fifo_hangup_check',fifo_name='%q',fifo_hangup_check='%q',"
											  "origination_caller_id_name=Queue,origination_caller_id_number='Queue: %q'}%s",
											  node->name, node->name,  node->name, expanded_originate_string);
		}

	}

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", node ? node->name : "");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "pre-dial");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Outbound-UUID", h->uuid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "outbound-strategy", "enterprise");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "originate_string", originate_string);
		switch_event_fire(&event);
	}


	sql = switch_mprintf("update fifo_outbound set ring_count=ring_count+1 where uuid='%s'", h->uuid);
	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

	status = switch_ivr_originate(NULL, &session, &cause, originate_string, h->timeout, NULL, NULL, NULL, NULL, ovars, SOF_NONE, NULL);

	if (status != SWITCH_STATUS_SUCCESS) {

		sql = switch_mprintf("update fifo_outbound set ring_count=ring_count-1, "
							 "outbound_fail_count=outbound_fail_count+1, next_avail=%ld + lag + 1 where uuid='%q'",
							 (long) switch_epoch_time_now(NULL) + (node ? node->retry_delay : 0), h->uuid);
		fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", node ? node->name : "");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "post-dial");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Outbound-UUID", h->uuid);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "outbound-strategy", "enterprise");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "result", "failure");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "cause", switch_channel_cause2str(cause));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "originate_string", originate_string);
			switch_event_fire(&event);
		}

		goto end;
	}

	channel = switch_core_session_get_channel(session);

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", node ? node->name : "");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "post-dial");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Outbound-UUID", h->uuid);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "outbound-strategy", "enterprise");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "result", "success");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "originate_string", originate_string);
		switch_event_fire(&event);
	}


	if ((member_wait = switch_channel_get_variable(channel, "fifo_member_wait")) || (member_wait = switch_channel_get_variable(channel, "member_wait"))) {
		if (strcasecmp(member_wait, "wait") && strcasecmp(member_wait, "nowait")) {
			member_wait = NULL;
		}
	}

	switch_channel_set_variable(channel, "fifo_outbound_uuid", h->uuid);
	app_name = "fifo";
	arg = switch_core_session_sprintf(session, "%s out %s", h->node_name, member_wait ? member_wait : "wait");
	extension = switch_caller_extension_new(session, app_name, arg);
	switch_caller_extension_add_application(session, extension, app_name, arg);
	switch_channel_set_caller_extension(channel, extension);
	switch_channel_set_state(channel, CS_EXECUTE);
	switch_core_session_rwunlock(session);

	sql = switch_mprintf("update fifo_outbound set ring_count=ring_count-1 where uuid='%q' and ring_count > 0", h->uuid);
	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

  end:

	if ( originate_string ){
		switch_safe_free(originate_string);
	}

	if (expanded_originate_string && expanded_originate_string != h->originate_string) {
		switch_safe_free(expanded_originate_string);
	}

	switch_event_destroy(&ovars);
	if (node) {
		switch_mutex_lock(node->update_mutex);
		if (node->ring_consumer_count-- < 0) {
			node->ring_consumer_count = 0;
		}
		node->busy = 0;
		switch_mutex_unlock(node->update_mutex);
		switch_thread_rwlock_unlock(node->rwlock);
	}
	switch_core_destroy_memory_pool(&h->pool);

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	switch_mutex_unlock(globals.mutex);

	return NULL;
}

static int place_call_ringall_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct callback_helper *cbh = (struct callback_helper *) pArg;
	struct call_helper *h;

	h = switch_core_alloc(cbh->pool, sizeof(*h));
	h->pool = cbh->pool;
	h->uuid = switch_core_strdup(h->pool, argv[0]);
	h->node_name = switch_core_strdup(h->pool, argv[1]);
	h->originate_string = switch_core_strdup(h->pool, argv[2]);
	h->timeout = atoi(argv[5]);

	cbh->rows[cbh->rowcount++] = h;

	if (cbh->rowcount == MAX_ROWS) return -1;

	if (cbh->need) {
		cbh->need--;
		return cbh->need ? 0 : -1;
	}

	return 0;

}

static int place_call_enterprise_callback(void *pArg, int argc, char **argv, char **columnNames)
{

	int *need = (int *) pArg;

	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	struct call_helper *h;

	switch_core_new_memory_pool(&pool);
	h = switch_core_alloc(pool, sizeof(*h));
	h->pool = pool;
	h->uuid = switch_core_strdup(h->pool, argv[0]);
	h->node_name = switch_core_strdup(h->pool, argv[1]);
	h->originate_string = switch_core_strdup(h->pool, argv[2]);
	h->timeout = atoi(argv[5]);


	switch_threadattr_create(&thd_attr, h->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, o_thread_run, h, h->pool);

	(*need)--;

	return *need ? 0 : -1;
}

static void find_consumers(fifo_node_t *node)
{
	char *sql;


	sql = switch_mprintf("select uuid, fifo_name, originate_string, simo_count, use_count, timeout, lag, "
						 "next_avail, expires, static, outbound_call_count, outbound_fail_count, hostname "
						 "from fifo_outbound "
						 "where taking_calls = 1 and (fifo_name = '%q') and ((use_count+ring_count) < simo_count) and (next_avail = 0 or next_avail <= %ld) "
						 "order by next_avail, outbound_fail_count, outbound_call_count",
						 node->name, (long) switch_epoch_time_now(NULL)
						 );



	switch(node->outbound_strategy) {
	case NODE_STRATEGY_ENTERPRISE:
		{
			int need = node_caller_count(node);

			if (node->outbound_per_cycle && node->outbound_per_cycle < need) {
				need = node->outbound_per_cycle;
			}

			fifo_execute_sql_callback(globals.sql_mutex, sql, place_call_enterprise_callback, &need);

		}
		break;
	case NODE_STRATEGY_RINGALL:
		{
			switch_thread_t *thread;
			switch_threadattr_t *thd_attr = NULL;
			struct callback_helper *cbh = NULL;
			switch_memory_pool_t *pool = NULL;

			switch_core_new_memory_pool(&pool);
			cbh = switch_core_alloc(pool, sizeof(*cbh));
			cbh->pool = pool;
			cbh->need = 1;

			if (node->outbound_per_cycle != cbh->need) {
				cbh->need = node->outbound_per_cycle;
			}

			fifo_execute_sql_callback(globals.sql_mutex, sql, place_call_ringall_callback, cbh);

			if (cbh->rowcount) {
				switch_threadattr_create(&thd_attr, cbh->pool);
				switch_threadattr_detach_set(thd_attr, 1);
				switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
				switch_thread_create(&thread, thd_attr, ringall_thread_run, cbh, cbh->pool);
			} else {
				switch_core_destroy_memory_pool(&pool);
			}

		}
		break;
	default:
		break;
	}


	switch_safe_free(sql);
}

static void *SWITCH_THREAD_FUNC node_thread_run(switch_thread_t *thread, void *obj)
{
	fifo_node_t *node, *last, *this_node;
	int cur_priority = 1;

	globals.node_thread_running = 1;

	while (globals.node_thread_running == 1) {
		int ppl_waiting, consumer_total, idle_consumers, found = 0;

		switch_mutex_lock(globals.mutex);

		if (globals.debug) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Trying priority: %d\n", cur_priority);

		last = NULL;
		node = globals.nodes;

		while(node) {
			int x = 0;
			switch_event_t *pop;

			this_node = node;
			node = node->next;

			if (this_node->ready == 0) {
				for (x = 0; x < MAX_PRI; x++) {
					while (fifo_queue_pop(this_node->fifo_list[x], &pop, 2) == SWITCH_STATUS_SUCCESS) {
						const char *caller_uuid = switch_event_get_header(pop, "unique-id");
						switch_ivr_kill_uuid(caller_uuid, SWITCH_CAUSE_MANAGER_REQUEST);
						switch_event_destroy(&pop);
					}
				}

			}


			if (this_node->ready == 0 && switch_thread_rwlock_trywrlock(this_node->rwlock) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s removed.\n", this_node->name);

				for (x = 0; x < MAX_PRI; x++) {
					while (fifo_queue_pop(this_node->fifo_list[x], &pop, 2) == SWITCH_STATUS_SUCCESS) {
						switch_event_destroy(&pop);
					}
				}

				if (last) {
					last->next = this_node->next;
				} else {
					globals.nodes = this_node->next;
				}

				switch_core_hash_destroy(&this_node->consumer_hash);
				switch_mutex_unlock(this_node->mutex);
				switch_mutex_unlock(this_node->update_mutex);
				switch_thread_rwlock_unlock(this_node->rwlock);
				switch_core_destroy_memory_pool(&this_node->pool);
				continue;
			}

			last = this_node;

			if (this_node->outbound_priority == 0) this_node->outbound_priority = 5;

			if (this_node->has_outbound && !this_node->busy && this_node->outbound_priority == cur_priority) {
				ppl_waiting = node_caller_count(this_node);
				consumer_total = this_node->consumer_count;
				idle_consumers = node_idle_consumers(this_node);

				if (globals.debug) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "%s waiting %d consumer_total %d idle_consumers %d ring_consumers %d pri %d\n",
									  this_node->name, ppl_waiting, consumer_total, idle_consumers, this_node->ring_consumer_count, this_node->outbound_priority);
				}


				if ((ppl_waiting - this_node->ring_consumer_count > 0) && (!consumer_total || !idle_consumers)) {
					found++;
					find_consumers(this_node);
					switch_yield(1000000);
				}
			}
		}
	

		if (++cur_priority > 10) {
			cur_priority = 1;
		}

		switch_mutex_unlock(globals.mutex);

		if (cur_priority == 1) {
			switch_yield(1000000);
		}
	}

	globals.node_thread_running = 0;

	return NULL;
}

static void start_node_thread(switch_memory_pool_t *pool)
{
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, pool);
	//switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&globals.node_thread, thd_attr, node_thread_run, pool, pool);
}

static int stop_node_thread(void)
{
	switch_status_t st = SWITCH_STATUS_SUCCESS;

	globals.node_thread_running = -1;
	switch_thread_join(&st, globals.node_thread);

	return 0;
}

static void check_cancel(fifo_node_t *node)
{
	int ppl_waiting;

	if (node->outbound_strategy != NODE_STRATEGY_ENTERPRISE) {
		return;
	}

	ppl_waiting = node_caller_count(node);

	if (node->ring_consumer_count > 0 && ppl_waiting < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Outbound call count (%d) exceeds required value for queue %s (%d), "
						  "Ending extraneous calls\n", node->ring_consumer_count, node->name, ppl_waiting);


		switch_core_session_hupall_matching_var("fifo_hangup_check", node->name, SWITCH_CAUSE_ORIGINATOR_CANCEL);
	}
}

static void send_presence(fifo_node_t *node)
{
	switch_event_t *event;
	int wait_count = 0;

	if (!globals.running) {
		return;
	}

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "proto", "queue");

		if (node->domain_name) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s@%s", node->name, node->domain_name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", node->name, node->domain_name);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "login", node->name);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "from", node->name);
		}

		if ((wait_count = node_caller_count(node)) > 0) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Active (%d waiting)", wait_count);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "status", "Idle");
		}
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "rpid", "unknown");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", 0);

		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "channel-state", wait_count > 0 ? "CS_ROUTING" : "CS_HANGUP");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "unique-id", node->name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "answer-state", wait_count > 0 ? "early" : "terminated");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "presence-call-direction", "inbound");
		switch_event_fire(&event);
	}
}

static void pres_event_handler(switch_event_t *event)
{
	char *to = switch_event_get_header(event, "to");
	char *domain_name = NULL;
	char *dup_to = NULL, *node_name , *dup_node_name;
	fifo_node_t *node;

	if (!globals.running) {
		return;
	}

	if (!to || strncasecmp(to, "queue+", 6) || !strchr(to, '@')) {
		return;
	}

	dup_to = strdup(to);
	switch_assert(dup_to);

	node_name = dup_to + 6;

	if ((domain_name = strchr(node_name, '@'))) {
		*domain_name++ = '\0';
	}

	dup_node_name = switch_mprintf("%q@%q", node_name, domain_name);


	switch_mutex_lock(globals.mutex);
	if (!(node = switch_core_hash_find(globals.fifo_hash, node_name)) && !(node = switch_core_hash_find(globals.fifo_hash, dup_node_name))) {
		node = create_node(node_name, 0, globals.sql_mutex);
		node->domain_name = switch_core_strdup(node->pool, domain_name);
		node->ready = 1;
	}

	switch_thread_rwlock_rdlock(node->rwlock);
	send_presence(node);
	switch_thread_rwlock_unlock(node->rwlock);

	switch_mutex_unlock(globals.mutex);

	switch_safe_free(dup_to);
	switch_safe_free(dup_node_name);
}

static uint32_t fifo_add_outbound(const char *node_name, const char *url, uint32_t priority)
{
	fifo_node_t *node;
	switch_event_t *call_event;
	uint32_t i = 0;

	if (priority >= MAX_PRI) {
		priority = MAX_PRI - 1;
	}

	if (!node_name) return 0;

	switch_mutex_lock(globals.mutex);

	if (!(node = switch_core_hash_find(globals.fifo_hash, node_name))) {
		node = create_node(node_name, 0, globals.sql_mutex);
	}

	switch_thread_rwlock_rdlock(node->rwlock);

	switch_mutex_unlock(globals.mutex);

	switch_event_create(&call_event, SWITCH_EVENT_CHANNEL_DATA);
	switch_event_add_header_string(call_event, SWITCH_STACK_BOTTOM, "dial-url", url);

	fifo_queue_push(node->fifo_list[priority], call_event);
	call_event = NULL;

	i = fifo_queue_size(node->fifo_list[priority]);

	switch_thread_rwlock_unlock(node->rwlock);

	return i;

}

SWITCH_STANDARD_API(fifo_check_bridge_function)
{
	stream->write_function(stream, "%s", (cmd && check_bridge_call(cmd)) ? "true" : "false");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(fifo_add_outbound_function)
{
	char *data = NULL, *argv[4] = { 0 };
	int argc;
	uint32_t priority = 0;

	if (zstr(cmd)) {
		goto fail;
	}

	data = strdup(cmd);

	if ((argc = switch_separate_string(data, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2 || !argv[0]) {
		goto fail;
	}

	if (argv[2]) {
		int tmp = atoi(argv[2]);
		if (tmp > 0) {
			priority = tmp;
		}
	}

	stream->write_function(stream, "%d", fifo_add_outbound(argv[0], argv[1], priority));


	free(data);
	return SWITCH_STATUS_SUCCESS;


  fail:

	free(data);
	stream->write_function(stream, "0");
	return SWITCH_STATUS_SUCCESS;

}

static void dec_use_count(switch_core_session_t *session, const char *type)
{
	char *sql;
	const char *outbound_id = NULL;
	switch_event_t *event;
	long now = (long) switch_epoch_time_now(NULL);
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if ((outbound_id = switch_channel_get_variable(channel, "fifo_outbound_uuid"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s untracking call on uuid %s!\n", switch_channel_get_name(channel), outbound_id);


		sql = switch_mprintf("delete from fifo_bridge where consumer_uuid='%q'", switch_core_session_get_uuid(session));
		fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_FALSE);

		del_bridge_call(outbound_id);
		sql = switch_mprintf("update fifo_outbound set use_count=use_count-1, stop_time=%ld, next_avail=%ld + lag + 1 where use_count > 0 and uuid='%q'",
							 now, now, outbound_id);
		fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);
		fifo_dec_use_count(outbound_id);
	}

	do_unbridge(session, NULL);

	if (type) {
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			uint64_t hold_usec = 0, tt_usec = 0;
			switch_caller_profile_t *originator_cp = NULL;

			originator_cp = switch_channel_get_caller_profile(channel);
			switch_channel_event_set_data(channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", MANUAL_QUEUE_NAME);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "channel-consumer-stop");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Type", type);
			if (outbound_id) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", outbound_id);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", fifo_get_use_count(outbound_id));
			}
			hold_usec = originator_cp->times->hold_accum;
			tt_usec = (switch_micro_time_now() - originator_cp->times->bridged) - hold_usec;
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Bridge-Time-us", "%"SWITCH_TIME_T_FMT, originator_cp->times->bridged);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Bridge-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(originator_cp->times->bridged / 1000));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Bridge-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(originator_cp->times->bridged / 1000000));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Talk-Time-us", "%"SWITCH_TIME_T_FMT, tt_usec);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Talk-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(tt_usec / 1000));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Talk-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(tt_usec / 1000000));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Hold-Time-us", "%"SWITCH_TIME_T_FMT, hold_usec);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Hold-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(hold_usec / 1000));
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Hold-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(hold_usec / 1000000));

			switch_event_fire(&event);
		}
	}
}

static switch_status_t hanguphook(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);

	if (state >= CS_HANGUP && !switch_channel_test_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_DID_HOOK)) {
		dec_use_count(session, "manual");
		switch_core_event_hook_remove_state_change(session, hanguphook);
		switch_channel_set_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_DID_HOOK);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(fifo_track_call_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *sql;
	const char *col1 = NULL, *col2 = NULL, *cid_name, *cid_number;
	switch_event_t *event;

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid!\n");
		return;
	}

	if (switch_channel_test_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_TRACKING)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s trying to double-track call!\n", switch_channel_get_name(channel));
		return;
	}

	switch_channel_set_variable(channel, "fifo_outbound_uuid", data);
	switch_channel_set_variable(channel, "fifo_track_call", "true");

	add_bridge_call(data);

	switch_channel_set_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_TRACKING);

	switch_core_event_hook_add_receive_message(session, messagehook);
	switch_core_event_hook_add_state_run(session, hanguphook);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s tracking call on uuid %s!\n", switch_channel_get_name(channel), data);


	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
		col1 = "manual_calls_in_count";
		col2 = "manual_calls_in_total_count";
	} else {
		col1 = "manual_calls_out_count";
		col2 = "manual_calls_out_total_count";
	}

	sql = switch_mprintf("update fifo_outbound set stop_time=0,start_time=%ld,outbound_fail_count=0,use_count=use_count+1,%s=%s+1,%s=%s+1 where uuid='%q'",
						 (long) switch_epoch_time_now(NULL), col1, col1, col2, col2, data);
	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);
	fifo_inc_use_count(data);

	if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
		cid_name = switch_channel_get_variable(channel, "destination_number");
		cid_number = cid_name;
	} else {
		cid_name = switch_channel_get_variable(channel, "caller_id_name");
		cid_number = switch_channel_get_variable(channel, "caller_id_number");
	}

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", MANUAL_QUEUE_NAME);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "channel-consumer-start");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", data);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", fifo_get_use_count(data));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Type", "manual");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-CID-Name", cid_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-CID-Number", cid_number);
		switch_event_fire(&event);
	}
}


static void fifo_caller_add(fifo_node_t *node, switch_core_session_t *session)
{
	char *sql;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	sql = switch_mprintf("insert into fifo_callers (fifo_name,uuid,caller_caller_id_name,caller_caller_id_number,timestamp) "
						 "values ('%q','%q','%q','%q',%ld)",
						 node->name,
						 switch_core_session_get_uuid(session),
						 switch_str_nil(switch_channel_get_variable(channel, "caller_id_name")),
						 switch_str_nil(switch_channel_get_variable(channel, "caller_id_number")),
						 switch_epoch_time_now(NULL));

	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);
}

static void fifo_caller_del(const char *uuid)
{
	char *sql;

	if (uuid) {
		sql = switch_mprintf("delete from fifo_callers where uuid='%q'", uuid);
	} else {
		sql = switch_mprintf("delete from fifo_callers");
	}

	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

}



typedef enum {
	STRAT_MORE_PPL,
	STRAT_WAITING_LONGER,
} fifo_strategy_t;

#define MAX_NODES_PER_CONSUMER 25
#define FIFO_DESC "Fifo for stacking parked calls."
#define FIFO_USAGE "<fifo name>[!<importance_number>] [in [<announce file>|undef] [<music file>|undef] | out [wait|nowait] [<announce file>|undef] [<music file>|undef]]"
SWITCH_STANDARD_APP(fifo_function)
{
	int argc;
	char *mydata = NULL, *argv[5] = { 0 };
	fifo_node_t *node = NULL, *node_list[MAX_NODES_PER_CONSUMER + 1] = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int do_destroy = 0, do_wait = 1, node_count = 0, i = 0;
	const char *moh = NULL;
	const char *announce = NULL;
	switch_event_t *event = NULL;
	char date[80] = "";
	switch_time_exp_t tm;
	switch_time_t ts = switch_micro_time_now();
	switch_size_t retsize;
	char *list_string;
	int nlist_count;
	char *nlist[MAX_NODES_PER_CONSUMER];
	int consumer = 0, in_table = 0;
	const char *arg_fifo_name = NULL;
	const char *arg_inout = NULL;
	const char *serviced_uuid = NULL;
	
	if (!globals.running) {
		return;
	}

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No Args\n");
		return;
	}

	switch_channel_set_variable(channel, "fifo_hangup_check", NULL);

	mydata = switch_core_session_strdup(session, data);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	arg_fifo_name = argv[0];
	arg_inout = argv[1];

	if (!(arg_fifo_name && arg_inout)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
		return;
	}

	if (!strcasecmp(arg_inout, "out")) {
		consumer = 1;
	} else if (strcasecmp(arg_inout, "in")) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
		return;
	}

	list_string = switch_core_session_strdup(session, arg_fifo_name);

	if (!(nlist_count = switch_separate_string(list_string, ',', nlist, (sizeof(nlist) / sizeof(nlist[0]))))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
		return;
	}

	if (!consumer && nlist_count > 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
		return;
	}

	switch_mutex_lock(globals.mutex);
	for (i = 0; i < nlist_count; i++) {
		int importance = 0;
		char *p;

		if ((p = strrchr(nlist[i], '!'))) {
			*p++ = '\0';
			importance = atoi(p);
			if (importance < 0) {
				importance = 0;
			}
		}


		if (!(node = switch_core_hash_find(globals.fifo_hash, nlist[i]))) {
			node = create_node(nlist[i], importance, globals.sql_mutex);
			node->ready = 1;
		}

		switch_thread_rwlock_rdlock(node->rwlock);
		node_list[node_count++] = node;
	}

	switch_mutex_unlock(globals.mutex);

	moh = switch_channel_get_variable(channel, "fifo_music");
	announce = switch_channel_get_variable(channel, "fifo_announce");

	if (consumer) {
		if (argc > 3) {
			announce = argv[3];
		}

		if (argc > 4) {
			moh = argv[4];
		}
	} else {
		if (argc > 2) {
			announce = argv[2];
		}

		if (argc > 3) {
			moh = argv[3];
		}
	}

	if (moh && !strcasecmp(moh, "silence")) {
		moh = NULL;
	}

	check_string(announce);
	check_string(moh);
	switch_assert(node);

	switch_core_media_bug_pause(session);

	if (!consumer) {
		switch_core_session_t *other_session;
		switch_channel_t *other_channel;
		const char *uuid = switch_core_session_get_uuid(session);
		const char *pri;
		char tmp[25] = "";
		int p = 0;
		int aborted = 0;
		fifo_chime_data_t cd = { {0} };
		const char *chime_list = switch_channel_get_variable(channel, "fifo_chime_list");
		const char *chime_freq = switch_channel_get_variable(channel, "fifo_chime_freq");
		const char *orbit_exten = switch_channel_get_variable(channel, "fifo_orbit_exten");
		const char *orbit_dialplan = switch_channel_get_variable(channel, "fifo_orbit_dialplan");
		const char *orbit_context = switch_channel_get_variable(channel, "fifo_orbit_context");

		const char *orbit_ann = switch_channel_get_variable(channel, "fifo_orbit_announce");
		const char *caller_exit_key = switch_channel_get_variable(channel, "fifo_caller_exit_key");
		int freq = 30;
		int ftmp = 0;
		int to = 60;
		switch_event_t *call_event;

		if (orbit_exten) {
			char *ot;
			if ((cd.orbit_exten = switch_core_session_strdup(session, orbit_exten))) {
				if ((ot = strchr(cd.orbit_exten, ':'))) {
					*ot++ = '\0';
					if ((to = atoi(ot)) < 0) {
						to = 60;
					}
				}
				cd.orbit_timeout = switch_epoch_time_now(NULL) + to;
			}
			cd.orbit_dialplan = switch_core_session_strdup(session, orbit_dialplan);
			cd.orbit_context = switch_core_session_strdup(session, orbit_context);
		}

		if (chime_freq) {
			ftmp = atoi(chime_freq);
			if (ftmp > 0) {
				freq = ftmp;
			}
		}

		switch_channel_answer(channel);

		switch_mutex_lock(node->update_mutex);

		if ((pri = switch_channel_get_variable(channel, "fifo_priority"))) {
			p = atoi(pri);
		}

		if (p >= MAX_PRI) {
			p = MAX_PRI - 1;
		}

		if (!node_caller_count(node)) {
			node->start_waiting = switch_micro_time_now();
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "push");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Slot", "%d", p);
			switch_event_fire(&event);
		}

		switch_event_create(&call_event, SWITCH_EVENT_CHANNEL_DATA);
		switch_channel_event_set_data(channel, call_event);


		fifo_queue_push(node->fifo_list[p], call_event);
		fifo_caller_add(node, session);
		in_table = 1;

		call_event = NULL;
		switch_snprintf(tmp, sizeof(tmp), "%d", fifo_queue_size(node->fifo_list[p]));
		switch_channel_set_variable(channel, "fifo_position", tmp);

		if (!pri) {
			switch_snprintf(tmp, sizeof(tmp), "%d", p);
			switch_channel_set_variable(channel, "fifo_priority", tmp);
		}

		switch_mutex_unlock(node->update_mutex);

		ts = switch_micro_time_now();
		switch_time_exp_lt(&tm, ts);
		switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
		switch_channel_set_variable(channel, "fifo_status", "WAITING");
		switch_channel_set_variable(channel, "fifo_timestamp", date);
		switch_channel_set_variable(channel, "fifo_serviced_uuid", NULL);

		switch_channel_set_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_BRIDGE_TAG);

		if (chime_list) {
			char *list_dup = switch_core_session_strdup(session, chime_list);
			cd.total = switch_separate_string(list_dup, ',', cd.list, (sizeof(cd.list) / sizeof(cd.list[0])));
			cd.freq = freq;
			cd.next = switch_epoch_time_now(NULL) + cd.freq;
			cd.exit_key = (char *) switch_channel_get_variable(channel, "fifo_caller_exit_key");
		}

		send_presence(node);

		while (switch_channel_ready(channel)) {
			switch_input_args_t args = { 0 };
			char buf[25] = "";
			switch_status_t rstatus;

			args.input_callback = moh_on_dtmf;
			args.buf = buf;
			args.buflen = sizeof(buf);

			if (cd.total || cd.orbit_timeout) {
				args.read_frame_callback = caller_read_frame_callback;
				args.user_data = &cd;
			}

			if (cd.abort || cd.do_orbit) {
				aborted = 1;
				goto abort;
			}

			if ((serviced_uuid = switch_channel_get_variable(channel, "fifo_serviced_uuid"))) {
				break;
			}

			switch_core_session_flush_private_events(session);

			if (moh) {
				rstatus = switch_ivr_play_file(session, NULL, moh, &args);
			} else {
				rstatus = switch_ivr_collect_digits_callback(session, &args, 0, 0);
			}

			if (!SWITCH_READ_ACCEPTABLE(rstatus)) {
				aborted = 1;
				goto abort;
			}

			if (match_key(caller_exit_key, *buf)) {
				switch_channel_set_variable(channel, "fifo_caller_exit_key", (char *)buf);
				aborted = 1;
				goto abort;
			}

		}

		if (!serviced_uuid && switch_channel_ready(channel)) {
			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
		} else if ((other_session = switch_core_session_locate(serviced_uuid))) {
			int ready;
			other_channel = switch_core_session_get_channel(other_session);
			ready = switch_channel_ready(other_channel);
			switch_core_session_rwunlock(other_session);
			if (!ready) {
				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			}
		}

		switch_core_session_flush_private_events(session);

		if (switch_channel_ready(channel)) {
			if (announce) {
				switch_ivr_play_file(session, NULL, announce, NULL);
			}
		}

	abort:

		switch_channel_clear_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_BRIDGE_TAG);

		if (!aborted && switch_channel_ready(channel)) {
			switch_channel_set_state(channel, CS_HIBERNATE);
			goto done;
		} else {
			ts = switch_micro_time_now();
			switch_time_exp_lt(&tm, ts);
			switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
			switch_channel_set_variable(channel, "fifo_status", cd.do_orbit ? "TIMEOUT" : "ABORTED");
			switch_channel_set_variable(channel, "fifo_timestamp", date);

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(channel, event);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", cd.do_orbit ? "timeout" : "abort");
				switch_event_fire(&event);
			}

			switch_mutex_lock(globals.mutex);
			switch_mutex_lock(node->update_mutex);
			node_remove_uuid(node, uuid);
			switch_mutex_unlock(node->update_mutex);
			send_presence(node);
			check_cancel(node);
			switch_mutex_unlock(globals.mutex);

		}

		if ((switch_true(switch_channel_get_variable(channel, "fifo_caller_exit_to_orbit")) || cd.do_orbit) && cd.orbit_exten) {
			if (orbit_ann) {
				switch_ivr_play_file(session, NULL, orbit_ann, NULL);
			}

			if (strcmp(cd.orbit_exten, "_continue_")) {
				switch_ivr_session_transfer(session, cd.orbit_exten, cd.orbit_dialplan, cd.orbit_context);
			}
		}

		cancel_caller_outbound_call(switch_core_session_get_uuid(session), SWITCH_CAUSE_ORIGINATOR_CANCEL);

		goto done;

	} else {					/* consumer */
		switch_event_t *pop = NULL;
		switch_frame_t *read_frame;
		switch_status_t status;
		switch_core_session_t *other_session;
		switch_input_args_t args = { 0 };
		const char *pop_order = NULL;
		int custom_pop = 0;
		int pop_array[MAX_PRI] = { 0 };
		char *pop_list[MAX_PRI] = { 0 };
		const char *fifo_consumer_wrapup_sound = NULL;
		const char *fifo_consumer_wrapup_key = NULL;
		const char *sfifo_consumer_wrapup_time = NULL;
		uint32_t fifo_consumer_wrapup_time = 0;
		switch_time_t wrapup_time_elapsed = 0, wrapup_time_started = 0, wrapup_time_remaining = 0;
		const char *my_id;
		char buf[5] = "";
		const char *strat_str = switch_channel_get_variable(channel, "fifo_strategy");
		fifo_strategy_t strat = STRAT_WAITING_LONGER;
		const char *url = NULL;
		const char *caller_uuid = NULL;
		const char *outbound_id = switch_channel_get_variable(channel, "fifo_outbound_uuid");
		switch_event_t *event;
		const char *cid_name = NULL, *cid_number = NULL;

		//const char *track_use_count = switch_channel_get_variable(channel, "fifo_track_use_count");
		//int do_track = switch_true(track_use_count);

		if (switch_core_event_hook_remove_receive_message(session, messagehook) == SWITCH_STATUS_SUCCESS) {
			dec_use_count(session, NULL);
			switch_core_event_hook_remove_state_change(session, hanguphook);
			switch_channel_clear_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_TRACKING);
		}

		if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			cid_name = switch_channel_get_variable(channel, "callee_id_name");
			cid_number = switch_channel_get_variable(channel, "callee_id_number");

			if (!cid_name) {
				cid_name = switch_channel_get_variable(channel, "destination_number");
			}
			if (!cid_number) {
				cid_number = cid_name;
			}
		} else {
			cid_name = switch_channel_get_variable(channel, "caller_id_name");
			cid_number = switch_channel_get_variable(channel, "caller_id_number");
		}

		if (!zstr(strat_str)) {
			if (!strcasecmp(strat_str, "more_ppl")) {
				strat = STRAT_MORE_PPL;
			} else if (!strcasecmp(strat_str, "waiting_longer")) {
				strat = STRAT_WAITING_LONGER;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid strategy\n");
				goto done;
			}
		}

		if (argc > 2) {
			if (!strcasecmp(argv[2], "nowait")) {
				do_wait = 0;
			} else if (strcasecmp(argv[2], "wait")) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
				goto done;
			}
		}

		if (!(my_id = switch_channel_get_variable(channel, "fifo_consumer_id"))) {
			my_id = switch_core_session_get_uuid(session);
		}

		if (do_wait) {
			for (i = 0; i < node_count; i++) {
				if (!(node = node_list[i])) {
					continue;
				}
				switch_mutex_lock(node->mutex);
				node->consumer_count++;
				switch_core_hash_insert(node->consumer_hash, switch_core_session_get_uuid(session), session);
				switch_mutex_unlock(node->mutex);
			}
			switch_channel_answer(channel);
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_start");
			switch_event_fire(&event);
		}

		ts = switch_micro_time_now();
		switch_time_exp_lt(&tm, ts);
		switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
		switch_channel_set_variable(channel, "fifo_status", "WAITING");
		switch_channel_set_variable(channel, "fifo_timestamp", date);

		if ((pop_order = switch_channel_get_variable(channel, "fifo_pop_order"))) {
			char *tmp = switch_core_session_strdup(session, pop_order);
			int x;
			custom_pop = switch_separate_string(tmp, ',', pop_list, (sizeof(pop_list) / sizeof(pop_list[0])));
			if (custom_pop >= MAX_PRI) {
				custom_pop = MAX_PRI - 1;
			}

			for (x = 0; x < custom_pop; x++) {
				int temp;
				switch_assert(pop_list[x]);
				temp = atoi(pop_list[x]);
				if (temp > -1 && temp < MAX_PRI) {
					pop_array[x] = temp;
				}
			}
		} else {
			int x = 0;
			for (x = 0; x < MAX_PRI; x++) {
				pop_array[x] = x;
			}
		}

		while (switch_channel_ready(channel)) {
			int x = 0, winner = -1;
			switch_time_t longest = (0xFFFFFFFFFFFFFFFFULL / 2);
			uint32_t importance = 0, waiting = 0, most_waiting = 0;

			pop = NULL;

			if (moh && do_wait) {
				switch_status_t moh_status;
				memset(&args, 0, sizeof(args));
				args.read_frame_callback = consumer_read_frame_callback;
				args.user_data = node_list;
				moh_status = switch_ivr_play_file(session, NULL, moh, &args);

				if (!SWITCH_READ_ACCEPTABLE(moh_status)) {
					break;
				}
			}

			for (i = 0; i < node_count; i++) {
				if (!(node = node_list[i])) {
					continue;
				}

				if ((waiting = node_caller_count(node))) {

					if (!importance || node->importance > importance) {
						if (strat == STRAT_WAITING_LONGER) {
							if (node->start_waiting < longest) {
								longest = node->start_waiting;
								winner = i;
							}
						} else {
							if (waiting > most_waiting) {
								most_waiting = waiting;
								winner = i;
							}
						}
					}

					if (node->importance > importance) {
						importance = node->importance;
					}
				}
			}

			if (winner > -1) {
				node = node_list[winner];
			} else {
				node = NULL;
			}

			if (node) {
				const char *varval, *check = NULL;

				check = switch_channel_get_variable(channel, "fifo_bridge_uuid_required");

				if ((varval = switch_channel_get_variable(channel, "fifo_bridge_uuid"))) {
					if (check_bridge_call(varval) && switch_true(check)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Call has already been answered\n",
										  switch_channel_get_name(channel));
						goto done;
					}

					cancel_consumer_outbound_call(outbound_id, SWITCH_CAUSE_ORIGINATOR_CANCEL);

					for (x = 0; x < MAX_PRI; x++) {
						if (fifo_queue_pop_nameval(node->fifo_list[pop_array[x]], "+unique-id", varval, &pop, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && pop) {
							cancel_caller_outbound_call(varval, SWITCH_CAUSE_PICKED_OFF);
							break;
						}
					}
					if (!pop && switch_true(check)) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s Call has already been answered\n",
										  switch_channel_get_name(channel));

						goto done;
					}
				}

				if (!pop && (varval = switch_channel_get_variable(channel, "fifo_target_skill"))) {
					for (x = 0; x < MAX_PRI; x++) {
						if (fifo_queue_pop_nameval(node->fifo_list[pop_array[x]], "variable_fifo_skill",
												   varval, &pop, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && pop) {
							break;
						}
					}
				}

				if (!pop) {
					for (x = 0; x < MAX_PRI; x++) {
						if (fifo_queue_pop_nameval(node->fifo_list[pop_array[x]], "variable_fifo_vip", "true",
												   &pop, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && pop) {
							break;
						}
					}
				}

				if (!pop) {
					if (custom_pop) {
						for (x = 0; x < MAX_PRI; x++) {
							if (fifo_queue_pop(node->fifo_list[pop_array[x]], &pop, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && pop) {
								break;
							}
						}
					} else {
						for (x = 0; x < MAX_PRI; x++) {
							if (fifo_queue_pop(node->fifo_list[x], &pop, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && pop) {
								break;
							}
						}
					}
				}

				if (pop && !node_caller_count(node)) {
					switch_mutex_lock(node->update_mutex);
					node->start_waiting = 0;
					switch_mutex_unlock(node->update_mutex);
				}
			}

			if (!pop) {
				if (!do_wait) {
					break;
				}

				status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

				if (!SWITCH_READ_ACCEPTABLE(status)) {
					break;
				}

				continue;
			}

			url = switch_event_get_header(pop, "dial-url");
			caller_uuid = switch_core_session_strdup(session, switch_event_get_header(pop, "unique-id"));
			switch_event_destroy(&pop);

			if (url) {
				switch_call_cause_t cause = SWITCH_CAUSE_NONE;
				const char *o_announce = NULL;

				if ((o_announce = switch_channel_get_variable(channel, "fifo_outbound_announce"))) {
					status = switch_ivr_play_file(session, NULL, o_announce, NULL);
					if (!SWITCH_READ_ACCEPTABLE(status)) {
						break;
					}
				}

				if (switch_ivr_originate(session, &other_session, &cause, url, 120, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL) != SWITCH_STATUS_SUCCESS) {
					other_session = NULL;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Originate to [%s] failed, cause: %s\n", url,
									  switch_channel_cause2str(cause));

					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
						switch_channel_event_set_data(channel, event);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "caller_outbound");
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Result", "failure:%s", switch_channel_cause2str(cause));
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Outbound-URL", url);
						switch_event_fire(&event);
					}

				} else {
					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
						switch_channel_event_set_data(channel, event);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "caller_outbound");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Result", "success");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Outbound-URL", url);
						switch_event_fire(&event);
					}
					url = NULL;
					caller_uuid = switch_core_session_strdup(session, switch_core_session_get_uuid(other_session));
				}

			} else {
				if ((other_session = switch_core_session_locate(caller_uuid))) {
					switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
						switch_channel_event_set_data(other_channel, event);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "caller_pop");
						switch_event_fire(&event);
					}
				}
			}

			if (node && other_session) {
				switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
				switch_caller_profile_t *originator_cp, *originatee_cp;
				const char *o_announce = NULL;
				const char *record_template = switch_channel_get_variable(channel, "fifo_record_template");
				char *expanded = NULL;
				char *sql = NULL;
				long epoch_start, epoch_end;

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_pop");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-UUID", switch_core_session_get_uuid(other_session));
					switch_event_fire(&event);
				}

				if ((o_announce = switch_channel_get_variable(other_channel, "fifo_override_announce"))) {
					announce = o_announce;
				}

				if (announce) {
					status = switch_ivr_play_file(session, NULL, announce, NULL);
					if (!SWITCH_READ_ACCEPTABLE(status)) {
						break;
					}
				}


				switch_channel_set_variable(other_channel, "fifo_serviced_by", my_id);
				switch_channel_set_variable(other_channel, "fifo_serviced_uuid", switch_core_session_get_uuid(session));
				switch_channel_set_flag(other_channel, CF_BREAK);

				while (switch_channel_ready(channel) && switch_channel_ready(other_channel) &&
					   switch_channel_test_app_flag_key(FIFO_APP_KEY, other_channel, FIFO_APP_BRIDGE_TAG)) {
					status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
					if (!SWITCH_READ_ACCEPTABLE(status)) {
						break;
					}
				}

				if (!(switch_channel_ready(channel))) {
					const char *app = switch_channel_get_variable(other_channel, "current_application");
					const char *arg = switch_channel_get_variable(other_channel, "current_application_data");
					switch_caller_extension_t *extension = NULL;                


					switch_channel_set_variable_printf(channel, "last_sent_callee_id_name", "%s (AGENT FAIL)", 
													   switch_channel_get_variable(other_channel, "caller_id_name"));
					switch_channel_set_variable(channel, "last_sent_callee_id_number", switch_channel_get_variable(other_channel, "caller_id_number"));

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, 
									  "Customer %s %s [%s] appears to be abandoned by agent %s [%s] "
									  "but is still on the line, redirecting them back to the queue with VIP status.\n",
									  switch_channel_get_name(other_channel), 
									  switch_channel_get_variable(other_channel, "caller_id_name"), 
									  switch_channel_get_variable(other_channel, "caller_id_number"),
									  switch_channel_get_variable(channel, "caller_id_name"),
									  switch_channel_get_variable(channel, "caller_id_number"));

					switch_channel_wait_for_state_timeout(other_channel, CS_HIBERNATE, 5000);

					send_presence(node);
					check_cancel(node);
					
					if (app) {
						extension = switch_caller_extension_new(other_session, app, arg);
						switch_caller_extension_add_application(other_session, extension, app, arg);
						switch_channel_set_caller_extension(other_channel, extension);
						switch_channel_set_state(other_channel, CS_EXECUTE);
					} else {
						switch_channel_hangup(other_channel, SWITCH_CAUSE_NORMAL_CLEARING);
					}
					switch_channel_set_variable(other_channel, "fifo_vip", "true");

					switch_core_session_rwunlock(other_session);
					break;
				}

				switch_channel_answer(channel);

				if (switch_channel_inbound_display(other_channel)) {
					if (switch_channel_direction(other_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
						switch_channel_set_flag(other_channel, CF_BLEG);
					}
				}


				switch_channel_step_caller_profile(channel);
				switch_channel_step_caller_profile(other_channel);

				originator_cp = switch_channel_get_caller_profile(channel);
				originatee_cp = switch_channel_get_caller_profile(other_channel);
				
				switch_channel_set_originator_caller_profile(other_channel, switch_caller_profile_clone(other_session, originator_cp));
				switch_channel_set_originatee_caller_profile(channel, switch_caller_profile_clone(session, originatee_cp));
				
				
				originator_cp->callee_id_name = switch_core_strdup(originator_cp->pool, originatee_cp->callee_id_name);
				originator_cp->callee_id_number = switch_core_strdup(originator_cp->pool, originatee_cp->callee_id_number);


				originatee_cp->callee_id_name = switch_core_strdup(originatee_cp->pool, originatee_cp->caller_id_name);
				originatee_cp->callee_id_number = switch_core_strdup(originatee_cp->pool, originatee_cp->caller_id_number);
				
				originatee_cp->caller_id_name = switch_core_strdup(originatee_cp->pool, originator_cp->caller_id_name);
				originatee_cp->caller_id_number = switch_core_strdup(originatee_cp->pool, originator_cp->caller_id_number);




				ts = switch_micro_time_now();
				switch_time_exp_lt(&tm, ts);
				epoch_start = (long)switch_epoch_time_now(NULL);
				switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
				switch_channel_set_variable(channel, "fifo_status", "TALKING");
				switch_channel_set_variable(channel, "fifo_target", caller_uuid);
				switch_channel_set_variable(channel, "fifo_timestamp", date);
				switch_channel_set_variable_printf(channel, "fifo_epoch_start_bridge", "%ld", epoch_start);
				switch_channel_set_variable(channel, "fifo_role", "consumer");

				switch_channel_set_variable(other_channel, "fifo_status", "TALKING");
				switch_channel_set_variable(other_channel, "fifo_timestamp", date);
				switch_channel_set_variable_printf(other_channel, "fifo_epoch_start_bridge", "%ld", epoch_start);
				switch_channel_set_variable(other_channel, "fifo_target", switch_core_session_get_uuid(session));
				switch_channel_set_variable(other_channel, "fifo_role", "caller");

				send_presence(node);

				if (record_template) {
					expanded = switch_channel_expand_variables(other_channel, record_template);
					switch_ivr_record_session(session, expanded, 0, NULL);
				}

				switch_core_media_bug_resume(session);
				switch_core_media_bug_resume(other_session);

				switch_process_import(session, other_channel, "fifo_caller_consumer_import", switch_channel_get_variable(channel, "fifo_import_prefix"));
				switch_process_import(other_session, channel, "fifo_consumer_caller_import", switch_channel_get_variable(other_channel, "fifo_import_prefix"));


				if (outbound_id) {
					cancel_consumer_outbound_call(outbound_id, SWITCH_CAUSE_ORIGINATOR_CANCEL);
					add_bridge_call(outbound_id);

					sql = switch_mprintf("update fifo_outbound set stop_time=0,start_time=%ld,use_count=use_count+1,outbound_fail_count=0 where uuid='%s'",
										 switch_epoch_time_now(NULL), outbound_id);


					fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);
					fifo_inc_use_count(outbound_id);

				}

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "channel-consumer-start");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Type", "onhook");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-CID-Name", cid_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-CID-Number", cid_number);
					if (outbound_id) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", outbound_id);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", fifo_get_use_count(outbound_id));
					}
					switch_event_fire(&event);
				}



				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-consumer-start");
					if (outbound_id) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", outbound_id);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", fifo_get_use_count(outbound_id));
					}

					switch_event_fire(&event);
				}
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(other_channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-caller-start");
					switch_event_fire(&event);
				}

				
				add_bridge_call(switch_core_session_get_uuid(other_session));
				add_bridge_call(switch_core_session_get_uuid(session));

				sql = switch_mprintf("insert into fifo_bridge "
									 "(fifo_name,caller_uuid,caller_caller_id_name,caller_caller_id_number,consumer_uuid,consumer_outgoing_uuid,bridge_start) "
									 "values ('%q','%q','%q','%q','%q','%q',%ld)",
									 node->name,
									 switch_core_session_get_uuid(other_session),
									 switch_str_nil(switch_channel_get_variable(other_channel, "caller_id_name")),
									 switch_str_nil(switch_channel_get_variable(other_channel, "caller_id_number")),
									 switch_core_session_get_uuid(session),
									 switch_str_nil(outbound_id),
									 (long) switch_epoch_time_now(NULL)
									 );


				fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_FALSE);


				switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(other_session));
				switch_channel_set_variable(other_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(session));

				switch_channel_set_variable(switch_core_session_get_channel(other_session), "fifo_initiated_bridge", "true");
				switch_channel_set_variable(switch_core_session_get_channel(other_session), "fifo_bridge_role", "caller");
				switch_channel_set_variable(switch_core_session_get_channel(session), "fifo_initiated_bridge", "true");
				switch_channel_set_variable(switch_core_session_get_channel(session), "fifo_bridge_role", "consumer");

				switch_ivr_multi_threaded_bridge(session, other_session, on_dtmf, other_session, session);

				if (switch_channel_test_flag(other_channel, CF_TRANSFER) && switch_channel_up(other_channel)) {
					switch_channel_set_variable(switch_core_session_get_channel(other_session), "fifo_initiated_bridge", NULL);
					switch_channel_set_variable(switch_core_session_get_channel(other_session), "fifo_bridge_role", NULL);
				}
				
				if (switch_channel_test_flag(channel, CF_TRANSFER) && switch_channel_up(channel)) {
					switch_channel_set_variable(switch_core_session_get_channel(other_session), "fifo_initiated_bridge", NULL);
					switch_channel_set_variable(switch_core_session_get_channel(other_session), "fifo_bridge_role", NULL);
				}

				if (outbound_id) {
					long now = (long) switch_epoch_time_now(NULL);

					sql = switch_mprintf("update fifo_outbound set stop_time=%ld, use_count=use_count-1, "
										 "outbound_call_total_count=outbound_call_total_count+1, "
										 "outbound_call_count=outbound_call_count+1, next_avail=%ld + lag + 1 where uuid='%s' and use_count > 0",
										 now, now, outbound_id);

					fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

					del_bridge_call(outbound_id);
					fifo_dec_use_count(outbound_id);

				}


				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					uint64_t hold_usec = 0, tt_usec = 0;
					switch_channel_event_set_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", arg_fifo_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "channel-consumer-stop");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Type", "onhook");
					if (outbound_id) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", outbound_id);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", fifo_get_use_count(outbound_id));
					}
					hold_usec = originator_cp->times->hold_accum;
					tt_usec = (switch_micro_time_now() - originator_cp->times->bridged) - hold_usec;
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Bridge-Time-us", "%"SWITCH_TIME_T_FMT, originator_cp->times->bridged);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Bridge-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(originator_cp->times->bridged / 1000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Bridge-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(originator_cp->times->bridged / 1000000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Talk-Time-us", "%"SWITCH_TIME_T_FMT, tt_usec);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Talk-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(tt_usec / 1000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Talk-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(tt_usec / 1000000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Hold-Time-us", "%"SWITCH_TIME_T_FMT, hold_usec);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Hold-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(hold_usec / 1000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Hold-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(hold_usec / 1000000));
					
					switch_event_fire(&event);
				}

				del_bridge_call(switch_core_session_get_uuid(session));
				del_bridge_call(switch_core_session_get_uuid(other_session));


				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-consumer-stop");
					if (outbound_id) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Outbound-ID", outbound_id);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Consumer-Use-Count", "%d", fifo_get_use_count(outbound_id));
					}	
					switch_event_fire(&event);
				}
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					uint64_t hold_usec = 0, tt_usec = 0;
					switch_channel_event_set_data(other_channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "bridge-caller-stop");
					hold_usec = originatee_cp->times->hold_accum;
					tt_usec = (switch_micro_time_now() - originatee_cp->times->bridged) - hold_usec;
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-Talk-Time-us", "%"SWITCH_TIME_T_FMT, tt_usec);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-Talk-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(tt_usec / 1000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-Talk-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(tt_usec / 1000000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-Hold-Time-us", "%"SWITCH_TIME_T_FMT, hold_usec);
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-Hold-Time-ms", "%"SWITCH_TIME_T_FMT, (uint64_t)(hold_usec / 1000));
					switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Caller-Hold-Time-s", "%"SWITCH_TIME_T_FMT, (uint64_t)(hold_usec / 1000000));
					switch_event_fire(&event);
				}

				epoch_end = (long)switch_epoch_time_now(NULL);

				switch_channel_set_variable_printf(channel, "fifo_epoch_stop_bridge", "%ld", epoch_end);
				switch_channel_set_variable_printf(channel, "fifo_bridge_seconds", "%d", epoch_end - epoch_start);

				switch_channel_set_variable_printf(other_channel, "fifo_epoch_stop_bridge", "%ld", epoch_end);
				switch_channel_set_variable_printf(other_channel, "fifo_bridge_seconds", "%d", epoch_end - epoch_start);

				sql = switch_mprintf("delete from fifo_bridge where consumer_uuid='%q'", switch_core_session_get_uuid(session));
				fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_FALSE);


				if (switch_channel_ready(channel)) {
					switch_core_media_bug_pause(session);
				}

				if (record_template) {
					switch_ivr_stop_record_session(session, expanded);
					if (expanded != record_template) {
						switch_safe_free(expanded);
					}
				}

				ts = switch_micro_time_now();
				switch_time_exp_lt(&tm, ts);
				switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
				switch_channel_set_variable(channel, "fifo_status", "WAITING");
				switch_channel_set_variable(channel, "fifo_timestamp", date);

				switch_channel_set_variable(other_channel, "fifo_status", "DONE");
				switch_channel_set_variable(other_channel, "fifo_timestamp", date);

				send_presence(node);
				check_cancel(node);

				switch_core_session_rwunlock(other_session);


				if (!do_wait || !switch_channel_ready(channel)) {
					break;
				}

				fifo_consumer_wrapup_sound = switch_channel_get_variable(channel, "fifo_consumer_wrapup_sound");
				fifo_consumer_wrapup_key = switch_channel_get_variable(channel, "fifo_consumer_wrapup_key");
				sfifo_consumer_wrapup_time = switch_channel_get_variable(channel, "fifo_consumer_wrapup_time");
				if (!zstr(sfifo_consumer_wrapup_time)) {
					fifo_consumer_wrapup_time = atoi(sfifo_consumer_wrapup_time);
				} else {
					fifo_consumer_wrapup_time = 5000;
				}

				memset(buf, 0, sizeof(buf));

				if (fifo_consumer_wrapup_time || !zstr(fifo_consumer_wrapup_key)) {
					switch_channel_set_variable(channel, "fifo_status", "WRAPUP");
					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
						switch_channel_event_set_data(channel, event);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_wrapup");
						switch_event_fire(&event);
					}
				}

				if (!zstr(fifo_consumer_wrapup_sound)) {
					memset(&args, 0, sizeof(args));
					args.buf = buf;
					args.buflen = sizeof(buf);
					status = switch_ivr_play_file(session, NULL, fifo_consumer_wrapup_sound, &args);
					if (!SWITCH_READ_ACCEPTABLE(status)) {
						break;
					}
				}

				if (fifo_consumer_wrapup_time) {
					wrapup_time_started = switch_micro_time_now();
				}

				if (!zstr(fifo_consumer_wrapup_key) && strcmp(buf, fifo_consumer_wrapup_key)) {
					while (switch_channel_ready(channel)) {
						char terminator = 0;

						if (fifo_consumer_wrapup_time) {
							wrapup_time_elapsed = (switch_micro_time_now() - wrapup_time_started) / 1000;
							if (wrapup_time_elapsed > fifo_consumer_wrapup_time) {
								break;
							} else {
								wrapup_time_remaining = fifo_consumer_wrapup_time - wrapup_time_elapsed + 100;
							}
						}

						switch_ivr_collect_digits_count(session, buf, sizeof(buf) - 1, 1, fifo_consumer_wrapup_key, &terminator, 0, 0,
														(uint32_t) wrapup_time_remaining);
						if ((terminator == *fifo_consumer_wrapup_key) || !(switch_channel_ready(channel))) {
							break;
						}

					}
				} else if (fifo_consumer_wrapup_time && (zstr(fifo_consumer_wrapup_key) || !strcmp(buf, fifo_consumer_wrapup_key))) {
					while (switch_channel_ready(channel)) {
						wrapup_time_elapsed = (switch_micro_time_now() - wrapup_time_started) / 1000;
						if (wrapup_time_elapsed > fifo_consumer_wrapup_time) {
							break;
						}
						switch_yield(500);
					}
				}
				switch_channel_set_variable(channel, "fifo_status", "WAITING");
			}

			if (do_wait && switch_channel_ready(channel)) {
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_data(channel, event);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_reentrance");
					switch_event_fire(&event);
				}
			}
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Name", argv[0]);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_stop");
			switch_event_fire(&event);
		}

		if (do_wait) {
			for (i = 0; i < node_count; i++) {
				if (!(node = node_list[i])) {
					continue;
				}
				switch_mutex_lock(node->mutex);
				switch_core_hash_delete(node->consumer_hash, switch_core_session_get_uuid(session));
				node->consumer_count--;
				switch_mutex_unlock(node->mutex);
			}
		}

		if (outbound_id && switch_channel_up(channel)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s is still alive, tracking call.\n", switch_channel_get_name(channel));
			fifo_track_call_function(session, outbound_id);
		}

	}

  done:

	if (!consumer && in_table) {
		fifo_caller_del(switch_core_session_get_uuid(session));
	}

	if (switch_true(switch_channel_get_variable(channel, "fifo_destroy_after_use"))) {
		do_destroy = 1;
	}

	switch_mutex_lock(globals.mutex);
	for (i = 0; i < node_count; i++) {
		if (!(node = node_list[i])) {
			continue;
		}
		switch_thread_rwlock_unlock(node->rwlock);
		
		if (node->ready == 1 && do_destroy && node_caller_count(node) == 0 && node->consumer_count == 0) {
			switch_core_hash_delete(globals.fifo_hash, node->name);
			node->ready = 0;
		}
	}
	switch_mutex_unlock(globals.mutex);

	switch_channel_clear_app_flag_key(FIFO_APP_KEY, channel, FIFO_APP_BRIDGE_TAG);

	switch_core_media_bug_resume(session);
}

struct xml_helper {
	switch_xml_t xml;
	fifo_node_t *node;
	char *container;
	char *tag;
	int cc_off;
	int row_off;
	int verbose;
};

static int xml_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct xml_helper *h = (struct xml_helper *) pArg;
	switch_xml_t x_out;
	int c_off = 0;
	char exp_buf[128] = "";
	switch_time_exp_t tm;
	switch_time_t etime = 0;
	char atime[128] = "";
	char *expires = exp_buf, *tb = atime;
	int arg = 0;

	for(arg = 0; arg < argc; arg++) {
		if (!argv[arg]) {
			argv[arg] = "";
		}
	}

	arg = 0;

	if (argv[7]) {
		if ((etime = atol(argv[7]))) {
			switch_size_t retsize;

			switch_time_exp_lt(&tm, switch_time_from_sec(etime));
			switch_strftime_nocheck(exp_buf, &retsize, sizeof(exp_buf), "%Y-%m-%d %T", &tm);
		} else {
			switch_set_string(exp_buf, "now");
		}
	}


	if (atoi(argv[13])) {
		arg = 17;
	} else {
		arg = 18;
	}


	if ((etime = atol(argv[arg]))) {
		switch_size_t retsize;
		switch_time_exp_lt(&tm, switch_time_from_sec(etime));
		switch_strftime_nocheck(atime, &retsize, sizeof(atime), "%Y-%m-%d %T", &tm);
	} else {
		switch_set_string(atime, "now");
	}


	x_out = switch_xml_add_child_d(h->xml, h->tag, c_off++);
	switch_xml_set_attr_d(x_out, "simo", argv[3]);
	switch_xml_set_attr_d(x_out, "use_count", argv[4]);
	switch_xml_set_attr_d(x_out, "timeout", argv[5]);
	switch_xml_set_attr_d(x_out, "lag", argv[6]);
	switch_xml_set_attr_d(x_out, "outbound-call-count", argv[10]);
	switch_xml_set_attr_d(x_out, "outbound-fail-count", argv[11]);
	switch_xml_set_attr_d(x_out, "taking-calls", argv[13]);
	switch_xml_set_attr_d(x_out, "status", argv[14]);

	switch_xml_set_attr_d(x_out, "outbound-call-total-count", argv[15]);
	switch_xml_set_attr_d(x_out, "outbound-fail-total-count", argv[16]);

	if (arg == 17) {
		switch_xml_set_attr_d(x_out, "logged-on-since", tb);
	} else {
		switch_xml_set_attr_d(x_out, "logged-off-since", tb);
	}

	switch_xml_set_attr_d(x_out, "manual-calls-out-count", argv[19]);
	switch_xml_set_attr_d(x_out, "manual-calls-in-count", argv[20]);
	switch_xml_set_attr_d(x_out, "manual-calls-out-total-count", argv[21]);
	switch_xml_set_attr_d(x_out, "manual-calls-in-total-count", argv[22]);

	if (argc > 23) {
		switch_xml_set_attr_d(x_out, "ring-count", argv[23]);

		if ((etime = atol(argv[24]))) {
			switch_size_t retsize;
			switch_time_exp_lt(&tm, switch_time_from_sec(etime));
			switch_strftime_nocheck(atime, &retsize, sizeof(atime), "%Y-%m-%d %T", &tm);
		} else {
			switch_set_string(atime, "never");
		}

		switch_xml_set_attr_d(x_out, "start-time", tb);

		if ((etime = atol(argv[25]))) {
			switch_size_t retsize;
			switch_time_exp_lt(&tm, switch_time_from_sec(etime));
			switch_strftime_nocheck(atime, &retsize, sizeof(atime), "%Y-%m-%d %T", &tm);
		} else {
			switch_set_string(atime, "never");
		}

		switch_xml_set_attr_d(x_out, "stop-time", tb);
	}


	switch_xml_set_attr_d(x_out, "next-available", expires);

	switch_xml_set_txt_d(x_out, argv[2]);

	return 0;
}

static int xml_outbound(switch_xml_t xml, fifo_node_t *node, char *container, char *tag, int cc_off, int verbose)
{
	struct xml_helper h = { 0 };
	char *sql;

	if (!strcmp(node->name, MANUAL_QUEUE_NAME)) {

		sql = switch_mprintf("select uuid, '%s', originate_string, simo_count, use_count, timeout,"
							 "lag, next_avail, expires, static, outbound_call_count, outbound_fail_count,"
							 "hostname, taking_calls, status, outbound_call_total_count, outbound_fail_total_count, active_time, inactive_time,"
							 "manual_calls_out_count, manual_calls_in_count, manual_calls_out_total_count, manual_calls_in_total_count from fifo_outbound "
							 "group by "
							 "uuid, originate_string, simo_count, use_count, timeout,"
							 "lag, next_avail, expires, static, outbound_call_count, outbound_fail_count,"
							 "hostname, taking_calls, status, outbound_call_total_count, outbound_fail_total_count, active_time, inactive_time,"
							 "manual_calls_out_count, manual_calls_in_count, manual_calls_out_total_count, manual_calls_in_total_count",
							 MANUAL_QUEUE_NAME);


	} else {
		sql = switch_mprintf("select uuid, fifo_name, originate_string, simo_count, use_count, timeout, "
							 "lag, next_avail, expires, static, outbound_call_count, outbound_fail_count, "
							 "hostname, taking_calls, status, outbound_call_total_count, outbound_fail_total_count, active_time, inactive_time, "
							 "manual_calls_out_count, manual_calls_in_count, manual_calls_out_total_count, manual_calls_in_total_count,"
							 "ring_count,start_time,stop_time "
							 "from fifo_outbound where fifo_name = '%q'", node->name);
	}

	h.xml = xml;
	h.node = node;
	h.container = container;
	h.tag = tag;
	h.cc_off = cc_off;
	h.row_off = 0;
	h.verbose = verbose;

	h.xml = switch_xml_add_child_d(h.xml, h.container, h.cc_off++);

	fifo_execute_sql_callback(globals.sql_mutex, sql, xml_callback, &h);

	switch_safe_free(sql);

	return h.cc_off;
}


static int xml_bridge_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct xml_helper *h = (struct xml_helper *) pArg;
	switch_xml_t x_bridge, x_var, x_caller, x_consumer, x_cdr;
	char exp_buf[128] = "";
	switch_time_exp_t tm;
	switch_time_t etime = 0;
	int off = 0, tag_off = 0;
	switch_core_session_t *session;
	char url_buf[512] = "";
	char *encoded;

	if ((etime = atol(argv[6]))) {
		switch_size_t retsize;

		switch_time_exp_lt(&tm, switch_time_from_sec(etime));
		switch_strftime_nocheck(exp_buf, &retsize, sizeof(exp_buf), "%Y-%m-%d %T", &tm);
	} else {
		switch_set_string(exp_buf, "now");
	}


	x_bridge = switch_xml_add_child_d(h->xml, h->tag, h->row_off++);

	switch_xml_set_attr_d(x_bridge, "fifo_name", argv[0]);
	switch_xml_set_attr_d_buf(x_bridge, "bridge_start", exp_buf);
	switch_xml_set_attr_d(x_bridge, "bridge_start_epoch", argv[6]);

	x_caller = switch_xml_add_child_d(x_bridge, "caller", tag_off++);

	switch_xml_set_attr_d(x_caller, "uuid", argv[1]);

	encoded = switch_url_encode(argv[2], url_buf, sizeof(url_buf));
	switch_xml_set_attr_d(x_caller, "caller_id_name", encoded);

	encoded = switch_url_encode(argv[3], url_buf, sizeof(url_buf));
	switch_xml_set_attr_d(x_caller, "caller_id_number", encoded);



	if (h->verbose) {
		if ((session = switch_core_session_locate(argv[1]))) {
			x_cdr = switch_xml_add_child_d(x_caller, "cdr", 0);
			switch_ivr_generate_xml_cdr(session, &x_cdr);
			switch_core_session_rwunlock(session);
		}
	}

	off = 0;

	x_consumer = switch_xml_add_child_d(x_bridge, "consumer", tag_off++);

	x_var = switch_xml_add_child_d(x_consumer, "uuid", off++);
	switch_xml_set_txt_d(x_var, argv[4]);
	x_var = switch_xml_add_child_d(x_consumer, "outgoing_uuid", off++);
	switch_xml_set_txt_d(x_var, argv[5]);

	if (h->verbose) {
		if ((session = switch_core_session_locate(argv[1]))) {
			x_cdr = switch_xml_add_child_d(x_consumer, "cdr", 0);
			switch_ivr_generate_xml_cdr(session, &x_cdr);
			switch_core_session_rwunlock(session);
		}
	}

	return 0;
}

static int xml_bridges(switch_xml_t xml, fifo_node_t *node, char *container, char *tag, int cc_off, int verbose)
{
	struct xml_helper h = { 0 };
	char *sql = switch_mprintf("select "
							   "fifo_name,caller_uuid,caller_caller_id_name,caller_caller_id_number,consumer_uuid,consumer_outgoing_uuid,bridge_start "
							   "from fifo_bridge where fifo_name = '%q'", node->name);

	h.xml = xml;
	h.node = node;
	h.container = container;
	h.tag = tag;
	h.cc_off = cc_off;
	h.row_off = 0;
	h.verbose = verbose;

	h.xml = switch_xml_add_child_d(h.xml, h.container, h.cc_off++);

	fifo_execute_sql_callback(globals.sql_mutex, sql, xml_bridge_callback, &h);

	switch_safe_free(sql);

	return h.cc_off;
}

static int xml_hash(switch_xml_t xml, switch_hash_t *hash, char *container, char *tag, int cc_off, int verbose)
{
	switch_xml_t x_tmp, x_caller, x_cp;
	switch_hash_index_t *hi;
	switch_core_session_t *session;
	switch_channel_t *channel;
	void *val;
	const void *var;

	x_tmp = switch_xml_add_child_d(xml, container, cc_off++);
	switch_assert(x_tmp);

	for (hi = switch_core_hash_first(hash); hi; hi = switch_core_hash_next(&hi)) {
		int c_off = 0, d_off = 0;
		const char *status;
		const char *ts;
		char url_buf[512] = "";
		char *encoded;

		switch_core_hash_this(hi, &var, NULL, &val);
		session = (switch_core_session_t *) val;
		channel = switch_core_session_get_channel(session);
		x_caller = switch_xml_add_child_d(x_tmp, tag, c_off++);
		switch_assert(x_caller);

		switch_xml_set_attr_d(x_caller, "uuid", switch_core_session_get_uuid(session));

		if ((status = switch_channel_get_variable(channel, "fifo_status"))) {
			switch_xml_set_attr_d(x_caller, "status", status);
		}

		if ((status = switch_channel_get_variable(channel, "caller_id_name"))) {
			encoded = switch_url_encode(status, url_buf, sizeof(url_buf));
			switch_xml_set_attr_d(x_caller, "caller_id_name", encoded);
		}

		if ((status = switch_channel_get_variable(channel, "caller_id_number"))) {
			encoded = switch_url_encode(status, url_buf, sizeof(url_buf));
			switch_xml_set_attr_d(x_caller, "caller_id_number", encoded);
		}

		if ((ts = switch_channel_get_variable(channel, "fifo_timestamp"))) {
			switch_xml_set_attr_d(x_caller, "timestamp", ts);
		}

		if ((ts = switch_channel_get_variable(channel, "fifo_target"))) {
			switch_xml_set_attr_d(x_caller, "target", ts);
		}

		if (verbose) {
			if (!(x_cp = switch_xml_add_child_d(x_caller, "cdr", d_off++))) {
				abort();
			}

			switch_ivr_generate_xml_cdr(session, &x_cp);
		}
	}

	return cc_off;
}


static int xml_caller(switch_xml_t xml, fifo_node_t *node, char *container, char *tag, int cc_off, int verbose)
{
	switch_xml_t x_tmp, x_caller, x_cp;
	int i, x;
	switch_core_session_t *session;
	switch_channel_t *channel;

	x_tmp = switch_xml_add_child_d(xml, container, cc_off++);
	switch_assert(x_tmp);

	for (x = 0; x < MAX_PRI; x++) {
		fifo_queue_t *q = node->fifo_list[x];

		switch_mutex_lock(q->mutex);

		for (i = 0; i < q->idx; i++) {

			int c_off = 0, d_off = 0;
			const char *status;
			const char *ts;
			const char *uuid = switch_event_get_header(q->data[i], "unique-id");
			char sl[30] = "";
			char url_buf[512] = "";
			char *encoded;

			if (!uuid) {
				continue;
			}

			if (!(session = switch_core_session_locate(uuid))) {
				continue;
			}

			channel = switch_core_session_get_channel(session);
			x_caller = switch_xml_add_child_d(x_tmp, tag, c_off++);
			switch_assert(x_caller);

			switch_xml_set_attr_d(x_caller, "uuid", switch_core_session_get_uuid(session));

			if ((status = switch_channel_get_variable(channel, "fifo_status"))) {
				switch_xml_set_attr_d(x_caller, "status", status);
			}

			if ((status = switch_channel_get_variable(channel, "caller_id_name"))) {
				encoded = switch_url_encode(status, url_buf, sizeof(url_buf));
				switch_xml_set_attr_d(x_caller, "caller_id_name", encoded);
			}

			if ((status = switch_channel_get_variable(channel, "caller_id_number"))) {
				encoded = switch_url_encode(status, url_buf, sizeof(url_buf));
				switch_xml_set_attr_d(x_caller, "caller_id_number", encoded);
			}

			if ((ts = switch_channel_get_variable(channel, "fifo_timestamp"))) {
				switch_xml_set_attr_d(x_caller, "timestamp", ts);
			}

			if ((ts = switch_channel_get_variable(channel, "fifo_target"))) {
				switch_xml_set_attr_d(x_caller, "target", ts);
			}

			if ((ts = switch_channel_get_variable(channel, "fifo_position"))) {
				switch_xml_set_attr_d(x_caller, "position", ts);
			}

			switch_snprintf(sl, sizeof(sl), "%d", x);
			switch_xml_set_attr_d_buf(x_caller, "slot", sl);


			if (verbose) {
				if (!(x_cp = switch_xml_add_child_d(x_caller, "cdr", d_off++))) {
					abort();
				}

				switch_ivr_generate_xml_cdr(session, &x_cp);
			}

			switch_core_session_rwunlock(session);
			session = NULL;
		}

		switch_mutex_unlock(q->mutex);
	}

	return cc_off;
}

static void list_node(fifo_node_t *node, switch_xml_t x_report, int *off, int verbose)
{
	switch_xml_t x_fifo;
	int cc_off = 0;
	char buffer[35];
	char *tmp = buffer;

	x_fifo = switch_xml_add_child_d(x_report, "fifo", (*off)++);;
	switch_assert(x_fifo);

	switch_xml_set_attr_d(x_fifo, "name", node->name);
	switch_snprintf(tmp, sizeof(buffer), "%d", node->consumer_count);
	switch_xml_set_attr_d(x_fifo, "consumer_count", tmp);
	switch_snprintf(tmp, sizeof(buffer), "%d", node_caller_count(node));
	switch_xml_set_attr_d(x_fifo, "caller_count", tmp);
	switch_snprintf(tmp, sizeof(buffer), "%d", node_caller_count(node));
	switch_xml_set_attr_d(x_fifo, "waiting_count", tmp);
	switch_snprintf(tmp, sizeof(buffer), "%u", node->importance);
	switch_xml_set_attr_d(x_fifo, "importance", tmp);

	switch_snprintf(tmp, sizeof(buffer), "%u", node->outbound_per_cycle);
	switch_xml_set_attr_d(x_fifo, "outbound_per_cycle", tmp);

	switch_snprintf(tmp, sizeof(buffer), "%u", node->ring_timeout);
	switch_xml_set_attr_d(x_fifo, "ring_timeout", tmp);

	switch_snprintf(tmp, sizeof(buffer), "%u", node->default_lag);
	switch_xml_set_attr_d(x_fifo, "default_lag", tmp);

	switch_snprintf(tmp, sizeof(buffer), "%u", node->outbound_priority);
	switch_xml_set_attr_d(x_fifo, "outbound_priority", tmp);

	switch_xml_set_attr_d(x_fifo, "outbound_strategy", strat_parse(node->outbound_strategy));

	cc_off = xml_outbound(x_fifo, node, "outbound", "member", cc_off, verbose);
	cc_off = xml_caller(x_fifo, node, "callers", "caller", cc_off, verbose);
	cc_off = xml_hash(x_fifo, node->consumer_hash, "consumers", "consumer", cc_off, verbose);
	cc_off = xml_bridges(x_fifo, node, "bridges", "bridge", cc_off, verbose);
}


void dump_hash(switch_hash_t *hash, switch_stream_handle_t *stream)
{
	switch_hash_index_t *hi;
	void *val;
	const void *var;

	switch_mutex_lock(globals.mutex);
	for (hi = switch_core_hash_first(hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, &var, NULL, &val);
		stream->write_function(stream, "  %s\n", (char *)var);
	}
	switch_mutex_unlock(globals.mutex);
}

void node_dump(switch_stream_handle_t *stream)
{


	switch_hash_index_t *hi;
	fifo_node_t *node;
	void *val;
	switch_mutex_lock(globals.mutex);
	for (hi = switch_core_hash_first(globals.fifo_hash); hi; hi = switch_core_hash_next(&hi)) {
		switch_core_hash_this(hi, NULL, NULL, &val);
		if ((node = (fifo_node_t *) val)) {
			stream->write_function(stream, "node: %s\n"
								   " outbound_name: %s\n"
								   " outbound_per_cycle: %d"
								   " outbound_priority: %d"
								   " outbound_strategy: %s\n"
								   " has_outbound: %d\n"
								   " outbound_priority: %d\n"
								   " busy: %d\n"
								   " ready: %d\n"
								   " waiting: %d\n"
								   ,
								   node->name, node->outbound_name, node->outbound_per_cycle,
								   node->outbound_priority, strat_parse(node->outbound_strategy),
								   node->has_outbound,
								   node->outbound_priority,
								   node->busy,
								   node->ready,
								   node_caller_count(node)

								   );
		}
	}

	stream->write_function(stream, " caller_orig:\n");
	dump_hash(globals.caller_orig_hash, stream);
	stream->write_function(stream, " consumer_orig:\n");
	dump_hash(globals.consumer_orig_hash, stream);
	stream->write_function(stream, " bridge:\n");
	dump_hash(globals.bridge_hash, stream);

	switch_mutex_unlock(globals.mutex);


}



#define FIFO_API_SYNTAX "list|list_verbose|count|debug|status|importance [<fifo name>]|reparse [del_all]"
SWITCH_STANDARD_API(fifo_api_function)
{
	fifo_node_t *node;
	char *data = NULL;
	int argc = 0;
	char *argv[5] = { 0 };
	switch_hash_index_t *hi;
	void *val;
	const void *var;
	int x = 0, verbose = 0;

	if (!globals.running) {
		return SWITCH_STATUS_FALSE;
	}

	if (!zstr(cmd)) {
		data = strdup(cmd);
		switch_assert(data);
	}


	switch_mutex_lock(globals.mutex);

	if (zstr(cmd) || (argc = switch_separate_string(data, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1 || !argv[0]) {
		stream->write_function(stream, "%s\n", FIFO_API_SYNTAX);
		goto done;
	}

	if (!strcasecmp(argv[0], "status")) {
		node_dump(stream);
		goto done;
	}

	if (!strcasecmp(argv[0], "debug")) {
		if (argv[1]) {
			if ((globals.debug = atoi(argv[1])) < 0) {
				globals.debug = 0;
			}
		}
		stream->write_function(stream, "debug %d\n", globals.debug);
		goto done;
	}

	verbose = !strcasecmp(argv[0], "list_verbose");

	if (!strcasecmp(argv[0], "reparse")) {
		load_config(1, argv[1] && !strcasecmp(argv[1], "del_all"));
		stream->write_function(stream, "+OK\n");
		goto done;
	}

	if (!strcasecmp(argv[0], "list") || verbose) {
		char *xml_text = NULL;
		switch_xml_t x_report = switch_xml_new("fifo_report");
		switch_assert(x_report);

		if (argc < 2) {
			for (hi = switch_core_hash_first(globals.fifo_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &var, NULL, &val);
				node = (fifo_node_t *) val;

				switch_mutex_lock(node->mutex);
				list_node(node, x_report, &x, verbose);
				switch_mutex_unlock(node->mutex);
			}
		} else {
			if ((node = switch_core_hash_find(globals.fifo_hash, argv[1]))) {
				switch_mutex_lock(node->mutex);
				list_node(node, x_report, &x, verbose);
				switch_mutex_unlock(node->mutex);
			}
		}
		xml_text = switch_xml_toxml(x_report, SWITCH_FALSE);
		switch_assert(xml_text);
		stream->write_function(stream, "%s\n", xml_text);
		switch_xml_free(x_report);
		switch_safe_free(xml_text);

	} else if (!strcasecmp(argv[0], "importance")) {
		if (argv[1] && (node = switch_core_hash_find(globals.fifo_hash, argv[1]))) {
			int importance = 0;
			if (argc > 2) {
				importance = atoi(argv[2]);
				if (importance < 0) {
					importance = 0;
				}
				node->importance = importance;
			}
			stream->write_function(stream, "importance: %u\n", node->importance);
		} else {
			stream->write_function(stream, "no fifo by that name\n");
		}
	} else if (!strcasecmp(argv[0], "count")) {
		if (argc < 2) {
			for (hi = switch_core_hash_first(globals.fifo_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &var, NULL, &val);
				node = (fifo_node_t *) val;
				switch_mutex_lock(node->update_mutex);
				stream->write_function(stream, "%s:%d:%d:%d:%d:%d\n", (char *) var, node->consumer_count, node_caller_count(node), node->member_count, node->ring_consumer_count, node_idle_consumers(node));
				switch_mutex_unlock(node->update_mutex);
				x++;
			}

			if (!x) {
				stream->write_function(stream, "none\n");
			}
		} else if ((node = switch_core_hash_find(globals.fifo_hash, argv[1]))) {
			switch_mutex_lock(node->update_mutex);
			stream->write_function(stream, "%s:%d:%d:%d:%d:%d\n", argv[1], node->consumer_count, node_caller_count(node), node->member_count, node->ring_consumer_count, node_idle_consumers(node));
			switch_mutex_unlock(node->update_mutex);
		} else {
			stream->write_function(stream, "none\n");
		}
	} else if (!strcasecmp(argv[0], "has_outbound")) {
		if (argc < 2) {
			for (hi = switch_core_hash_first(globals.fifo_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, &var, NULL, &val);
				node = (fifo_node_t *) val;
				switch_mutex_lock(node->update_mutex);
				stream->write_function(stream, "%s:%d\n", (char *) var, node->has_outbound);
				switch_mutex_unlock(node->update_mutex);
				x++;
			}

			if (!x) {
				stream->write_function(stream, "none\n");
			}
		} else if ((node = switch_core_hash_find(globals.fifo_hash, argv[1]))) {
			switch_mutex_lock(node->update_mutex);
			stream->write_function(stream, "%s:%d\n", argv[1], node->has_outbound);
			switch_mutex_unlock(node->update_mutex);
		} else {
			stream->write_function(stream, "none\n");
		}
	} else {
		stream->write_function(stream, "-ERR Usage: %s\n", FIFO_API_SYNTAX);
	}

  done:

	switch_safe_free(data);

	switch_mutex_unlock(globals.mutex);
	return SWITCH_STATUS_SUCCESS;
}


const char outbound_sql[] =
	"create table fifo_outbound (\n"
	" uuid varchar(255),\n"
	" fifo_name varchar(255),\n"
	" originate_string varchar(255),\n"
	" simo_count integer,\n"
	" use_count integer,\n"
	" timeout integer,\n"
	" lag integer,\n"
	" next_avail integer not null default 0,\n"
	" expires integer not null default 0,\n"
	" static integer not null default 0,\n"
	" outbound_call_count integer not null default 0,\n"
	" outbound_fail_count integer not null default 0,\n"
	" hostname varchar(255),\n"
	" taking_calls integer not null default 1,\n"
	" status varchar(255),\n"
	" outbound_call_total_count integer not null default 0,\n"
	" outbound_fail_total_count integer not null default 0,\n"
	" active_time integer not null default 0,\n"
	" inactive_time integer not null default 0,\n"
	" manual_calls_out_count integer not null default 0,\n"
	" manual_calls_in_count integer not null default 0,\n"
	" manual_calls_out_total_count integer not null default 0,\n"
	" manual_calls_in_total_count integer not null default 0,\n"
	" ring_count integer not null default 0,\n"
	" start_time integer not null default 0,\n"
	" stop_time integer not null default 0\n"
	");\n";


const char bridge_sql[] =
	"create table fifo_bridge (\n"
	" fifo_name varchar(1024) not null,\n"
	" caller_uuid varchar(255) not null,\n"
	" caller_caller_id_name varchar(255),\n"
	" caller_caller_id_number varchar(255),\n"

	" consumer_uuid varchar(255) not null,\n"
	" consumer_outgoing_uuid varchar(255),\n"
	" bridge_start integer\n"
	");\n"
;

const char callers_sql[] =
	"create table fifo_callers (\n"
	" fifo_name varchar(255) not null,\n"
	" uuid varchar(255) not null,\n"
	" caller_caller_id_name varchar(255),\n"
	" caller_caller_id_number varchar(255),\n"
	" timestamp integer\n"
	");\n"
;



static void extract_fifo_outbound_uuid(char *string, char *uuid, switch_size_t len)
{
	switch_event_t *ovars;
	char *parsed = NULL;
	const char *fifo_outbound_uuid;

	switch_event_create(&ovars, SWITCH_EVENT_REQUEST_PARAMS);

	switch_event_create_brackets(string, '{', '}', ',', &ovars, &parsed, SWITCH_TRUE);

	if ((fifo_outbound_uuid = switch_event_get_header(ovars, "fifo_outbound_uuid"))) {
		switch_snprintf(uuid, len, "%s", fifo_outbound_uuid);
	}

	switch_safe_free(parsed);
	switch_event_destroy(&ovars);
}

/*!
 * Load or reload the configuration
 *
 * On the initial load, non-static members are preserved unless the
 * parameter `delete-all-outbound-members-on-startup` is set.  The
 * parameter `del_all` is ignored in this case.
 *
 * On reload, non-static members are preserved unless `del_all` is
 * set.
 *
 * \param reload true if we're reloading the config
 * \param del_all delete all outbound members when reloading;
 *   not used unless reload is true
 */
static switch_status_t load_config(int reload, int del_all)
{
	char *cf = "fifo.conf";
	switch_xml_t cfg, xml, fifo, fifos, member, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *sql;
	switch_bool_t delete_all_outbound_member_on_startup = SWITCH_FALSE;
	switch_cache_db_handle_t *dbh = NULL;
	fifo_node_t *node;

	strncpy(globals.hostname, switch_core_get_switchname(), sizeof(globals.hostname) - 1);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	globals.dbname = "fifo";
	
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = NULL;
			char *val = NULL;

			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "outbound-strategy") && !zstr(val)) {
				default_strategy = parse_strat(val);
			}

			if (!strcasecmp(var, "odbc-dsn") && !zstr(val)) {
				if (switch_odbc_available() || switch_pgsql_available()) {
					switch_set_string(globals.odbc_dsn, val);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
				}
			} else if (!strcasecmp(var, "dbname") && !zstr(val)) {
				globals.dbname = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "allow-transcoding") && !zstr(val)) {
				globals.allow_transcoding = switch_true(val);
			} else if (!strcasecmp(var, "db-pre-trans-execute") && !zstr(val)) {
				globals.pre_trans_execute = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "db-post-trans-execute") && !zstr(val)) {
				globals.post_trans_execute = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "db-inner-pre-trans-execute") && !zstr(val)) {
				globals.inner_pre_trans_execute = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "db-inner-post-trans-execute") && !zstr(val)) {
				globals.inner_post_trans_execute = switch_core_strdup(globals.pool, val);
			} else if (!strcasecmp(var, "delete-all-outbound-member-on-startup")) {
				delete_all_outbound_member_on_startup = switch_true(val);
			}
		}
	}


	if (!(dbh = fifo_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot open DB!\n");
		goto done;
	}

	if (!reload) {
		switch_sql_queue_manager_init_name("fifo",
										   &globals.qm,
										   2,
										   !zstr(globals.odbc_dsn) ? globals.odbc_dsn : globals.dbname,
										   SWITCH_MAX_TRANS,
										   globals.pre_trans_execute,
										   globals.post_trans_execute,
										   globals.inner_pre_trans_execute,
										   globals.inner_post_trans_execute);
		
		switch_sql_queue_manager_start(globals.qm);



		switch_cache_db_test_reactive(dbh, "delete from fifo_outbound where static = 1 or taking_calls < 0 or stop_time < 0",
									  "drop table fifo_outbound", outbound_sql);
		switch_cache_db_test_reactive(dbh, "delete from fifo_bridge", "drop table fifo_bridge", bridge_sql);
		switch_cache_db_test_reactive(dbh, "delete from fifo_callers", "drop table fifo_callers", callers_sql);
	}

	switch_cache_db_release_db_handle(&dbh);

	if (!reload) {
		char *sql= "update fifo_outbound set start_time=0,stop_time=0,ring_count=0,use_count=0,outbound_call_count=0,outbound_fail_count=0 where static=0";
		fifo_execute_sql_queued(&sql, SWITCH_FALSE, SWITCH_TRUE);
		fifo_init_use_count();
	}

	if (reload) {
		switch_hash_index_t *hi;
		fifo_node_t *node;
		void *val;
		switch_mutex_lock(globals.mutex);
		for (hi = switch_core_hash_first(globals.fifo_hash); hi; hi = switch_core_hash_next(&hi)) {
			switch_core_hash_this(hi, NULL, NULL, &val);
			if ((node = (fifo_node_t *) val) && node->is_static && node->ready == 1) {
				node->ready = -1;
			}
		}
		switch_mutex_unlock(globals.mutex);
	}

	if ((reload && del_all) || (!reload && delete_all_outbound_member_on_startup)) {
		sql = switch_mprintf("delete from fifo_outbound where hostname='%q'", globals.hostname);
	} else {
		sql = switch_mprintf("delete from fifo_outbound where static=1 and hostname='%q'", globals.hostname);
	}

	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

	if (!(node = switch_core_hash_find(globals.fifo_hash, MANUAL_QUEUE_NAME))) {
		node = create_node(MANUAL_QUEUE_NAME, 0, globals.sql_mutex);
		node->ready = 2;
		node->is_static = 0;
	}


	if ((fifos = switch_xml_child(cfg, "fifos"))) {
		for (fifo = switch_xml_child(fifos, "fifo"); fifo; fifo = fifo->next) {
			const char *name, *outbound_strategy;
			const char *val;
			int imp = 0, outbound_per_cycle = 1, outbound_priority = 5;
			int simo_i = 1;
			int taking_calls_i = 1;
			int timeout_i = 60;
			int lag_i = 10;
			int ring_timeout = 60;
			int default_lag = 30;

			name = switch_xml_attr(fifo, "name");

			if (!name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "fifo has no name!\n");
				continue;
			}

			if (!strcasecmp(name, MANUAL_QUEUE_NAME)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s is a reserved name, use another name please.\n", MANUAL_QUEUE_NAME);
				continue;
			}

			if ((val = switch_xml_attr(fifo, "importance"))) {
				if ((imp = atoi(val)) < 0) {
					imp = 0;
				}
			}

			switch_mutex_lock(globals.mutex);
			if (!(node = switch_core_hash_find(globals.fifo_hash, name))) {
				node = create_node(name, imp, globals.sql_mutex);
			}

			if ((val = switch_xml_attr(fifo, "outbound_name"))) {
				node->outbound_name = switch_core_strdup(node->pool, val);
			}

			switch_mutex_unlock(globals.mutex);

			switch_assert(node);

			switch_mutex_lock(node->mutex);

			outbound_strategy = switch_xml_attr(fifo, "outbound_strategy");


			if ((val = switch_xml_attr(fifo, "outbound_per_cycle"))) {
				if ((outbound_per_cycle = atoi(val)) < 0) {
					outbound_per_cycle = 1;
				}
				node->has_outbound = 1;
			}

			if ((val = switch_xml_attr(fifo, "retry_delay"))) {
				int tmp;

				if ((tmp = atoi(val)) < 0) {
					tmp = 0;
				}

				node->retry_delay = tmp;
			}

			if ((val = switch_xml_attr(fifo, "outbound_priority"))) {
				outbound_priority = atoi(val);

				if (outbound_priority < 1 || outbound_priority > 10) {
					outbound_priority = 5;
				}
				node->has_outbound = 1;
			}

			if ((val = switch_xml_attr(fifo, "outbound_ring_timeout"))) {
				int tmp = atoi(val);
				if (tmp > 10) {
					ring_timeout = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Invalid ring_timeout: must be > 10 for queue %s\n", node->name);
				}
			}

			if ((val = switch_xml_attr(fifo, "outbound_default_lag"))) {
				int tmp = atoi(val);
				if (tmp > 10) {
					default_lag = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Invalid default_lag: must be > 10 for queue %s\n", node->name);
				}
			}

			node->ring_timeout = ring_timeout;
			node->outbound_per_cycle = outbound_per_cycle;
			node->outbound_priority = outbound_priority;
			node->default_lag = default_lag;

			if (outbound_strategy) {
				node->outbound_strategy = parse_strat(outbound_strategy);
				node->has_outbound = 1;
			}

			for (member = switch_xml_child(fifo, "member"); member; member = member->next) {
				const char *simo = switch_xml_attr_soft(member, "simo");
				const char *lag = switch_xml_attr_soft(member, "lag");
				const char *timeout = switch_xml_attr_soft(member, "timeout");
				const char *taking_calls = switch_xml_attr_soft(member, "taking_calls");
				char *name_dup, *p;
				char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };

				if (switch_stristr("fifo_outbound_uuid=", member->txt)) {
					extract_fifo_outbound_uuid(member->txt, digest, sizeof(digest));
				} else {
					switch_md5_string(digest, (void *) member->txt, strlen(member->txt));
				}

				if (simo) {
					simo_i = atoi(simo);
				}

				if (taking_calls) {
					if ((taking_calls_i = atoi(taking_calls)) < 1) {
						taking_calls_i = 1;
					}
				}

				if (timeout) {
					if ((timeout_i = atoi(timeout)) < 10) {
						timeout_i = ring_timeout;
					}

				}

				if (lag) {
					if ((lag_i = atoi(lag)) < 0) {
						lag_i = default_lag;
					}
				}

				name_dup = strdup(node->name);
				if ((p = strchr(name_dup, '@'))) {
					*p = '\0';
				}


				sql = switch_mprintf("insert into fifo_outbound "
									 "(uuid, fifo_name, originate_string, simo_count, use_count, timeout, lag, "
									 "next_avail, expires, static, outbound_call_count, outbound_fail_count, hostname, taking_calls, "
									 "active_time, inactive_time) "
									 "values ('%q','%q','%q',%d,%d,%d,%d,0,0,1,0,0,'%q',%d,%ld,0)",
									 digest, node->name, member->txt, simo_i, 0, timeout_i, lag_i, globals.hostname, taking_calls_i,
									 (long) switch_epoch_time_now(NULL));

				switch_assert(sql);
				fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_FALSE);
				free(name_dup);
				node->has_outbound = 1;
				node->member_count++;
			}
			node->ready = 1;
			node->is_static = 1;
			switch_mutex_unlock(node->mutex);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s configured\n", node->name);

		}
	}
	switch_xml_free(xml);

  done:

	if (reload) {
		fifo_node_t *node;

		switch_mutex_lock(globals.mutex);
		for (node = globals.nodes; node; node = node->next) {
			if (node->ready == -1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s queued for removal\n", node->name);
				switch_core_hash_delete(globals.fifo_hash, node->name);
				node->ready = 0;
			}
		}
		switch_mutex_unlock(globals.mutex);
	}



	return status;
}


static void fifo_member_add(char *fifo_name, char *originate_string, int simo_count, int timeout, int lag, time_t expires, int taking_calls)
{
	char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };
	char *sql, *name_dup, *p;
    char outbound_count[80] = "";
    callback_t cbt = { 0 };
	fifo_node_t *node = NULL;

	if (!fifo_name) return;

	if (switch_stristr("fifo_outbound_uuid=", originate_string)) {
		extract_fifo_outbound_uuid(originate_string, digest, sizeof(digest));
	} else {
		switch_md5_string(digest, (void *) originate_string, strlen(originate_string));
	}

	sql = switch_mprintf("delete from fifo_outbound where fifo_name='%q' and uuid = '%q'", fifo_name, digest);
	switch_assert(sql);
	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);


	switch_mutex_lock(globals.mutex);
	if (!(node = switch_core_hash_find(globals.fifo_hash, fifo_name))) {
		node = create_node(fifo_name, 0, globals.sql_mutex);
		node->ready = 1;
	}
	switch_mutex_unlock(globals.mutex);

	name_dup = strdup(fifo_name);
	if ((p = strchr(name_dup, '@'))) {
		*p = '\0';
	}

	sql = switch_mprintf("insert into fifo_outbound "
						 "(uuid, fifo_name, originate_string, simo_count, use_count, timeout, "
						 "lag, next_avail, expires, static, outbound_call_count, outbound_fail_count, hostname, taking_calls, active_time, inactive_time) "
						 "values ('%q','%q','%q',%d,%d,%d,%d,%d,%ld,0,0,0,'%q',%d,%ld,0)",
						 digest, fifo_name, originate_string, simo_count, 0, timeout, lag, 0, (long) expires, globals.hostname, taking_calls,
						 (long)switch_epoch_time_now(NULL));
	switch_assert(sql);
	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);
	free(name_dup);

    cbt.buf = outbound_count; 
    cbt.len = sizeof(outbound_count);
    sql = switch_mprintf("select count(*) from fifo_outbound where fifo_name = '%q'", fifo_name);
    fifo_execute_sql_callback(globals.sql_mutex, sql, sql2str_callback, &cbt);
    node->member_count = atoi(outbound_count);
    if (node->member_count > 0) {
        node->has_outbound = 1;
    } else {
        node->has_outbound = 0;
    }
    switch_safe_free(sql);
}

static void fifo_member_del(char *fifo_name, char *originate_string)
{
	char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };
	char *sql;
	char outbound_count[80] = "";
	callback_t cbt = { 0 };
	fifo_node_t *node = NULL;

	if (!fifo_name) return;


	if (switch_stristr("fifo_outbound_uuid=", originate_string)) {
		extract_fifo_outbound_uuid(originate_string, digest, sizeof(digest));
	} else {
		switch_md5_string(digest, (void *) originate_string, strlen(originate_string));
	}

	sql = switch_mprintf("delete from fifo_outbound where fifo_name='%q' and uuid = '%q' and hostname='%q'", fifo_name, digest, globals.hostname);
	switch_assert(sql);
	fifo_execute_sql_queued(&sql, SWITCH_TRUE, SWITCH_TRUE);

	switch_mutex_lock(globals.mutex);
	if (!(node = switch_core_hash_find(globals.fifo_hash, fifo_name))) {
		node = create_node(fifo_name, 0, globals.sql_mutex);
		node->ready = 1;
	}
	switch_mutex_unlock(globals.mutex);

	cbt.buf = outbound_count;
	cbt.len = sizeof(outbound_count);
	sql = switch_mprintf("select count(*) from fifo_outbound where fifo_name = '%q'", node->name);
	fifo_execute_sql_callback(globals.sql_mutex, sql, sql2str_callback, &cbt);
    node->member_count = atoi(outbound_count);
	if (node->member_count > 0) {
        node->has_outbound = 1;
	} else {
        node->has_outbound = 0;
	}
	switch_safe_free(sql);
}

#define FIFO_MEMBER_API_SYNTAX "[add <fifo_name> <originate_string> [<simo_count>] [<timeout>] [<lag>] [<expires>] [<taking_calls>] | del <fifo_name> <originate_string>]"
SWITCH_STANDARD_API(fifo_member_api_function)
{
	char *fifo_name;
	char *originate_string;
	int simo_count = 1;
	int timeout = 60;
	int lag = 5;
	int taking_calls = 1;
	char *action;
	char *mydata = NULL, *argv[8] = { 0 };
	int argc;
	time_t expires = 0;

	if (!globals.running) {
		return SWITCH_STATUS_FALSE;
	}

	if (zstr(cmd)) {
		stream->write_function(stream, "-USAGE: %s\n", FIFO_MEMBER_API_SYNTAX);
		return SWITCH_STATUS_SUCCESS;
	}

	mydata = strdup(cmd);
	switch_assert(mydata);

	argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	if (argc < 3) {
		stream->write_function(stream, "%s", "-ERR Invalid!\n");
		goto done;
	}

	action = argv[0];
	fifo_name = argv[1];
	originate_string = argv[2];

	if (action && !strcasecmp(action, "add")) {
		if (argc > 3) {
			simo_count = atoi(argv[3]);
		}
		if (argc > 4) {
			timeout = atoi(argv[4]);
		}
		if (argc > 5) {
			lag = atoi(argv[5]);
		}
		if (argc > 6) {
			expires = switch_epoch_time_now(NULL) + atoi(argv[6]);
		}
		if (argc > 7) {
			taking_calls = atoi(argv[7]);
		}
		if (simo_count < 0) {
			simo_count = 1;
		}
		if (timeout < 0) {
			timeout = 60;
		}
		if (lag < 0) {
			lag = 5;
		}
		if (taking_calls < 1) {
			taking_calls = 1;
		}

		fifo_member_add(fifo_name, originate_string, simo_count, timeout, lag, expires, taking_calls);
		stream->write_function(stream, "%s", "+OK\n");
	} else if (action && !strcasecmp(action, "del")) {
		fifo_member_del(fifo_name, originate_string);
		stream->write_function(stream, "%s", "+OK\n");
	} else {
		stream->write_function(stream, "%s", "-ERR Invalid!\n");
		goto done;
	}

  done:

	free(mydata);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MODULE_LOAD_FUNCTION(mod_fifo_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *commands_api_interface;
	switch_status_t status;

	/* create/register custom event message type */
	if (switch_event_reserve_subclass(FIFO_EVENT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!", FIFO_EVENT);
		return SWITCH_STATUS_TERM;
	}

	/* Subscribe to presence request events */
	if (switch_event_bind_removable(modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY,
									pres_event_handler, NULL, &globals.node) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to presence request events!\n");
		return SWITCH_STATUS_GENERR;
	}

	globals.pool = pool;
	switch_core_hash_init(&globals.fifo_hash);

	switch_core_hash_init(&globals.caller_orig_hash);
	switch_core_hash_init(&globals.consumer_orig_hash);
	switch_core_hash_init(&globals.bridge_hash);
	switch_core_hash_init(&globals.use_hash);
	switch_mutex_init(&globals.caller_orig_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.consumer_orig_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.bridge_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.use_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.sql_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	globals.running = 1;

	if ((status = load_config(0, 1)) != SWITCH_STATUS_SUCCESS) {
		switch_event_unbind(&globals.node);
		switch_event_free_subclass(FIFO_EVENT);
		switch_core_hash_destroy(&globals.fifo_hash);
		return status;
	}


	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "fifo", "Park with FIFO", FIFO_DESC, fifo_function, FIFO_USAGE, SAF_NONE);
	SWITCH_ADD_APP(app_interface, "fifo_track_call", "Count a call as a fifo call in the manual_calls queue",
				   "", fifo_track_call_function, "<fifo_outbound_uuid>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_API(commands_api_interface, "fifo", "Return data about a fifo", fifo_api_function, FIFO_API_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "fifo_member", "Add members to a fifo", fifo_member_api_function, FIFO_MEMBER_API_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "fifo_add_outbound", "Add outbound members to a fifo", fifo_add_outbound_function, "<node> <url> [<priority>]");
	SWITCH_ADD_API(commands_api_interface, "fifo_check_bridge", "check if uuid is in a bridge", fifo_check_bridge_function, "<uuid>|<outbound_id>");
	switch_console_set_complete("add fifo list");
	switch_console_set_complete("add fifo list_verbose");
	switch_console_set_complete("add fifo count");
	switch_console_set_complete("add fifo has_outbound");
	switch_console_set_complete("add fifo importance");
	switch_console_set_complete("add fifo reparse");
	switch_console_set_complete("add fifo_check_bridge ::console::list_uuid");

	start_node_thread(globals.pool);

	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
*/
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fifo_shutdown)
{
	switch_event_t *pop = NULL;
	fifo_node_t *node, *this_node;
	switch_mutex_t *mutex = globals.mutex;

	switch_sql_queue_manager_destroy(&globals.qm);

	switch_event_unbind(&globals.node);
	switch_event_free_subclass(FIFO_EVENT);

	switch_mutex_lock(mutex);

	globals.running = 0;
	/* Cleanup */

	stop_node_thread();

	while(globals.threads) {
		switch_cond_next();
	}

	node = globals.nodes;

	while(node) {
		int x = 0;

		this_node = node;
		node = node->next;

		
		switch_mutex_lock(this_node->update_mutex);
		switch_mutex_lock(this_node->mutex);
		for (x = 0; x < MAX_PRI; x++) {
			while (fifo_queue_pop(this_node->fifo_list[x], &pop, 2) == SWITCH_STATUS_SUCCESS) {
				switch_event_destroy(&pop);
			}
		}
		switch_mutex_unlock(this_node->mutex);
		switch_core_hash_delete(globals.fifo_hash, this_node->name);
		switch_core_hash_destroy(&this_node->consumer_hash);
		switch_mutex_unlock(this_node->update_mutex);
		switch_core_destroy_memory_pool(&this_node->pool);
	}

	switch_core_hash_destroy(&globals.fifo_hash);
	switch_core_hash_destroy(&globals.caller_orig_hash);
	switch_core_hash_destroy(&globals.consumer_orig_hash);
	switch_core_hash_destroy(&globals.bridge_hash);
	switch_core_hash_destroy(&globals.use_hash);
	memset(&globals, 0, sizeof(globals));
	switch_mutex_unlock(mutex);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

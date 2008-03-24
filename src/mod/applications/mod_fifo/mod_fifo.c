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
 *
 * mod_fifo.c -- FIFO
 *
 */
#include <switch.h>

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fifo_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_fifo_load);
SWITCH_MODULE_DEFINITION(mod_fifo, mod_fifo_load, mod_fifo_shutdown, NULL);

#define FIFO_EVENT "fifo::info"

#define MAX_PRI 10

struct fifo_node {
    char *name;
    switch_mutex_t *mutex;
    switch_queue_t *fifo_list[MAX_PRI];
    switch_hash_t *caller_hash;
    switch_hash_t *consumer_hash;
    int caller_count;
    int waiting_count;
    int consumer_count;
};

typedef struct fifo_node fifo_node_t;


static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
    switch_core_session_t *bleg = (switch_core_session_t *) buf;

	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
        {
            switch_dtmf_t *dtmf = (switch_dtmf_t *) input;

            if (dtmf->digit == '*') {
                if (switch_channel_test_flag(switch_core_session_get_channel(session), CF_ORIGINATOR)) {
                    switch_channel_hangup(switch_core_session_get_channel(bleg), SWITCH_CAUSE_NORMAL_CLEARING);
                    return SWITCH_STATUS_BREAK;
                }
            }
        }
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

#define check_string(s) if (!switch_strlen_zero(s) && !strcasecmp(s, "undef")) { s = NULL; }

static switch_status_t read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	fifo_node_t *node = (fifo_node_t *) user_data;
	int x = 0, total = 0;
	
	for (x = 0; x < MAX_PRI; x++) {
		total += switch_queue_size(node->fifo_list[x]);
	}

    if (total) {
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}

static struct {
    switch_hash_t *fifo_hash;
    switch_mutex_t *mutex;
    switch_memory_pool_t *pool;
    int running;
} globals;


static fifo_node_t *create_node(const char *name)
{
    fifo_node_t *node;
	int x = 0;
	
    if (!globals.running) {
        return NULL;
    }

    node = switch_core_alloc(globals.pool, sizeof(*node));
    node->name = switch_core_strdup(globals.pool, name);
	for (x = 0; x < MAX_PRI; x++) {
		switch_queue_create(&node->fifo_list[x], SWITCH_CORE_QUEUE_LEN, globals.pool);
		switch_assert(node->fifo_list[x]);
	}
    switch_core_hash_init(&node->caller_hash, globals.pool);
    switch_core_hash_init(&node->consumer_hash, globals.pool);
	
    switch_mutex_init(&node->mutex, SWITCH_MUTEX_NESTED, globals.pool);
    switch_core_hash_insert(globals.fifo_hash, name, node);

    return node;
}

static void send_presence(fifo_node_t *node)
{
    switch_event_t *event;

    if (!globals.running) {
        return;
    }

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", "%s", "park");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", node->name);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s", node->name);
        if (node->waiting_count > 0) {
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Active (%d waiting)", node->waiting_count);
        } else {
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Idle");
        }
        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "%s", "unknown");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "alt_event_type", "dialog");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_count", "%d", 0);

        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "channel-state", "%s", node->waiting_count > 0 ? "CS_RING" : "CS_HANGUP");
        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "unique-id", "%s", node->name);
        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "answer-state", "%s", node->waiting_count > 0 ? "early" : "terminated");
        switch_event_add_header(event, SWITCH_STACK_BOTTOM, "call-direction", "%s", "inbound");
		switch_event_fire(&event);
	}
}


static void pres_event_handler(switch_event_t *event)
{
	char *to = switch_event_get_header(event, "to");
	char *dup_to = NULL, *node_name;
    fifo_node_t *node;

    if (!globals.running) {
        return;
    }
    
	if (!to || strncasecmp(to, "park+", 5)) {
		return;
	}

	dup_to = strdup(to);
    switch_assert(dup_to);

	node_name = dup_to + 5;

    switch_mutex_lock(globals.mutex);
    if (!(node = switch_core_hash_find(globals.fifo_hash, node_name))) {
        node = create_node(node_name);
    }

    switch_mutex_lock(node->mutex);
    send_presence(node);
    switch_mutex_unlock(node->mutex);

    switch_mutex_unlock(globals.mutex);

	switch_safe_free(dup_to);
}

#define FIFO_DESC "Fifo for stacking parked calls."
#define FIFO_USAGE "<fifo name> [in [<announce file>|undef] [<music file>|undef] | out [wait|nowait] [<announce file>|undef] [<music file>|undef]]"
SWITCH_STANDARD_APP(fifo_function)
{
    int argc;
    char *mydata = NULL, *argv[5] = { 0 };
    fifo_node_t *node;
    switch_channel_t *channel = switch_core_session_get_channel(session);
    int nowait = 0;
    const char *moh = NULL;
    const char *announce = NULL;
    switch_event_t *event = NULL;
    char date[80] = "";
    switch_time_exp_t tm;
    switch_time_t ts = switch_timestamp_now();
    switch_size_t retsize;

    if (!globals.running) {
        return;
    }

	if (switch_strlen_zero(data)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Args\n");
        return;
    }

    mydata = switch_core_session_strdup(session, data);
    switch_assert(mydata);
    if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2 || !argv[0]) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
        return;
    }

	switch_mutex_lock(globals.mutex);
    if (!(node = switch_core_hash_find(globals.fifo_hash, argv[0]))) {
        node = create_node(argv[0]);
    }
    switch_mutex_unlock(globals.mutex);

	moh = switch_channel_get_variable(channel, "fifo_music");
    announce = switch_channel_get_variable(channel, "fifo_announce");

    if (argc > 2) {
        nowait = !strcasecmp(argv[2], "nowait");
    }

    if (!strcasecmp(argv[1], "in")) {
        const char *uuid = strdup(switch_core_session_get_uuid(session));
		const char *pri;
		int p = 0;

        switch_channel_answer(channel);

        if (argc > 2) {
            announce = argv[2];
        }
        
        if (argc > 3) {
            moh = argv[3];
        }

        check_string(announce);
        check_string(moh);

		if (moh) {
            switch_ivr_broadcast(uuid, moh, SMF_LOOP | SMF_ECHO_ALEG);
        }
        
        switch_channel_set_flag(channel, CF_TAGGED);

        switch_mutex_lock(node->mutex);
        node->caller_count++;
        node->waiting_count++;
        send_presence(node);
        switch_core_hash_insert(node->caller_hash, uuid, session);

        if ((pri = switch_channel_get_variable(channel, "fifo_priority"))) {
			p = atoi(pri);
		}

		if (p >= MAX_PRI) {
			p = MAX_PRI - 1;
		}

        switch_queue_push(node->fifo_list[p], (void *)uuid);

        switch_mutex_unlock(node->mutex);

        ts = switch_timestamp_now();
        switch_time_exp_lt(&tm, ts);
        switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
        switch_channel_set_variable(channel, "fifo_status", "WAITING");
        switch_channel_set_variable(channel, "fifo_timestamp", date);
        
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Name", "%s", argv[0]);
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "push");
			switch_event_fire(&event);
		}
		
        switch_ivr_park(session, NULL);

		if (switch_channel_ready(channel)) {
            if (announce) {
                switch_ivr_play_file(session, NULL, announce, NULL);
            }
        }

        switch_channel_clear_flag(channel, CF_TAGGED);

        if (switch_channel_ready(channel)) {
            switch_channel_set_state(channel, CS_HIBERNATE);            
        } else {
            ts = switch_timestamp_now();
            switch_time_exp_lt(&tm, ts);
            switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
            switch_channel_set_variable(channel, "fifo_status", "ABORTED");
            switch_channel_set_variable(channel, "fifo_timestamp", date);
            
            if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
                switch_channel_event_set_data(channel, event);
                switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Name", "%s", argv[0]);
                switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "abort");
                switch_event_fire(&event);
            }
            switch_mutex_lock(node->mutex);
            node->caller_count--;
            node->waiting_count--;
            send_presence(node);
            switch_core_hash_delete(node->caller_hash, uuid);
            switch_mutex_unlock(node->mutex);
        }
        
        return;
    } else if (!strcasecmp(argv[1], "out")) {
        void *pop = NULL;
        switch_frame_t *read_frame;
        switch_status_t status;
        char *uuid;
        int done = 0;
        switch_core_session_t *other_session;
        switch_input_args_t args = { 0 };
		const char *pop_order = NULL;
		int custom_pop = 0;
		int pop_array[MAX_PRI] = { 0 };
		char *pop_list[MAX_PRI] = { 0 };
		const char *fifo_consumer_wrapup_sound = NULL;
		const char *fifo_consumer_wrapup_key = NULL;
		char buf[5] = "";

        if (argc > 3) {
            announce = argv[3];
        }

        if (argc > 4) {
            moh = argv[4];
        }
        
        check_string(announce);
        check_string(moh);
        
        if (!nowait) {
            switch_mutex_lock(node->mutex);
            node->consumer_count++;
            switch_core_hash_insert(node->consumer_hash, switch_core_session_get_uuid(session), session);
            switch_mutex_unlock(node->mutex);
            switch_channel_answer(channel);
        }

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
            switch_channel_event_set_data(channel, event);
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Name", "%s", argv[0]);
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_start");
            switch_event_fire(&event);
        }

        ts = switch_timestamp_now();
        switch_time_exp_lt(&tm, ts);
        switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
        switch_channel_set_variable(channel, "fifo_status", "WAITING");
        switch_channel_set_variable(channel, "fifo_timestamp", date);

        if ((pop_order = switch_channel_get_variable(channel, "fifo_pop_order"))) {
			char *tmp = switch_core_session_strdup(session, pop_order);
			int x;
			custom_pop = switch_separate_string(tmp, ',', pop_list, (sizeof(pop_list) / sizeof(pop_list[0])));
			if (custom_pop >= MAX_PRI) {
				custom_pop = MAX_PRI -1;
			}

			for (x = 0; x < custom_pop; x++) {
				int tmp = atoi(pop_list[x]);
				if (tmp > -1 && tmp < MAX_PRI) {
					pop_array[x] = tmp;
				}
			}
		}
		
		while(switch_channel_ready(channel)) {
			int x = 0 ;
			pop = NULL;

            if (moh) {
                args.read_frame_callback = read_frame_callback;
                args.user_data = node;
                switch_ivr_play_file(session, NULL, moh, &args);
            }
			
			if (custom_pop) {
				for(x = 0; x < MAX_PRI; x++) {
					if (switch_queue_trypop(node->fifo_list[pop_array[x]], &pop) == SWITCH_STATUS_SUCCESS && pop) {
						break;
					}
				}
			} else {
				for(x = 0; x < MAX_PRI; x++) {
					if (switch_queue_trypop(node->fifo_list[x], &pop) == SWITCH_STATUS_SUCCESS && pop) {
						break;
					}
				}
			}

			if (!pop) {
                if (nowait) {
                    break;
                }

                status = switch_core_session_read_frame(session, &read_frame, -1, 0);

                if (!SWITCH_READ_ACCEPTABLE(status)) {
                    break;
                }

                continue;
            }
			
            uuid = (char *) pop;
			pop = NULL;

            if ((other_session = switch_core_session_locate(uuid))) {
                switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
                switch_caller_profile_t *cloned_profile;

				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
                    switch_channel_event_set_data(other_channel, event);
                    switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Name", "%s", argv[0]);
                    switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "caller_pop");
                    switch_event_fire(&event);
                }

                if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
                    switch_channel_event_set_data(channel, event);
                    switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Name", "%s", argv[0]);
                    switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_pop");
                    switch_event_fire(&event);
                }

				if (announce) {
                    switch_ivr_play_file(session, NULL, announce, NULL);
                } else {
                    switch_ivr_sleep(session, 500);
                }

                if (switch_channel_test_flag(other_channel, CF_TAGGED)) {
                    switch_channel_clear_flag(other_channel, CF_CONTROLLED);
					switch_core_session_flush_private_events(other_session);
                    switch_channel_stop_broadcast(other_channel);
                    switch_core_session_kill_channel(other_session, SWITCH_SIG_BREAK);
                    while (switch_channel_test_flag(other_channel, CF_TAGGED)) {
                        status = switch_core_session_read_frame(session, &read_frame, -1, 0);
                        if (!SWITCH_READ_ACCEPTABLE(status)) {
                            break;
                        }
                    }
                }

                switch_channel_answer(channel);
				cloned_profile = switch_caller_profile_clone(other_session, switch_channel_get_caller_profile(channel));
                switch_assert(cloned_profile);
                switch_channel_set_originator_caller_profile(other_channel, cloned_profile);

				cloned_profile = switch_caller_profile_clone(session, switch_channel_get_caller_profile(other_channel));
                switch_assert(cloned_profile);
                switch_assert(cloned_profile->next == NULL);
                switch_channel_set_originatee_caller_profile(channel, cloned_profile);
				
                ts = switch_timestamp_now();
                switch_time_exp_lt(&tm, ts);
                switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
                switch_channel_set_variable(channel, "fifo_status", "TALKING");
                switch_channel_set_variable(channel, "fifo_target", uuid);
                switch_channel_set_variable(channel, "fifo_timestamp", date);
                
                switch_channel_set_variable(other_channel, "fifo_status", "TALKING");
                switch_channel_set_variable(other_channel, "fifo_timestamp", date);
                switch_channel_set_variable(other_channel, "fifo_target", switch_core_session_get_uuid(session));
                switch_mutex_lock(node->mutex);
                node->waiting_count--;
                send_presence(node);
                switch_mutex_unlock(node->mutex);
                switch_ivr_multi_threaded_bridge(session, other_session, on_dtmf, other_session, session);

                ts = switch_timestamp_now();
                switch_time_exp_lt(&tm, ts);
                switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
                switch_channel_set_variable(channel, "fifo_status", "WAITING");
                switch_channel_set_variable(channel, "fifo_timestamp", date);

                switch_channel_set_variable(other_channel, "fifo_status", "DONE");
                switch_channel_set_variable(other_channel, "fifo_timestamp", date);
                
                switch_mutex_lock(node->mutex);
                node->caller_count--;
                send_presence(node);
                switch_core_hash_delete(node->caller_hash, uuid);
                switch_mutex_unlock(node->mutex);
                switch_core_session_rwunlock(other_session);
                if (nowait) {
                    done = 1;
                }

				fifo_consumer_wrapup_sound = switch_channel_get_variable(channel, "fifo_consumer_wrapup_sound");
				fifo_consumer_wrapup_key = switch_channel_get_variable(channel, "fifo_consumer_wrapup_key");
				memset(buf, 0, sizeof(buf));

				if (!switch_strlen_zero(fifo_consumer_wrapup_sound)) {
					args.buf = buf;
					args.buflen = sizeof(buf);
					
					memset(&args, 0, sizeof(args));
					switch_ivr_play_file(session, NULL, fifo_consumer_wrapup_sound, &args);
				}

				if (!switch_strlen_zero(fifo_consumer_wrapup_key) && strcmp(buf, fifo_consumer_wrapup_key)) {
					for(;;) {
						char terminator = 0;						
						switch_ivr_collect_digits_count(session, buf, sizeof(buf)-1, 1, fifo_consumer_wrapup_key, &terminator, 0, 0, 0);
						if (terminator == *fifo_consumer_wrapup_key) {
							break;
						}
					}
				}
            }

            switch_safe_free(uuid);

            if (done) {
                break;
            }
        }

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, FIFO_EVENT) == SWITCH_STATUS_SUCCESS) {
            switch_channel_event_set_data(channel, event);
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Name", "%s", argv[0]);
            switch_event_add_header(event, SWITCH_STACK_BOTTOM, "FIFO-Action", "consumer_stop");
            switch_event_fire(&event);
        }

        if (!nowait) {
            switch_mutex_lock(node->mutex);
            switch_core_hash_delete(node->consumer_hash, switch_core_session_get_uuid(session));
            node->consumer_count--;
            switch_mutex_unlock(node->mutex);
        }
        
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "USAGE %s\n", FIFO_USAGE);
    }
}

static int xml_hash(switch_xml_t xml, switch_hash_t *hash, char *container, char *tag, int cc_off, int verbose)
{
    switch_xml_t x_tmp, x_caller, x_cp, variables;
    switch_hash_index_t *hi;
    switch_core_session_t *session;
    switch_channel_t *channel;
    void *val;
    const void *var;

    x_tmp = switch_xml_add_child_d(xml, container, cc_off++);
    switch_assert(x_tmp);

    for (hi = switch_hash_first(NULL, hash); hi; hi = switch_hash_next(hi)) {
        int c_off = 0, d_off = 0;
        const char *status;
        const char *ts;

        switch_hash_this(hi, &var, NULL, &val);
        session = (switch_core_session_t *) val;
        channel = switch_core_session_get_channel(session);
        x_caller = switch_xml_add_child_d(x_tmp, tag, c_off++);
        switch_assert(x_caller);
        
        switch_xml_set_attr_d(x_caller, "uuid", switch_core_session_get_uuid(session));

        if ((status = switch_channel_get_variable(channel, "fifo_status"))) {
            switch_xml_set_attr_d(x_caller, "status", status);
        }

        if ((ts = switch_channel_get_variable(channel, "fifo_timestamp"))) {
            switch_xml_set_attr_d(x_caller, "timestamp", ts);
        }

        if ((ts = switch_channel_get_variable(channel, "fifo_target"))) {
            switch_xml_set_attr_d(x_caller, "target", ts);
        }
        
		if (!(x_cp = switch_xml_add_child_d(x_caller, "caller_profile", d_off++))) {
            abort();
		}
        if (verbose) {
            d_off += switch_ivr_set_xml_profile_data(x_cp, switch_channel_get_caller_profile(channel), d_off);
        
            if (!(variables = switch_xml_add_child_d(x_caller, "variables", c_off++))) {
                abort();
            }
        
            switch_ivr_set_xml_chan_vars(variables, channel, c_off);
        }
        
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
    switch_snprintf(tmp, sizeof(buffer), "%d", node->caller_count);
    switch_xml_set_attr_d(x_fifo, "caller_count", tmp);
    switch_snprintf(tmp, sizeof(buffer), "%d", node->waiting_count);
    switch_xml_set_attr_d(x_fifo, "waiting_count", tmp);
    
    cc_off = xml_hash(x_fifo, node->caller_hash, "callers", "caller", cc_off, verbose);
    cc_off = xml_hash(x_fifo, node->consumer_hash, "consumers", "consumer", cc_off, verbose);

}

#define FIFO_API_SYNTAX "list|count [<fifo name>]"
SWITCH_STANDARD_API(fifo_api_function)
{
    int len = 0;
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

    if (!switch_strlen_zero(cmd)) {
        data = strdup(cmd);
        switch_assert(data);
    }
    
    if (switch_strlen_zero(cmd) || (argc = switch_separate_string(data, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 1 || !argv[0]) {
        stream->write_function(stream, "%s\n", FIFO_API_SYNTAX);
        return SWITCH_STATUS_SUCCESS;
    }

    switch_mutex_lock(globals.mutex);
    verbose = !strcasecmp(argv[0], "list_verbose");

    if (!strcasecmp(argv[0], "list") || verbose) {    
        char *xml_text = NULL;
        switch_xml_t x_report = switch_xml_new("fifo_report");
        switch_assert(x_report);

        if (argc < 2) {
            for (hi = switch_hash_first(NULL, globals.fifo_hash); hi; hi = switch_hash_next(hi)) {
                switch_hash_this(hi, &var, NULL, &val);
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
        
    } else if (!strcasecmp(argv[0], "count")) {
        if (argc < 2) {
            for (hi = switch_hash_first(NULL, globals.fifo_hash); hi; hi = switch_hash_next(hi)) {
				int x = 0;
                switch_hash_this(hi, &var, NULL, &val);
                node = (fifo_node_t *) val;
				len = 0;
				for (x = 0 ;x < MAX_PRI; x++) {
					len += switch_queue_size(node->fifo_list[x]);
				}
                switch_mutex_lock(node->mutex);
                stream->write_function(stream, "%s:%d:%d:%d\n", (char *)var, node->consumer_count, node->caller_count, len);
                switch_mutex_unlock(node->mutex);
                x++;
            }
            
            if (!x) {
                stream->write_function(stream, "none\n");
            }
        } else {
            if ((node = switch_core_hash_find(globals.fifo_hash, argv[1]))) {
				int x = 0;
				len = 0;
				for (x = 0 ;x < MAX_PRI; x++) {
                    len += switch_queue_size(node->fifo_list[x]);
                }
				
            }
            switch_mutex_lock(node->mutex);
            stream->write_function(stream, "%s:%d:%d:%d\n", argv[1], node->consumer_count, node->caller_count, len);
            switch_mutex_unlock(node->mutex);
        }
    }

    switch_mutex_unlock(globals.mutex);
    return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_fifo_load)
{
	switch_application_interface_t *app_interface;
    switch_api_interface_t *commands_api_interface;


	/* create/register custom event message type */
	if (switch_event_reserve_subclass(FIFO_EVENT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!", FIFO_EVENT);
		return SWITCH_STATUS_TERM;
	}

	/* Subscribe to presence request events */
	if (switch_event_bind((char *) modname, SWITCH_EVENT_PRESENCE_PROBE, SWITCH_EVENT_SUBCLASS_ANY, pres_event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't subscribe to presence request events!\n");
		return SWITCH_STATUS_GENERR;
	}

    switch_core_new_memory_pool(&globals.pool);
    switch_core_hash_init(&globals.fifo_hash, globals.pool);
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "fifo", "Park with FIFO", FIFO_DESC, fifo_function, FIFO_USAGE, SAF_NONE);
    SWITCH_ADD_API(commands_api_interface, "fifo", "Return data about a fifo", fifo_api_function, FIFO_API_SYNTAX);
    globals.running = 1;

	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down 
*/
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_fifo_shutdown)
{
    switch_hash_index_t *hi;
    void *val, *pop;
    fifo_node_t *node;
    switch_memory_pool_t *pool = globals.pool;
    switch_mutex_t *mutex = globals.mutex;

    switch_mutex_lock(mutex);
    
    globals.running = 0;
    /* Cleanup*/
    for (hi = switch_hash_first(NULL, globals.fifo_hash); hi; hi = switch_hash_next(hi)) {
		int x = 0 ;
        switch_hash_this(hi, NULL, NULL, &val);
        node = (fifo_node_t *) val;
		for (x = 0; x < MAX_PRI; x++) {
			while (switch_queue_trypop(node->fifo_list[x], &pop) == SWITCH_STATUS_SUCCESS) {
				free(pop);
			}
		}
        switch_core_hash_destroy(&node->caller_hash);
        switch_core_hash_destroy(&node->consumer_hash);
    }
    switch_core_hash_destroy(&globals.fifo_hash);
    memset(&globals, 0, sizeof(globals));    
    switch_mutex_unlock(mutex);
    switch_core_destroy_memory_pool(&pool);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

#include "mod_kazoo.h"

#define MAX_PEERS 250

typedef struct {
	switch_core_session_t *session;
	switch_core_session_t *bleg;
	switch_call_cause_t cause;
	switch_call_cause_t cancel_cause;
	const char *bridgeto;
	uint32_t timelimit_sec;
	const switch_state_handler_table_t *table;
	const char *cid_name_override;
	const char *cid_num_override;
	switch_caller_profile_t *caller_profile_override;
	switch_event_t *ovars;
	switch_originate_flag_t flags;
	switch_status_t status;
	int done;
	switch_thread_t *thread;
	switch_mutex_t *mutex;
} kz_enterprise_originate_handle_t;


struct kz_ent_originate_ringback {
	switch_core_session_t *session;
	int running;
	const char *ringback_data;
	switch_thread_t *thread;
};

static void *SWITCH_THREAD_FUNC kz_enterprise_originate_thread(switch_thread_t *thread, void *obj)
{
	kz_enterprise_originate_handle_t *handle = (kz_enterprise_originate_handle_t *) obj;

	handle->done = 0;
	handle->status = switch_ivr_originate(NULL, &handle->bleg, &handle->cause,
										  handle->bridgeto, handle->timelimit_sec,
										  handle->table,
										  handle->cid_name_override,
										  handle->cid_num_override,
										  handle->caller_profile_override,
										  handle->ovars,
										  handle->flags,
										  &handle->cancel_cause,
										  NULL);


	handle->done = 1;
	switch_mutex_lock(handle->mutex);
	switch_mutex_unlock(handle->mutex);

	if (handle->done != 2) {
		if (handle->status == SWITCH_STATUS_SUCCESS && handle->bleg) {
			switch_channel_t *channel = switch_core_session_get_channel(handle->bleg);

			switch_channel_set_variable(channel, "group_dial_status", "loser");
			switch_channel_hangup(channel, SWITCH_CAUSE_LOSE_RACE);
			switch_core_session_rwunlock(handle->bleg);
		}
	}

	return NULL;
}

static void *SWITCH_THREAD_FUNC kz_enterprise_originate_ringback_thread(switch_thread_t *thread, void *obj)
{
	struct kz_ent_originate_ringback *rb_data = (struct kz_ent_originate_ringback *) obj;
	switch_core_session_t *session = rb_data->session;
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	channel = switch_core_session_get_channel(session);

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "started ringback thread\n");

	while (rb_data->running && switch_channel_ready(channel)) {
		switch_ivr_parse_all_messages(session);
		if (status != SWITCH_STATUS_BREAK) {
			if (zstr(rb_data->ringback_data) || !strcasecmp(rb_data->ringback_data, "silence")) {
				status = switch_ivr_collect_digits_callback(session, NULL, 0, 0);
			} else if (switch_is_file_path(rb_data->ringback_data)) {
				status = switch_ivr_play_file(session, NULL, rb_data->ringback_data, NULL);
			} else {
				status = switch_ivr_gentones(session, rb_data->ringback_data, 0, NULL);
			}
		}

		if (status == SWITCH_STATUS_BREAK) {
			switch_channel_set_flag(channel, CF_NOT_READY);
		}
	}
	switch_core_session_rwunlock(session);

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "stopped ringback thread\n");

	rb_data->running = 0;
	return NULL;
}


void start_ringback(switch_core_session_t *session, struct kz_ent_originate_ringback *rb_data, switch_threadattr_t *thd_attr, switch_memory_pool_t *pool)
{
	const char *ringback_data = NULL;
	switch_channel_t *channel = NULL;

	if (!session) return;

	channel = switch_core_session_get_channel(session);
	if (channel && !switch_channel_test_flag(channel, CF_PROXY_MODE) && !switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
		if (switch_channel_test_flag(channel, CF_ANSWERED)) {
			ringback_data = switch_channel_get_variable(channel, "transfer_ringback");
		}

		if (!ringback_data) {
			ringback_data = switch_channel_get_variable(channel, "ringback");
		}

		if (ringback_data || switch_channel_media_ready(channel)) {
			rb_data->ringback_data = ringback_data;
			rb_data->session = session;
			rb_data->running = 1;
			if (!switch_channel_media_ready(channel)) {
				if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
					return;
				}
			}
			switch_thread_create(&rb_data->thread, thd_attr, kz_enterprise_originate_ringback_thread, rb_data, pool);
		}
	}
}

void stop_ringback(switch_core_session_t *session, struct kz_ent_originate_ringback *rb_data)
{
	switch_status_t tstatus = SWITCH_STATUS_FALSE;
	if (!session) return;
	if (rb_data->running) {
		rb_data->running = 0;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "waiting for ringback thread to exit\n");
		switch_thread_join(&tstatus, rb_data->thread);
	}
	rb_data->thread = NULL;
}

SWITCH_DECLARE(switch_status_t) kz_switch_ivr_enterprise_originate(switch_core_session_t *session,
																switch_core_session_t **bleg,
																switch_call_cause_t *cause,
																const char *bridgeto,
																uint32_t timelimit_sec,
																const switch_state_handler_table_t *table,
																const char *cid_name_override,
																const char *cid_num_override,
																switch_caller_profile_t *caller_profile_override,
																switch_event_t *ovars, switch_originate_flag_t flags,
																switch_call_cause_t *cancel_cause)
{
	int x_argc = 0;
	char *x_argv[MAX_PEERS] = { 0 };
	kz_enterprise_originate_handle_t *hp = NULL, handles[MAX_PEERS] = { {0} };
	int i;
	switch_caller_profile_t *cp = NULL;
	switch_channel_t *channel = NULL;
	char *data;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_threadattr_t *thd_attr = NULL;
	int running = 0, over = 0;
	switch_status_t tstatus = SWITCH_STATUS_FALSE;
	switch_memory_pool_t *pool;
	switch_event_header_t *hi = NULL;
	struct kz_ent_originate_ringback rb_data = { 0 };
	switch_event_t *var_event = NULL;
	int getcause = 1;
    const char* holding = NULL;
	switch_bool_t xfer = SWITCH_FALSE;


	*cause = SWITCH_CAUSE_SUCCESS;

	switch_core_new_memory_pool(&pool);

	if (zstr(bridgeto)) {
		*cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		getcause = 0;
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	data = switch_core_strdup(pool, bridgeto);

	if (session) {
		switch_caller_profile_t *cpp = NULL;
		channel = switch_core_session_get_channel(session);
		if ((cpp = switch_channel_get_caller_profile(channel))) {
			cp = switch_caller_profile_dup(pool, cpp);
		}
	}

	if (ovars) {
		var_event = ovars;
	} else {
		if (switch_event_create_plain(&var_event, SWITCH_EVENT_CHANNEL_DATA) != SWITCH_STATUS_SUCCESS) {
			abort();
		}
	}

	if (session) {
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "ent_originate_aleg_uuid", switch_core_session_get_uuid(session));
	}

	if (channel) {
		const char *tmp_var = NULL;

		switch_channel_process_export(channel, NULL, var_event, SWITCH_EXPORT_VARS_VARIABLE);

		if ((tmp_var = switch_channel_get_variable(channel, "effective_ani"))) {
			switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_ani", tmp_var);
		}

		if ((tmp_var = switch_channel_get_variable(channel, "effective_aniii"))) {
			switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_aniii", tmp_var);
		}

		if ((tmp_var = switch_channel_get_variable(channel, "effective_caller_id_name"))) {
			switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_caller_id_name", tmp_var);
		}

		if ((tmp_var = switch_channel_get_variable(channel, "effective_caller_id_number"))) {
			switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_caller_id_number", tmp_var);
		}
	}

	/* strip leading spaces */
	while (data && *data && *data == ' ') {
		data++;
	}

	/* extract channel variables, allowing multiple sets of braces */
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Parsing ultra-global variables\n");
	while (data && *data == '<') {
		char *parsed = NULL;

		if (switch_event_create_brackets(data, '<', '>', ',', &var_event, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Parse Error!\n");
			switch_goto_status(SWITCH_STATUS_GENERR, done);
		}

		data = parsed;
	}

	/* strip leading spaces (again) */
	while (data && *data && *data == ' ') {
		data++;
	}

	if (ovars && ovars != var_event) {
		for (hi = ovars->headers; hi; hi = hi->next) {
			switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, hi->name, hi->value);
		}
	}

	switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "ignore_early_media", "true");

	if (!(x_argc = switch_separate_string_string(data, SWITCH_ENT_ORIGINATE_DELIM, x_argv, MAX_PEERS))) {
		*cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
		getcause = 0;
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

	for (i = 0; i < x_argc; i++) {
		handles[i].session = session;
		handles[i].bleg = NULL;
		handles[i].cause = 0;
		handles[i].cancel_cause = 0;
		handles[i].bridgeto = x_argv[i];
		handles[i].timelimit_sec = timelimit_sec;
		handles[i].table = table;
		handles[i].cid_name_override = cid_name_override;
		handles[i].cid_num_override = cid_num_override;
		handles[i].caller_profile_override = cp;
		switch_event_dup(&handles[i].ovars, var_event);
		handles[i].flags = flags;
		switch_mutex_init(&handles[i].mutex, SWITCH_MUTEX_NESTED, pool);
		switch_mutex_lock(handles[i].mutex);
		switch_thread_create(&handles[i].thread, thd_attr, kz_enterprise_originate_thread, &handles[i], pool);
	}

	start_ringback(session, &rb_data, thd_attr, pool);

	for (;;) {
		running = 0;
		over = 0;

		if (channel && !switch_channel_ready(channel)) {
            // switch_call_cause_t channel_cause = switch_channel_get_cause(channel);
            // const char* channel_cause_str = switch_channel_cause2str(channel_cause);
            // const char* channel_flags = kz_channel_get_flags_string(channel);
            // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "CHANNEL NOT READY => %d %s => %s\n", channel_cause, channel_cause_str, channel_flags);

            if (!xfer) {
				holding = switch_channel_get_variable(channel, SWITCH_HOLDING_UUID_VARIABLE);

                if (holding && switch_channel_test_flag(channel, CF_TRANSFER) && switch_channel_test_flag(channel, CF_XFER_ZOMBIE)) {
                    switch_core_session_t *holding_session;

                    if ((holding_session = switch_core_session_locate(holding))) {
                        switch_channel_t *holding_channel = switch_core_session_get_channel(holding_session);

                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "2600hz sends greetings!\n");

						stop_ringback(session, &rb_data);
                        channel = holding_channel;
						session = holding_session;
						start_ringback(session, &rb_data, thd_attr, pool);
						xfer = SWITCH_TRUE;
                        switch_core_session_rwunlock(holding_session);
                        continue;
                    }

                }
            }

			if (switch_channel_down_nosig(channel) || !xfer) {
				break;
			}
		}

		if (cancel_cause && *cancel_cause > 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "cancel cause %d %s\n", *cancel_cause, switch_channel_cause2str(*cancel_cause));
			break;
		}

		for (i = 0; i < x_argc; i++) {


			if (handles[i].done == 0) {
				running++;
			} else if (handles[i].done == 1) {
				if (handles[i].status == SWITCH_STATUS_SUCCESS) {
					handles[i].done = 2;
					hp = &handles[i];
					goto done;
				} else {
					handles[i].done = -1;
				}
			} else {
				over++;
			}

			switch_yield(10000);
		}

		if (!running || over == x_argc) {
			break;
		}
	}


  done:

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "we're done\n");

	if (hp) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "we're done with hp\n");
		*cause = hp->cause;
		getcause = 0;
		status = hp->status;
		*bleg = hp->bleg;
		if (*bleg) {
			switch_channel_t *bchan = switch_core_session_get_channel(*bleg);
			switch_caller_profile_t *cloned_profile;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "we're done with hp and bleg\n");

			if (session) {
				cloned_profile = switch_caller_profile_clone(*bleg, cp);
				switch_channel_set_originator_caller_profile(bchan, cloned_profile);

				cloned_profile = switch_caller_profile_clone(session, switch_channel_get_caller_profile(bchan));
				switch_channel_set_originatee_caller_profile(channel, cloned_profile);
			}
		}
		switch_mutex_unlock(hp->mutex);
		switch_thread_join(&tstatus, hp->thread);
		switch_event_destroy(&hp->ovars);
	}

	for (i = 0; i < x_argc; i++) {
		if (hp == &handles[i]) {
			continue;
		}

		if (cancel_cause && *cancel_cause > 0) {
			handles[i].cancel_cause = *cancel_cause;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "setting cancel cause %d - %s\n", *cancel_cause, switch_channel_cause2str(*cancel_cause));
		} else {
			handles[i].cancel_cause = SWITCH_CAUSE_LOSE_RACE;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "setting LOOSE_RACE\n");
		}

		if (handles[i].bleg) {
			switch_channel_t *bchan = switch_core_session_get_channel(handles[i].bleg);
			if (bchan) {
				switch_channel_clear_flag(bchan, CF_BLOCK_STATE);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "no bchan on setting LOOSE_RACE\n");
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "no bleg on setting LOOSE_RACE\n");
		}

	}

	for (i = 0; i < x_argc; i++) {

		if (hp == &handles[i]) {
			continue;
		}

		if (getcause && channel && handles[i].cause && handles[i].cause != SWITCH_CAUSE_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "handling cause %d %s\n", handles[i].cause, switch_channel_cause2str(handles[i].cause));
			switch_channel_handle_cause(channel, handles[i].cause);
		}

		switch_mutex_unlock(handles[i].mutex);

		if (getcause && *cause != handles[i].cause && handles[i].cause != SWITCH_CAUSE_LOSE_RACE && handles[i].cause != SWITCH_CAUSE_NO_PICKUP) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "setting cause to %d %s\n", handles[i].cause, switch_channel_cause2str(handles[i].cause));
			*cause = handles[i].cause;
			getcause++;
		}

		switch_thread_join(&tstatus, handles[i].thread);
		switch_event_destroy(&handles[i].ovars);
	}

	stop_ringback(session, &rb_data);

  end:

	if (getcause == 1 && *cause == SWITCH_CAUSE_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "setting NO ANSWER\n");
		*cause = SWITCH_CAUSE_NO_ANSWER;
	}

	if (channel) {
		if (*cause == SWITCH_CAUSE_SUCCESS) {
			switch_channel_set_variable(channel, "originate_disposition", "success");
		} else {
			switch_channel_set_variable(channel, "originate_disposition", "failure");
			switch_channel_set_variable(channel, "hangup_cause", switch_channel_cause2str(*cause));
		}
	}


	if (var_event && var_event != ovars) {
		switch_event_destroy(&var_event);
	}

	switch_core_destroy_memory_pool(&pool);

	return status;

}


SWITCH_DECLARE(switch_status_t) kz_switch_ivr_originate(switch_core_session_t *session,
													 switch_core_session_t **bleg,
													 switch_call_cause_t *cause,
													 const char *bridgeto,
													 uint32_t timelimit_sec,
													 const switch_state_handler_table_t *table,
													 const char *cid_name_override,
													 const char *cid_num_override,
													 switch_caller_profile_t *caller_profile_override,
													 switch_event_t *ovars, switch_originate_flag_t flags,
													 switch_call_cause_t *cancel_cause,
													 switch_dial_handle_t *dh)
{
	if (strstr(bridgeto, SWITCH_ENT_ORIGINATE_DELIM)) {
		return kz_switch_ivr_enterprise_originate(session, bleg, cause, bridgeto, timelimit_sec, table, cid_name_override, cid_num_override,
											   caller_profile_override, ovars, flags, cancel_cause);
	} else {
		return switch_ivr_originate(session, bleg, cause, bridgeto, timelimit_sec, table, cid_name_override, cid_num_override,
											   caller_profile_override, ovars, flags, cancel_cause, dh);
    }
}

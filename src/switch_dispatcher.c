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
 * Eliot Gable <egable@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Eliot Gable <egable@gmail.com>
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * switch_dispatcher.c -- Threaded event dispatcher.
 *
 */


#include <switch.h>
#include <switch_dispatcher.h>

#ifdef BUILD_DISPATCHER

void switch_dispatcher_event_handler(switch_event_t *event)
{
	switch_event_t *event_dup = NULL;
	switch_dispatcher_t *disp = NULL;

	if (!event) return;

	if (!event->bind_user_data) return;

	disp = (switch_dispatcher_t *)event->bind_user_data;
	if (!switch_atomic_read(&disp->accept_messages)) return;

	if (switch_event_dup(&event_dup, event) == SWITCH_STATUS_SUCCESS) {
		switch_dispatcher_message_t *mesg = NULL;

		switch_zmalloc(mesg, sizeof(*mesg));
		mesg->data = event_dup;
		mesg->is_switch_event = SWITCH_TRUE;
		switch_queue_push(disp->queue, mesg);
	
		switch_atomic_inc(&disp->queue_count);
		if (switch_atomic_read(&disp->max_queue_count) < switch_atomic_read(&disp->queue_count)) {
			switch_atomic_inc(&disp->max_queue_count);
		}
		if (switch_mutex_trylock(disp->mutex) == SWITCH_STATUS_SUCCESS) {
			switch_thread_cond_broadcast(disp->cond);
			switch_mutex_unlock(disp->mutex);
		}
	}

}

static void *SWITCH_THREAD_FUNC switch_dispatcher_thread(switch_thread_t *thread, void *obj)
{
	void *tmp = NULL;
	switch_status_t status;
	switch_dispatcher_t *disp = (switch_dispatcher_t *)obj;

	switch_atomic_set(&disp->running, 1);
	switch_mutex_lock(disp->mutex);
	while((status = switch_queue_trypop(disp->queue, &tmp)) == SWITCH_STATUS_SUCCESS || !switch_atomic_read(&disp->halt))
	{
		switch_dispatcher_message_t *mesg = NULL;
        switch_time_t start = 0;
        switch_time_t end = 0;

		if (status == SWITCH_STATUS_SUCCESS) {

			if (disp->collect_stats) {
				start = switch_time_now();
				switch_atomic_dec(&disp->queue_count);
			}
			mesg = (switch_dispatcher_message_t *)tmp; tmp = NULL;

			if (mesg->is_switch_event) {
				switch_event_t *event = NULL;
				switch_dispatcher_event_binding_data_t *bdata = NULL;
				char *key = NULL;

				event = (switch_event_t *)mesg->data;
				key = switch_mprintf("%x:%s", (uint64_t)event->event_id, event->subclass_name);

				switch_mutex_lock(disp->event_mutex);
				bdata = (switch_dispatcher_event_binding_data_t *)switch_core_hash_find(disp->event_bindings, key);
				if (bdata) {
					event->bind_user_data = bdata->binding_data;
					(*(bdata->callback))(event);
				}
				switch_mutex_unlock(disp->event_mutex);
				switch_safe_free(key);

			} else {
				(*disp->callback)(mesg->data, disp->thread_data);
			}

			switch_safe_free(mesg);
			
			if (disp->collect_stats) {
				end = switch_time_now();

				switch_mutex_lock(disp->process_loop_stats_mutex);
				disp->total_count++;
				disp->count_cur_sec++;
				disp->last_duration = (end - start);
				disp->total_time += disp->last_duration;
				if (disp->last_duration > disp->max_duration) {
					disp->max_duration = disp->last_duration;
				}
				if (end - 1000000 > disp->last_sec_start) {
					disp->last_sec_start = end;
					disp->count_past_sec = disp->count_cur_sec;
					disp->count_cur_sec = 0;
					if (disp->count_past_sec > disp->max_per_sec) {
						disp->max_per_sec = disp->count_past_sec;
					}
				}
				switch_mutex_unlock(disp->process_loop_stats_mutex);
			}
		}

		if (!mesg) {
            if (!switch_queue_size(disp->queue)) {
                switch_thread_cond_wait(disp->cond, disp->mutex);
            } else {
                switch_cond_next();
            }
        }
	}
	switch_mutex_unlock(disp->mutex);
	switch_atomic_set(&disp->running, 0);

	return NULL;	
}


SWITCH_DECLARE(switch_status_t) switch_dispatcher_create_real(switch_dispatcher_t **dispatcher_out, switch_size_t queue_size, switch_bool_t drop_overflow,
															  switch_bool_t collect_stats, switch_dispatcher_func_t callback, void *thread_data,
															  const char *file, const char *func, int line)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_dispatcher_t *disp = NULL;

	if (!dispatcher_out) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a switch_dispatcher_t*\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (!callback) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null callback function.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	
	switch_zmalloc(disp, sizeof(*disp));
	disp->callback = callback;
	disp->queue_size = (queue_size ? queue_size : 131072);
	disp->drop_overflow = drop_overflow;
	disp->thread_data = thread_data;
	switch_core_new_memory_pool(&disp->pool);
	switch_mutex_init(&disp->mutex, SWITCH_MUTEX_NESTED, disp->pool);
	switch_mutex_init(&disp->process_loop_stats_mutex, SWITCH_MUTEX_NESTED, disp->pool);
	switch_core_hash_init(&disp->event_bindings, disp->pool);
	switch_thread_cond_create(&disp->cond, disp->pool);
	switch_queue_create(&disp->queue, queue_size, disp->pool);
	switch_threadattr_create(&disp->thread_attr, disp->pool);
	switch_threadattr_detach_set(disp->thread_attr, 0);
	switch_threadattr_stacksize_set(disp->thread_attr, SWITCH_THREAD_STACKSIZE);
	switch_atomic_set(&disp->running, 0);
	switch_atomic_set(&disp->halt, 0);
	switch_atomic_set(&disp->accept_messages, 1);
	status = switch_thread_create(&disp->thread, disp->thread_attr, switch_dispatcher_thread, (void *)disp, disp->pool);
	*dispatcher_out = disp;

 done:
	return status;
}
								
SWITCH_DECLARE(switch_status_t) switch_dispatcher_destroy_real(switch_dispatcher_t **dispatcher_out, const char *file, const char *func, int line)
{
	switch_dispatcher_t *disp = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!dispatcher_out) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a switch_dispatcher_t*\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	disp = *dispatcher_out;
	*dispatcher_out = NULL;
	switch_atomic_set(&disp->accept_messages, 0);

	while (switch_atomic_read(&disp->running)) {
		switch_mutex_lock(disp->mutex);
		switch_thread_cond_broadcast(disp->cond);
		switch_mutex_unlock(disp->mutex);
		switch_yield(100);
	}

	switch_thread_join(&status, disp->thread);
	switch_mutex_destroy(disp->mutex);
	switch_mutex_destroy(disp->process_loop_stats_mutex);
	switch_core_hash_destroy(&disp->event_bindings);
	switch_thread_cond_destroy(disp->cond);
	switch_core_destroy_memory_pool(&disp->pool);

 done:
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_dispatcher_enqueue_real(switch_dispatcher_t *disp, void *data, const char *file, const char *func, int line)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_dispatcher_message_t *mesg = NULL;

	if (!disp) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a switch_dispatcher_t as the first argument.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (!data) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null void pointer to a message.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (!switch_atomic_read(&disp->accept_messages)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Dispatcher passed in call to %s was already shut down.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	switch_zmalloc(mesg, sizeof(*mesg));
	mesg->data = data;
	mesg->is_switch_event = SWITCH_FALSE;
	switch_queue_push(disp->queue, mesg);
	
	if (disp->collect_stats) {
		/* Technically, this whole set of stats operations should be wrapped in a mutex, but at worst it makes the max count off by 1, and there is no
		   reason to impact performance for such an issue. */
		switch_atomic_inc(&disp->queue_count);
		if (switch_atomic_read(&disp->max_queue_count) < switch_atomic_read(&disp->queue_count)) {
			switch_atomic_inc(&disp->max_queue_count);
		}
	}
	if (switch_mutex_trylock(disp->mutex) == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_broadcast(disp->cond);
		switch_mutex_unlock(disp->mutex);
	}

 done:
	return status;
}

SWITCH_DECLARE(void) switch_dispatcher_dump_stats_real(switch_dispatcher_t *disp, switch_stream_handle_t *stream, const char *file,
																  const char *func, int line)
{
	switch_mutex_lock(disp->process_loop_stats_mutex);

	stream->write_function(stream, "%41s: %10u | %35s: %10.3f sec\n",
                           "Current messages waiting to be processed", (uint32_t)(switch_atomic_read(&disp->queue_count)),
                           "Total message processing time",            ((double)disp->total_time / 1000000)
                           );

    stream->write_function(stream, "%41s: %10u | %35s: %10.3f ms\n",
                           "Max messages waiting to be processed",        (uint32_t)(switch_atomic_read(&disp->max_queue_count)),
                           "Last message processing duration",            ((double)disp->last_duration / 1000)
                           );

    stream->write_function(stream, "%41s: %10u | %35s: %10.3f ms\n",
                           "Messages processed past second",           (uint32_t)(switch_atomic_read(&disp->count_past_sec)),
                           "Average message processing duration",      (disp->total_count ? ((double)disp->total_time / 1000) / disp->total_count : 0)
                           );

    stream->write_function(stream, "%38s: %10u | %35s: %10.3f ms  |\n",
                           "Max messages processed per second",        (uint32_t)(switch_atomic_read(&disp->max_per_sec)),
                           "Max message processing duration",          ((double)disp->max_duration / 1000)
                           );

    stream->write_function(stream, "%38s: %10u |\n",
                           "Total messages processed", (uint64_t)(disp->total_count)
                           );

	switch_mutex_unlock(disp->process_loop_stats_mutex);
}


SWITCH_DECLARE(switch_status_t) switch_dispatcher_event_bind_real(switch_dispatcher_t *disp, const char *id, switch_event_types_t event,
																  const char *subclass_name, switch_event_callback_t callback,
																  void *user_data, const char *file, const char *func, int line)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_event_node_t* node = NULL;
	switch_dispatcher_event_binding_data_t *bdata = NULL;
	char *key = NULL;

	if (!disp) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a switch_dispatcher_t as the first argument.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (!switch_atomic_read(&disp->accept_messages)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Dispatcher passed in call to %s was already shut down.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (zstr(subclass_name) && (event & SWITCH_EVENT_CUSTOM)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a proper subclass name as argument 4 when binding to SWITCH_EVENT_CUSTOM.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (!callback) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a callback function of type switch_event_callback_t as argument 5.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	key = switch_mprintf("%x:%s", (uint64_t)event, subclass_name);
    bdata = (switch_dispatcher_event_binding_data_t*)switch_core_hash_find(disp->event_bindings, key);
	if (node) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a unique event and subclass name combination. The requested event and subclass name combination was already found in the list of active bindings.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	switch_mutex_lock(disp->event_mutex);
	switch_event_bind_removable(id, event, subclass_name, switch_dispatcher_event_handler, bdata, &node);
	if (node) {
		switch_dispatcher_event_binding_data_t *bdata = NULL;
		switch_zmalloc(bdata, sizeof(*bdata));
		bdata->node = node;
		bdata->binding_data = user_data;
		bdata->callback = callback;
		switch_core_hash_insert(disp->event_bindings, key, bdata);
	} else {
		status = SWITCH_STATUS_FALSE;
	}
	switch_mutex_unlock(disp->event_mutex);

 done:
	switch_safe_free(key);
	return status;
}


SWITCH_DECLARE(switch_status_t) switch_dispatcher_event_unbind_real(switch_dispatcher_t *disp, switch_event_types_t event, const char *subclass_name, 
																	const char *file, const char *func, int line)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_dispatcher_event_binding_data_t *bdata = NULL;
	char *key = NULL;

	if (!disp) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a switch_dispatcher_t as the first argument.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (!switch_atomic_read(&disp->accept_messages)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Dispatcher passed in call to %s was already shut down.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}
	if (zstr(subclass_name) && (event & SWITCH_EVENT_CUSTOM)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, NULL, SWITCH_LOG_CRIT, "BUG: Call to %s must be passed a non-null pointer to a proper subclass name as argument 3 when unbinding from SWITCH_EVENT_CUSTOM.\n", __FUNCTION__);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	key = switch_mprintf("%x:%s", (uint64_t)event, subclass_name);
	switch_mutex_lock(disp->event_mutex);
	bdata = (switch_dispatcher_event_binding_data_t*)switch_core_hash_find(disp->event_bindings, key);
	if (bdata) {
		switch_event_unbind(&bdata->node);
		switch_safe_free(bdata);
		switch_core_hash_delete(disp->event_bindings, key);
	} else {
		status = SWITCH_STATUS_FALSE;
	}
	switch_mutex_unlock(disp->event_mutex);

 done:
	switch_safe_free(key);
	return status;
}

#endif

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

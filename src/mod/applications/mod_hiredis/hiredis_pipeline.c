/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2016, Anthony Minessale II <anthm@freeswitch.org>
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
* Christopher Rienzo <chris.rienzo@citrix.com>
*
* hiredis_pipeline.c -- batched operations to redis
*
*/

#include <mod_hiredis.h>

/**
 * Thread that processes redis requests
 * @param thread this thread
 * @param obj the profile
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC pipeline_thread(switch_thread_t *thread, void *obj)
{
	hiredis_profile_t *profile = (hiredis_profile_t *)obj;
	switch_thread_rwlock_rdlock(profile->pipeline_lock);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Redis pipeline thread started for [%s]\n", profile->name);

	while ( profile->pipeline_running || switch_queue_size(profile->active_requests) > 0 ) {
		void *val = NULL;
		if ( switch_queue_pop_timeout(profile->active_requests, &val, 500 * 1000) == SWITCH_STATUS_SUCCESS && val ) {
			int request_count = 1;
			hiredis_request_t *requests = (hiredis_request_t *)val;
			hiredis_request_t *cur_request = requests;
			cur_request->next = NULL;
			/* This would be easier to code in reverse order, but I prefer to execute requests in the order that they arrive */
			while ( request_count < profile->max_pipelined_requests ) {
				if ( switch_queue_trypop(profile->active_requests, &val) == SWITCH_STATUS_SUCCESS && val ) {
					request_count++;
					cur_request = cur_request->next = (hiredis_request_t *)val;
					cur_request->next = NULL;
				} else {
					break;
				}
			}
			hiredis_profile_execute_requests(profile, NULL, requests);
			cur_request = requests;
			while ( cur_request ) {
				hiredis_request_t *next_request = cur_request->next; /* done here to prevent race with waiter */
				if ( cur_request->response ) {
					/* signal waiter */
					switch_mutex_lock(cur_request->mutex);
					cur_request->done = 1;
					switch_thread_cond_signal(cur_request->cond);
					switch_mutex_unlock(cur_request->mutex);
				} else {
					/* nobody to signal, clean it up */
					switch_safe_free(cur_request->request);
					switch_safe_free(cur_request->keys);
					switch_safe_free(cur_request->session_uuid);
					switch_queue_trypush(profile->request_pool, cur_request);
				}
				cur_request = next_request;
			}
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Redis pipeline thread ended for [%s]\n", profile->name);

	switch_thread_rwlock_unlock(profile->pipeline_lock);

	return NULL;
}

/**
 * Add a pipeline thread to the profile's thread pool
 */
void hiredis_pipeline_thread_start(hiredis_profile_t *profile)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	profile->pipeline_running = 1;
	switch_threadattr_create(&thd_attr, profile->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, pipeline_thread, profile, profile->pool);
}

/**
 * Wait for all pipeline threads to complete
 */
void hiredis_pipeline_threads_stop(hiredis_profile_t *profile)
{
	if ( profile->pipeline_running ) {
		profile->pipeline_running = 0;
		switch_queue_interrupt_all(profile->active_requests);
		switch_thread_rwlock_wrlock(profile->pipeline_lock);
	}
}

/**
 * Execute pipelined request and wait for response.
 * @param profile to use
 * @param session (optional)
 * @param request - the request
 * @return status SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t hiredis_profile_execute_pipeline_request(hiredis_profile_t *profile, switch_core_session_t *session, hiredis_request_t *request)
{
	switch_status_t status;

	/* send request to thread pool */
	if ( profile->pipeline_running && switch_queue_trypush(profile->active_requests, request) == SWITCH_STATUS_SUCCESS ) {
		if ( request->response ) {
			/* wait for response */
			switch_mutex_lock(request->mutex);
			while ( !request->done ) {
				switch_thread_cond_timedwait(request->cond, request->mutex, 1000 * 1000);
			}

			/* get response */
			switch_mutex_unlock(request->mutex);
			status = request->status;

			/* save back to pool */
			switch_queue_trypush(profile->request_pool, request);
		} else {
			status = SWITCH_STATUS_SUCCESS;
		}
	} else {
		/* failed... do sync request instead */
		status = hiredis_profile_execute_sync(profile, session, request->response, request->request);
		if ( !request->response ) {
			switch_safe_free(request->request);
			switch_safe_free(request->keys);
			switch_safe_free(request->session_uuid);
		}
		switch_queue_trypush(profile->request_pool, request);
	}
	return status;
}

/**
 * Execute pipelined request and wait for response.
 * @param profile to use
 * @param session (optional)
 * @param resp (optional) - if no resp, this function will not wait for the result
 * @param request_string - the request
 * @return status SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t hiredis_profile_execute_pipeline(hiredis_profile_t *profile, switch_core_session_t *session, char **resp, const char *request_string)
{
	void *val = NULL;
	hiredis_request_t *request = NULL;

	if (switch_queue_trypop(profile->request_pool, &val) == SWITCH_STATUS_SUCCESS && val) {
		request = (hiredis_request_t *)val;
	} else {
		request = switch_core_alloc(profile->pool, sizeof(*request));
		switch_thread_cond_create(&request->cond, profile->pool);
		switch_mutex_init(&request->mutex, SWITCH_MUTEX_UNNESTED, profile->pool);
	}
	request->response = resp;
	request->done = 0;
	request->do_eval = 0;
	request->num_keys = 0;
	request->keys = NULL;
	request->status = SWITCH_STATUS_SUCCESS;
	request->next = NULL;
	request->session_uuid = NULL;
	request->argc = 0;
	if ( resp ) {
		/* will block, no need to dup memory */
		request->request = (char *)request_string;
		if ( session ) {
			request->session_uuid = switch_core_session_get_uuid(session);
		}
	} else {
		/* fire and forget... need to dup memory */
		request->request = strdup(request_string);
		if ( session ) {
			request->session_uuid = strdup(switch_core_session_get_uuid(session));
		}
	}

	return hiredis_profile_execute_pipeline_request(profile, session, request);
}

/**
 * Execute pipelined eval and wait for response.
 * @param profile to use
 * @param session (optional)
 * @param resp (optional) - if no resp, this function will not wait for the result
 * @param script
 * @param num_keys
 * @param keys
 * @return status SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t hiredis_profile_eval_pipeline(hiredis_profile_t *profile, switch_core_session_t *session, char **resp, const char *script, int num_keys, const char *keys)
{
	void *val = NULL;
	hiredis_request_t *request = NULL;

	if (switch_queue_trypop(profile->request_pool, &val) == SWITCH_STATUS_SUCCESS && val) {
		request = (hiredis_request_t *)val;
	} else {
		request = switch_core_alloc(profile->pool, sizeof(*request));
		switch_thread_cond_create(&request->cond, profile->pool);
		switch_mutex_init(&request->mutex, SWITCH_MUTEX_UNNESTED, profile->pool);
	}
	request->response = resp;
	request->done = 0;
	request->do_eval = 1;
	request->num_keys = num_keys;
	request->status = SWITCH_STATUS_SUCCESS;
	request->next = NULL;
	request->session_uuid = NULL;
	request->argc = 0;
	if ( resp ) {
		/* will block, no need to dup memory */
		request->request = (char *)script;
		request->keys = (char *)keys;
		if ( session ) {
			request->session_uuid = switch_core_session_get_uuid(session);
		}
	} else {
		/* fire and forget... need to dup memory */
		request->request = strdup(script);
		request->keys = strdup(keys);
		if ( session ) {
			request->session_uuid = strdup(switch_core_session_get_uuid(session));
		}
	}

	return hiredis_profile_execute_pipeline_request(profile, session, request);
}

/**
 * Execute pipelined request and wait for response.
 * @param profile to use
 * @param session (optional)
 * @param resp (optional) - if no resp, this function will not wait for the result
 * @param format_string - the request
 * @return status SWITCH_STATUS_SUCCESS if successful
 */
switch_status_t hiredis_profile_execute_pipeline_printf(hiredis_profile_t *profile, switch_core_session_t *session, char **resp, const char *format_string, ...)
{
	switch_status_t result = SWITCH_STATUS_GENERR;
	char *request = NULL;
	va_list ap;
	int ret;

	va_start(ap, format_string);
	ret = switch_vasprintf(&request, format_string, ap);
	va_end(ap);

	if ( ret != -1 ) {
		result = hiredis_profile_execute_pipeline(profile, session, resp, request);
	}
	switch_safe_free(request);
	return result;
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

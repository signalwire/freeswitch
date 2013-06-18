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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Joseph Sullivan <jossulli@amazon.com>
 *
 *
 * switch_core_session.c -- Main Core Library (session routines)
 *
 */

#include "switch.h"
#include "switch_core.h"
#include "private/switch_core_pvt.h"

struct switch_session_manager session_manager;

SWITCH_DECLARE(void) switch_core_session_set_dmachine(switch_core_session_t *session, switch_ivr_dmachine_t *dmachine, switch_digit_action_target_t target)
{
	int i = (int) target;

	if (i == 0 || i == 1) {
		if (dmachine) {
			switch_ivr_dmachine_set_target(dmachine, target);
		}
		session->dmachine[i] = dmachine;
	}
}

SWITCH_DECLARE(switch_ivr_dmachine_t *) switch_core_session_get_dmachine(switch_core_session_t *session, switch_digit_action_target_t target)
{
	int i = (int) target;

	if (i == 0 || i == 1) {	
		return session->dmachine[i];
	}

	return NULL;
}


SWITCH_DECLARE(stfu_instance_t *) switch_core_session_get_jb(switch_core_session_t *session, switch_media_type_t type)
{
	if (session->endpoint_interface->io_routines->get_jb) {
		return session->endpoint_interface->io_routines->get_jb(session, type);
	}

	return NULL;
}

SWITCH_DECLARE(void) switch_core_session_soft_lock(switch_core_session_t *session, uint32_t sec)
{
	session->soft_lock = sec;
}

SWITCH_DECLARE(void) switch_core_session_soft_unlock(switch_core_session_t *session)
{
	session->soft_lock = 0;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_codec_slin(switch_core_session_t *session, switch_slin_data_t *data)
													
{
	switch_codec_implementation_t read_impl = { 0 };
	int interval;

	switch_core_session_get_read_impl(session, &read_impl);
	interval = read_impl.microseconds_per_packet / 1000;
	data->session = session;

	if (switch_core_codec_init(&data->codec,
							   "L16",
							   NULL,
							   read_impl.actual_samples_per_second,
							   interval,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, NULL) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session),
						  SWITCH_LOG_DEBUG, "Codec Activated L16@%uhz %dms\n", read_impl.actual_samples_per_second, interval);

		memset(&data->write_frame, 0, sizeof(data->write_frame));

		data->write_frame.codec = &data->codec;
		data->write_frame.data = data->frame_data;
		data->write_frame.buflen = sizeof(data->frame_data);
		data->write_frame.datalen = 0;
		switch_core_session_set_read_codec(session, &data->codec);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_core_session_t *) switch_core_session_perform_locate(const char *uuid_str, const char *file, const char *func, int line)
{
	switch_core_session_t *session = NULL;

	if (uuid_str) {
		switch_mutex_lock(runtime.session_hash_mutex);
		if ((session = switch_core_hash_find(session_manager.session_table, uuid_str))) {
			/* Acquire a read lock on the session */
#ifdef SWITCH_DEBUG_RWLOCKS
			if (switch_core_session_perform_read_lock(session, file, func, line) != SWITCH_STATUS_SUCCESS) {
#if EMACS_CC_MODE_IS_BUGGY
			}
#endif
#else
			if (switch_core_session_read_lock(session) != SWITCH_STATUS_SUCCESS) {
#endif
				/* not available, forget it */
				session = NULL;
			}
		}
		switch_mutex_unlock(runtime.session_hash_mutex);
	}

	/* if its not NULL, now it's up to you to rwunlock this */
	return session;
}





SWITCH_DECLARE(switch_core_session_t *) switch_core_session_perform_force_locate(const char *uuid_str, const char *file, const char *func, int line)
{
	switch_core_session_t *session = NULL;
	switch_status_t status;

	if (uuid_str) {
		switch_mutex_lock(runtime.session_hash_mutex);
		if ((session = switch_core_hash_find(session_manager.session_table, uuid_str))) {
			/* Acquire a read lock on the session */

			if (switch_test_flag(session, SSF_DESTROYED)) {
				status = SWITCH_STATUS_FALSE;
#ifdef SWITCH_DEBUG_RWLOCKS
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, uuid_str, SWITCH_LOG_ERROR, "%s %s Read lock FAIL\n",
								  switch_core_session_get_uuid(session), switch_channel_get_name(session->channel));
#endif
			} else {
				status = (switch_status_t) switch_thread_rwlock_tryrdlock(session->rwlock);
#ifdef SWITCH_DEBUG_RWLOCKS
				switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, uuid_str, SWITCH_LOG_ERROR, "%s %s Read lock ACQUIRED\n",
								  switch_core_session_get_uuid(session), switch_channel_get_name(session->channel));
#endif
			}

			if (status != SWITCH_STATUS_SUCCESS) {
				/* not available, forget it */
				session = NULL;
			}
		}
		switch_mutex_unlock(runtime.session_hash_mutex);
	}

	/* if its not NULL, now it's up to you to rwunlock this */
	return session;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_perform_get_partner(switch_core_session_t *session, switch_core_session_t **partner,
																		const char *file, const char *func, int line)
{
	const char *uuid;

	if ((uuid = switch_channel_get_partner_uuid(session->channel))) {
		if ((*partner = switch_core_session_perform_locate(uuid, file, func, line))) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	*partner = NULL;
	return SWITCH_STATUS_FALSE;
}


struct str_node {
	char *str;
	struct str_node *next;
};

SWITCH_DECLARE(uint32_t) switch_core_session_hupall_matching_var_ans(const char *var_name, const char *var_val, switch_call_cause_t cause, 
																	 switch_hup_type_t type)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	switch_memory_pool_t *pool;
	struct str_node *head = NULL, *np;
	uint32_t r = 0;

	switch_core_new_memory_pool(&pool);

	if (!var_val)
		return r;

	switch_mutex_lock(runtime.session_hash_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
				int ans = switch_channel_test_flag(switch_core_session_get_channel(session), CF_ANSWERED);
				if ((ans && (type & SHT_ANSWERED)) || (!ans && (type & SHT_UNANSWERED))) {
					np = switch_core_alloc(pool, sizeof(*np));
					np->str = switch_core_strdup(pool, session->uuid_str);
					np->next = head;
					head = np;
				}
				switch_core_session_rwunlock(session);
			}
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	for(np = head; np; np = np->next) {
		if ((session = switch_core_session_locate(np->str))) {
			const char *this_val;
			if (switch_channel_up_nosig(session->channel) &&
				(this_val = switch_channel_get_variable(session->channel, var_name)) && (!strcmp(this_val, var_val))) {			
				switch_channel_hangup(session->channel, cause);
				r++;
			}
			switch_core_session_rwunlock(session);
		}
	}

	switch_core_destroy_memory_pool(&pool);

	return r;
}


SWITCH_DECLARE(switch_console_callback_match_t *) switch_core_session_findall_matching_var(const char *var_name, const char *var_val)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	switch_memory_pool_t *pool;
	struct str_node *head = NULL, *np;
	switch_console_callback_match_t *my_matches = NULL;

	switch_core_new_memory_pool(&pool);

	if (!var_val)
		return NULL;

	switch_mutex_lock(runtime.session_hash_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
				np = switch_core_alloc(pool, sizeof(*np));
				np->str = switch_core_strdup(pool, session->uuid_str);
				np->next = head;
				head = np;
				switch_core_session_rwunlock(session);
			}
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	for(np = head; np; np = np->next) {
		if ((session = switch_core_session_locate(np->str))) {
			const char *this_val;
			if (switch_channel_up_nosig(session->channel) &&
				(this_val = switch_channel_get_variable_dup(session->channel, var_name, SWITCH_FALSE, -1)) && (!strcmp(this_val, var_val))) {			
				switch_console_push_match(&my_matches, (const char *) np->str);
			}
			switch_core_session_rwunlock(session);
		}
	}

	switch_core_destroy_memory_pool(&pool);


	return my_matches;
}

SWITCH_DECLARE(void) switch_core_session_hupall_endpoint(const switch_endpoint_interface_t *endpoint_interface, switch_call_cause_t cause)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	switch_memory_pool_t *pool;
    struct str_node *head = NULL, *np;
	
	switch_core_new_memory_pool(&pool);
	
	switch_mutex_lock(runtime.session_hash_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
				if (session->endpoint_interface == endpoint_interface) {
					np = switch_core_alloc(pool, sizeof(*np));
                    np->str = switch_core_strdup(pool, session->uuid_str);
                    np->next = head;
					head = np;
				}
				switch_core_session_rwunlock(session);
			}
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	for(np = head; np; np = np->next) {
		if ((session = switch_core_session_locate(np->str))) {
			switch_channel_hangup(session->channel, cause);
			switch_core_session_rwunlock(session);
		}
	}

	switch_core_destroy_memory_pool(&pool);

}

SWITCH_DECLARE(void) switch_core_session_hupall(switch_call_cause_t cause)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	switch_memory_pool_t *pool;
	struct str_node *head = NULL, *np;

	switch_core_new_memory_pool(&pool);


	switch_mutex_lock(runtime.session_hash_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
				np = switch_core_alloc(pool, sizeof(*np));
				np->str = switch_core_strdup(pool, session->uuid_str);
				np->next = head;
				head = np;
				switch_core_session_rwunlock(session);
			}
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	for(np = head; np; np = np->next) { 
		if ((session = switch_core_session_locate(np->str))) {
			switch_channel_hangup(session->channel, cause);
			switch_core_session_rwunlock(session);
		}
	}

	switch_core_destroy_memory_pool(&pool);

}


SWITCH_DECLARE(switch_console_callback_match_t *) switch_core_session_findall(void)
{
	switch_hash_index_t *hi;
	void *val;
	switch_core_session_t *session;
	switch_console_callback_match_t *my_matches = NULL;

	switch_mutex_lock(runtime.session_hash_mutex);
	for (hi = switch_hash_first(NULL, session_manager.session_table); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		if (val) {
			session = (switch_core_session_t *) val;
			if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
				switch_console_push_match(&my_matches, session->uuid_str);
				switch_core_session_rwunlock(session);
			}
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	return my_matches;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_message_send(const char *uuid_str, switch_core_session_message_t *message)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.session_hash_mutex);
	if ((session = switch_core_hash_find(session_manager.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_up_nosig(session->channel)) {
				status = switch_core_session_receive_message(session, message);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_event_send(const char *uuid_str, switch_event_t **event)
{
	switch_core_session_t *session = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_mutex_lock(runtime.session_hash_mutex);
	if ((session = switch_core_hash_find(session_manager.session_table, uuid_str)) != 0) {
		/* Acquire a read lock on the session or forget it the channel is dead */
		if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
			if (switch_channel_up_nosig(session->channel)) {
				status = switch_core_session_queue_event(session, event);
			}
			switch_core_session_rwunlock(session);
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	return status;
}


SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	return session->private_info;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_set_private(switch_core_session_t *session, void *private_info)
{
	switch_assert(session != NULL);
	session->private_info = private_info;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(int) switch_core_session_add_stream(switch_core_session_t *session, void *private_info)
{
	session->streams[session->stream_count++] = private_info;
	return session->stream_count - 1;
}

SWITCH_DECLARE(void *) switch_core_session_get_stream(switch_core_session_t *session, int index)
{
	return session->streams[index];
}


SWITCH_DECLARE(int) switch_core_session_get_stream_count(switch_core_session_t *session)
{
	return session->stream_count;
}

SWITCH_DECLARE(switch_call_cause_t) switch_core_session_outgoing_channel(switch_core_session_t *session, switch_event_t *var_event,
																		 const char *endpoint_name,
																		 switch_caller_profile_t *caller_profile,
																		 switch_core_session_t **new_session,
																		 switch_memory_pool_t **pool,
																		 switch_originate_flag_t flags, switch_call_cause_t *cancel_cause)
{
	switch_io_event_hook_outgoing_channel_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_endpoint_interface_t *endpoint_interface;
	switch_channel_t *channel = NULL;
	switch_caller_profile_t *outgoing_profile = caller_profile;
	switch_call_cause_t cause = SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL;
	const char *forwardvar;
	int forwardval = 70;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	}

	if (!endpoint_interface->io_routines->outgoing_channel) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Could not locate outgoing channel interface for %s\n", endpoint_name);
		return SWITCH_CAUSE_CHAN_NOT_IMPLEMENTED;
	}

	if (session) {
		channel = switch_core_session_get_channel(session);

		switch_assert(channel != NULL);

		forwardvar = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);
		if (!zstr(forwardvar)) {
			forwardval = atoi(forwardvar) - 1;
		}
		if (forwardval <= 0) {
			return SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR;
		}

		if (caller_profile) {
			const char *ecaller_id_name = NULL, *ecaller_id_number = NULL;

			if (!(flags & SOF_NO_EFFECTIVE_CID_NAME)) {
				ecaller_id_name = switch_channel_get_variable(channel, "effective_caller_id_name");
			}

			if (!(flags & SOF_NO_EFFECTIVE_CID_NUM)) {
				ecaller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number");
			}

			if (ecaller_id_name || ecaller_id_number) {
				outgoing_profile = switch_caller_profile_clone(session, caller_profile);

				if (ecaller_id_name) {
					outgoing_profile->caller_id_name = ecaller_id_name;
				}
				if (ecaller_id_number) {
					outgoing_profile->caller_id_number = ecaller_id_number;
				}
			}
		}
		if (!outgoing_profile) {
			outgoing_profile = switch_channel_get_caller_profile(channel);
		}
	}

	if ((cause =
		 endpoint_interface->io_routines->outgoing_channel(session, var_event, outgoing_profile, new_session, pool, flags,
														   cancel_cause)) != SWITCH_CAUSE_SUCCESS) {
		UNPROTECT_INTERFACE(endpoint_interface);
		return cause;
	}

	if (session) {
		for (ptr = session->event_hooks.outgoing_channel; ptr; ptr = ptr->next) {
			if ((status = ptr->outgoing_channel(session, var_event, caller_profile, *new_session, flags)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}

	if (!*new_session) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT,
						  "Outgoing method for endpoint: [%s] returned: [%s] but there is no new session!\n", endpoint_name,
						  switch_channel_cause2str(cause));
		UNPROTECT_INTERFACE(endpoint_interface);
		return SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	} else {
		switch_caller_profile_t *profile = NULL, *cloned_profile = NULL;
		switch_event_t *event;
		switch_channel_t *peer_channel = switch_core_session_get_channel(*new_session);
		const char *use_uuid;

		switch_assert(peer_channel);

		if (channel && switch_true(switch_channel_get_variable(channel, "session_copy_loglevel"))) {
			(*new_session)->loglevel = session->loglevel;
		}


		if ((use_uuid = switch_event_get_header(var_event, "origination_uuid"))) {
			use_uuid = switch_core_session_strdup(*new_session, use_uuid);
			if (switch_core_session_set_uuid(*new_session, use_uuid) == SWITCH_STATUS_SUCCESS) {
				switch_event_del_header(var_event, "origination_uuid");
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_DEBUG, "%s set UUID=%s\n", switch_channel_get_name(peer_channel),
								  use_uuid);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(*new_session), SWITCH_LOG_CRIT, "%s set UUID=%s FAILED\n",
								  switch_channel_get_name(peer_channel), use_uuid);
			}
		}

		if (channel) {
			const char *val;
			switch_codec_t *vid_read_codec = NULL, *read_codec = switch_core_session_get_read_codec(session);
			const char *ep, *max_forwards = switch_core_session_sprintf(session, "%d", forwardval);

			switch_channel_set_variable(peer_channel, SWITCH_MAX_FORWARDS_VARIABLE, max_forwards);

			profile = switch_channel_get_caller_profile(channel);

			vid_read_codec = switch_core_session_get_video_read_codec(session);

			if (read_codec && read_codec->implementation && switch_core_codec_ready(read_codec)) {
				char rc[80] = "", vrc[80] = "", tmp[160] = "";

				switch_codec2str(read_codec, rc, sizeof(rc));
				if (vid_read_codec && vid_read_codec->implementation && switch_core_codec_ready(vid_read_codec)) {
					vrc[0] = ',';
					switch_codec2str(vid_read_codec, vrc + 1, sizeof(vrc) - 1);
					switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_VIDEO_CODEC_VARIABLE, vrc + 1);
				}

				switch_snprintf(tmp, sizeof(tmp), "%s%s", rc, vrc);
				switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_CODEC_VARIABLE, tmp);
			} else if ((ep = switch_channel_get_variable(channel, "ep_codec_string"))) {
				switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_CODEC_VARIABLE, ep);
			}

			switch_channel_set_variable(peer_channel, SWITCH_ORIGINATOR_VARIABLE, switch_core_session_get_uuid(session));
			switch_channel_set_variable(peer_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(session));
			// Needed by 3PCC proxy so that aleg can find bleg to pass SDP to, when final ACK arrives.
			switch_channel_set_variable(channel, SWITCH_ORIGINATE_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(*new_session));

			if ((val = switch_channel_get_variable(channel, SWITCH_PROCESS_CDR_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_PROCESS_CDR_VARIABLE, val);
			}

			if ((val = switch_channel_get_variable(channel, SWITCH_R_SDP_VARIABLE))) {
				switch_channel_set_variable(peer_channel, SWITCH_B_SDP_VARIABLE, val);
			}

			if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
				if (switch_channel_test_cap(peer_channel, CC_BYPASS_MEDIA)) {
					switch_channel_set_flag(peer_channel, CF_PROXY_MODE);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
									  "%s does not support the proxy feature, disabling.\n", switch_channel_get_name(peer_channel));
					switch_channel_clear_flag(channel, CF_PROXY_MODE);
				}
			}

			if (switch_channel_test_flag(channel, CF_PROXY_MEDIA)) {
				if (switch_channel_test_cap(peer_channel, CC_PROXY_MEDIA)) {
					switch_channel_set_flag(peer_channel, CF_PROXY_MEDIA);
					if (switch_channel_test_flag(channel, CF_VIDEO)) {
						switch_channel_set_flag(peer_channel, CF_VIDEO);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
									  "%s does not support the proxy feature, disabling.\n", switch_channel_get_name(peer_channel));
					switch_channel_clear_flag(channel, CF_PROXY_MEDIA);
				}
			}

			if (switch_channel_test_flag(channel, CF_ZRTP_PASSTHRU_REQ)) {
				switch_channel_set_flag(peer_channel, CF_ZRTP_PASSTHRU_REQ);
			}

			if (profile) {
				if ((cloned_profile = switch_caller_profile_clone(*new_session, profile)) != 0) {
					switch_channel_set_originator_caller_profile(peer_channel, cloned_profile);
				}
			}


			if ((profile = switch_channel_get_caller_profile(peer_channel))) {
				if ((cloned_profile = switch_caller_profile_clone(session, profile)) != 0) {
					switch_channel_set_origination_caller_profile(channel, cloned_profile);
				}
			}

		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_OUTGOING) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(peer_channel, event);
			switch_event_fire(&event);
		}
	}

	UNPROTECT_INTERFACE(endpoint_interface);
	return cause;
}

static const char *message_names[] = {
	"REDIRECT_AUDIO",
	"TRANSMIT_TEXT",
	"ANSWER",
	"PROGRESS",
	"BRIDGE",
	"UNBRIDGE",
	"TRANSFER",
	"RINGING",
	"MEDIA",
	"NOMEDIA",
	"HOLD",
	"UNHOLD",
	"REDIRECT",
	"RESPOND",
	"BROADCAST",
	"MEDIA_REDIRECT",
	"DEFLECT",
	"VIDEO_REFRESH_REQ",
	"DISPLAY",
	"TRANSCODING_NECESSARY",
	"AUDIO_SYNC",
	"REQUEST_IMAGE_MEDIA",
	"UUID_CHANGE",
	"SIMPLIFY",
	"DEBUG_MEDIA",
	"PROXY_MEDIA",
	"APPLICATION_EXEC",
	"APPLICATION_EXEC_COMPLETE",
	"PHONE_EVENT",
	"T38_DESCRIPTION",
	"UDPTL_MODE",
	"CLEAR_PROGRESS",
	"JITTER_BUFFER",
	"RECOVERY_REFRESH",
	"SIGNAL_DATA",
	"MESSAGE",
	"INFO",
	"AUDIO_DATA",
	"BLIND_TRANSFER_RESPONSE",
	"STUN_ERROR",
	"INVALID"
};

SWITCH_DECLARE(switch_status_t) switch_core_session_perform_receive_message(switch_core_session_t *session,
																			switch_core_session_message_t *message,
																			const char *file, const char *func, int line)
{
	switch_io_event_hook_receive_message_t *ptr;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(session != NULL);

	if ((status = switch_core_session_read_lock_hangup(session)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}
	
	if (!message->_file) {
		message->_file = file;
	}

	if (!message->_func) {
		message->_func = func;
	}

	if (!message->_line) {
		message->_line = line;
	}

	if (message->message_id > SWITCH_MESSAGE_INVALID-1) {
		message->message_id = SWITCH_MESSAGE_INVALID-1;
	}

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, message->_file, message->_func, message->_line,
					  switch_core_session_get_uuid(session), SWITCH_LOG_DEBUG1, "%s receive message [%s]\n",
					  switch_channel_get_name(session->channel), message_names[message->message_id]);


	if (message->message_id == SWITCH_MESSAGE_INDICATE_CLEAR_PROGRESS) {
		switch_channel_clear_flag(session->channel, CF_EARLY_MEDIA);
	}

	if (message->message_id == SWITCH_MESSAGE_INDICATE_DISPLAY) {
		char *arg = NULL;

		if (zstr(message->string_array_arg[0]) && !zstr(message->string_arg)) {
			arg = switch_core_session_strdup(session, message->string_arg);
			switch_separate_string(arg, '|', (char **)message->string_array_arg, 2);			
		}

		if (!zstr(message->string_array_arg[0])) {
			switch_channel_set_variable(session->channel, "last_sent_callee_id_name", message->string_array_arg[0]);
		}

		if (!zstr(message->string_array_arg[1])) {
			switch_channel_set_variable(session->channel, "last_sent_callee_id_number", message->string_array_arg[1]);
		}
		

		if (switch_true(switch_channel_get_variable(session->channel, SWITCH_IGNORE_DISPLAY_UPDATES_VARIABLE))) {
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, message->_file, message->_func, message->_line,
							  switch_core_session_get_uuid(session), SWITCH_LOG_DEBUG1, "Ignoring display update.\n");
			status = SWITCH_STATUS_SUCCESS;
			goto end;
		}

	}

	if (switch_channel_down_nosig(session->channel) && message->message_id != SWITCH_MESSAGE_INDICATE_SIGNAL_DATA) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, message->_file, message->_func, message->_line,
						  switch_core_session_get_uuid(session), SWITCH_LOG_DEBUG, "%s skip receive message [%s] (channel is hungup already)\n",
						  switch_channel_get_name(session->channel), message_names[message->message_id]);
	
	} else {
		if (session->media_handle) {
			status = switch_core_media_receive_message(session, message);
		}
		if (status == SWITCH_STATUS_SUCCESS || message->message_id == SWITCH_MESSAGE_INDICATE_SIGNAL_DATA) {
			if (session->endpoint_interface->io_routines->receive_message) {
				status = session->endpoint_interface->io_routines->receive_message(session, message);
			}
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		for (ptr = session->event_hooks.receive_message; ptr; ptr = ptr->next) {
			if ((status = ptr->receive_message(session, message)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}


		if (message->message_id == SWITCH_MESSAGE_INDICATE_BRIDGE &&
			switch_channel_test_flag(session->channel, CF_CONFIRM_BLIND_TRANSFER)) {
			switch_core_session_t *other_session;
			const char *uuid = switch_channel_get_variable(session->channel, "blind_transfer_uuid");

			switch_channel_clear_flag(session->channel, CF_CONFIRM_BLIND_TRANSFER);

			if (!zstr(uuid) && (other_session = switch_core_session_locate(uuid))) {
				switch_core_session_message_t msg = { 0 };			
				msg.message_id = SWITCH_MESSAGE_INDICATE_BLIND_TRANSFER_RESPONSE;
				msg.from = __FILE__;
				msg.numeric_arg = 1;
				switch_core_session_receive_message(other_session, &msg);
				switch_core_session_rwunlock(other_session);
			}
		}
	}


	message->_file = NULL;
	message->_func = NULL;
	message->_line = 0;

	if (switch_channel_up_nosig(session->channel)) {
		if (message->message_id == SWITCH_MESSAGE_INDICATE_BRIDGE || message->message_id == SWITCH_MESSAGE_INDICATE_UNBRIDGE) {
			switch_core_media_bug_flush_all(session);
			switch_core_recovery_track(session);
		}

		switch (message->message_id) {
		case SWITCH_MESSAGE_REDIRECT_AUDIO:
		case SWITCH_MESSAGE_INDICATE_ANSWER:
		case SWITCH_MESSAGE_INDICATE_PROGRESS:
		case SWITCH_MESSAGE_INDICATE_BRIDGE:
		case SWITCH_MESSAGE_INDICATE_UNBRIDGE:
		case SWITCH_MESSAGE_INDICATE_TRANSFER:
		case SWITCH_MESSAGE_INDICATE_RINGING:
		case SWITCH_MESSAGE_INDICATE_MEDIA:
		case SWITCH_MESSAGE_INDICATE_NOMEDIA:
		case SWITCH_MESSAGE_INDICATE_HOLD:
		case SWITCH_MESSAGE_INDICATE_UNHOLD:
		case SWITCH_MESSAGE_INDICATE_REDIRECT:
		case SWITCH_MESSAGE_INDICATE_RESPOND:
		case SWITCH_MESSAGE_INDICATE_BROADCAST:
		case SWITCH_MESSAGE_INDICATE_MEDIA_REDIRECT:
		case SWITCH_MESSAGE_INDICATE_DEFLECT:
			switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);
			break;
		default:
			break;
		}
	}

  end:

	switch_core_session_free_message(&message);
	switch_core_session_rwunlock(session);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_pass_indication(switch_core_session_t *session, switch_core_session_message_types_t indication)
{
	switch_core_session_message_t msg = { 0 };
	switch_core_session_t *other_session;
	const char *uuid;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (((uuid = switch_channel_get_partner_uuid(channel))) && (other_session = switch_core_session_locate(uuid))) {
		msg.message_id = indication;
		msg.from = __FILE__;
		status = switch_core_session_receive_message(other_session, &msg);
		switch_core_session_rwunlock(other_session);
	} else {
		status = SWITCH_STATUS_FALSE;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_indication(switch_core_session_t *session, switch_core_session_message_types_t indication)
{
	switch_core_session_message_t *msg;

	if ((msg = malloc(sizeof(*msg)))) {
		memset(msg, 0, sizeof(*msg));
		msg->message_id = indication;
		msg->from = __FILE__;
		switch_set_flag(msg, SCSMF_DYNAMIC);
		switch_core_session_queue_message(session, msg);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_message(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	if (session->message_queue) {
		if (switch_queue_trypush(session->message_queue, message) == SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		}

		switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);

		switch_core_session_wake_session_thread(session);

	}

	return status;
}

SWITCH_DECLARE(void) switch_core_session_free_message(switch_core_session_message_t **message)
{
	switch_core_session_message_t *to_free = *message;
	int i;
	char *s;

	*message = NULL;

	if (switch_test_flag(to_free, SCSMF_DYNAMIC)) {
		s = (char *) to_free->string_arg;
		switch_safe_free(s);
		switch_safe_free(to_free->pointer_arg);

		for (i = 0; i < MESSAGE_STRING_ARG_MAX; i++) {
			s = (char *) to_free->string_array_arg[i];
			switch_safe_free(s);
		}

		switch_safe_free(to_free);
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_message(switch_core_session_t *session, switch_core_session_message_t **message)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	switch_assert(session != NULL);

	if (session->message_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->message_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*message = (switch_core_session_message_t *) pop;
			if ((*message)->delivery_time && (*message)->delivery_time > switch_epoch_time_now(NULL)) {
				switch_core_session_queue_message(session, *message);
				*message = NULL;
				status = SWITCH_STATUS_FALSE;
			}
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_flush_message(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;
	switch_core_session_message_t *message;

	switch_assert(session != NULL);


	if (session->message_queue) {
		while ((status = (switch_status_t) switch_queue_trypop(session->message_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			message = (switch_core_session_message_t *) pop;
			switch_ivr_process_indications(session, message);
			switch_core_session_free_message(&message);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_signal_data(switch_core_session_t *session, void *signal_data)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	if (session->signal_data_queue) {
		if (switch_queue_push(session->signal_data_queue, signal_data) == SWITCH_STATUS_SUCCESS) {
			status = SWITCH_STATUS_SUCCESS;
		}

		switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);

		switch_core_session_wake_session_thread(session);

	}
	
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_signal_data(switch_core_session_t *session, void **signal_data)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	switch_assert(session != NULL);

	if (session->signal_data_queue) {
		if ((status = (switch_status_t) switch_queue_trypop(session->signal_data_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*signal_data = pop;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_receive_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_io_event_hook_receive_event_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	/* Acquire a read lock on the session or forget it the channel is dead */
	if (switch_core_session_read_lock(session) == SWITCH_STATUS_SUCCESS) {
		if (switch_channel_up_nosig(session->channel)) {
			if (session->endpoint_interface->io_routines->receive_event) {
				status = session->endpoint_interface->io_routines->receive_event(session, *event);
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				for (ptr = session->event_hooks.receive_event; ptr; ptr = ptr->next) {
					if ((status = ptr->receive_event(session, *event)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}

			if (status == SWITCH_STATUS_BREAK) {
				status = SWITCH_STATUS_SUCCESS;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				switch_event_destroy(event);
			}
		}
		switch_core_session_rwunlock(session);
	}

	switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(session != NULL);

	if (session->event_queue) {
		if (switch_queue_trypush(session->event_queue, *event) == SWITCH_STATUS_SUCCESS) {
			*event = NULL;
			status = SWITCH_STATUS_SUCCESS;

			switch_core_session_wake_session_thread(session);
		}
	}

	return status;
}

SWITCH_DECLARE(uint32_t) switch_core_session_messages_waiting(switch_core_session_t *session)
{
	int x = 0;

	if (session->private_event_queue) {
		x += switch_queue_size(session->private_event_queue);
	}

	if (session->message_queue) {
		x += switch_queue_size(session->message_queue);
	}

	return x;
}

SWITCH_DECLARE(uint32_t) switch_core_session_event_count(switch_core_session_t *session)
{
	if (session->event_queue) {
		return switch_queue_size(session->event_queue);
	}

	return 0;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_event(switch_core_session_t *session, switch_event_t **event, switch_bool_t force)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;

	switch_assert(session != NULL);

	if (session->event_queue && (force || !switch_channel_test_flag(session->channel, CF_DIVERT_EVENTS))) {
		if ((status = (switch_status_t) switch_queue_trypop(session->event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*event = (switch_event_t *) pop;
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_queue_private_event(switch_core_session_t *session, switch_event_t **event, switch_bool_t priority)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_queue_t *queue;

	switch_assert(session != NULL);
	switch_assert(event != NULL);

	if (session->private_event_queue) {
		queue = priority ? session->private_event_queue_pri : session->private_event_queue;

		(*event)->event_id = SWITCH_EVENT_PRIVATE_COMMAND;
		if (switch_queue_trypush(queue, *event) == SWITCH_STATUS_SUCCESS) {
			*event = NULL;
			switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

#define check_media(session)											\
	{																	\
		if (switch_channel_test_flag(session->channel, CF_BROADCAST_DROP_MEDIA)) { \
			switch_channel_clear_flag(session->channel, CF_BROADCAST_DROP_MEDIA); \
			switch_ivr_nomedia(session->uuid_str, SMF_REBRIDGE);		\
		}																\
	}																	\

SWITCH_DECLARE(uint32_t) switch_core_session_private_event_count(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	uint32_t count = 0;

	if (session->private_event_queue) {

		if (!switch_channel_test_flag(channel, CF_EVENT_LOCK)) {
			count = switch_queue_size(session->private_event_queue);
		}

		if (!switch_channel_test_flag(channel, CF_EVENT_LOCK_PRI)) {
			count += switch_queue_size(session->private_event_queue_pri);
		}

		if (count == 0) {
			check_media(session);
		}
	}

	return count;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_dequeue_private_event(switch_core_session_t *session, switch_event_t **event)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	void *pop;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_queue_t *queue;

	if (session->private_event_queue) {
		if (switch_queue_size(session->private_event_queue_pri)) {
			queue = session->private_event_queue_pri;

			if (switch_channel_test_flag(channel, CF_EVENT_LOCK_PRI)) {
				return SWITCH_STATUS_FALSE;
			}
		} else {
			queue = session->private_event_queue;

			if (switch_channel_test_flag(channel, CF_EVENT_LOCK)) {
				return SWITCH_STATUS_FALSE;
			}
		}

		if ((status = (switch_status_t) switch_queue_trypop(queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			*event = (switch_event_t *) pop;
		} else {
			check_media(session);
		}
	}

	return status;
}

SWITCH_DECLARE(uint32_t) switch_core_session_flush_private_events(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int x = 0;
	void *pop;

	if (session->private_event_queue) {
		while ((status = (switch_status_t) switch_queue_trypop(session->private_event_queue_pri, &pop)) == SWITCH_STATUS_SUCCESS) {
			if (pop) {
				switch_event_t *event = (switch_event_t *) pop;
				switch_event_destroy(&event);
			}
			x++;
		}
		while ((status = (switch_status_t) switch_queue_trypop(session->private_event_queue, &pop)) == SWITCH_STATUS_SUCCESS) {
			if (pop) {
				switch_event_t *event = (switch_event_t *) pop;
				switch_event_destroy(&event);
			}
			x++;
		}
		check_media(session);
	}

	return x;
}

SWITCH_DECLARE(void) switch_core_session_reset(switch_core_session_t *session, switch_bool_t flush_dtmf, switch_bool_t reset_read_codec)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_size_t has;

	if (reset_read_codec) {
		switch_core_session_set_read_codec(session, NULL);
	}

	/* clear resamplers */
	switch_mutex_lock(session->resample_mutex);
	switch_resample_destroy(&session->read_resampler);
	switch_resample_destroy(&session->write_resampler);
	switch_mutex_unlock(session->resample_mutex);
	/* clear indications */
	switch_core_session_flush_message(session);

	/* wipe these, they will be recreated if need be */
	switch_mutex_lock(session->codec_write_mutex);
	switch_buffer_destroy(&session->raw_write_buffer);
	switch_mutex_unlock(session->codec_write_mutex);

	switch_mutex_lock(session->codec_read_mutex);
	switch_buffer_destroy(&session->raw_read_buffer);
	switch_mutex_unlock(session->codec_read_mutex);

	if (flush_dtmf) {
		while ((has = switch_channel_has_dtmf(channel))) {
			switch_channel_flush_dtmf(channel);
		}
	}

	switch_clear_flag(session, SSF_WARN_TRANSCODE);
	switch_ivr_deactivate_unicast(session);
	switch_channel_clear_flag(channel, CF_BREAK);
}


SWITCH_DECLARE(switch_channel_t *) switch_core_session_get_channel(switch_core_session_t *session)
{
	switch_assert(session->channel);
	return session->channel;
}

SWITCH_DECLARE(switch_mutex_t *) switch_core_session_get_mutex(switch_core_session_t *session)
{
	return session->mutex;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_wake_session_thread(switch_core_session_t *session)
{
	switch_status_t status;
	int tries = 0;

	/* If trylock fails the signal is already awake so we needn't bother ..... or do we????*/

 top:

	status = switch_mutex_trylock(session->mutex);
	
	if (status == SWITCH_STATUS_SUCCESS) {
		switch_thread_cond_signal(session->cond);
		switch_mutex_unlock(session->mutex);
	} else {
		if (switch_channel_state_thread_trylock(session->channel) == SWITCH_STATUS_SUCCESS) {
			/* We've beat them for sure, as soon as we release this lock, they will be checking their queue on the next line. */
			switch_channel_state_thread_unlock(session->channel);
		} else {
			/* What luck!  The channel has already started going to sleep *after* we checked if we need to wake it up.
			   It will miss any messages in its queue because they were inserted after *it* checked its queue.  (catch-22)
			   So, it's not asleep yet, but it's too late for us to be sure they know we want them to stay awake and check its queue again.
			   Now *we* need to sleep instead but just for 1ms so we can circle back and try again.
			   This is so rare (yet possible) to happen that we can be fairly certian it will not happen 2x in a row but we'll try 10x just in case.
			 */
			if (++tries < 10) {
				switch_cond_next();
				goto top;
			}
		}
	}

	return status;
}

SWITCH_DECLARE(void) switch_core_session_signal_state_change(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_io_event_hook_state_change_t *ptr;

	switch_core_session_wake_session_thread(session);

	if (session->endpoint_interface->io_routines->state_change) {
		status = session->endpoint_interface->io_routines->state_change(session);
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		for (ptr = session->event_hooks.state_change; ptr; ptr = ptr->next) {
			if ((status = ptr->state_change(session)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}
	switch_core_session_kill_channel(session, SWITCH_SIG_BREAK);
}

SWITCH_DECLARE(unsigned int) switch_core_session_running(switch_core_session_t *session)
{
	return switch_test_flag(session, SSF_THREAD_RUNNING) ? 1 : 0;
}

SWITCH_DECLARE(unsigned int) switch_core_session_started(switch_core_session_t *session)
{
	return switch_test_flag(session, SSF_THREAD_STARTED) ? 1 : 0;
}

SWITCH_DECLARE(int) switch_core_session_sync_clock(void)
{
	int doit = 0;

	switch_mutex_lock(runtime.session_hash_mutex);
	if (session_manager.session_count == 0) {
		doit = 1;
	} else {
		switch_set_flag((&runtime), SCF_SYNC_CLOCK_REQUESTED);
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	if (doit)  {
		switch_time_sync();
	}

	return doit;

}

SWITCH_DECLARE(void) switch_core_session_perform_destroy(switch_core_session_t **session, const char *file, const char *func, int line)
{
	switch_memory_pool_t *pool;
	switch_event_t *event;
	switch_endpoint_interface_t *endpoint_interface = (*session)->endpoint_interface;
	int i;


	switch_core_session_flush_private_events(*session);

	if (switch_core_session_running(*session) && !switch_test_flag((*session), SSF_DESTROYABLE)) {
		switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, switch_core_session_get_uuid(*session), SWITCH_LOG_ERROR,
						  "Cowardly ignoring an attempt to call destroy on a running session.\n");
	}

	switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, func, line, switch_core_session_get_uuid(*session), SWITCH_LOG_NOTICE, "Close Channel %s [%s]\n",
					  switch_channel_get_name((*session)->channel), switch_channel_state_name(switch_channel_get_state((*session)->channel)));


	switch_core_session_reset(*session, TRUE, SWITCH_TRUE);

	switch_core_media_bug_remove_all(*session);
	switch_ivr_deactivate_unicast(*session);

	switch_scheduler_del_task_group((*session)->uuid_str);

	switch_mutex_lock(runtime.session_hash_mutex);
	switch_core_hash_delete(session_manager.session_table, (*session)->uuid_str);
	if (session_manager.session_count) {
		session_manager.session_count--;
		if (session_manager.session_count == 0) {
			if (switch_test_flag((&runtime), SCF_SYNC_CLOCK_REQUESTED)) {
				switch_time_sync();
				switch_clear_flag((&runtime), SCF_SYNC_CLOCK_REQUESTED);
			}
		}
	}
	switch_mutex_unlock(runtime.session_hash_mutex);

	if ((*session)->plc) {
		plc_free((*session)->plc);
		(*session)->plc = NULL;
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_DESTROY) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data((*session)->channel, event);
		switch_event_fire(&event);
	}

	switch_core_session_destroy_state(*session);

	switch_buffer_destroy(&(*session)->raw_read_buffer);
	switch_buffer_destroy(&(*session)->raw_write_buffer);
	switch_ivr_clear_speech_cache(*session);
	switch_channel_uninit((*session)->channel);

	for (i = 0; i < 2; i++) {
		if ((*session)->dmachine[i]) {
			switch_ivr_dmachine_destroy(&(*session)->dmachine[i]);
		}
	}

	pool = (*session)->pool;
	//#ifndef NDEBUG
	//memset(*session, 0, sizeof(switch_core_session_t));
	//#endif
	*session = NULL;
	switch_core_destroy_memory_pool(&pool);

	UNPROTECT_INTERFACE(endpoint_interface);
}



SWITCH_STANDARD_SCHED_FUNC(sch_heartbeat_callback)
{
	switch_event_t *event;
	switch_core_session_t *session;
	char *uuid = task->cmd_arg;

	if ((session = switch_core_session_locate(uuid))) {
		switch_event_create(&event, SWITCH_EVENT_SESSION_HEARTBEAT);
		switch_channel_event_set_data(session->channel, event);
		switch_event_fire(&event);

		/* reschedule this task */
		task->runtime = switch_epoch_time_now(NULL) + session->track_duration;

		switch_core_session_rwunlock(session);
	}
}

SWITCH_DECLARE(void) switch_core_session_unsched_heartbeat(switch_core_session_t *session)
{
	if (session->track_id) {
		switch_scheduler_del_task_id(session->track_id);
		session->track_id = 0;
	}
}

SWITCH_DECLARE(void) switch_core_session_sched_heartbeat(switch_core_session_t *session, uint32_t seconds)
{

	switch_core_session_unsched_heartbeat(session);
	session->track_id = switch_scheduler_add_task(switch_epoch_time_now(NULL), sch_heartbeat_callback, (char *) __SWITCH_FUNC__,
												  switch_core_session_get_uuid(session), 0, strdup(switch_core_session_get_uuid(session)), SSHF_FREE_ARG);
}

SWITCH_DECLARE(void) switch_core_session_enable_heartbeat(switch_core_session_t *session, uint32_t seconds)
{
	switch_assert(session != NULL);

	if (!seconds) {
		seconds = 60;
	}

	session->track_duration = seconds;

	if (switch_channel_test_flag(session->channel, CF_PROXY_MODE)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s using scheduler due to bypass_media mode\n",
						  switch_channel_get_name(session->channel));
		switch_core_session_sched_heartbeat(session, seconds);
		return;
	}

	switch_core_session_unsched_heartbeat(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "%s setting session heartbeat to %u second(s).\n",
					  switch_channel_get_name(session->channel), seconds);

	session->read_frame_count = 0;

}

SWITCH_DECLARE(void) switch_core_session_disable_heartbeat(switch_core_session_t *session)
{
	switch_core_session_unsched_heartbeat(session);
	switch_assert(session != NULL);
	session->read_frame_count = 0;
	session->track_duration = 0;

}

SWITCH_DECLARE(switch_bool_t) switch_core_session_in_thread(switch_core_session_t *session)
{
	return switch_thread_equal(switch_thread_self(), session->thread_id) ? SWITCH_TRUE : SWITCH_FALSE;
}

static void *SWITCH_THREAD_FUNC switch_core_session_thread(switch_thread_t *thread, void *obj)
{
	switch_core_session_t *session = obj;
	switch_event_t *event;
	char *event_str = NULL;
	const char *val;

	session->thread = thread;
	session->thread_id = switch_thread_self();

	switch_core_session_run(session);
	switch_core_media_bug_remove_all(session);

	if (session->soft_lock) {
		uint32_t loops = session->soft_lock * 10;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Session %" SWITCH_SIZE_T_FMT " (%s) Soft-Locked, "
						  "Waiting %u for external entities\n",
						  session->id, switch_channel_get_name(session->channel), session->soft_lock);

		while(--loops > 0) {
			if (!session->soft_lock) break;
			switch_yield(100000);
		}

	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Session %" SWITCH_SIZE_T_FMT " (%s) Locked, Waiting on external entities\n",
					  session->id, switch_channel_get_name(session->channel));
	switch_core_session_write_lock(session);
	switch_set_flag(session, SSF_DESTROYED);

	if ((val = switch_channel_get_variable(session->channel, "memory_debug")) && switch_true(val)) {
		if (switch_event_create(&event, SWITCH_EVENT_GENERAL) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(session->channel, event);
			switch_event_serialize(event, &event_str, SWITCH_FALSE);
			switch_assert(event_str);
			switch_core_memory_pool_tag(switch_core_session_get_pool(session), switch_core_session_strdup(session, event_str));
			free(event_str);
			switch_event_destroy(&event);
		}
	}

	switch_core_session_rwunlock(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Session %" SWITCH_SIZE_T_FMT " (%s) Ended\n",
					  session->id, switch_channel_get_name(session->channel));

	switch_set_flag(session, SSF_DESTROYABLE);
	switch_core_session_destroy(&session);
	return NULL;
}

typedef struct switch_thread_pool_node_s {
	switch_memory_pool_t *pool;
} switch_thread_pool_node_t;

static void *SWITCH_THREAD_FUNC switch_core_session_thread_pool_worker(switch_thread_t *thread, void *obj)
{
	switch_thread_pool_node_t *node = (switch_thread_pool_node_t *) obj;
	switch_memory_pool_t *pool = node->pool;
	void *pop;
	int check = 0;

	switch_mutex_lock(session_manager.mutex);
	session_manager.running++;
	switch_mutex_unlock(session_manager.mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Worker Thread %ld Started\n", (long) thread);

	while(session_manager.ready) {
		switch_status_t check_status;

		pop = NULL;

		if (check) {
			check_status = switch_queue_trypop(session_manager.thread_queue, &pop);
		} else {
			switch_mutex_lock(session_manager.mutex);
			session_manager.popping++;
			switch_mutex_unlock(session_manager.mutex);
			
			check_status = switch_queue_pop(session_manager.thread_queue, &pop);

			switch_mutex_lock(session_manager.mutex);
			session_manager.popping--;
			switch_mutex_unlock(session_manager.mutex);
		}

		if (check_status == SWITCH_STATUS_SUCCESS && pop) {
			switch_thread_data_t *td = (switch_thread_data_t *) pop;
			
			if (!td) break;

			switch_mutex_lock(session_manager.mutex);
			session_manager.busy++;
			switch_mutex_unlock(session_manager.mutex);
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Worker Thread %ld Processing\n", (long) thread);


			td->func(thread, td->obj);

			if (td->alloc) {
				free(td);
			}
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Worker Thread %ld Done Processing\n", (long) thread);
			
			switch_mutex_lock(session_manager.mutex);
			session_manager.busy--;
			switch_mutex_unlock(session_manager.mutex);

		} else {
			if (check) {
				break;
			}
			check++;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "Worker Thread %ld Ended\n", (long) thread);

	switch_mutex_lock(session_manager.mutex);
	session_manager.running--;
	switch_mutex_unlock(session_manager.mutex);

	switch_core_destroy_memory_pool(&pool);

	return NULL;
}

static void thread_launch_failure(void)
{
	uint32_t sess_count;

	switch_mutex_lock(session_manager.mutex);

	sess_count = switch_core_session_count();

	if (sess_count > 110) {

		switch_core_session_limit(sess_count - 10);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "LUKE: I'm hit, but not bad.\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
						  "LUKE'S VOICE: Artoo, see what you can do with it. Hang on back there....\n"
						  "Green laserfire moves past the beeping little robot as his head turns.  "
						  "After a few beeps and a twist of his mechanical arm,\n"
						  "Artoo reduces the max sessions to %d thus, saving the switch from certain doom.\n", sess_count - 10);

	}
	
	switch_mutex_unlock(session_manager.mutex);
}

static switch_status_t check_queue(void) 
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int ttl = 0;
	int x = 0;

	switch_mutex_lock(session_manager.mutex);
	ttl = switch_queue_size(session_manager.thread_queue);
	x = (session_manager.running - session_manager.busy);
	switch_mutex_unlock(session_manager.mutex);


	while (x < ttl) {
		switch_thread_t *thread;
		switch_threadattr_t *thd_attr;
		switch_memory_pool_t *pool;
		switch_thread_pool_node_t *node;
		
		switch_core_new_memory_pool(&pool);
		node = switch_core_alloc(pool, sizeof(*node));
		node->pool = pool;

		switch_threadattr_create(&thd_attr, node->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_threadattr_priority_set(thd_attr, SWITCH_PRI_LOW);

		if (switch_thread_create(&thread, thd_attr, switch_core_session_thread_pool_worker, node, node->pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Thread Failure!\n");
			switch_core_destroy_memory_pool(&pool);
			status = SWITCH_STATUS_GENERR;
			thread_launch_failure();
		} else {
			status = SWITCH_STATUS_SUCCESS;
		}
		x++;
	}

	return status;
}


static void *SWITCH_THREAD_FUNC switch_core_session_thread_pool_manager(switch_thread_t *thread, void *obj)
{
	int x = 0;

	while(session_manager.ready) {
		switch_yield(100000);

		if (++x == 300) {
			if (session_manager.popping) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, 
								  "Thread pool: running:%d busy:%d popping:%d\n", session_manager.running, session_manager.busy, session_manager.popping);

				switch_queue_interrupt_all(session_manager.thread_queue);

				x--;

				continue;
			} else {
				x = 0;
			}
		}

		check_queue();
	}

	while(session_manager.running) {
		switch_queue_interrupt_all(session_manager.thread_queue);
		switch_yield(20000);
	}

	
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_thread_pool_launch_thread(switch_thread_data_t **tdp)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_thread_data_t *td;

	switch_assert(tdp);

	td = *tdp;
	*tdp = NULL;

	switch_queue_push(session_manager.thread_queue, td);
	check_queue();

	return status;	
}

SWITCH_DECLARE(switch_status_t) switch_core_session_thread_pool_launch(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_INUSE;
	switch_thread_data_t *td;

	switch_mutex_lock(session->mutex);
	if (switch_test_flag(session, SSF_THREAD_RUNNING)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Cannot double-launch thread!\n");
	} else if (switch_test_flag(session, SSF_THREAD_STARTED)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Cannot launch thread again after it has already been run!\n");
	} else {
		status = SWITCH_STATUS_SUCCESS;
		switch_set_flag(session, SSF_THREAD_RUNNING);
		switch_set_flag(session, SSF_THREAD_STARTED);
		td = switch_core_session_alloc(session, sizeof(*td));
		td->obj = session;
		td->func = switch_core_session_thread;
		switch_queue_push(session_manager.thread_queue, td);
		check_queue();
	}
	switch_mutex_unlock(session->mutex);

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_thread_launch(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr;
	
	if (switch_test_flag(session, SSF_THREAD_RUNNING) || switch_test_flag(session, SSF_THREAD_STARTED)) {
		status = SWITCH_STATUS_INUSE;
		goto end;
	}


	if (switch_test_flag((&runtime), SCF_SESSION_THREAD_POOL)) {
		return switch_core_session_thread_pool_launch(session);
	}
	
	switch_mutex_lock(session->mutex);

	if (switch_test_flag(session, SSF_THREAD_RUNNING)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Cannot double-launch thread!\n");
	} else if (switch_test_flag(session, SSF_THREAD_STARTED)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Cannot launch thread again after it has already been run!\n");
	} else {
		switch_set_flag(session, SSF_THREAD_RUNNING);
		switch_set_flag(session, SSF_THREAD_STARTED);

		switch_threadattr_create(&thd_attr, session->pool);
		switch_threadattr_detach_set(thd_attr, 1);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

		if (switch_thread_create(&thread, thd_attr, switch_core_session_thread, session, session->pool) == SWITCH_STATUS_SUCCESS) {
			switch_set_flag(session, SSF_THREAD_STARTED);
			status = SWITCH_STATUS_SUCCESS;
		} else {
			switch_clear_flag(session, SSF_THREAD_RUNNING);
			switch_clear_flag(session, SSF_THREAD_STARTED);	
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Cannot create thread!\n");
			thread_launch_failure();
		}
	}

	switch_mutex_unlock(session->mutex);

 end:

	return status;
}

SWITCH_DECLARE(void) switch_core_session_launch_thread(switch_core_session_t *session, switch_thread_start_t func, void *obj)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_threadattr_create(&thd_attr, session->pool);
	switch_threadattr_detach_set(thd_attr, 1);

	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	if (switch_thread_create(&thread, thd_attr, func, obj, session->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Cannot create thread!\n");
		thread_launch_failure();
	}

}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_uuid(switch_core_session_t *session, const char *use_uuid)
{
	switch_event_t *event;
	switch_core_session_message_t msg = { 0 };
	switch_caller_profile_t *profile;

	switch_assert(use_uuid);

	if (!strcmp(use_uuid, session->uuid_str)) {
		return SWITCH_STATUS_SUCCESS;
	}


	switch_mutex_lock(runtime.session_hash_mutex);
	if (switch_core_hash_find(session_manager.session_table, use_uuid)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Duplicate UUID!\n");
		switch_mutex_unlock(runtime.session_hash_mutex);
		return SWITCH_STATUS_FALSE;
	}

	msg.message_id = SWITCH_MESSAGE_INDICATE_UUID_CHANGE;
	msg.from = switch_channel_get_name(session->channel);
	msg.string_array_arg[0] = session->uuid_str;
	msg.string_array_arg[1] = use_uuid;
	switch_core_session_receive_message(session, &msg);

	if ((profile = switch_channel_get_caller_profile(session->channel))) {
		profile->uuid = switch_core_strdup(profile->pool, use_uuid);
	}

	switch_channel_set_variable(session->channel, "uuid", use_uuid);
	switch_channel_set_variable(session->channel, "call_uuid", use_uuid);

	switch_event_create(&event, SWITCH_EVENT_CHANNEL_UUID);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Old-Unique-ID", session->uuid_str);
	switch_core_hash_delete(session_manager.session_table, session->uuid_str);
	switch_set_string(session->uuid_str, use_uuid);
	switch_core_hash_insert(session_manager.session_table, session->uuid_str, session);
	switch_mutex_unlock(runtime.session_hash_mutex);
	switch_channel_event_set_data(session->channel, event);
	switch_event_fire(&event);


	return SWITCH_STATUS_SUCCESS;
}

static char *xml_find_var(switch_xml_t vars, const char *name)
{
	switch_xml_t var;
	if ((var = switch_xml_child(vars, name)) && var->txt) {
		return var->txt;
	}

	return NULL;
}

static void parse_array(const char *str, uint32_t *array, int32_t array_len)
{
	char *p, *v, *dup, *next = NULL;

	if (zstr(str)) {
		return;
	}

	dup = strdup(str);

	p = dup;
	while (p) {
		if ((next = strchr(p, ';'))) {
			*next++ = '\0';
		}

		if ((v = strchr(p, '='))) {
			*v++ = '\0';
		}

		if (p && v) {
			int x = 0, y = 0;

			x = atoi(p);
			y = atoi(v);

			if (x < array_len) {
				array[x] = y;
			}
		}

		p = next;

	}

	free(dup);
}

SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request_xml(switch_endpoint_interface_t *endpoint_interface,
																		switch_memory_pool_t **pool, switch_xml_t xml)
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_xml_t tag, tag2, tag3, vars, callflow;
	switch_call_direction_t direction = SWITCH_CALL_DIRECTION_OUTBOUND;
	char *flag_str = NULL, *cap_str = NULL, *direction_s = NULL, *uuid = NULL;
	uint32_t flags[CF_FLAG_MAX] = { 0 };
	uint32_t caps[CC_FLAG_MAX] = { 0 };
	int i;

	vars = switch_xml_child(xml, "variables");
	uuid = xml_find_var(vars, "uuid");

	if ((tag = switch_xml_child(xml, "channel_data"))) {
		direction_s = xml_find_var(tag, "direction");
		direction = !strcmp(direction_s, "outbound") ? SWITCH_CALL_DIRECTION_OUTBOUND : SWITCH_CALL_DIRECTION_INBOUND;
		flag_str = xml_find_var(tag, "flags");
		cap_str = xml_find_var(tag, "caps");
	}

	parse_array(flag_str, flags, CF_FLAG_MAX);
	parse_array(cap_str, caps, CC_FLAG_MAX);

	flags[CF_RECOVERING] = 0;
	flags[CF_RECOVERING_BRIDGE] = 0;
	flags[CF_TRACKED] = 0;
	flags[CF_TRANSFER] = 0;
	flags[CF_ACCEPT_CNG] = 0;
	flags[CF_REDIRECT] = 0;
	flags[CF_BRIDGED] = 0;
	flags[CF_HOLD] = 0;
	flags[CF_SERVICE] = 0;
	flags[CF_TAGGED] = 0;
	flags[CF_WINNER] = 0;
	flags[CF_EARLY_OK] = 0;
	flags[CF_CONTROLLED] = 0;
	flags[CF_SUSPEND] = 0;
	flags[CF_EVENT_PARSE] = 0;
	flags[CF_GEN_RINGBACK] = 0;
	flags[CF_BREAK] = 0;
	flags[CF_BROADCAST] = 0;
	flags[CF_UNICAST] = 0;
	flags[CF_EVENT_LOCK] = 0;
	flags[CF_EVENT_LOCK_PRI] = 0;
	flags[CF_RESET] = 0;
	flags[CF_ORIGINATING] = 0;
	flags[CF_STOP_BROADCAST] = 0;
	flags[CF_INNER_BRIDGE] = 0;
	flags[CF_REQ_MEDIA] = 0;
	flags[CF_PAUSE_BUGS] = 0;
	flags[CF_DIVERT_EVENTS] = 0;
	flags[CF_BLOCK_STATE] = 0;
	flags[CF_FS_RTP] = 0;
	flags[CF_REPORTING] = 0;
	flags[CF_PARK] = 0;
	flags[CF_TIMESTAMP_SET] = 0;
	flags[CF_ORIGINATOR] = 0;
	flags[CF_XFER_ZOMBIE] = 0;
	flags[CF_MEDIA_ACK] = 0;
	flags[CF_THREAD_SLEEPING] = 0;
	flags[CF_DISABLE_RINGBACK] = 0;
	flags[CF_NOT_READY] = 0;
	flags[CF_SIGNAL_BRIDGE_TTL] = 0;
	flags[CF_MEDIA_BRIDGE_TTL] = 0;
	flags[CF_BYPASS_MEDIA_AFTER_BRIDGE] = 0;
	flags[CF_LEG_HOLDING] = 0;
	flags[CF_BROADCAST_DROP_MEDIA] = 0;
	flags[CF_EARLY_HANGUP] = 0;
	flags[CF_MEDIA_SET] = 0;
	flags[CF_CONSUME_ON_ORIGINATE] = 0;
	flags[CF_PASSTHRU_PTIME_MISMATCH] = 0;
	flags[CF_BRIDGE_NOWRITE] = 0;
	flags[CF_RECOVERED] = 0;
	flags[CF_JITTERBUFFER] = 0;
	flags[CF_JITTERBUFFER_PLC] = 0;
	flags[CF_DIALPLAN] = 0;
	flags[CF_BLOCK_BROADCAST_UNTIL_MEDIA] = 0;
	flags[CF_CNG_PLC] = 0;
	flags[CF_ATTENDED_TRANSFER] = 0;
	flags[CF_LAZY_ATTENDED_TRANSFER] = 0;
	flags[CF_SIGNAL_DATA] = 0;
	flags[CF_SIMPLIFY] = 0;


	if (!(session = switch_core_session_request_uuid(endpoint_interface, direction, SOF_NO_LIMITS, pool, uuid))) {
		return NULL;
	}

	channel = switch_core_session_get_channel(session);

	for (i = 0; i < CF_FLAG_MAX; i++) {
		if (flags[i]) {
			switch_channel_set_flag_value(channel, i, flags[i]);
		}
	}

	for (i = 0; i < CC_FLAG_MAX; i++) {
		if (caps[i]) {
			switch_channel_set_cap_value(channel, i, caps[i]);
		}
	}

	if ((tag2 = switch_xml_child(xml, "variables"))) {
		for (tag = tag2->child; tag; tag = tag->sibling) {
			if (tag->name && tag->txt) {
				char *p = strdup(tag->txt);
				char *val = p;
				switch_url_decode(val);
				switch_channel_set_variable(channel, tag->name, val);
				if (!strcasecmp(tag->name, "channel_name")) {
					switch_channel_set_name(channel, val);
				}
				free(p);
			}
		}
	}
	
	if ((callflow = switch_xml_child(xml, "callflow"))) {
		if ((tag2 = switch_xml_child(callflow, "caller_profile"))) {
			switch_caller_profile_t *caller_profile;
			char *tmp;

			caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
													   xml_find_var(tag2, "username"),
													   xml_find_var(tag2, "dialplan"),
													   xml_find_var(tag2, "caller_id_name"),
													   xml_find_var(tag2, "caller_id_number"),
													   xml_find_var(tag2, "network_addr"),
													   xml_find_var(tag2, "ani"),
													   xml_find_var(tag2, "aniii"),
													   xml_find_var(tag2, "rdnis"),
													   xml_find_var(tag2, "source"),
													   xml_find_var(tag2, "context"), xml_find_var(tag2, "destination_number"));

			if ((tmp = xml_find_var(tag2, "callee_id_name"))) {
				caller_profile->callee_id_name = switch_core_session_strdup(session, tmp);
			}

			if ((tmp = xml_find_var(tag2, "callee_id_number"))) {
				caller_profile->callee_id_number = switch_core_session_strdup(session, tmp);
			}

			if ((tag3 = switch_xml_child(callflow, "times"))) {
				caller_profile->times = (switch_channel_timetable_t *) switch_core_session_alloc(session, sizeof(*caller_profile->times));

				caller_profile->times->resurrected = switch_time_now();

				for (tag3 = tag3->child; tag3; tag3 = tag3->sibling) {
					int64_t v;

					if (tag3->name && tag3->txt) {
						v = atoll(tag3->txt);
						if (!strcmp(tag3->name, "created_time")) {
							caller_profile->times->created = v;
						} else if (!strcmp(tag3->name, "profile_created_time")) {
							caller_profile->times->profile_created = v;
						} else if (!strcmp(tag3->name, "progress_time")) {
							caller_profile->times->progress = v;
						} else if (!strcmp(tag3->name, "progress_media_time")) {
							caller_profile->times->progress_media = v;
						} else if (!strcmp(tag3->name, "answered_time")) {
							caller_profile->times->answered = v;
						} else if (!strcmp(tag3->name, "hangup_time")) {
							caller_profile->times->hungup = v;
						} else if (!strcmp(tag3->name, "transfer_time")) {
							caller_profile->times->transferred = v;
						}
					}

				}
			}

			switch_channel_set_caller_profile(channel, caller_profile);
			if ((tag = switch_xml_child(tag2, "originator")) && (tag = tag->child)) {
				caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
														   xml_find_var(tag, "username"),
														   xml_find_var(tag, "dialplan"),
														   xml_find_var(tag, "caller_id_name"),
														   xml_find_var(tag, "caller_id_number"),
														   xml_find_var(tag, "network_addr"),
														   xml_find_var(tag, "ani"),
														   xml_find_var(tag, "aniii"),
														   xml_find_var(tag, "rdnis"),
														   xml_find_var(tag, "source"),
														   xml_find_var(tag, "context"), xml_find_var(tag, "destination_number"));

				switch_channel_set_originator_caller_profile(channel, caller_profile);
			}

			if ((tag = switch_xml_child(tag2, "originatee")) && (tag = tag->child)) {
				caller_profile = switch_caller_profile_new(switch_core_session_get_pool(session),
														   xml_find_var(tag, "username"),
														   xml_find_var(tag, "dialplan"),
														   xml_find_var(tag, "caller_id_name"),
														   xml_find_var(tag, "caller_id_number"),
														   xml_find_var(tag, "network_addr"),
														   xml_find_var(tag, "ani"),
														   xml_find_var(tag, "aniii"),
														   xml_find_var(tag, "rdnis"),
														   xml_find_var(tag, "source"),
														   xml_find_var(tag, "context"), xml_find_var(tag, "destination_number"));

				switch_channel_set_originatee_caller_profile(channel, caller_profile);
			}

		}


		switch_channel_set_flag(channel, CF_RECOVERED);
	}


	if (!channel || !switch_channel_get_caller_profile(channel)) {
		if (session) {
			switch_core_session_destroy(&session);
		}
	}


	return session;
}



SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request_uuid(switch_endpoint_interface_t
																		 *endpoint_interface,
																		 switch_call_direction_t direction,
																		 switch_originate_flag_t originate_flags,
																		 switch_memory_pool_t **pool, const char *use_uuid)
{
	switch_memory_pool_t *usepool;
	switch_core_session_t *session;
	switch_uuid_t uuid;
	uint32_t count = 0;
	int32_t sps = 0;


	if (use_uuid && switch_core_hash_find(session_manager.session_table, use_uuid)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Duplicate UUID!\n");
		return NULL;
	}

	if (direction == SWITCH_CALL_DIRECTION_INBOUND && !switch_core_ready_inbound()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The system cannot create any inbound sessions at this time.\n");
		return NULL;
	}

	if (direction == SWITCH_CALL_DIRECTION_OUTBOUND && !switch_core_ready_outbound()) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The system cannot create any outbound sessions at this time.\n");
		return NULL;
	}

	if (!switch_core_ready() || endpoint_interface == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "The system cannot create any sessions at this time.\n");
		return NULL;
	}

	if (runtime.min_idle_time > 0 && runtime.profile_time < runtime.min_idle_time) {
		return NULL;
	}

	PROTECT_INTERFACE(endpoint_interface);

	if (!(originate_flags & SOF_NO_LIMITS)) {
		switch_mutex_lock(runtime.throttle_mutex);
		count = session_manager.session_count;
		sps = --runtime.sps;
		switch_mutex_unlock(runtime.throttle_mutex);
		
		if (sps <= 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Throttle Error! %d\n", session_manager.session_count);
			UNPROTECT_INTERFACE(endpoint_interface);
			return NULL;
		}

		if ((count + 1) > session_manager.session_limit) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Over Session Limit! %d\n", session_manager.session_limit);
			UNPROTECT_INTERFACE(endpoint_interface);
			return NULL;
		}
	}


	if (pool && *pool) {
		usepool = *pool;
		*pool = NULL;
	} else {
		switch_core_new_memory_pool(&usepool);
	}

	session = switch_core_alloc(usepool, sizeof(*session));
	session->pool = usepool;

	switch_core_memory_pool_set_data(session->pool, "__session", session);

	if (switch_channel_alloc(&session->channel, direction, session->pool) != SWITCH_STATUS_SUCCESS) {
		abort();
	}

	switch_channel_init(session->channel, session, CS_NEW, 0);

	if (direction == SWITCH_CALL_DIRECTION_OUTBOUND) {
		switch_channel_set_flag(session->channel, CF_OUTBOUND);
	}

	/* The session *IS* the pool you may not alter it because you have no idea how
	   its all private it will be passed to the thread run function */

	if (use_uuid) {
		switch_set_string(session->uuid_str, use_uuid);
	} else {
		switch_uuid_get(&uuid);
		switch_uuid_format(session->uuid_str, &uuid);
	}

	switch_channel_set_variable(session->channel, "uuid", session->uuid_str);
	switch_channel_set_variable(session->channel, "call_uuid", session->uuid_str);

	session->endpoint_interface = endpoint_interface;
	session->raw_write_frame.data = session->raw_write_buf;
	session->raw_write_frame.buflen = sizeof(session->raw_write_buf);
	session->raw_read_frame.data = session->raw_read_buf;
	session->raw_read_frame.buflen = sizeof(session->raw_read_buf);


	session->enc_write_frame.data = session->enc_write_buf;
	session->enc_write_frame.buflen = sizeof(session->enc_write_buf);
	session->enc_read_frame.data = session->enc_read_buf;
	session->enc_read_frame.buflen = sizeof(session->enc_read_buf);

	switch_mutex_init(&session->mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_mutex_init(&session->resample_mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_mutex_init(&session->codec_read_mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_mutex_init(&session->codec_write_mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_mutex_init(&session->frame_read_mutex, SWITCH_MUTEX_NESTED, session->pool);
	switch_thread_rwlock_create(&session->bug_rwlock, session->pool);
	switch_thread_cond_create(&session->cond, session->pool);
	switch_thread_rwlock_create(&session->rwlock, session->pool);
	switch_thread_rwlock_create(&session->io_rwlock, session->pool);
	switch_queue_create(&session->message_queue, SWITCH_MESSAGE_QUEUE_LEN, session->pool);
	switch_queue_create(&session->signal_data_queue, SWITCH_MESSAGE_QUEUE_LEN, session->pool);
	switch_queue_create(&session->event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	switch_queue_create(&session->private_event_queue, SWITCH_EVENT_QUEUE_LEN, session->pool);
	switch_queue_create(&session->private_event_queue_pri, SWITCH_EVENT_QUEUE_LEN, session->pool);

	switch_mutex_lock(runtime.session_hash_mutex);
	switch_core_hash_insert(session_manager.session_table, session->uuid_str, session);
	session->id = session_manager.session_id++;
	session_manager.session_count++;
	switch_mutex_unlock(runtime.session_hash_mutex);

	switch_channel_set_variable_printf(session->channel, "session_id", "%u", session->id);

	return session;
}

SWITCH_DECLARE(uint32_t) switch_core_session_count(void)
{
	return session_manager.session_count;
}

SWITCH_DECLARE(switch_size_t) switch_core_session_get_id(switch_core_session_t *session)
{
	return session->id;
}

SWITCH_DECLARE(switch_size_t) switch_core_session_id_dec(void)
{
	switch_mutex_lock(runtime.session_hash_mutex);
	session_manager.session_id--;
	switch_mutex_unlock(runtime.session_hash_mutex);
	return session_manager.session_id;
}

SWITCH_DECLARE(switch_size_t) switch_core_session_id(void)
{
	return session_manager.session_id;
}


SWITCH_DECLARE(switch_core_session_t *) switch_core_session_request_by_name(const char *endpoint_name,
																			switch_call_direction_t direction, switch_memory_pool_t **pool)
{
	switch_endpoint_interface_t *endpoint_interface;
	switch_core_session_t *session;

	if ((endpoint_interface = switch_loadable_module_get_endpoint_interface(endpoint_name)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not locate channel type %s\n", endpoint_name);
		return NULL;
	}

	session = switch_core_session_request(endpoint_interface, direction, SOF_NONE, pool);

	UNPROTECT_INTERFACE(endpoint_interface);

	return session;
}

#ifndef SWITCH_PREFIX_DIR
#define SWITCH_PREFIX_DIR "."
#endif

SWITCH_DECLARE(uint8_t) switch_core_session_compare(switch_core_session_t *a, switch_core_session_t *b)
{
	switch_assert(a != NULL);
	switch_assert(b != NULL);

	return (uint8_t) (a->endpoint_interface == b->endpoint_interface);
}

SWITCH_DECLARE(uint8_t) switch_core_session_check_interface(switch_core_session_t *session, const switch_endpoint_interface_t *endpoint_interface)
{
	switch_assert(session != NULL);
	switch_assert(endpoint_interface != NULL);

	return (uint8_t) (session->endpoint_interface == endpoint_interface);
}

SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session_t *session)
{
	return session->uuid_str;
}


SWITCH_DECLARE(uint32_t) switch_core_session_limit(uint32_t new_limit)
{
	if (new_limit) {
		session_manager.session_limit = new_limit;
	}

	return session_manager.session_limit;
}

SWITCH_DECLARE(double) switch_core_min_idle_cpu(double new_limit)
{
	if (new_limit >= 0) {
		runtime.min_idle_time = new_limit;
	}

	return runtime.min_idle_time;
}


SWITCH_DECLARE(double) switch_core_idle_cpu(void)
{
	return runtime.profile_time;
}

SWITCH_DECLARE(uint32_t) switch_core_sessions_per_second(uint32_t new_limit)
{
	if (new_limit) {
		runtime.sps_total = new_limit;
	}

	return runtime.sps_total;
}

void switch_core_session_init(switch_memory_pool_t *pool)
{
	memset(&session_manager, 0, sizeof(session_manager));
	session_manager.session_limit = 1000;
	session_manager.session_id = 1;
	session_manager.memory_pool = pool;
	switch_core_hash_init(&session_manager.session_table, session_manager.memory_pool);
	
	if (switch_test_flag((&runtime), SCF_SESSION_THREAD_POOL)) {
		switch_threadattr_t *thd_attr;

		switch_mutex_init(&session_manager.mutex, SWITCH_MUTEX_NESTED, session_manager.memory_pool);
		switch_queue_create(&session_manager.thread_queue, 100000, session_manager.memory_pool);
		switch_threadattr_create(&thd_attr, session_manager.memory_pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&session_manager.manager_thread, thd_attr, switch_core_session_thread_pool_manager, NULL, session_manager.memory_pool);		
		session_manager.ready = 1;
	}

}

void switch_core_session_uninit(void)
{
	int sanity = 100;
	switch_status_t st = SWITCH_STATUS_FALSE;

	switch_core_hash_destroy(&session_manager.session_table);
	session_manager.ready = 0;

	switch_thread_join(&st, session_manager.manager_thread);

	while(session_manager.running && --sanity > 0) {
		switch_queue_interrupt_all(session_manager.thread_queue);
		switch_yield(100000);
	}
	
}

SWITCH_DECLARE(switch_app_log_t *) switch_core_session_get_app_log(switch_core_session_t *session)
{
	return session->app_log;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_get_app_flags(const char *app, int32_t *flags)
{
	switch_application_interface_t *application_interface;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(flags);

	*flags = 0;

	if ((application_interface = switch_loadable_module_get_application_interface(app)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Application %s\n", app);
		goto end;
	} else if (application_interface->flags) {
		*flags = application_interface->flags;
		status = SWITCH_STATUS_SUCCESS;
	}

	UNPROTECT_INTERFACE(application_interface);

  end:

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_core_session_execute_application_async(switch_core_session_t *session, const char *app, const char *arg)
{
	switch_event_t *execute_event;
	char *ap, *arp;
	
	if (!arg && strstr(app, "::")) {
		ap = switch_core_session_strdup(session, app);
		app = ap;

		if ((arp = strstr(ap, "::"))) {
			*arp = '\0';
			arg = arp + 2;
		}
	}
	
	if (switch_event_create(&execute_event, SWITCH_EVENT_COMMAND) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "call-command", "execute");
		switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "execute-app-name", app);

		if (arg) {
			switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "execute-app-arg", arg);
		}
		
		if (!switch_channel_test_flag(session->channel, CF_PROXY_MODE)) {
			switch_channel_set_flag(session->channel, CF_BLOCK_BROADCAST_UNTIL_MEDIA);
		}

		switch_event_add_header_string(execute_event, SWITCH_STACK_BOTTOM, "event-lock", "true");
		switch_core_session_queue_private_event(session, &execute_event, SWITCH_FALSE);
		
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_core_session_execute_application_get_flags(switch_core_session_t *session, const char *app,
																				  const char *arg, int32_t *flags)
{
	switch_application_interface_t *application_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_channel_down_nosig(session->channel)) {
		char *p;
		if (!arg && (p = strstr(app, "::"))) {
			*p++ = '0';
			*p++ = '0';
			arg = p;

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s ASYNC CALL CONVERTED TO INLINE %s(%s)\n", 
							  switch_channel_get_name(session->channel), app, switch_str_nil(arg));			
		}
		
		if ((application_interface = switch_loadable_module_get_application_interface(app)) == 0) {
			return SWITCH_STATUS_FALSE;
		}

		if (switch_test_flag(application_interface, SAF_ZOMBIE_EXEC)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s ZOMBIE EXEC %s(%s)\n", 
							  switch_channel_get_name(session->channel), app, switch_str_nil(arg));
			goto exec;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
						  "%s Channel is hungup and application '%s' does not have the zombie_exec flag.\n",
						  switch_channel_get_name(session->channel), app);

		switch_goto_status(SWITCH_STATUS_IGNORE, done);
	}

	if (!arg && strstr(app, "::")) {
		return switch_core_session_execute_application_async(session, app, arg);
	}

	if ((application_interface = switch_loadable_module_get_application_interface(app)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Application %s\n", app);
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	if (!application_interface->application_function) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No Function for %s\n", app);
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		switch_goto_status(SWITCH_STATUS_FALSE, done);
	}

	if (flags && application_interface->flags) {
		*flags = application_interface->flags;
	}

	if (switch_channel_test_flag(session->channel, CF_PROXY_MODE) && !switch_test_flag(application_interface, SAF_SUPPORT_NOMEDIA)) {
		switch_ivr_media(session->uuid_str, SMF_NONE);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Application %s Requires media on channel %s!\n",
						  app, switch_channel_get_name(session->channel));
	} else if (!switch_test_flag(application_interface, SAF_SUPPORT_NOMEDIA) && !switch_channel_media_ready(session->channel)) {
		if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_INBOUND) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Application %s Requires media! pre_answering channel %s\n",
							  app, switch_channel_get_name(session->channel));
			if (switch_channel_pre_answer(session->channel) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Well, that didn't work very well did it? ...\n");
				switch_goto_status(SWITCH_STATUS_FALSE, done);
			}
		} else {
			uint32_t ready = 0, sanity = 2000;

			do {
				sanity--;
				ready = switch_channel_media_up(session->channel);
				switch_cond_next();
			} while(!ready && sanity);

			if (!ready) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, 
								  "Cannot execute app '%s' media required on an outbound channel that does not have media established\n", app);
				switch_goto_status(SWITCH_STATUS_FALSE, done);
			}
		}
	}

 exec:

	switch_core_session_exec(session, application_interface, arg);

  done:

	UNPROTECT_INTERFACE(application_interface);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_exec(switch_core_session_t *session,
														 const switch_application_interface_t *application_interface, const char *arg)
{
	switch_app_log_t *log, *lp;
	switch_event_t *event;
	const char *var;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *expanded = NULL;
	const char *app, *app_uuid_var;
	switch_core_session_message_t msg = { 0 };
	char delim = ',';
	int scope = 0;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *app_uuid = uuid_str;
	
	if ((app_uuid_var = switch_channel_get_variable(channel, "app_uuid"))) {
		app_uuid = (char *)app_uuid_var;
		switch_channel_set_variable(channel, "app_uuid", NULL);
	} else {
		switch_uuid_str(uuid_str, sizeof(uuid_str));
	}

	switch_assert(application_interface);

	app = application_interface->interface_name;

	if (arg) {
		expanded = switch_channel_expand_variables(session->channel, arg);
	}

	if (expanded && *expanded == '%' && (*(expanded+1) == '[' || *(expanded+2) == '[')) {
		char *p, *dup;
		switch_event_t *ovars = NULL;
		
		p = expanded + 1;

		if (*p != '[') {
			delim = *p;
			p++;
		}

		dup = strdup(p);
		
		if (expanded != arg) {
			switch_safe_free(expanded);
		}

		switch_event_create_brackets(dup, '[', ']', delim, &ovars, &expanded, SWITCH_TRUE);
		free(dup);

		switch_channel_set_scope_variables(session->channel, &ovars);
		scope = 1;
	}

	
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG_CLEAN(session), SWITCH_LOG_DEBUG, "EXECUTE %s %s(%s)\n",
					  switch_channel_get_name(session->channel), app, switch_str_nil(expanded));

	if ((var = switch_channel_get_variable(session->channel, "verbose_presence")) && switch_true(var)) {
		char *myarg = NULL;
		if (expanded) {
			myarg = switch_mprintf("%s(%s)", app, expanded);
		} else if (!zstr(arg)) {
			myarg = switch_mprintf("%s(%s)", app, arg);
		} else {
			myarg = switch_mprintf("%s", app);
		}
		if (myarg) {
			switch_channel_presence(session->channel, "unknown", myarg, NULL);
			switch_safe_free(myarg);
		}
	}

	if (!(var = switch_channel_get_variable(session->channel, SWITCH_DISABLE_APP_LOG_VARIABLE)) || (!(switch_true(var)))) {
		log = switch_core_session_alloc(session, sizeof(*log));

		log->app = switch_core_session_strdup(session, application_interface->interface_name);
		if (expanded) {
			log->arg = switch_core_session_strdup(session, expanded);
		}

		log->stamp = switch_time_now();

		for (lp = session->app_log; lp && lp->next; lp = lp->next);

		if (lp) {
			lp->next = log;
		} else {
			session->app_log = log;
		}
	}

	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_VARIABLE, application_interface->interface_name);
	switch_channel_set_variable_var_check(channel, SWITCH_CURRENT_APPLICATION_DATA_VARIABLE, expanded, SWITCH_FALSE);
	switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, NULL);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", application_interface->interface_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", expanded);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-UUID", app_uuid);
		switch_event_fire(&event);
	}

	switch_channel_clear_flag(session->channel, CF_BREAK);

	switch_assert(application_interface->application_function);

	switch_channel_set_variable(session->channel, SWITCH_CURRENT_APPLICATION_VARIABLE, application_interface->interface_name);

	msg.from = __FILE__;
	msg.message_id = SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC;
	msg.string_array_arg[0] = application_interface->interface_name;
	msg.string_array_arg[1] = expanded;
	switch_core_session_receive_message(session, &msg);

	application_interface->application_function(session, expanded);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE_COMPLETE) == SWITCH_STATUS_SUCCESS) {
		const char *resp = switch_channel_get_variable(session->channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE);
		switch_channel_event_set_data(session->channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", application_interface->interface_name);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", expanded);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Response", resp ? resp : "_none_");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-UUID", app_uuid);
		switch_event_fire(&event);
	}

	msg.message_id = SWITCH_MESSAGE_INDICATE_APPLICATION_EXEC_COMPLETE;
	switch_core_session_receive_message(session, &msg);

	if (expanded != arg) {
		switch_safe_free(expanded);
	}

	if (scope) {
		switch_channel_set_scope_variables(session->channel, NULL);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_execute_exten(switch_core_session_t *session, const char *exten, const char *dialplan,
																  const char *context)
{
	char *dp[25];
	char *dpstr;
	int argc, x, count = 0;
	switch_caller_profile_t *profile, *new_profile, *pp = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_dialplan_interface_t *dialplan_interface = NULL;
	switch_caller_extension_t *extension = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(profile = switch_channel_get_caller_profile(channel))) {
		return SWITCH_STATUS_FALSE;
	}

	if (session->stack_count > SWITCH_MAX_STACKS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error %s too many stacked extensions\n",
						  switch_channel_get_name(session->channel));
		return SWITCH_STATUS_FALSE;
	}

	session->stack_count++;

	new_profile = switch_caller_profile_clone(session, profile);
	new_profile->destination_number = switch_core_strdup(new_profile->pool, exten);
	new_profile->times = (switch_channel_timetable_t *) switch_core_session_alloc(session, sizeof(*new_profile->times));
	*new_profile->times = *profile->times;


	if (!zstr(dialplan)) {
		new_profile->dialplan = switch_core_strdup(new_profile->pool, dialplan);
	}

	if (!zstr(context)) {
		new_profile->context = switch_core_strdup(new_profile->pool, context);
	}

	dpstr = switch_core_session_strdup(session, new_profile->dialplan);

	switch_channel_set_hunt_caller_profile(channel, new_profile);
	argc = switch_separate_string(dpstr, ',', dp, (sizeof(dp) / sizeof(dp[0])));
	for (x = 0; x < argc; x++) {
		char *dpname = dp[x];
		char *dparg = NULL;

		if (dpname) {
			if ((dparg = strchr(dpname, ':'))) {
				*dparg++ = '\0';
			}
		} else {
			continue;
		}

		if (!(dialplan_interface = switch_loadable_module_get_dialplan_interface(dpname))) {
			continue;
		}

		count++;

		extension = dialplan_interface->hunt_function(session, dparg, new_profile);
		UNPROTECT_INTERFACE(dialplan_interface);

		if (extension) {
			break;
		}
	}

	if (!extension) {
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	new_profile->caller_extension = extension;

	if (profile->caller_extension) {
		for (pp = profile->caller_extension->children; pp && pp->next; pp = pp->next);

		if (pp) {
			pp->next = new_profile;
		} else {
			profile->caller_extension->children = new_profile;
		}
	}

	while (switch_channel_ready(channel) && extension->current_application) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Execute %s(%s)\n",
						  extension->current_application->application_name, switch_str_nil(extension->current_application->application_data));

		if (switch_core_session_execute_application(session,
													extension->current_application->application_name,
													extension->current_application->application_data) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}

		extension->current_application = extension->current_application->next;
	}

  done:
	switch_channel_set_hunt_caller_profile(channel, NULL);

	session->stack_count--;
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_set_loglevel(switch_core_session_t *session, switch_log_level_t loglevel)
{
	switch_assert(session != NULL);
	session->loglevel = loglevel;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_log_level_t) switch_core_session_get_loglevel(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	return session->loglevel;
}

SWITCH_DECLARE(switch_status_t) switch_core_session_refresh_video(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_channel_test_flag(channel, CF_VIDEO)) {
		switch_core_session_message_t msg = { 0 };
		msg.from = __FILE__;
		msg.message_id = SWITCH_MESSAGE_INDICATE_VIDEO_REFRESH_REQ;
		switch_core_session_receive_message(session, &msg);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
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

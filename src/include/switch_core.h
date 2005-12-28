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
 *
 * switch_core.h -- Core Library
 *
 */
#ifndef SWITCH_CORE_H
#define SWITCH_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

#define MAX_CORE_THREAD_SESSION_OBJS 128
struct switch_core_thread_session {
	int running;
	void *objs[MAX_CORE_THREAD_SESSION_OBJS];
	switch_memory_pool *pool;
};

struct switch_core_session;
struct switch_core_runtime;

SWITCH_DECLARE(switch_status) switch_core_init(void);
SWITCH_DECLARE(switch_status) switch_core_destroy(void);
SWITCH_DECLARE(switch_status) switch_core_do_perl(char *txt);
SWITCH_DECLARE(switch_status) switch_core_new_memory_pool(switch_memory_pool **pool);
SWITCH_DECLARE(switch_status) switch_core_destroy_memory_pool(switch_memory_pool **pool);
SWITCH_DECLARE(void) switch_core_session_run(switch_core_session *session);
SWITCH_DECLARE(switch_core_session *) switch_core_session_request(const switch_endpoint_interface *endpoint_interface, switch_memory_pool *pool);
SWITCH_DECLARE(switch_core_session *) switch_core_session_request_by_name(char *endpoint_name, switch_memory_pool *pool);
SWITCH_DECLARE(void *) switch_core_session_alloc(switch_core_session *session, size_t memory);
SWITCH_DECLARE(void) switch_core_session_thread_launch(switch_core_session *session);
SWITCH_DECLARE(switch_channel *) switch_core_session_get_channel(switch_core_session *session);
SWITCH_DECLARE(void *) switch_core_session_alloc(switch_core_session *session, size_t memory);
SWITCH_DECLARE(void) switch_core_session_destroy(switch_core_session **session);
SWITCH_DECLARE(void *) switch_core_permenant_alloc(size_t memory);
SWITCH_DECLARE(char *) switch_core_permenant_strdup(char *todup);
SWITCH_DECLARE(char *) switch_core_session_strdup(switch_core_session *session, char *todup);
SWITCH_DECLARE(void *) switch_core_session_get_private(switch_core_session *session);
SWITCH_DECLARE(switch_status) switch_core_session_set_private(switch_core_session *session, void *private);
SWITCH_DECLARE(void *) switch_core_alloc(switch_memory_pool *pool, size_t memory);
/* Hash Frontend */
SWITCH_DECLARE(switch_status) switch_core_hash_init(switch_hash **hash, switch_memory_pool *pool);
SWITCH_DECLARE(switch_status) switch_core_hash_destroy(switch_hash *hash);
SWITCH_DECLARE(switch_status) switch_core_hash_insert(switch_hash *hash, char *key, void *data);
SWITCH_DECLARE(switch_status) switch_core_hash_insert_dup(switch_hash *hash, char *key, void *data);
SWITCH_DECLARE(switch_status) switch_core_hash_delete(switch_hash *hash, char *key);
SWITCH_DECLARE(void *) switch_core_hash_find(switch_hash *hash, char *key);
SWITCH_DECLARE(void) switch_core_launch_thread(void *(*func)(switch_thread *, void*), void *obj);
SWITCH_DECLARE(FILE *) switch_core_data_channel(switch_text_channel channel);
SWITCH_DECLARE(void) switch_core_session_launch_thread(switch_core_session *session, void *(*func)(switch_thread *, void *), void *obj);
SWITCH_DECLARE(switch_status) switch_core_timer_init(switch_timer *timer, char *timer_name, int interval, int samples, switch_memory_pool *pool);
SWITCH_DECLARE(int) switch_core_timer_next(switch_timer *timer);
SWITCH_DECLARE(switch_status) switch_core_timer_destroy(switch_timer *timer);
SWITCH_DECLARE(void) switch_core_thread_session_end(switch_core_thread_session *thread_session);
SWITCH_DECLARE(void) switch_core_service_session(switch_core_session *session, switch_core_thread_session *thread_session);
SWITCH_DECLARE(switch_status) switch_core_session_outgoing_channel(switch_core_session *session,
											 char *endpoint_name,
											 switch_caller_profile *caller_profile,
											 switch_core_session **new_session);
SWITCH_DECLARE(switch_status) switch_core_session_answer_channel(switch_core_session *session);
SWITCH_DECLARE(switch_status) switch_core_session_read_frame(switch_core_session *session, switch_frame **frame, int timeout);
SWITCH_DECLARE(switch_status) switch_core_session_write_frame(switch_core_session *session, switch_frame *frame, int timeout);
SWITCH_DECLARE(switch_status) switch_core_session_kill_channel(switch_core_session *session, switch_signal sig);
SWITCH_DECLARE(switch_status) switch_core_session_waitfor_read(switch_core_session *session, int timeout);
SWITCH_DECLARE(switch_status) switch_core_session_waitfor_write(switch_core_session *session, int timeout);
SWITCH_DECLARE(switch_status) switch_core_session_send_dtmf(switch_core_session *session, char *dtmf);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_outgoing(switch_core_session *session, switch_outgoing_channel_hook outgoing_channel);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_answer_channel(switch_core_session *session, switch_answer_channel_hook answer_channel);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_read_frame(switch_core_session *session, switch_read_frame_hook read_frame);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_write_frame(switch_core_session *session, switch_write_frame_hook write_frame);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_kill_channel(switch_core_session *session, switch_kill_channel_hook kill_channel);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_waitfor_read(switch_core_session *session, switch_waitfor_read_hook waitfor_read);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_waitfor_write(switch_core_session *session, switch_waitfor_write_hook waitfor_write);
SWITCH_DECLARE(switch_status) switch_core_session_add_event_hook_send_dtmf(switch_core_session *session, switch_send_dtmf_hook send_dtmf);
SWITCH_DECLARE(switch_status) switch_core_codec_init(switch_codec *codec, char *codec_name, int rate, int ms, int channels, switch_codec_flag flags, const switch_codec_settings *codec_settings, switch_memory_pool *pool);
SWITCH_DECLARE(switch_status) switch_core_codec_encode(switch_codec *codec,
								 switch_codec *other_codec,
								 void *decoded_data,
								 size_t decoded_data_len,
								 void *encoded_data,
								 size_t *encoded_data_len,
								 unsigned int *flag);
SWITCH_DECLARE(switch_status) switch_core_codec_decode(switch_codec *codec,
								 switch_codec *other_codec,
								 void *encoded_data,
								 size_t encoded_data_len,
								 void *decoded_data,
								 size_t *decoded_data_len,
								 unsigned int *flag);
SWITCH_DECLARE(switch_status) switch_core_codec_destroy(switch_codec *codec);
SWITCH_DECLARE(switch_status) switch_core_session_set_read_codec(switch_core_session *session, switch_codec *codec);
SWITCH_DECLARE(switch_status) switch_core_session_set_write_codec(switch_core_session *session, switch_codec *codec);
SWITCH_DECLARE(switch_memory_pool *) switch_core_session_get_pool(switch_core_session *session);
SWITCH_DECLARE(void) pbx_core_session_signal_state_change(switch_core_session *session);
SWITCH_DECLARE(char *) switch_core_strdup(switch_memory_pool *pool, char *todup);
SWITCH_DECLARE(switch_core_db *) switch_core_db_open_file(char *filename);
SWITCH_DECLARE(char *) switch_core_session_get_uuid(switch_core_session *session);
SWITCH_DECLARE(switch_status) switch_core_file_open(switch_file_handle *fh, char *file_path, unsigned int flags, switch_memory_pool *pool);
SWITCH_DECLARE(switch_status) switch_core_file_read(switch_file_handle *fh, void *data, size_t *len);
SWITCH_DECLARE(switch_status) switch_core_file_write(switch_file_handle *fh, void *data, size_t *len);
SWITCH_DECLARE(switch_status) switch_core_file_seek(switch_file_handle *fh, unsigned int *cur_pos, unsigned int samples, int whence);
SWITCH_DECLARE(switch_status) switch_core_file_close(switch_file_handle *fh);

#define SWITCH_CORE_DB "core"
#define switch_core_db_handle() switch_core_db_open_file(SWITCH_CORE_DB)

#ifdef __cplusplus
}
#endif


#endif

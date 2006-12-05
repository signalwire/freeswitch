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
 * Neal Horman <neal at wanlink dot com>
 *
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */
#include <switch.h>

static const char modname[] = "mod_conference";
static const char global_app_name[] = "conference";
static char *global_cf_name = "conference.conf";
static switch_api_interface_t conf_api_interface;

/* Size to allocate for audio buffers */
#define CONF_BUFFER_SIZE 1024 * 128 
#define CONF_EVENT_MAINT "conference::maintenence"
#define CONF_DEFAULT_LEADIN 20

#define CONF_DBLOCK_SIZE CONF_BUFFER_SIZE
#define CONF_DBUFFER_SIZE CONF_BUFFER_SIZE
#define CONF_DBUFFER_MAX 0
#define CONF_CHAT_PROTO "conf"

typedef enum {
	FILE_STOP_CURRENT,
	FILE_STOP_ALL
} file_stop_t;

/* Global Values */
static struct {
	switch_memory_pool_t *conference_pool;
	switch_mutex_t *conference_mutex;
	switch_hash_t *conference_hash;
	switch_mutex_t *id_mutex;
	switch_mutex_t *hash_mutex;
	uint32_t id_pool;
	int32_t running;
	uint32_t threads;
} globals;

struct conference_member;
struct conference_obj;
typedef struct conference_obj conference_obj_t;
typedef struct conference_member conference_member_t;

typedef enum {
	MFLAG_RUNNING = (1 << 0),
	MFLAG_CAN_SPEAK = (1 << 1),
	MFLAG_CAN_HEAR = (1 << 2),
	MFLAG_KICKED = (1 << 3),
	MFLAG_ITHREAD = (1 << 4),
	MFLAG_NOCHANNEL = (1 << 5)
} member_flag_t;


typedef enum {
	CFLAG_RUNNING = (1 << 0),
	CFLAG_DYNAMIC = (1 << 1),
	CFLAG_ENFORCE_MIN = (1 << 2),
	CFLAG_DESTRUCT = (1 << 3),
	CFLAG_LOCKED = (1 << 4),
	CFLAG_ANSWERED = (1 << 5)
} conf_flag_t;

typedef enum {
	RFLAG_CAN_SPEAK = (1 << 0),
	RFLAG_CAN_HEAR = (1 << 1)
} relation_flag_t;

typedef enum {
	NODE_TYPE_FILE,
	NODE_TYPE_SPEECH
} node_type_t;

struct confernce_file_node {
	switch_file_handle_t fh;
	switch_speech_handle_t sh;
	node_type_t type;
	uint8_t done;
	switch_memory_pool_t *pool;
	uint32_t leadin;
	struct confernce_file_node *next;
};

typedef struct confernce_file_node confernce_file_node_t;

/* Conference Object */
struct conference_obj {
	char *name;
	char *timer_name;
	char *tts_engine;
	char *tts_voice;
	char *enter_sound;
	char *exit_sound;
	char *alone_sound;
	char *ack_sound;
	char *nack_sound;
	char *muted_sound;
	char *unmuted_sound;
	char *locked_sound;
	char *kicked_sound;
	char *caller_id_name;
	char *caller_id_number;
	char *pin;
	char *pin_sound;
	char *bad_pin_sound;
	char *profile_name;
	char *domain;
	uint32_t flags;
	switch_call_cause_t bridge_hangup_cause;
	switch_mutex_t *flag_mutex;
	uint32_t rate;
	uint32_t interval;
	switch_mutex_t *mutex;
	conference_member_t *members;
	switch_mutex_t *member_mutex;
	confernce_file_node_t *fnode;
	switch_memory_pool_t *pool;
	switch_thread_rwlock_t *rwlock;
	uint32_t count;
	int32_t energy_level;
	uint8_t min;
};

/* Relationship with another member */
struct conference_relationship {
	uint32_t id;
	uint32_t flags;
	struct conference_relationship *next;
};
typedef struct conference_relationship conference_relationship_t;

/* Conference Member Object */
struct conference_member {
	uint32_t id;
	switch_core_session_t *session;
	conference_obj_t *conference;
	conference_obj_t *last_conference;
	switch_memory_pool_t *pool;
	switch_buffer_t *audio_buffer;
	switch_buffer_t *mux_buffer;
	switch_buffer_t *resample_buffer;
	uint32_t flags;
	switch_mutex_t *flag_mutex;
	switch_mutex_t *audio_in_mutex;
	switch_mutex_t *audio_out_mutex;
	switch_codec_t read_codec;
	switch_codec_t write_codec;
	char *rec_path;
	uint8_t *frame;
	uint8_t *mux_frame;
	uint32_t buflen;
	uint32_t read;
	uint32_t len;
	int32_t energy_level;
	int32_t volume_in_level;
	int32_t volume_out_level;
	uint32_t native_rate;
	switch_audio_resampler_t *mux_resampler;
	switch_audio_resampler_t *read_resampler;
	confernce_file_node_t *fnode;
	conference_relationship_t *relationships;
	struct conference_member *next;
};

/* Record Node */
struct conference_record {
	conference_obj_t *conference;
	char *path;
	switch_memory_pool_t *pool;
};
typedef struct conference_record conference_record_t;

/* Function Prototypes */
static uint32_t next_member_id(void);
static conference_relationship_t *member_get_relationship(conference_member_t *member, conference_member_t *other_member);
static conference_member_t *conference_member_get(conference_obj_t *conference, uint32_t id);
static conference_relationship_t *member_add_relationship(conference_member_t *member, uint32_t id);
static void member_del_relationship(conference_member_t *member, uint32_t id);
static switch_status_t conference_add_member(conference_obj_t *conference, conference_member_t *member);
static void conference_del_member(conference_obj_t *conference, conference_member_t *member);
static void *SWITCH_THREAD_FUNC conference_thread_run(switch_thread_t *thread, void *obj);
static void conference_loop(conference_member_t *member);
static uint32_t conference_stop_file(conference_obj_t *conference, file_stop_t stop);
static switch_status_t conference_play_file(conference_obj_t *conference, char *file, uint32_t leadin, switch_channel_t *channel);
static switch_status_t conference_say(conference_obj_t *conference, char *text, uint32_t leadin);
static void conference_list(conference_obj_t *conference, switch_stream_handle_t *stream, char *delim);
static switch_status_t conf_function(char *buf, switch_core_session_t *session, switch_stream_handle_t *stream);
static switch_status_t audio_bridge_on_ring(switch_core_session_t *session);
static switch_status_t conference_outcall(conference_obj_t *conference,
										  switch_core_session_t *session,
										  char *bridgeto,
										  uint32_t timeout,
										  char *flags,
										  char *cid_name,
										  char *cid_num);
static void conference_function(switch_core_session_t *session, char *data);
static void launch_conference_thread(conference_obj_t *conference);
static void *SWITCH_THREAD_FUNC input_thread_run(switch_thread_t *thread, void *obj);
static void launch_input_thread(conference_member_t *member, switch_memory_pool_t *pool);
static switch_status_t conference_local_play_file(switch_core_session_t *session, char *path, uint32_t leadin, char *buf, switch_size_t len);
static switch_status_t conference_member_play_file(conference_member_t *member, char *file, uint32_t leadin);
static switch_status_t conference_member_say(conference_obj_t *conference, conference_member_t *member, char *text, uint32_t leadin);
static uint32_t conference_member_stop_file(conference_member_t *member, file_stop_t stop);
static conference_obj_t *conference_new(char *name, switch_xml_t profile, switch_memory_pool_t *pool);
static switch_status_t chat_send(char *proto, char *from, char *to, char *subject, char *body, char *hint);
static void launch_conference_record_thread(conference_obj_t *conference, char *path);

static void conference_member_itterator(conference_obj_t *conference,
										switch_stream_handle_t *stream,
										int (*pfncallback)(conference_obj_t*, conference_member_t*, int, switch_stream_handle_t*, void*),
										void *data);

/* Return a Distinct ID # */
static uint32_t next_member_id(void)
{
	uint32_t id;

	switch_mutex_lock(globals.id_mutex);
	id = ++globals.id_pool;
	switch_mutex_unlock(globals.id_mutex);

	return id;
}

/* if other_member has a relationship with member, produce it */
static conference_relationship_t *member_get_relationship(conference_member_t *member, conference_member_t *other_member)
{
	conference_relationship_t *rel = NULL, *global = NULL;

	switch_mutex_lock(member->flag_mutex);
	switch_mutex_lock(other_member->flag_mutex);

	if (member->relationships) {
		for (rel = member->relationships; rel; rel = rel->next) {
			if (rel->id == other_member->id) {
				break;
			}

			/* 0 matches everyone.
			   (We will still test the others brcause a real match carries more clout) */

			if (rel->id == 0) { 
				global = rel;
			}
		}
	}

	switch_mutex_unlock(other_member->flag_mutex);
	switch_mutex_unlock(member->flag_mutex);

	if (!rel && global) {
		rel = global;
	}

	return rel;
}

static conference_member_t *conference_member_get(conference_obj_t *conference, uint32_t id)
{
	conference_member_t *member = NULL;

	for(member = conference->members; member; member = member->next) {

		if (switch_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		if (member->id == id) {
			break;
		}
	}

	return member;
}

static int conference_record_stop(conference_obj_t *conference, char *path)
{
	conference_member_t *member = NULL;
	int count = 0;

	for(member = conference->members; member; member = member->next) {
		if (switch_test_flag(member, MFLAG_NOCHANNEL) && (!path || !strcmp(path, member->rec_path))) {
			switch_clear_flag_locked(member, MFLAG_RUNNING);
			count++;
		}
	}

	return count;
}

/* Add a custom relationship to a member */
static conference_relationship_t *member_add_relationship(conference_member_t *member, uint32_t id)
{
	conference_relationship_t *rel = NULL;

	if ((rel = switch_core_alloc(member->pool, sizeof(*rel)))) {
		rel->id = id;

		switch_mutex_lock(member->flag_mutex);
		rel->next = member->relationships;
		member->relationships = rel;
		switch_mutex_unlock(member->flag_mutex);
	}

	return rel;
}

/* Remove a custom relationship from a member */
static void member_del_relationship(conference_member_t *member, uint32_t id)
{
	conference_relationship_t *rel, *last = NULL;

	switch_mutex_lock(member->flag_mutex);
	for (rel = member->relationships; rel; rel = rel->next) {
		if (rel->id == id) {
			/* we just forget about rel here cos it was allocated by the member's pool 
			   it will be freed when the member is */
			if (last) {
				last->next = rel->next;
			} else {
				member->relationships = rel->next;
			}
		}
		last = rel;
	}
	switch_mutex_unlock(member->flag_mutex);
}

/* Gain exclusive access and add the member to the list */
static switch_status_t conference_add_member(conference_obj_t *conference, conference_member_t *member)
{
	switch_event_t *event;

	if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	switch_mutex_lock(member->audio_in_mutex);
	switch_mutex_lock(member->audio_out_mutex);
	switch_mutex_lock(member->flag_mutex);
	member->conference = member->last_conference = conference;
	member->next = conference->members;
	member->energy_level = conference->energy_level;
	conference->members = member;


	if (!switch_test_flag(member, MFLAG_NOCHANNEL)) {
		conference->count++;
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", conference->name, conference->domain);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Active (%d caller%s)", conference->count, conference->count == 1 ? "" : "s");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}


		if (conference->enter_sound) {
			conference_play_file(conference, conference->enter_sound, CONF_DEFAULT_LEADIN, switch_core_session_get_channel(member->session));
		}

		if (conference->count == 1 && conference->alone_sound) {
			conference_play_file(conference, conference->alone_sound, 0, switch_core_session_get_channel(member->session));
		}

		if (conference->min && conference->count >= conference->min) {
			switch_set_flag(conference, CFLAG_ENFORCE_MIN);	
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(member->session);
			switch_channel_event_set_data(channel, event);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", member->id);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "add-member");
			switch_event_fire(&event);
		}
	}
	switch_mutex_unlock(member->flag_mutex);
	switch_mutex_unlock(member->audio_out_mutex);
	switch_mutex_unlock(member->audio_in_mutex);
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);

	return SWITCH_STATUS_SUCCESS;
}

/* Gain exclusive access and remove the member from the list */
static void conference_del_member(conference_obj_t *conference, conference_member_t *member)
{
	conference_member_t *imember, *last = NULL;
	switch_event_t *event;

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	switch_mutex_lock(member->audio_in_mutex);
	switch_mutex_lock(member->audio_out_mutex);
	switch_mutex_lock(member->flag_mutex);

	for (imember = conference->members; imember; imember = imember->next) {
		if (imember == member ) {
			if (last) {
				last->next = imember->next;
			} else {
				conference->members = imember->next;
			}
			break;
		}
		last = imember;
	}


	member->conference = NULL;

	if (!switch_test_flag(member, MFLAG_NOCHANNEL)) {
		conference->count--;
		if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", conference->name, conference->domain);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Active (%d caller%s)", conference->count, conference->count == 1 ? "" : "s");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");
			switch_event_fire(&event);
		}

		if ((conference->min && switch_test_flag(conference, CFLAG_ENFORCE_MIN) && conference->count < conference->min) 
			|| (switch_test_flag(conference, CFLAG_DYNAMIC) && conference->count == 0) ) {
				switch_set_flag(conference, CFLAG_DESTRUCT);
		} else { 
			if (conference->exit_sound) {
				conference_play_file(conference, conference->exit_sound, 0, switch_core_session_get_channel(member->session));
			}
			if (conference->count == 1 && conference->alone_sound) {
				conference_play_file(conference, conference->alone_sound, 0, switch_core_session_get_channel(member->session));
			} 
		}

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(member->session);
			switch_channel_event_set_data(channel, event);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", member->id);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "del-member");
			switch_event_fire(&event);
		}
	}
	switch_mutex_unlock(member->flag_mutex);
	switch_mutex_unlock(member->audio_out_mutex);
	switch_mutex_unlock(member->audio_in_mutex);
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);


	switch_thread_rwlock_unlock(conference->rwlock);

}

/* Main monitor thread (1 per distinct conference room) */
static void *SWITCH_THREAD_FUNC conference_thread_run(switch_thread_t *thread, void *obj)
{
	conference_obj_t *conference = (conference_obj_t *) obj;
	conference_member_t *imember, *omember;
	uint32_t divider = 1000 / conference->interval;
	uint32_t samples = (conference->rate / divider);
	uint32_t bytes = samples * 2;
	uint8_t ready = 0;
	switch_timer_t timer = {0};
	switch_event_t *event;

	if (switch_core_timer_init(&timer, conference->timer_name, conference->interval, samples, conference->pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "setup timer success interval: %u  samples: %u\n", conference->interval, samples);	
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");	
		return NULL;
	}

	switch_mutex_lock(globals.hash_mutex);
	globals.threads++;
	switch_mutex_unlock(globals.hash_mutex);

	while(globals.running && !switch_test_flag(conference, CFLAG_DESTRUCT)) {
		uint8_t file_frame[CONF_BUFFER_SIZE] = {0};
		switch_size_t file_sample_len = samples;
		switch_size_t file_data_len = samples * 2;

		/* Sync the conference to a single timing source */
		switch_core_timer_next(&timer);

		switch_mutex_lock(conference->mutex);
		ready = 0;

		/* Read one frame of audio from each member channel and save it for redistribution */
		for (imember = conference->members; imember; imember = imember->next) {
			if (imember->buflen) {
				memset(imember->frame, 255, imember->buflen);
			} else {
				imember->frame = switch_core_alloc(imember->pool, bytes);
				imember->mux_frame = switch_core_alloc(imember->pool, bytes);
				imember->buflen = bytes;
			}

			switch_mutex_lock(imember->audio_in_mutex);
			/* if there is audio in the resample buffer it takes precedence over the other data */
			if (imember->mux_resampler && switch_buffer_inuse(imember->resample_buffer) >= bytes) {
				imember->read = (uint32_t)switch_buffer_read(imember->resample_buffer, imember->frame, bytes);
				ready++;
			} else if ((imember->read = (uint32_t)switch_buffer_read(imember->audio_buffer, imember->frame, imember->buflen))) {
				/* If the caller is not at the right sample rate resample him to suit and buffer accordingly */
				if (imember->mux_resampler) {
					int16_t *bptr = (int16_t *) imember->frame;
					int16_t out[1024];
					int len = (int) imember->read;

					imember->mux_resampler->from_len = switch_short_to_float(bptr, imember->mux_resampler->from, (int) len / 2);
					imember->mux_resampler->to_len = switch_resample_process(imember->mux_resampler, imember->mux_resampler->from,
																		 imember->mux_resampler->from_len, imember->mux_resampler->to,
																		 imember->mux_resampler->to_size, 0);
					switch_float_to_short(imember->mux_resampler->to, out, len);
					len = imember->mux_resampler->to_len * 2;
					switch_buffer_write(imember->resample_buffer, out, len);
					if (switch_buffer_inuse(imember->resample_buffer) >= bytes) {
						imember->read = (uint32_t)switch_buffer_read(imember->resample_buffer, imember->frame, bytes);
						ready++;
					}
				} else {
					ready++; /* Tally of how many channels had data */
				}
			}
			switch_mutex_unlock(imember->audio_in_mutex);
		}

		/* If a file or speech event is being played */
		if (conference->fnode) {
			/* Lead in time */
			if (conference->fnode->leadin) {
				conference->fnode->leadin--;
			} else {
				ready++;
				if (conference->fnode->type == NODE_TYPE_SPEECH) {
					switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_BLOCKING;
					uint32_t rate = conference->rate;

					if (switch_core_speech_read_tts(&conference->fnode->sh,
													file_frame,
													&file_data_len,
													&rate,
													&flags) == SWITCH_STATUS_SUCCESS) {
						file_sample_len = file_data_len / 2;
					} else {
						file_sample_len = file_data_len  = 0;
					}
				} else if (conference->fnode->type == NODE_TYPE_FILE) {
					switch_core_file_read(&conference->fnode->fh, file_frame, &file_sample_len);
				}

				if (file_sample_len <= 0) {
					conference->fnode->done++;
				}
			}
		}

		if (ready) {
			/* Build a muxed frame for every member that contains the mixed audio of everyone else */
			for (omember = conference->members; omember; omember = omember->next) {
				omember->len = bytes;
				if (conference->fnode) {
					memcpy(omember->mux_frame, file_frame, file_sample_len * 2);
				} else {
					memset(omember->mux_frame, 255, bytes);
				}
				for (imember = conference->members; imember; imember = imember->next) {

					if (imember == omember) {
						/* Don't add audio from yourself */
						continue;
					}

					if (imember->read) { /* mux the frame with the collective */
						uint32_t x;
						int16_t *bptr, *muxed;

						/* If they are not supposed to talk to us then don't let them */
						if (omember->relationships) {
							conference_relationship_t *rel;

							if ((rel = member_get_relationship(omember, imember))) {
								if (! switch_test_flag(rel, RFLAG_CAN_HEAR)) {
									continue;
								}
							}
						}

						/* If we are not supposed to hear them then don't let it happen */
						if (imember->relationships) {
							conference_relationship_t *rel;

							if ((rel = member_get_relationship(imember, omember))) {
								if (! switch_test_flag(rel, RFLAG_CAN_SPEAK)) {
									continue;
								}
							}
						}

						if (imember->read > imember->len) {
							imember->len = imember->read;
						}

						bptr = (int16_t *) imember->frame;
						muxed = (int16_t *) omember->mux_frame;


						for (x = 0; x < imember->read / 2; x++) {
							int32_t z = muxed[x] + bptr[x];
							switch_normalize_to_16bit(z);
							muxed[x] = (int16_t)z;
						}

						ready++;
					}
				}
			}

			/* Go back and write each member his dedicated copy of the audio frame that does not contain his own audio. */
			for (imember = conference->members; imember; imember = imember->next) {
				switch_mutex_lock(imember->audio_out_mutex);
				switch_buffer_write(imember->mux_buffer, imember->mux_frame, imember->len);
				switch_mutex_unlock(imember->audio_out_mutex);
			}
		}

		if (conference->fnode && conference->fnode->done) {
			confernce_file_node_t *fnode;
			switch_memory_pool_t *pool;

			if (conference->fnode->type == NODE_TYPE_SPEECH) {
				switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
				switch_core_speech_close(&conference->fnode->sh, &flags);
			} else {
				switch_core_file_close(&conference->fnode->fh);
			}

			fnode = conference->fnode;
			conference->fnode = conference->fnode->next;

			pool = fnode->pool;
			fnode = NULL;
			switch_core_destroy_memory_pool(&pool);

		}

		switch_mutex_unlock(conference->mutex);
	} /* Rinse ... Repeat */

	switch_core_timer_destroy(&timer);

	if (switch_test_flag(conference, CFLAG_DESTRUCT)) {

		switch_mutex_lock(conference->mutex);

		for(imember = conference->members; imember; imember = imember->next) {
			switch_channel_t *channel;

			if (!switch_test_flag(imember, MFLAG_NOCHANNEL)) {
				channel = switch_core_session_get_channel(imember->session);

				// add this little bit to preserve the bridge cause code in case of an early media call that
				// never answers
				if (switch_test_flag(conference, CFLAG_ANSWERED)) {
					switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);	
				} else 	{
					// put actual cause code from outbound channel hangup here
					switch_channel_hangup(channel, conference->bridge_hangup_cause);
				}
			}

			switch_clear_flag_locked(imember, MFLAG_RUNNING);
		}

		switch_mutex_unlock(conference->mutex);

		/* Wait till everybody is out */
		switch_clear_flag_locked(conference, CFLAG_RUNNING);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Lock ON\n");
		switch_thread_rwlock_wrlock(conference->rwlock);
		switch_thread_rwlock_unlock(conference->rwlock);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write Lock OFF\n");

		switch_mutex_lock(globals.hash_mutex);		
		switch_core_hash_delete(globals.conference_hash, conference->name);
		switch_mutex_unlock(globals.hash_mutex);

		if (conference->pool) {
			switch_memory_pool_t *pool = conference->pool;
			switch_core_destroy_memory_pool(&pool);
		}
	}

	if (switch_event_create(&event, SWITCH_EVENT_PRESENCE_IN) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "proto", CONF_CHAT_PROTO);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "login", "%s", conference->name);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "from", "%s@%s", conference->name, conference->domain);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "status", "Inactive");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rpid", "idle");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "event_type", "presence");

		switch_event_fire(&event);
	}
	switch_mutex_lock(globals.hash_mutex);
	globals.threads--;	
	switch_mutex_unlock(globals.hash_mutex);

	return NULL;
}

/* Sub-Routine called by a channel inside a conference */
static void conference_loop(conference_member_t *member)
{
	switch_channel_t *channel;
	switch_frame_t write_frame = {0};
	uint8_t data[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_timer_t timer = {0};
	uint32_t divider = 1000 / member->conference->interval;
	uint32_t samples = (member->conference->rate / divider);
	uint32_t bytes = samples * 2;

	channel = switch_core_session_get_channel(member->session);

	assert(channel != NULL);
	assert(member->conference != NULL);

	if (switch_core_timer_init(&timer,
							   member->conference->timer_name,
							   member->conference->interval,
							   samples,
							   NULL) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "setup timer %s success interval: %u  samples: %u\n", 
						  member->conference->timer_name, member->conference->interval, samples);	
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Setup Failed.  Conference Cannot Start\n");	
		return;
	}

	write_frame.data = data;
	write_frame.buflen = sizeof(data);
	write_frame.codec = &member->write_codec;

	if (switch_test_flag(member->conference, CFLAG_ANSWERED)) {
		switch_channel_answer(channel);
	}

	/* Start a thread to read data and feed it into the buffer and use this thread to generate output */
	launch_input_thread(member, switch_core_session_get_pool(member->session));

	while(switch_test_flag(member, MFLAG_RUNNING) && switch_test_flag(member, MFLAG_ITHREAD) && switch_channel_ready(channel)) {
		char dtmf[128] = "";
		uint8_t file_frame[CONF_BUFFER_SIZE] = {0};
		switch_size_t file_data_len = samples * 2;
		switch_size_t file_sample_len = samples;
		char *digit;
		char msg[512];
		switch_event_t *event;

		if (switch_core_session_dequeue_event(member->session, &event) == SWITCH_STATUS_SUCCESS) {
			char *from = switch_event_get_header(event, "from");
			char *to = switch_event_get_header(event, "to");
			char *proto = switch_event_get_header(event, "proto");
			char *subject = switch_event_get_header(event, "subject");
			char *hint = switch_event_get_header(event, "hint");
			char *body = switch_event_get_body(event);
			char *p, *freeme = NULL;

			if ((p = strchr(to, '+')) && 
				strncmp(to, CONF_CHAT_PROTO, strlen(CONF_CHAT_PROTO))) {
				freeme = switch_mprintf("%s+%s@%s", CONF_CHAT_PROTO, member->conference->name, member->conference->domain);
				to = freeme;
			}

			chat_send(proto, from, to, subject, body, hint);
			switch_safe_free(freeme);
			switch_event_destroy(&event);
		}

		if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
			// test to see if outbound channel has answered
			if (switch_channel_test_flag(channel, CF_ANSWERED) && !switch_test_flag(member->conference, CFLAG_ANSWERED)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Outbound conference channel answered, setting CFLAG_ANSWERED");
				switch_set_flag(member->conference, CFLAG_ANSWERED);
			}
		} else {
			if (switch_test_flag(member->conference, CFLAG_ANSWERED) && !switch_channel_test_flag(channel, CF_ANSWERED)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CLFAG_ANSWERED set, answering inbound channel\n");
				switch_channel_answer(channel);
			}
		}

		if (switch_channel_has_dtmf(channel)) {
			switch_channel_dequeue_dtmf(channel, dtmf, sizeof(dtmf));

			for (digit = dtmf; *digit; digit++) {
				switch(*digit) {
				case '0':
					if (switch_test_flag(member, MFLAG_CAN_SPEAK)) {
						switch_clear_flag_locked(member, MFLAG_CAN_SPEAK | MFLAG_CAN_HEAR);
						if (member->conference->muted_sound) {
							conference_member_play_file(member, member->conference->muted_sound, 0);
						} else {
							snprintf(msg, sizeof(msg), "Muted");
							conference_member_say(member->conference, member, msg, 0);
						}
					} else {
						switch_set_flag_locked(member, MFLAG_CAN_SPEAK);
						if (member->conference->unmuted_sound) {
							conference_member_play_file(member, member->conference->unmuted_sound, 0);
						} else {
							snprintf(msg, sizeof(msg), "Un-Muted");
							conference_member_say(member->conference, member, msg, 0);
						}
					}
					break;
				case '*':
					if (switch_test_flag(member, MFLAG_CAN_SPEAK)) {
						switch_clear_flag_locked(member, MFLAG_CAN_SPEAK|MFLAG_CAN_HEAR);
						if (member->conference->muted_sound) {
							conference_member_play_file(member, member->conference->muted_sound, 0);
						} else {
							snprintf(msg, sizeof(msg), "Muted");
							conference_member_say(member->conference, member, msg, 0);
						}
					} else {
						switch_set_flag_locked(member, MFLAG_CAN_SPEAK|MFLAG_CAN_HEAR);
						if (member->conference->unmuted_sound) {
							conference_member_play_file(member, member->conference->unmuted_sound, 0);
						} else {
							snprintf(msg, sizeof(msg), "UN-Muted");
							conference_member_say(member->conference, member, msg, 0);
						}
					}
					break;
				case '9':
					switch_mutex_lock(member->flag_mutex);
					member->energy_level += 100;
					if (member->energy_level > 1200) {
						member->energy_level = 1200;
					}
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '8':
					switch_mutex_lock(member->flag_mutex);
					member->energy_level = member->conference->energy_level;
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '7':
					switch_mutex_lock(member->flag_mutex);
					member->energy_level -= 100;
					if (member->energy_level < 0) {
						member->energy_level = 0;
					}
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Energy level %d", member->energy_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '3':
					switch_mutex_lock(member->flag_mutex);
					member->volume_out_level++;
					switch_normalize_volume(member->volume_out_level);
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '2':
					switch_mutex_lock(member->flag_mutex);
					member->volume_out_level = 0;
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '1':
					switch_mutex_lock(member->flag_mutex);
					member->volume_out_level--;
					switch_normalize_volume(member->volume_out_level);
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Volume level %d", member->volume_out_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '6':
					switch_mutex_lock(member->flag_mutex);
					member->volume_in_level++;
					switch_normalize_volume(member->volume_in_level);
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '5':
					switch_mutex_lock(member->flag_mutex);
					member->volume_in_level = 0;
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '4':
					switch_mutex_lock(member->flag_mutex);
					member->volume_in_level--;
					switch_normalize_volume(member->volume_in_level);
					switch_mutex_unlock(member->flag_mutex);
					snprintf(msg, sizeof(msg), "Gain level %d", member->volume_in_level);
					conference_member_say(member->conference, member, msg, 0);
					break;
				case '#':
					switch_clear_flag_locked(member, MFLAG_RUNNING);
					break;
				default:
					break;
				}
			}
		}

		if (member->fnode) {
			if (member->fnode->done) {
				confernce_file_node_t *fnode;
				switch_memory_pool_t *pool;

				if (member->fnode->type == NODE_TYPE_SPEECH) {
					switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
					switch_core_speech_close(&member->fnode->sh, &flags);
				} else {
					switch_core_file_close(&member->fnode->fh);
				}

				switch_mutex_lock(member->flag_mutex);
				fnode = member->fnode;
				member->fnode = member->fnode->next;
				switch_mutex_unlock(member->flag_mutex);

				pool = fnode->pool;
				fnode = NULL;
				switch_core_destroy_memory_pool(&pool);

			} else {
				if (member->fnode->leadin) {
					member->fnode->leadin--;
				} else {
					if (member->fnode->type == NODE_TYPE_SPEECH) {
						switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_BLOCKING;
						uint32_t rate = member->conference->rate;

						if (switch_core_speech_read_tts(&member->fnode->sh,
														file_frame,
														&file_data_len,
														&rate,
														&flags) == SWITCH_STATUS_SUCCESS) {
							file_sample_len = file_data_len / 2;
						} else {
							file_sample_len = file_data_len  = 0;
						}
					} else if (member->fnode->type == NODE_TYPE_FILE) {
						switch_core_file_read(&member->fnode->fh, file_frame, &file_sample_len);
						file_data_len = file_sample_len * 2;
					}

					if (file_sample_len <= 0) {
						member->fnode->done++;
					} else { /* there is file node data to deliver */
						write_frame.data = file_frame;
						write_frame.datalen = (uint32_t)file_data_len;
						write_frame.samples = (uint32_t)file_sample_len;
						/* Check for output volume adjustments */
						if (member->volume_out_level) {
							switch_change_sln_volume(write_frame.data, write_frame.samples, member->volume_out_level);
						}
						switch_core_session_write_frame(member->session, &write_frame, -1, 0);

						/* forget the conference data we played file node data instead */
						switch_mutex_lock(member->audio_out_mutex);
						switch_buffer_zero(member->mux_buffer);
						switch_mutex_unlock(member->audio_out_mutex);
					}
					switch_core_timer_next(&timer);
				}
			}
		} else {
			switch_buffer_t *use_buffer = NULL;
			uint32_t mux_used = (uint32_t)switch_buffer_inuse(member->mux_buffer);
			//uint32_t res_used = member->mux_resampler ? switch_buffer_inuse(member->resample_buffer) : 0;

			if (mux_used) {
				/* Flush the output buffer and write all the data (presumably muxed) back to the channel */
				switch_mutex_lock(member->audio_out_mutex);
				write_frame.data = data;
				use_buffer = member->mux_buffer;

				while ((write_frame.datalen = (uint32_t)switch_buffer_read(use_buffer, write_frame.data, bytes))) {
					if (write_frame.datalen && switch_test_flag(member, MFLAG_CAN_HEAR)) {
						write_frame.samples = write_frame.datalen / 2;

						/* Check for output volume adjustments */
						if (member->volume_out_level) {
							switch_change_sln_volume(write_frame.data, write_frame.samples, member->volume_out_level);
						}

						switch_core_session_write_frame(member->session, &write_frame, -1, 0);
					}
				}

				switch_mutex_unlock(member->audio_out_mutex);
			} else {
				switch_core_timer_next(&timer);
			}
		}
	} /* Rinse ... Repeat */

	switch_clear_flag_locked(member, MFLAG_RUNNING);
	switch_core_timer_destroy(&timer);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Channel leaving conference, cause: %s\n",
			switch_channel_cause2str(switch_channel_get_cause(channel)));

	// if it's an outbound channel, store the release cause in the conference struct, we might need it
	if (switch_channel_test_flag(channel, CF_OUTBOUND)) {
		member->conference->bridge_hangup_cause = switch_channel_get_cause(channel);
	}

	/* Wait for the input thead to end */
	while(switch_test_flag(member, MFLAG_ITHREAD)) {
		switch_yield(1000);
	}
}

/* Sub-Routine called by a record entity inside a conference */
static void *SWITCH_THREAD_FUNC conference_record_thread_run(switch_thread_t *thread, void *obj)
{
	switch_frame_t write_frame = {0};
	uint8_t data[SWITCH_RECCOMMENDED_BUFFER_SIZE];
	switch_file_handle_t fh = {0};
	conference_member_t smember = {0}, *member;
	conference_record_t *rec = (conference_record_t *) obj;
	uint32_t divider = 1000 / rec->conference->interval;
	uint32_t samples = (rec->conference->rate / divider);
	uint32_t bytes = samples * 2;
	uint32_t mux_used;
	char *vval;

	switch_mutex_lock(globals.hash_mutex);
	globals.threads++;
	switch_mutex_unlock(globals.hash_mutex);		

	member = &smember;

	member->flags = MFLAG_CAN_HEAR | MFLAG_NOCHANNEL | MFLAG_RUNNING;

	write_frame.data = data;
	write_frame.buflen = sizeof(data);
	assert(rec->conference != NULL);

	member->conference = rec->conference;
	member->native_rate = rec->conference->rate;
	member->rec_path = rec->path;
	fh.channels = 1;
	fh.samplerate = rec->conference->rate;
	member->id = next_member_id();
	member->pool = rec->pool;

	switch_mutex_init(&member->flag_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->audio_in_mutex, SWITCH_MUTEX_NESTED, rec->pool);
	switch_mutex_init(&member->audio_out_mutex, SWITCH_MUTEX_NESTED, rec->pool);

	/* Setup an audio buffer for the incoming audio */
	if (switch_buffer_create_dynamic(&member->audio_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto end;
	}

	/* Setup an audio buffer for the outgoing audio */
	if (switch_buffer_create_dynamic(&member->mux_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto end;
	}

	if (conference_add_member(rec->conference, member) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Joining Conference\n");
		goto end;
	}

	if (switch_core_file_open(&fh,
							  rec->path,
							  SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT,
							  rec->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening File [%s]\n", rec->path);
		goto end;
	}

	if ((vval = switch_mprintf("Conference %s", rec->conference->name))) {
		switch_core_file_set_string(&fh, SWITCH_AUDIO_COL_STR_TITLE, vval);
		switch_safe_free(vval);
	}

	while(switch_test_flag(member, MFLAG_RUNNING) && switch_test_flag(rec->conference, CFLAG_RUNNING) && rec->conference->count) {
		if ((mux_used = (uint32_t) switch_buffer_inuse(member->mux_buffer)) >= bytes) {
			/* Flush the output buffer and write all the data (presumably muxed) to the file */
			switch_mutex_lock(member->audio_out_mutex);
			write_frame.data = data;
			while ((write_frame.datalen = (uint32_t)switch_buffer_read(member->mux_buffer, write_frame.data, mux_used))) {
				if (!switch_test_flag((&fh), SWITCH_FILE_PAUSE)) {
					switch_size_t len = (switch_size_t) mux_used / 2;
					switch_core_file_write(&fh, write_frame.data, &len);
				}
			}
			switch_mutex_unlock(member->audio_out_mutex);
		} else {
			switch_yield(20000);
		}
	} /* Rinse ... Repeat */

	conference_del_member(rec->conference, member);
	switch_buffer_destroy(&member->audio_buffer);
	switch_buffer_destroy(&member->mux_buffer);
	switch_clear_flag_locked(member, MFLAG_RUNNING);
	switch_core_file_close(&fh);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Recording Stopped\n");

end:

	if (rec->pool) {
		switch_memory_pool_t *pool = rec->pool;
		rec = NULL;
		switch_core_destroy_memory_pool(&pool);
	}

	switch_mutex_lock(globals.hash_mutex);
	globals.threads--;
	switch_mutex_unlock(globals.hash_mutex);

	return NULL;
}

/* Make files stop playing in a conference either the current one or all of them */
static uint32_t conference_stop_file(conference_obj_t *conference, file_stop_t stop)
{
	confernce_file_node_t *nptr;
	uint32_t count = 0;

	switch_mutex_lock(conference->mutex);

	if (stop == FILE_STOP_ALL) {
		for (nptr = conference->fnode; nptr; nptr = nptr->next) {
			nptr->done++;
			count++;
		}
	} else {
		if (conference->fnode) {
			conference->fnode->done++;
			count++;
		}
	}

	switch_mutex_unlock(conference->mutex);

	return count;
}

static uint32_t conference_member_stop_file(conference_member_t *member, file_stop_t stop)
{
	confernce_file_node_t *nptr;
	uint32_t count = 0;

	switch_mutex_lock(member->flag_mutex);

	if (stop == FILE_STOP_ALL) {
		for (nptr = member->fnode; nptr; nptr = nptr->next) {
			nptr->done++;
			count++;
		}
	} else {
		if (member->fnode) {
			member->fnode->done++;
			count++;
		}
	}

	switch_mutex_unlock(member->flag_mutex);

	return count;
}

/* Play a file in the conference rooom */
static switch_status_t conference_play_file(conference_obj_t *conference, char *file, uint32_t leadin, switch_channel_t *channel)
{
	confernce_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;
	uint32_t count;
    char *expanded = NULL;
    uint8_t frexp = 0;
    switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	count = conference->count;
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);	

	if (!count) {
		return SWITCH_STATUS_FALSE;
	}

    if (channel) {
        if ((expanded = switch_channel_expand_variables(channel, file)) != file) {
            file = expanded;
            frexp = 1;
        }
    }


#ifdef WIN32
	if (*(file +1) != ':' && *file != '/') {
#else
	if (*file != '/') {
#endif
		status = conference_say(conference, file, leadin);
        if (frexp) {
            switch_safe_free(expanded);
        }
        return status;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		status = SWITCH_STATUS_MEMERR;
        goto done;
	}

	/* Create a node object*/
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_MEMERR;
        goto done;
	}

	fnode->type = NODE_TYPE_FILE;
	fnode->leadin = leadin;

	/* Open the file */
	if (switch_core_file_open(&fnode->fh,
							  file,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  pool) != SWITCH_STATUS_SUCCESS) {
		switch_core_destroy_memory_pool(&pool);
		status = SWITCH_STATUS_NOTFOUND;
        goto done;
	}

	fnode->pool = pool;

	/* Queue the node */
	switch_mutex_lock(conference->mutex);
	for (nptr = conference->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		conference->fnode = fnode;
	}
	switch_mutex_unlock(conference->mutex);

    done:

    if (frexp) {
        switch_safe_free(expanded);
    }

	return status;
}

/* Play a file in the conference rooom to a member */
static switch_status_t conference_member_play_file(conference_member_t *member, char *file, uint32_t leadin)
{
	confernce_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;

	if (*file != '/') {
		return conference_member_say(member->conference, member, file, leadin);
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* Create a node object*/
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}

	fnode->type = NODE_TYPE_FILE;
	fnode->leadin = leadin;

	/* Open the file */
	if (switch_core_file_open(&fnode->fh,
							  file,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT,
							  pool) != SWITCH_STATUS_SUCCESS) {
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_NOTFOUND;
	}

	fnode->pool = pool;

	/* Queue the node */
	switch_mutex_lock(member->flag_mutex);
	for (nptr = member->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		member->fnode = fnode;
	}
	switch_mutex_unlock(member->flag_mutex);

	return SWITCH_STATUS_SUCCESS;
}

/* Say some thing with TTS in the conference rooom */
static switch_status_t conference_member_say(conference_obj_t *conference, conference_member_t *member, char *text, uint32_t leadin)
{
	confernce_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;

	if (!(conference->tts_engine && conference->tts_voice)) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* Create a node object*/
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}

	fnode->type = NODE_TYPE_SPEECH;
	fnode->leadin = leadin;

	memset(&fnode->sh, 0, sizeof(fnode->sh));
	if (switch_core_speech_open(&fnode->sh,
								conference->tts_engine,
								conference->tts_voice,
								conference->rate,
								&flags,
								conference->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid TTS module [%s]!\n", conference->tts_engine);
		return SWITCH_STATUS_FALSE;
	}


	fnode->pool = pool;

	/* Queue the node */
	switch_mutex_lock(member->flag_mutex);
	for (nptr = member->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		member->fnode = fnode;
	}
	switch_mutex_unlock(member->flag_mutex);

	/* Begin Generation */
	switch_sleep(200000);
	switch_core_speech_feed_tts(&fnode->sh, text, &flags);

	return SWITCH_STATUS_SUCCESS;
}

/* Say some thing with TTS in the conference rooom */
static switch_status_t conference_say(conference_obj_t *conference, char *text, uint32_t leadin)
{
	confernce_file_node_t *fnode, *nptr;
	switch_memory_pool_t *pool;
	switch_speech_flag_t flags = SWITCH_SPEECH_FLAG_NONE;
	uint32_t count;

	switch_mutex_lock(conference->mutex);
	switch_mutex_lock(conference->member_mutex);
	count = conference->count;
	if (!(conference->tts_engine && conference->tts_voice)) {
		count = 0;
	}
	switch_mutex_unlock(conference->member_mutex);
	switch_mutex_unlock(conference->mutex);	

	if (!count) {
		return SWITCH_STATUS_FALSE;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* Create a node object*/
	if (!(fnode = switch_core_alloc(pool, sizeof(*fnode)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}

	fnode->type = NODE_TYPE_SPEECH;
	fnode->leadin = leadin;

	memset(&fnode->sh, 0, sizeof(fnode->sh));
	if (switch_core_speech_open(&fnode->sh,
								conference->tts_engine,
								conference->tts_voice,
								conference->rate,
								&flags,
								conference->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid TTS module [%s]!\n", conference->tts_engine);
		return SWITCH_STATUS_FALSE;
	}

	fnode->pool = pool;

	/* Queue the node */
	switch_mutex_lock(conference->mutex);
	for (nptr = conference->fnode; nptr && nptr->next; nptr = nptr->next);

	if (nptr) {
		nptr->next = fnode;
	} else {
		conference->fnode = fnode;
	}
	switch_mutex_unlock(conference->mutex);

	/* Begin Generation */
	switch_sleep(200000);
	switch_core_speech_feed_tts(&fnode->sh, text, &flags);

	return SWITCH_STATUS_SUCCESS;
}

static void conference_member_itterator(conference_obj_t *conference, switch_stream_handle_t *stream, int (*pfncallback)(conference_obj_t*, conference_member_t*, int, switch_stream_handle_t*, void*), void *data)
{
	conference_member_t *member = NULL;

	if(conference != NULL && stream != NULL && pfncallback != NULL) {
		switch_mutex_lock(conference->member_mutex);

		for (member = conference->members; member; member = member->next) {
			pfncallback(conference,member,member->id,stream,data);
		}
		switch_mutex_unlock(conference->member_mutex);
	}
}

static void conference_list(conference_obj_t *conference, switch_stream_handle_t *stream, char *delim)
{
	conference_member_t *member = NULL;

	switch_mutex_lock(conference->member_mutex);

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;
		char *uuid;
		char *name;
		uint32_t count = 0;

		if (switch_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}

		uuid = switch_core_session_get_uuid(member->session);
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);
		name = switch_channel_get_name(channel);


		stream->write_function(stream, "%u%s%s%s%s%s%s%s%s%s", 
							   member->id,delim,
							   name,delim,
							   uuid,delim,
							   profile->caller_id_name,delim,
							   profile->caller_id_number, delim);

		if (switch_test_flag(member, MFLAG_CAN_HEAR)) {
			stream->write_function(stream, "hear");
			count++;
		}

		if (switch_test_flag(member, MFLAG_CAN_SPEAK)) {
			stream->write_function(stream, "%s%s", count ? "|" : "", "speak");
			count++;
		}

		stream->write_function(stream, "\n");
	}
	switch_mutex_unlock(conference->member_mutex);
}

static void conference_list_pretty(conference_obj_t *conference, switch_stream_handle_t *stream)
{
	conference_member_t *member = NULL;

	switch_mutex_lock(conference->member_mutex);
	stream->write_function(stream, "<pre>Current Callers:\n");

	for (member = conference->members; member; member = member->next) {
		switch_channel_t *channel;
		switch_caller_profile_t *profile;

		if (switch_test_flag(member, MFLAG_NOCHANNEL)) {
			continue;
		}
		channel = switch_core_session_get_channel(member->session);
		profile = switch_channel_get_caller_profile(channel);


		stream->write_function(stream, "*) %s (%s)\n", 
							   profile->caller_id_name,
							   profile->caller_id_number
							   );

	}
	switch_mutex_unlock(conference->member_mutex);
}

static int conference_function_mute_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		switch_clear_flag_locked(member, MFLAG_CAN_SPEAK);
		if (member->conference->muted_sound) {
			conference_member_play_file(member, member->conference->muted_sound, 0);
		}
		stream->write_function(stream, "OK mute %u\n", id);
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(member->session);
			switch_channel_event_set_data(channel, event);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "mute-member");
			switch_event_fire(&event);
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

static int conference_function_unmute_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		switch_set_flag_locked(member, MFLAG_CAN_SPEAK);
		stream->write_function(stream, "OK unmute %u\n", id);
		if (member->conference->unmuted_sound) {
			conference_member_play_file(member, member->conference->unmuted_sound, 0);
		}
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(member->session);
			switch_channel_event_set_data(channel, event);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "unmute-member");
			switch_event_fire(&event);
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

static int conference_function_deaf_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		switch_clear_flag_locked(member, MFLAG_CAN_HEAR);
		stream->write_function(stream, "OK deaf %u\n", id);
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(member->session);
			switch_channel_event_set_data(channel, event);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "deaf-member");
			switch_event_fire(&event);
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

static int conference_function_undeaf_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		switch_set_flag_locked(member, MFLAG_CAN_HEAR);
		stream->write_function(stream, "OK undeaf %u\n", id);
		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(member->session);
			switch_channel_event_set_data(channel, event);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "undeaf-member");
			switch_event_fire(&event);
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

static int conference_function_kick_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		switch_mutex_lock(member->flag_mutex);
		switch_clear_flag(member, MFLAG_RUNNING);
		switch_set_flag(member, MFLAG_KICKED);
		switch_mutex_unlock(member->flag_mutex);

		stream->write_function(stream, "OK kicked %u\n", id);

		if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *channel = switch_core_session_get_channel(member->session);
			switch_channel_event_set_data(channel, event);

			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "kick-member");
			switch_event_fire(&event);
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

static int conference_function_energy_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		if (data) {
			switch_mutex_lock(member->flag_mutex);
			member->energy_level = atoi((char *)data);
			switch_mutex_unlock(member->flag_mutex);
		}

		stream->write_function(stream, "Energy %u=%d\n", id, member->energy_level);

		if (data) {
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				switch_channel_t *channel = switch_core_session_get_channel(member->session);
				switch_channel_event_set_data(channel, event);

				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "energy-level-member");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Energy-Level", "%d", member->energy_level);

				switch_event_fire(&event);
			}
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

static int conference_function_volume_in_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		if (data) {
			switch_mutex_lock(member->flag_mutex);
			member->volume_in_level = atoi((char *)data);
			switch_normalize_volume(member->volume_in_level);
			switch_mutex_unlock(member->flag_mutex);
		}

		stream->write_function(stream, "Volume IN %u=%d\n", id, member->volume_in_level);
		if (data) {
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				switch_channel_t *channel = switch_core_session_get_channel(member->session);
				switch_channel_event_set_data(channel, event);

				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "volume-in-member");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Volume-Level", "%u", member->volume_in_level);

				switch_event_fire(&event);
			}
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

static int conference_function_volume_out_member(conference_obj_t *conference, conference_member_t *member, int id, switch_stream_handle_t *stream, void *data)
{
	int err = 0;

	if (member != NULL || (member = conference_member_get(conference, id))) {
		switch_event_t *event;

		if (data) {
			switch_mutex_lock(member->flag_mutex);
			member->volume_out_level = atoi((char *)data);
			switch_normalize_volume(member->volume_out_level);
			switch_mutex_unlock(member->flag_mutex);
		}

		stream->write_function(stream, "Volume OUT %u=%d\n", id, member->volume_out_level);

		if (data) {
			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				switch_channel_t *channel = switch_core_session_get_channel(member->session);
				switch_channel_event_set_data(channel, event);

				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "volume-out-member");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Volume-Level", "%u", member->volume_out_level);

				switch_event_fire(&event);
			}
		}
	} else {
		stream->write_function(stream, "Non-Existant ID %u\n", id);
		err = 1;
	}

	return err;
}

/* API Interface Function */
static switch_status_t conf_function(char *buf, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	char *lbuf = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *http = NULL;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (stream->event) {
		http = switch_event_get_header(stream->event, "http-host");
	}

	if (http) {
		/* Output must be to a web browser */
		stream->write_function(stream, "<pre>\n");
	}

	if (!buf) {
		stream->write_function(stream, "%s", conf_api_interface.syntax);
		return status;
	}

	if ((lbuf = strdup(buf))) {
		conference_obj_t *conference = NULL;
		int argc;
		char *argv[25];
		switch_event_t *event;

		argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

		/* Figure out what conference */
		if (argc) {
			if (!strcasecmp(argv[0], "commands")) {
				stream->write_function(stream, "%s", conf_api_interface.syntax);
				goto done;
			} else if (!strcasecmp(argv[0], "list")) {
				switch_hash_index_t *hi;
				void *val;
				char *d = ";";

				if (argv[1]) {
					if (argv[2] && !strcasecmp(argv[1], "delim")) {
						d = argv[2];

						if (*d == '"') {
							if (++d) {
								char *p;
								if ((p = strchr(d, '"'))) {
									*p = '\0';
								}
							} else {
								d = ";";
							}
						}
					}
				} 

				for (hi = switch_hash_first(globals.conference_pool, globals.conference_hash); hi; hi = switch_hash_next(hi)) {
					switch_hash_this(hi, NULL, NULL, &val);
					conference = (conference_obj_t *) val;

					stream->write_function(stream, "Conference %s (%u members)\n", conference->name, conference->count);
					conference_list(conference, stream, d);
					stream->write_function(stream, "\n");
				}
				goto done;
			} else if (!(conference = (conference_obj_t *) switch_core_hash_find(globals.conference_hash, argv[0]))) {
				stream->write_function(stream, "No Conference called %s found.\n", argv[0]);
				goto done;
			}

			if (argc > 1) { 
				if (!strcasecmp(argv[1], "lock")) {
					switch_set_flag_locked(conference, CFLAG_LOCKED);
					stream->write_function(stream, "OK %s locked\n", argv[0]);
					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "lock");
						switch_event_fire(&event);
					}
					goto done;
				} else if (!strcasecmp(argv[1], "unlock")) {
					switch_clear_flag_locked(conference, CFLAG_LOCKED);
					stream->write_function(stream, "OK %s unlocked\n", argv[0]);
					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "unlock");
						switch_event_fire(&event);
					}
					goto done;
				} else if (!strcasecmp(argv[1], "dial")) {
					if (argc > 2) {
						conference_outcall(conference, NULL, argv[2], 60, argv[3], argv[4], argv[5]);
						stream->write_function(stream, "OK\n");
						goto done;
					} else {
						stream->write_function(stream, "Error!\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "play")) {
					if (argc == 3) {
						if (conference_play_file(conference, argv[2], 0, NULL) == SWITCH_STATUS_SUCCESS) {
							stream->write_function(stream, "(play) Playing file %s\n", argv[2]);
							if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
								switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
								switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "play-file");
								switch_event_add_header(event, SWITCH_STACK_BOTTOM, "File", argv[2]);
								switch_event_fire(&event);
							}
							goto done;
						} else {
							stream->write_function(stream, "(play) File: %s not found.\n", argv[2] ? argv[2] : "(unspecified)");
							goto done;
						}
					} else if (argc == 4) {
						uint32_t id = atoi(argv[3]);
						conference_member_t *member;

						if ((member = conference_member_get(conference, id))) {
							if (conference_member_play_file(member, argv[2], 0) == SWITCH_STATUS_SUCCESS) {
								stream->write_function(stream, "(play) Playing file %s to member %u\n", argv[2], id);
								if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
									switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
									switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
									switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "play-file-member");
									switch_event_add_header(event, SWITCH_STACK_BOTTOM, "File", argv[2]);
									switch_event_fire(&event);
								}
								goto done;
							} else {
								stream->write_function(stream, "(play) File: %s not found.\n", argv[2] ? argv[2] : "(unspecified)");
								goto done;
							}
						} else {
							stream->write_function(stream, "Member: %u not found.\n", id);
							goto done;
						}
					}
				} else if (!strcasecmp(argv[1], "say")) {
					char *tbuf = NULL;
					char *text;

					if (argc > 2 && (tbuf = strdup(buf))) {
						if ((text = strstr(tbuf, "say"))) {
							text += 4;
							while(*text == ' ') {
								text++;
							}
							if (!switch_strlen_zero(text) && conference_say(conference, text, 0) == SWITCH_STATUS_SUCCESS) {
								stream->write_function(stream, "(say) OK\n");
								if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
									switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
									switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "speak-text");
									switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Text", text);
									switch_event_fire(&event);
								}
								goto done;
							} else {
								stream->write_function(stream, "(say) Error!");
								goto done;
							}
						}

						free(tbuf);
					} else {
						stream->write_function(stream, "(say) Error! No text.");
					}
				} else if (!strcasecmp(argv[1], "saymember")) {
					char *tbuf = NULL, *text, *name;
					uint32_t id;
					conference_member_t *member;

					if (argc > 3) {
						id = atoi(argv[3]);
					} else {
						stream->write_function(stream, "(saymember) Syntax Error!");
						goto done;
					}

					if ((tbuf = strdup(buf))) {
						if ((name = strstr(tbuf, "saymember "))) {
							name += 10;

							if (*name) {
								text = strchr(name, ' ');
								id = atoi(name);
							} else {
								stream->write_function(stream, "(saymember) Syntax Error!");
								goto done;
							}



							if ((member = conference_member_get(conference, id))) {
								if (text && conference_member_say(conference, member, text, 0) == SWITCH_STATUS_SUCCESS) {
									stream->write_function(stream, "(saymember) OK\n");
									if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
										switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", conference->name);
										switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "speak-text-member");
										switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", id);
										switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Text", text);
										switch_event_fire(&event);
									}
								} else {
									stream->write_function(stream, "(saymember) Error!");
								}
							} else {
								stream->write_function(stream, "(saymember) Unknown Member %u!", id);
							}
						} else {
							stream->write_function(stream, "(saymember) Syntax Error!");
						}

						free(tbuf);
						goto done;
					}
				} else if (!strcasecmp(argv[1], "stop")) {
					uint8_t current = 0, all = 0;

					if (argc > 2) {
						current = strcasecmp(argv[2], "current") ? 0 : 1;
						all = strcasecmp(argv[2], "all") ? 0 : 1;
					}

					if (current || all) {
						if (argc == 4) {
							uint32_t id = atoi(argv[3]);
							conference_member_t *member;
							if ((member = conference_member_get(conference, id))) {
								uint32_t stopped = conference_member_stop_file(member, current ? FILE_STOP_CURRENT : FILE_STOP_ALL);
								stream->write_function(stream, "Stopped %u files.\n", stopped);
							} else {
								stream->write_function(stream, "Member: %u not found.\n", id);
								goto done;
							}
						} else {
							uint32_t stopped = conference_stop_file(conference, current ? FILE_STOP_CURRENT : FILE_STOP_ALL);
							stream->write_function(stream, "Stopped %u files.\n", stopped);
						}
					} else {
						stream->write_function(stream, "Usage stop [current/all]\n");
						goto done;
					}

				} else if (!strcasecmp(argv[1], "energy")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_energy_member(conference, NULL, id, stream, argv[3]);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_energy_member, argv[3]); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage energy <id|all> [<newval>]\n");
						goto done;
					}

				} else if (!strcasecmp(argv[1], "volume_in")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_volume_in_member(conference, NULL, id, stream, argv[3]);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_volume_in_member, argv[3]); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage volume_in <[id|all]> [<newval>]\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "volume_out")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_volume_out_member(conference, NULL, id, stream, argv[3]);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_volume_out_member, argv[3]); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage volume_out <[id|all> [<newval>]\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "mute")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_mute_member(conference, NULL, id, stream, NULL);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_mute_member, NULL); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage mute <[id|all]>\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "unmute")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_unmute_member(conference, NULL, id, stream, NULL);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_unmute_member, NULL); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage unmute <[id|all]>\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "deaf")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_deaf_member(conference, NULL, id, stream, NULL);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_deaf_member, NULL); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage deaf <[id|all]>\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "undeaf")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_undeaf_member(conference, NULL, id, stream, NULL);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_undeaf_member, NULL); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage undeaf <[id|all]>\n");
						goto done;
					}

				} else if (!strcasecmp(argv[1], "record")) {
					if (argc > 2) {
						launch_conference_record_thread(conference, argv[2]);
						goto done;
					} else {
						stream->write_function(stream, "usage record <filename>\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "norecord")) {
					if (argc > 2) {
						int all = (strcasecmp(argv[2], "all") == 0 );

						if(!conference_record_stop(conference, all ? NULL : argv[2]) && !all) {
							stream->write_function(stream, "non-existant recording '%s'\n", argv[2]);
						}
						goto done;
					} else {
						stream->write_function(stream, "usage norecord <[filename | all]>\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "kick")) {
					if (argc > 2) {
						uint32_t id = atoi(argv[2]);
						int all = ( id == 0 && strcasecmp(argv[2], "all") == 0 );

						if (!all) {
							conference_function_kick_member(conference, NULL, id, stream, NULL);
							goto done;
						} else {
							conference_member_itterator(conference, stream, &conference_function_kick_member, NULL); 
							goto done;
						}
					} else {
						stream->write_function(stream, "usage kick <[id|all]>\n");
						goto done;
					}
				} else if (!strcasecmp(argv[1], "transfer")) {
					char *transfer_usage = "Usage transfer <id> <confname>\n";
					if (argc > 3) {
						conference_member_t *member = NULL;
						uint32_t id = atoi(argv[2]);
						conference_obj_t *new_conference = NULL;
						switch_channel_t *channel;
						switch_event_t *event;
						char *profile_name;
						switch_xml_t cxml = NULL, cfg = NULL, profile = NULL, profiles = NULL;

						if (!(member = conference_member_get(conference, id))) {								
							stream->write_function(stream, "No Member %u in conference %s.\n", id, conference->name);
							goto done;
						}

						channel = switch_core_session_get_channel(member->session);

						if (!(new_conference = (conference_obj_t *) switch_core_hash_find(globals.conference_hash, argv[3]))) {
							switch_memory_pool_t *pool;
							char *conf_name;

							/* Setup a memory pool to use. */
							if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
								goto done;
							}

							conf_name = switch_core_strdup(pool, argv[3]);

							if ((profile_name = strchr(conf_name, '@'))) {
								*profile_name++ = '\0';

								/* Open the config from the xml registry */
								if (!(cxml = switch_xml_open_cfg(global_cf_name, &cfg, NULL))) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", global_cf_name);
									goto done;
								}

								if ((profiles = switch_xml_child(cfg, "profiles"))) {
									profile = switch_xml_find_child(profiles, "profile", "name", profile_name);
								}
							} 


							/* Release the config registry handle */
							if (cxml) {
								switch_xml_free(cxml);
								cxml = NULL;
							}

							/* Create the conference object. */
							new_conference = conference_new(conf_name, profile, pool);


							if (!new_conference) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
								goto done;
							}

							/* Set the minimum number of members (once you go above it you cannot go below it) */
							new_conference->min = 1;

							/* Indicate the conference is dynamic */
							switch_set_flag_locked(new_conference, CFLAG_DYNAMIC);

							/* Start the conference thread for this conference */
							launch_conference_thread(new_conference);
						}

						conference_del_member(member->last_conference, member);
						conference_add_member(new_conference, member);
						stream->write_function(stream, "OK Member %u sent to conference %s.\n", id, argv[3]);

						if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							switch_channel_event_set_data(channel, event);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", member->id);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Old-Conference-Name", conference->name);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-Conference-Name", argv[3]);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "transfer");
							switch_event_fire(&event);
						}

					} else {
						stream->write_function(stream, transfer_usage);
						goto done;
					}
				} else if (!strcasecmp(argv[1], "relate")) {
					char *relate_usage = "Usage relate <id> <id> [nospeak|nohear|clear]\n";
					if (argc > 4) {
						uint8_t nospeak = 0, nohear = 0, clear = 0;
						nospeak = strstr(argv[4], "nospeak") ? 1 : 0;
						nohear = strstr(argv[4], "nohear") ? 1 : 0;

						if (!strcasecmp(argv[4], "clear")) {
							clear = 1;
						}

						if (!(clear || nospeak || nohear)) { 
							stream->write_function(stream, relate_usage);
							goto done;
						}

						if (clear) {
							conference_member_t *member = NULL;
							uint32_t id = atoi(argv[2]);
							uint32_t oid = atoi(argv[3]);

							switch_mutex_lock(conference->mutex);
							switch_mutex_lock(conference->member_mutex);
							if ((member = conference_member_get(conference, id))) {
								member_del_relationship(member, oid);
								stream->write_function(stream, "relationship %u->%u cleared.", id, oid);
							} else {
								stream->write_function(stream, "relationship %u->%u not found", id, oid);
							}
							switch_mutex_unlock(conference->member_mutex);
							switch_mutex_unlock(conference->mutex);
						} else if (nospeak || nohear) {
							conference_member_t *member = NULL, *other_member = NULL;
							uint32_t id = atoi(argv[2]);
							uint32_t oid = atoi(argv[3]);

							switch_mutex_lock(conference->mutex);
							switch_mutex_lock(conference->member_mutex);
							if ((member = conference_member_get(conference, id)) && (other_member = conference_member_get(conference, oid))) {
								conference_relationship_t *rel = NULL;
								if ((rel = member_get_relationship(member, other_member))) {
									rel->flags = 0;
								} else {
									rel = member_add_relationship(member, oid);
								}

								if (rel) {
									switch_set_flag(rel, RFLAG_CAN_SPEAK | RFLAG_CAN_HEAR);
									if (nospeak) {
										switch_clear_flag(rel, RFLAG_CAN_SPEAK);
									}
									if (nohear) {
										switch_clear_flag(rel, RFLAG_CAN_HEAR);
									}
									stream->write_function(stream, "ok %u->%u set\n", id, oid);
								} else {
									stream->write_function(stream, "error!\n");
								}

							} else {
								stream->write_function(stream, "relationship %u->%u not found", id, oid);
							}
							switch_mutex_unlock(conference->member_mutex);
							switch_mutex_unlock(conference->mutex);
						}


					} else {
						stream->write_function(stream, relate_usage);
					}
				} else if (!strcasecmp(argv[1], "list")) {
					char *d = ";";

					if (argv[2]) {
						if (argv[3] && !strcasecmp(argv[2], "delim")) {
							d = argv[3];

							if (*d == '"') {
								if (++d) {
									char *p;
									if ((p = strchr(d, '"'))) {
										*p = '\0';
									}
								} else {
									d = ";";
								}
							}
						}
					}
					conference_list(conference, stream, d);
				} else {
					stream->write_function(stream, "Command: %s not found.\n", argv[1]);
					goto done;
				}

			} else {
				stream->write_function(stream, "Command not specified.\n");
				goto done;
			}
		} else {
			stream->write_function(stream, "USAGE: %s\n", conf_api_interface.syntax);
		}
	} else {
		stream->write_function(stream, "Memory Error!\n");
	}

done:

	if (lbuf) {
		free(lbuf);
	}

	return status;

}

static switch_status_t audio_bridge_on_ring(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CUSTOM RING\n");

	/* put the channel in a passive state so we can loop audio to it */
	switch_channel_set_state(channel, CS_TRANSMIT);
	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t audio_bridge_peer_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ audio_bridge_on_ring,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL,
	/*.on_hold */ NULL,
};

static switch_status_t conference_outcall(conference_obj_t *conference,
										  switch_core_session_t *session,
										  char *bridgeto,
										  uint32_t timeout,
										  char *flags,
										  char *cid_name,
										  char *cid_num)
{
	switch_core_session_t *peer_session;
	switch_channel_t *peer_channel;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *caller_channel = NULL;
	char appdata[512];
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;


	if (switch_thread_rwlock_tryrdlock(conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Read Lock Fail\n");
		return SWITCH_STATUS_FALSE;
	}

	if (session) {
		caller_channel = switch_core_session_get_channel(session);
	
	}

	if (switch_ivr_originate(session,
							 &peer_session,
							 &cause,
							 bridgeto,
							 timeout,
							 &audio_bridge_peer_state_handlers,
							 cid_name,
							 cid_num,
							 NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create outgoing channel, cause: %s\n",
				  switch_channel_cause2str(cause));
		if (caller_channel) {
			switch_channel_hangup(caller_channel, cause);
		}
		goto done;
	} 


	peer_channel = switch_core_session_get_channel(peer_session);
	assert(peer_channel != NULL);

	if (!switch_test_flag(conference, CFLAG_RUNNING)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Conference is gone now, nevermind..\n");
		if (caller_channel) {
			switch_channel_hangup(caller_channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
		}
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
		goto done;
	}

	if (caller_channel && switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
		switch_channel_answer(caller_channel);
	}

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
		switch_caller_extension_t *extension = NULL;
		if ((extension = switch_caller_extension_new(peer_session, conference->name, conference->name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
			status = SWITCH_STATUS_MEMERR;
			goto done;
		}
		/* add them to the conference */
		if (flags) {
			snprintf(appdata, sizeof(appdata), "%s +flags{%s}", conference->name, flags);
			switch_caller_extension_add_application(peer_session, extension, (char *) global_app_name, appdata);
		} else {
			switch_caller_extension_add_application(peer_session, extension, (char *) global_app_name, conference->name);
		}

		switch_channel_set_caller_extension(peer_channel, extension);
		switch_channel_set_state(peer_channel, CS_EXECUTE);

	} else {
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ANSWER);
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

done:
	switch_thread_rwlock_unlock(conference->rwlock);
	return status;
}

/* Play a file */
static switch_status_t conference_local_play_file(switch_core_session_t *session, char *path, uint32_t leadin, char *buf, switch_size_t len)
{
	uint32_t x = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	for (x = 0; x < leadin; x++) {
		switch_frame_t *read_frame;
		switch_status_t status = switch_core_session_read_frame(session, &read_frame, 1000, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		status = switch_ivr_play_file(session, NULL, path, NULL, NULL, NULL, 0);
	}

	return status;
}

/* Application interface function that is called from the dialplan to join the channel to a conference */
static void conference_function(switch_core_session_t *session, char *data)
{
	switch_codec_t *read_codec = NULL;
	switch_memory_pool_t *pool = NULL, *freepool = NULL;
	uint32_t flags = 0;
	conference_member_t member = {0};
	conference_obj_t *conference = NULL;
	switch_channel_t *channel = NULL;
	char *mydata = switch_core_session_strdup(session, data);
	char *conf_name = NULL;
	char *bridge_prefix = "bridge:";
	char *flags_prefix = "+flags{";
	char *bridgeto = NULL;
	char *profile_name = NULL;
	switch_xml_t cxml = NULL, cfg = NULL, profile = NULL, profiles = NULL;
	char *flags_str;
	member_flag_t uflags = MFLAG_CAN_SPEAK | MFLAG_CAN_HEAR;
	switch_core_session_message_t msg = {0};
	uint8_t isbr = 0;
	char *dpin = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (!mydata) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return;
	}

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
		return;
	}


	if ((flags_str=strstr(mydata, flags_prefix))) {
		char *p;

		*flags_str = '\0';
		flags_str += strlen(flags_prefix);
		if ((p = strchr(flags_str, '}'))) {
			*p = '\0';
		}

		if (strstr(flags_str, "mute")) {
			uflags &= ~MFLAG_CAN_SPEAK;
		} else if (strstr(flags_str, "deaf")) {
			uflags &= ~MFLAG_CAN_HEAR;
		}
	}

	if (!strncasecmp(mydata, bridge_prefix, strlen(bridge_prefix))) {
		isbr = 1;
		mydata += strlen(bridge_prefix);
		if ((bridgeto = strchr(mydata, ':'))) {
			*bridgeto++ = '\0';
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Config Error!\n");
			goto done;
		}
	}

	conf_name = mydata;

	if ((dpin = strchr(conf_name, '+'))) {
		*dpin++ = '\0';
	}

	if ((profile_name = strchr(conf_name, '@'))) {
		*profile_name++ = '\0';

		/* Open the config from the xml registry */
		if (!(cxml = switch_xml_open_cfg(global_cf_name, &cfg, NULL))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", global_cf_name);
			goto done;
		}

		if ((profiles = switch_xml_child(cfg, "profiles"))) {
			profile = switch_xml_find_child(profiles, "profile", "name", profile_name);
		}
	} 

	if (isbr) {
		char *uuid = switch_core_session_get_uuid(session);

		if (!strcmp(conf_name, "_uuid_")) {
			conf_name = uuid;
		}

		if ((conference = (conference_obj_t *) switch_core_hash_find(globals.conference_hash, conf_name))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Conference %s already exists!\n", conf_name);
			goto done;
		}

		/* Create the conference object. */
		conference = conference_new(conf_name, profile, pool);

		if (!conference) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
			goto done;
		}

		/* Set the minimum number of members (once you go above it you cannot go below it) */
		conference->min = 2;

		if (dpin) {
			conference->pin = switch_core_strdup(conference->pool, dpin);
		}

		/* Indicate the conference is dynamic */
		switch_set_flag_locked(conference, CFLAG_DYNAMIC);	

		/* Start the conference thread for this conference */
		launch_conference_thread(conference);

	} else {
		/* Figure out what conference to call. */
		if ((conference = (conference_obj_t *) switch_core_hash_find(globals.conference_hash, conf_name))) {
			freepool = pool;
		} else {
			/* Create the conference object. */
			conference = conference_new(conf_name, profile, pool);


			if (!conference) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
				goto done;
			}

			if (dpin) {
				conference->pin = switch_core_strdup(conference->pool, dpin);
			}

			/* Set the minimum number of members (once you go above it you cannot go below it) */
			conference->min = 1;

			/* Indicate the conference is dynamic */
			switch_set_flag_locked(conference, CFLAG_DYNAMIC);

			/* Start the conference thread for this conference */
			launch_conference_thread(conference);

		}

		if (conference->pin) {
			char term = '\0';
			char pin[80] = "";
			char *buf;

			/* Answer the channel */
			switch_channel_answer(channel);

			if (conference->pin_sound) {
				conference_local_play_file(session, conference->pin_sound, 20, pin, sizeof(pin));
			} 

			if (strlen(pin) < strlen(conference->pin)) {
				buf = pin + strlen(pin);
				switch_ivr_collect_digits_count(session,
												buf,
												sizeof(pin) - (unsigned int)strlen(pin),
												(unsigned int)strlen(conference->pin) - (unsigned int)strlen(pin),
												"#", &term, 10000);
			}

			if (strcmp(pin, conference->pin)) {
				if (conference->bad_pin_sound) {
					conference_local_play_file(session, conference->bad_pin_sound, 20, NULL, 0);
				}
				goto done;
			}
		}

		if (switch_test_flag(conference, CFLAG_LOCKED)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Conference %s is locked.\n", conf_name);
			if (conference->locked_sound) {
				/* Answer the channel */
				switch_channel_answer(channel);
				conference_local_play_file(session, conference->locked_sound, 20, NULL, 0);
			}
			goto done;
		}
	}

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}

	if (!switch_strlen_zero(bridgeto) && strcasecmp(bridgeto, "none")) {
		if (conference_outcall(conference, session, bridgeto, 60, NULL, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}
	} else {	
		// if we're not using "bridge:" set the conference answered flag
		// and this isn't an outbound channel, answer the call
		if (!switch_channel_test_flag(channel, CF_OUTBOUND)) 
			switch_set_flag(conference, CFLAG_ANSWERED);
	}

	/* Save the original read codec. */
	read_codec = switch_core_session_get_read_codec(session);
	member.native_rate = read_codec->implementation->samples_per_second;

	/* Setup a Signed Linear codec for reading audio. */
	if (switch_core_codec_init(&member.read_codec,
							   "L16",
							   NULL,
							   //conference->rate,
							   read_codec->implementation->samples_per_second,
							   //conference->interval,
							   read_codec->implementation->microseconds_per_frame / 1000,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL,
							   pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Success L16@%uhz 1 channel %dms\n",
						  conference->rate, conference->interval);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz 1 channel %dms\n",
						  conference->rate, conference->interval);
		flags = 0;
		goto done;
	}

	if (read_codec->implementation->samples_per_second != conference->rate) {
		switch_audio_resampler_t **resampler = read_codec->implementation->samples_per_second > conference->rate ? 
			&member.read_resampler : &member.mux_resampler;

		switch_resample_create(resampler,
							   read_codec->implementation->samples_per_second,
							   read_codec->implementation->samples_per_second * 20,
							   conference->rate,
							   conference->rate * 20,
							   switch_core_session_get_pool(session));

		/* Setup an audio buffer for the resampled audio */
		if (switch_buffer_create_dynamic(&member.resample_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
			goto done;
		}
	}
	/* Setup a Signed Linear codec for writing audio. */
	if (switch_core_codec_init(&member.write_codec,
							   "L16",
							   NULL,
							   conference->rate,
							   //read_codec->implementation->samples_per_second,
							   conference->interval,
							   //read_codec->implementation->microseconds_per_frame / 1000,
							   1,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   NULL,
							   pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Success L16@%uhz 1 channel %dms\n",
						  conference->rate, conference->interval);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz 1 channel %dms\n",
						  conference->rate, conference->interval);
		flags = 0;
		goto codec_done2;
	}

	/* Setup an audio buffer for the incoming audio */
	if (switch_buffer_create_dynamic(&member.audio_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto codec_done1;
	}

	/* Setup an audio buffer for the outgoing audio */
	if (switch_buffer_create_dynamic(&member.mux_buffer, CONF_DBLOCK_SIZE, CONF_DBUFFER_SIZE, CONF_DBUFFER_MAX) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error Creating Audio Buffer!\n");
		goto codec_done1;
	}

	/* Prepare MUTEXS */
	member.id = next_member_id();
	member.pool = pool;
	member.session = session;
	switch_mutex_init(&member.flag_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&member.audio_in_mutex, SWITCH_MUTEX_NESTED, pool);
	switch_mutex_init(&member.audio_out_mutex, SWITCH_MUTEX_NESTED, pool);

	/* Install our Signed Linear codec so we get the audio in that format */
	switch_core_session_set_read_codec(member.session, &member.read_codec);

	/* Add the caller to the conference */
	if (conference_add_member(conference, &member) != SWITCH_STATUS_SUCCESS) {
		goto codec_done1;
	}
	switch_set_flag_locked((&member), MFLAG_RUNNING | uflags);

	msg.from = __FILE__;

	/* Tell the channel we are going to be in a bridge */
	msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
	switch_core_session_receive_message(session, &msg);

	/* Run the confernece loop */
	conference_loop(&member);

	/* Tell the channel we are no longer going to be in a bridge */
	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	switch_core_session_receive_message(session, &msg);

	/* Remove the caller from the conference */
	conference_del_member(member.last_conference, &member);

	/* Put the original codec back */
	switch_core_session_set_read_codec(member.session, read_codec);

	/* Clean Up.  codec_done(X): is for error situations after the codecs were setup and done: is for situations before */
codec_done1:
	switch_core_codec_destroy(&member.read_codec);
codec_done2:
	switch_core_codec_destroy(&member.write_codec);
done:

	switch_buffer_destroy(&member.resample_buffer);
	switch_buffer_destroy(&member.audio_buffer);
	switch_buffer_destroy(&member.mux_buffer);

	/* Release the config registry handle */
	if (cxml) {
		switch_xml_free(cxml);
	}

	if (freepool) {
		switch_core_destroy_memory_pool(&freepool);
	}

	if (switch_test_flag(&member, MFLAG_KICKED) && conference->kicked_sound) {
		switch_ivr_play_file(session, NULL, conference->kicked_sound, NULL, NULL, NULL, 0);
	}

	switch_core_session_reset(session);

}

/* Create a thread for the conference and launch it */
static void launch_conference_thread(conference_obj_t *conference)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_set_flag_locked(conference, CFLAG_RUNNING);
	switch_threadattr_create(&thd_attr, conference->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_mutex_lock(globals.hash_mutex);
	switch_core_hash_insert(globals.conference_hash, conference->name, conference);
	switch_mutex_unlock(globals.hash_mutex);
	switch_thread_create(&thread, thd_attr, conference_thread_run, conference, conference->pool);
}

static void launch_conference_record_thread(conference_obj_t *conference, char *path)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_memory_pool_t *pool;
	conference_record_t *rec;

	/* Setup a memory pool to use. */
	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
	}

	/* Create a node object*/
	if (!(rec = switch_core_alloc(pool, sizeof(*rec)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
		switch_core_destroy_memory_pool(&pool);
	}

	rec->conference = conference;
	rec->path = switch_core_strdup(pool, path);
	rec->pool = pool;

	switch_threadattr_create(&thd_attr, rec->pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, conference_record_thread_run, rec, rec->pool);
}

static void *SWITCH_THREAD_FUNC input_thread_run(switch_thread_t *thread, void *obj)
{
	conference_member_t *member = obj;
	switch_channel_t *channel;
	switch_status_t status;
	switch_frame_t *read_frame = NULL;
	switch_codec_t *read_codec;
	uint32_t hangover = 40,
		hangunder = 15,
		hangover_hits = 0,
		hangunder_hits = 0,
		energy_level = 0,
		diff_level = 400;
	uint8_t talking = 0;

	assert(member != NULL);

	channel = switch_core_session_get_channel(member->session);
	assert(channel != NULL);

	read_codec = switch_core_session_get_read_codec(member->session);
	assert(read_codec != NULL);

	/* As long as we have a valid read, feed that data into an input buffer where the conference thread will take it 
	   and mux it with any audio from other channels. */

	while(switch_test_flag(member, MFLAG_RUNNING) && switch_channel_ready(channel)) {
		/* Read a frame. */
		status = switch_core_session_read_frame(member->session, &read_frame, -1, 0);

		/* end the loop, if appropriate */
		if (!SWITCH_READ_ACCEPTABLE(status) || !switch_test_flag(member, MFLAG_RUNNING)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		energy_level = member->energy_level;

		if (switch_test_flag(member, MFLAG_CAN_SPEAK) && energy_level) {
			uint32_t energy = 0, i = 0, samples = 0, j = 0, score = 0;
			int16_t *data;

			data = read_frame->data;
			samples = read_frame->datalen / sizeof(*data);

			for (i = 0; i < samples; i++) {
				energy += abs(data[j]);
				j += read_codec->implementation->number_of_channels;
			}

			score = energy / samples;

			if (score > energy_level) {
				uint32_t diff = score - energy_level;
				if (hangover_hits) {
					hangover_hits--;
				}

				if (diff >= diff_level || ++hangunder_hits >= hangunder) {
					hangover_hits = hangunder_hits = 0;

					if (!talking) {
						switch_event_t *event;
						talking = 1;		
						if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							switch_channel_event_set_data(channel, event);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", member->conference->name);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", member->id);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "start-talking");
							switch_event_fire(&event);
						}
					}
				} 
			} else {
				if (hangunder_hits) {
					hangunder_hits--;
				}
				if (talking) {
					switch_event_t *event;
					if (++hangover_hits >= hangover) {
						hangover_hits = hangunder_hits = 0;
						talking = 0;

						if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
							switch_channel_event_set_data(channel, event);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Conference-Name", member->conference->name);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Member-ID", "%u", member->id);
							switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Action", "stop-talking");
							switch_event_fire(&event);
						}					
					}
				}
			}
		}

		/* skip frames that are not actual media or when we are muted or silent */
		if ((talking || energy_level == 0) && switch_test_flag(member, MFLAG_CAN_SPEAK)) {
			if (member->read_resampler) {
				int16_t *bptr = (int16_t *) read_frame->data;
				int len = (int) read_frame->datalen;;

				member->read_resampler->from_len = switch_short_to_float(bptr, member->read_resampler->from, (int) len / 2);
				member->read_resampler->to_len = switch_resample_process(member->read_resampler, member->read_resampler->from,
																		 member->read_resampler->from_len, member->read_resampler->to,
																		 member->read_resampler->to_size, 0);
				switch_float_to_short(member->read_resampler->to, read_frame->data, len);
				len = member->read_resampler->to_len * 2;
				read_frame->datalen = len;
				read_frame->samples = len / 2;
			}
			/* Check for input volume adjustments */
			if (member->volume_in_level) {
				switch_change_sln_volume(read_frame->data, read_frame->datalen / 2, member->volume_in_level);
			}

			/* Write the audio into the input buffer */
			switch_mutex_lock(member->audio_in_mutex);
			switch_buffer_write(member->audio_buffer, read_frame->data, read_frame->datalen);
			switch_mutex_unlock(member->audio_in_mutex);
		}
	}

	switch_clear_flag_locked(member, MFLAG_ITHREAD);

	return NULL;
}

/* Create a thread for the conference and launch it */
static void launch_input_thread(conference_member_t *member, switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_set_flag_locked(member, MFLAG_ITHREAD);
	switch_thread_create(&thread, thd_attr, input_thread_run, member, pool);
}

static const switch_application_interface_t conference_application_interface = {
	/*.interface_name */ global_app_name,
	/*.application_function */ conference_function,
	NULL, NULL, NULL,
	/*.next*/ NULL
};

static switch_api_interface_t conf_api_interface = {
	/*.interface_name */ "conference",
	/*.desc */ "Conference",
	/*.function */ conf_function,
	/*.syntax */ 
		"list [delim <string>]\n"
		"\t<confname> list [delim <string>]\n"
		"\t<confname> energy <member_id|all> [<newval>]\n"
		"\t<confname> volume_in <member_id|all> [<newval>]\n"
		"\t<confname> volume_out <member_id|all> [<newval>]\n"
		"\t<confname> play <file_path> [<member_id>]\n"
		"\t<confname> say <text>\n"
		"\t<confname> saymember <member_id><text>\n"
		"\t<confname> stop <[current|all]> [<member_id>]\n"
		"\t<confname> kick <[member_id|all]>\n"
		"\t<confname> mute <[member_id|all]>\n"
		"\t<confname> unmute <[member_id|all]>\n"
		"\t<confname> deaf <[member_id|all]>\n"
		"\t<confname> undef <[member_id|all]>\n"
		"\t<confname> relate <member_id> <other_member_id> [nospeak|nohear]\n"
		"\t<confname> lock\n"
		"\t<confname> unlock\n"
		"\t<confname> dial <endpoint_module_name>/<destination>\n"
		"\t<confname> transfer <member_id> <conference_name>\n"
		"\t<confname> record <filename>\n"
		"\t<confname> norecord <[filename|all]>\n",
	/*.next */ 
};

static switch_status_t chat_send(char *proto, char *from, char *to, char *subject, char *body, char *hint)
{
	char name[512] = "", *p;
	switch_chat_interface_t *ci;
	conference_obj_t *conference = NULL;
	switch_stream_handle_t stream = {0};

	if ((p = strchr(to, '+'))) {
		to = ++p;
	}

	if (!body) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(ci = switch_loadable_module_get_chat_interface(proto))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invaid Chat Interface [%s]!\n", proto);
	}


	if ((p = strchr(to, '@'))) {
		switch_copy_string(name, to, ++p-to);
	} else {
		switch_copy_string(name, to, sizeof(name));
	}

	if (!(conference = (conference_obj_t *) switch_core_hash_find(globals.conference_hash, name))) {
		ci->chat_send(CONF_CHAT_PROTO, to, hint && strchr(hint, '/') ? hint : from, "", "Sorry, We're Closed", NULL);
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_STANDARD_STREAM(stream);

	if (strstr(body, "list")) {
		conference_list_pretty(conference, &stream);
	} else {
		stream.write_function(&stream, "The only command we have so far is 'list'.\nGet coding or go press PayPal!!\n");
	}

	ci->chat_send(CONF_CHAT_PROTO, to, from, "", stream.data, NULL);
	switch_safe_free(stream.data);



	return SWITCH_STATUS_SUCCESS;
}

static const switch_chat_interface_t conference_chat_interface = {
	/*.name */ CONF_CHAT_PROTO,
	/*.chat_send */ chat_send,

};

static switch_loadable_module_interface_t conference_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ &conference_application_interface,
	/*.api_interface */ &conf_api_interface,
	/*.file_interface */ NULL,
	/*.speech_interface */ NULL,
	/*.directory_interface */ NULL,
	/*.chat_interface */ &conference_chat_interface
};

/* create a new conferene with a specific profile */
static conference_obj_t *conference_new(char *name, switch_xml_t profile, switch_memory_pool_t *pool)
{
	conference_obj_t *conference;
	switch_xml_t param;
	char *rate_name = NULL;
	char *interval_name = NULL;
	char *timer_name = NULL;
	char *domain = NULL;
	char *tts_engine = NULL;
	char *tts_voice = NULL;
	char *enter_sound = NULL;
	char *exit_sound = NULL;
	char *alone_sound = NULL;
	char *ack_sound = NULL;
	char *nack_sound = NULL;
	char *muted_sound = NULL;
	char *unmuted_sound = NULL;
	char *locked_sound = NULL;
	char *kicked_sound = NULL;
	char *pin = NULL;
	char *pin_sound = NULL; 
	char *bad_pin_sound = NULL;
	char *energy_level = NULL;
	char *caller_id_name = NULL;
	char *caller_id_number = NULL;
	uint32_t rate = 8000, interval = 20;
	switch_status_t status;

	/* Conference Name */
	if (switch_strlen_zero(name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Record! no name.\n");
		return NULL;
	}

	for (param = switch_xml_child(profile, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");
		char buf[128] = "";
		char *p;

		if ((p = strchr(var, '_'))) {
			switch_copy_string(buf, var, sizeof(buf));
			for(p = buf; *p; p++) {
				if (*p == '_') {
					*p = '-';
				}
			}
			var = buf;
		}

		if (!strcasecmp(var, "rate")) {
			rate_name = val;
		} else if (!strcasecmp(var, "domain")) {
			domain = val;
		} else if (!strcasecmp(var, "interval")) {
			interval_name= val;
		} else if (!strcasecmp(var, "timer-name")) {
			timer_name= val;
		} else if (!strcasecmp(var, "tts-engine")) {
			tts_engine= val;
		} else if (!strcasecmp(var, "tts-voice")) {
			tts_voice= val;
		} else if (!strcasecmp(var, "enter-sound")) {
			enter_sound = val;
		} else if (!strcasecmp(var, "exit-sound")) {
			exit_sound = val;
		} else if (!strcasecmp(var, "alone-sound")) {
			alone_sound = val;
		} else if (!strcasecmp(var, "ack-sound")) {
			ack_sound = val;
		} else if (!strcasecmp(var, "nack-sound")) {
			nack_sound = val;
		} else if (!strcasecmp(var, "muted-sound")) {
			muted_sound = val;
		} else if (!strcasecmp(var, "unmuted-sound")) {
			unmuted_sound = val;
		} else if (!strcasecmp(var, "locked-sound")) {
			locked_sound= val;
		} else if (!strcasecmp(var, "kicked-sound")) {
			kicked_sound = val;
		} else if (!strcasecmp(var, "pin")) {
			pin = val;
		} else if (!strcasecmp(var, "pin-sound")) {
			pin_sound = val;
		} else if (!strcasecmp(var, "bad-pin-sound")) {
			bad_pin_sound = val;
		} else if (!strcasecmp(var, "energy-level")) {
			energy_level = val;
		} else if (!strcasecmp(var, "caller-id-name")) {
			caller_id_name = val;
		} else if (!strcasecmp(var, "caller-id-number")) {
			caller_id_number = val;
		}
	}

	/* Set defaults and various paramaters */

	/* Speed in hertz */
	if (!switch_strlen_zero(rate_name)) {
		uint32_t r = atoi(rate_name);
		if (r) {
			rate = r;
		}
	}

	/* Packet Interval in milliseconds */
	if (!switch_strlen_zero(interval_name)) {
		uint32_t i = atoi(interval_name);
		if (i) {
			interval = i;
		}
	}

	/* Timer module to use */
	if (switch_strlen_zero(timer_name)) {
		timer_name = "soft";
	}

	/* Caller ID Name */
	if (switch_strlen_zero(caller_id_name)) {
		caller_id_name = (char *) global_app_name;
	}

	/* Caller ID Number */
	if (switch_strlen_zero(caller_id_number)) {
		caller_id_number = "0000000000";
	}

	if (!pool) {
		/* Setup a memory pool to use. */
		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
			status = SWITCH_STATUS_TERM;
			return NULL;
		}
	}

	/* Create the conference object. */
	if (!(conference = switch_core_alloc(pool, sizeof(*conference)))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Memory Error!\n");
		status = SWITCH_STATUS_TERM;
		return NULL;
	}

	/* Initilize the object with some settings */
	conference->pool = pool;
	conference->timer_name = switch_core_strdup(conference->pool, timer_name);
	conference->tts_engine = switch_core_strdup(conference->pool, tts_engine);
	conference->tts_voice = switch_core_strdup(conference->pool, tts_voice);

	conference->caller_id_name = switch_core_strdup(conference->pool, caller_id_name);
	conference->caller_id_number = switch_core_strdup(conference->pool, caller_id_number);

	if (!switch_strlen_zero(enter_sound)) {
		conference->enter_sound = switch_core_strdup(conference->pool, enter_sound);
	}

	if (!switch_strlen_zero(exit_sound)) {
		conference->exit_sound = switch_core_strdup(conference->pool, exit_sound);
	}

	if (!switch_strlen_zero(ack_sound)) {
		conference->ack_sound = switch_core_strdup(conference->pool, ack_sound);
	}

	if (!switch_strlen_zero(nack_sound)) {
		conference->nack_sound = switch_core_strdup(conference->pool, nack_sound);
	}

	if (!switch_strlen_zero(muted_sound)) {
		conference->muted_sound = switch_core_strdup(conference->pool, muted_sound);
	}

	if (!switch_strlen_zero(unmuted_sound)) {
		conference->unmuted_sound = switch_core_strdup(conference->pool, unmuted_sound);
	}

	if (!switch_strlen_zero(kicked_sound)) {
		conference->kicked_sound = switch_core_strdup(conference->pool, kicked_sound);
	}

	if (!switch_strlen_zero(pin_sound)) {
		conference->pin_sound = switch_core_strdup(conference->pool, pin_sound);
	}

	if (!switch_strlen_zero(bad_pin_sound)) {
		conference->bad_pin_sound = switch_core_strdup(conference->pool, bad_pin_sound);
	}

	if (!switch_strlen_zero(pin)) {
		conference->pin = switch_core_strdup(conference->pool, pin);
	}

	if (!switch_strlen_zero(alone_sound)) {
		conference->alone_sound = switch_core_strdup(conference->pool, alone_sound);
	} 

	if (!switch_strlen_zero(locked_sound)) {
		conference->locked_sound = switch_core_strdup(conference->pool, locked_sound);
	}

	if (!switch_strlen_zero(energy_level)) {
		conference->energy_level = atoi(energy_level);
	}

	conference->name = switch_core_strdup(conference->pool, name);
	if (domain) {
		conference->domain = switch_core_strdup(conference->pool, domain);
	} else {
		conference->domain = "cluecon.com";
	}
	conference->rate = rate;
	conference->interval = interval;


	/* Activate the conference mutex for exclusivity */
	switch_mutex_init(&conference->mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_mutex_init(&conference->member_mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_mutex_init(&conference->flag_mutex, SWITCH_MUTEX_NESTED, conference->pool);
	switch_thread_rwlock_create(&conference->rwlock, conference->pool);

	return conference;
}

/* Called by FreeSWITCH when the module loads */
SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	memset(&globals, 0, sizeof(globals));

	/* Connect my internal structure to the blank pointer passed to me */
	*module_interface = &conference_module_interface;

	if (switch_event_reserve_subclass(CONF_EVENT_MAINT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!", CONF_EVENT_MAINT);
		return SWITCH_STATUS_TERM;
	}

	/* Setup the pool */
	if (switch_core_new_memory_pool(&globals.conference_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no conference pool\n");
		return SWITCH_STATUS_TERM;
	}

	/* Setup a hash to store conferences by name */
	switch_core_hash_init(&globals.conference_hash, globals.conference_pool);
	switch_mutex_init(&globals.conference_mutex, SWITCH_MUTEX_NESTED, globals.conference_pool);
	switch_mutex_init(&globals.id_mutex, SWITCH_MUTEX_NESTED, globals.conference_pool);
	switch_mutex_init(&globals.hash_mutex, SWITCH_MUTEX_NESTED, globals.conference_pool);

	globals.running = 1;
	/* indicate that the module should continue to be loaded */
	return status;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{


	if (globals.running) {
		globals.running = 0;
		while (globals.threads) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for %d threads\n", globals.threads);
			switch_yield(100000);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

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
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter at 0xdecafbad dot com>
 * Dale Thatcher <freeswitch at dalethatcher dot com>
 * Chris Danielson <chris at maxpowersoft dot com>
 * Rupa Schomaker <rupa@rupa.com>
 * David Weekly <david@weekly.org>
 * Joao Mesquita <jmesquita@gmail.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */
#include <mod_conference.h>

const char *conference_utils_combine_flag_var(switch_core_session_t *session, const char *var_name)
{
	switch_event_header_t *hp;
	switch_event_t *event, *cevent;
	char *ret = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_core_get_variables(&event);
	switch_channel_get_variables(channel, &cevent);
	switch_event_merge(event, cevent);


	for (hp = event->headers; hp; hp = hp->next) {
		char *var = hp->name;
		char *val = hp->value;

		if (!strcasecmp(var, var_name)) {
			if (hp->idx) {
				int i;
				for (i = 0; i < hp->idx; i++) {
					if (zstr(ret)) {
						ret = switch_core_session_sprintf(session, "%s", hp->array[i]);
					} else {
						ret = switch_core_session_sprintf(session, "%s|%s", ret, hp->array[i]);
					}
				}
			} else {
				if (zstr(ret)) {
					ret = switch_core_session_sprintf(session, "%s", val);
				} else {
					ret = switch_core_session_sprintf(session, "%s|%s", ret, val);
				}
			}
		} else if (!strncasecmp(var, var_name, strlen(var_name)) && switch_true(val)) {
			char *p = var + strlen(var_name);

			if (*p == '_' && *(p+1)) {
				p++;
				ret = switch_core_session_sprintf(session, "%s|%s", ret, p);
			}
		}
	}


	switch_event_destroy(&event);
	switch_event_destroy(&cevent);

	return ret;

}

void conference_utils_set_mflags(const char *flags, member_flag_t *f)
{
	if (flags) {
		char *dup = strdup(flags);
		char *p;
		char *argv[10] = { 0 };
		int i, argc = 0;

		f[MFLAG_CAN_SPEAK] = f[MFLAG_CAN_HEAR] = f[MFLAG_CAN_BE_SEEN] = 1;

		for (p = dup; p && *p; p++) {
			if (*p == ',') {
				*p = '|';
			}
		}

		argc = switch_separate_string(dup, '|', argv, (sizeof(argv) / sizeof(argv[0])));

		for (i = 0; i < argc && argv[i]; i++) {
			if (!strcasecmp(argv[i], "mute")) {
				f[MFLAG_CAN_SPEAK] = 0;
				f[MFLAG_TALKING] = 0;
            } else if (!strcasecmp(argv[i], "vmute")) {
                f[MFLAG_CAN_BE_SEEN] = 0;
			} else if (!strcasecmp(argv[i], "deaf")) {
				f[MFLAG_CAN_HEAR] = 0;
			} else if (!strcasecmp(argv[i], "mute-detect")) {
				f[MFLAG_MUTE_DETECT] = 1;
			} else if (!strcasecmp(argv[i], "dist-dtmf")) {
				f[MFLAG_DIST_DTMF] = 1;
			} else if (!strcasecmp(argv[i], "moderator")) {
				f[MFLAG_MOD] = 1;
			} else if (!strcasecmp(argv[i], "nomoh")) {
				f[MFLAG_NOMOH] = 1;
			} else if (!strcasecmp(argv[i], "endconf")) {
				f[MFLAG_ENDCONF] = 1;
			} else if (!strcasecmp(argv[i], "mintwo")) {
				f[MFLAG_MINTWO] = 1;
			} else if (!strcasecmp(argv[i], "video-bridge")) {
				f[MFLAG_VIDEO_BRIDGE] = 1;
			} else if (!strcasecmp(argv[i], "ghost")) {
				f[MFLAG_GHOST] = 1;
			} else if (!strcasecmp(argv[i], "join-only")) {
				f[MFLAG_JOIN_ONLY] = 1;
			} else if (!strcasecmp(argv[i], "flip-video")) {
				f[MFLAG_FLIP_VIDEO] = 1;
			} else if (!strcasecmp(argv[i], "positional")) {
				f[MFLAG_POSITIONAL] = 1;
			} else if (!strcasecmp(argv[i], "no-positional")) {
				f[MFLAG_NO_POSITIONAL] = 1;
			} else if (!strcasecmp(argv[i], "join-vid-floor")) {
				f[MFLAG_JOIN_VID_FLOOR] = 1;
			} else if (!strcasecmp(argv[i], "no-minimize-encoding")) {
				f[MFLAG_NO_MINIMIZE_ENCODING] = 1;
			} else if (!strcasecmp(argv[i], "second-screen")) {
				f[MFLAG_SECOND_SCREEN] = 1;
				f[MFLAG_CAN_SPEAK] = 0;
				f[MFLAG_TALKING] = 0;
				f[MFLAG_CAN_HEAR] = 0;
				f[MFLAG_SILENT] = 1;
			}
		}

		free(dup);
	}
}



void conference_utils_set_cflags(const char *flags, conference_flag_t *f)
{
	if (flags) {
		char *dup = strdup(flags);
		char *p;
		char *argv[10] = { 0 };
		int i, argc = 0;

		for (p = dup; p && *p; p++) {
			if (*p == ',') {
				*p = '|';
			}
		}

		argc = switch_separate_string(dup, '|', argv, (sizeof(argv) / sizeof(argv[0])));

		for (i = 0; i < argc && argv[i]; i++) {
			if (!strcasecmp(argv[i], "wait-mod")) {
				f[CFLAG_WAIT_MOD] = 1;
			} else if (!strcasecmp(argv[i], "video-floor-only")) {
				f[CFLAG_VID_FLOOR] = 1;
			} else if (!strcasecmp(argv[i], "audio-always")) {
				f[CFLAG_AUDIO_ALWAYS] = 1;
			} else if (!strcasecmp(argv[i], "restart-auto-record")) {
				f[CFLAG_CONF_RESTART_AUTO_RECORD] = 1;
			} else if (!strcasecmp(argv[i], "json-events")) {
				f[CFLAG_JSON_EVENTS] = 1;
			} else if (!strcasecmp(argv[i], "livearray-sync")) {
				f[CFLAG_LIVEARRAY_SYNC] = 1;
			} else if (!strcasecmp(argv[i], "livearray-json-status")) {
				f[CFLAG_JSON_STATUS] = 1;
			} else if (!strcasecmp(argv[i], "rfc-4579")) {
				f[CFLAG_RFC4579] = 1;
			} else if (!strcasecmp(argv[i], "auto-3d-position")) {
				f[CFLAG_POSITIONAL] = 1;
			} else if (!strcasecmp(argv[i], "minimize-video-encoding")) {
				f[CFLAG_MINIMIZE_VIDEO_ENCODING] = 1;
			} else if (!strcasecmp(argv[i], "video-bridge-first-two")) {
				f[CFLAG_VIDEO_BRIDGE_FIRST_TWO] = 1;
			} else if (!strcasecmp(argv[i], "video-required-for-canvas")) {
				f[CFLAG_VIDEO_REQUIRED_FOR_CANVAS] = 1;
			} else if (!strcasecmp(argv[i], "video-mute-exit-canvas")) {
				f[CFLAG_VIDEO_MUTE_EXIT_CANVAS] = 1;
			} else if (!strcasecmp(argv[i], "manage-inbound-video-bitrate")) {
				f[CFLAG_MANAGE_INBOUND_VIDEO_BITRATE] = 1;
			} else if (!strcasecmp(argv[i], "video-muxing-personal-canvas")) {
				f[CFLAG_PERSONAL_CANVAS] = 1;
			}
		}

		free(dup);
	}
}


void conference_utils_clear_eflags(char *events, uint32_t *f)
{
	char buf[512] = "";
	char *next = NULL;
	char *event = buf;

	if (events) {
		switch_copy_string(buf, events, sizeof(buf));

		while (event) {
			next = strchr(event, ',');
			if (next) {
				*next++ = '\0';
			}

			if (!strcmp(event, "add-member")) {
				*f &= ~EFLAG_ADD_MEMBER;
			} else if (!strcmp(event, "del-member")) {
				*f &= ~EFLAG_DEL_MEMBER;
			} else if (!strcmp(event, "energy-level")) {
				*f &= ~EFLAG_ENERGY_LEVEL;
			} else if (!strcmp(event, "volume-level")) {
				*f &= ~EFLAG_VOLUME_LEVEL;
			} else if (!strcmp(event, "gain-level")) {
				*f &= ~EFLAG_GAIN_LEVEL;
			} else if (!strcmp(event, "dtmf")) {
				*f &= ~EFLAG_DTMF;
			} else if (!strcmp(event, "stop-talking")) {
				*f &= ~EFLAG_STOP_TALKING;
			} else if (!strcmp(event, "start-talking")) {
				*f &= ~EFLAG_START_TALKING;
			} else if (!strcmp(event, "mute-detect")) {
				*f &= ~EFLAG_MUTE_DETECT;
			} else if (!strcmp(event, "mute-member")) {
				*f &= ~EFLAG_MUTE_MEMBER;
			} else if (!strcmp(event, "unmute-member")) {
				*f &= ~EFLAG_UNMUTE_MEMBER;
			} else if (!strcmp(event, "kick-member")) {
				*f &= ~EFLAG_KICK_MEMBER;
			} else if (!strcmp(event, "dtmf-member")) {
				*f &= ~EFLAG_DTMF_MEMBER;
			} else if (!strcmp(event, "energy-level-member")) {
				*f &= ~EFLAG_ENERGY_LEVEL_MEMBER;
			} else if (!strcmp(event, "volume-in-member")) {
				*f &= ~EFLAG_VOLUME_IN_MEMBER;
			} else if (!strcmp(event, "volume-out-member")) {
				*f &= ~EFLAG_VOLUME_OUT_MEMBER;
			} else if (!strcmp(event, "play-file")) {
				*f &= ~EFLAG_PLAY_FILE;
			} else if (!strcmp(event, "play-file-done")) {
				*f &= ~EFLAG_PLAY_FILE_DONE;
			} else if (!strcmp(event, "play-file-member")) {
				*f &= ~EFLAG_PLAY_FILE_MEMBER;
			} else if (!strcmp(event, "speak-text")) {
				*f &= ~EFLAG_SPEAK_TEXT;
			} else if (!strcmp(event, "speak-text-member")) {
				*f &= ~EFLAG_SPEAK_TEXT_MEMBER;
			} else if (!strcmp(event, "lock")) {
				*f &= ~EFLAG_LOCK;
			} else if (!strcmp(event, "unlock")) {
				*f &= ~EFLAG_UNLOCK;
			} else if (!strcmp(event, "transfer")) {
				*f &= ~EFLAG_TRANSFER;
			} else if (!strcmp(event, "bgdial-result")) {
				*f &= ~EFLAG_BGDIAL_RESULT;
			} else if (!strcmp(event, "floor-change")) {
				*f &= ~EFLAG_FLOOR_CHANGE;
			} else if (!strcmp(event, "record")) {
				*f &= ~EFLAG_RECORD;
			}

			event = next;
		}
	}
}


void conference_utils_merge_mflags(member_flag_t *a, member_flag_t *b)
{
	int x;

	for (x = 0; x < MFLAG_MAX; x++) {
		if (b[x]) a[x] = 1;
	}
}

void conference_utils_set_flag(conference_obj_t *conference, conference_flag_t flag)
{
	conference->flags[flag] = 1;
}
void conference_utils_set_flag_locked(conference_obj_t *conference, conference_flag_t flag)
{
	switch_mutex_lock(conference->flag_mutex);
	conference->flags[flag] = 1;
	switch_mutex_unlock(conference->flag_mutex);
}
void conference_utils_clear_flag(conference_obj_t *conference, conference_flag_t flag)
{
	conference->flags[flag] = 0;
}
void conference_utils_clear_flag_locked(conference_obj_t *conference, conference_flag_t flag)
{
	switch_mutex_lock(conference->flag_mutex);
	conference->flags[flag] = 0;
	switch_mutex_unlock(conference->flag_mutex);
}
switch_bool_t conference_utils_test_flag(conference_obj_t *conference, conference_flag_t flag)
{
	return !!conference->flags[flag];
}

#if 0
void conference_utils_conference_utils_set_mflag(conference_obj_t *conference, member_flag_t mflag)
{
	conference->mflags[mflag] = 1;
}

void conference_utils_clear_mflag(conference_obj_t *conference, member_flag_t mflag)
{
	conference->mflags[mflag] = 0;
}

switch_bool_t conference_utils_test_mflag(conference_obj_t *conference, member_flag_t mflag)
{
	return !!conference->mflags[mflag];
}
#endif

void conference_utils_member_set_flag(conference_member_t *member, member_flag_t flag)
{
	member->flags[flag] = 1;

	if (flag == MFLAG_SECOND_SCREEN) {
		member->flags[MFLAG_CAN_SPEAK] = 0;
		member->flags[MFLAG_CAN_HEAR] = 0;
		member->flags[MFLAG_CAN_BE_SEEN] = 0;
	}
}

void conference_utils_member_set_flag_locked(conference_member_t *member, member_flag_t flag)
{
	switch_mutex_lock(member->flag_mutex);
	conference_utils_member_set_flag(member, flag);
	switch_mutex_unlock(member->flag_mutex);
}

void conference_utils_member_clear_flag(conference_member_t *member, member_flag_t flag)
{
	member->flags[flag] = 0;
}
void conference_utils_member_clear_flag_locked(conference_member_t *member, member_flag_t flag)
{
	switch_mutex_lock(member->flag_mutex);
	member->flags[flag] = 0;
	switch_mutex_unlock(member->flag_mutex);
}
switch_bool_t conference_utils_member_test_flag(conference_member_t *member, member_flag_t flag)
{
	return !!member->flags[flag];
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

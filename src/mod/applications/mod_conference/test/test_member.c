/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2019, Anthony Minessale II <anthm@freeswitch.org>
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
 * Chris Rienzo <chris@signalwire.com>
 * Seven Du <seven@signalwire.com>
 *
 *
 * test_member.c -- tests member functions
 *
 */
#include <switch.h>
#include <stdlib.h>
#include <mod_conference.h>
#include <conference_member.c>

#include <test/switch_test.h>

/* Check whether the conference API lists the member with this uuid. */
static switch_bool_t conference_lists_member(const char *conference_name, const char *member_uuid)
{
	switch_stream_handle_t stream = { 0 };
	char *args = switch_mprintf("%s json_list", conference_name);
	cJSON *conferences = NULL, *members = NULL, *member = NULL;
	switch_bool_t found = SWITCH_FALSE;

	SWITCH_STANDARD_STREAM(stream);
	switch_api_execute("conference", args, NULL, &stream);

	if (stream.data && strncmp((char *)stream.data, "-ERR", 4) && (conferences = cJSON_Parse((char *)stream.data))) {
		if ((members = cJSON_GetObjectItem(cJSON_GetArrayItem(conferences, 0), "members"))) {
			for (member = members->child; member; member = member->next) {
				if (!strcmp(switch_str_nil(cJSON_GetObjectCstr(member, "uuid")), member_uuid)) {
					found = SWITCH_TRUE;
					break;
				}
			}
		}

		cJSON_Delete(conferences);
	}

	switch_safe_free(stream.data);
	switch_safe_free(args);

	return found;
}

/* Wait until the conference lists the member. A member is listed only after its event channel
   permissions are set, so a listed member has its final permissions. */
static switch_bool_t wait_for_conference_member(const char *conference_name, const char *member_uuid, int timeout_ms)
{
	int waited_ms = 0;

	while (!conference_lists_member(conference_name, member_uuid)) {
		if (waited_ms >= timeout_ms) {
			return SWITCH_FALSE;
		}

		switch_yield(10000);
		waited_ms += 10;
	}

	return SWITCH_TRUE;
}

/* Wait until the cookie's permission on the event channel matches the expected value. */
static switch_bool_t wait_for_event_channel_permission(const char *cookie, const char *event_channel, switch_bool_t expected, int timeout_ms)
{
	int waited_ms = 0;

	while (switch_event_channel_permission_verify(cookie, event_channel) != expected) {
		if (waited_ms >= timeout_ms) {
			return SWITCH_FALSE;
		}

		switch_yield(10000);
		waited_ms += 10;
	}

	return SWITCH_TRUE;
}

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_ivr_originate)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(member_test)
		{
			char path[4096];
			char logo[1024];
			conference_member_t smember = { 0 };
			conference_member_t *member = &smember;
			switch_image_t *img;
			int i;

			sprintf(logo, "%s%s%s%s%s%s%s", "{position=left-bot,text_x=center,"
			"center_offset=190,text=#000000:transparent:", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "font/AEH.ttf:50:"
			"'FREESWITCH ROCKS',alt_text_x=center,alt_center_offset=190,"
			"alt_text_y=88,alt_text=#ffffff:transparent:", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "font/AEH.ttf:40:"
			"'freeswitch'}");
			sprintf(path, "%s%s%s%s", logo, SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "../images/signalwire.png");
			switch_mutex_init(&member->write_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->flag_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->fnode_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->audio_in_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->audio_out_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->read_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_thread_rwlock_create(&member->rwlock, fst_pool);

			conference_member_set_logo(member, path);
			img = member->video_logo;

			for(i = 2; i <= 10; i += 2) {
				switch_image_t *scaled_img = NULL;
				char name[1024];

				switch_snprintf(name, sizeof(name), "../images/logo-signalwire-scaled-%d.png", i);
				sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, name);
				switch_img_scale(img, &scaled_img, img->d_w / i, img->d_h / i);
				fst_requires(scaled_img);
				switch_img_write_png(scaled_img, path);
				switch_img_free(&scaled_img);
			}

			switch_img_free(&img);
		}
		FST_TEST_END()

		/* A live array member's event channel cookie gets the live array channel, but not the moderator channel. */
		FST_SESSION_BEGIN(livearray_participant_no_mod_channel)
		{
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);
			const char *conference_name = switch_core_session_get_uuid(fst_session);
			const char *cookie = "livearray-participant-cookie";
			/* the "livearray" test profile sets livearray-sync and the domain the channel names end with */
			const char *la_channel = switch_core_sprintf(fst_pool, "conference-liveArray.%s@livearray.test", conference_name);
			const char *mod_channel = switch_core_sprintf(fst_pool, "conference-mod.%s@livearray.test", conference_name);

			switch_channel_set_variable(channel, "event_channel_cookie", cookie);
			fst_session_park(fst_session);

			switch_core_session_execute_application_async(fst_session, "conference", switch_core_sprintf(fst_pool, "%s@livearray", conference_name));

			if (!wait_for_conference_member(conference_name, switch_core_session_get_uuid(fst_session), 3000)) {
				fst_fail("Expect the participant to join the conference");
				goto livearray_participant_end;
			}

			fst_xcheck(switch_event_channel_permission_verify(cookie, la_channel), "Expect a participant to be granted the live array channel");
			fst_xcheck(!switch_event_channel_permission_verify(cookie, mod_channel), "Expect a participant not to be granted the moderator channel");

		livearray_participant_end:
			switch_event_channel_permission_clear(cookie);
		}
		FST_SESSION_END()

		/* A live array moderator's event channel cookie gets the moderator channel, and loses it on leaving. */
		FST_SESSION_BEGIN(livearray_moderator_mod_channel)
		{
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);
			const char *conference_name = switch_core_session_get_uuid(fst_session);
			const char *cookie = "livearray-moderator-cookie";
			/* the "livearray" test profile sets livearray-sync and the domain the channel name ends with */
			const char *mod_channel = switch_core_sprintf(fst_pool, "conference-mod.%s@livearray.test", conference_name);

			switch_channel_set_variable(channel, "event_channel_cookie", cookie);
			switch_channel_set_variable(channel, "conference_member_flags", "moderator");
			fst_session_park(fst_session);

			switch_core_session_execute_application_async(fst_session, "conference", switch_core_sprintf(fst_pool, "%s@livearray", conference_name));

			if (!wait_for_conference_member(conference_name, switch_core_session_get_uuid(fst_session), 3000)) {
				fst_fail("Expect the moderator to join the conference");
				goto livearray_moderator_end;
			}

			fst_xcheck(switch_event_channel_permission_verify(cookie, mod_channel), "Expect a moderator to be granted the moderator channel");

			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

			fst_xcheck(wait_for_event_channel_permission(cookie, mod_channel, SWITCH_FALSE, 3000),
				"Expect the moderator channel to be revoked when the member leaves");

		livearray_moderator_end:
			switch_event_channel_permission_clear(cookie);
		}
		FST_SESSION_END()

		/* Calls sharing an event channel cookie: a participant call leaving keeps the moderator channel granted to the moderator call. */
		FST_SESSION_BEGIN(livearray_participant_part_keeps_mod_channel)
		{
			switch_channel_t *channel = switch_core_session_get_channel(fst_session);
			const char *conference_name = switch_core_session_get_uuid(fst_session);
			const char *conference_args = switch_core_sprintf(fst_pool, "%s@livearray", conference_name);
			const char *cookie = "livearray-shared-cookie";
			const char *mod_channel = switch_core_sprintf(fst_pool, "conference-mod.%s@livearray.test", conference_name);
			/* part revokes the info channel last, after deciding on the moderator channel */
			const char *info_channel = switch_core_sprintf(fst_pool, "conference-info.%s@livearray.test", conference_name);
			switch_core_session_t *participant_session = NULL;
			switch_channel_t *participant_channel = NULL;
			switch_call_cause_t cause = SWITCH_CAUSE_NONE;

			switch_channel_set_variable(channel, "event_channel_cookie", cookie);
			switch_channel_set_variable(channel, "conference_member_flags", "moderator");
			fst_session_park(fst_session);

			switch_core_session_execute_application_async(fst_session, "conference", conference_args);

			if (!wait_for_conference_member(conference_name, switch_core_session_get_uuid(fst_session), 3000)) {
				fst_fail("Expect the moderator to join the conference");
				goto livearray_shared_cookie_end;
			}

			if (switch_ivr_originate(NULL, &participant_session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL) != SWITCH_STATUS_SUCCESS || !participant_session) {
				fst_fail("Expect the participant session to be created");
				goto livearray_shared_cookie_end;
			}

			participant_channel = switch_core_session_get_channel(participant_session);
			switch_channel_set_state(participant_channel, CS_SOFT_EXECUTE);
			switch_channel_wait_for_state(participant_channel, NULL, CS_SOFT_EXECUTE);
			switch_channel_set_variable(participant_channel, "send_silence_when_idle", "-1");
			switch_channel_set_variable(participant_channel, "event_channel_cookie", cookie);
			fst_session_park(participant_session);

			switch_core_session_execute_application_async(participant_session, "conference", conference_args);

			if (!wait_for_conference_member(conference_name, switch_core_session_get_uuid(participant_session), 3000)) {
				fst_fail("Expect the participant to join the conference");
				goto livearray_shared_cookie_end;
			}

			fst_xcheck(switch_event_channel_permission_verify(cookie, mod_channel), "Expect the moderator call to hold the moderator channel");

			switch_channel_hangup(participant_channel, SWITCH_CAUSE_NORMAL_CLEARING);

			if (!wait_for_event_channel_permission(cookie, info_channel, SWITCH_FALSE, 3000)) {
				fst_fail("Expect the participant leaving to revoke the info channel");
				goto livearray_shared_cookie_end;
			}

			fst_xcheck(switch_event_channel_permission_verify(cookie, mod_channel),
				"Expect the moderator channel to stay granted when a participant on the same cookie leaves");

		livearray_shared_cookie_end:
			if (participant_session) {
				switch_channel_hangup(participant_channel, SWITCH_CAUSE_NORMAL_CLEARING);
				switch_core_session_rwunlock(participant_session);
			}

			switch_event_channel_permission_clear(cookie);
		}
		FST_SESSION_END()

	}
	FST_SUITE_END()
}
FST_CORE_END()

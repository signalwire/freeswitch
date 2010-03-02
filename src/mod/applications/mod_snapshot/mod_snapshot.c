/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 *
 * mod_snapshot.c -- record a sliding window of audio and take snapshots to disk
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_snapshot_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_snapshot_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_snapshot, mod_snapshot_load, NULL, NULL);

struct cap_cb {
	switch_buffer_t *buffer;
	switch_mutex_t *mutex;
	char *base;
};

static switch_bool_t capture_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct cap_cb *cb = (struct cap_cb *) user_data;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
			if (cb->buffer) {
				switch_buffer_destroy(&cb->buffer);
			}
			switch_channel_set_private(channel, "snapshot", NULL);
		}

		break;
	case SWITCH_ABC_TYPE_READ:

		if (cb->buffer) {
			uint8_t data[SWITCH_RECOMMENDED_BUFFER_SIZE];
			switch_frame_t frame = { 0 };

			frame.data = data;
			frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

			if (switch_mutex_trylock(cb->mutex) == SWITCH_STATUS_SUCCESS) {
				while (switch_core_media_bug_read(bug, &frame, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS && !switch_test_flag((&frame), SFF_CNG)) {
					if (frame.datalen)
						switch_buffer_slide_write(cb->buffer, frame.data, frame.datalen);
				}
				switch_mutex_unlock(cb->mutex);
			}

		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

static switch_status_t start_capture(switch_core_session_t *session, unsigned int seconds, switch_media_bug_flag_t flags, const char *base)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_codec_implementation_t read_impl = { 0 };
	struct cap_cb *cb;
	switch_size_t bytes;
	switch_bind_flag_t bind_flags = 0;

	if (switch_channel_get_private(channel, "snapshot")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Already Running.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (seconds < 5) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Must be at least 5 seconds!\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_core_session_get_read_impl(session, &read_impl);

	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	cb = switch_core_session_alloc(session, sizeof(*cb));
	cb->base = switch_core_session_strdup(session, base);

	bytes = read_impl.samples_per_second * seconds * 2;

	switch_buffer_create_dynamic(&cb->buffer, bytes, bytes, bytes);
	switch_mutex_init(&cb->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	if ((status = switch_core_media_bug_add(session, "snapshot", NULL, capture_callback, cb, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	bind_flags = SBF_DIAL_ALEG | SBF_EXEC_ALEG | SBF_EXEC_SAME;
	switch_ivr_bind_dtmf_meta_session(session, 7, bind_flags, "snapshot::snap");

	switch_channel_set_private(channel, "snapshot", bug);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t do_snap(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug = switch_channel_get_private(channel, "snapshot");
	char *file;
	switch_file_handle_t fh = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	switch_size_t bytes_read;
	int16_t pdata[4096] = { 0 };

	if (bug) {
		switch_time_exp_t tm;
		switch_size_t retsize;
		char date[80] = "";
		struct cap_cb *cb = (struct cap_cb *) switch_core_media_bug_get_user_data(bug);

		if (!cb) {
			return SWITCH_STATUS_FALSE;
		}

		switch_time_exp_lt(&tm, switch_time_make(switch_epoch_time_now(NULL), 0));
		switch_strftime(date, &retsize, sizeof(date), "%Y_%m_%d_%H_%M_%S", &tm);

		file = switch_core_session_sprintf(session, "%s%s%s_%s.wav", SWITCH_GLOBAL_dirs.sounds_dir, SWITCH_PATH_SEPARATOR, cb->base, date);

		switch_core_session_get_read_impl(session, &read_impl);
		fh.channels = 0;
		fh.native_rate = read_impl.actual_samples_per_second;

		if (switch_core_file_open(&fh,
								  file,
								  0,
								  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error opening %s\n", file);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);
			return SWITCH_STATUS_FALSE;
		}

		switch_mutex_lock(cb->mutex);
		while ((bytes_read = switch_buffer_read(cb->buffer, pdata, sizeof(pdata)))) {
			switch_size_t samples = bytes_read / 2;

			if (switch_core_file_write(&fh, pdata, &samples) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
		switch_mutex_unlock(cb->mutex);
		switch_core_file_close(&fh);
		switch_core_set_variable("file", file);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Wrote %s\n", file);
	}

	return SWITCH_STATUS_SUCCESS;

}

#define SNAP_SYNTAX "start <sec> <read|write>"
SWITCH_STANDARD_APP(snapshot_app_function)
{
	char *argv[4] = { 0 };
	int argc = 0;
	char *lbuf = NULL;
	switch_media_bug_flag_t flags = SMBF_READ_STREAM | SMBF_WRITE_STREAM | SMBF_READ_PING;

	if (!zstr(data) && (lbuf = switch_core_session_strdup(session, data))) {
		argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc < 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Usage: %s\n", SNAP_SYNTAX);
		return;
	}

	if (!strcasecmp(argv[0], "start")) {
		char *sec = argv[1];
		char *fl = argv[2];
		const char *base = argv[3];
		int seconds = 5;

		if (sec) {
			int tmp = atoi(sec);
			if (tmp > 5) {
				seconds = tmp;
			}
		}

		if (fl) {
			flags = SMBF_READ_PING;
			if (switch_stristr("read", fl)) {
				flags |= SMBF_READ_STREAM;
			}
			if (switch_stristr("write", fl)) {
				flags |= SMBF_WRITE_STREAM;
			}
		}

		if (!base) {
			base = "mod_snapshot";
		}

		start_capture(session, seconds, flags, base);

	}

	if (!strcasecmp(argv[0], "snap")) {
		do_snap(session);
		return;
	}
}


#define SNAP_API_SYNTAX "<uuid> <warning>"
SWITCH_STANDARD_API(snapshot_function)
{
	char *mycmd = NULL, *argv[5] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 1 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", SNAP_API_SYNTAX);
		goto done;
	} else {
		switch_core_session_t *lsession = NULL;

		if ((lsession = switch_core_session_locate(argv[0]))) {
			if (!strcasecmp(argv[1], "snap")) {
				status = do_snap(lsession);
			} else if (!strcasecmp(argv[1], "start")) {
				char *sec = argv[1];
				char *fl = argv[2];
				const char *base = argv[3];
				int seconds = 5;
				switch_media_bug_flag_t flags = SMBF_READ_STREAM | SMBF_WRITE_STREAM | SMBF_READ_PING;

				if (sec) {
					int tmp = atoi(sec);
					if (tmp > 5) {
						seconds = tmp;
					}
				}

				if (fl) {
					flags = SMBF_READ_PING;
					if (switch_stristr("read", fl)) {
						flags |= SMBF_READ_STREAM;
					}
					if (switch_stristr("write", fl)) {
						flags |= SMBF_WRITE_STREAM;
					}
				}

				if (!base) {
					base = "mod_snapshot";
				}

				status = start_capture(lsession, seconds, flags, base);
			}

			switch_core_session_rwunlock(lsession);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

  done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

/* Macro expands to: switch_status_t mod_snapshot_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_snapshot_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Hello World!\n");

	SWITCH_ADD_API(api_interface, "uuid_snapshot", "Snapshot API", snapshot_function, SNAP_API_SYNTAX);
	SWITCH_ADD_APP(app_interface, "snapshot", "", "", snapshot_app_function, SNAP_SYNTAX, SAF_SUPPORT_NOMEDIA);

	/* indicate that the module should continue to be loaded */
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4
 */

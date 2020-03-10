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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * John Wehle <john@feith.com>
 *
 *
 * switch_core_file.c -- Main Core Library (File I/O Functions)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"


static switch_status_t get_file_size(switch_file_handle_t *fh, const char **string)
{
	switch_status_t status;
	switch_file_t *newfile;
	switch_size_t size = 0;

	switch_assert(string);

	status = switch_file_open(&newfile, fh->spool_path ? fh->spool_path : fh->file_path, SWITCH_FOPEN_READ, SWITCH_FPROT_OS_DEFAULT, fh->memory_pool);

	if (status != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	size = switch_file_get_size(newfile);

	if (size) {
		*string = switch_core_sprintf(fh->memory_pool, "%" SWITCH_SIZE_T_FMT, size);
	}

	status = switch_file_close(newfile);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_perform_file_open(const char *file, const char *func, int line,
															  switch_file_handle_t *fh,
															  const char *file_path,
															  uint32_t channels, uint32_t rate, unsigned int flags, switch_memory_pool_t *pool)
{
	char *ext;
	switch_status_t status = SWITCH_STATUS_FALSE;
	char stream_name[128] = "";
	char *rhs = NULL;
	const char *spool_path = NULL;
	int is_stream = 0;
	char *fp = NULL;
	int to = 0;
	int force_channels = 0;
	uint32_t core_channel_limit;

	if (switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Handle already open\n");
		return SWITCH_STATUS_FALSE;
	}

	fh->samples_in = 0;

	if (!(flags & SWITCH_FILE_FLAG_WRITE)) {
		fh->samplerate = 0;
		fh->native_rate = 0;
		fh->channels = 0;
		fh->real_channels = 0;
	}
	
	if (!fh->samplerate) {
		if (!(fh->samplerate = rate)) {
			fh->samplerate = 8000;
		}
	}

	if (zstr(file_path)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Filename\n");
		return SWITCH_STATUS_FALSE;
	}

	fh->flags = flags;

	if (pool) {
		fh->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&fh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			UNPROTECT_INTERFACE(fh->file_interface);
			return status;
		}
		switch_set_flag(fh, SWITCH_FILE_FLAG_FREE_POOL);
	}

	switch_mutex_init(&fh->flag_mutex, SWITCH_MUTEX_NESTED, fh->memory_pool);

	fh->mm.samplerate = 44100;
	fh->mm.channels = 1;
	fh->mm.keyint = 60;
	fh->mm.ab = 128;
	fh->mm.vencspd = SWITCH_VIDEO_ENCODE_SPEED_DEFAULT;
	fh->mm.vprofile = SWITCH_VIDEO_PROFILE_BASELINE;
	fh->mm.try_hardware_encoder = 1;

	if (*file_path == '{') {
		char *timeout;
		char *modname;
		const char *val;
		int tmp;

		fp = switch_core_strdup(fh->memory_pool, file_path);

		while (*fp == '{') {
			char *parsed = NULL;

			if (switch_event_create_brackets(fp, '{', '}', ',', &fh->params, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
				goto fail;
			}

			fp = parsed;
		}

		file_path = fp;

		if ((timeout = switch_event_get_header(fh->params, "timeout"))) {
			if ((to = atoi(timeout)) < 1) {
				to = 0;
			}
		}

		if ((modname = switch_event_get_header(fh->params, "modname"))) {
			fh->modname = switch_core_strdup(fh->memory_pool, modname);
		}

		if ((val = switch_event_get_header(fh->params, "samplerate"))) {
			tmp = atoi(val);
			if (tmp >= 8000) {
				fh->mm.samplerate = tmp;
			}
		}

		if ((val = switch_event_get_header(fh->params, "force_channels"))) {
			tmp = atoi(val);
			if (tmp >= 0 && tmp < 3) {
				force_channels = tmp;
			}
		}

		if ((val = switch_event_get_header(fh->params, "ab"))) {
			tmp = atoi(val);
			if (tmp > 16) {
				fh->mm.ab = tmp;
			}
		}

		if ((val = switch_event_get_header(fh->params, "cbr"))) {
			tmp = switch_true(val);
			fh->mm.cbr = tmp;
		}

		if ((val = switch_event_get_header(fh->params, "vb"))) {
			tmp = atoi(val);

			if (strrchr(val, 'k')) {
				tmp *= 1024;
			} else if (strrchr(val, 'm')) {
				tmp *= 1048576;
			}

			fh->mm.vb = tmp;
		}

		if ((val = switch_event_get_header(fh->params, "vw"))) {
			tmp = atoi(val);
			if (tmp > 0) {
				fh->mm.vw = tmp;
			}
		}

		if ((val = switch_event_get_header(fh->params, "vh"))) {
			tmp = atoi(val);
			if (tmp > 0) {
				fh->mm.vh = tmp;
			}
		}

		if ((val = switch_event_get_header(fh->params, "try_hardware_encoder"))) {
			fh->mm.try_hardware_encoder = switch_true(val);
		}

		if ((val = switch_event_get_header(fh->params, "auth_username"))) {
			fh->mm.auth_username = switch_core_strdup(fh->memory_pool, val);
		}

		if ((val = switch_event_get_header(fh->params, "auth_password"))) {
			fh->mm.auth_password = switch_core_strdup(fh->memory_pool, val);
		}

		if ((val = switch_event_get_header(fh->params, "fps"))) {
			float ftmp = atof(val);
			if (ftmp > 0.0f) {
				fh->mm.fps = ftmp;
			}
		}

		if ((val = switch_event_get_header(fh->params, "vbuf"))) {
			tmp = atoi(val);

			if (strrchr(val, 'k')) {
				tmp *= 1024;
			} else if (strrchr(val, 'm')) {
				tmp *= 1048576;
			}

			if (tmp > 0 && tmp < 104857600 /*100mb*/) {
				fh->mm.vbuf = tmp;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid buffer size: %d\n", tmp);
			}
		}

		if ((val = switch_event_get_header(fh->params, "vencspd"))) {
			if (!strcasecmp(val, "slow")) {
				fh->mm.vencspd = SWITCH_VIDEO_ENCODE_SPEED_SLOW;
			} else if (!strcasecmp(val, "medium")) {
				fh->mm.vencspd = SWITCH_VIDEO_ENCODE_SPEED_MEDIUM;
			} else if (!strcasecmp(val, "fast")) {
				fh->mm.vencspd = SWITCH_VIDEO_ENCODE_SPEED_FAST;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid video encode speed: %s\n", val);
			}
		}

		if ((val = switch_event_get_header(fh->params, "vprofile"))) {
			if (!strcasecmp(val, "baseline")) {
				fh->mm.vprofile = SWITCH_VIDEO_PROFILE_BASELINE;
			} else if (!strcasecmp(val, "main")) {
				fh->mm.vprofile = SWITCH_VIDEO_PROFILE_MAIN;
			} else if (!strcasecmp(val, "high")) {
				fh->mm.vprofile = SWITCH_VIDEO_PROFILE_HIGH;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid video profile: %s\n", val);
			}
		}
	}

	if (switch_directory_exists(file_path, fh->memory_pool) == SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File [%s] is a directory not a file.\n", file_path);
		status = SWITCH_STATUS_GENERR;
		goto fail;
	}

	if ((rhs = strstr(file_path, SWITCH_URL_SEPARATOR))) {
		switch_copy_string(stream_name, file_path, (rhs + 1) - file_path);
		ext = stream_name;
		file_path = rhs + 3;
		fh->stream_name = switch_core_strdup(fh->memory_pool, stream_name);
		fh->file_path = switch_core_strdup(fh->memory_pool, file_path);
		is_stream = 1;
	} else {
		if ((flags & SWITCH_FILE_FLAG_WRITE)) {

			if (fh->params) {
				spool_path = switch_event_get_header(fh->params, "spool_path");
			}

			if (!spool_path) {
				spool_path = switch_core_get_variable_pdup(SWITCH_AUDIO_SPOOL_PATH_VARIABLE, fh->memory_pool);
			}
		}

		if ((ext = strrchr(file_path, '.')) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown file Format [%s]\n", file_path);
			switch_goto_status(SWITCH_STATUS_FALSE, fail);
		}
		ext++;
		fh->file_path = switch_core_strdup(fh->memory_pool, file_path);
	}



	if ((fh->file_interface = switch_loadable_module_get_file_interface(ext, fh->modname)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid file format [%s] for [%s]!\n", ext, file_path);
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}

	fh->file = file;
	fh->func = func;
	fh->line = line;

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_VIDEO) && !fh->file_interface->file_read_video) {
		switch_clear_flag_locked(fh, SWITCH_FILE_FLAG_VIDEO);
	}

	if (spool_path) {
		char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
		switch_uuid_t uuid;
		switch_uuid_get(&uuid);
		switch_uuid_format(uuid_str, &uuid);

		fh->spool_path = switch_core_sprintf(fh->memory_pool, "%s%s%s.%s", spool_path, SWITCH_PATH_SEPARATOR, uuid_str, ext);
	} else {
		fh->spool_path = NULL;
	}

	if (rhs) {
		fh->handler = switch_core_strdup(fh->memory_pool, rhs);
	} else {
		fh->handler = NULL;
	}

	if (force_channels == channels) {
		force_channels = 0;
	}

	if (force_channels && force_channels > 0 && force_channels < 3) {
		fh->real_channels = channels ? channels : fh->channels;
		fh->channels = force_channels;
		fh->mm.channels = fh->channels;
	} else {

		if (channels) {
			fh->channels = channels;
		} else {
			fh->channels = 1;
		}

		fh->mm.channels = fh->channels;
	}

	file_path = fh->spool_path ? fh->spool_path : fh->file_path;

	if ((status = fh->file_interface->file_open(fh, file_path)) != SWITCH_STATUS_SUCCESS) {
		if (fh->spool_path) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Spool dir is set.  Make sure [%s] is also a valid path\n", fh->spool_path);
		}
		UNPROTECT_INTERFACE(fh->file_interface);
		goto fail;
	}

	if (fh->channels > 2) {
		/* just show a warning for more than 2 channels, no matter if we allow them or not */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File [%s] has more than 2 channels: [%u]\n", file_path, fh->channels);
	}

	core_channel_limit = switch_core_max_audio_channels(0);
	if (core_channel_limit && fh->channels > core_channel_limit) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File [%s] has more channels (%u) than limit (%u). Closing.\n", file_path, fh->channels, core_channel_limit);
		fh->file_interface->file_close(fh);
		UNPROTECT_INTERFACE(fh->file_interface);
		switch_goto_status(SWITCH_STATUS_FALSE, fail);
	}

	if (!force_channels && !fh->real_channels) {
		fh->real_channels = fh->channels;

		if (channels) {
			fh->channels = channels;
		}
	}

	if ((flags & SWITCH_FILE_FLAG_WRITE) && !is_stream && (status = switch_file_exists(file_path, fh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File [%s] not created!\n", file_path);
		fh->file_interface->file_close(fh);
		UNPROTECT_INTERFACE(fh->file_interface);
		goto fail;
	}

	if (to) {
		fh->max_samples = (fh->samplerate / 1000) * to;
	}


	if ((flags & SWITCH_FILE_FLAG_READ)) {
		fh->native_rate = fh->samplerate;
	} else {
		fh->native_rate = rate;
	}

	if (fh->samplerate && rate && fh->samplerate != rate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "File %s sample rate %d doesn't match requested rate %d\n", file_path, fh->samplerate, rate);
		if ((flags & SWITCH_FILE_FLAG_READ)) {
			fh->samplerate = rate;
		}
	}

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_VIDEO)) {
		fh->pre_buffer_datalen = 0;
	}

	if (fh->pre_buffer_datalen) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Prebuffering %d bytes\n", (int)fh->pre_buffer_datalen);
		switch_buffer_create_dynamic(&fh->pre_buffer, fh->pre_buffer_datalen * fh->channels, fh->pre_buffer_datalen * fh->channels, 0);
		fh->pre_buffer_data = switch_core_alloc(fh->memory_pool, fh->pre_buffer_datalen * fh->channels);
	}


	if (fh->real_channels != fh->channels && (flags & SWITCH_FILE_FLAG_READ) && !(fh->flags & SWITCH_FILE_NOMUX)) {
		fh->cur_channels = fh->real_channels;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File has %d channels, muxing to %d channel%s will occur.\n", fh->real_channels, fh->channels, fh->channels == 1 ? "" : "s");
	}

	switch_set_flag_locked(fh, SWITCH_FILE_OPEN);
	return status;

  fail:

	switch_clear_flag_locked(fh, SWITCH_FILE_OPEN);

	if (fh->params) {
		switch_event_destroy(&fh->params);
	}

	fh->samples_in = 0;
	fh->max_samples = 0;

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&fh->memory_pool);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_read(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_size_t want, orig_len = *len;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

  top:

	if (fh->max_samples > 0 && fh->samples_in >= (switch_size_t)fh->max_samples) {
		*len = 0;
		return SWITCH_STATUS_FALSE;
	}

	if (fh->buffer && switch_buffer_inuse(fh->buffer) >= *len * 2 * fh->channels) {
		*len = switch_buffer_read(fh->buffer, data, orig_len * 2 * fh->channels) / 2 / fh->channels;
		return *len == 0 ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(fh, SWITCH_FILE_DONE)) {
		switch_clear_flag_locked(fh, SWITCH_FILE_DONE);
		switch_clear_flag_locked(fh, SWITCH_FILE_BUFFER_DONE);
		*len = 0;
		return SWITCH_STATUS_FALSE;
	}

	want = *len;

  more:

	if (fh->pre_buffer) {
		switch_size_t rlen;
		int asis = switch_test_flag(fh, SWITCH_FILE_NATIVE);

		if (!switch_test_flag(fh, SWITCH_FILE_BUFFER_DONE)) {
			rlen = asis ? fh->pre_buffer_datalen : fh->pre_buffer_datalen / 2 / fh->real_channels;

			if (switch_buffer_inuse(fh->pre_buffer) < rlen * 2 * fh->channels) {
				if ((status = fh->file_interface->file_read(fh, fh->pre_buffer_data, &rlen)) == SWITCH_STATUS_BREAK) {
					return SWITCH_STATUS_BREAK;
				}


				if (status != SWITCH_STATUS_SUCCESS || !rlen) {
					switch_set_flag_locked(fh, SWITCH_FILE_BUFFER_DONE);
				} else {
					if (fh->real_channels != fh->channels && !switch_test_flag(fh, SWITCH_FILE_NOMUX)) {
						switch_mux_channels((int16_t *) fh->pre_buffer_data, rlen, fh->real_channels, fh->channels);
					}
					switch_buffer_write(fh->pre_buffer, fh->pre_buffer_data, asis ? rlen : rlen * 2 * fh->channels);
				}
			}
		}

		rlen = switch_buffer_read(fh->pre_buffer, data, asis ? *len : *len * 2 * fh->channels);
		fh->samples_in += rlen;
		*len = asis ? rlen : rlen / 2 / fh->channels;

		if (*len == 0) {
			switch_set_flag_locked(fh, SWITCH_FILE_DONE);
			goto top;
		} else {
			status = SWITCH_STATUS_SUCCESS;
		}

	} else {

		if ((status = fh->file_interface->file_read(fh, data, len)) == SWITCH_STATUS_BREAK) {
			return SWITCH_STATUS_BREAK;
		}

		if (status != SWITCH_STATUS_SUCCESS || !*len) {
			switch_set_flag_locked(fh, SWITCH_FILE_DONE);
			goto top;
		}

		fh->samples_in += *len;

		if (fh->real_channels != fh->channels && !switch_test_flag(fh, SWITCH_FILE_NOMUX)) {
			switch_mux_channels((int16_t *) data, *len, fh->real_channels, fh->channels);
		}
	}

	if (!switch_test_flag(fh, SWITCH_FILE_NATIVE) && fh->native_rate != fh->samplerate) {
		if (!fh->resampler) {
			if (switch_resample_create(&fh->resampler,
									   fh->native_rate, fh->samplerate, (uint32_t) orig_len, SWITCH_RESAMPLE_QUALITY, fh->channels) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to create resampler!\n");
				return SWITCH_STATUS_GENERR;
			}
		}

		switch_resample_process(fh->resampler, data, (uint32_t) *len);

		if (fh->resampler->to_len < want || fh->resampler->to_len > orig_len) {
			if (!fh->buffer) {
				int factor = fh->resampler->to_len * fh->samplerate / 1000;
				switch_buffer_create_dynamic(&fh->buffer, factor, factor, 0);
				switch_assert(fh->buffer);
			}
			if (!fh->dbuf || fh->dbuflen < fh->resampler->to_len * 2 * fh->channels) {
				void *mem;
				fh->dbuflen = fh->resampler->to_len * 2 * fh->channels;
				mem = realloc(fh->dbuf, fh->dbuflen);
				switch_assert(mem);
				fh->dbuf = mem;
			}
			switch_assert(fh->resampler->to_len * 2 * fh->channels <= fh->dbuflen);
			memcpy((int16_t *) fh->dbuf, fh->resampler->to, fh->resampler->to_len * 2 * fh->channels);
			switch_buffer_write(fh->buffer, fh->dbuf, fh->resampler->to_len * 2 * fh->channels);

			if (switch_buffer_inuse(fh->buffer) < want * 2 * fh->channels) {
				*len = want;
				goto more;
			}
			*len = switch_buffer_read(fh->buffer, data, orig_len * 2 * fh->channels) / 2 / fh->channels;
		} else {
			memcpy(data, fh->resampler->to, fh->resampler->to_len * 2 * fh->channels);
			*len = fh->resampler->to_len;
		}


	}

	return status;
}

SWITCH_DECLARE(switch_bool_t) switch_core_file_has_video(switch_file_handle_t *fh, switch_bool_t check_open)
{
	return ((!check_open || switch_test_flag(fh, SWITCH_FILE_OPEN)) && switch_test_flag(fh, SWITCH_FILE_FLAG_VIDEO)) ? SWITCH_TRUE : SWITCH_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_write(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	switch_size_t orig_len = *len;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!fh->file_interface->file_write) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
		return SWITCH_STATUS_SUCCESS;
	}


	if (fh->real_channels != fh->channels && !switch_test_flag(fh, SWITCH_FILE_NOMUX)) {
		int need = *len * 2 * (fh->real_channels > fh->channels ? fh->real_channels : fh->channels);

		if (need > fh->muxlen) {
			fh->muxbuf = realloc(fh->muxbuf, need);
			switch_assert(fh->muxbuf);
			fh->muxlen = need;
		}

		if (fh->muxbuf) {
			memcpy(fh->muxbuf, data, *len * 2);
			data = fh->muxbuf;
		}

		switch_mux_channels((int16_t *) data, *len, fh->real_channels, fh->channels);
	}


	if (!switch_test_flag(fh, SWITCH_FILE_NATIVE) && fh->native_rate != fh->samplerate) {
		if (!fh->resampler) {
			if (switch_resample_create(&fh->resampler,
									   fh->native_rate,
									   fh->samplerate,
									   (uint32_t) orig_len * 2 * fh->channels, SWITCH_RESAMPLE_QUALITY, fh->channels) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to create resampler!\n");
				return SWITCH_STATUS_GENERR;
			}
		}

		switch_resample_process(fh->resampler, data, (uint32_t) * len);

		if (fh->resampler->to_len > orig_len) {
			if (!fh->dbuf || (fh->dbuflen < fh->resampler->to_len * 2 * fh->channels)) {
				void *mem;
				fh->dbuflen = fh->resampler->to_len * 2 * fh->channels;
				mem = realloc(fh->dbuf, fh->dbuflen);
				switch_assert(mem);
				fh->dbuf = mem;
			}
			switch_assert(fh->resampler->to_len * 2 * fh->channels <= fh->dbuflen);
			memcpy(fh->dbuf, fh->resampler->to, fh->resampler->to_len * 2 * fh->channels);
			data = fh->dbuf;
		} else {
			memcpy(data, fh->resampler->to, fh->resampler->to_len * 2 * fh->channels);
		}

		*len = fh->resampler->to_len;
	}

	if (!*len) {
		return SWITCH_STATUS_SUCCESS;
	}


	if (fh->pre_buffer) {
		switch_size_t rlen, blen;
		switch_size_t datalen_adj = fh->pre_buffer_datalen;
		switch_status_t status = SWITCH_STATUS_SUCCESS;
		int asis = switch_test_flag(fh, SWITCH_FILE_NATIVE);

		switch_buffer_write(fh->pre_buffer, data, (asis ? *len : *len * 2) * fh->channels);

		rlen = switch_buffer_inuse(fh->pre_buffer);

		if (fh->pre_buffer_datalen % fh->channels) {
			datalen_adj = fh->pre_buffer_datalen - (fh->pre_buffer_datalen % fh->channels);
		}

		if (rlen >= datalen_adj) {
			if ((blen = switch_buffer_read(fh->pre_buffer, fh->pre_buffer_data, datalen_adj))) {
				if (!asis)
					blen /= 2;
				if (fh->channels > 1)
					blen /= fh->channels;
				if ((status = fh->file_interface->file_write(fh, fh->pre_buffer_data, &blen)) != SWITCH_STATUS_SUCCESS) {
					*len = 0;
				}
			}
		}
		fh->samples_out += orig_len;
		return status;
	} else {
		switch_status_t status;
		if ((status = fh->file_interface->file_write(fh, data, len)) == SWITCH_STATUS_SUCCESS) {
			fh->samples_out += orig_len;
		}
		return status;
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_file_write_video(switch_file_handle_t *fh, switch_frame_t *frame)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_GENERR;
	}

	if (!fh->file_interface->file_write_video) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
		return SWITCH_STATUS_SUCCESS;
	}

	return fh->file_interface->file_write_video(fh, frame);

}

SWITCH_DECLARE(switch_status_t) switch_core_file_read_video(switch_file_handle_t *fh, switch_frame_t *frame, switch_video_read_flag_t flags)
{
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_GENERR;
	}

	if (!fh->file_interface->file_read_video) {
		return SWITCH_STATUS_FALSE;
	}

	status = fh->file_interface->file_read_video(fh, frame, flags);

	if (status == SWITCH_STATUS_FALSE) {
		switch_cond_next();
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_seek(switch_file_handle_t *fh, unsigned int *cur_pos, int64_t samples, int whence)
{
	switch_status_t status;
	int ok = 1;

	switch_assert(fh != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN) || !fh->file_interface->file_seek) {
		ok = 0;
	} else if (switch_test_flag(fh, SWITCH_FILE_FLAG_WRITE)) {
		if (!(switch_test_flag(fh, SWITCH_FILE_WRITE_APPEND) || switch_test_flag(fh, SWITCH_FILE_WRITE_OVER))) {
			ok = 0;
		}
	} else if (!switch_test_flag(fh, SWITCH_FILE_FLAG_READ)) {
		ok = 0;
	}

	if (!ok) {
		return SWITCH_STATUS_FALSE;
	}

	if (fh->buffer) {
		switch_buffer_zero(fh->buffer);
	}

	if (fh->pre_buffer) {
		switch_buffer_zero(fh->pre_buffer);
	}

	if (whence == SWITCH_SEEK_CUR) {
		unsigned int cur = 0;

		if (switch_test_flag(fh, SWITCH_FILE_FLAG_WRITE)) {
			fh->file_interface->file_seek(fh, &cur, fh->samples_out, SEEK_SET);
		} else {
			fh->file_interface->file_seek(fh, &cur, fh->offset_pos, SEEK_SET);
		}
	}

	switch_set_flag_locked(fh, SWITCH_FILE_SEEK);
	status = fh->file_interface->file_seek(fh, cur_pos, samples, whence);

	fh->offset_pos = *cur_pos;

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_WRITE)) {
		fh->samples_out = *cur_pos;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_set_string(switch_file_handle_t *fh, switch_audio_col_t col, const char *string)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!fh->file_interface->file_set_string) {
		return SWITCH_STATUS_FALSE;
	}

	return fh->file_interface->file_set_string(fh, col, string);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_get_string(switch_file_handle_t *fh, switch_audio_col_t col, const char **string)
{
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN) && col < SWITCH_AUDIO_COL_STR_FILE_SIZE) {
		return SWITCH_STATUS_FALSE;
	}

	if (!fh->file_interface->file_get_string) {
		if (col == SWITCH_AUDIO_COL_STR_FILE_SIZE) {
			return get_file_size(fh, string);
		}

		return SWITCH_STATUS_FALSE;
	}

	status = fh->file_interface->file_get_string(fh, col, string);

	if (status == SWITCH_STATUS_SUCCESS && string) return status;

	if (col == SWITCH_AUDIO_COL_STR_FILE_SIZE) {
		return get_file_size(fh, string);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_truncate(switch_file_handle_t *fh, int64_t offset)
{
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!(switch_test_flag(fh, SWITCH_FILE_OPEN) && switch_test_flag(fh, SWITCH_FILE_FLAG_WRITE))) {
		return SWITCH_STATUS_FALSE;
	}

	if (!fh->file_interface->file_truncate) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = fh->file_interface->file_truncate(fh, offset)) == SWITCH_STATUS_SUCCESS) {
		if (fh->buffer) {
			switch_buffer_zero(fh->buffer);
		}
		if (fh->pre_buffer) {
			switch_buffer_zero(fh->pre_buffer);
		}
		fh->samples_out = 0;
		fh->pos = 0;
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_core_file_command(switch_file_handle_t *fh, switch_file_command_t command)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	switch(command) {
	case SCFC_FLUSH_AUDIO:
		if (fh->pre_buffer) {
			switch_buffer_zero(fh->pre_buffer);
		}
		break;
	default:
		break;
	}

	if (fh->file_interface->file_command) {
		switch_mutex_lock(fh->flag_mutex);
		status = fh->file_interface->file_command(fh, command);
		switch_mutex_unlock(fh->flag_mutex);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_pre_close(switch_file_handle_t *fh)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(fh != NULL);

	if (!fh->file_interface) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (fh->pre_buffer) {
		if (switch_test_flag(fh, SWITCH_FILE_FLAG_WRITE)) {
			switch_size_t rlen, blen;
			int asis = switch_test_flag(fh, SWITCH_FILE_NATIVE);

			while ((rlen = switch_buffer_inuse(fh->pre_buffer))) {
				if ((blen = switch_buffer_read(fh->pre_buffer, fh->pre_buffer_data, fh->pre_buffer_datalen))) {
					if (!asis)
						blen /= 2;
					if (fh->channels > 1)
						blen /= fh->channels;

					if (fh->file_interface->file_write(fh, fh->pre_buffer_data, &blen) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}
		}

		switch_buffer_destroy(&fh->pre_buffer);
	}

	switch_clear_flag_locked(fh, SWITCH_FILE_OPEN);
	switch_set_flag_locked(fh, SWITCH_FILE_PRE_CLOSED);

	if (fh->file_interface->file_pre_close) {
		status = fh->file_interface->file_pre_close(fh);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_close(switch_file_handle_t *fh)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		status = switch_core_file_pre_close(fh);
	} else if (!switch_test_flag(fh, SWITCH_FILE_PRE_CLOSED)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_clear_flag_locked(fh, SWITCH_FILE_PRE_CLOSED);

	fh->file_interface->file_close(fh);

	if (fh->params) {
		switch_event_destroy(&fh->params);
	}

	fh->samples_in = 0;
	fh->max_samples = 0;
	
	if (fh->buffer) {
		switch_buffer_destroy(&fh->buffer);
	}

	switch_resample_destroy(&fh->resampler);

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&fh->memory_pool);
	}

	fh->memory_pool = NULL;

	switch_safe_free(fh->dbuf);
	switch_safe_free(fh->muxbuf);

	if (fh->spool_path) {
		char *command;

#ifdef _MSC_VER
		command = switch_mprintf("move %s %s", fh->spool_path, fh->file_path);
#else
		command = switch_mprintf("/bin/mv %s %s", fh->spool_path, fh->file_path);
#endif
		if (system(command) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to copy spooled file [%s] to [%s] because of a command error : %s\n", fh->spool_path, fh->file_path, command);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Copy spooled file [%s] to [%s]\n", fh->spool_path, fh->file_path);
		}
		free(command);
	}

	UNPROTECT_INTERFACE(fh->file_interface);
	fh->file_interface = NULL;

	return status;
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

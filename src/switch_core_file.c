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

SWITCH_DECLARE(switch_status_t) switch_core_perform_file_open(const char *file, const char *func, int line,
															  switch_file_handle_t *fh,
															  const char *file_path,
															  uint8_t channels, uint32_t rate, unsigned int flags, switch_memory_pool_t *pool)
{
	char *ext;
	switch_status_t status;
	char stream_name[128] = "";
	char *rhs = NULL;
	const char *spool_path = NULL;
	int is_stream = 0;

	if (switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Handle already open\n");
		return SWITCH_STATUS_FALSE;
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


	if ((rhs = strstr(file_path, SWITCH_URL_SEPARATOR))) {
		switch_copy_string(stream_name, file_path, (rhs + 1) - file_path);
		ext = stream_name;
		file_path = rhs + 3;
		fh->file_path = switch_core_strdup(fh->memory_pool, file_path);
		is_stream = 1;
	} else {
		if ((flags & SWITCH_FILE_FLAG_WRITE)) {

			char *p, *e;

			fh->file_path = switch_core_strdup(fh->memory_pool, file_path);
			p = fh->file_path;

			if (*p == '[' && *(p + 1) == *SWITCH_PATH_SEPARATOR) {
				e = switch_find_end_paren(p, '[', ']');

				if (e) {
					*e = '\0';
					spool_path = p + 1;
					fh->file_path = e + 1;
				}
			}

			if (!spool_path) {
				spool_path = switch_core_get_variable(SWITCH_AUDIO_SPOOL_PATH_VARIABLE);
			}

			file_path = fh->file_path;
		}

		if ((ext = strrchr(file_path, '.')) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown file Format [%s]\n", file_path);
			switch_goto_status(SWITCH_STATUS_FALSE, fail);
		}
		ext++;
		fh->file_path = switch_core_strdup(fh->memory_pool, file_path);
	}



	if ((fh->file_interface = switch_loadable_module_get_file_interface(ext)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid file format [%s] for [%s]!\n", ext, file_path);
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}

	fh->file = file;
	fh->func = func;
	fh->line = line;


	if (spool_path) {
		char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
		switch_uuid_t uuid;
		switch_uuid_get(&uuid);
		switch_uuid_format(uuid_str, &uuid);

		fh->spool_path = switch_core_sprintf(fh->memory_pool, "%s%s%s.%s", spool_path, SWITCH_PATH_SEPARATOR, uuid_str, ext);
	}

	if (rhs) {
		fh->handler = switch_core_strdup(fh->memory_pool, rhs);
	}

	if (channels) {
		fh->channels = channels;
	} else {
		fh->channels = 1;
	}

	file_path = fh->spool_path ? fh->spool_path : fh->file_path;


	if ((status = fh->file_interface->file_open(fh, file_path)) != SWITCH_STATUS_SUCCESS) {
		if (fh->spool_path) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Spool dir is set.  Make sure [%s] is also a valid path\n", fh->spool_path);
		}
		UNPROTECT_INTERFACE(fh->file_interface);
		switch_goto_status(status, fail);
	}


	if ((flags & SWITCH_FILE_FLAG_WRITE) && !is_stream && (status = switch_file_exists(file_path, fh->memory_pool)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File [%s] not created!\n", file_path);
		fh->file_interface->file_close(fh);
		UNPROTECT_INTERFACE(fh->file_interface);
		switch_goto_status(status, fail);
	}

	if ((flags & SWITCH_FILE_FLAG_READ)) {
		fh->native_rate = fh->samplerate;
	} else {
		fh->native_rate = rate;
	}

	if (fh->samplerate && rate && fh->samplerate != rate) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Sample rate doesn't match\n");
		if ((flags & SWITCH_FILE_FLAG_READ)) {
			fh->samplerate = rate;
		}
	}

	if (fh->pre_buffer_datalen) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Prebuffering %d bytes\n", (int)fh->pre_buffer_datalen);
		switch_buffer_create_dynamic(&fh->pre_buffer, fh->pre_buffer_datalen * fh->channels, fh->pre_buffer_datalen * fh->channels / 2, 0);
		fh->pre_buffer_data = switch_core_alloc(fh->memory_pool, fh->pre_buffer_datalen * fh->channels);
	}

	if (fh->channels > 1 && (flags & SWITCH_FILE_FLAG_READ)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File has %d channels, muxing to mono will occur.\n", fh->channels);
	}

	switch_set_flag(fh, SWITCH_FILE_OPEN);
	return status;

  fail:

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&fh->memory_pool);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_read(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_size_t want, got, orig_len = *len;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

  top:

	if (fh->buffer && switch_buffer_inuse(fh->buffer) >= *len * 2) {
		*len = switch_buffer_read(fh->buffer, data, orig_len * 2) / 2;
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_test_flag(fh, SWITCH_FILE_DONE)) {
		switch_clear_flag(fh, SWITCH_FILE_DONE);
		*len = 0;
		return SWITCH_STATUS_FALSE;
	}

	want = *len;

  more:

	if (fh->pre_buffer) {
		switch_size_t rlen;
		int asis = switch_test_flag(fh, SWITCH_FILE_NATIVE);

		if (!switch_test_flag(fh, SWITCH_FILE_BUFFER_DONE)) {
			rlen = asis ? fh->pre_buffer_datalen : fh->pre_buffer_datalen / 2;

			if (switch_buffer_inuse(fh->pre_buffer) < rlen * 2) {
				if ((status = fh->file_interface->file_read(fh, fh->pre_buffer_data, &rlen)) != SWITCH_STATUS_SUCCESS || !rlen) {
					switch_set_flag(fh, SWITCH_FILE_BUFFER_DONE);
				} else {
					fh->samples_in += rlen;
					if (fh->channels > 1) {
						switch_mux_channels((int16_t *) fh->pre_buffer_data, rlen, fh->channels);
					}
					switch_buffer_write(fh->pre_buffer, fh->pre_buffer_data, asis ? rlen : rlen * 2);
				}
			}
		}

		rlen = switch_buffer_read(fh->pre_buffer, data, asis ? *len : *len * 2);
		*len = asis ? rlen : rlen / 2;

		if (*len == 0) {
			switch_set_flag(fh, SWITCH_FILE_DONE);
			goto top;
		} else {
			status = SWITCH_STATUS_SUCCESS;
		}

	} else {

		if ((status = fh->file_interface->file_read(fh, data, len)) != SWITCH_STATUS_SUCCESS || !*len) {
			switch_set_flag(fh, SWITCH_FILE_DONE);
			goto top;
		}

		fh->samples_in += *len;

		if (fh->channels > 1) {
			switch_mux_channels((int16_t *) data, *len, fh->channels);
		}

	}


	got = *len;

	if (!switch_test_flag(fh, SWITCH_FILE_NATIVE) && fh->native_rate != fh->samplerate) {
		if (!fh->resampler) {
			if (switch_resample_create(&fh->resampler,
									   fh->native_rate, fh->samplerate, (uint32_t) orig_len, SWITCH_RESAMPLE_QUALITY, 1) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to create resampler!\n");
				return SWITCH_STATUS_GENERR;
			}
		}

		switch_resample_process(fh->resampler, data, (uint32_t) * len);

		if (fh->resampler->to_len < want || fh->resampler->to_len > orig_len) {
			if (!fh->buffer) {
				int factor = fh->resampler->to_len * fh->samplerate / 1000;
				switch_buffer_create_dynamic(&fh->buffer, factor, factor, 0);
				switch_assert(fh->buffer);
			}
			if (!fh->dbuf || fh->dbuflen < fh->resampler->to_len * 2) {
				void *mem;
				fh->dbuflen = fh->resampler->to_len * 2;
				mem = realloc(fh->dbuf, fh->dbuflen);
				switch_assert(mem);
				fh->dbuf = mem;
			}
			switch_assert(fh->resampler->to_len * 2 <= fh->dbuflen);
			memcpy((int16_t *) fh->dbuf, fh->resampler->to, fh->resampler->to_len * 2);
			switch_buffer_write(fh->buffer, fh->dbuf, fh->resampler->to_len * 2);

			if (switch_buffer_inuse(fh->buffer) < want * 2) {
				*len = want;
				goto more;
			}
			*len = switch_buffer_read(fh->buffer, data, orig_len * 2) / 2;
		} else {
			memcpy(data, fh->resampler->to, fh->resampler->to_len * 2);
			*len = fh->resampler->to_len;
		}


	}

	return status;
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
			if (!fh->dbuf) {
				void *mem;
				fh->dbuflen = fh->resampler->to_len * 2 * fh->channels;
				mem = realloc(fh->dbuf, fh->dbuflen);
				switch_assert(mem);
				fh->dbuf = mem;
			}
			switch_assert(fh->resampler->to_len * 2 <= fh->dbuflen);
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
		switch_status_t status = SWITCH_STATUS_SUCCESS;
		int asis = switch_test_flag(fh, SWITCH_FILE_NATIVE);

		switch_buffer_write(fh->pre_buffer, data, (asis ? *len : *len * 2) * fh->channels);

		rlen = switch_buffer_inuse(fh->pre_buffer);

		if (rlen >= fh->pre_buffer_datalen) {
			if ((blen = switch_buffer_read(fh->pre_buffer, fh->pre_buffer_data, fh->pre_buffer_datalen))) {
				if (!asis)
					blen /= 2;
				if (fh->channels > 1)
					blen /= fh->channels;
				if ((status = fh->file_interface->file_write(fh, fh->pre_buffer_data, &blen)) != SWITCH_STATUS_SUCCESS) {
					*len = 0;
				}
				fh->samples_out += blen;
			}
		}

		return status;
	} else {
		switch_status_t status;
		if ((status = fh->file_interface->file_write(fh, data, len)) == SWITCH_STATUS_SUCCESS) {
			fh->samples_out += orig_len;
		}
		return status;
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_file_seek(switch_file_handle_t *fh, unsigned int *cur_pos, int64_t samples, int whence)
{
	size_t bytes = 0;
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN) || !switch_test_flag(fh, SWITCH_FILE_FLAG_READ)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!fh->file_interface->file_seek) {
		return SWITCH_STATUS_FALSE;
	}

	if (fh->buffer) {
		bytes += switch_buffer_inuse(fh->buffer);
		switch_buffer_zero(fh->buffer);
	}

	if (fh->pre_buffer) {
		bytes += switch_buffer_inuse(fh->pre_buffer);
		switch_buffer_zero(fh->pre_buffer);
	}

	if (whence == SWITCH_SEEK_CUR) {
		samples -= bytes / sizeof(int16_t);
	}

	switch_set_flag(fh, SWITCH_FILE_SEEK);
	status = fh->file_interface->file_seek(fh, cur_pos, samples, whence);
	if (samples) {
		fh->offset_pos = *cur_pos;
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
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if (!fh->file_interface->file_get_string) {
		return SWITCH_STATUS_FALSE;
	}

	return fh->file_interface->file_get_string(fh, col, string);
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

SWITCH_DECLARE(switch_status_t) switch_core_file_close(switch_file_handle_t *fh)
{
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if (fh->buffer) {
		switch_buffer_destroy(&fh->buffer);
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
					fh->samples_out += blen;
				}
			}
		}

		switch_buffer_destroy(&fh->pre_buffer);
	}

	switch_clear_flag(fh, SWITCH_FILE_OPEN);
	status = fh->file_interface->file_close(fh);

	switch_resample_destroy(&fh->resampler);


	if (fh->spool_path) {
		char *command;
		int result;

#ifdef _MSC_VER
		command = switch_mprintf("move %s %s", fh->spool_path, fh->file_path);
#else
		command = switch_mprintf("/bin/mv %s %s", fh->spool_path, fh->file_path);
#endif
		result = system(command);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Copy spooled file [%s] to [%s]\n", fh->spool_path, fh->file_path);
		free(command);
	}


	UNPROTECT_INTERFACE(fh->file_interface);

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&fh->memory_pool);
	}

	switch_safe_free(fh->dbuf);


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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

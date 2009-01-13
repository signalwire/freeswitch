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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
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

	if (switch_strlen_zero(file_path)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Filename\n");
		return SWITCH_STATUS_FALSE;
	}

	if ((rhs = strstr(file_path, SWITCH_URL_SEPARATOR))) {
		switch_copy_string(stream_name, file_path, (rhs + 1) - file_path);
		ext = stream_name;
		file_path = rhs + 3;
	} else {
		if ((ext = strrchr(file_path, '.')) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown file Format [%s]\n", file_path);
			return SWITCH_STATUS_FALSE;
		}
		ext++;
	}

	if ((fh->file_interface = switch_loadable_module_get_file_interface(ext)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid file format [%s] for [%s]!\n", ext, file_path);
		return SWITCH_STATUS_GENERR;
	}

	fh->file = file;
	fh->func = func;
	fh->line = line;

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

	if (rhs) {
		fh->handler = switch_core_strdup(fh->memory_pool, rhs);
	}

	if (!fh->samplerate) {
		if (!(fh->samplerate = rate)) {
			fh->samplerate = 8000;
		}
	}

	if (channels) {
		fh->channels = channels;
	} else {
		fh->channels = 1;
	}

	if ((status = fh->file_interface->file_open(fh, file_path)) != SWITCH_STATUS_SUCCESS) {
		UNPROTECT_INTERFACE(fh->file_interface);
		return status;
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
		switch_buffer_create_dynamic(&fh->pre_buffer, fh->pre_buffer_datalen, fh->pre_buffer_datalen / 2, 0);
		fh->pre_buffer_data = switch_core_alloc(fh->memory_pool, fh->pre_buffer_datalen);
	}

	switch_set_flag(fh, SWITCH_FILE_OPEN);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_core_file_read(switch_file_handle_t *fh, void *data, switch_size_t *len)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_size_t want, got, orig_len = *len;
	
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

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
	}


	got = *len;
	
	if (!switch_test_flag(fh, SWITCH_FILE_NATIVE) && fh->native_rate != fh->samplerate) {
		if (!fh->resampler) {
			if (switch_resample_create(&fh->resampler,
									   fh->native_rate, orig_len, fh->samplerate, (uint32_t) orig_len, fh->memory_pool) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to create resampler!\n");
				return SWITCH_STATUS_GENERR;
			}
		}

		fh->resampler->from_len = switch_short_to_float(data, fh->resampler->from, (int) *len);
		fh->resampler->to_len =
			switch_resample_process(fh->resampler, fh->resampler->from, fh->resampler->from_len, fh->resampler->to, fh->resampler->to_size, 0);

		if (fh->resampler->to_len < want || fh->resampler->to_len > orig_len) {
			if (!fh->buffer) {
				int factor = fh->resampler->to_len * fh->samplerate / 1000;
				switch_buffer_create_dynamic(&fh->buffer, factor, factor, 0);
				switch_assert(fh->buffer);
			}
			if (!fh->dbuf) {
				fh->dbuflen = want * 8;
				fh->dbuf = switch_core_alloc(fh->memory_pool, fh->dbuflen);
			}
			switch_assert(fh->resampler->to_len <= fh->dbuflen);
			
			switch_float_to_short(fh->resampler->to, (int16_t *) fh->dbuf, fh->resampler->to_len);
			switch_buffer_write(fh->buffer, fh->dbuf, fh->resampler->to_len * 2);

			if (switch_buffer_inuse(fh->buffer) < want * 2) {
				*len = want;
				goto more;
			}
			*len = switch_buffer_read(fh->buffer, data, orig_len * 2) / 2;
		} else {
			switch_float_to_short(fh->resampler->to, data, fh->resampler->to_len);
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

	if (!fh->file_interface->file_write) {
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_test_flag(fh, SWITCH_FILE_NATIVE) && fh->native_rate != fh->samplerate) {
		if (!fh->resampler) {
			if (switch_resample_create(&fh->resampler,
									   fh->native_rate, orig_len, fh->samplerate, (uint32_t) orig_len, fh->memory_pool) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Unable to create resampler!\n");
				return SWITCH_STATUS_GENERR;
			}
		}

		fh->resampler->from_len = switch_short_to_float(data, fh->resampler->from, (int) *len);
		fh->resampler->to_len =
			switch_resample_process(fh->resampler, fh->resampler->from, fh->resampler->from_len, fh->resampler->to, fh->resampler->to_size, 0);
		if (fh->resampler->to_len > orig_len) {
			if (!fh->dbuf) {
				fh->dbuflen = fh->resampler->to_len * 2;
				fh->dbuf = switch_core_alloc(fh->memory_pool, fh->dbuflen);
			}
			switch_assert(fh->resampler->to_len <= fh->dbuflen);
			switch_float_to_short(fh->resampler->to, (int16_t *) fh->dbuf, fh->resampler->to_len);
			data = fh->dbuf;
		} else {
			switch_float_to_short(fh->resampler->to, data, fh->resampler->to_len);
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

		switch_buffer_write(fh->pre_buffer, data, asis ? *len : *len * 2);

		rlen = switch_buffer_inuse(fh->pre_buffer);
		if (rlen >= fh->pre_buffer_datalen) {
			blen = switch_buffer_read(fh->pre_buffer, fh->pre_buffer_data, fh->pre_buffer_datalen);
			if (!asis) blen /= 2;
			if ((status = fh->file_interface->file_write(fh, fh->pre_buffer_data, &blen)) != SWITCH_STATUS_SUCCESS) {
				*len = 0;
			}
		}
		
		return status;
	} else {
		return fh->file_interface->file_write(fh, data, len);
	}
}

SWITCH_DECLARE(switch_status_t) switch_core_file_seek(switch_file_handle_t *fh, unsigned int *cur_pos, int64_t samples, int whence)
{
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!fh->file_interface->file_seek) {
		return SWITCH_STATUS_FALSE;
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

	if (!fh->file_interface->file_set_string) {
		return SWITCH_STATUS_FALSE;
	}

	return fh->file_interface->file_set_string(fh, col, string);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_get_string(switch_file_handle_t *fh, switch_audio_col_t col, const char **string)
{
	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!fh->file_interface->file_get_string) {
		return SWITCH_STATUS_FALSE;
	}

	return fh->file_interface->file_get_string(fh, col, string);
}

SWITCH_DECLARE(switch_status_t) switch_core_file_close(switch_file_handle_t *fh)
{
	switch_status_t status;

	switch_assert(fh != NULL);
	switch_assert(fh->file_interface != NULL);

	if (!switch_test_flag(fh, SWITCH_FILE_OPEN)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_clear_flag(fh, SWITCH_FILE_OPEN);
	status = fh->file_interface->file_close(fh);

	if (fh->buffer) {
		switch_buffer_destroy(&fh->buffer);
	}

	if (fh->pre_buffer) {
		if (switch_test_flag(fh, SWITCH_FILE_FLAG_WRITE)) {
			switch_size_t rlen, blen;
			int asis = switch_test_flag(fh, SWITCH_FILE_NATIVE);

			while((rlen = switch_buffer_inuse(fh->pre_buffer))) {
				blen = switch_buffer_read(fh->pre_buffer, fh->pre_buffer_data, fh->pre_buffer_datalen);
				if (asis) blen /= 2;
				if (fh->file_interface->file_write(fh, fh->pre_buffer_data, &blen) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			}
		}

		switch_buffer_destroy(&fh->pre_buffer);
	}

	switch_resample_destroy(&fh->resampler);

	UNPROTECT_INTERFACE(fh->file_interface);

	if (switch_test_flag(fh, SWITCH_FILE_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&fh->memory_pool);
	}

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

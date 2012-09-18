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
 *
 *
 * mod_sndfile.c -- Framework Demo Module
 *
 */
#include <switch.h>
#include <sndfile.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_sndfile_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sndfile_shutdown);
SWITCH_MODULE_DEFINITION(mod_sndfile, mod_sndfile_load, mod_sndfile_shutdown, NULL);


static struct {
	switch_hash_t *format_hash;
} globals;

struct format_map {
	char *ext;
	char *uext;
	uint32_t format;
};

struct sndfile_context {
	SF_INFO sfinfo;
	SNDFILE *handle;
};

typedef struct sndfile_context sndfile_context;

static switch_status_t sndfile_file_open(switch_file_handle_t *handle, const char *path)
{
	sndfile_context *context;
	int mode = 0;
	char *ext;
	struct format_map *map = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *alt_path = NULL, *last, *ldup = NULL;
	size_t alt_len = 0;
	int rates[4] = { 8000, 16000, 32000, 48000 };
	int i;
	sf_count_t frames = 0;
#ifdef WIN32
	char ps = '/';
#else
	char ps = '/';
#endif

	if ((ext = strrchr(path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}
	ext++;

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		mode += SFM_READ;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND) || switch_test_flag(handle, SWITCH_FILE_WRITE_OVER) || handle->offset_pos) {
			mode += SFM_RDWR;
		} else {
			mode += SFM_WRITE;
		}
	}

	if (!mode) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Mode!\n");
		return SWITCH_STATUS_GENERR;
	}

	if ((context = switch_core_alloc(handle->memory_pool, sizeof(*context))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	map = switch_core_hash_find(globals.format_hash, ext);

	if (mode & SFM_WRITE) {
		context->sfinfo.channels = handle->channels;
		context->sfinfo.samplerate = handle->samplerate;
		if (handle->samplerate == 8000 || handle->samplerate == 16000 ||
			handle->samplerate == 24000 || handle->samplerate == 32000 || handle->samplerate == 48000 ||
			handle->samplerate == 11025 || handle->samplerate == 22050 || handle->samplerate == 44100) {
			context->sfinfo.format |= SF_FORMAT_PCM_16;
		}
	}

	if (map) {
		context->sfinfo.format |= map->format;
	}

	if (!strcmp(ext, "r8") || !strcmp(ext, "raw")) {
		context->sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 8000;
	} else if (!strcmp(ext, "r16")) {
		context->sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 16000;
	} else if (!strcmp(ext, "r24")) {
		context->sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_PCM_24;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 24000;
	} else if (!strcmp(ext, "r32")) {
		context->sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_PCM_32;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 32000;
	} else if (!strcmp(ext, "gsm")) {
		context->sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_GSM610;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 8000;
	} else if (!strcmp(ext, "ul") || !strcmp(ext, "ulaw")) {
		context->sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_ULAW;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 8000;
	} else if (!strcmp(ext, "al") || !strcmp(ext, "alaw")) {
		context->sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_ALAW;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 8000;
	} else if (!strcmp(ext, "adpcm")) {
		context->sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_IMA_ADPCM;
		context->sfinfo.channels = 1;
		context->sfinfo.samplerate = 8000;
	} else if (!strcmp(ext, "oga")) {
		context->sfinfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
		context->sfinfo.samplerate = handle->samplerate;
	}

	if ((mode & SFM_WRITE) && sf_format_check(&context->sfinfo) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error : file format is invalid (0x%08X).\n", context->sfinfo.format);
		return SWITCH_STATUS_GENERR;
	}

	alt_len = strlen(path) + 10;
	switch_zmalloc(alt_path, alt_len);

	switch_copy_string(alt_path, path, alt_len);

	/* This block attempts to add the sample rate to the path
	   if the sample rate is already present in the path it does nothing
	   and reverts to the original file name.
	 */
	if ((last = strrchr(alt_path, ps))) {
		last++;
#ifdef WIN32
		if (strrchr(last, '\\')) {
			last = strrchr(alt_path, '\\');	/* do not swallow a back slash if they are intermixed under windows */
			last++;
		}
#endif
		ldup = strdup(last);
		switch_assert(ldup);
		switch_snprintf(last, alt_len - (last - alt_path), "%d%s%s", handle->samplerate, SWITCH_PATH_SEPARATOR, ldup);
		if ((context->handle = sf_open(alt_path, mode, &context->sfinfo))) {
			path = alt_path;
		} else {
			/* Try to find the file at the highest rate possible if we can't find one that matches the exact rate.
			   If we don't find any, we will default back to the original file name.
			 */
			for (i = 3; i >= 0; i--) {
				switch_snprintf(last, alt_len - (last - alt_path), "%d%s%s", rates[i], SWITCH_PATH_SEPARATOR, ldup);
				if ((context->handle = sf_open(alt_path, mode, &context->sfinfo))) {
					path = alt_path;
					break;
				}
			}
		}
	}

	if (!context->handle) {
		if ((context->handle = sf_open(path, mode, &context->sfinfo)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening File [%s] [%s]\n", path, sf_strerror(context->handle));
			status = SWITCH_STATUS_GENERR;
			goto end;
		}
	}
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Opening File [%s] rate %dhz\n", path, context->sfinfo.samplerate);
	handle->samples = (unsigned int) context->sfinfo.frames;
	handle->samplerate = context->sfinfo.samplerate;
	handle->channels = (uint8_t) context->sfinfo.channels;
	handle->format = context->sfinfo.format;
	handle->sections = context->sfinfo.sections;
	handle->seekable = context->sfinfo.seekable;
	handle->speed = 0;
	handle->private_info = context;

	if (handle->offset_pos) {
		frames = handle->offset_pos;
		handle->offset_pos = 0;
	}

	if (switch_test_flag(handle, SWITCH_FILE_WRITE_APPEND)) {
		handle->pos = sf_seek(context->handle, frames, SEEK_END);
	} else if (switch_test_flag(handle, SWITCH_FILE_WRITE_OVER)) {
		handle->pos = sf_seek(context->handle, frames, SEEK_SET);
	} else {		
		sf_command(context->handle, SFC_FILE_TRUNCATE, &frames, sizeof(frames));
	}


  end:

	switch_safe_free(alt_path);
	switch_safe_free(ldup);

	return status;
}

static switch_status_t sndfile_file_truncate(switch_file_handle_t *handle, int64_t offset)
{
	sndfile_context *context = handle->private_info;
	sf_command(context->handle, SFC_FILE_TRUNCATE, &offset, sizeof(offset));
	handle->pos = 0;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sndfile_file_close(switch_file_handle_t *handle)
{
	sndfile_context *context = handle->private_info;

	sf_close(context->handle);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t sndfile_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	sndfile_context *context = handle->private_info;
	sf_count_t count;
	switch_status_t r = SWITCH_STATUS_SUCCESS;
	
	if (!handle->seekable) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File is not seekable\n");
		return SWITCH_STATUS_NOTIMPL;
	}

	if ((count = sf_seek(context->handle, samples, whence)) == ((sf_count_t) -1)) {
		r = SWITCH_STATUS_BREAK;
		count = sf_seek(context->handle, -1, SEEK_END);
	}
	
	*cur_sample = (unsigned int) count;
	handle->pos = *cur_sample;	
	
	return r;
}

static switch_status_t sndfile_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	size_t inlen = *len;
	sndfile_context *context = handle->private_info;

	if (switch_test_flag(handle, SWITCH_FILE_DATA_RAW)) {
		*len = (size_t) sf_read_raw(context->handle, data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_INT)) {
		*len = (size_t) sf_readf_int(context->handle, (int *) data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_SHORT)) {
		*len = (size_t) sf_readf_short(context->handle, (short *) data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_FLOAT)) {
		*len = (size_t) sf_readf_float(context->handle, (float *) data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_DOUBLE)) {
		*len = (size_t) sf_readf_double(context->handle, (double *) data, inlen);
	} else {
		*len = (size_t) sf_readf_int(context->handle, (int *) data, inlen);
	}

	handle->pos += *len;
	handle->sample_count += *len;

	return *len ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

static switch_status_t sndfile_file_write(switch_file_handle_t *handle, void *data, size_t *len)
{
	size_t inlen = *len;
	sndfile_context *context = handle->private_info;

	if (switch_test_flag(handle, SWITCH_FILE_DATA_RAW)) {
		*len = (size_t) sf_write_raw(context->handle, data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_INT)) {
		*len = (size_t) sf_writef_int(context->handle, (int *) data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_SHORT)) {
		*len = (size_t) sf_writef_short(context->handle, (short *) data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_FLOAT)) {
		*len = (size_t) sf_writef_float(context->handle, (float *) data, inlen);
	} else if (switch_test_flag(handle, SWITCH_FILE_DATA_DOUBLE)) {
		*len = (size_t) sf_writef_double(context->handle, (double *) data, inlen);
	} else {
		*len = (size_t) sf_writef_int(context->handle, (int *) data, inlen);
	}

	handle->sample_count += *len;

	return sf_error(context->handle) == SF_ERR_NO_ERROR ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

static switch_status_t sndfile_file_set_string(switch_file_handle_t *handle, switch_audio_col_t col, const char *string)
{
	sndfile_context *context = handle->private_info;

	return sf_set_string(context->handle, (int) col, string) ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

static switch_status_t sndfile_file_get_string(switch_file_handle_t *handle, switch_audio_col_t col, const char **string)
{
	sndfile_context *context = handle->private_info;
	const char *s;

	if ((s = sf_get_string(context->handle, (int) col))) {
		*string = s;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

/* Registration */

static char **supported_formats;

static switch_status_t setup_formats(void)
{
	SF_FORMAT_INFO info;
	char buffer[128];
	int format, major_count, subtype_count, m, s;
	int len, x, skip;
	char *extras[] = { "r8", "r16", "r24", "r32", "gsm", "ul", "ulaw", "al", "alaw", "adpcm", NULL };
	int exlen = (sizeof(extras) / sizeof(extras[0]));
	buffer[0] = 0;

	sf_command(NULL, SFC_GET_LIB_VERSION, buffer, sizeof(buffer));
	if (strlen(buffer) < 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_ERROR, "Line %d: could not retrieve lib version.\n", __LINE__);
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "\nLibSndFile Version : %s Supported Formats\n", buffer);
	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "================================================================================\n");
	sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof(int));
	sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count, sizeof(int));

	//sfinfo.channels = 1;
	len = ((major_count + (exlen + 2)) * sizeof(char *));
	supported_formats = switch_core_permanent_alloc(len);

	len = 0;
	for (m = 0; m < major_count; m++) {
		skip = 0;
		info.format = m;
		sf_command(NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof(info));
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_INFO, "%s  (extension \"%s\")\n", info.name, info.extension);
		for (x = 0; x < len; x++) {
			if (supported_formats[x] == info.extension) {
				skip++;
				break;
			}
		}
		if (!skip) {
			char *p;
			struct format_map *map = switch_core_permanent_alloc(sizeof(*map));
			switch_assert(map);

			map->ext = switch_core_permanent_strdup(info.extension);
			map->uext = switch_core_permanent_strdup(info.extension);
			map->format = info.format;
			if (map->ext) {
				for (p = map->ext; *p; p++) {
					*p = (char) switch_tolower(*p);
				}
				switch_core_hash_insert(globals.format_hash, map->ext, map);
			}
			if (map->uext) {
				for (p = map->uext; *p; p++) {
					*p = (char) switch_toupper(*p);
				}
				switch_core_hash_insert(globals.format_hash, map->uext, map);
			}
			supported_formats[len++] = (char *) info.extension;
		}
		format = info.format;

		for (s = 0; s < subtype_count; s++) {
			info.format = s;
			sf_command(NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof(info));
			format = (format & SF_FORMAT_TYPEMASK) | info.format;
			//sfinfo.format = format;
			/*
			   if (sf_format_check(&sfinfo)) {
			   switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_DEBUG, "   %s\n", info.name);
			   }
			 */
		}
	}
	for (m = 0; m < exlen; m++) {
		supported_formats[len++] = extras[m];
	}

	switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_NOTICE, "================================================================================\n");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_sndfile_load)
{
	switch_file_interface_t *file_interface;

	switch_core_hash_init(&globals.format_hash, pool);

	if (setup_formats() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	file_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = sndfile_file_open;
	file_interface->file_close = sndfile_file_close;
	file_interface->file_truncate = sndfile_file_truncate;
	file_interface->file_read = sndfile_file_read;
	file_interface->file_write = sndfile_file_write;
	file_interface->file_seek = sndfile_file_seek;
	file_interface->file_set_string = sndfile_file_set_string;
	file_interface->file_get_string = sndfile_file_get_string;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_sndfile_shutdown)
{
	switch_core_hash_destroy(&globals.format_hash);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

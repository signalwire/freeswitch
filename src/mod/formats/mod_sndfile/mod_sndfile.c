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
 * mod_sndfile.c -- Framework Demo Module
 *
 */
#include <switch.h>
#include <sndfile.h>

static const char modname[] = "mod_sndfile";

struct sndfile_context {
	SF_INFO sfinfo;
	SNDFILE *handle;
};

typedef struct sndfile_context sndfile_context;

switch_status sndfile_file_open(switch_file_handle *handle, char *path)
{
	sndfile_context *context;
	int mode = 0;
	char *ext;
	int ready = 1;
	
	if (!(ext = strrchr(path, '.'))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}
	ext++;


	if (switch_test_flag(handle, SWITCH_FILE_FLAG_READ)) {
		mode += SFM_READ;
	}

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		mode += SFM_WRITE;
	}

	if (!mode) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Invalid Mode!\n");
		return SWITCH_STATUS_GENERR;
	}

	if (!(context = switch_core_alloc(handle->memory_pool, sizeof(*context)))) {
		return SWITCH_STATUS_MEMERR;
	}

	if (mode & SFM_WRITE) {
		context->sfinfo.channels = handle->channels;
		context->sfinfo.samplerate = handle->samplerate;
		if (handle->samplerate == 8000 || handle->samplerate == 16000) {
			context->sfinfo.format |= SF_FORMAT_PCM_16;
		} else if (handle->samplerate == 24000) {
			context->sfinfo.format |= SF_FORMAT_PCM_24;
		} else if (handle->samplerate == 32000) {
			context->sfinfo.format |= SF_FORMAT_PCM_32;
		}

		/* Could add more else if() but i am too lazy atm.. */
		if (!strcasecmp(ext, "wav")) {
			context->sfinfo.format |= SF_FORMAT_WAV;
		} else {
			ready = 0;
		}
	} else {
		ready = 0;
	}


	if (!ready) {
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
		}
	}


	if ((mode & SFM_WRITE) && sf_format_check (&context->sfinfo) == 0) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Error : file format is invalid (0x%08X).\n", context->sfinfo.format);
		return SWITCH_STATUS_GENERR;
	};


	if (!(context->handle = sf_open(path, mode, &context->sfinfo))) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Error Opening File [%s] [%s]\n", path,
							  sf_strerror(context->handle));
		return SWITCH_STATUS_GENERR;
	}

	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "Opening File [%s] %dhz\n", path, context->sfinfo.samplerate);
	handle->samples = (unsigned int) context->sfinfo.frames;
	handle->samplerate = context->sfinfo.samplerate;
	handle->channels = context->sfinfo.channels;
	handle->format = context->sfinfo.format;
	handle->sections = context->sfinfo.sections;
	handle->seekable = context->sfinfo.seekable;

	handle->private = context;

	return SWITCH_STATUS_SUCCESS;
}

switch_status sndfile_file_close(switch_file_handle *handle)
{
	sndfile_context *context = handle->private;

	sf_close(context->handle);

	return SWITCH_STATUS_SUCCESS;
}

switch_status sndfile_file_seek(switch_file_handle *handle, unsigned int *cur_sample, unsigned int samples, int whence)
{
	sndfile_context *context = handle->private;

	if (!handle->seekable) {
		return SWITCH_STATUS_NOTIMPL;
	}

	*cur_sample = (unsigned int) sf_seek(context->handle, samples, whence);

	return SWITCH_STATUS_SUCCESS;

}

switch_status sndfile_file_read(switch_file_handle *handle, void *data, size_t *len)
{
	size_t inlen = *len;
	sndfile_context *context = handle->private;

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

	return SWITCH_STATUS_SUCCESS;
}

switch_status sndfile_file_write(switch_file_handle *handle, void *data, size_t *len)
{
	size_t inlen = *len;
	sndfile_context *context = handle->private;

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

	return SWITCH_STATUS_SUCCESS;
}

/* Registration */

static char **supported_formats;

static switch_file_interface sndfile_file_interface = {
	/*.interface_name */ modname,
	/*.file_open */ sndfile_file_open,
	/*.file_close */ sndfile_file_close,
	/*.file_read */ sndfile_file_read,
	/*.file_write */ sndfile_file_write,
	/*.file_seek */ sndfile_file_seek,
	/*.extens */ NULL,
	/*.next */ NULL,
};

static switch_loadable_module_interface sndfile_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ NULL,
	/*.dialplan_interface */ NULL,
	/*.codec_interface */ NULL,
	/*.application_interface */ NULL,
	/*.api_interface */ NULL,
	/*.file_interface */ &sndfile_file_interface
};

static switch_status setup_formats(void)
{
	SF_FORMAT_INFO info;
	SF_INFO sfinfo;
	char buffer[128];
	int format, major_count, subtype_count, m, s;
	int len, x, skip;
	char *extras[] = { "r8", "r16", "r24", "r32", "gsm", NULL };
	int exlen = (sizeof(extras) / sizeof(extras[0]));
	buffer[0] = 0;
	sf_command(NULL, SFC_GET_LIB_VERSION, buffer, sizeof(buffer));
	if (strlen(buffer) < 1) {
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "Line %d: could not retrieve lib version.\n", __LINE__);
		return SWITCH_STATUS_FALSE;
	}


	switch_console_printf(SWITCH_CHANNEL_CONSOLE, "\nLibSndFile Version : %s Supported Formats\n", buffer);
	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN,
						  "================================================================================\n");
	sf_command(NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof(int));
	sf_command(NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count, sizeof(int));

	sfinfo.channels = 1;
	len = ((major_count + (exlen + 2)) * sizeof(char *));
	supported_formats = switch_core_permenant_alloc(len);

	len = 0;
	for (m = 0; m < major_count; m++) {
		skip = 0;
		info.format = m;
		sf_command(NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof(info));
		switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "%s  (extension \"%s\")\n", info.name, info.extension);
		for (x = 0; x < len; x++) {
			if (supported_formats[x] == info.extension) {
				skip++;
				break;
			}
		}
		if (!skip) {
			supported_formats[len++] = (char *) info.extension;
		}
		format = info.format;

		for (s = 0; s < subtype_count; s++) {
			info.format = s;
			sf_command(NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof(info));
			format = (format & SF_FORMAT_TYPEMASK) | info.format;
			sfinfo.format = format;
			if (sf_format_check(&sfinfo)) {
				switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN, "   %s\n", info.name);
			}
		}

	}
	for (m = 0; m < exlen; m++) {
		supported_formats[len++] = extras[m];
	}



	switch_console_printf(SWITCH_CHANNEL_CONSOLE_CLEAN,
						  "================================================================================\n");
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status) switch_module_load(switch_loadable_module_interface **interface, char *filename)
{


	if (setup_formats() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	/* connect my internal structure to the blank pointer passed to me */
	sndfile_file_interface.extens = supported_formats;
	*interface = &sndfile_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

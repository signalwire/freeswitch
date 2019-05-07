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
 *
 * Seven Du <seven@signalwire.com>
 *
 * fs_tts.c -- Use TTS to generate a sound file
 *
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#ifndef WIN32
#ifdef HAVE_SETRLIMIT
#include <sys/resource.h>
#endif
#endif

#include <switch.h>

/* Picky compiler */
#ifdef __ICC
#pragma warning (disable:167)
#endif

static void fs_tts_cleanup()
{
	switch_safe_free(SWITCH_GLOBAL_dirs.conf_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.mod_dir);
	switch_safe_free(SWITCH_GLOBAL_dirs.log_dir);
}

int main(int argc, char *argv[])
{
	int r = 1;
	switch_bool_t verbose = SWITCH_FALSE;
	const char *err = NULL;
	int i;
	char *extra_modules[100] = { 0 };
	int extra_modules_count = 0;
	int cmd_fail = 0;
	const char *tts_engine = "flite";
	const char *tts_voice = "default";
	const char *input = NULL;
	const char *output = NULL;
	const char *text = NULL;
	int channels = 1;
	int rate = 8000;
	switch_file_handle_t fh_input = { 0 }, fh_output = { 0 };
	char buf[2048];
	char txtbuf[2048] = { 0 };
	switch_size_t len = 0;
	switch_memory_pool_t *pool = NULL;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
				case 'c':
					i++;
					if((SWITCH_GLOBAL_dirs.conf_dir = (char *) malloc(strlen(argv[i]) + 1)) == NULL) {
						return 255;
					}
					strcpy(SWITCH_GLOBAL_dirs.conf_dir, argv[i]);
					break;
				case 'k':
					i++;
					if((SWITCH_GLOBAL_dirs.log_dir = (char *) malloc(strlen(argv[i]) + 1)) == NULL) {
						return 255;
					}
					strcpy(SWITCH_GLOBAL_dirs.log_dir, argv[i]);
					break;
				case 'm':
					i++;
					if((SWITCH_GLOBAL_dirs.mod_dir = (char *) malloc(strlen(argv[i]) + 1)) == NULL) {
						return 255;
					}
					strcpy(SWITCH_GLOBAL_dirs.mod_dir, argv[i]);
					break;
				case 'l':
					i++;
					/* Load extra modules */
					if (strchr(argv[i], ','))  {
						extra_modules_count = switch_split(argv[i], ',', extra_modules);
					} else {
						extra_modules_count = 1;
						extra_modules[0] = argv[i];
					}
					break;
				case 'i':
					input = argv[++i];
					break;
				case 'e':
					tts_engine = argv[++i];
					break;
				case 'V':
					tts_voice = argv[++i];
					break;
				case 'r':
					rate = atoi(argv[++i]);
					break;
				case 'v':
					verbose = SWITCH_TRUE;
					break;
				default:
					printf("Command line option not recognized: %s\n", argv[i]);
					cmd_fail = 1;
			}
		} else {
			break;
		}
	}

	if (argc - i < 1 || cmd_fail) {
		goto usage;
	}

	output = argv[i++];

	if (zstr(output)) {
		goto usage;
	}

	if (argc - i > 1) {
		text = argv[i++];
	}

	if (switch_core_init(SCF_MINIMAL, verbose, &err) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot init core [%s]\n", err);
		goto end;
	}

	switch_loadable_module_init(SWITCH_FALSE);
	switch_loadable_module_load_module("", "CORE_PCM_MODULE", SWITCH_TRUE, &err);
	switch_loadable_module_load_module("", "CORE_SPEEX_MODULE", SWITCH_TRUE, &err);
	switch_loadable_module_load_module("", "CORE_SOFTTIMER_MODULE", SWITCH_TRUE, &err);
	switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, "mod_console", SWITCH_TRUE, &err);

	for (i = 0; i < extra_modules_count; i++) {
		if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, extra_modules[i], SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			fprintf(stderr, "Cannot init %s [%s]\n", extra_modules[i], err);
			goto end;
		}
	}

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, "mod_sndfile", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot init mod_sndfile [%s]\n", err);
		goto end;
	}

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, "mod_ssml", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Cannot init mod_ssml [%s]\n", err);
		goto end;
	}

	if (!strcmp(tts_engine, "polly")) {
		if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, "mod_polly", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			fprintf(stderr, "Cannot init mod_polly [%s]\n", err);
			goto end;
		}
	}

	if (!strcmp(tts_engine, "gcloud")) {
		if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, "mod_gcloud", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			fprintf(stderr, "Cannot init mod_polly [%s]\n", err);
			goto end;
		}
	}

	if (!strcmp(tts_voice, "default") && !strcmp(tts_engine, "flite")) {
		tts_voice = "kal";
	}

	switch_core_new_memory_pool(&pool);

	if (zstr(text) || *text == '-') { // read from stdin
		while(read(STDIN_FILENO, txtbuf + len, 1) == 1) {
			if(++len == sizeof(txtbuf) - 1) break;
		}
	} else if (input) {
		int fd = open(input, O_RDONLY);

		if (fd== -1) {
			fprintf(stderr, "Error opening file %s\n", input);
			goto end;
		}

		len = read(fd, txtbuf, sizeof(txtbuf) - 1);
		close(fd);
	}

	if (len > 0) {
		text = txtbuf;
	}

	input = switch_core_sprintf(pool, "tts://%s|%s|%s", tts_engine, tts_voice, text);

	// input = "tts://polly|default|Hello";

	if (verbose) {
		fprintf(stderr, "Speaking %s\n", input);
	}

	if (switch_core_file_open(&fh_input, input, channels, rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Couldn't open %s\n", input);
		goto end;
	}

	if (verbose) {
		fprintf(stderr, "Opening file %s\n", output);
	}

	if (switch_core_file_open(&fh_output, output, channels, rate, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Couldn't open %s\n", output);
		goto end;
	}

	len = sizeof(buf) / 2;

	while (switch_core_file_read(&fh_input, buf, &len) == SWITCH_STATUS_SUCCESS) {
		if (switch_core_file_write(&fh_output, buf, &len) != SWITCH_STATUS_SUCCESS) {
			fprintf(stderr, "Write error\n");
			goto end;
		}

		len = sizeof(buf) / 2;
	}

	r = 0;

end:
	if (switch_test_flag(&fh_input, SWITCH_FILE_OPEN)) {
		switch_core_file_close(&fh_input);
	}

	if (switch_test_flag(&fh_output, SWITCH_FILE_OPEN)) {
		switch_core_file_close(&fh_output);
	}

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}

	fs_tts_cleanup();

	// switch_core_destroy();

	return r;
usage:
	printf("Usage: %s [options] output [text]\n\n", argv[0]);
	printf("The output must end in the format, e.g., myfile.wav myfile.mp3\n");
	printf("\t\t -c path\t\t Path to the FS configurations.\n");
	printf("\t\t -k path\t\t Path to the FS log directory\n");
	printf("\t\t -l module[,module]\t Load additional modules (comma-separated)\n");
	printf("\t\t -m path\t\t Path to the modules.\n");
	printf("\t\t -r rate\t\t sampling rate\n");
	printf("\t\t -v\t\t\t verbose\n");
	printf("\t\t -e\t\t\t TTS engine\n");
	printf("\t\t -V\t\t\t TTS voice\n");
	fs_tts_cleanup();
	return 1;
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

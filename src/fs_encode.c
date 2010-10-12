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
 * Mathieu Rene <mrene@avgs.ca>
 *
 * fs_encode.c -- Encode a native file
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
#include <switch_version.h>


/* Picky compiler */
#ifdef __ICC
#pragma warning (disable:167)
#endif

int main(int argc, char *argv[]) 
{
	int r = 1;
	switch_bool_t verbose = SWITCH_FALSE;
	const char *err = NULL;
	int i;
	char *extra_modules[100] = { 0 };
	int extra_modules_count = 0;
	int cmd_fail = 0;
	const char *fmtp = "";
	int ptime = 20;
	const char *input, *output, *format = NULL;
	int channels = 1;
	int rate = 8000;
	switch_file_handle_t fh_input = { 0 }, fh_output = { 0 };
	switch_codec_t codec = { 0 };
	char buf[1024];
	switch_size_t len = sizeof(buf)/2;
	switch_memory_pool_t *pool;
	
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch(argv[i][1]) {
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
				case 'f':
					fmtp = argv[++i];
					break;
				case 'p':
					ptime = atoi(argv[++i]);
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
	
	if (argc - i < 2 || cmd_fail) {
		goto usage;
	}
	
	input = argv[i++];
	output = argv[i++];
	if (zstr(input) || zstr(output) || !(format = strchr(output, '.'))) {
		goto usage;
	}
	
	format++;
	
	if (switch_core_init(SCF_MINIMAL, verbose, &err) != SWITCH_STATUS_SUCCESS) {
		printf("Cannot init core [%s]\n", err);
		r = 1;
		goto end;
	}
	
	switch_loadable_module_init(SWITCH_FALSE);
	
	for (i = 0; i < extra_modules_count; i++) {
		if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) extra_modules[i], SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
			printf("Cannot init %s [%s]\n", extra_modules[i], err);
			r = 1;
			goto end;
		}
	}
	
	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) "mod_sndfile", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
		printf("Cannot init mod_sndfile [%s]\n", err);
		r = 1;
		goto end;
	}
	
	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) "mod_native_file", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
		printf("Cannot init mod_native_file [%s]\n", err);
		r = 1;
		goto end;
	}
	
	switch_core_new_memory_pool(&pool);
	if (verbose) {
		fprintf(stderr, "Opening file %s\n", input);	
	}
	if (switch_core_file_open(&fh_input, input, channels, rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Couldn't open %s\n", input);
		r = 1;
		goto end;
	}
	

	if (verbose) {
		fprintf(stderr, "Opening file %s\n", output);
	}
	if (switch_core_file_open(&fh_output, output, channels, rate, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_NATIVE, NULL) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Couldn't open %s\n", output);
		r = 1;
		goto end;	
	}

	if (switch_test_flag(&fh_input, SWITCH_FILE_NATIVE)) {
		fprintf(stderr, "Input as native file is not implemented\n");
		goto end;
	}
	
	if (switch_core_codec_init(&codec, format, fmtp, rate, ptime, channels, SWITCH_CODEC_FLAG_ENCODE, NULL, pool) != SWITCH_STATUS_SUCCESS) {
		fprintf(stderr, "Couldn't initialize codec for %s@%dh@%di\n", format, rate, ptime);
		goto end;
	}
	
	while (switch_core_file_read(&fh_input, buf, &len) == SWITCH_STATUS_SUCCESS) {
		char encode_buf[1024];
		uint32_t encoded_len = sizeof(buf);
		uint32_t encoded_rate = rate;
		unsigned int flags = 0;
		
		if (switch_core_codec_encode(&codec, NULL, buf, len*2, rate, encode_buf, &encoded_len, &encoded_rate, &flags) != SWITCH_STATUS_SUCCESS) {
			fprintf(stderr, "Codec encoder error\n");
			goto end;
		}
		
		len = encoded_len;
		if (switch_core_file_write(&fh_output, encode_buf, &len) != SWITCH_STATUS_SUCCESS) {
			fprintf(stderr, "Write error\n");
			goto end;
		}
		
		len = sizeof(buf)/2;
	}
	
	r = 0;

end:
	
	if (fh_input.file_interface) {
		switch_core_file_close(&fh_input);		
	}

	if (fh_output.file_interface) {
		switch_core_file_close(&fh_output);		
	}

	if (pool) {
		switch_core_destroy_memory_pool(&pool);
	}
	switch_core_destroy();
	return r;
usage:
	printf("Usage: %s [options] input output\n\n", argv[0]);
	printf("The output must end in the format, eg: myfile.SPEEX\n");
	printf("\t\t -l module[,module]\t Load additional modules (coma-separated)\n");
	printf("\t\t -f format\t fmtp to pass to the codec\n");
	printf("\t\t -p ptime\t ptime to use while encoding\n");
	printf("\t\t -r rate\t sampling rate\n");
	printf("\t\t -v\t verbose\n");
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

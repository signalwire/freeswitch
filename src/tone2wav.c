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
 * Michael Jerris <mike@jerris.com>
 * Pawel Pierscionek <pawel@voiceworks.pl>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 *
 *
 * tone2wav.c -- Generate a .wav from teletone spec
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


static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	switch_file_handle_t *fh = (switch_file_handle_t *) ts->user_data;
	int wrote;
	switch_size_t len;

	wrote = teletone_mux_tones(ts, map);

	len = wrote;
	if (switch_core_file_write(fh, ts->buffer, &len) != SWITCH_STATUS_SUCCESS) {
		return -1;
	}

	return 0;
}

#define fail() r = 255; goto end

/* the main application entry point */
int main(int argc, char *argv[])
{
	teletone_generation_session_t ts;
	int file_flags = SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT;
	int r = 0;
	int rate = 8000;
	char *file = NULL, *script = NULL;
	switch_file_handle_t fh = { 0 };
	const char *err = NULL;
	switch_bool_t verbose = SWITCH_FALSE;
	int i = 0, c = 1, help = 0;

	for(i = 1; i < argc; i++) {
		if (!strstr(argv[i], "-")) break;
		
		if (!strcmp(argv[i], "-v")) {
			verbose = SWITCH_TRUE;
		}

		if (!strcmp(argv[i], "-s")) {
			c = 2;
		}

		if (!strcmp(argv[i], "-h")) {
			help = 1;
		}

		if (!strncmp(argv[i], "-R", 2)) {
			char *p = argv[i] + 2;

			if (p) {
				int tmp = atoi(p);
				if (tmp > 0) {
					rate = tmp;
				}
			}
		}
	}

	if (argc - i != 2 || help) {
		char *app = NULL;

		if (!help) printf("Invalid input!\n");

		if ((app = strrchr(argv[0], '/'))) {
			app++;
		} else {
			app = argv[0];
		}


		printf("USAGE: %s [options] <file> <tones>\n", app);
		printf("================================================================================\n");
		printf("Options:\n"
			   "-s\t\tStereo\n"
			   "-h\t\tHelp\n"
			   "-R<rate>\tSet Rate (8000,16000,32000,48000) etc.\n"
			   "-v\t\tVerbose Logging\n"
			   "<file>\t\tAny file supported by libsndfile\n"
			   "<tones>\t\tA valid teletone script http://wiki.freeswitch.org/wiki/TGML"
			   "\n\n\n"
			   );
		return 255;
	}
	
	file = argv[i];
	script = argv[i+1];

	if (switch_core_init(SCF_MINIMAL, verbose, &err) != SWITCH_STATUS_SUCCESS) {
		printf("Cannot init core [%s]\n", err);
		fail();
	}

	switch_loadable_module_init(SWITCH_FALSE);

	if (switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) "mod_sndfile", SWITCH_TRUE, &err) != SWITCH_STATUS_SUCCESS) {
		printf("Cannot init mod_sndfile [%s]\n", err);
		fail();
	}
	
	if (switch_core_file_open(&fh, file, c, rate, file_flags, NULL) != SWITCH_STATUS_SUCCESS) {
		printf("Cannot open file %s\n", file);
		fail();
	}
	
	teletone_init_session(&ts, 0, teletone_handler, &fh);
	ts.rate = rate;
	ts.channels = c;
	ts.duration = 250 * (rate / 1000 / c);
	ts.wait = 50 * (rate / 1000 / c);
	if (verbose) {
		ts.debug = 10;
		ts.debug_stream = stdout;
	}
	teletone_run(&ts, script);
	teletone_destroy_session(&ts);
	switch_core_file_close(&fh);
	
	printf("File: %s generated...\n\nPlease support:\nFreeSWITCH http://www.freeswitch.org\nClueCon http://www.cluecon.com\n", file);

 end:

	switch_core_destroy();

	return r;

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

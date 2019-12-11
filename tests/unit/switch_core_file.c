/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2018, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 * Dragos Oancea <dragos@signalwire.com>
 *
 * switch_core_file.c -- tests file core functions
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_core_file)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(test_switch_core_file_close)
		{
			switch_file_handle_t fh = { 0 };
			switch_status_t status = SWITCH_STATUS_FALSE;
			static char filename[] = "/tmp/fs_unit_test.wav";
			static char hdr[] = /*simplest wav file*/
				"\x52\x49\x46\x46"
				"\x24\x00\x00\x00"
				"\x57\x41\x56\x45"
				"\x66\x6d\x74\x20"
				"\x10\x00\x00\x00"
				"\x01\x00\x01\x00"
				"\x44\xac\x00\x00"
				"\x88\x58\x01\x00"
				"\x02\x00\x10\x00"
				"\x64\x61\x74\x61"
				"\x00\x00";
			FILE *f = NULL;

			f = fopen(filename, "w"); 
			fst_check(f != NULL);
			fwrite(hdr, 1, sizeof(hdr) ,f);
			fclose(f);

			status = switch_core_file_open(&fh, filename, 1, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(switch_test_flag(&fh, SWITCH_FILE_OPEN));
			status = switch_core_file_pre_close(&fh);
			fst_check(!switch_test_flag(&fh, SWITCH_FILE_OPEN));
			fst_check(switch_test_flag(&fh, SWITCH_FILE_PRE_CLOSED));
			fst_check(status == SWITCH_STATUS_SUCCESS);
			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(!switch_test_flag(&fh, SWITCH_FILE_PRE_CLOSED));

			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_FALSE);

			unlink(filename);
		}
		FST_TEST_END()
		FST_TEST_BEGIN(test_switch_core_file_no_pre_close)
		{
			switch_file_handle_t fh = { 0 };
			switch_status_t status = SWITCH_STATUS_FALSE;
			static char filename[] = "/tmp/fs_unit_test.wav";
			static char hdr[] = /*simplest wav file*/
				"\x52\x49\x46\x46"
				"\x24\x00\x00\x00"
				"\x57\x41\x56\x45"
				"\x66\x6d\x74\x20"
				"\x10\x00\x00\x00"
				"\x01\x00\x01\x00"
				"\x44\xac\x00\x00"
				"\x88\x58\x01\x00"
				"\x02\x00\x10\x00"
				"\x64\x61\x74\x61"
				"\x00\x00";
			FILE *f = NULL;

			f = fopen(filename, "w"); 
			fst_check(f != NULL);
			fwrite(hdr, 1, sizeof(hdr) ,f);
			fclose(f);

			status = switch_core_file_open(&fh, filename, 1, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(switch_test_flag(&fh, SWITCH_FILE_OPEN));
			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			fst_check(!switch_test_flag(&fh, SWITCH_FILE_PRE_CLOSED));

			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_FALSE);

			unlink(filename);
		}
		FST_TEST_END()

	}
	FST_SUITE_END()
}
FST_CORE_END()

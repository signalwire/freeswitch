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

		FST_TEST_BEGIN(test_switch_core_file_write_mono)
		{
			switch_status_t status = SWITCH_STATUS_FALSE;
			switch_file_handle_t fhw = { 0 };
			static char filename[] = "/tmp/fs_write_unit_test.wav";
			int16_t *buf;
			int nr_frames = 3, i;
			switch_size_t len;

			len = 160;
			switch_malloc(buf, len * sizeof(int16_t));

			status = switch_core_file_open(&fhw, filename, 1, 8000, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			for (i = 0; i < nr_frames; i++) {
				memset(buf, i, len * sizeof(int16_t));
				switch_core_file_write(&fhw, buf, &len);
			}

			status = switch_core_file_close(&fhw);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_safe_free(buf);
			unlink(filename);
		}
		FST_TEST_END()
		FST_TEST_BEGIN(test_switch_core_file_write_mono_to_stereo)
		{
			switch_status_t status = SWITCH_STATUS_FALSE;
			switch_file_handle_t fhw = { 0 };
			static char filename[] = "/tmp/fs_write_unit_test.wav";
			int16_t *buf;
			int nr_frames = 3, i, want_channels = 2;
			switch_size_t len;

			len = 160;
			switch_malloc(buf, len * sizeof(int16_t));

			status = switch_core_file_open(&fhw, filename, want_channels, 8000, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			fhw.real_channels = 1; 

			for (i = 0; i < nr_frames; i++) {
				memset(buf, i, len * sizeof(int16_t));
				status = switch_core_file_write(&fhw, buf, &len);
				fst_requires(status == SWITCH_STATUS_SUCCESS);
			}

			status = switch_core_file_close(&fhw);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_safe_free(buf);
			unlink(filename);

		}
		FST_TEST_END()
		FST_TEST_BEGIN(test_switch_core_file_open_max_audio_channels)
		{
			switch_file_handle_t fh = { 0 };
			switch_status_t status = SWITCH_STATUS_FALSE;
			static char filename[] = "/tmp/fs_unit_test.wav";
			static char hdr[] = /*simplest wav file, hardcoded 8 channels*/
				"\x52\x49\x46\x46"
				"\x24\x00\x00\x00"
				"\x57\x41\x56\x45"
				"\x66\x6d\x74\x20"
				"\x10\x00\x00\x00"
				"\x01\x00\x08\x00"
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

			switch_core_max_audio_channels(6);
			status = switch_core_file_open(&fh, filename, 1, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_FALSE);

			switch_core_max_audio_channels(8);
			status = switch_core_file_open(&fh, filename, 1, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			unlink(filename);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_core_file_write_wav_subtype_ulaw)
		{
			switch_status_t status = SWITCH_STATUS_FALSE;
			switch_file_handle_t fhw = { 0 };
			switch_file_handle_t fhr = { 0 };
			static char path[] = "{subtype=ulaw}/tmp/fs_write_unit_test_ulaw.wav";
			static char filename[] = "/tmp/fs_write_unit_test_ulaw.wav";
			int16_t *buf;
			int16_t *rbuf;
			int i;
			switch_size_t len;
			unsigned char hdr[36] = { 0 };
			FILE *f = NULL;

			len = 160;
			switch_malloc(buf, len * sizeof(int16_t));
			switch_malloc(rbuf, len * sizeof(int16_t));

			/* a ramp spanning most of the 16-bit range */
			for (i = 0; i < (int)len; i++) {
				buf[i] = (int16_t)(-30000 + i * 375);
			}

			status = switch_core_file_open(&fhw, path, 1, 8000, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_file_write(&fhw, buf, &len);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_file_close(&fhw);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* RIFF/WAVE container with a u-law payload: fmt tag == 7, bits per sample == 8 */
			f = fopen(filename, "rb");
			fst_requires(f != NULL);
			fst_check(fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr));
			fclose(f);

			fst_check(!memcmp(hdr, "RIFF", 4));
			fst_check(!memcmp(hdr + 8, "WAVE", 4));
			fst_check(hdr[20] == 0x07 && hdr[21] == 0x00);
			fst_check(hdr[34] == 8 && hdr[35] == 0);

			/* it must read back through the core (the same path playback uses) and
			   every decoded sample must sit within u-law quantization error of the
			   original -- proves real companding, not just a successful open */
			status = switch_core_file_open(&fhr, filename, 1, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			if (status == SWITCH_STATUS_SUCCESS) {
				len = 160;
				status = switch_core_file_read(&fhr, rbuf, &len);
				fst_check(status == SWITCH_STATUS_SUCCESS);
				fst_check(len == 160);

				for (i = 0; i < (int)len; i++) {
					int in = buf[i], out = rbuf[i];
					int tolerance = abs(in) / 16 + 16; /* u-law segment step is < |x|/8 */
					if (abs(out - in) > tolerance) {
						fst_check(!"u-law round-trip sample outside quantization tolerance");
						break;
					}
				}

				status = switch_core_file_close(&fhr);
				fst_check(status == SWITCH_STATUS_SUCCESS);
			}

			switch_safe_free(buf);
			switch_safe_free(rbuf);
			unlink(filename);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_core_file_write_wav_subtype_unknown)
		{
			switch_status_t status = SWITCH_STATUS_FALSE;
			switch_file_handle_t fhw = { 0 };
			static char path[] = "{subtype=bogus}/tmp/fs_write_unit_test_subtype.wav";
			static char filename[] = "/tmp/fs_write_unit_test_subtype.wav";
			int16_t *buf;
			int nr_frames = 3, i;
			switch_size_t len;
			unsigned char hdr[36] = { 0 };
			FILE *f = NULL;

			len = 160;
			switch_malloc(buf, len * sizeof(int16_t));

			/* an unrecognized subtype logs a warning and falls back to PCM_16 */
			status = switch_core_file_open(&fhw, path, 1, 8000, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			for (i = 0; i < nr_frames; i++) {
				memset(buf, i, len * sizeof(int16_t));
				status = switch_core_file_write(&fhw, buf, &len);
				fst_requires(status == SWITCH_STATUS_SUCCESS);
			}

			status = switch_core_file_close(&fhw);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			f = fopen(filename, "rb");
			fst_requires(f != NULL);
			fst_check(fread(hdr, 1, sizeof(hdr), f) == sizeof(hdr));
			fclose(f);

			fst_check(!memcmp(hdr, "RIFF", 4));
			fst_check(!memcmp(hdr + 8, "WAVE", 4));
			fst_check(hdr[20] == 0x01 && hdr[21] == 0x00);
			fst_check(hdr[34] == 16 && hdr[35] == 0);

			switch_safe_free(buf);
			unlink(filename);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(test_switch_core_file_write_subtype_container_mismatch)
		{
			switch_status_t status = SWITCH_STATUS_FALSE;
			switch_file_handle_t fhw = { 0 };
			static char path[] = "{subtype=ulaw}/tmp/fs_write_unit_test_mismatch.flac";
			static char filename[] = "/tmp/fs_write_unit_test_mismatch.flac";
			int16_t *buf;
			int nr_frames = 3, i;
			switch_size_t len;
			unsigned char magic[4] = { 0 };
			FILE *f = NULL;

			len = 160;
			switch_malloc(buf, len * sizeof(int16_t));

			/* u-law is not a valid subtype for a FLAC container: the open must
			   still succeed (warn + fall back to the container's default subtype),
			   not fail with GENERR as it would without the fallback */
			status = switch_core_file_open(&fhw, path, 1, 8000, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			for (i = 0; i < nr_frames; i++) {
				memset(buf, i, len * sizeof(int16_t));
				status = switch_core_file_write(&fhw, buf, &len);
				fst_requires(status == SWITCH_STATUS_SUCCESS);
			}

			status = switch_core_file_close(&fhw);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* the file that lands is a real FLAC (fLaC magic): the container was
			   preserved and only the unsupported subtype was dropped */
			f = fopen(filename, "rb");
			fst_requires(f != NULL);
			fst_check(fread(magic, 1, sizeof(magic), f) == sizeof(magic));
			fclose(f);
			fst_check(!memcmp(magic, "fLaC", 4));

			switch_safe_free(buf);
			unlink(filename);
		}
		FST_TEST_END()

	}
	FST_SUITE_END()
}
FST_CORE_END()

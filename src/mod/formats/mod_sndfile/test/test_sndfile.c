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
 * Dragos Oancea <dragos@signalwire.com>
 *
 * test_sndfile.c -- tests mod_sndfile
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

/* Media files used:
 *
 * 1. hi.wav
 *
 * General
 * Complete name                            : test/sounds/hi.wav
 * Format                                   : Wave
 * File size                                : 67.2 KiB
 * Duration                                 : 2 s 150 ms
 * Overall bit rate mode                    : Constant
 * Overall bit rate                         : 256 kb/s
 *
 * Audio
 * Format                                   : PCM
 * Format settings, Endianness              : Little
 * Format settings, Sign                    : Signed
 * Codec ID                                 : 1
 * Duration                                 : 2 s 150 ms
 * Bit rate mode                            : Constant
 * Bit rate                                 : 256 kb/s
 * Channel(s)                               : 1 channel
 * Sampling rate                            : 16.0 kHz
 * Bit depth                                : 16 bits
 * Stream size                              : 67.2 KiB (100%)
 *
 *
 * 2. hello_stereo.wav 
 *
 *
 * General
 * Complete name                            : sounds/hello_stereo.wav
 * Format                                   : Wave
 * File size                                : 220 KiB
 * Duration                                 : 1 s 277 ms
 * Overall bit rate mode                    : Constant
 * Overall bit rate                         : 1 412 kb/s
 *
 * Audio
 * Format                                   : PCM
 * Format settings, Endianness              : Little
 * Format settings, Sign                    : Signed
 * Codec ID                                 : 1
 * Duration                                 : 1 s 277 ms
 * Bit rate mode                            : Constant
 * Bit rate                                 : 1 411.2 kb/s
 * Channel(s)                               : 2 channels
 * Sampling rate                            : 44.1 kHz
 * Bit depth                                : 16 bits
 * Stream size                              : 220 KiB (100%)
 *

*/

char *extensions[] = { 
	"aiff", "au", "avr", "caf", 
	"flac", "htk", "iff", "mat", 
	"mpc", "paf", "pvf", "rf64", 
	"sd2", "sds", "sf", "voc", 
	"w64", "wav", "wve", "xi",
	"raw", "r8", "r16", "r24", 
	"r32", "ul", "ulaw", "al", 
	"alaw", "gsm", "vox", "oga", "ogg"};

FST_CORE_BEGIN("test_formats_and_muxing")
{
	FST_SUITE_BEGIN(test_sndfile)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
			fst_requires_module("mod_sndfile");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(sndfile_write_read_mono)
		{
			/* play mono, record mono, open mono */
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			static char play_filename[] = "../sounds/hi.wav";
			char path[4096];
			switch_file_handle_t fh = { 0 };
			int16_t *audiobuf;
			switch_size_t len, rd;
			char *recording;
			int i, exlen, timeout_sec = 2, duration = 3000; /*ms*/
			switch_stream_handle_t stream = { 0 };

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, play_filename);

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("sndfile_debug", "on", session, &stream);

			switch_safe_free(stream.data);

			exlen = (sizeof(extensions) / sizeof(extensions[0]));

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(session);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			for (i = 0; i < exlen; i++) {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Testing media file extension: [%s]\n", extensions[i]);

				recording = switch_mprintf("/tmp/%s.%s", switch_core_session_get_uuid(session), extensions[i]);
				status = switch_ivr_record_session(session, recording, duration, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				status = switch_ivr_play_file(session, NULL, path, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_sleep(1000 * duration); // wait for audio to finish playing

				switch_ivr_stop_record_session(session, "all");

				status = switch_core_file_open(&fh, recording, 1, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				rd = 320; // samples
				len = rd * sizeof(*audiobuf);
				switch_zmalloc(audiobuf, len);

				status = switch_core_file_read(&fh, audiobuf, &rd);
				fst_requires(status == SWITCH_STATUS_SUCCESS);
				fst_check(rd = 320); // check that we read the wanted number of samples

				status = switch_core_file_close(&fh);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				switch_safe_free(audiobuf);

				unlink(recording);

				switch_safe_free(recording);

				switch_sleep(1000000);
			}

			channel = switch_core_session_get_channel(session);
			fst_requires(channel);

			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			fst_check(!switch_channel_ready(channel));

			switch_core_session_rwunlock(session);
		}

		FST_TEST_END()

		FST_TEST_BEGIN(sndfile_write_read_m2s)
		{
			/* play mono file, record mono, open stereo */
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			static char play_filename[] = "../sounds/hi.wav";
			char path[4096];
			switch_file_handle_t fh = { 0 };
			int16_t *audiobuf;
			switch_size_t len, rd;
			char *recording;
			int i, exlen, channels = 2, timeout_sec = 2, duration = 3000; /*ms*/
			switch_stream_handle_t stream = { 0 };
		
			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, play_filename);

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("sndfile_debug", "on", session, &stream);

			switch_safe_free(stream.data);

			exlen = (sizeof(extensions) / sizeof(extensions[0]));

			for (i = 0; i < exlen; i++) {

				status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
				fst_requires(session);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Testing media file extension: [%s]\n", extensions[i]);

				recording = switch_mprintf("/tmp/%s.%s", switch_core_session_get_uuid(session), extensions[i]);

				status = switch_ivr_record_session(session, recording, duration, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				status = switch_ivr_play_file(session, NULL, path, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_sleep(1000 * duration); // wait for audio to finish playing

				switch_ivr_stop_record_session(session, "all");

				channel = switch_core_session_get_channel(session);
				fst_requires(channel);

				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				fst_check(!switch_channel_ready(channel));

				switch_core_session_rwunlock(session);

				status = switch_core_file_open(&fh, recording, channels, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				rd = 320; // samples
				len = rd * sizeof(*audiobuf) * channels;
				switch_zmalloc(audiobuf, len);

				status = switch_core_file_read(&fh, audiobuf, &rd);
				fst_requires(status == SWITCH_STATUS_SUCCESS);
				fst_check(rd = 320); // check that we read the wanted number of samples

				status = switch_core_file_close(&fh);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				switch_safe_free(audiobuf);

				unlink(recording);

				switch_safe_free(recording);

				switch_sleep(1000000);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(sndfile_write_read_s2m)
		{
			/* play stereo wav, record stereo, open stereo file */
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			static char play_filename[] = "../sounds/hello_stereo.wav";
			char path[4096];
			switch_file_handle_t fh = { 0 };
			int16_t *audiobuf;
			switch_size_t len, rd;
			char *recording, *rec_path;
			int i, exlen, channels = 2, timeout_sec = 2, duration = 3000; /*ms*/
			switch_stream_handle_t stream = { 0 };
			
			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, play_filename);

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("sndfile_debug", "on", session, &stream);

			switch_safe_free(stream.data);

			exlen = (sizeof(extensions) / sizeof(extensions[0]));

			for (i = 0; i < exlen; i++) {

				status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);

				fst_requires(session);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Testing media file extension: [%s]\n", extensions[i]);

				rec_path = switch_mprintf("/tmp/%s.%s", switch_core_session_get_uuid(session), extensions[i]);
				recording = switch_mprintf("{force_channels=2}%s", rec_path);

				channel = switch_core_session_get_channel(session);
				fst_requires(channel);

				switch_channel_set_variable(channel, "enable_file_write_buffering", "true");

				status = switch_ivr_record_session(session, recording, duration, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				status = switch_ivr_play_file(session, NULL, path, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_sleep(1000 * duration); // wait for audio to finish playing

				switch_ivr_stop_record_session(session, "all");

				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				fst_check(!switch_channel_ready(channel));

				switch_core_session_rwunlock(session);

				status = switch_core_file_open(&fh, recording, channels, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				rd = 320; // samples
				len = rd * sizeof(*audiobuf) * channels;
				switch_zmalloc(audiobuf, len);

				status = switch_core_file_read(&fh, audiobuf, &rd);
				fst_requires(status == SWITCH_STATUS_SUCCESS);
				fst_check(rd = 320); // check that we read the wanted number of samples

				status = switch_core_file_close(&fh);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				switch_safe_free(audiobuf);

				unlink(rec_path);

				switch_safe_free(rec_path);
				switch_safe_free(recording);

				switch_sleep(1000000);
			}
		}

		FST_TEST_END()

		FST_TEST_BEGIN(sndfile_write_read_stereo)
		{
			/* play stereo wav, record stereo, open stereo file */
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			static char play_filename[] = "../sounds/hello_stereo.wav";
			char path[4096];
			switch_file_handle_t fh = { 0 };
			int16_t *audiobuf;
			switch_size_t len, rd;
			char *recording, *rec_path;
			int i, exlen, channels = 2, timeout_sec = 2, duration = 3000; /*ms*/
			switch_stream_handle_t stream = { 0 };
			
			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, play_filename);

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("sndfile_debug", "on", session, &stream);

			switch_safe_free(stream.data);

			exlen = (sizeof(extensions) / sizeof(extensions[0]));

			for (i = 0; i < exlen; i++) {

				status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", timeout_sec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
				fst_requires(session);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Testing media file extension: [%s]\n", extensions[i]);

				rec_path = switch_mprintf("/tmp/%s.%s", switch_core_session_get_uuid(session), extensions[i]);
				recording = switch_mprintf("{force_channels=2}%s", rec_path);

				channel = switch_core_session_get_channel(session);
				fst_requires(channel);

				switch_channel_set_variable(channel, "RECORD_STEREO", "true");
				switch_channel_set_variable(channel, "enable_file_write_buffering", "true");

				status = switch_ivr_record_session(session, recording, duration, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				status = switch_ivr_play_file(session, NULL, path, NULL);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				switch_sleep(1000 * duration); // wait for audio to finish playing

				switch_ivr_stop_record_session(session, "all");

				switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
				fst_check(!switch_channel_ready(channel));

				switch_core_session_rwunlock(session);

				status = switch_core_file_open(&fh, recording, channels, 8000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				rd = 320; // samples
				len = rd * sizeof(*audiobuf) * channels;
				switch_zmalloc(audiobuf, len);

				status = switch_core_file_read(&fh, audiobuf, &rd);
				fst_requires(status == SWITCH_STATUS_SUCCESS);
				fst_check(rd = 320); // check that we read the wanted number of samples

				status = switch_core_file_close(&fh);
				fst_requires(status == SWITCH_STATUS_SUCCESS);

				switch_safe_free(audiobuf);

				unlink(rec_path);

				switch_safe_free(rec_path);
				switch_safe_free(recording);

				switch_sleep(1000000);
			}
		}
		FST_TEST_END()

		FST_TEST_BEGIN(unload_mod_sndfile)
		{
			const char *err = NULL;
			switch_sleep(1000000);
			fst_check(switch_loadable_module_unload_module((char *)"../.libs", (char *)"mod_sndfile", SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

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
 *
 * test_opusfile.c -- tests mod_opusfile
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>
#include <libteletone_detect.h>
#include <libteletone.h>

//#undef HAVE_OPUSFILE_ENCODE

#define OGG_MIN_PAGE_SIZE 2400

static switch_status_t test_detect_tone_in_file(const char *filepath, int rate, int freq) {
	teletone_multi_tone_t mt;
	teletone_tone_map_t map;
	int16_t data[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
	size_t len = rate * 2 / 100; // in samples
	switch_status_t status;
	switch_file_handle_t fh = { 0 };

	status = switch_core_file_open(&fh, filepath, 1, rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open file [%s]\n", filepath);
		return SWITCH_STATUS_FALSE;
	} 

	mt.sample_rate = rate;
	map.freqs[0] = (teletone_process_t)freq;

	teletone_multi_tone_init(&mt, &map);

	while (switch_core_file_read(&fh, &data, &len) == SWITCH_STATUS_SUCCESS) {
		if (teletone_multi_tone_detect(&mt, data, len)) {
			switch_core_file_close(&fh);
			return SWITCH_STATUS_SUCCESS;
		}
	}

	switch_core_file_close(&fh);
	return SWITCH_STATUS_FALSE;
}

FST_CORE_BEGIN(".")
{
	FST_SUITE_BEGIN(test_opusfile)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_loopback");
			fst_requires_module("mod_opusfile");
			fst_requires_module("mod_sndfile");
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(opusfile_read)
		{
			switch_core_session_t *session = NULL;
			switch_channel_t *channel = NULL;
			switch_status_t status;
			switch_call_cause_t cause;
			/*
			$ mediainfo hi.opus
			General
			Complete name                            : hi.opus
			Format                                   : OGG
			File size                                : 8.55 KiB
			Duration                                 : 2s 157ms
			Overall bit rate                         : 32.5 Kbps
			Writing application                      : opusenc from opus-tools 0.1.10

			Audio
			ID                                       : 277454932 (0x1089A054)
			Format                                   : Opus
			Duration                                 : 2s 157ms
			Channel(s)                               : 1 channel
			Channel positions                        : Front: C
			Sampling rate                            : 16.0 KHz
			Compression mode                         : Lossy
			Writing library                          : libopus 1.2~alpha2
			*/
			static char filename[] = "sounds/hi.opus"; // duration in samples: 103200
			char path[4096];
			switch_file_handle_t fh = { 0 };
			int16_t *audiobuf;
			switch_size_t len; 

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, filename);

			status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
			fst_requires(session);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_file_open(&fh, path, 1, 48000, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			len = 128 * 1024 * sizeof(*audiobuf);
			switch_zmalloc(audiobuf, len);

			status = switch_core_file_read(&fh, audiobuf, &len);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			/* [INFO] mod_opusfile.c:292 [OGG/OPUS File] Duration (samples): 103200 */
			/* compare the read sample count with the one in the OGG/OPUS header. */
			fst_check(len == 103200);

			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_safe_free(audiobuf);
			channel = switch_core_session_get_channel(session);
			fst_requires(channel);

			switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
			fst_check(!switch_channel_ready(channel));

			switch_core_session_rwunlock(session);

			switch_sleep(1000000);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(opusfile_stream) 
		{
			switch_codec_t read_codec = { 0 };
			switch_status_t status;
			switch_codec_settings_t codec_settings = {{ 0 }};
			unsigned char buf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			switch_file_handle_t fhw = { 0 };
			uint32_t flags = 0;
			uint32_t rate;
			static char tmp_filename[] = "/tmp/opusfile-stream-unit_test.wav";
			char path[4096];
			uint32_t filerate = 48000;
			uint32_t torate = 8000;
			/*decode*/
			uint32_t decoded_len;
			size_t write_len;
			unsigned char decbuf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			switch_stream_handle_t stream = { 0 };
			switch_timer_t timer;
#ifdef  HAVE_OPUSFILE_ENCODE
			switch_file_handle_t fh = { 0 };
			unsigned char encbuf[SWITCH_RECOMMENDED_BUFFER_SIZE] = { 0 };
			switch_codec_t write_codec = { 0 };
			switch_size_t len = 960;
			uint32_t encoded_len;
			unsigned int pages = 0;
			/*
			General
			Complete name                            : sounds/audiocheck.net_sin_1000Hz_-3dBFS_6s.wav
			Format                                   : Wave
			File size                                : 563 KiB
			Duration                                 : 6 s 0 ms
			Overall bit rate mode                    : Constant
			Overall bit rate                         : 768 kb/s

			Audio
			Format                                   : PCM
			Format settings, Endianness              : Little
			Format settings, Sign                    : Signed
			Codec ID                                 : 1
			Duration                                 : 6 s 0 ms
			Bit rate mode                            : Constant
			Bit rate                                 : 768 kb/s
			Channel(s)                               : 1 channel
			Sampling rate                            : 48.0 kHz
			Bit depth                                : 16 bits
			Stream size                              : 563 KiB (100%)
			*/
			static char filename[] = "sounds/audiocheck.net_sin_1000Hz_-3dBFS_6s.wav";
#else 
			static char opus_filename[] = "sounds/opusfile-test-ogg.bitstream";
			switch_file_t *fd;
			switch_size_t flen;
#endif

			status = switch_core_codec_init(&read_codec,
			"OPUSSTREAM",
			"mod_opusfile",
			NULL,
			filerate,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			SWITCH_STANDARD_STREAM(stream);

			switch_api_execute("opusfile_debug", "on", NULL, &stream);

			switch_safe_free(stream.data);

			switch_core_timer_init(&timer, "soft", 20, 960, fst_pool);

#ifdef HAVE_OPUSFILE_ENCODE
			status = switch_core_codec_init(&write_codec,
			"OPUSSTREAM",
			"mod_opusfile",
			NULL,
			filerate,
			20,
			1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
			&codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, filename);

			status = switch_core_file_open(&fh, path, 1, filerate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_file_open(&fhw, tmp_filename, 1, torate, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			fhw.native_rate = filerate; // make sure we resample to 8000 Hz, because teletone wants this rate

			while (switch_core_file_read(&fh, &buf, &len) == SWITCH_STATUS_SUCCESS) {

				status = switch_core_codec_encode(&write_codec, NULL, &buf, len * sizeof(int16_t), filerate, &encbuf, &encoded_len, &rate, &flags);
				fst_check(status == SWITCH_STATUS_SUCCESS);

				if (encoded_len) {
					pages++;
					status = switch_core_codec_decode(&read_codec, NULL, &encbuf, encoded_len, filerate, &decbuf, &decoded_len, &rate, &flags);
					switch_core_timer_next(&timer);
					fst_check(status == SWITCH_STATUS_SUCCESS);
					write_len = decoded_len / sizeof(int16_t);
					if (write_len) switch_core_file_write(&fhw, &decbuf, &write_len);
				}
			}

			// continue reading, encoded pages are buffered
			while (switch_core_codec_decode(&read_codec, NULL, &encbuf, 0, filerate, &decbuf, &decoded_len, &rate, &flags) == SWITCH_STATUS_SUCCESS && decoded_len) {
				switch_core_timer_next(&timer);
				write_len = decoded_len / sizeof(int16_t);
				status = switch_core_file_write(&fhw, &decbuf, &write_len);
				fst_check(status == SWITCH_STATUS_SUCCESS);
			}

			switch_core_codec_destroy(&write_codec);

			status = switch_core_file_close(&fh);
			fst_check(status == SWITCH_STATUS_SUCCESS);

#else	
			// the test will perform only decoding

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, opus_filename);

			// open the file raw and buffer data to the decoder
			status = switch_file_open(&fd, path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, fst_pool);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			status = switch_core_file_open(&fhw, tmp_filename, 1, torate, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			fhw.native_rate = filerate;

			flen = 4096;
			while (switch_file_read(fd, &buf, &flen) == SWITCH_STATUS_SUCCESS || flen != 0) {
				status = SWITCH_STATUS_SUCCESS;
				while (status == SWITCH_STATUS_SUCCESS) {
					switch_core_timer_next(&timer);
					status = switch_core_codec_decode(&read_codec, NULL, &buf, flen, filerate, &decbuf, &decoded_len, &rate, &flags);
					fst_check(status == SWITCH_STATUS_SUCCESS);
					write_len = decoded_len / sizeof(int16_t);
					if (write_len) switch_core_file_write(&fhw, &decbuf, &write_len);
					else break;
				}
			}
#endif 
			// continue reading, encoded pages are buffered
			while (switch_core_codec_decode(&read_codec, NULL, &buf, 0, filerate, &decbuf, &decoded_len, &rate, &flags) == SWITCH_STATUS_SUCCESS && decoded_len) {
				switch_core_timer_next(&timer);
				write_len = decoded_len / sizeof(int16_t);
				status = switch_core_file_write(&fhw, &decbuf, &write_len);
				fst_check(status == SWITCH_STATUS_SUCCESS);
			}
			
			status = switch_core_file_close(&fhw);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_core_codec_destroy(&read_codec);

			// final test
			status = test_detect_tone_in_file(tmp_filename, torate, 1000 /*Hz*/);
			fst_requires(status == SWITCH_STATUS_SUCCESS);

			switch_core_timer_destroy(&timer);

			unlink(tmp_filename);
		}

		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

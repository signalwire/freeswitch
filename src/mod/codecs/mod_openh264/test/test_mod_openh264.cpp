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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Pax Cao <yiping.cao@msn.com>
 *
 * mod_openh264_test -- mod_openh264 tests
 *
 */

#include <test/switch_test.h>

#define 	TEST_SIMU_RTP_MAX_LEN 		4200

/* Add our command line options. */
static fctcl_init_t my_cl_options[] = {
	{"--disable-hw",                 /* long_opt */
	 NULL,                           /* short_opt (optional) */
	 FCTCL_STORE_TRUE  ,             /* action */
	 "disable hardware encoder"     /* help */
	 },

	FCTCL_INIT_NULL /* Sentinel */
};

FST_CORE_BEGIN("conf")
{
	fctcl_install(my_cl_options);

	FST_MODULE_BEGIN(mod_openh264, mod_openh264_test)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_openh264");
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(decoder_testcase)
		{
			switch_status_t status;
			switch_codec_t codec = { 0 };
			switch_codec_settings_t codec_settings = {{ 0 }};
			switch_image_t *img;
			uint8_t buf[TEST_SIMU_RTP_MAX_LEN + 12] = {0};
			switch_frame_t frame = { 0 };
			int packets = 0;
			switch_status_t decode_status;
			FILE *fp = NULL;
			long file_size = 0;
			switch_bool_t		fh_status = SWITCH_FALSE;
			int   file_count = 1;
			switch_set_string(codec_settings.video.config_profile_name, "test-decoder-1");

			if (!fctcl_is("--disable-hw")) {
				codec_settings.video.try_hardware_encoder = 1;
			}

			status = switch_core_codec_init(&codec,
							   "H264",
							   NULL,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			
			frame.packet = buf;
			frame.packetlen = TEST_SIMU_RTP_MAX_LEN + 12;
			frame.data = buf + 12;
			frame.datalen = TEST_SIMU_RTP_MAX_LEN;
			frame.payload = 96;
			frame.m = SWITCH_FALSE;
			frame.seq = 0;
			frame.timestamp = 375233;
			frame.img = (switch_image_t*)NULL;
			do {
				switch_size_t len = TEST_SIMU_RTP_MAX_LEN;
				char  file_name[128] = { 0 };
				frame.seq++;
				switch_snprintf(file_name, 128, "./data/case1.packet%d.264", file_count);

				fp = fopen(file_name, "rb");
				if (!fp) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						 "read video file %s failed \n", file_name);
					break;
				}
				fseek(fp, 0, SEEK_END);
				file_size = ftell(fp);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "file %s length: %ld \n", file_name, file_size);
				fseek(fp, 0, SEEK_SET);	 
				
				frame.datalen = fread(frame.data, sizeof(char), file_size, fp);
				fst_check(frame.datalen == file_size);

				if (4 == file_count ) {
					frame.m = SWITCH_TRUE;
				} 
				if (5 == file_count) {
					frame.m = SWITCH_TRUE;
					frame.timestamp = 380633;
				}
				decode_status = switch_core_codec_decode_video(&codec, &frame);
				fst_check(decode_status == SWITCH_STATUS_SUCCESS || decode_status == SWITCH_STATUS_MORE_DATA);
				
				if (frame.img != NULL) {
					// write down the decoded
					status = switch_img_write_png(frame.img, (char *)"case1.output.qcif.png");
					fst_check(status == SWITCH_STATUS_SUCCESS);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "write output file done! \n");
					
				}
				fclose(fp);
				file_count++;
			} while (decode_status == SWITCH_STATUS_MORE_DATA && file_count < 6);
			
			switch_core_codec_destroy(&codec);

			file_count = 1;
			status = switch_core_codec_init(&codec,
							   "H264",
							   NULL,
							   NULL,
							   0,
							   0,
							   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
							   &codec_settings, fst_pool);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			
			frame.packet = buf;
			frame.packetlen = TEST_SIMU_RTP_MAX_LEN + 12;
			frame.data = buf + 12;
			frame.datalen = TEST_SIMU_RTP_MAX_LEN;
			frame.payload = 96;
			frame.m = SWITCH_FALSE;
			frame.seq = 0;
			frame.timestamp = 375233;
			frame.img = (switch_image_t*)NULL;
			do {
				switch_size_t len = TEST_SIMU_RTP_MAX_LEN;
				char  file_name[128] = { 0 };
				frame.seq++;
				switch_snprintf(file_name, 128, "./data/case2.packet%d.264", file_count);

				fp = fopen(file_name, "rb");
				if (!fp) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
						 "read video file %s failed \n", file_name);
					break;
				}
				fseek(fp, 0, SEEK_END);
				file_size = ftell(fp);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "file %s length: %ld \n", file_name, file_size);
				fseek(fp, 0, SEEK_SET);	 
				
				frame.datalen = fread(frame.data, sizeof(char), file_size, fp);
				fst_check(frame.datalen == file_size);

				if (3 == file_count) {
					frame.m = SWITCH_TRUE;
				} 
				if (4 == file_count) {
					frame.m = SWITCH_TRUE;
					frame.timestamp = 380633;
				} 
				decode_status = switch_core_codec_decode_video(&codec, &frame);
				fst_check(decode_status == SWITCH_STATUS_SUCCESS || decode_status == SWITCH_STATUS_MORE_DATA);
				if (frame.img != NULL) {
					// write down the decoded
					status = switch_img_write_png(frame.img, (char *)"case2.output.qcif.png");
					fst_check(status == SWITCH_STATUS_SUCCESS);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
						 "write output file done! \n");
					
				}
				fclose(fp);
				file_count++;
			} while (decode_status == SWITCH_STATUS_MORE_DATA && file_count < 5);
			
			switch_core_codec_destroy(&codec);

		}
		FST_TEST_END()


		FST_TEARDOWN_BEGIN()
		{
			const char *err = NULL;
			switch_sleep(1000000);
			fst_check(switch_loadable_module_unload_module(SWITCH_GLOBAL_dirs.mod_dir, (char *)"mod_openh264", SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS);
		}
		FST_TEARDOWN_END()
	}
	FST_MODULE_END()
}
FST_CORE_END()

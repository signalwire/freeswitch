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
 *
 *
 * mod_av_test -- avcodec tests
 *
 */

#include <test/switch_test.h>

int loop = 0;

/* Add our command line options. */
static fctcl_init_t my_cl_options[] = {
	{"--disable-hw",                 /* long_opt */
	 NULL,                           /* short_opt (optional) */
	 FCTCL_STORE_TRUE  ,             /* action */
	 "disable hardware encoder"     /* help */
	 },

	{"--loop",                       /* long_opt */
	 NULL,                           /* short_opt (optional) */
	 FCTCL_STORE_VALUE ,             /* action */
	 "loops to encode a picture"     /* help */
	 },
	FCTCL_INIT_NULL /* Sentinel */
};

FST_CORE_BEGIN("conf")
{
        const char *loop_;
	fctcl_install(my_cl_options);

	loop_ = fctcl_val("--loop");
	if (loop_) loop = atoi(loop_);

	FST_MODULE_BEGIN(mod_av, mod_av_test)
	{
		FST_SETUP_BEGIN()
		{
			// fst_requires_module("mod_av");
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(encoder_test)
		{
			switch_status_t status;
			switch_codec_t codec = { 0 };
			switch_codec_settings_t codec_settings = { 0 };
			switch_image_t *img;
			uint8_t buf[SWITCH_DEFAULT_VIDEO_SIZE + 12];
			switch_frame_t frame = { 0 };
			int packets = 0;
			switch_status_t encode_status;

			// switch_set_string(codec_settings.video.config_profile_name, "conference");

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

			img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 1280, 720, 1);
			fst_requires(img);


			frame.packet = buf;
			frame.packetlen = SWITCH_DEFAULT_VIDEO_SIZE + 12;
			frame.data = buf + 12;
			frame.datalen = SWITCH_DEFAULT_VIDEO_SIZE;
			frame.payload = 96;
			frame.m = 0;
			frame.seq = 0;
			frame.timestamp = 0;
			frame.img = img;

			do {
				frame.datalen = SWITCH_DEFAULT_VIDEO_SIZE;
				encode_status = switch_core_codec_encode_video(&codec, &frame);

				if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {

					fst_requires((encode_status == SWITCH_STATUS_SUCCESS && frame.m) || !frame.m);

					if (frame.flags & SFF_PICTURE_RESET) {
						frame.flags &= ~SFF_PICTURE_RESET;
						fst_check(0);
					}

					if (frame.datalen == 0) break;

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[%d]: %02x %02x | m=%d | %d\n", loop, buf[12], buf[13], frame.m, frame.datalen);
					packets++;
				}

			} while(encode_status == SWITCH_STATUS_MORE_DATA || loop-- > 1);

			fst_check(frame.m == 1);
			fst_check(packets > 0);

			switch_core_codec_destroy(&codec);
		}
		FST_TEST_END()

		FST_TEARDOWN_BEGIN()
		{
			const char *err = NULL;
			switch_sleep(1000000);
			fst_check(switch_loadable_module_unload_module(SWITCH_GLOBAL_dirs.mod_dir, (char *)"mod_av", SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS);
		}
		FST_TEARDOWN_END()
	}
	FST_MODULE_END()
}
FST_CORE_END()

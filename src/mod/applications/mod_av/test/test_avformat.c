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
 * Seven Du <seven@signalwire.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * test_avformat -- avformat tests
 *
 */

#include <test/switch_test.h>

#define SAMPLES 160

FST_CORE_BEGIN("conf")
{
	FST_MODULE_BEGIN(mod_av, mod_av_test)
	{
		FST_SETUP_BEGIN()
		{
			fst_requires_module("mod_av");
		}
		FST_SETUP_END()

		FST_TEST_BEGIN(avformat_test_colorspace_RGB)
		{
			switch_status_t status;
			switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 1280, 720, 1);
			switch_file_handle_t fh = { 0 };
			uint8_t data[SAMPLES * 2] = { 0 };
			switch_frame_t frame = { 0 };
			switch_size_t len = SAMPLES;
			uint32_t flags = SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT | SWITCH_FILE_FLAG_VIDEO;
			int i = 0;

			fst_requires(img);

			status = switch_core_file_open(&fh, "{colorspace=0}./test_RGB.mp4", 1, 8000, flags, fst_pool);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(switch_test_flag(&fh, SWITCH_FILE_OPEN));

			status = switch_core_file_write(&fh, data, &len);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "status: %d len: %d\n", status, (int)len);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			// fst_requires(len == SAMPLES);

			frame.img = img;
			status = switch_core_file_write_video(&fh, &frame);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_image_t *ccimg = switch_img_read_png("./cluecon.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(ccimg);

			switch_rgb_color_t color = {0};
			color.a = 255;

			for (i = 0; i < 30; i++) {
				len = SAMPLES;

				if (i == 10) {
					color.r = 255;
				} else if (i == 20) {
					color.r = 0;
					color.b = 255;
				}

				switch_img_fill(img, 0, 0, img->d_w, img->d_h, &color);
				switch_img_patch(img, ccimg, i * 10, i * 10);

				status = switch_core_file_write(&fh, data, &len);
				status = switch_core_file_write_video(&fh, &frame);
				switch_yield(100000);
			}

			switch_core_file_close(&fh);
			switch_img_free(&img);
			switch_img_free(&ccimg);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(avformat_test_colorspace_BT7)
		{
			switch_status_t status;
			switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 1280, 720, 1);
			switch_file_handle_t fh = { 0 };
			uint8_t data[SAMPLES * 2] = { 0 };
			switch_frame_t frame = { 0 };
			switch_size_t len = SAMPLES;
			uint32_t flags = SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT | SWITCH_FILE_FLAG_VIDEO;
			int i = 0;

			fst_requires(img);

			status = switch_core_file_open(&fh, "{colorspace=1}./test_BT7.mp4", 1, 8000, flags, fst_pool);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			fst_requires(switch_test_flag(&fh, SWITCH_FILE_OPEN));

			status = switch_core_file_write(&fh, data, &len);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "status: %d len: %d\n", status, (int)len);
			fst_check(status == SWITCH_STATUS_SUCCESS);
			// fst_requires(len == SAMPLES);

			frame.img = img;
			status = switch_core_file_write_video(&fh, &frame);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_image_t *ccimg = switch_img_read_png("./cluecon.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(ccimg);

			switch_rgb_color_t color = {0};
			color.a = 255;

			for (i = 0; i < 30; i++) {
				len = SAMPLES;

				if (i == 10) {
					color.r = 255;
				} else if (i == 20) {
					color.r = 0;
					color.b = 255;
				}

				switch_img_fill(img, 0, 0, img->d_w, img->d_h, &color);
				switch_img_patch(img, ccimg, i * 10, i * 10);

				status = switch_core_file_write(&fh, data, &len);
				status = switch_core_file_write_video(&fh, &frame);
				switch_yield(100000);
			}

			switch_core_file_close(&fh);
			switch_img_free(&img);
			switch_img_free(&ccimg);
		}
		FST_TEST_END()

		FST_TEARDOWN_BEGIN()
		{
		  //const char *err = NULL;
		  switch_sleep(1000000);
		  //fst_check(switch_loadable_module_unload_module(SWITCH_GLOBAL_dirs.mod_dir, (char *)"mod_av", SWITCH_TRUE, &err) == SWITCH_STATUS_SUCCESS);
		}
		FST_TEARDOWN_END()
	}
	FST_MODULE_END()
}
FST_CORE_END()

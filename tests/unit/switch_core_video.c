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
 * Seven Du <seven@signalwire.com>
 *
 *
 * switch_core_video.c -- tests core_video
 *
 */
#include <switch.h>
#include <stdlib.h>

#include <test/switch_test.h>

FST_CORE_BEGIN("./conf")
{
	FST_SUITE_BEGIN(switch_ivr_originate)
	{
		FST_SETUP_BEGIN()
		{
		}
		FST_SETUP_END()

		FST_TEARDOWN_BEGIN()
		{
		}
		FST_TEARDOWN_END()

		FST_TEST_BEGIN(data_url_test)
		{
			char *data_url = NULL;
			switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 120, 60, 1);
			switch_image_t *argb_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_ARGB, 120, 60, 1);
			switch_rgb_color_t color = { 0 };
			color.r = 255;
			// color.g = 255;
			// color.b = 255;

			switch_img_fill(img, 0, 0, img->d_w, img->d_h, &color);
			switch_img_add_text(img->planes[0], img->d_w, 10, 10, "-1234567890");
			switch_img_write_png(img, "images/test-rgb.png");

			switch_img_data_url_png(img, &data_url);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "I420: %s\n", data_url);
			free(data_url);
			data_url = NULL;

			switch_img_copy(img, &argb_img);

			{
				uint8_t *p = argb_img->planes[0];
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%d %d %d %d\n", *p, *(p+1), *(p+2), *(p+3));
			}

			switch_img_write_png(argb_img, "images/test-argb.png");
			switch_img_data_url_png(argb_img, &data_url);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ARGB: %s\n", data_url);
			free(data_url);


			switch_img_free(&img);
			switch_img_free(&argb_img);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(img_patch)
		{
			int width = 320;
			int height = 240;

			switch_image_t *timg = switch_img_write_text_img(width, height, SWITCH_FALSE, "#ffffff:transparent:FreeMono.ttf:24:This is a test!");
			fst_requires(timg != NULL);
			switch_status_t status = switch_img_write_png(timg, "images/test_text.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			width *=2;
			height *=2;

			switch_rgb_color_t bgcolor = {0, 0, 0}; // red
			bgcolor.b = 255;

			switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
			fst_requires(img);
			switch_img_fill(img, 0, 0, width, height, &bgcolor);
			status = switch_img_write_png(img, "images/test.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_patch(img, timg, 0, 0);

			status = switch_img_write_png(img, "images/test_patched.png");

			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_free(&img);
			switch_img_free(&timg);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(img_patch_alpha)
		{
			switch_image_t *timg = switch_img_read_png("images/test_text.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(timg != NULL);

			switch_image_t *img = switch_img_read_png("images/banner.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(img);
			switch_img_patch(img, timg, 0, 0);
			switch_status_t status = switch_img_write_png(img, "images/test_patched_alpha.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_free(&img);
			img = switch_img_read_png("images/banner.png", SWITCH_IMG_FMT_ARGB);
			switch_img_patch_rgb(img, timg, 0, 0, SWITCH_TRUE);
			status = switch_img_write_png(img, "images/test_patched_noalpha.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_free(&img);
			switch_img_free(&timg);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(img_patch_banner_alpha)
		{
			switch_status_t status;
			switch_image_t *img = switch_img_read_png("images/banner.png", SWITCH_IMG_FMT_ARGB);
			switch_image_t *img2 = switch_img_read_png("images/banner.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(img);
			fst_requires(img2);
			switch_img_patch(img, img2, 80, 20);
			status = switch_img_write_png(img, "images/test_patched_banner_alpha.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_free(&img);
			img = switch_img_read_png("images/banner.png", SWITCH_IMG_FMT_ARGB);
			switch_img_patch_rgb(img, img2, 80, 20, SWITCH_TRUE);
			status = switch_img_write_png(img, "images/test_patched_banner_noalpha.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_free(&img);
			switch_img_free(&img2);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(img_patch_signalwire_alpha)
		{
			switch_image_t *timg_small = NULL;
			switch_image_t *timg = switch_img_read_png("images/signalwire.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(timg != NULL);

			switch_image_t *img = switch_img_read_png("images/banner.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(img);
			switch_img_scale(timg, &timg_small, timg->d_w / 5, timg->d_h / 5);
			switch_img_patch(img, timg_small, 80, 20);
			switch_status_t status = switch_img_write_png(img, "images/test_patched_signalwire_alpha.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_free(&img);
			switch_img_free(&timg);
			switch_img_free(&timg_small);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(img_patch_signalwire_no_alpha)
		{
			switch_image_t *timg_small = NULL;
			switch_image_t *timg = switch_img_read_png("images/signalwire.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(timg != NULL);

			switch_image_t *img = switch_img_read_png("images/banner.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(img);
			switch_img_scale(timg, &timg_small, timg->d_w / 5, timg->d_h / 5);
			switch_img_patch_rgb(img, timg_small, 80, 20, SWITCH_TRUE);
			switch_status_t status = switch_img_write_png(img, "images/test_patched_signalwire_no_alpha.png");
			fst_check(status == SWITCH_STATUS_SUCCESS);

			switch_img_free(&img);
			switch_img_free(&timg);
			switch_img_free(&timg_small);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(stb_data_url)
		{
			switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, 120, 60, 1);
			switch_rgb_color_t color = { 0 };
			color.r = 255;
			// color.g = 255;
			// color.b = 255;
			char *data_url = NULL;

			switch_img_fill(img, 0, 0, img->d_w, img->d_h, &color);
			switch_img_add_text(img->planes[0], img->d_w, 10, 10, "-1234567890");

			switch_img_data_url(img, &data_url, "png", 0);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "PNG: %s\n", data_url);
			free(data_url);
			data_url = NULL;

			switch_img_data_url(img, &data_url, "jpeg", 50);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "JPG: %s\n", data_url);

			free(data_url);
			switch_img_free(&img);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(read_from_file)
		{
			switch_image_t *img;

			img = switch_img_read_from_file("../../images/cluecon.png", SWITCH_IMG_FMT_I420);
			fst_requires(img);
			switch_img_write_png(img, "cluecon-rgb.png");
			switch_img_free(&img);

			img = switch_img_read_from_file("../../images/cluecon.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(img);
			switch_img_write_png(img, "cluecon-argb.png");
			switch_img_free(&img);

			img = switch_img_read_from_file("../../images/cluecon.jpg", SWITCH_IMG_FMT_I420);
			fst_requires(img);
			switch_img_write_png(img, "cluecon-jpg.png");
			switch_img_free(&img);
		}
		FST_TEST_END()

		FST_TEST_BEGIN(write_to_file)
		{
			switch_image_t *img;
			switch_status_t status;

			img = switch_img_read_from_file("../../images/cluecon.png", SWITCH_IMG_FMT_I420);
			fst_requires(img);
			status = switch_img_write_to_file(img, "cluecon-rgb-write.png", 0);
			switch_img_free(&img);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			img = switch_img_read_from_file("../../images/cluecon.png", SWITCH_IMG_FMT_ARGB);
			fst_requires(img);
			status = switch_img_write_to_file(img, "cluecon-argb-write.png", 0);
			switch_img_free(&img);
			fst_check(status == SWITCH_STATUS_SUCCESS);

			img = switch_img_read_from_file("../../images/cluecon.jpg", SWITCH_IMG_FMT_I420);
			fst_requires(img);
			status = switch_img_write_to_file(img, "cluecon-jpg-write.jpg", 100);
			switch_img_free(&img);
			fst_check(status == SWITCH_STATUS_SUCCESS);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

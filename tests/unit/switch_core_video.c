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
			switch_img_write_png(img, "test-rgb.png");

			switch_img_data_url_png(img, &data_url);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "I420: %s\n", data_url);
			free(data_url);
			data_url = NULL;

			switch_img_copy(img, &argb_img);

			{
				uint8_t *p = argb_img->planes[0];
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%d %d %d %d\n", *p, *(p+1), *(p+2), *(p+3));
			}

			switch_img_write_png(argb_img, "test-argb.png");
			switch_img_data_url_png(argb_img, &data_url);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "ARGB: %s\n", data_url);
			free(data_url);


			switch_img_free(&img);
			switch_img_free(&argb_img);
		}
		FST_TEST_END()
	}
	FST_SUITE_END()
}
FST_CORE_END()

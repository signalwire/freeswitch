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
 * test_image.c -- test images
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

		FST_TEST_BEGIN(scale_test)
		{
			char path[4096];
			switch_image_t *img;
			int i;

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "../images/signalwire.png");
			img = switch_img_read_png(path, SWITCH_IMG_FMT_I420);

			for(i = 2; i <= 10; i += 2) {
				switch_image_t *scaled_img = NULL;
				char name[1024];

				switch_snprintf(name, sizeof(name), "../images/signalwire-scaled-I420-%d.png", i);
				sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, name);
				switch_img_scale(img, &scaled_img, img->d_w / i, img->d_h / i);
				fst_requires(scaled_img);
				switch_img_write_png(scaled_img, path);
				switch_img_free(&scaled_img);
			}

			switch_img_free(&img);

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "../images/signalwire.png");
			img = switch_img_read_png(path, SWITCH_IMG_FMT_ARGB);

			for(i = 2; i <= 10; i += 2) {
				switch_image_t *scaled_img = NULL;
				char name[1024];

				switch_snprintf(name, sizeof(name), "../images/signalwire-scaled-ARGB-%d.png", i);
				sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, name);
				switch_img_scale(img, &scaled_img, img->d_w / i, img->d_h / i);
				fst_requires(scaled_img);
				switch_img_write_png(scaled_img, path);
				switch_img_free(&scaled_img);
			}

			switch_img_free(&img);

		}
		FST_TEST_END()

		FST_TEST_BEGIN(scale_test)
		{
			char path[1024];
			switch_image_t *img;
			char *font_face = "font/AEH.ttf";
			char *fg = "#000000";
			char *altfg = "#FFFFFF";
			char *bg = NULL;
			int font_size = 6;
			switch_img_txt_handle_t *txthandle = NULL;
			const char *txt = "FEESWITCH ROCKS";
			const char *alttxt = "freeswitch";

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "../images/signalwire-scaled-ARGB-8.png");
			img = switch_img_read_png(path, SWITCH_IMG_FMT_ARGB);

			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, font_face);
			switch_img_txt_handle_create(&txthandle, path, fg, bg, font_size, 0, NULL);
			switch_img_txt_handle_render(txthandle, img, 50, 3, txt, NULL, fg, bg, font_size, 0);
			switch_img_txt_handle_render(txthandle, img, 60, 15, alttxt, NULL, altfg, "#000000", font_size, 0);
			sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "../images/signalwire-scaled-ARGB-8-txt.png");
			switch_img_write_png(img, path);

			switch_img_free(&img);
			switch_img_txt_handle_destroy(&txthandle);
		}
		FST_TEST_END()



	}
	FST_SUITE_END()
}
FST_CORE_END()

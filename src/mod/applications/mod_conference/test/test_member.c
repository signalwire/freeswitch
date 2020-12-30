/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2019, Anthony Minessale II <anthm@freeswitch.org>
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
 * test_member.c -- tests member functions
 *
 */
#include <switch.h>
#include <stdlib.h>
#include <mod_conference.h>
#include <conference_member.c>

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

		FST_TEST_BEGIN(member_test)
		{
			char path[4096];
			char logo[1024];
			conference_member_t smember = { 0 };
			conference_member_t *member = &smember;
			switch_image_t *img;
			int i;

			sprintf(logo, "%s%s%s%s%s%s%s", "{position=left-bot,text_x=center,"
			"center_offset=190,text=#000000:transparent:", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "font/AEH.ttf:50:"
			"'FREESWITCH ROCKS',alt_text_x=center,alt_center_offset=190,"
			"alt_text_y=88,alt_text=#ffffff:transparent:", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "font/AEH.ttf:40:"
			"'freeswitch'}");
			sprintf(path, "%s%s%s%s", logo, SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, "../images/signalwire.png");
			switch_mutex_init(&member->write_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->flag_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->fnode_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->audio_in_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->audio_out_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_mutex_init(&member->read_mutex, SWITCH_MUTEX_NESTED, fst_pool);
			switch_thread_rwlock_create(&member->rwlock, fst_pool);

			conference_member_set_logo(member, path);
			img = member->video_logo;

			for(i = 2; i <= 10; i += 2) {
				switch_image_t *scaled_img = NULL;
				char name[1024];

				switch_snprintf(name, sizeof(name), "../images/logo-signalwire-scaled-%d.png", i);
				sprintf(path, "%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, name);
				switch_img_scale(img, &scaled_img, img->d_w / i, img->d_h / i);
				fst_requires(scaled_img);
				switch_img_write_png(scaled_img, path);
				switch_img_free(&scaled_img);
			}

			switch_img_free(&img);
		}
		FST_TEST_END()

	}
	FST_SUITE_END()
}
FST_CORE_END()

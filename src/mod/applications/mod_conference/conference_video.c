/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * Neal Horman <neal at wanlink dot com>
 * Bret McDanel <trixter at 0xdecafbad dot com>
 * Dale Thatcher <freeswitch at dalethatcher dot com>
 * Chris Danielson <chris at maxpowersoft dot com>
 * Rupa Schomaker <rupa@rupa.com>
 * David Weekly <david@weekly.org>
 * Joao Mesquita <jmesquita@gmail.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Seven Du <dujinfang@gmail.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 * William King <william.king@quentustech.com>
 *
 * mod_conference.c -- Software Conference Bridge
 *
 */
#include <mod_conference.h>

static struct conference_fps FPS_VALS[] = {
	{1.0f, 1000, 90},
	{5.0f, 200, 450},
	{10.0f, 100, 900},
	{15.0f, 66, 1364},
	{16.60f, 60, 1500},
	{20.0f, 50, 4500},
	{25.0f, 40, 2250},
	{30.0f, 33, 2700},
	{33.0f, 30, 2790},
	{66.60f, 15, 6000},
	{100.0f, 10, 9000},
	{0,0,0}
};


int conference_video_set_fps(conference_obj_t *conference, float fps)
{
	int i = 0, j = 0;

	for (i = 0; FPS_VALS[i].ms; i++) {
		if (FPS_VALS[i].fps == fps) {

			conference->video_fps = FPS_VALS[i];

			for (j = 0; j <= conference->canvas_count; j++) {
				if (conference->canvases[j]) {
					conference->canvases[j]->video_timer_reset = 1;
				}
			}

			return 1;
		}
	}

	return 0;
}


void conference_video_parse_layouts(conference_obj_t *conference, int WIDTH, int HEIGHT)
{
	switch_event_t *params;
	switch_xml_t cxml = NULL, cfg = NULL, x_layouts, x_layout, x_layout_settings, x_group, x_groups, x_image;
	char cmd_str[256] = "";

	switch_mutex_lock(conference_globals.setup_mutex);
	if (!conference->layout_hash) {
		switch_core_hash_init(&conference->layout_hash);
	}


	if (!conference->layout_group_hash) {
		switch_core_hash_init(&conference->layout_group_hash);
	}
	switch_mutex_unlock(conference_globals.setup_mutex);

	switch_event_create(&params, SWITCH_EVENT_COMMAND);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "conference_name", conference->name);

	if (!(cxml = switch_xml_open_cfg("conference_layouts.conf", &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", "conference_layouts.conf");
		goto done;
	}

	if ((x_layout_settings = switch_xml_child(cfg, "layout-settings"))) {
		if ((x_layouts = switch_xml_child(x_layout_settings, "layouts"))) {
			for (x_layout = switch_xml_child(x_layouts, "layout"); x_layout; x_layout = x_layout->next) {
				video_layout_t *vlayout;
				const char *val = NULL, *name = NULL;
				switch_bool_t auto_3d = SWITCH_FALSE;

				if ((val = switch_xml_attr(x_layout, "name"))) {
					name = val;
				}

				if (!name) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid layout\n");
					continue;
				}

				auto_3d = switch_true(switch_xml_attr(x_layout, "auto-3d-position"));

				vlayout = switch_core_alloc(conference->pool, sizeof(*vlayout));
				vlayout->name = switch_core_strdup(conference->pool, name);

				for (x_image = switch_xml_child(x_layout, "image"); x_image; x_image = x_image->next) {
					const char *res_id = NULL, *audio_position = NULL;
					int x = -1, y = -1, scale = -1, hscale = -1, floor = 0, flooronly = 0, fileonly = 0, overlap = 0, zoom = 0;

					if ((val = switch_xml_attr(x_image, "x"))) {
						x = atoi(val);
					}

					if ((val = switch_xml_attr(x_image, "y"))) {
						y = atoi(val);
					}

					if ((val = switch_xml_attr(x_image, "scale"))) {
						scale = atoi(val);
					}

					if ((val = switch_xml_attr(x_image, "hscale"))) {
						hscale = atoi(val);
					}

					if ((val = switch_xml_attr(x_image, "zoom"))) {
						zoom = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "floor"))) {
						floor = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "floor-only"))) {
						flooronly = floor = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "file-only"))) {
						fileonly = floor = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "overlap"))) {
						overlap = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "reservation_id"))) {
						res_id = val;
					}

					if ((val = switch_xml_attr(x_image, "audio-position"))) {
						audio_position = val;
					}


					if (x < 0 || y < 0 || scale < 0 || !name) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid image\n");
						continue;
					}

					if (hscale == -1) {
						hscale = scale;
					}

					vlayout->images[vlayout->layers].x = x;
					vlayout->images[vlayout->layers].y = y;
					vlayout->images[vlayout->layers].scale = scale;
					vlayout->images[vlayout->layers].hscale = hscale;
					vlayout->images[vlayout->layers].zoom = zoom;
					vlayout->images[vlayout->layers].floor = floor;
					vlayout->images[vlayout->layers].flooronly = flooronly;
					vlayout->images[vlayout->layers].fileonly = fileonly;
					vlayout->images[vlayout->layers].overlap = overlap;

					if (res_id) {
						vlayout->images[vlayout->layers].res_id = switch_core_strdup(conference->pool, res_id);
					}

					if (auto_3d || audio_position) {
						if (auto_3d || !strcasecmp(audio_position, "auto")) {
							int x_pos = WIDTH * x / VIDEO_LAYOUT_SCALE;
							int y_pos = HEIGHT * y / VIDEO_LAYOUT_SCALE;
							int width = WIDTH * scale / VIDEO_LAYOUT_SCALE;
							int height = HEIGHT * hscale / VIDEO_LAYOUT_SCALE;
							int center_x = x_pos + width / 2;
							int center_y = y_pos + height / 2;
							int half_x = WIDTH / 2;
							int half_y = HEIGHT / 2;
							float xv = 0, yv = 0;

							if (center_x > half_x) {
								xv = (float)(center_x - half_x) / half_x;
							} else {
								xv = (float) -1 - (center_x / half_x) * -1;
							}

							if (center_y > half_y) {
								yv = -1 - ((center_y - half_y) / half_y) * -1;
							} else {
								yv = center_y / half_y;
							}

							vlayout->images[vlayout->layers].audio_position = switch_core_sprintf(conference->pool, "%02f:0.0:%02f", xv, yv);

						} else {
							vlayout->images[vlayout->layers].audio_position = switch_core_strdup(conference->pool, audio_position);
						}
					}

					vlayout->layers++;
				}

				switch_core_hash_insert(conference->layout_hash, name, vlayout);
				switch_snprintf(cmd_str, sizeof(cmd_str), "add conference ::conference::conference_list_conferences vid-layout %s", name);
				switch_console_set_complete(cmd_str);
			}

		}

		if ((x_groups = switch_xml_child(x_layout_settings, "groups"))) {
			for (x_group = switch_xml_child(x_groups, "group"); x_group; x_group = x_group->next) {
				const char *name = switch_xml_attr(x_group, "name");
				layout_group_t *lg;
				video_layout_node_t *last_vlnode = NULL;

				x_layout = switch_xml_child(x_group, "layout");

				if (!name || !x_group || !x_layout) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid group\n");
					continue;
				}

				lg = switch_core_alloc(conference->pool, sizeof(*lg));
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding layout group %s\n", name);
				switch_core_hash_insert(conference->layout_group_hash, name, lg);

				while(x_layout) {
					const char *nname = x_layout->txt;
					video_layout_t *vlayout;
					video_layout_node_t *vlnode;

					if ((vlayout = switch_core_hash_find(conference->layout_hash, nname))) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding node %s to layout group %s\n", nname, name);

						vlnode = switch_core_alloc(conference->pool, sizeof(*vlnode));
						vlnode->vlayout = vlayout;

						if (last_vlnode) {
							last_vlnode->next = vlnode;
							last_vlnode = last_vlnode->next;
						} else {
							lg->layouts = last_vlnode = vlnode;
						}


					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid group member %s\n", nname);
					}

					x_layout = x_layout->next;
				}
			}
		}
	}


 done:

	if (cxml) {
		switch_xml_free(cxml);
		cxml = NULL;
	}

	switch_event_destroy(&params);

}

/* do not use this on an img cropped with switch_img_set_rect() */
void conference_video_reset_image(switch_image_t *img, switch_rgb_color_t *color)
{
	switch_img_fill(img, 0, 0, img->d_w, img->d_h, color);
}

/* clear layer and conference_video_reset_layer called inside lock always */

void conference_video_clear_layer(mcu_layer_t *layer)
{
	switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->bgcolor);
	layer->banner_patched = 0;
	layer->refresh = 1;
}

void conference_video_reset_layer(mcu_layer_t *layer)
{
	layer->tagged = 0;

	switch_img_free(&layer->banner_img);
	switch_img_free(&layer->logo_img);
	switch_img_free(&layer->logo_text_img);

	layer->mute_patched = 0;
	layer->banner_patched = 0;
	layer->is_avatar = 0;

	if (layer->geometry.overlap) {
		layer->canvas->refresh = 1;
	}

	switch_img_free(&layer->img);
	layer->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, layer->screen_w, layer->screen_h, 1);
	switch_assert(layer->img);

	conference_video_clear_layer(layer);
	switch_img_free(&layer->cur_img);
}

void conference_video_scale_and_patch(mcu_layer_t *layer, switch_image_t *ximg, switch_bool_t freeze)
{
	switch_image_t *IMG, *img;

	switch_mutex_lock(layer->canvas->mutex);

	IMG = layer->canvas->img;
	img = ximg ? ximg : layer->cur_img;

	switch_assert(IMG);

	if (!img) {
		switch_mutex_unlock(layer->canvas->mutex);
		return;
	}

	if (layer->refresh) {
		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->letterbox_bgcolor);
		layer->refresh = 0;
	}

	if (layer->geometry.scale) {
		int img_w = 0, img_h = 0;
		double screen_aspect = 0, img_aspect = 0;
		int x_pos = layer->x_pos;
		int y_pos = layer->y_pos;

		img_w = layer->screen_w = IMG->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE;
		img_h = layer->screen_h = IMG->d_h * layer->geometry.hscale / VIDEO_LAYOUT_SCALE;

		screen_aspect = (double) layer->screen_w / layer->screen_h;
		img_aspect = (double) img->d_w / img->d_h;

		if (layer->geometry.zoom) {
			if (screen_aspect < img_aspect) {
				int cropsize = 0;
				double scale = 1;
				if (img->d_h != layer->screen_h) {
					scale = (double)layer->screen_h / img->d_h;
				}
				cropsize = ((img->d_w )-((double)layer->screen_w/scale)) / 2;

				switch_img_set_rect(img, cropsize, 0, layer->screen_w/scale, layer->screen_h/scale);
				img_aspect = (double) img->d_w / img->d_h;
			} else if (screen_aspect > img_aspect) {
				int cropsize = 0;
				double scale = 1;
				if (img->d_w != layer->screen_w) {
					scale = (double)layer->screen_w / img->d_w;
				}
				cropsize = ((img->d_h )-((double)layer->screen_h/scale)) / 2;

				switch_img_set_rect(img, 0, cropsize, layer->screen_w/scale, layer->screen_h/scale);
				img_aspect = (double) img->d_w / img->d_h;
			}
		}

		if (freeze) {
			switch_img_free(&layer->img);
		}

		if (screen_aspect > img_aspect) {
			img_w = img_aspect * layer->screen_h;
			x_pos += (layer->screen_w - img_w) / 2;
		} else if (screen_aspect < img_aspect) {
			img_h = layer->screen_w / img_aspect;
			y_pos += (layer->screen_h - img_h) / 2;
		}

		if (layer->img && (layer->img->d_w != img_w || layer->img->d_h != img_h)) {
			switch_img_free(&layer->img);
			layer->banner_patched = 0;
			conference_video_clear_layer(layer);
		}

		if (!layer->img) {
			layer->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, img_w, img_h, 1);
		}

		if (layer->banner_img && !layer->banner_patched) {
			switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->letterbox_bgcolor);
			switch_img_patch(IMG, layer->banner_img, layer->x_pos, layer->y_pos + (layer->screen_h - layer->banner_img->d_h));

			if (!freeze) {
				switch_img_set_rect(layer->img, 0, 0, layer->img->d_w, layer->img->d_h - layer->banner_img->d_h);
			}

			layer->banner_patched = 1;
		}

		switch_assert(layer->img);

		if (switch_img_scale(img, &layer->img, img_w, img_h) == SWITCH_STATUS_SUCCESS) {
			if (layer->bugged && layer->member_id > -1) {
				conference_member_t *member;
				if ((member = conference_member_get(layer->canvas->conference, layer->member_id))) {
					switch_frame_t write_frame = { 0 };
					write_frame.img = layer->img;
					switch_core_media_bug_patch_video(member->session, &write_frame);
					switch_thread_rwlock_unlock(member->rwlock);
				}
			}

			switch_img_patch(IMG, layer->img, x_pos, y_pos);
		}

		if (layer->logo_img) {
			int ew = layer->screen_w, eh = layer->screen_h - (layer->banner_img ? layer->banner_img->d_h : 0);
			int ex = 0, ey = 0;

			switch_img_fit(&layer->logo_img, ew, eh);
			switch_img_find_position(layer->logo_pos, ew, eh, layer->logo_img->d_w, layer->logo_img->d_h, &ex, &ey);
			switch_img_patch(IMG, layer->logo_img, layer->x_pos + ex, layer->y_pos + ey);
			if (layer->logo_text_img) {
				int tx = 0, ty = 0;
				switch_img_find_position(POS_LEFT_BOT,
										 layer->logo_img->d_w, layer->logo_img->d_h, layer->logo_text_img->d_w, layer->logo_text_img->d_h, &tx, &ty);
				switch_img_patch(IMG, layer->logo_text_img, layer->x_pos + ex + tx, layer->y_pos + ey + ty);
			}

		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "insert at %d,%d\n", 0, 0);
		switch_img_patch(IMG, img, 0, 0);
	}

	switch_mutex_unlock(layer->canvas->mutex);
}

void conference_video_set_canvas_bgcolor(mcu_canvas_t *canvas, char *color)
{
	switch_color_set_rgb(&canvas->bgcolor, color);
	conference_video_reset_image(canvas->img, &canvas->bgcolor);
}

void conference_video_set_canvas_letterbox_bgcolor(mcu_canvas_t *canvas, char *color)
{
	switch_color_set_rgb(&canvas->letterbox_bgcolor, color);
}

void conference_video_check_used_layers(mcu_canvas_t *canvas)
{
	int i;

	if (!canvas) return;

	canvas->layers_used = 0;
	for (i = 0; i < canvas->total_layers; i++) {
		if (canvas->layers[i].member_id) {
			canvas->layers_used++;
		}
	}
}

void conference_video_detach_video_layer(conference_member_t *member)
{
	mcu_layer_t *layer = NULL;
	mcu_canvas_t *canvas = NULL;

	switch_mutex_lock(member->conference->canvas_mutex);

	if (member->canvas_id < 0) goto end;

	canvas = member->conference->canvases[member->canvas_id];

	if (!canvas || member->video_layer_id < 0) {
		goto end;
	}

	switch_mutex_lock(canvas->mutex);

	layer = &canvas->layers[member->video_layer_id];

	if (layer->geometry.audio_position) {
		conference_api_sub_position(member, NULL, "0:0:0");
	}

	if (layer->txthandle) {
		switch_img_txt_handle_destroy(&layer->txthandle);
	}

	conference_video_reset_layer(layer);
	layer->member_id = 0;
	member->video_layer_id = -1;
	member->layer_timeout = DEFAULT_LAYER_TIMEOUT;

	//member->canvas_id = 0;
	//member->watching_canvas_id = -1;
	member->avatar_patched = 0;
	conference_video_check_used_layers(canvas);
	canvas->send_keyframe = 1;
	switch_mutex_unlock(canvas->mutex);

 end:

	switch_mutex_unlock(member->conference->canvas_mutex);

}


void conference_video_layer_set_logo(conference_member_t *member, mcu_layer_t *layer, const char *path)
{
	const char *var = NULL;
	char *dup = NULL;
	switch_event_t *params = NULL;
	char *parsed = NULL;
	char *tmp;
	switch_img_position_t pos = POS_LEFT_TOP;

	switch_mutex_lock(layer->canvas->mutex);

	if (!path) {
		path = member->video_logo;
	}

	if (!path) {
		goto end;
	}

	if (path) {
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->logo_text_img);
	}

	if (*path == '{') {
		dup = strdup(path);
		path = dup;

		if (switch_event_create_brackets((char *)path, '{', '}', ',', &params, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		} else {
			path = parsed;
		}
	}


	if (zstr(path) || !strcasecmp(path, "reset")) {
		path = switch_channel_get_variable_dup(member->channel, "video_logo_path", SWITCH_FALSE, -1);
	}

	if (zstr(path) || !strcasecmp(path, "clear")) {
		switch_img_free(&layer->banner_img);
		layer->banner_patched = 0;

		switch_img_fill(member->conference->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h,
						&member->conference->canvas->letterbox_bgcolor);

		goto end;
	}

	if ((tmp = strchr(path, '}'))) {
		path = tmp + 1;
	}


	if (params) {
		if ((var = switch_event_get_header(params, "position"))) {
			pos = parse_img_position(var);
		}
	}

	if (path && strcasecmp(path, "clear")) {
		layer->logo_img = switch_img_read_png(path, SWITCH_IMG_FMT_ARGB);
	}

	if (layer->logo_img) {
		layer->logo_pos = pos;

		if (params) {
			if ((var = switch_event_get_header(params, "text"))) {
				layer->logo_text_img = switch_img_write_text_img(layer->screen_w, layer->screen_h, SWITCH_FALSE, var);
			}
		}
	}

	if (params) switch_event_destroy(&params);

	switch_safe_free(dup);

 end:

	switch_mutex_unlock(layer->canvas->mutex);

}

void conference_video_layer_set_banner(conference_member_t *member, mcu_layer_t *layer, const char *text)
{
	switch_rgb_color_t fgcolor, bgcolor;
	int font_scale = 4;
	int font_size = 0;
	const char *fg = "#cccccc";
	const char *bg = "#142e55";
	char *parsed = NULL;
	switch_event_t *params = NULL;
	const char *font_face = NULL;
	const char *var, *tmp = NULL;
	char *dup = NULL;

	switch_mutex_lock(layer->canvas->mutex);

	if (!text) {
		text = member->video_banner_text;
	}

	if (!text) {
		goto end;
	}

	if (*text == '{') {
		dup = strdup(text);
		text = dup;

		if (switch_event_create_brackets((char *)text, '{', '}', ',', &params, &parsed, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS || !parsed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
		} else {
			text = parsed;
		}
	}

	if (zstr(text) || !strcasecmp(text, "reset")) {
		text = switch_channel_get_variable_dup(member->channel, "video_banner_text", SWITCH_FALSE, -1);
	}

	if (zstr(text) || !strcasecmp(text, "clear")) {
		switch_img_free(&layer->banner_img);
		layer->banner_patched = 0;

		switch_img_fill(member->conference->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h,
						&member->conference->canvas->letterbox_bgcolor);

		goto end;
	}

	if ((tmp = strchr(text, '}'))) {
		text = tmp + 1;
	}

	if (params) {
		if ((var = switch_event_get_header(params, "fg"))) {
			fg = var;
		}

		if ((var = switch_event_get_header(params, "bg"))) {
			bg = var;
		}

		if ((var = switch_event_get_header(params, "font_face"))) {
			font_face = var;
		}

		if ((var = switch_event_get_header(params, "font_scale"))) {
			int tmp = atoi(var);

			if (tmp >= 5 && tmp <= 50) {
				font_scale = tmp;
			}
		}
	}

	if (layer->screen_h < layer->screen_w) {
		font_size = (double)(font_scale / 100.0f) * layer->screen_h;
	} else {
		font_size = (double)(font_scale / 100.0f) * layer->screen_w;
	}

	switch_color_set_rgb(&fgcolor, fg);
	switch_color_set_rgb(&bgcolor, bg);

	if (layer->txthandle) {
		switch_img_txt_handle_destroy(&layer->txthandle);
	}

	switch_img_txt_handle_create(&layer->txthandle, font_face, fg, bg, font_size, 0, NULL);

	if (!layer->txthandle) {
		switch_img_free(&layer->banner_img);
		layer->banner_patched = 0;

		switch_img_fill(member->conference->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h,
						&member->conference->canvas->letterbox_bgcolor);

		goto end;
	}

	switch_img_free(&layer->banner_img);
	switch_img_free(&layer->logo_img);
	switch_img_free(&layer->logo_text_img);
	layer->banner_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, layer->screen_w, font_size * 2, 1);

	conference_video_reset_image(layer->banner_img, &bgcolor);
	switch_img_txt_handle_render(layer->txthandle, layer->banner_img, font_size / 2, font_size / 2, text, NULL, fg, bg, 0, 0);

 end:

	if (params) switch_event_destroy(&params);

	switch_safe_free(dup);

	switch_mutex_unlock(layer->canvas->mutex);
}

void conference_video_reset_video_bitrate_counters(conference_member_t *member)
{
	member->managed_kps = 0;
	member->blackouts = 0;
	member->good_img = 0;
	member->blanks = 0;
}

switch_status_t conference_video_attach_video_layer(conference_member_t *member, mcu_canvas_t *canvas, int idx)
{
	mcu_layer_t *layer = NULL;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *var = NULL;

	if (!member->session) abort();

	channel = switch_core_session_get_channel(member->session);


	if (!switch_channel_test_flag(channel, CF_VIDEO) && !member->avatar_png_img) {
		return SWITCH_STATUS_FALSE;
	}

	if (member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY && !member->avatar_png_img) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(member->conference->canvas_mutex);

	switch_mutex_lock(canvas->mutex);

	layer = &canvas->layers[idx];

	layer->tagged = 0;

	if (layer->fnode || layer->geometry.fileonly) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (layer->geometry.flooronly && member->id != member->conference->video_floor_holder) {
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (layer->geometry.res_id) {
		if (!member->video_reservation_id || strcmp(layer->geometry.res_id, member->video_reservation_id)) {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}

	if (layer->member_id && layer->member_id == member->id) {
		member->video_layer_id = idx;
		switch_goto_status(SWITCH_STATUS_BREAK, end);
	}

	if (layer->geometry.res_id || member->video_reservation_id) {
		if (!layer->geometry.res_id || !member->video_reservation_id || strcmp(layer->geometry.res_id, member->video_reservation_id)) {
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}
	}

	if (member->video_layer_id > -1) {
		conference_video_detach_video_layer(member);
	}

	conference_video_reset_layer(layer);
	switch_img_free(&layer->mute_img);

	member->avatar_patched = 0;

	if (member->avatar_png_img) {
		layer->is_avatar = 1;
	}

	var = NULL;
	if (member->video_banner_text || (var = switch_channel_get_variable_dup(channel, "video_banner_text", SWITCH_FALSE, -1))) {
		conference_video_layer_set_banner(member, layer, var);
	}

	var = NULL;
	if (member->video_logo || (var = switch_channel_get_variable_dup(channel, "video_logo_path", SWITCH_FALSE, -1))) {
		conference_video_layer_set_logo(member, layer, var);
	}

	layer->member_id = member->id;
	member->video_layer_id = idx;
	member->canvas_id = canvas->canvas_id;
	member->layer_timeout = DEFAULT_LAYER_TIMEOUT;
	canvas->send_keyframe = 1;

	//member->watching_canvas_id = canvas->canvas_id;
	conference_video_check_used_layers(canvas);

	if (layer->geometry.audio_position) {
		conference_api_sub_position(member, NULL, layer->geometry.audio_position);
	}

	switch_img_fill(canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &canvas->letterbox_bgcolor);
	conference_video_reset_video_bitrate_counters(member);

 end:

	switch_mutex_unlock(canvas->mutex);

	switch_mutex_unlock(member->conference->canvas_mutex);

	return status;
}

void conference_video_init_canvas_layers(conference_obj_t *conference, mcu_canvas_t *canvas, video_layout_t *vlayout)
{
	int i = 0;

	if (!canvas) return;

	switch_mutex_lock(canvas->mutex);
	canvas->layout_floor_id = -1;

	if (!vlayout) {
		vlayout = canvas->new_vlayout;
		canvas->new_vlayout = NULL;
	}

	if (!vlayout) {
		switch_mutex_unlock(canvas->mutex);
		return;
	}

	canvas->vlayout = vlayout;

	for (i = 0; i < vlayout->layers; i++) {
		mcu_layer_t *layer = &canvas->layers[i];
		layer->geometry.x = vlayout->images[i].x;
		layer->geometry.y = vlayout->images[i].y;
		layer->geometry.hscale = vlayout->images[i].scale;
		if (vlayout->images[i].hscale) {
			layer->geometry.hscale = vlayout->images[i].hscale;
		}
		layer->geometry.scale = vlayout->images[i].scale;
		layer->geometry.zoom = vlayout->images[i].zoom;
		layer->geometry.floor = vlayout->images[i].floor;
		layer->geometry.overlap = vlayout->images[i].overlap;
		layer->idx = i;
		layer->refresh = 1;

		layer->screen_w = canvas->img->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE;
		layer->screen_h = canvas->img->d_h * layer->geometry.hscale / VIDEO_LAYOUT_SCALE;

		// if (layer->screen_w % 2) layer->screen_w++; // round to even
		// if (layer->screen_h % 2) layer->screen_h++; // round to even

		layer->x_pos = canvas->img->d_w * layer->geometry.x / VIDEO_LAYOUT_SCALE;
		layer->y_pos = canvas->img->d_h * layer->geometry.y / VIDEO_LAYOUT_SCALE;


		if (layer->geometry.floor) {
			canvas->layout_floor_id = i;
		}

		/* if we ever decided to reload layers config on demand the pointer assignment below  will lead to segs but we
		   only load them once forever per conference so these pointers are valid for the life of the conference */
		layer->geometry.res_id = vlayout->images[i].res_id;
		layer->geometry.audio_position = vlayout->images[i].audio_position;
	}

	conference_video_reset_image(canvas->img, &canvas->bgcolor);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		mcu_layer_t *layer = &canvas->layers[i];

		layer->member_id = 0;
		layer->tagged = 0;
		layer->banner_patched = 0;
		layer->refresh = 1;
		layer->canvas = canvas;
		conference_video_reset_layer(layer);
	}

	canvas->layers_used = 0;
	canvas->total_layers = vlayout->layers;
	canvas->send_keyframe = 1;

	switch_mutex_unlock(canvas->mutex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Canvas position %d applied layout %s\n", canvas->canvas_id, vlayout->name);

}

switch_status_t conference_video_attach_canvas(conference_obj_t *conference, mcu_canvas_t *canvas, int super)
{
	if (conference->canvas_count >= MAX_CANVASES + 1) {
		return SWITCH_STATUS_FALSE;
	}

	canvas->canvas_id = conference->canvas_count;

	if (!super) {
		conference->canvas_count++;

		if (!conference->canvas) {
			conference->canvas = canvas;
		}
	}

	conference->canvases[canvas->canvas_id] = canvas;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Canvas attached to position %d\n", canvas->canvas_id);

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t conference_video_init_canvas(conference_obj_t *conference, video_layout_t *vlayout, mcu_canvas_t **canvasP)
{
	mcu_canvas_t *canvas;

	if (conference->canvas_count >= MAX_CANVASES) {
		return SWITCH_STATUS_FALSE;
	}

	canvas = switch_core_alloc(conference->pool, sizeof(*canvas));
	canvas->conference = conference;
	canvas->pool = conference->pool;
	switch_mutex_init(&canvas->mutex, SWITCH_MUTEX_NESTED, conference->pool);
	canvas->layout_floor_id = -1;

	switch_img_free(&canvas->img);

	canvas->width = conference->canvas_width;
	canvas->height = conference->canvas_height;

	canvas->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, canvas->width, canvas->height, 0);
	switch_queue_create(&canvas->video_queue, 200, canvas->pool);

	switch_assert(canvas->img);

	switch_mutex_lock(canvas->mutex);
	conference_video_set_canvas_bgcolor(canvas, conference->video_canvas_bgcolor);
	conference_video_set_canvas_letterbox_bgcolor(canvas, conference->video_letterbox_bgcolor);
	conference_video_init_canvas_layers(conference, canvas, vlayout);
	switch_mutex_unlock(canvas->mutex);

	canvas->canvas_id = -1;
	*canvasP = canvas;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Layout set to %s\n", vlayout->name);

	return SWITCH_STATUS_SUCCESS;
}

int conference_video_flush_queue(switch_queue_t *q)
{
	switch_image_t *img;
	void *pop;
	int r = 0;

	if (!q) return 0;

	while (switch_queue_size(q) > 1 && switch_queue_trypop(q, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		img = (switch_image_t *)pop;
		switch_img_free(&img);
		r++;
	}

	return r + switch_queue_size(q);
}


void conference_video_destroy_canvas(mcu_canvas_t **canvasP) {
	int i;
	mcu_canvas_t *canvas = *canvasP;

	switch_img_free(&canvas->img);
	conference_video_flush_queue(canvas->video_queue);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		switch_img_free(&canvas->layers[i].img);
	}

	*canvasP = NULL;
}

void conference_video_write_canvas_image_to_codec_group(conference_obj_t *conference, mcu_canvas_t *canvas, codec_set_t *codec_set,
														int codec_index, uint32_t timestamp, switch_bool_t need_refresh,
														switch_bool_t need_keyframe, switch_bool_t need_reset)

{
	conference_member_t *imember;
	switch_frame_t write_frame = { 0 }, *frame = NULL;
	switch_status_t encode_status = SWITCH_STATUS_FALSE;

	write_frame = codec_set->frame;
	frame = &write_frame;
	frame->img = codec_set->frame.img;
	frame->packet = codec_set->frame.packet;
	frame->packetlen = codec_set->frame.packetlen;

	switch_clear_flag(frame, SFF_SAME_IMAGE);
	frame->m = 0;
	frame->timestamp = timestamp;

	if (need_reset) {
		int type = 1; // sum flags: 1 encoder; 2; decoder
		switch_core_codec_control(&codec_set->codec, SCC_VIDEO_RESET, SCCT_INT, (void *)&type, NULL, NULL);
		need_refresh = SWITCH_TRUE;
	}

	if (need_refresh || need_keyframe) {
		switch_core_codec_control(&codec_set->codec, SCC_VIDEO_REFRESH, SCCT_NONE, NULL, NULL, NULL);
	}

	do {

		frame->data = ((unsigned char *)frame->packet) + 12;
		frame->datalen = SWITCH_DEFAULT_VIDEO_SIZE;

		encode_status = switch_core_codec_encode_video(&codec_set->codec, frame);

		if (encode_status == SWITCH_STATUS_SUCCESS || encode_status == SWITCH_STATUS_MORE_DATA) {

			switch_assert((encode_status == SWITCH_STATUS_SUCCESS && frame->m) || !frame->m);

			if (frame->datalen == 0) {
				break;
			}

			if (frame->timestamp) {
				switch_set_flag(frame, SFF_RAW_RTP_PARSE_FRAME);
			}

			frame->packetlen = frame->datalen + 12;

			switch_mutex_lock(conference->member_mutex);
			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;

				if (imember->watching_canvas_id != canvas->canvas_id) {
					continue;
				}

				if (conference_utils_member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
					continue;
				}

				if (imember->video_codec_index != codec_index) {
					continue;
				}

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				//if (need_refresh) {
				//	switch_core_session_request_video_refresh(imember->session);
				//}

				//switch_core_session_write_encoded_video_frame(imember->session, frame, 0, 0);
				switch_set_flag(frame, SFF_ENCODED);

				if (switch_frame_buffer_dup(imember->fb, frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					switch_queue_push(imember->mux_out_queue, dupframe);
					dupframe = NULL;
				}

				switch_clear_flag(frame, SFF_ENCODED);

				switch_core_session_rwunlock(imember->session);
			}
			switch_mutex_unlock(conference->member_mutex);
		}

	} while(encode_status == SWITCH_STATUS_MORE_DATA);
}

video_layout_t *conference_video_find_best_layout(conference_obj_t *conference, layout_group_t *lg, uint32_t count)
{
	video_layout_node_t *vlnode = NULL, *last = NULL;

	if (!count) count = conference->members_with_video + conference->members_with_avatar;

	for (vlnode = lg->layouts; vlnode; vlnode = vlnode->next) {
		if (vlnode->vlayout->layers >= count) {
			break;
		}

		last = vlnode;
	}

	return vlnode? vlnode->vlayout : last ? last->vlayout : NULL;
}

video_layout_t *conference_video_get_layout(conference_obj_t *conference, const char *video_layout_name, const char *video_layout_group)
{
	layout_group_t *lg = NULL;
	video_layout_t *vlayout = NULL;

	if (video_layout_group) {
		lg = switch_core_hash_find(conference->layout_group_hash, video_layout_group);
		vlayout = conference_video_find_best_layout(conference, lg, 0);
	} else {
		vlayout = switch_core_hash_find(conference->layout_hash, video_layout_name);
	}

	return vlayout;
}

void conference_video_vmute_snap(conference_member_t *member, switch_bool_t clear)
{


	if (member->canvas_id > -1 && member->video_layer_id > -1) {
		mcu_layer_t *layer = NULL;
		mcu_canvas_t *canvas = NULL;

		canvas = member->conference->canvases[member->canvas_id];

		switch_mutex_lock(canvas->mutex);
		layer = &canvas->layers[member->video_layer_id];
		switch_img_free(&layer->mute_img);
		switch_img_free(&member->video_mute_img);

		if (!clear && layer->cur_img) {
			switch_img_copy(layer->cur_img, &member->video_mute_img);
			switch_img_copy(layer->cur_img, &layer->mute_img);
		}

		switch_mutex_unlock(canvas->mutex);
	}
}


void conference_video_canvas_del_fnode_layer(conference_obj_t *conference, conference_file_node_t *fnode)
{
	mcu_canvas_t *canvas = conference->canvases[fnode->canvas_id];

	switch_mutex_lock(canvas->mutex);
	if (fnode->layer_id > -1) {
		mcu_layer_t *xlayer = &canvas->layers[fnode->layer_id];

		fnode->layer_id = -1;
		fnode->canvas_id = -1;
		xlayer->fnode = NULL;
		conference_video_reset_layer(xlayer);
	}
	switch_mutex_unlock(canvas->mutex);
}

void conference_video_canvas_set_fnode_layer(mcu_canvas_t *canvas, conference_file_node_t *fnode, int idx)
{
	mcu_layer_t *layer = NULL;
	mcu_layer_t *xlayer = NULL;

	switch_mutex_lock(canvas->mutex);

	if (idx == -1) {
		int i;

		if (canvas->layout_floor_id > -1) {
			idx = canvas->layout_floor_id;
			xlayer = &canvas->layers[idx];

			if (xlayer->fnode) {
				idx = -1;
			}
		}

		if (idx < 0) {
			for (i = 0; i < canvas->total_layers; i++) {
				xlayer = &canvas->layers[i];

				if (xlayer->fnode || xlayer->geometry.res_id || xlayer->member_id) {
					continue;
				}

				idx = i;
				break;
			}
		}
	}

	if (idx < 0) goto end;

	layer = &canvas->layers[idx];

	layer->fnode = fnode;
	fnode->layer_id = idx;
	fnode->canvas_id = canvas->canvas_id;

	if (layer->member_id > -1) {
		conference_member_t *member;

		if ((member = conference_member_get(canvas->conference, layer->member_id))) {
			conference_video_detach_video_layer(member);
			switch_thread_rwlock_unlock(member->rwlock);
		}
	}

 end:

	switch_mutex_unlock(canvas->mutex);
}


void conference_video_launch_muxing_write_thread(conference_member_t *member)
{
	switch_threadattr_t *thd_attr = NULL;
	switch_mutex_lock(conference_globals.hash_mutex);
	if (!member->video_muxing_write_thread) {
		switch_threadattr_create(&thd_attr, member->pool);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&member->video_muxing_write_thread, thd_attr, conference_video_muxing_write_thread_run, member, member->pool);
	}
	switch_mutex_unlock(conference_globals.hash_mutex);
}
void conference_video_launch_muxing_thread(conference_obj_t *conference, mcu_canvas_t *canvas, int super)
{
	switch_threadattr_t *thd_attr = NULL;

	switch_mutex_lock(conference_globals.hash_mutex);
	if (!canvas->video_muxing_thread) {
		switch_threadattr_create(&thd_attr, conference->pool);
		switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		conference_utils_set_flag(conference, CFLAG_VIDEO_MUXING);
		switch_thread_create(&canvas->video_muxing_thread, thd_attr,
							 super ? conference_video_super_muxing_thread_run : conference_video_muxing_thread_run, canvas, conference->pool);
	}
	switch_mutex_unlock(conference_globals.hash_mutex);
}

void *SWITCH_THREAD_FUNC conference_video_muxing_write_thread_run(switch_thread_t *thread, void *obj)
{
	conference_member_t *member = (conference_member_t *) obj;
	void *pop;
	int loops = 0;

	while(conference_utils_member_test_flag(member, MFLAG_RUNNING) || switch_queue_size(member->mux_out_queue)) {
		switch_frame_t *frame;

		if (conference_utils_member_test_flag(member, MFLAG_RUNNING)) {
			if (switch_queue_pop(member->mux_out_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				if (!pop) continue;

				if (loops == 0 || loops == 50) {
					switch_core_media_gen_key_frame(member->session);
					switch_core_session_request_video_refresh(member->session);
				}

				loops++;

				frame = (switch_frame_t *) pop;

				if (switch_test_flag(frame, SFF_ENCODED)) {
					switch_core_session_write_encoded_video_frame(member->session, frame, 0, 0);
				} else {
					switch_core_session_write_video_frame(member->session, frame, SWITCH_IO_FLAG_NONE, 0);
				}
				switch_frame_buffer_free(member->fb, &frame);
			}
		} else {
			if (switch_queue_trypop(member->mux_out_queue, &pop) == SWITCH_STATUS_SUCCESS) {
				if (pop) {
					frame = (switch_frame_t *) pop;
					switch_frame_buffer_free(member->fb, &frame);
				}
			}
		}
	}

	return NULL;
}

void conference_video_check_recording(conference_obj_t *conference, switch_frame_t *frame)
{
	conference_member_t *imember;

	if (!conference->recording_members) {
		return;
	}

	switch_mutex_lock(conference->member_mutex);

	for (imember = conference->members; imember; imember = imember->next) {
		if (!imember->rec) {
			continue;
		}
		if (switch_test_flag((&imember->rec->fh), SWITCH_FILE_OPEN) && switch_core_file_has_video(&imember->rec->fh)) {
			switch_core_file_write_video(&imember->rec->fh, frame);
		}
	}

	switch_mutex_unlock(conference->member_mutex);

}

void conference_video_check_avatar(conference_member_t *member, switch_bool_t force)
{
	const char *avatar = NULL, *var = NULL;
	mcu_canvas_t *canvas;

	if (member->canvas_id < 0) {
		return;
	}

	canvas = member->conference->canvases[member->canvas_id];

	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS) &&
		(!switch_channel_test_flag(member->channel, CF_VIDEO) || member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY)) {
		return;
	}

	if (canvas) {
		switch_mutex_lock(canvas->mutex);
	}

	member->avatar_patched = 0;

	if (!force && switch_channel_test_flag(member->channel, CF_VIDEO) && member->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
		conference_utils_member_set_flag_locked(member, MFLAG_ACK_VIDEO);
	} else {
		if (member->conference->no_video_avatar) {
			avatar = member->conference->no_video_avatar;
		}

		if ((var = switch_channel_get_variable_dup(member->channel, "video_no_video_avatar_png", SWITCH_FALSE, -1))) {
			avatar = var;
		}
	}

	if ((var = switch_channel_get_variable_dup(member->channel, "video_avatar_png", SWITCH_FALSE, -1))) {
		avatar = var;
	}

	switch_img_free(&member->avatar_png_img);

	if (avatar) {
		member->avatar_png_img = switch_img_read_png(avatar, SWITCH_IMG_FMT_I420);
	}

	if (force && !member->avatar_png_img && member->video_mute_img) {
		switch_img_copy(member->video_mute_img, &member->avatar_png_img);
	}

	if (canvas) {
		switch_mutex_unlock(canvas->mutex);
	}
}

void conference_video_check_flush(conference_member_t *member)
{
	int flushed;

	if (!member->channel || !switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return;
	}

	flushed = conference_video_flush_queue(member->video_queue);

	if (flushed && member->auto_avatar) {
		switch_channel_video_sync(member->channel);

		switch_img_free(&member->avatar_png_img);
		member->avatar_patched = 0;
		conference_video_reset_video_bitrate_counters(member);
		member->blanks = 0;
		member->auto_avatar = 0;
	}
}

void conference_video_patch_fnode(mcu_canvas_t *canvas, conference_file_node_t *fnode)
{
	if (fnode && fnode->layer_id > -1) {
		mcu_layer_t *layer = &canvas->layers[fnode->layer_id];
		switch_frame_t file_frame = { 0 };
		switch_status_t status = switch_core_file_read_video(&fnode->fh, &file_frame, SVR_FLUSH);

		if (status == SWITCH_STATUS_SUCCESS) {
			switch_img_free(&layer->cur_img);
			layer->cur_img = file_frame.img;
			layer->tagged = 1;
		} else if (status == SWITCH_STATUS_IGNORE) {
			if (canvas && fnode->layer_id > -1 ) {
				conference_video_canvas_del_fnode_layer(canvas->conference, fnode);
			}
		}
	}
}

void conference_video_fnode_check(conference_file_node_t *fnode) {
	mcu_canvas_t *canvas = fnode->conference->canvases[fnode->canvas_id];

	if (switch_core_file_has_video(&fnode->fh) && switch_core_file_read_video(&fnode->fh, NULL, SVR_CHECK) == SWITCH_STATUS_BREAK) {
		int full_screen = 0;

		if (fnode->fh.params && fnode->conference->canvas_count == 1) {
			full_screen = switch_true(switch_event_get_header(fnode->fh.params, "full-screen"));
		}

		if (full_screen) {
			canvas->play_file = 1;
			canvas->conference->playing_video_file = 1;
		} else {
			conference_video_canvas_set_fnode_layer(canvas, fnode, -1);
		}
	}
}


switch_status_t conference_video_find_layer(conference_obj_t *conference, mcu_canvas_t *canvas, conference_member_t *member, mcu_layer_t **layerP)
{
	uint32_t avatar_layers = 0;
	mcu_layer_t *layer = NULL;
	int i;

	switch_mutex_lock(conference->canvas_mutex);

	for (i = 0; i < canvas->total_layers; i++) {
		mcu_layer_t *xlayer = &canvas->layers[i];

		if (xlayer->is_avatar && xlayer->member_id != conference->video_floor_holder) {
			avatar_layers++;
		}
	}

	if (!layer &&
		(canvas->layers_used < canvas->total_layers ||
		 (avatar_layers && !member->avatar_png_img) || conference_utils_member_test_flag(member, MFLAG_MOD)) &&
		(member->avatar_png_img || member->video_flow != SWITCH_MEDIA_FLOW_SENDONLY)) {
		/* find an empty layer */
		for (i = 0; i < canvas->total_layers; i++) {
			mcu_layer_t *xlayer = &canvas->layers[i];

			if (xlayer->geometry.res_id) {
				if (member->video_reservation_id && !strcmp(xlayer->geometry.res_id, member->video_reservation_id)) {
					layer = xlayer;
					conference_video_attach_video_layer(member, canvas, i);
					break;
				}
			} else if (xlayer->geometry.flooronly && !xlayer->fnode) {
				if (member->id == conference->video_floor_holder) {
					layer = xlayer;
					conference_video_attach_video_layer(member, canvas, i);
					break;
				}
			} else if ((!xlayer->member_id || (!member->avatar_png_img &&
											   xlayer->is_avatar &&
											   xlayer->member_id != conference->video_floor_holder)) &&
					   !xlayer->fnode && !xlayer->geometry.fileonly) {
				switch_status_t lstatus;

				lstatus = conference_video_attach_video_layer(member, canvas, i);

				if (lstatus == SWITCH_STATUS_SUCCESS || lstatus == SWITCH_STATUS_BREAK) {
					layer = xlayer;
					break;
				}
			}
		}
	}

	switch_mutex_unlock(conference->canvas_mutex);

	if (layer) {
		*layerP = layer;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

void conference_video_next_canvas(conference_member_t *imember)
{
	if (imember->canvas_id == imember->conference->canvas_count - 1) {
		imember->canvas_id = 0;
	} else {
		imember->canvas_id++;
	}

	imember->layer_timeout = DEFAULT_LAYER_TIMEOUT;
}

void conference_video_pop_next_image(conference_member_t *member, switch_image_t **imgP)
{
	switch_image_t *img = *imgP;
	int size = 0;
	void *pop;

	if (!member->avatar_png_img && switch_channel_test_flag(member->channel, CF_VIDEO)) {
		do {
			if (switch_queue_trypop(member->video_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
				switch_img_free(&img);
				img = (switch_image_t *)pop;
				member->blanks = 0;
			} else {
				break;
			}
			size = switch_queue_size(member->video_queue);
		} while(size > member->conference->video_fps.fps / 2);

		if (conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) && member->video_layer_id > -1 && member->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
			if (img) {
				member->good_img++;
				if ((member->good_img % (int)(member->conference->video_fps.fps * 10)) == 0) {
					conference_video_reset_video_bitrate_counters(member);
				}
			} else {
				member->blanks++;
				member->good_img = 0;

				if (member->blanks == member->conference->video_fps.fps || (member->blanks % (int)(member->conference->video_fps.fps * 10)) == 0) {
					member->managed_kps = 0;
					switch_core_session_request_video_refresh(member->session);
				}

				if (member->blanks == member->conference->video_fps.fps * 5) {
					member->blackouts++;
					conference_video_check_avatar(member, SWITCH_TRUE);
					member->managed_kps = 0;

					if (member->avatar_png_img) {
						//if (layer) {
						//layer->is_avatar = 1;
						//}

						member->auto_avatar = 1;
					}
				}
			}
		}
	} else {
		conference_video_check_flush(member);
	}

	*imgP = img;
}

void conference_video_check_auto_bitrate(conference_member_t *member, mcu_layer_t *layer)
{
	if (conference_utils_test_flag(member->conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE) && !member->managed_kps) {
		switch_core_session_message_t msg = { 0 };
		int kps;
		int w = 320;
		int h = 240;

		if (layer) {
			if (layer->screen_w > 320 && layer->screen_h > 240) {
				w = layer->screen_w;
				h = layer->screen_h;
			}
		}

		if (!layer || !conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) || member->avatar_png_img) {
			kps = 200;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps because user's image is not visible\n",
							  switch_channel_get_name(member->channel), kps);
		} else {
			kps = switch_calc_bitrate(w, h, 2, member->conference->video_fps.fps);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps to accomodate %dx%d resolution\n",
							  switch_channel_get_name(member->channel), kps, layer->screen_w, layer->screen_h);
		}

		msg.message_id = SWITCH_MESSAGE_INDICATE_BITRATE_REQ;
		msg.numeric_arg = kps * 1024;
		msg.from = __FILE__;

		switch_core_session_receive_message(member->session, &msg);
		member->managed_kps = kps;
	}
}

void *SWITCH_THREAD_FUNC conference_video_muxing_thread_run(switch_thread_t *thread, void *obj)
{
	mcu_canvas_t *canvas = (mcu_canvas_t *) obj;
	conference_obj_t *conference = canvas->conference;
	conference_member_t *imember;
	switch_codec_t *check_codec = NULL;
	codec_set_t *write_codecs[MAX_MUX_CODECS] = { 0 };
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	int i = 0;
	uint32_t video_key_freq = 10000000;
	switch_time_t last_key_time = 0;
	mcu_layer_t *layer = NULL;
	switch_frame_t write_frame = { 0 };
	uint8_t *packet = NULL;
	switch_image_t *write_img = NULL, *file_img = NULL;
	uint32_t timestamp = 0;
	//video_layout_t *vlayout = conference_video_get_layout(conference);
	int members_with_video = 0, members_with_avatar = 0;
	int do_refresh = 0;
	int last_file_count = 0;

	canvas->video_timer_reset = 1;

	packet = switch_core_alloc(conference->pool, SWITCH_RTP_MAX_BUF_LEN);

	while (conference_globals.running && !conference_utils_test_flag(conference, CFLAG_DESTRUCT) && conference_utils_test_flag(conference, CFLAG_VIDEO_MUXING)) {
		switch_bool_t need_refresh = SWITCH_FALSE, need_keyframe = SWITCH_FALSE, need_reset = SWITCH_FALSE;
		switch_time_t now;
		int min_members = 0;
		int count_changed = 0;
		int file_count = 0, check_async_file = 0, check_file = 0;
		switch_image_t *async_file_img = NULL, *normal_file_img = NULL, *file_imgs[2] = { 0 };
		switch_frame_t file_frame = { 0 };
		int j = 0;

		switch_mutex_lock(canvas->mutex);
		if (canvas->new_vlayout) {
			conference_video_init_canvas_layers(conference, canvas, NULL);
		}
		switch_mutex_unlock(canvas->mutex);

		if (canvas->video_timer_reset) {
			canvas->video_timer_reset = 0;

			if (canvas->timer.interval) {
				switch_core_timer_destroy(&canvas->timer);
			}

			switch_core_timer_init(&canvas->timer, "soft", conference->video_fps.ms, conference->video_fps.samples, NULL);
			canvas->send_keyframe = 1;
		}

		if (!conference->playing_video_file) {
			switch_core_timer_next(&canvas->timer);
		}

		now = switch_micro_time_now();

		if (members_with_video != conference->members_with_video) {
			do_refresh = 100;
			count_changed = 1;
		}

		if (members_with_avatar != conference->members_with_avatar) {
			count_changed = 1;
		}

		if (count_changed && !conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;
			int canvas_count = 0;

			switch_mutex_lock(conference->member_mutex);
			for (imember = conference->members; imember; imember = imember->next) {
				if (imember->canvas_id == canvas->canvas_id || imember->canvas_id == -1) {
					canvas_count++;
				}
			}
			switch_mutex_unlock(conference->member_mutex);

			if (conference->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, conference->video_layout_group))) {
				if ((vlayout = conference_video_find_best_layout(conference, lg, canvas_count))) {
					switch_mutex_lock(conference->member_mutex);
					conference->canvas->new_vlayout = vlayout;
					switch_mutex_unlock(conference->member_mutex);
				}
			}
		}

		if (count_changed) {
			need_refresh = 1;
			need_keyframe = 1;
			do_refresh = 100;
		}

		if (conference->async_fnode && switch_core_file_has_video(&conference->async_fnode->fh)) {
			check_async_file = 1;
			file_count++;
		}

		if (conference->fnode && switch_core_file_has_video(&conference->fnode->fh)) {
			check_file = 1;
			file_count++;
		}

		if (file_count != last_file_count) {
			count_changed = 1;
		}

		last_file_count = file_count;

		if (do_refresh) {
			if ((do_refresh % 50) == 0) {
				switch_mutex_lock(conference->member_mutex);

				for (imember = conference->members; imember; imember = imember->next) {
					if (imember->canvas_id != canvas->canvas_id) continue;

					if (imember->session && switch_channel_test_flag(imember->channel, CF_VIDEO)) {
						switch_core_session_request_video_refresh(imember->session);
						switch_core_media_gen_key_frame(imember->session);
					}
				}
				switch_mutex_unlock(conference->member_mutex);
			}
			do_refresh--;
		}

		members_with_video = conference->members_with_video;
		members_with_avatar = conference->members_with_avatar;

		if (conference_utils_test_flag(conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
			if (conference->members_with_video < 3) {
				switch_yield(20000);
				continue;
			}
		}

		switch_mutex_lock(conference->member_mutex);

		for (imember = conference->members; imember; imember = imember->next) {
			switch_image_t *img = NULL;
			int i;

			if (!imember->session || (!switch_channel_test_flag(imember->channel, CF_VIDEO) && !imember->avatar_png_img) ||
				conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS) || switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}

			if (imember->watching_canvas_id == canvas->canvas_id && switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
				switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
				need_keyframe = SWITCH_TRUE;
			}

			if (conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) &&
				imember->watching_canvas_id > -1 && imember->watching_canvas_id == canvas->canvas_id &&
				!conference_utils_member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				min_members++;

				if (switch_channel_test_flag(imember->channel, CF_VIDEO)) {
					if (imember->video_codec_index < 0 && (check_codec = switch_core_session_get_video_write_codec(imember->session))) {
						for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
							if (check_codec->implementation->codec_id == write_codecs[i]->codec.implementation->codec_id) {
								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								break;
							}
						}

						if (imember->video_codec_index < 0) {
							write_codecs[i] = switch_core_alloc(conference->pool, sizeof(codec_set_t));

							if (switch_core_codec_copy(check_codec, &write_codecs[i]->codec,
													   &conference->video_codec_settings, conference->pool) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "Setting up video write codec %s at slot %d\n", write_codecs[i]->codec.implementation->iananame, i);

								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								write_codecs[i]->frame.packet = switch_core_alloc(conference->pool, buflen);
								write_codecs[i]->frame.data = ((uint8_t *)write_codecs[i]->frame.packet) + 12;
								write_codecs[i]->frame.packetlen = buflen;
								write_codecs[i]->frame.buflen = buflen - 12;
								switch_set_flag((&write_codecs[i]->frame), SFF_RAW_RTP);

							}
						}
					}

					if (imember->video_codec_index < 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Codec Error\n");
						switch_core_session_rwunlock(imember->session);
						continue;
					}
				}
			}

			if (imember->canvas_id > -1 && imember->canvas_id != canvas->canvas_id) {
				switch_core_session_rwunlock(imember->session);
				continue;
			}

			if (conference->playing_video_file) {
				switch_core_session_rwunlock(imember->session);
				continue;
			}

			//VIDFLOOR
			if (conference->canvas_count == 1 && canvas->layout_floor_id > -1 && imember->id == conference->video_floor_holder &&
				imember->video_layer_id != canvas->layout_floor_id) {
				conference_video_attach_video_layer(imember, canvas, canvas->layout_floor_id);
			}

			conference_video_pop_next_image(imember, &img);
			layer = NULL;

			switch_mutex_lock(canvas->mutex);
			//printf("MEMBER %d layer_id %d canvas: %d/%d\n", imember->id, imember->video_layer_id,
			//	   canvas->layers_used, canvas->total_layers);

			if (imember->video_layer_id > -1) {
				layer = &canvas->layers[imember->video_layer_id];
				if (layer->member_id != imember->id) {
					layer = NULL;
					imember->video_layer_id = -1;
					imember->layer_timeout = DEFAULT_LAYER_TIMEOUT;
				}
			}

			if (imember->avatar_png_img) {
				if (layer) {
					if (!imember->avatar_patched || !layer->cur_img) {
						layer->tagged = 1;
						//layer->is_avatar = 1;
						switch_img_free(&layer->cur_img);
						switch_img_copy(imember->avatar_png_img, &layer->cur_img);
						imember->avatar_patched = 1;
					}
				}
				switch_img_free(&img);
			}

			if (!layer) {
				if (conference_video_find_layer(conference, canvas, imember, &layer) == SWITCH_STATUS_SUCCESS) {
					imember->layer_timeout = 0;
				} else {
					if (--imember->layer_timeout <= 0) {
						conference_video_next_canvas(imember);
					}
				}
			}

			conference_video_check_auto_bitrate(imember, layer);

			if (layer) {

				//if (layer->cur_img && layer->cur_img != imember->avatar_png_img) {
				//	switch_img_free(&layer->cur_img);
				//}

				if (conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN)) {
					layer->mute_patched = 0;
				} else {
					switch_image_t *tmp;

					if (img && img != imember->avatar_png_img) {
						switch_img_free(&img);
					}

					if (!layer->mute_patched) {

						if (imember->video_mute_img || layer->mute_img) {
							conference_video_clear_layer(layer);

							if (!layer->mute_img && imember->video_mute_img) {
								//layer->mute_img = switch_img_read_png(imember->video_mute_png, SWITCH_IMG_FMT_I420);
								switch_img_copy(imember->video_mute_img, &layer->mute_img);
							}

							if (layer->mute_img) {
								conference_video_scale_and_patch(layer, layer->mute_img, SWITCH_FALSE);
							}
						}


						tmp = switch_img_write_text_img(layer->screen_w, layer->screen_h, SWITCH_TRUE, "VIDEO MUTED");
						switch_img_patch(canvas->img, tmp, layer->x_pos, layer->y_pos);
						switch_img_free(&tmp);

						layer->mute_patched = 1;
					}
				}


				if (img) {

					if (img != layer->cur_img) {
						switch_img_free(&layer->cur_img);
						layer->cur_img = img;
					}


					img = NULL;
					layer->tagged = 1;

					if (switch_core_media_bug_count(imember->session, "patch:video")) {
						layer->bugged = 1;
					}
				}
			}

			switch_mutex_unlock(canvas->mutex);

			if (img && img != imember->avatar_png_img) {
				switch_img_free(&img);
			}

			if (imember->session) {
				switch_core_session_rwunlock(imember->session);
			}
		}

		switch_mutex_unlock(conference->member_mutex);

		if (conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS)) {
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;
			conference_member_t *omember;

			if (video_key_freq && (now - last_key_time) > video_key_freq) {
				need_keyframe = SWITCH_TRUE;
				last_key_time = now;
			}

			switch_mutex_lock(conference->member_mutex);

			for (imember = conference->members; imember; imember = imember->next) {

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
					switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
					need_keyframe = SWITCH_TRUE;
				}

				if (count_changed) {
					int total = conference->members_with_video;

					if (!conference_utils_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
						total += conference->members_with_avatar;
					}

					if (imember->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
						total--;
					}

					if (total < 1) total = 1;

					if (conference->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, conference->video_layout_group))) {
						if ((vlayout = conference_video_find_best_layout(conference, lg, total + file_count))) {
							conference_video_init_canvas_layers(conference, imember->canvas, vlayout);
						}
					}
				}

				if (imember->video_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
					conference_video_pop_next_image(imember, &imember->pcanvas_img);
				}

				switch_core_session_rwunlock(imember->session);
			}

			if (check_async_file) {
				if (switch_core_file_read_video(&conference->async_fnode->fh, &file_frame, SVR_BLOCK | SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
					if ((async_file_img = file_frame.img)) {
						file_imgs[j++] = async_file_img;
					}
				}
			}

			if (check_file) {
				if (switch_core_file_read_video(&conference->fnode->fh, &file_frame, SVR_BLOCK | SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
					if ((normal_file_img = file_frame.img)) {
						file_imgs[j++] = normal_file_img;
					}
				}
			}

			for (imember = conference->members; imember; imember = imember->next) {
				int i = 0;

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO || imember->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				for (omember = conference->members; omember; omember = omember->next) {
					mcu_layer_t *layer = NULL;
					switch_image_t *use_img = NULL;

					if (!omember->session || !switch_channel_test_flag(omember->channel, CF_VIDEO) || omember->video_flow == SWITCH_MEDIA_FLOW_SENDONLY) {
						continue;
					}

					if (conference->members_with_video + conference->members_with_avatar != 1 && imember == omember) {
						continue;
					}

					if (i < imember->canvas->total_layers) {
						layer = &imember->canvas->layers[i++];
						if (layer->member_id != omember->id) {
							const char *var = NULL;

							layer->mute_patched = 0;
							layer->avatar_patched = 0;
							switch_img_free(&layer->banner_img);
							switch_img_free(&layer->logo_img);

							if (layer->geometry.audio_position) {
								conference_api_sub_position(omember, NULL, layer->geometry.audio_position);
							}

							var = NULL;
							if (omember->video_banner_text ||
								(var = switch_channel_get_variable_dup(omember->channel, "video_banner_text", SWITCH_FALSE, -1))) {
								conference_video_layer_set_banner(omember, layer, var);
							}

							var = NULL;
							if (omember->video_logo ||
								(var = switch_channel_get_variable_dup(omember->channel, "video_logo_path", SWITCH_FALSE, -1))) {
								conference_video_layer_set_logo(omember, layer, var);
							}
						}

						layer->member_id = omember->id;
					}

					if (!layer && omember->al) {
						conference_api_sub_position(omember, NULL, "0:0:0");
					}

					use_img = omember->pcanvas_img;

					if (layer) {

						if (use_img && !omember->avatar_png_img) {
							layer->avatar_patched = 0;
						} else {
							if (!layer->avatar_patched) {
								conference_video_scale_and_patch(layer, omember->avatar_png_img, SWITCH_FALSE);
								layer->avatar_patched = 1;
							}
							use_img = NULL;
							layer = NULL;
						}

						if (layer) {
							if (conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN)) {
								layer->mute_patched = 0;
							} else {
								if (!layer->mute_patched) {
									switch_image_t *tmp;
									conference_video_scale_and_patch(layer, imember->video_mute_img ? imember->video_mute_img : omember->pcanvas_img, SWITCH_FALSE);
									tmp = switch_img_write_text_img(layer->screen_w, layer->screen_h, SWITCH_TRUE, "VIDEO MUTED");
									switch_img_patch(imember->canvas->img, tmp, layer->x_pos, layer->y_pos);
									switch_img_free(&tmp);
									layer->mute_patched = 1;
								}

								use_img = NULL;
								layer = NULL;
							}
						}

						if (layer && use_img) {
							conference_video_scale_and_patch(layer, use_img, SWITCH_FALSE);
						}
					}

					conference_video_check_auto_bitrate(omember, layer);
				}

				for (j = 0; j < file_count; j++) {
					switch_image_t *img = file_imgs[j];

					if (i < imember->canvas->total_layers) {
						layer = &imember->canvas->layers[i++];
						conference_video_scale_and_patch(layer, img, SWITCH_FALSE);
					}
				}

				switch_core_session_rwunlock(imember->session);
			}

			switch_img_free(&normal_file_img);
			switch_img_free(&async_file_img);

			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (need_keyframe) {
					switch_core_media_gen_key_frame(imember->session);
				}

				switch_set_flag(&write_frame, SFF_RAW_RTP);
				write_frame.img = imember->canvas->img;
				write_frame.packet = packet;
				write_frame.data = ((uint8_t *)packet) + 12;
				write_frame.datalen = 0;
				write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
				write_frame.packetlen = 0;

				if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					switch_queue_push(imember->mux_out_queue, dupframe);
					dupframe = NULL;
				}

				switch_core_session_rwunlock(imember->session);
			}

			switch_mutex_unlock(conference->member_mutex);
		} else {

			if (canvas->canvas_id == 0) {
				if (conference->async_fnode) {
					if (conference->async_fnode->layer_id > -1) {
						conference_video_patch_fnode(canvas, conference->async_fnode);
					} else {
						conference_video_fnode_check(conference->async_fnode);
					}
				}

				if (conference->fnode) {
					if (conference->fnode->layer_id > -1) {
						conference_video_patch_fnode(canvas, conference->fnode);
					} else {
						conference_video_fnode_check(conference->fnode);
					}
				}
			}

			if (!conference->playing_video_file) {
				for (i = 0; i < canvas->total_layers; i++) {
					mcu_layer_t *layer = &canvas->layers[i];

					if (!layer->mute_patched && (layer->member_id > -1 || layer->fnode) && layer->cur_img && (layer->tagged || layer->geometry.overlap)) {
						if (canvas->refresh) {
							layer->refresh = 1;
							canvas->refresh++;
						}

						if (layer->cur_img) {
							conference_video_scale_and_patch(layer, NULL, SWITCH_FALSE);
						}

						layer->tagged = 0;
					}

					layer->bugged = 0;
				}
			}

			if (canvas->refresh > 1) {
				canvas->refresh = 0;
			}

			if (canvas->send_keyframe > 0) {
				if (canvas->send_keyframe == 1 || (canvas->send_keyframe % 10) == 0) {
					need_keyframe = SWITCH_TRUE;
					need_refresh = SWITCH_TRUE;
				}
				canvas->send_keyframe--;
			}

			if (video_key_freq && (now - last_key_time) > video_key_freq) {
				need_keyframe = SWITCH_TRUE;
				last_key_time = now;
			}

			write_img = canvas->img;
			timestamp = canvas->timer.samplecount;

			if (conference->playing_video_file) {
				if (switch_core_file_read_video(&conference->fnode->fh, &write_frame, SVR_BLOCK | SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
					switch_img_free(&file_img);

					if (canvas->play_file) {
						canvas->send_keyframe = 1;
						canvas->play_file = 0;

						canvas->timer.interval = 1;
						canvas->timer.samples = 90;
					}

					write_img = file_img = write_frame.img;

					switch_core_timer_sync(&canvas->timer);
					timestamp = canvas->timer.samplecount;
				}
			} else if (file_img) {
				switch_img_free(&file_img);
			}

			write_frame.img = write_img;

			if (conference->canvas_count == 1) {
				conference_video_check_recording(conference, &write_frame);
			}

			if (conference->canvas_count > 1) {
				switch_image_t *img_copy = NULL;

				switch_img_copy(write_img, &img_copy);

				if (switch_queue_trypush(canvas->video_queue, img_copy) != SWITCH_STATUS_SUCCESS) {
					switch_img_free(&img_copy);
				}
			}

			if (min_members && conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
				for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
					write_codecs[i]->frame.img = write_img;
					conference_video_write_canvas_image_to_codec_group(conference, canvas, write_codecs[i], i,
																	   timestamp, need_refresh, need_keyframe, need_reset);

					if (canvas->video_write_bandwidth) {
						switch_core_codec_control(&write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &canvas->video_write_bandwidth, NULL, NULL);
						canvas->video_write_bandwidth = 0;
					}

				}
			}

			switch_mutex_lock(conference->member_mutex);
			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;

				if (imember->watching_canvas_id != canvas->canvas_id) continue;

				if (conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) && !conference_utils_member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
					continue;
				}

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (need_keyframe) {
					switch_core_media_gen_key_frame(imember->session);
				}

				switch_set_flag(&write_frame, SFF_RAW_RTP);
				write_frame.img = write_img;
				write_frame.packet = packet;
				write_frame.data = ((uint8_t *)packet) + 12;
				write_frame.datalen = 0;
				write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
				write_frame.packetlen = 0;

				//switch_core_session_write_video_frame(imember->session, &write_frame, SWITCH_IO_FLAG_NONE, 0);

				if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					switch_queue_push(imember->mux_out_queue, dupframe);
					dupframe = NULL;
				}

				if (imember->session) {
					switch_core_session_rwunlock(imember->session);
				}
			}

			switch_mutex_unlock(conference->member_mutex);
		} // NOT PERSONAL
	}

	switch_img_free(&file_img);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		layer = &canvas->layers[i];

		switch_mutex_lock(canvas->mutex);
		switch_img_free(&layer->cur_img);
		switch_img_free(&layer->img);
		layer->banner_patched = 0;
		switch_img_free(&layer->banner_img);
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->logo_text_img);
		switch_img_free(&layer->mute_img);
		switch_mutex_unlock(canvas->mutex);

		if (layer->txthandle) {
			switch_img_txt_handle_destroy(&layer->txthandle);
		}
	}

	for (i = 0; i < MAX_MUX_CODECS; i++) {
		if (write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec)) {
			switch_core_codec_destroy(&write_codecs[i]->codec);
		}
	}

	switch_core_timer_destroy(&canvas->timer);
	conference_video_destroy_canvas(&canvas);

	return NULL;
}

void pop_conference_video_next_canvas_image(mcu_canvas_t *canvas, switch_image_t **imgP)
{
	switch_image_t *img = *imgP;
	int size = 0;
	void *pop;

	switch_img_free(&img);

	do {
		if (switch_queue_trypop(canvas->video_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
			switch_img_free(&img);
			img = (switch_image_t *)pop;
		} else {
			break;
		}
		size = switch_queue_size(canvas->video_queue);
	} while(size > canvas->conference->video_fps.fps / 2);

	*imgP = img;
}

void *SWITCH_THREAD_FUNC conference_video_super_muxing_thread_run(switch_thread_t *thread, void *obj)
{
	mcu_canvas_t *canvas = (mcu_canvas_t *) obj;
	conference_obj_t *conference = canvas->conference;
	conference_member_t *imember;
	switch_codec_t *check_codec = NULL;
	codec_set_t *write_codecs[MAX_MUX_CODECS] = { 0 };
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	int i = 0;
	switch_time_t last_key_time = 0;
	uint32_t video_key_freq = 10000000;
	mcu_layer_t *layer = NULL;
	switch_frame_t write_frame = { 0 };
	uint8_t *packet = NULL;
	switch_image_t *write_img = NULL;
	uint32_t timestamp = 0;
	int last_used_canvases[MAX_CANVASES] = { 0 };


	canvas->video_timer_reset = 1;

	packet = switch_core_alloc(conference->pool, SWITCH_RTP_MAX_BUF_LEN);

	while (conference_globals.running && !conference_utils_test_flag(conference, CFLAG_DESTRUCT) && conference_utils_test_flag(conference, CFLAG_VIDEO_MUXING)) {
		switch_bool_t need_refresh = SWITCH_FALSE, need_keyframe = SWITCH_FALSE, need_reset = SWITCH_FALSE;
		switch_time_t now;
		int min_members = 0;
		int count_changed = 0;
		int  layer_idx = 0, j = 0;
		switch_image_t *img = NULL;
		int used_canvases = 0;

		switch_mutex_lock(canvas->mutex);
		if (canvas->new_vlayout) {
			conference_video_init_canvas_layers(conference, canvas, NULL);
		}
		switch_mutex_unlock(canvas->mutex);

		if (canvas->video_timer_reset) {
			canvas->video_timer_reset = 0;

			if (canvas->timer.interval) {
				switch_core_timer_destroy(&canvas->timer);
			}

			switch_core_timer_init(&canvas->timer, "soft", conference->video_fps.ms, conference->video_fps.samples, NULL);
			canvas->send_keyframe = 1;
		}

		if (!conference->playing_video_file) {
			switch_core_timer_next(&canvas->timer);
		}

		now = switch_micro_time_now();

		if (canvas->send_keyframe > 0) {
			if (canvas->send_keyframe == 1 || (canvas->send_keyframe % 10) == 0) {
				need_keyframe = SWITCH_TRUE;
				need_refresh = SWITCH_TRUE;
			}
			canvas->send_keyframe--;
		}

		if (video_key_freq && (now - last_key_time) > video_key_freq) {
			need_keyframe = SWITCH_TRUE;
			last_key_time = now;
		}

		for (j = 0; j < conference->canvas_count; j++) {
			mcu_canvas_t *jcanvas = (mcu_canvas_t *) conference->canvases[j];

			if (jcanvas->layers_used > 0 || conference->super_canvas_show_all_layers) {
				used_canvases++;
			}

			if (jcanvas->layers_used != last_used_canvases[j]) {
				count_changed++;
			}

			last_used_canvases[j] = jcanvas->layers_used;
		}

		if (count_changed) {
			int total = used_canvases;
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;

			if (total < 1) total = 1;

			if ((lg = switch_core_hash_find(conference->layout_group_hash, CONFERENCE_MUX_DEFAULT_SUPER_LAYOUT))) {
				if ((vlayout = conference_video_find_best_layout(conference, lg, total))) {
					conference_video_init_canvas_layers(conference, canvas, vlayout);
				}
			}
		}

		switch_mutex_lock(conference->member_mutex);

		for (imember = conference->members; imember; imember = imember->next) {
			int i;

			if (!imember->session || (!switch_channel_test_flag(imember->channel, CF_VIDEO) && !imember->avatar_png_img) ||
				conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS) || switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}

			if (imember->watching_canvas_id == canvas->canvas_id && switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
				switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
				need_keyframe = SWITCH_TRUE;
			}

			if (conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) &&
				imember->watching_canvas_id > -1 && imember->watching_canvas_id == canvas->canvas_id &&
				!conference_utils_member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				min_members++;

				if (switch_channel_test_flag(imember->channel, CF_VIDEO)) {
					if (imember->video_codec_index < 0 && (check_codec = switch_core_session_get_video_write_codec(imember->session))) {
						for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
							if (check_codec->implementation->codec_id == write_codecs[i]->codec.implementation->codec_id) {
								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								break;
							}
						}

						if (imember->video_codec_index < 0) {
							write_codecs[i] = switch_core_alloc(conference->pool, sizeof(codec_set_t));

							if (switch_core_codec_copy(check_codec, &write_codecs[i]->codec,
													   &conference->video_codec_settings, conference->pool) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "Setting up video write codec %s at slot %d\n", write_codecs[i]->codec.implementation->iananame, i);

								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								write_codecs[i]->frame.packet = switch_core_alloc(conference->pool, buflen);
								write_codecs[i]->frame.data = ((uint8_t *)write_codecs[i]->frame.packet) + 12;
								write_codecs[i]->frame.packetlen = buflen;
								write_codecs[i]->frame.buflen = buflen - 12;
								switch_set_flag((&write_codecs[i]->frame), SFF_RAW_RTP);

							}
						}
					}

					if (imember->video_codec_index < 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Codec Error\n");
						switch_core_session_rwunlock(imember->session);
						continue;
					}
				}
			}

			switch_core_session_rwunlock(imember->session);
		}

		switch_mutex_unlock(conference->member_mutex);

		layer_idx = 0;

		for (j = 0; j < conference->canvas_count; j++) {
			mcu_canvas_t *jcanvas = (mcu_canvas_t *) conference->canvases[j];

			pop_conference_video_next_canvas_image(jcanvas, &img);

			if (!jcanvas->layers_used && !conference->super_canvas_show_all_layers) {
				switch_img_free(&img);
				continue;
			}

			if (layer_idx < canvas->total_layers) {
				layer = &canvas->layers[layer_idx++];

				if (layer->member_id != jcanvas->canvas_id) {
					layer->member_id = jcanvas->canvas_id;
					switch_img_free(&layer->cur_img);
				}

				if (canvas->refresh) {
					layer->refresh = 1;
					canvas->refresh++;
				}

				if (img) {

					if (conference->super_canvas_label_layers) {
						char str[80] = "";
						switch_image_t *tmp;
						const char *format = "#cccccc:#142e55:FreeSans.ttf:4%:";

						switch_snprintf(str, sizeof(str), "%sCanvas %d", format, jcanvas->canvas_id + 1);
						tmp = switch_img_write_text_img(img->d_w, img->d_h, SWITCH_TRUE, str);
						switch_img_patch(img, tmp, 0, 0);
						switch_img_free(&tmp);
					}

					switch_img_free(&layer->cur_img);
					layer->cur_img = img;
					img = NULL;
				}

				conference_video_scale_and_patch(layer, NULL, SWITCH_FALSE);
			}

			switch_img_free(&img);
		}

		if (canvas->refresh > 1) {
			canvas->refresh = 0;
		}

		write_img = canvas->img;
		timestamp = canvas->timer.samplecount;

		if (!write_img) continue;

		write_frame.img = write_img;
		conference_video_check_recording(conference, &write_frame);

		if (min_members && conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
			for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
				write_codecs[i]->frame.img = write_img;
				conference_video_write_canvas_image_to_codec_group(conference, canvas, write_codecs[i], i, timestamp, need_refresh, need_keyframe, need_reset);

				if (canvas->video_write_bandwidth) {
					switch_core_codec_control(&write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &canvas->video_write_bandwidth, NULL, NULL);
					canvas->video_write_bandwidth = 0;
				}
			}
		}

		switch_mutex_lock(conference->member_mutex);
		for (imember = conference->members; imember; imember = imember->next) {
			switch_frame_t *dupframe;

			if (imember->watching_canvas_id != canvas->canvas_id) continue;

			if (conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) && !conference_utils_member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				continue;
			}

			if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) ||
				switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}

			if (need_refresh) {
				switch_core_session_request_video_refresh(imember->session);
			}

			if (need_keyframe) {
				switch_core_media_gen_key_frame(imember->session);
			}

			switch_set_flag(&write_frame, SFF_RAW_RTP);
			write_frame.img = write_img;
			write_frame.packet = packet;
			write_frame.data = ((uint8_t *)packet) + 12;
			write_frame.datalen = 0;
			write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
			write_frame.packetlen = 0;

			//switch_core_session_write_video_frame(imember->session, &write_frame, SWITCH_IO_FLAG_NONE, 0);

			if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
				switch_queue_push(imember->mux_out_queue, dupframe);
				dupframe = NULL;
			}

			if (imember->session) {
				switch_core_session_rwunlock(imember->session);
			}
		}

		switch_mutex_unlock(conference->member_mutex);
	}

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		layer = &canvas->layers[i];

		switch_mutex_lock(canvas->mutex);
		switch_img_free(&layer->cur_img);
		switch_img_free(&layer->img);
		layer->banner_patched = 0;
		switch_img_free(&layer->banner_img);
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->logo_text_img);
		switch_img_free(&layer->mute_img);
		switch_mutex_unlock(canvas->mutex);

		if (layer->txthandle) {
			switch_img_txt_handle_destroy(&layer->txthandle);
		}
	}

	for (i = 0; i < MAX_MUX_CODECS; i++) {
		if (write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec)) {
			switch_core_codec_destroy(&write_codecs[i]->codec);
		}
	}

	switch_core_timer_destroy(&canvas->timer);
	conference_video_destroy_canvas(&canvas);

	return NULL;
}


void conference_video_find_floor(conference_member_t *member, switch_bool_t entering)
{
	conference_member_t *imember;
	conference_obj_t *conference = member->conference;

	if (!entering) {
		if (member->id == conference->video_floor_holder) {
			conference_video_set_floor_holder(conference, NULL, SWITCH_FALSE);
		} else if (member->id == conference->last_video_floor_holder) {
			conference->last_video_floor_holder = 0;
		}
	}

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {

		if (!(imember->session)) {
			continue;
		}

		if (imember->video_flow == SWITCH_MEDIA_FLOW_SENDONLY && !imember->avatar_png_img) {
			continue;
		}

		if (!switch_channel_test_flag(imember->channel, CF_VIDEO) && !imember->avatar_png_img) {
			continue;
		}

		if (!entering && imember->id == member->id) {
			continue;
		}

		if (conference->floor_holder && imember == conference->floor_holder) {
			conference_video_set_floor_holder(conference, imember, 0);
			continue;
		}

		if (!conference->video_floor_holder) {
			conference_video_set_floor_holder(conference, imember, 0);
			continue;
		}

		if (!conference->last_video_floor_holder) {
			conference->last_video_floor_holder = imember->id;
			switch_core_session_request_video_refresh(imember->session);
			continue;
		}

	}
	switch_mutex_unlock(conference->member_mutex);

	if (conference->last_video_floor_holder == conference->video_floor_holder) {
		conference->last_video_floor_holder = 0;
	}
}

void conference_video_reset_member_codec_index(conference_member_t *member)
{
	member->video_codec_index = -1;
}

void conference_video_set_floor_holder(conference_obj_t *conference, conference_member_t *member, switch_bool_t force)
{
	switch_event_t *event;
	conference_member_t *imember = NULL;
	int old_id = 0;
	uint32_t old_member = 0;

	if (!member) {
		conference_utils_clear_flag(conference, CFLAG_VID_FLOOR_LOCK);
	}

	if ((!force && conference_utils_test_flag(conference, CFLAG_VID_FLOOR_LOCK))) {
		return;
	}

	if (member && member->video_flow == SWITCH_MEDIA_FLOW_SENDONLY && !member->avatar_png_img) {
		return;
	}

	if (conference->video_floor_holder) {
		if (member && conference->video_floor_holder == member->id) {
			return;
		} else {
			if (member) {
				conference->last_video_floor_holder = conference->video_floor_holder;
			}

			if (conference->last_video_floor_holder && (imember = conference_member_get(conference, conference->last_video_floor_holder))) {
				switch_core_session_request_video_refresh(imember->session);

				if (conference_utils_member_test_flag(imember, MFLAG_VIDEO_BRIDGE)) {
					conference_utils_set_flag(conference, CFLAG_VID_FLOOR_LOCK);
				}
				switch_thread_rwlock_unlock(imember->rwlock);
				imember = NULL;
			}

			old_member = conference->video_floor_holder;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Dropping video floor %d\n", old_member);
		}
	}


	if (!member) {
		switch_mutex_lock(conference->member_mutex);
		for (imember = conference->members; imember; imember = imember->next) {
			if (imember->id != conference->video_floor_holder && imember->channel && switch_channel_test_flag(imember->channel, CF_VIDEO)) {
				member = imember;
				break;
			}
		}
		switch_mutex_unlock(conference->member_mutex);
	}

	//VIDFLOOR
	if (conference->canvas_count == 1 && member && conference->canvas && conference->canvas->layout_floor_id > -1) {
		conference_video_attach_video_layer(member, conference->canvas, conference->canvas->layout_floor_id);
	}

	if (member) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Adding video floor %s\n",
						  switch_channel_get_name(member->channel));

		conference_video_check_flush(member);
		switch_core_session_video_reinit(member->session);
		conference->video_floor_holder = member->id;
		conference_member_update_status_field(member);
	} else {
		conference->video_floor_holder = 0;
	}

	if (old_member) {
		conference_member_t *old_member_p = NULL;

		old_id = old_member;

		if ((old_member_p = conference_member_get(conference, old_id))) {
			conference_member_update_status_field(old_member_p);
			switch_thread_rwlock_unlock(old_member_p->rwlock);
		}
	}

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		if (!imember->channel || !switch_channel_test_flag(imember->channel, CF_VIDEO)) {
			continue;
		}

		switch_channel_set_flag(imember->channel, CF_VIDEO_BREAK);
		switch_core_session_kill_channel(imember->session, SWITCH_SIG_BREAK);
		switch_core_session_video_reinit(imember->session);
	}
	switch_mutex_unlock(conference->member_mutex);

	conference_utils_set_flag(conference, CFLAG_FLOOR_CHANGE);

	if (test_eflag(conference, EFLAG_FLOOR_CHANGE)) {
		switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
		conference_event_add_data(conference, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "video-floor-change");
		if (old_id) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Old-ID", "%d", old_id);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Old-ID", "none");
		}
		if (conference->video_floor_holder) {
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "New-ID", "%d", conference->video_floor_holder);
		} else {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "New-ID", "none");
		}
		switch_event_fire(&event);
	}

}


void conference_video_write_frame(conference_obj_t *conference, conference_member_t *floor_holder, switch_frame_t *vid_frame)
{
	conference_member_t *imember;
	int want_refresh = 0;
	unsigned char buf[SWITCH_RTP_MAX_BUF_LEN] = "";
	switch_frame_t tmp_frame = { 0 };

	if (switch_test_flag(vid_frame, SFF_CNG) || !vid_frame->packet) {
		return;
	}

	if (conference_utils_test_flag(conference, CFLAG_FLOOR_CHANGE)) {
		conference_utils_clear_flag(conference, CFLAG_FLOOR_CHANGE);
	}

	if (vid_frame->img && conference->canvas) {
		switch_image_t *frame_img = NULL, *tmp_img = NULL;
		int x,y;

		switch_img_copy(vid_frame->img, &tmp_img);
		switch_img_fit(&tmp_img, conference->canvas->width, conference->canvas->height);
		frame_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, conference->canvas->width, conference->canvas->height, 1);
		conference_video_reset_image(frame_img, &conference->canvas->bgcolor);
		switch_img_find_position(POS_CENTER_MID, frame_img->d_w, frame_img->d_h, tmp_img->d_w, tmp_img->d_h, &x, &y);
		switch_img_patch(frame_img, tmp_img, x, y);
		tmp_frame.packet = buf;
		tmp_frame.data = buf + 12;
		tmp_frame.img = frame_img;
		switch_img_free(&tmp_img);
	}


	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		switch_core_session_t *isession = imember->session;

		if (!isession || switch_core_session_read_lock(isession) != SWITCH_STATUS_SUCCESS) {
			continue;
		}

		if (switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
			want_refresh++;
			switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
		}

		if (isession && switch_channel_test_flag(imember->channel, CF_VIDEO)) {
			int send_frame = 0;

			if (conference->canvas && conference_utils_test_flag(imember->conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
				if (switch_channel_test_flag(imember->channel, CF_VIDEO) && (conference->members_with_video == 1 || imember != floor_holder)) {
					send_frame = 1;
				}
			} else if (!conference_utils_member_test_flag(imember, MFLAG_RECEIVING_VIDEO) &&
					   (conference_utils_test_flag(conference, CFLAG_VID_FLOOR_LOCK) ||
						!(imember->id == imember->conference->video_floor_holder && imember->conference->last_video_floor_holder))) {
				send_frame = 1;
			}

			if (send_frame) {
				if (vid_frame->img) {
					if (conference->canvas) {
						tmp_frame.packet = buf;
						tmp_frame.packetlen = sizeof(buf) - 12;
						tmp_frame.data = buf + 12;
						switch_core_session_write_video_frame(imember->session, &tmp_frame, SWITCH_IO_FLAG_NONE, 0);
					} else {
						switch_core_session_write_video_frame(imember->session, vid_frame, SWITCH_IO_FLAG_NONE, 0);
					}
				} else {
					switch_assert(vid_frame->packetlen <= SWITCH_RTP_MAX_BUF_LEN);
					tmp_frame = *vid_frame;
					tmp_frame.packet = buf;
					tmp_frame.data = buf + 12;
					memcpy(tmp_frame.packet, vid_frame->packet, vid_frame->packetlen);
					tmp_frame.packetlen = vid_frame->packetlen;
					tmp_frame.datalen = vid_frame->datalen;
					switch_core_session_write_video_frame(imember->session, &tmp_frame, SWITCH_IO_FLAG_NONE, 0);
				}
			}
		}

		switch_core_session_rwunlock(isession);
	}
	switch_mutex_unlock(conference->member_mutex);

	switch_img_free(&tmp_frame.img);

	if (want_refresh && floor_holder->session) {
		switch_core_session_request_video_refresh(floor_holder->session);
	}
}

switch_status_t conference_video_thread_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data)
{
	//switch_channel_t *channel = switch_core_session_get_channel(session);
	//char *name = switch_channel_get_name(channel);
	conference_member_t *member = (conference_member_t *)user_data;
	conference_relationship_t *rel = NULL, *last = NULL;

	switch_assert(member);

	if (switch_test_flag(frame, SFF_CNG) || !frame->packet) {
		return SWITCH_STATUS_SUCCESS;
	}


	if (switch_thread_rwlock_tryrdlock(member->conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}


	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
		if (member->conference->members_with_video < 3) {
			conference_video_write_frame(member->conference, member, frame);
			conference_video_check_recording(member->conference, frame);
			switch_thread_rwlock_unlock(member->conference->rwlock);
			return SWITCH_STATUS_SUCCESS;
		}
	}


	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_MUXING)) {
		switch_image_t *img_copy = NULL;

		if (frame->img && (member->video_layer_id > -1 || member->canvas) && conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) &&
			!member->conference->playing_video_file && switch_queue_size(member->video_queue) < member->conference->video_fps.fps) {
			switch_img_copy(frame->img, &img_copy);
			switch_queue_push(member->video_queue, img_copy);
		}

		switch_thread_rwlock_unlock(member->conference->rwlock);
		return SWITCH_STATUS_SUCCESS;
	}

	for (rel = member->relationships; rel; rel = rel->next) {
		conference_member_t *imember;
		if (!(rel->flags & RFLAG_CAN_SEND_VIDEO)) continue;

		if ((imember = conference_member_get(member->conference, rel->id)) && conference_utils_member_test_flag(imember, MFLAG_RECEIVING_VIDEO)) {
			//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s %d->%d %d\n", name, member->id, imember->id, frame->datalen);
			switch_core_session_write_video_frame(imember->session, frame, SWITCH_IO_FLAG_NONE, 0);
			switch_thread_rwlock_unlock(imember->rwlock);
		} else { /* Stale .. Remove */
			if (last) {
				last->next = rel->next;
			} else {
				member->relationships = rel->next;
			}

			switch_mutex_lock(member->conference->member_mutex);
			member->conference->relationship_total--;
			switch_mutex_unlock(member->conference->member_mutex);

			continue;
		}

		last = rel;
	}


	if (member) {
		if (member->id == member->conference->video_floor_holder) {
			conference_video_write_frame(member->conference, member, frame);
			conference_video_check_recording(member->conference, frame);
		} else if (!conference_utils_test_flag(member->conference, CFLAG_VID_FLOOR_LOCK) && member->id == member->conference->last_video_floor_holder) {
			conference_member_t *fmember;

			if ((fmember = conference_member_get(member->conference, member->conference->video_floor_holder))) {
				switch_core_session_write_video_frame(fmember->session, frame, SWITCH_IO_FLAG_NONE, 0);
				switch_thread_rwlock_unlock(fmember->rwlock);
			}
		}
	}

	switch_thread_rwlock_unlock(member->conference->rwlock);

	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

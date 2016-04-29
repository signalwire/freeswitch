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
	uint32_t i = 0, j = 0;

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
				const char *val = NULL, *name = NULL, *bgimg = NULL;
				switch_bool_t auto_3d = SWITCH_FALSE;
				int border = 0;
				
				if ((val = switch_xml_attr(x_layout, "name"))) {
					name = val;
				}

				if (!name) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid layout\n");
					continue;
				}

				auto_3d = switch_true(switch_xml_attr(x_layout, "auto-3d-position"));

				bgimg = switch_xml_attr(x_layout, "bgimg");
				
				if ((val = switch_xml_attr(x_layout, "border"))) {
					border = atoi(val);
					if (border < 0) border = 0;
					if (border > 50) border = 50;
				}

				vlayout = switch_core_alloc(conference->pool, sizeof(*vlayout));
				vlayout->name = switch_core_strdup(conference->pool, name);

				if (bgimg) {
					vlayout->bgimg = switch_core_strdup(conference->pool, bgimg);
				}

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
					
					if ((val = switch_xml_attr(x_image, "border"))) {
						border = atoi(val);

						if (border < 0) border = 0;
						if (border > 50) border = 50;
					}

					if (x < 0 || y < 0 || scale < 0 || !name) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid image\n");
						continue;
					}

					if (hscale == -1) {
						hscale = scale;
					}
					
					if (!border) border = conference->video_border_size;
					
					vlayout->images[vlayout->layers].border = border;
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
							int x_pos = (int)(WIDTH * x / VIDEO_LAYOUT_SCALE);
							int y_pos = (int)(HEIGHT * y / VIDEO_LAYOUT_SCALE);
							int width = (int)(WIDTH * scale / VIDEO_LAYOUT_SCALE);
							int height = (int)(HEIGHT * hscale / VIDEO_LAYOUT_SCALE);
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
								yv = -1.0f - ((center_y - half_y) / half_y) * -1.0f;
							} else {
								yv = (float)center_y / half_y;
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
	switch_img_free(&layer->banner_img);
	switch_img_free(&layer->logo_img);
	switch_img_free(&layer->logo_text_img);

	layer->bugged = 0;
	layer->mute_patched = 0;
	layer->banner_patched = 0;
	layer->is_avatar = 0;
	layer->need_patch = 0;

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

	if (layer->clear) {
		conference_video_clear_layer(layer);
		layer->clear = 0;
	}

	if (layer->refresh) {
		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->letterbox_bgcolor);
		layer->refresh = 0;
	}

	if (layer->geometry.scale) {
		uint32_t img_w = 0, img_h = 0;
		double screen_aspect = 0, img_aspect = 0;
		int x_pos = layer->x_pos;
		int y_pos = layer->y_pos;
		switch_size_t img_addr = 0;

		img_w = layer->screen_w = (uint32_t)(IMG->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE);
		img_h = layer->screen_h = (uint32_t)(IMG->d_h * layer->geometry.hscale / VIDEO_LAYOUT_SCALE);


		screen_aspect = (double) layer->screen_w / layer->screen_h;
		img_aspect = (double) img->d_w / img->d_h;

		img_addr = (switch_size_t)img;

		if (layer->last_img_addr != img_addr && layer->geometry.zoom) {
			if (screen_aspect < img_aspect) {
				unsigned int cropsize = 0;
				double scale = 1;
				if (img->d_h != layer->screen_h) {
					scale = (double)layer->screen_h / img->d_h;
				}

				cropsize = (unsigned int)(((img->d_w )-((double)layer->screen_w/scale)) / 2);

				if (cropsize) {
					switch_img_set_rect(img, cropsize, 0, (unsigned int)(layer->screen_w/scale), (unsigned int)(layer->screen_h/scale));
					img_aspect = (double) img->d_w / img->d_h;
				}

			} else if (screen_aspect > img_aspect) {
				unsigned int cropsize = 0;
				double scale = 1;
				if (img->d_w != layer->screen_w) {
					scale = (double)layer->screen_w / img->d_w;
				}
				cropsize = (int)ceil(((img->d_h )-((double)layer->screen_h/scale)) / 2);
				if (cropsize) {
					switch_img_set_rect(img, 0, cropsize, (unsigned int)(layer->screen_w/scale), (unsigned int)(layer->screen_h/scale));
					img_aspect = (double) img->d_w / img->d_h;
				}
			}
		}

		if (freeze) {
			switch_img_free(&layer->img);
		}

		if (screen_aspect > img_aspect) {
			img_w = (uint32_t)ceil((double)img_aspect * layer->screen_h);
			x_pos += (layer->screen_w - img_w) / 2;
		} else if (screen_aspect < img_aspect) {
			img_h = (uint32_t)ceil((double)layer->screen_w / img_aspect);
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
			switch_img_fill(layer->canvas->img, layer->x_pos + layer->geometry.border, layer->y_pos + layer->geometry.border, layer->screen_w, layer->screen_h, &layer->canvas->letterbox_bgcolor);
			switch_img_fit(&layer->banner_img, layer->screen_w, layer->screen_h, SWITCH_FIT_SIZE);
			switch_img_patch(IMG, layer->banner_img, layer->x_pos + layer->geometry.border, layer->y_pos + (layer->screen_h - layer->banner_img->d_h) + layer->geometry.border);

			if (!freeze) {
				switch_img_set_rect(layer->img, 0, 0, layer->img->d_w, layer->img->d_h - layer->banner_img->d_h);
			}

			layer->banner_patched = 1;
		}

		switch_assert(layer->img);

		if (layer->geometry.border) {
			switch_img_fill(IMG, x_pos, y_pos, img_w, img_h, &layer->canvas->border_color);
		}
		
		img_w -= (layer->geometry.border * 2);
		img_h -= (layer->geometry.border * 2);

		switch_img_scale(img, &layer->img, img_w, img_h);

		if (layer->img) {
			if (layer->bugged) {
				if (layer->member_id > -1 && layer->member && switch_thread_rwlock_tryrdlock(layer->member->rwlock) == SWITCH_STATUS_SUCCESS) {
					switch_frame_t write_frame = { 0 };
					write_frame.img = layer->img;
				
					switch_core_media_bug_patch_video(layer->member->session, &write_frame);
					switch_thread_rwlock_unlock(layer->member->rwlock);
				}

				layer->bugged = 0;
			}
			
			switch_img_patch(IMG, layer->img, x_pos + layer->geometry.border, y_pos + layer->geometry.border);
		}


		if (layer->logo_img) {
			int ew = layer->screen_w - (layer->geometry.border * 2), eh = layer->screen_h - (layer->banner_img ? layer->banner_img->d_h : 0) - (layer->geometry.border * 2);
			int ex = 0, ey = 0;

			switch_img_fit(&layer->logo_img, ew, eh, layer->logo_fit);

			switch_img_find_position(layer->logo_pos, ew, eh, layer->logo_img->d_w, layer->logo_img->d_h, &ex, &ey);

			switch_img_patch(IMG, layer->logo_img, layer->x_pos + ex + layer->geometry.border, layer->y_pos + ey + layer->geometry.border);
			if (layer->logo_text_img) {
				int tx = 0, ty = 0;

				switch_img_fit(&layer->logo_text_img, (ew / 2) + 1, (eh / 2) + 1, SWITCH_FIT_SIZE);
				switch_img_find_position(POS_LEFT_BOT,
										 layer->logo_img->d_w, layer->logo_img->d_h, layer->logo_text_img->d_w, layer->logo_text_img->d_h, &tx, &ty);
				switch_img_patch(IMG, layer->logo_text_img, layer->x_pos + ex + tx + layer->geometry.border, layer->y_pos + ey + ty + layer->geometry.border);
			}

		}
		
		layer->last_img_addr = img_addr;

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

void conference_video_set_canvas_border_color(mcu_canvas_t *canvas, char *color)
{
	switch_color_set_rgb(&canvas->border_color, color);
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

mcu_layer_t *conference_video_get_layer_locked(conference_member_t *member)
{
	mcu_layer_t *layer = NULL;
	mcu_canvas_t *canvas = NULL;

	if ((canvas = conference_video_get_canvas_locked(member))) {
		switch_mutex_lock(canvas->mutex);

		if (member->video_layer_id > -1) {
			layer = &canvas->layers[member->video_layer_id];
		}

		if (!layer) {
			switch_mutex_unlock(canvas->mutex);
			conference_video_release_canvas(&canvas);
		}
	}
	
	return layer;
}

void conference_video_release_layer(mcu_layer_t **layer)
{
	mcu_canvas_t *canvas = NULL;

	if (!layer || !*layer) return;

	canvas = (*layer)->canvas;

	if (!canvas) return;

	switch_mutex_unlock(canvas->mutex);
	conference_video_release_canvas(&canvas);
	
	*layer = NULL;
}

mcu_canvas_t *conference_video_get_canvas_locked(conference_member_t *member)
{
	mcu_canvas_t *canvas = NULL;

	switch_mutex_lock(member->conference->canvas_mutex);

	if (member->canvas_id > -1 && member->video_layer_id > -1) {
		canvas = member->conference->canvases[member->canvas_id];
	}

	if (!canvas) {
	   	switch_mutex_unlock(member->conference->canvas_mutex);
	}

	return canvas;
}

void conference_video_release_canvas(mcu_canvas_t **canvasP)
{
	mcu_canvas_t *canvas = NULL;

	switch_assert(canvasP);
	
	canvas = *canvasP;

	if (!canvas) return;

	switch_mutex_unlock(canvas->conference->canvas_mutex);
	*canvasP = NULL;
}

void conference_video_clear_managed_kps(conference_member_t *member)
{
	member->managed_kps_set = 0;
	member->auto_kps_debounce_ticks = 0;
	member->layer_loops = 0;
}

void conference_video_detach_video_layer(conference_member_t *member)
{
	mcu_layer_t *layer = NULL;
	mcu_canvas_t *canvas = NULL;

	if (member->canvas_id < 0) return;
	
	if (!(canvas = conference_video_get_canvas_locked(member))) {
		return;
	}

	switch_mutex_lock(canvas->mutex);

	if (member->video_layer_id < 0) {
		switch_mutex_unlock(canvas->mutex);
		return;
	}
	
	layer = &canvas->layers[member->video_layer_id];

	if (layer->geometry.audio_position) {
		conference_api_sub_position(member, NULL, "0:0:0");
	}

	if (layer->txthandle) {
		switch_img_txt_handle_destroy(&layer->txthandle);
	}

	conference_video_reset_layer(layer);
	layer->member_id = 0;
	layer->member = NULL;
	member->video_layer_id = -1;
	member->layer_timeout = DEFAULT_LAYER_TIMEOUT;

	//member->canvas_id = 0;
	//member->watching_canvas_id = -1;
	member->avatar_patched = 0;
	conference_video_check_used_layers(canvas);
	canvas->send_keyframe = 1;
	conference_video_clear_managed_kps(member);

	if (conference_utils_test_flag(member->conference, CFLAG_JSON_STATUS)) {
		conference_member_update_status_field(member);
	}
	
	if (canvas->bgimg) {
		conference_video_set_canvas_bgimg(canvas, NULL);
	}

	switch_mutex_unlock(canvas->mutex);
	conference_video_release_canvas(&canvas);
	
}


void conference_video_layer_set_logo(conference_member_t *member, mcu_layer_t *layer, const char *path)
{
	const char *var = NULL;
	char *dup = NULL;
	switch_event_t *params = NULL;
	char *parsed = NULL;
	char *tmp;
	switch_img_position_t pos = POS_LEFT_TOP;
	switch_img_fit_t fit = SWITCH_FIT_SIZE;

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
		member->video_logo = NULL;
		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h,
						&layer->canvas->letterbox_bgcolor);

		goto end;
	}

	if ((tmp = strchr(path, '}'))) {
		path = tmp + 1;
	}

	if (params) {
		if ((var = switch_event_get_header(params, "position"))) {
			pos = parse_img_position(var);
		}

		if ((var = switch_event_get_header(params, "fit"))) {
			fit = parse_img_fit(var);
		}
	}

	if (path && strcasecmp(path, "clear")) {
		layer->logo_img = switch_img_read_png(path, SWITCH_IMG_FMT_ARGB);
	}

	if (layer->logo_img) {
		layer->logo_pos = pos;
		layer->logo_fit = fit;

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
	uint16_t font_size = 0;
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

	if (zstr(text) || !strcasecmp(text, "clear") || !strcasecmp(text, "allclear")) {
		switch_img_free(&layer->banner_img);
		layer->banner_patched = 0;

		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h,
						&layer->canvas->letterbox_bgcolor);

		if (zstr(text) || !strcasecmp(text, "allclear")) {
			switch_channel_set_variable(member->channel, "video_banner_text", NULL);
		}

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
		font_size = (uint16_t)((double)(font_scale / 100.0f) * layer->screen_h);
	} else {
		font_size = (uint16_t)((double)(font_scale / 100.0f) * layer->screen_w);
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

		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h,
						&layer->canvas->letterbox_bgcolor);

		goto end;
	}

	switch_img_free(&layer->banner_img);
	//switch_img_free(&layer->logo_img);
	//switch_img_free(&layer->logo_text_img);
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
	member->blackouts = 0;
	member->good_img = 0;
	member->blanks = 0;
	member->layer_loops = 0;
}

switch_status_t conference_video_attach_video_layer(conference_member_t *member, mcu_canvas_t *canvas, int idx)
{
	mcu_layer_t *layer = NULL;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	const char *var = NULL;

	if (!member->session) abort();

	channel = switch_core_session_get_channel(member->session);

	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS) &&
		!conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		return SWITCH_STATUS_FALSE;
	}


	if (!switch_channel_test_flag(channel, CF_VIDEO_READY) && !member->avatar_png_img) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY && !member->avatar_png_img) {
		return SWITCH_STATUS_FALSE;
	}

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

	if (layer->member_id && layer->member_id == (int)member->id) {
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
	layer->member = member;
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
	conference_video_clear_managed_kps(member);

	if (conference_utils_test_flag(member->conference, CFLAG_JSON_STATUS)) {
		conference_member_update_status_field(member);
	}

 end:

	switch_mutex_unlock(canvas->mutex);

	return status;
}

void conference_video_init_canvas_layers(conference_obj_t *conference, mcu_canvas_t *canvas, video_layout_t *vlayout)
{
	int i = 0;

	if (!canvas) return;

	switch_thread_rwlock_wrlock(canvas->video_rwlock);
	switch_mutex_lock(canvas->mutex);
	canvas->layout_floor_id = -1;

	if (!vlayout) {
		vlayout = canvas->new_vlayout;
		canvas->new_vlayout = NULL;
	}

	if (!vlayout) {
		switch_mutex_unlock(canvas->mutex);
		switch_thread_rwlock_unlock(canvas->video_rwlock);
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
		layer->geometry.border = vlayout->images[i].border;
		layer->geometry.floor = vlayout->images[i].floor;
		layer->geometry.overlap = vlayout->images[i].overlap;
		layer->idx = i;
		layer->refresh = 1;

																		
		layer->screen_w = (uint32_t)(canvas->img->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE);
		layer->screen_h = (uint32_t)(canvas->img->d_h * layer->geometry.hscale / VIDEO_LAYOUT_SCALE);

		// if (layer->screen_w % 2) layer->screen_w++; // round to even
		// if (layer->screen_h % 2) layer->screen_h++; // round to even

		layer->x_pos = (int)(canvas->img->d_w * layer->geometry.x / VIDEO_LAYOUT_SCALE);
		layer->y_pos = (int)(canvas->img->d_h * layer->geometry.y / VIDEO_LAYOUT_SCALE);


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
		if (layer->member) {
			//conference_video_detach_video_layer(layer->member);
			conference_video_clear_managed_kps(layer->member);
			layer->member->video_layer_id = -1;
			layer->member = NULL;
		}
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

	if (vlayout->bgimg) {
		conference_video_set_canvas_bgimg(canvas, vlayout->bgimg);
	} else if (canvas->bgimg) {
		switch_img_free(&canvas->bgimg);
	}

	if (conference->video_canvas_bgimg && !vlayout->bgimg) {
		conference_video_set_canvas_bgimg(canvas, conference->video_canvas_bgimg);
	}
	
	switch_mutex_unlock(canvas->mutex);
	switch_thread_rwlock_unlock(canvas->video_rwlock);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Canvas position %d applied layout %s\n", canvas->canvas_id + 1, vlayout->name);

}

switch_status_t conference_video_set_canvas_bgimg(mcu_canvas_t *canvas, const char *img_path)
{

	int x = 0, y = 0, scaled = 0;

	if (img_path) {
		switch_img_free(&canvas->bgimg);
		canvas->bgimg = switch_img_read_png(img_path, SWITCH_IMG_FMT_I420);
	} else {
		scaled = 1;
	}
	
	if (!canvas->bgimg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open image for bgimg\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!scaled) {
		switch_img_fit(&canvas->bgimg, canvas->img->d_w, canvas->img->d_h, SWITCH_FIT_SIZE);
	}
	switch_img_find_position(POS_CENTER_MID, canvas->img->d_w, canvas->img->d_h, canvas->bgimg->d_w, canvas->bgimg->d_h, &x, &y);
	switch_img_patch(canvas->img, canvas->bgimg, x, y);

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_video_attach_canvas(conference_obj_t *conference, mcu_canvas_t *canvas, int super)
{
	if (conference->canvas_count >= MAX_CANVASES + 1) {
		return SWITCH_STATUS_FALSE;
	}

	canvas->canvas_id = conference->canvas_count;

	if (!super) {
		conference->canvas_count++;
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
	switch_thread_rwlock_create(&canvas->video_rwlock, canvas->pool);

	switch_assert(canvas->img);

	switch_mutex_lock(canvas->mutex);
	conference_video_set_canvas_bgcolor(canvas, conference->video_canvas_bgcolor);
	conference_video_set_canvas_letterbox_bgcolor(canvas, conference->video_letterbox_bgcolor);
	conference_video_set_canvas_border_color(canvas, conference->video_border_color);
	conference_video_init_canvas_layers(conference, canvas, vlayout);
	switch_mutex_unlock(canvas->mutex);

	canvas->canvas_id = -1;
	*canvasP = canvas;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Layout set to %s\n", vlayout->name);

	return SWITCH_STATUS_SUCCESS;
}

int conference_video_flush_queue(switch_queue_t *q, int min)
{
	switch_image_t *img;
	void *pop;
	int r = 0;

	if (!q) return 0;

	while (switch_queue_size(q) > min && switch_queue_trypop(q, &pop) == SWITCH_STATUS_SUCCESS && pop) {
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
	switch_img_free(&canvas->bgimg);
	conference_video_flush_queue(canvas->video_queue, 0);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		switch_img_free(&canvas->layers[i].img);
	}

	*canvasP = NULL;
}

void conference_video_write_canvas_image_to_codec_group(conference_obj_t *conference, mcu_canvas_t *canvas, codec_set_t *codec_set,
														int codec_index, uint32_t timestamp, switch_bool_t need_refresh,
														switch_bool_t send_keyframe, switch_bool_t need_reset)

{
	conference_member_t *imember;
	switch_frame_t write_frame = { 0 }, *frame = NULL;
	switch_status_t encode_status = SWITCH_STATUS_FALSE;
	switch_image_t *scaled_img = codec_set->scaled_img;

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
		switch_core_codec_control(&codec_set->codec, SCC_VIDEO_RESET, SCCT_INT, (void *)&type, SCCT_NONE, NULL, NULL, NULL);
		need_refresh = SWITCH_TRUE;
	}

	if (send_keyframe) {
		switch_core_codec_control(&codec_set->codec, SCC_VIDEO_GEN_KEYFRAME, SCCT_NONE, NULL, SCCT_NONE, NULL, NULL, NULL);
	}

	if (scaled_img) {
		if (!send_keyframe && codec_set->fps_divisor > 1 && (codec_set->frame_count++) % codec_set->fps_divisor) {
			// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Skip one frame, total: %d\n", codec_set->frame_count);
			return;
		}

		switch_img_scale(frame->img, &scaled_img, scaled_img->d_w, scaled_img->d_h);
		frame->img = scaled_img;
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

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY) {
					switch_core_session_rwunlock(imember->session);
					continue;
				}

				//switch_core_session_write_encoded_video_frame(imember->session, frame, 0, 0);
				switch_set_flag(frame, SFF_ENCODED);

				if (switch_frame_buffer_dup(imember->fb, frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					if (switch_queue_trypush(imember->mux_out_queue, dupframe) != SWITCH_STATUS_SUCCESS) {
						switch_frame_buffer_free(imember->fb, &dupframe);
					}
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

	if (!count) {
		count = conference->members_with_video;

		if (!conference_utils_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
			count += conference->members_with_avatar;
		}
	}

	for (vlnode = lg->layouts; vlnode; vlnode = vlnode->next) {
		if (vlnode->vlayout->layers >= (int)count) {
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
		
		if ((canvas = conference_video_get_canvas_locked(member))) {
			
			switch_mutex_lock(canvas->mutex);
			layer = &canvas->layers[member->video_layer_id];
			switch_img_free(&layer->mute_img);
			switch_img_free(&member->video_mute_img);

			if (!clear && layer->cur_img) {
				switch_img_copy(layer->cur_img, &member->video_mute_img);
				switch_img_copy(layer->cur_img, &layer->mute_img);
			}

			switch_mutex_unlock(canvas->mutex);
			conference_video_release_canvas(&canvas);
		}
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
			
			if (xlayer->fnode && xlayer->fnode != fnode) {
				idx = -1;
			}
		}

		if (idx < 0) {
			for (i = 0; i < canvas->total_layers; i++) {
				xlayer = &canvas->layers[i];

				if (xlayer->fnode || (xlayer->geometry.res_id && (!fnode->res_id || strcmp(xlayer->geometry.res_id, fnode->res_id))) || xlayer->member_id) {
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
		switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
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
	switch_frame_t *frame;
	int loops = 0;
	switch_time_t last = 0;

	if (switch_thread_rwlock_tryrdlock(member->rwlock) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	//switch_core_autobind_cpu();

	while(conference_utils_member_test_flag(member, MFLAG_RUNNING)) {
		if (switch_queue_pop(member->mux_out_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			mcu_layer_t *layer = NULL;
			mcu_canvas_t *canvas = NULL;

			if (!pop) {
				break;
			}
				
			if (loops == 0 || loops == 50) {
				switch_core_media_gen_key_frame(member->session);
				switch_core_session_request_video_refresh(member->session);
			}

			loops++;
			
			if ((switch_size_t)pop != 1) {
				frame = (switch_frame_t *) pop;
				if (switch_test_flag(frame, SFF_ENCODED)) {
					switch_core_session_write_encoded_video_frame(member->session, frame, 0, 0);
				} else {
					switch_core_session_write_video_frame(member->session, frame, SWITCH_IO_FLAG_NONE, 0);
				}

				if (!switch_test_flag(frame, SFF_ENCODED) || frame->m) {
					switch_time_t now = switch_time_now();
					
					if (last) {
						int delta = (int)(now - last);
						if (delta > member->conference->video_fps.ms * 5000) {
							switch_core_session_request_video_refresh(member->session);							
						}
					}

					last = now;

					
				}

				switch_frame_buffer_free(member->fb, &frame);
			}

			canvas = NULL;                                                                                                                              
			layer = NULL;
			
			switch_mutex_lock(member->conference->canvas_mutex);
			if (member->video_layer_id > -1 && member->canvas_id > -1) {
				canvas = member->conference->canvases[member->canvas_id];
				layer = &canvas->layers[member->video_layer_id];

				if (layer->need_patch && switch_thread_rwlock_tryrdlock(canvas->video_rwlock) == SWITCH_STATUS_SUCCESS) {
					if (layer->need_patch) {
						conference_video_scale_and_patch(layer, NULL, SWITCH_FALSE);
						layer->need_patch = 0;
					}
					switch_thread_rwlock_unlock(canvas->video_rwlock);
				}
			}
			switch_mutex_unlock(member->conference->canvas_mutex);
			
			
		}
	}

	while (switch_queue_trypop(member->mux_out_queue, &pop) == SWITCH_STATUS_SUCCESS) {
		if (pop) {
			if ((switch_size_t)pop != 1) {
				frame = (switch_frame_t *) pop;
				switch_frame_buffer_free(member->fb, &frame);
			}
		}
	}

	switch_thread_rwlock_unlock(member->rwlock);

	return NULL;
}

void conference_video_check_recording(conference_obj_t *conference, mcu_canvas_t *canvas, switch_frame_t *frame)
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

		if (canvas && imember->canvas_id != canvas->canvas_id) {
			continue;
		}

		if (switch_test_flag((&imember->rec->fh), SWITCH_FILE_OPEN) && switch_core_file_has_video(&imember->rec->fh, SWITCH_TRUE)) {
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

	if (conference_utils_member_test_flag(member, MFLAG_SECOND_SCREEN)) {
		return;
	}

	canvas = conference_video_get_canvas_locked(member);

	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS) &&
		(!switch_channel_test_flag(member->channel, CF_VIDEO_READY) || switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY)) {
		if (canvas) {
			conference_video_release_canvas(&canvas);
		}
		return;
	}

	if (canvas) {
		switch_mutex_lock(canvas->mutex);
	}

	member->avatar_patched = 0;

	if (!force && switch_channel_test_flag(member->channel, CF_VIDEO_READY) && switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY) {
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
		conference_video_release_canvas(&canvas);
	}
}

void conference_video_check_flush(conference_member_t *member)
{
	int flushed;

	if (!member->channel || !switch_channel_test_flag(member->channel, CF_VIDEO_READY)) {
		return;
	}

	flushed = conference_video_flush_queue(member->video_queue, 1);

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
				fnode->canvas_id = canvas->canvas_id;
			}
		}
	}
}

void conference_video_fnode_check(conference_file_node_t *fnode, int canvas_id) {
	mcu_canvas_t *canvas = NULL;
	
	if (switch_core_file_has_video(&fnode->fh, SWITCH_TRUE) && switch_core_file_read_video(&fnode->fh, NULL, SVR_CHECK) == SWITCH_STATUS_BREAK) {
		int full_screen = 0;
		char *res_id = NULL;

		if (fnode->canvas_id == -1) {
			if (canvas_id == -1) {
				return;
			}
			fnode->canvas_id = canvas_id;
		}
		
		canvas = fnode->conference->canvases[fnode->canvas_id];
		if (fnode->fh.params && fnode->conference->canvas_count == 1) {
			full_screen = switch_true(switch_event_get_header(fnode->fh.params, "full-screen"));
		}

		if (fnode->fh.params) {
			if ((res_id = switch_event_get_header(fnode->fh.params, "reservation_id"))) {
				fnode->res_id = switch_core_strdup(fnode->pool, res_id);
			}
		}

		if (full_screen) {
			canvas->play_file = 1;
			canvas->conference->playing_video_file = 1;
		} else {
			conference_video_canvas_set_fnode_layer(canvas, fnode, -1);

			if (fnode->layer_id == -1) {
				switch_frame_t file_frame = { 0 };

				switch_core_file_read_video(&fnode->fh, &file_frame, SVR_FLUSH);
				switch_img_free(&file_frame.img);
			}
		}
	}
}


switch_status_t conference_video_find_layer(conference_obj_t *conference, mcu_canvas_t *canvas, conference_member_t *member, mcu_layer_t **layerP)
{
	uint32_t avatar_layers = 0;
	mcu_layer_t *layer = NULL;
	int i;

	if (conference_utils_test_flag(conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS) &&
		!conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(canvas->mutex);

	for (i = 0; i < canvas->total_layers; i++) {
		mcu_layer_t *xlayer = &canvas->layers[i];

		if (xlayer->is_avatar && xlayer->member_id != (int)conference->video_floor_holder) {
			avatar_layers++;
		}
	}

	if (!layer &&
		(canvas->layers_used < canvas->total_layers ||
		 (avatar_layers && !member->avatar_png_img) || conference_utils_member_test_flag(member, MFLAG_MOD)) &&
		(member->avatar_png_img || switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY)) {
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
											   (conference->canvas_count > 1 || xlayer->member_id != (int)conference->video_floor_holder))) &&
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

	switch_mutex_unlock(canvas->mutex);

	if (layer) {
		*layerP = layer;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

void conference_video_next_canvas(conference_member_t *imember)
{
	if (imember->canvas_id == (int)imember->conference->canvas_count - 1) {
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

	if (!member->avatar_png_img && switch_channel_test_flag(member->channel, CF_VIDEO_READY)) {
		do {
			if (switch_queue_trypop(member->video_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
				switch_img_free(&img);
				img = (switch_image_t *)pop;
				member->blanks = 0;
			} else {
				break;
			}
			size = switch_queue_size(member->video_queue);
		} while(size > 0);

		if (conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) && member->video_layer_id > -1 && switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY) {
			if (img) {
				member->good_img++;
				if ((member->good_img % (int)(member->conference->video_fps.fps * 10)) == 0) {
					conference_video_reset_video_bitrate_counters(member);
				}
			} else {
				member->blanks++;
				member->good_img = 0;

				if (member->blanks == member->conference->video_fps.fps || (member->blanks % (int)(member->conference->video_fps.fps * 10)) == 0) {
					switch_core_session_request_video_refresh(member->session);
				}

				if (member->blanks == member->conference->video_fps.fps * 5) {
					member->blackouts++;
					conference_video_check_avatar(member, SWITCH_TRUE);
					conference_video_clear_managed_kps(member);

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

void conference_video_set_incoming_bitrate(conference_member_t *member, int kps, switch_bool_t force)
{
	switch_core_session_message_t msg = { 0 };

	if (switch_channel_test_flag(member->channel, CF_VIDEO_BITRATE_UNMANAGABLE)) {
		return;
	}

	if (kps >= member->managed_kps) {
		member->auto_kps_debounce_ticks = 0;
	}

	if (!force && kps < member->managed_kps && member->conference->auto_kps_debounce) {
		member->auto_kps_debounce_ticks = member->conference->auto_kps_debounce / member->conference->video_fps.ms;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s setting bitrate debounce timer to %dms\n", 
						  switch_channel_get_name(member->channel), member->conference->auto_kps_debounce);
		member->managed_kps = kps;
		member->managed_kps_set = 0;
		return;
	}

	msg.message_id = SWITCH_MESSAGE_INDICATE_BITRATE_REQ;
	msg.numeric_arg = kps * 1024;
	msg.from = __FILE__;
	
	switch_core_session_receive_message(member->session, &msg);	

	member->managed_kps_set = 1;
	member->managed_kps = kps;

}

void conference_video_set_max_incoming_bitrate_member(conference_member_t *member, int kps)
{
	member->max_bw_in = kps;
	conference_video_clear_managed_kps(member);
}

void conference_video_set_absolute_incoming_bitrate_member(conference_member_t *member, int kps)
{
	member->max_bw_in = 0;
	member->force_bw_in = kps;
	conference_video_clear_managed_kps(member);
	if (!conference_utils_test_flag(member->conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE) && switch_channel_test_flag(member->channel, CF_VIDEO_READY)) {
		conference_video_set_incoming_bitrate(member, kps, SWITCH_TRUE);
	}
}

void conference_video_set_max_incoming_bitrate(conference_obj_t *conference, int kps)
{
	conference_member_t *imember;

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		if (imember->channel && switch_channel_ready(imember->channel) && conference_utils_member_test_flag(imember, MFLAG_RUNNING)) {
			conference_video_set_max_incoming_bitrate_member(imember, kps);
		}
	}
	switch_mutex_unlock(conference->member_mutex);	
}

void conference_video_set_absolute_incoming_bitrate(conference_obj_t *conference, int kps)
{
	conference_member_t *imember;

	switch_mutex_lock(conference->member_mutex);
	for (imember = conference->members; imember; imember = imember->next) {
		if (imember->channel && switch_channel_ready(imember->channel) && conference_utils_member_test_flag(imember, MFLAG_RUNNING)) {
			conference_video_set_absolute_incoming_bitrate_member(imember, kps);
		}
	}
	switch_mutex_unlock(conference->member_mutex);	
}

void conference_video_check_auto_bitrate(conference_member_t *member, mcu_layer_t *layer)
{
	switch_vid_params_t vid_params = { 0 };
	int kps = 0, kps_in = 0;
	int max = 0;
	int min_layer = 0, min = 0;
	
	if (!conference_utils_test_flag(member->conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE) || 
		switch_channel_test_flag(member->channel, CF_VIDEO_BITRATE_UNMANAGABLE)) {
		return;
	}

	switch_core_media_get_vid_params(member->session, &vid_params);

	if (!switch_channel_test_flag(member->channel, CF_VIDEO_READY) || !vid_params.width || !vid_params.height) {
		return;
	}

	if (member->layer_loops < 10) {
		return;
	}


	if (member->auto_kps_debounce_ticks) {
		if (--member->auto_kps_debounce_ticks == 0) {
			conference_video_set_incoming_bitrate(member, member->managed_kps, SWITCH_TRUE);
		}
		return;
	}
	
	if (vid_params.width != member->vid_params.width || vid_params.height != member->vid_params.height) {
		switch_core_session_request_video_refresh(member->session);
		conference_video_clear_managed_kps(member);
	}

	member->vid_params = vid_params;

	if (member->managed_kps_set) {
		return;
	}

	if ((kps_in = switch_calc_bitrate(vid_params.width, vid_params.height, 
									  member->conference->video_quality, (int)(member->conference->video_fps.fps))) < 512) {
		kps_in = 512;
	}

	if (layer) {
		kps = switch_calc_bitrate(layer->screen_w, layer->screen_h, member->conference->video_quality, (int)(member->conference->video_fps.fps));
	} else {
		kps = kps_in;
	}

	min_layer = kps / 2;
	min = kps_in / 2;
	
	if (min_layer > min) min = min_layer;
	
	if (member->conference->max_bw_in) {
		max = member->conference->max_bw_in;
	} else {
		max = member->max_bw_in;
	}

	if (member->conference->force_bw_in || member->force_bw_in) {
		if (!(kps = member->conference->force_bw_in)) {
			kps = member->force_bw_in;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s setting bitrate to %dkps because it was forced.\n",
						  switch_channel_get_name(member->channel), kps);
	} else {
		if (layer && conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps to accomodate %dx%d resolution\n",
							  switch_channel_get_name(member->channel), kps, layer->screen_w, layer->screen_h);
		} else {
			kps = min;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps because the user is not visible\n",
							  switch_channel_get_name(member->channel), kps);
		}
	}

	if (kps) {

		if (min > max) {
			min = max;
		}

		if (max && kps > max) {
			kps = max;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s overriding bitrate setting to %dkps because it was the max allowed.\n",
							  switch_channel_get_name(member->channel), kps);
		}


		if (min && kps < min) {
			kps = min;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s overriding bitrate setting to %dkps because it was the min allowed.\n",
							  switch_channel_get_name(member->channel), kps);
		}

		conference_video_set_incoming_bitrate(member, kps, SWITCH_FALSE);
	}
}

static void wait_for_canvas(mcu_canvas_t *canvas)
{
	for (;;) {
		int x = 0;
		int i = 0;

		for (i = 0; i < canvas->total_layers; i++) {
			mcu_layer_t *layer = &canvas->layers[i];
					
			if (layer->need_patch) {
				if (layer->member) {
					switch_queue_trypush(layer->member->mux_out_queue, (void *) 1);
					x++;
				} else {
					layer->need_patch = 0;
				}
			}
		}

		if (!x) break;
		switch_cond_next();
		switch_thread_rwlock_wrlock(canvas->video_rwlock);
		switch_thread_rwlock_unlock(canvas->video_rwlock);
	}
}

static void personal_attach(mcu_layer_t *layer, conference_member_t *member)
{
	layer->tagged = 1;

	if (layer->member_id != (int)member->id) {
		const char *var = NULL;

		layer->mute_patched = 0;
		layer->avatar_patched = 0;
		switch_img_free(&layer->banner_img);
		switch_img_free(&layer->logo_img);
		
		if (layer->geometry.audio_position) {
			conference_api_sub_position(member, NULL, layer->geometry.audio_position);
		}
		
		var = NULL;
		if (member->video_banner_text ||
			(var = switch_channel_get_variable_dup(member->channel, "video_banner_text", SWITCH_FALSE, -1))) {
			conference_video_layer_set_banner(member, layer, var);
		}
		
		var = NULL;
		if (member->video_logo ||
			(var = switch_channel_get_variable_dup(member->channel, "video_logo_path", SWITCH_FALSE, -1))) {
			conference_video_layer_set_logo(member, layer, var);
		}
	}
	
	layer->member_id = member->id;
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
	int layout_applied = 0;
	int files_playing = 0;
	int last_personal = conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS) ? 1 : 0;

	canvas->video_timer_reset = 1;
	canvas->video_layout_group = conference->video_layout_group;

	packet = switch_core_alloc(conference->pool, SWITCH_RTP_MAX_BUF_LEN);
	
	while (conference_globals.running && !conference_utils_test_flag(conference, CFLAG_DESTRUCT) && conference_utils_test_flag(conference, CFLAG_VIDEO_MUXING)) {
		switch_bool_t need_refresh = SWITCH_FALSE, send_keyframe = SWITCH_FALSE, need_reset = SWITCH_FALSE;
		switch_time_t now;
		int min_members = 0;
		int count_changed = 0;
		int file_count = 0, check_async_file = 0, check_file = 0;
		switch_image_t *async_file_img = NULL, *normal_file_img = NULL, *file_imgs[2] = { 0 };
		switch_frame_t file_frame = { 0 };
		int j = 0, personal = conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS) ? 1 : 0;
		int video_count = 0;

		if (!personal) {
			if (canvas->new_vlayout && switch_mutex_trylock(conference->canvas_mutex) == SWITCH_STATUS_SUCCESS) {
				conference_video_init_canvas_layers(conference, canvas, NULL);
				switch_mutex_unlock(conference->canvas_mutex);
			}
		}

		if (canvas->video_timer_reset) {
			canvas->video_timer_reset = 0;

			if (canvas->timer.interval) {
				switch_core_timer_destroy(&canvas->timer);
			}

			switch_core_timer_init(&canvas->timer, "soft", conference->video_fps.ms, conference->video_fps.samples, NULL);
			canvas->send_keyframe = 1;
		}

		video_count = 0;

		if (conference->async_fnode && switch_core_file_has_video(&conference->async_fnode->fh, SWITCH_TRUE)) {
			check_async_file = 1;
			file_count++;
			video_count++;
			files_playing = 1;
		}

		if (conference->fnode && switch_core_file_has_video(&conference->fnode->fh, SWITCH_TRUE)) {
			check_file = 1;
			file_count++;
			video_count++;
			files_playing = 1;
		}


		switch_mutex_lock(conference->member_mutex);
		for (imember = conference->members; imember; imember = imember->next) {
			int no_muted = conference_utils_test_flag(imember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS);
			int no_av = conference_utils_test_flag(imember->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS);
			int seen = conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN);
			
			if (imember->channel && switch_channel_ready(imember->channel) && switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && 
				!conference_utils_member_test_flag(imember, MFLAG_SECOND_SCREEN) &&
				conference_utils_member_test_flag(imember, MFLAG_RUNNING) && (!no_muted || seen) && (!no_av || (no_av && !imember->avatar_png_img))
				&& imember->canvas_id == canvas->canvas_id && imember->video_media_flow != SWITCH_MEDIA_FLOW_SENDONLY) {
				video_count++;
			}
			
		}

		if (video_count != canvas->video_count) {
			count_changed = 1;
		}

		canvas->video_count = video_count;
		switch_mutex_unlock(conference->member_mutex);

		switch_core_timer_next(&canvas->timer);

		now = switch_micro_time_now();

		if (last_personal != personal) {
			do_refresh = 100;
			count_changed = 1;
			if ((last_personal = personal)) {
				switch_mutex_lock(conference->member_mutex);
				conference->new_personal_vlayout = canvas->vlayout;
				switch_mutex_unlock(conference->member_mutex);
			}
			conference_utils_set_flag(conference, CFLAG_REFRESH_LAYOUT);
		}

		if (members_with_video != conference->members_with_video) {
			do_refresh = 100;
			count_changed = 1;
		}

		if (members_with_avatar != conference->members_with_avatar) {
			count_changed = 1;
		}
	
		if (conference_utils_test_flag(conference, CFLAG_REFRESH_LAYOUT)) {
			count_changed = 1;
			conference_utils_clear_flag(conference, CFLAG_REFRESH_LAYOUT);
		}

		if (count_changed) {
			need_refresh = 1;
			send_keyframe = 1;
			do_refresh = 100;
		}


		if (file_count != last_file_count) {
			count_changed = 1;
		}

		if (count_changed && !personal) {
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;
			
			if (canvas->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, canvas->video_layout_group))) {
				if ((vlayout = conference_video_find_best_layout(conference, lg, canvas->video_count))) {
					switch_mutex_lock(conference->member_mutex);
					canvas->new_vlayout = vlayout;
					switch_mutex_unlock(conference->member_mutex);
				}
			}
		}


		last_file_count = file_count;

		if (do_refresh) {
			if ((do_refresh % 50) == 0) {
				switch_mutex_lock(conference->member_mutex);

				for (imember = conference->members; imember; imember = imember->next) {
					if (imember->canvas_id != canvas->canvas_id) continue;

					if (imember->session && switch_channel_test_flag(imember->channel, CF_VIDEO_READY)) {
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

			if (!imember->session || (!switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && !imember->avatar_png_img) ||
				personal || switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}

			if (imember->watching_canvas_id == canvas->canvas_id && switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
				switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
				send_keyframe = SWITCH_TRUE;
			}

			if (conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) &&
				imember->watching_canvas_id > -1 && imember->watching_canvas_id == canvas->canvas_id &&
				!conference_utils_member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				min_members++;

				if (switch_channel_test_flag(imember->channel, CF_VIDEO_READY)) {
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
								if (conference->scale_h264_canvas_width > 0 && conference->scale_h264_canvas_height > 0 && !strcmp(check_codec->implementation->iananame, "H264")) {
									int32_t bw = -1;

									write_codecs[i]->fps_divisor = conference->scale_h264_canvas_fps_divisor;
									write_codecs[i]->scaled_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, conference->scale_h264_canvas_width, conference->scale_h264_canvas_height, 16);

									if (conference->scale_h264_canvas_bandwidth) {
										if (strcasecmp(conference->scale_h264_canvas_bandwidth, "auto")) {
											bw = switch_parse_bandwidth_string(conference->scale_h264_canvas_bandwidth);
										}
									}

									if (bw == -1) {
										float fps = conference->video_fps.fps;

										if (write_codecs[i]->fps_divisor) fps /= write_codecs[i]->fps_divisor;

										bw = switch_calc_bitrate(conference->scale_h264_canvas_width, conference->scale_h264_canvas_height, conference->video_quality, fps);
									}

									switch_core_codec_control(&write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &bw, SCCT_NONE, NULL, NULL, NULL);
								}
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

			if (conference_utils_test_flag(imember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS) && 
				!conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN) && imember->video_layer_id > -1) {
				conference_video_detach_video_layer(imember);
				switch_img_free(&imember->video_mute_img);
				
				if (imember->id == imember->conference->video_floor_holder) {
					conference_video_set_floor_holder(conference, NULL, SWITCH_FALSE);
				} else if (imember->id == imember->conference->last_video_floor_holder) {
					conference->last_video_floor_holder = 0;
				}
				
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


			if (conference->playing_video_file) {
				switch_img_free(&img);
				switch_core_session_rwunlock(imember->session);
				continue;
			}

			switch_mutex_lock(canvas->mutex);

			//printf("MEMBER %d layer_id %d canvas: %d/%d\n", imember->id, imember->video_layer_id,
			//	   canvas->layers_used, canvas->total_layers);

			if (imember->video_layer_id > -1) {
				layer = &canvas->layers[imember->video_layer_id];
				if (layer->member_id != (int)imember->id) {
					imember->video_layer_id = -1;
					layer = NULL;
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

			if (!layer && (!conference_utils_test_flag(imember->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS) || ((switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY)))) {
				if (conference_video_find_layer(conference, canvas, imember, &layer) == SWITCH_STATUS_SUCCESS) {
					imember->layer_timeout = 0;
				} else {
					if (--imember->layer_timeout <= 0) {

						conference_video_next_canvas(imember);
					}
				}
			}

			imember->layer_loops++;
			conference_video_check_auto_bitrate(imember, layer);

			if (layer) {

				layer->member = imember;

				//if (layer->cur_img && layer->cur_img != imember->avatar_png_img) {
				//	switch_img_free(&layer->cur_img);
				//}

				if (conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN) || switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY || conference_utils_test_flag(imember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS)) {
					layer->mute_patched = 0;
				} else {
					switch_image_t *tmp;

					if (img && img != imember->avatar_png_img) {
						switch_img_free(&img);
					}

					if (!layer->mute_patched) {
						
						if (!imember->video_mute_img) {
							conference_video_vmute_snap(imember, SWITCH_FALSE);
						}
						
						if (imember->video_mute_img || layer->mute_img) {
							conference_video_clear_layer(layer);
							
							if (!layer->mute_img) {								
								if (imember->video_mute_img) {
									//layer->mute_img = switch_img_read_png(imember->video_mute_png, SWITCH_IMG_FMT_I420);
									switch_img_copy(imember->video_mute_img, &layer->mute_img);
								}
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

		if (personal) {
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;
			conference_member_t *omember;

			if (video_key_freq && (now - last_key_time) > video_key_freq) {
				send_keyframe = SWITCH_TRUE;
				last_key_time = now;
			}

			switch_mutex_lock(conference->member_mutex);

			for (imember = conference->members; imember; imember = imember->next) {

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (!imember->canvas) {
					if ((vlayout = conference_video_get_layout(conference, conference->video_layout_name, canvas->video_layout_group))) {
						conference_video_init_canvas(conference, vlayout, &imember->canvas);
						conference_video_init_canvas_layers(conference, imember->canvas, vlayout);
					} else {
						continue;
					}
				}

				if (conference->new_personal_vlayout) {
					conference_video_init_canvas_layers(conference, imember->canvas, conference->new_personal_vlayout);
					layout_applied++;
				}
				
				if (switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
					switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
					send_keyframe = SWITCH_TRUE;
				}
				
				if (count_changed) {
					int total = conference->members_with_video;
					int kps;
					switch_vid_params_t vid_params = { 0 };

					if (!conference_utils_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
						total += conference->members_with_avatar;
					}

					if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY) {
						total--;
					}

					if (total < 1) total = 1;

					if (conference->members_with_video == 1 && file_count) {
						total = 0;
					}
					
					if (canvas->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, canvas->video_layout_group))) {
						if ((vlayout = conference_video_find_best_layout(conference, lg, total + file_count))) {
							conference_video_init_canvas_layers(conference, imember->canvas, vlayout);
						}
					}
					
					if (!switch_channel_test_flag(imember->channel, CF_VIDEO_BITRATE_UNMANAGABLE) && 
						conference_utils_test_flag(conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE)) {
						switch_core_media_get_vid_params(imember->session, &vid_params);
						kps = switch_calc_bitrate(vid_params.width, vid_params.height, conference->video_quality, (int)(imember->conference->video_fps.fps));
						conference_video_set_incoming_bitrate(imember, kps, SWITCH_TRUE);
					}
				}
				
				if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY) {
					conference_video_pop_next_image(imember, &imember->pcanvas_img);
				}

				switch_core_session_rwunlock(imember->session);
			}

			if (conference->new_personal_vlayout && layout_applied) {
				conference->new_personal_vlayout = NULL;
				layout_applied = 0;
			}
			
			if (check_async_file) {
				switch_status_t st = switch_core_file_read_video(&conference->async_fnode->fh, &file_frame, SVR_FLUSH);
				
				if (st == SWITCH_STATUS_SUCCESS) {
					if ((async_file_img = file_frame.img)) {
						switch_img_free(&file_imgs[j]);
						file_imgs[j++] = async_file_img;
					}
				} else if (st == SWITCH_STATUS_BREAK) {
					j++;
				}
			}

			if (check_file) {
				switch_status_t st = switch_core_file_read_video(&conference->fnode->fh, &file_frame, SVR_FLUSH);

				if (st == SWITCH_STATUS_SUCCESS) {
					if ((normal_file_img = file_frame.img)) {
						switch_img_free(&file_imgs[j]);
						file_imgs[j++] = normal_file_img;
					}
				} else if (st == SWITCH_STATUS_BREAK) {
					j++;
				}
			}

			for (imember = conference->members; imember; imember = imember->next) {
				int i = 0;
				mcu_layer_t *floor_layer = NULL;
				
				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) || !imember->canvas ||
					(switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY) ||
					(switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS)) {
					continue;
				}

				if (files_playing && !file_count) {
					i = 0;
					while (i < imember->canvas->total_layers) {
						layer = &imember->canvas->layers[i++];
						switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->bgcolor);
					}
					i = 0;
				}

				if (!file_count && imember->canvas->layout_floor_id > -1 && imember->conference->video_floor_holder &&
					imember->id != imember->conference->video_floor_holder) {
					
					if ((omember = conference_member_get(imember->conference, imember->conference->video_floor_holder))) {
						if (conference->members_with_video + conference->members_with_avatar == 1 || imember != omember) {
							layer = &imember->canvas->layers[imember->canvas->layout_floor_id];
							floor_layer = layer;
							layer = NULL;
						}
						
						switch_thread_rwlock_unlock(omember->rwlock);
					}
				}

				
				for (omember = conference->members; omember; omember = omember->next) {
					mcu_layer_t *layer = NULL;
					switch_image_t *use_img = NULL;

					if (!omember->session || !switch_channel_test_flag(omember->channel, CF_VIDEO_READY) ||
						switch_core_session_media_flow(omember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY) {
						continue;
					}
					
					if (conference->members_with_video + conference->members_with_avatar != 1 && imember == omember) {
						continue;
					}

					if (file_count && (conference->members_with_video + conference->members_with_avatar == 1)) {
						floor_layer = NULL;
					}
					
					if (!file_count && floor_layer && omember->id == conference->video_floor_holder) {
						layer = floor_layer;
					} else {
						if (floor_layer || (file_count && i == imember->canvas->layout_floor_id)) {
							if ((i+1) < imember->canvas->total_layers) {
								i++;
							}
						}
						
						if (i < imember->canvas->total_layers) {
							layer = &imember->canvas->layers[i++];
						}
					}

					if (layer) {
						personal_attach(layer, omember);
					}

					if (!layer && omember->al) {
						conference_api_sub_position(omember, NULL, "0:0:0");
					}

					use_img = omember->pcanvas_img;
					
					if (files_playing && layer && layer == &imember->canvas->layers[imember->canvas->layout_floor_id]) {
						use_img = NULL;
					}
					
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
							if (conference_utils_member_test_flag(omember, MFLAG_CAN_BE_SEEN)) {
								layer->mute_patched = 0;
							} else {
								if (!layer->mute_patched) {
									switch_image_t *tmp;
									conference_video_scale_and_patch(layer, omember->video_mute_img ? omember->video_mute_img : omember->pcanvas_img, SWITCH_FALSE);
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
				}

				for (j = 0; j < file_count; j++) {
					switch_image_t *img = file_imgs[j];

					if (j == 0 && imember->canvas->layout_floor_id > -1) {
						layer = &imember->canvas->layers[imember->canvas->layout_floor_id];
						conference_video_scale_and_patch(layer, img, SWITCH_FALSE);
					} else if (i < imember->canvas->total_layers) {
						layer = &imember->canvas->layers[i++];
						conference_video_scale_and_patch(layer, img, SWITCH_FALSE);
					}
				}
				
				switch_core_session_rwunlock(imember->session);
			}

			if (files_playing && !file_count) {
				switch_img_free(&file_imgs[0]);
				switch_img_free(&file_imgs[1]);
				files_playing = 0;
			}

			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (send_keyframe) {
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
			
			if (conference->async_fnode && (conference->async_fnode->canvas_id == canvas->canvas_id || conference->async_fnode->canvas_id == -1)) {
				if (conference->async_fnode->layer_id > -1) {
					conference_video_patch_fnode(canvas, conference->async_fnode);
				} else {
					conference_video_fnode_check(conference->async_fnode, canvas->canvas_id);
				}
			}

			if (conference->fnode && (conference->fnode->canvas_id == canvas->canvas_id || conference->fnode->canvas_id == -1)) {
				if (conference->fnode->layer_id > -1) {
					conference_video_patch_fnode(canvas, conference->fnode);
				} else {
					conference_video_fnode_check(conference->fnode, canvas->canvas_id);
				}
			}

			if (!conference->playing_video_file) {
				for (i = 0; i < canvas->total_layers; i++) {
					mcu_layer_t *layer = &canvas->layers[i];

					if (!layer->mute_patched && (layer->member_id > -1 || layer->fnode) && layer->cur_img && layer->tagged && !layer->geometry.overlap) {
						if (canvas->refresh) {
							layer->refresh = 1;
							canvas->refresh++;
						}

						if (layer->cur_img) {
							if (layer->member && switch_core_cpu_count() > 2) {
								layer->need_patch = 1;
							} else {
								conference_video_scale_and_patch(layer, NULL, SWITCH_FALSE);
							}
						}

						layer->tagged = 0;
					}
				}

				wait_for_canvas(canvas);

				for (i = 0; i < canvas->total_layers; i++) {
					mcu_layer_t *layer = &canvas->layers[i];

					if (!layer->mute_patched && (layer->member_id > -1 || layer->fnode) && layer->cur_img && layer->geometry.overlap) {
						if (canvas->refresh) {
							layer->refresh = 1;
							canvas->refresh++;
						}

						if (layer->cur_img) {
							if (layer->member) {
								layer->need_patch = 1;
							} else {
								conference_video_scale_and_patch(layer, NULL, SWITCH_FALSE);
							}
						}
					}
				}
			}

			if (canvas->refresh > 1) {
				if (canvas->bgimg) {
					conference_video_set_canvas_bgimg(canvas, NULL);
				}
				canvas->refresh = 0;
			}

			if (canvas->send_keyframe > 0) {
				if (canvas->send_keyframe == 1 || (canvas->send_keyframe % 10) == 0) {
					send_keyframe = SWITCH_TRUE;
					need_refresh = SWITCH_TRUE;
				}
				canvas->send_keyframe--;
			}

			if (video_key_freq && (now - last_key_time) > video_key_freq) {
				send_keyframe = SWITCH_TRUE;
				last_key_time = now;
			}

			write_img = canvas->img;
			timestamp = canvas->timer.samplecount;

			if (conference->playing_video_file) {
				if (switch_core_file_read_video(&conference->fnode->fh, &write_frame, SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
					switch_img_free(&file_img);

					if (canvas->play_file) {
						canvas->send_keyframe = 1;
						canvas->play_file = 0;
					}

					switch_img_free(&file_img);
					file_img = write_img = write_frame.img;

					switch_core_timer_sync(&canvas->timer);
					timestamp = canvas->timer.samplecount;
				} else if (file_img) {
					write_img = file_img;
				}
			} else if (file_img) {
				switch_img_free(&file_img);
			}
			
			write_frame.img = write_img;

			wait_for_canvas(canvas);
			
			if (canvas->recording) {
				conference_video_check_recording(conference, canvas, &write_frame);
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
																	   timestamp, need_refresh, send_keyframe, need_reset);

					if (canvas->video_write_bandwidth) {
						switch_core_codec_control(&write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, 
												  SCCT_INT, &canvas->video_write_bandwidth, SCCT_NONE, NULL, NULL, NULL);
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

				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}
				
				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY) {
					switch_core_session_rwunlock(imember->session);
					continue;
				}


				if (send_keyframe) {
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
					if (switch_queue_trypush(imember->mux_out_queue, dupframe) != SWITCH_STATUS_SUCCESS) {
						switch_frame_buffer_free(imember->fb, &dupframe);
					}
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
			switch_img_free(&(write_codecs[i]->scaled_img));
		}
	}

	conference_close_open_files(conference);

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
		switch_bool_t need_refresh = SWITCH_FALSE, send_keyframe = SWITCH_FALSE, need_reset = SWITCH_FALSE;
		switch_time_t now;
		int min_members = 0;
		int count_changed = 0;
		int  layer_idx = 0;
		uint32_t j = 0;
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

		switch_core_timer_next(&canvas->timer);

		now = switch_micro_time_now();

		if (canvas->send_keyframe > 0) {
			if (canvas->send_keyframe == 1 || (canvas->send_keyframe % 10) == 0) {
				send_keyframe = SWITCH_TRUE;
				need_refresh = SWITCH_TRUE;
			}
			canvas->send_keyframe--;
		}

		if (video_key_freq && (now - last_key_time) > video_key_freq) {
			send_keyframe = SWITCH_TRUE;
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

			if (!imember->session || (!switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && !imember->avatar_png_img) ||
				conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS) || switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}

			if (imember->watching_canvas_id == canvas->canvas_id && switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
				switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
				send_keyframe = SWITCH_TRUE;
			}

			if (conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING) &&
				imember->watching_canvas_id > -1 && imember->watching_canvas_id == canvas->canvas_id &&
				!conference_utils_member_test_flag(imember, MFLAG_NO_MINIMIZE_ENCODING)) {
				min_members++;

				if (switch_channel_test_flag(imember->channel, CF_VIDEO_READY)) {
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

		if (canvas->recording) {
			conference_video_check_recording(conference, canvas, &write_frame);
		}

		if (min_members && conference_utils_test_flag(conference, CFLAG_MINIMIZE_VIDEO_ENCODING)) {
			for (i = 0; write_codecs[i] && switch_core_codec_ready(&write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
				write_codecs[i]->frame.img = write_img;
				conference_video_write_canvas_image_to_codec_group(conference, canvas, write_codecs[i], i, timestamp, need_refresh, send_keyframe, need_reset);

				if (canvas->video_write_bandwidth) {
					switch_core_codec_control(&write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, 
											  SCCT_INT, &canvas->video_write_bandwidth, SCCT_NONE, NULL, NULL, NULL);
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

			
			if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ||
				switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
				continue;
			}

			if (need_refresh) {
				switch_core_session_request_video_refresh(imember->session);
			}

			if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY) {
				switch_core_session_rwunlock(imember->session);
				continue;
			}

			if (send_keyframe) {
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
				if (switch_queue_trypush(imember->mux_out_queue, dupframe) != SWITCH_STATUS_SUCCESS) {
					switch_frame_buffer_free(imember->fb, &dupframe);
				}
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

		if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY && !imember->avatar_png_img) {
			continue;
		}

		if (!switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && !imember->avatar_png_img) {
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

	if (conference->canvas_count > 1) {
		return;
	}

	if (member && member->video_reservation_id) {
		/* no video floor when a reservation id is set */
		return;
	}

	if ((!force && conference_utils_test_flag(conference, CFLAG_VID_FLOOR_LOCK))) {
		return;
	}

	if (member && switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY && !member->avatar_png_img) {
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
				conference_video_clear_managed_kps(imember);
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
			if (imember->id != conference->video_floor_holder && imember->channel && switch_channel_test_flag(imember->channel, CF_VIDEO_READY)) {
				member = imember;
				break;
			}
		}
		switch_mutex_unlock(conference->member_mutex);
	}

	//VIDFLOOR
	if (member) {
		mcu_canvas_t *canvas = NULL;

		if ((canvas = conference_video_get_canvas_locked(member))) {
			if (canvas->layout_floor_id > -1) {
				conference_video_attach_video_layer(member, canvas, canvas->layout_floor_id);
			}
			conference_video_release_canvas(&canvas);
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Adding video floor %s\n",
						  switch_channel_get_name(member->channel));

		conference_video_check_flush(member);
		switch_core_session_video_reinit(member->session);
		conference->video_floor_holder = member->id;
		conference_member_update_status_field(member);
		conference_video_clear_managed_kps(member);
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
		if (!imember->channel || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY)) {
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

	if (vid_frame->img && conference->canvases[0]) {
		switch_image_t *frame_img = NULL, *tmp_img = NULL;
		int x,y;

		switch_img_copy(vid_frame->img, &tmp_img);
		switch_img_fit(&tmp_img, conference->canvases[0]->width, conference->canvases[0]->height, SWITCH_FIT_SIZE);
		frame_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, conference->canvases[0]->width, conference->canvases[0]->height, 1);
		conference_video_reset_image(frame_img, &conference->canvases[0]->bgcolor);
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

		if (isession && switch_channel_test_flag(imember->channel, CF_VIDEO_READY)) {
			int send_frame = 0;

			if (conference->canvases[0] && conference_utils_test_flag(imember->conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
				if (switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && (conference->members_with_video == 1 || imember != floor_holder)) {
					send_frame = 1;
				}
			} else if (!conference_utils_member_test_flag(imember, MFLAG_RECEIVING_VIDEO) &&
					   (conference_utils_test_flag(conference, CFLAG_VID_FLOOR_LOCK) ||
						!(imember->id == imember->conference->video_floor_holder && imember->conference->last_video_floor_holder))) {
				send_frame = 1;
			}

			if (send_frame) {
				if (vid_frame->img) {
					if (conference->canvases[0]) {
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

	if (want_refresh) {
		for (imember = conference->members; imember; imember = imember->next) {
			switch_core_session_t *isession = imember->session;
			
			if (!isession || switch_core_session_read_lock(isession) != SWITCH_STATUS_SUCCESS) {
				continue;
			}
			
			if (switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ) {
				switch_core_session_request_video_refresh(imember->session);	
			}
			
			switch_core_session_rwunlock(isession);
		}
	}

	switch_mutex_unlock(conference->member_mutex);

	switch_img_free(&tmp_frame.img);
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

	if (switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_thread_rwlock_tryrdlock(member->conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}


	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
		if (member->conference->members_with_video < 3) {
			conference_video_write_frame(member->conference, member, frame);
			conference_video_check_recording(member->conference, NULL, frame);
			switch_thread_rwlock_unlock(member->conference->rwlock);
			return SWITCH_STATUS_SUCCESS;
		}
	}


	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_MUXING)) {
		switch_image_t *img_copy = NULL;

		if (frame->img && (member->video_layer_id > -1 || member->canvas) && 
			conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) &&
			switch_queue_size(member->video_queue) < member->conference->video_fps.fps * 2 &&
			!member->conference->playing_video_file) {

			if (conference_utils_member_test_flag(member, MFLAG_FLIP_VIDEO) || conference_utils_member_test_flag(member, MFLAG_ROTATE_VIDEO)) {
				if (conference_utils_member_test_flag(member, MFLAG_ROTATE_VIDEO)) {
					if (member->flip_count++ > (int)(member->conference->video_fps.fps / 2)) {
						member->flip += 90;
						if (member->flip > 270) {
							member->flip = 0;
						}
						member->flip_count = 0;
					}

					switch_img_rotate_copy(frame->img, &img_copy, member->flip);
				} else {
					switch_img_rotate_copy(frame->img, &img_copy, member->flip);
				}

			} else {
				switch_img_copy(frame->img, &img_copy);
			}
			
			if (switch_queue_trypush(member->video_queue, img_copy) != SWITCH_STATUS_SUCCESS) {
				switch_img_free(&img_copy);
			}
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
			conference_video_check_recording(member->conference, NULL, frame);
		} else if (!conference_utils_test_flag(member->conference, CFLAG_VID_FLOOR_LOCK) && member->id == member->conference->last_video_floor_holder) {
			conference_member_t *fmember;

			if ((fmember = conference_member_get(member->conference, member->conference->video_floor_holder))) {
				if (!conference_utils_member_test_flag(fmember, MFLAG_RECEIVING_VIDEO))
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

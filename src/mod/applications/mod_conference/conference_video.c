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

int conference_video_set_fps(conference_obj_t *conference, float fps)
{
	uint32_t j = 0;

	if (fps > 100) {
		return 0;
	}

	conference->video_fps.fps = fps;
	conference->video_fps.ms = (int) 1000 / fps;
	conference->video_fps.samples = (int) 90000 / conference->video_fps.ms;

	for (j = 0; j <= conference->canvas_count; j++) {
		if (conference->canvases[j]) {
			conference->canvases[j]->video_timer_reset = 1;
		}
	}

	return 1;
}

static int COMPLETE_INIT = 0;

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

	if (!(cxml = switch_xml_open_cfg(conference->video_layout_conf, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", conference->video_layout_conf);
		goto done;
	}

	if ((x_layout_settings = switch_xml_child(cfg, "layout-settings"))) {
		if ((x_layouts = switch_xml_child(x_layout_settings, "layouts"))) {
			for (x_layout = switch_xml_child(x_layouts, "layout"); x_layout; x_layout = x_layout->next) {
				video_layout_t *vlayout;
				const char *val = NULL, *name = NULL, *bgimg = NULL, *fgimg = NULL, *transition_in = NULL, *transition_out = NULL;
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
				fgimg = switch_xml_attr(x_layout, "fgimg");

				transition_in = switch_xml_attr(x_layout, "transition-in");
				transition_out = switch_xml_attr(x_layout, "transition-out");
				
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

				if (fgimg) {
					vlayout->fgimg = switch_core_strdup(conference->pool, fgimg);
				}

				if (transition_in) {
					vlayout->transition_in = switch_core_sprintf(conference->pool, "{full-screen=true}%s", transition_in);
				}
				
				if (transition_out) {
					vlayout->transition_out = switch_core_sprintf(conference->pool, "{full-screen=true}%s", transition_out);
				}

				for (x_image = switch_xml_child(x_layout, "image"); x_image; x_image = x_image->next) {
					const char *res_id = NULL, *audio_position = NULL, *role_id = NULL;
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
						fileonly = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "overlap"))) {
						overlap = switch_true(val);
					}

					if ((val = switch_xml_attr(x_image, "reservation_id"))) {
						res_id = val;
					}

					if ((val = switch_xml_attr(x_image, "reservation-id"))) {
						res_id = val;
					}

					if ((val = switch_xml_attr(x_image, "role-id"))) {
						role_id = val;
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

					if (fileonly) {
						floor = flooronly = 0;
					}

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

					if (role_id) {
						vlayout->images[vlayout->layers].role_id = switch_core_strdup(conference->pool, role_id);
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

				if (!conference->default_layout_name) {
					conference->default_layout_name = switch_core_strdup(conference->pool, name);
				}
				switch_core_hash_insert(conference->layout_hash, name, vlayout);
				
				if (!COMPLETE_INIT) {
					switch_mutex_lock(conference_globals.hash_mutex);
					if (!COMPLETE_INIT) {
						switch_snprintf(cmd_str, sizeof(cmd_str), "add conference ::conference::conference_list_conferences vid-layout %s", name);
						switch_console_set_complete(cmd_str);
						COMPLETE_INIT++;
					}
					switch_mutex_unlock(conference_globals.hash_mutex);
				}
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
	if (layer->canvas && layer->canvas->img) {
		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->bgcolor);
	}

	layer->banner_patched = 0;
	layer->refresh = 1;
	layer->mute_patched = 0;
}

static void set_default_cam_opts(mcu_layer_t *layer)
{
	//layer->cam_opts.autozoom = 1;
	//layer->cam_opts.autopan = 1;
	layer->cam_opts.manual_pan = 0;
	layer->cam_opts.manual_zoom = 0;
	layer->cam_opts.zoom_factor = 3;
	layer->cam_opts.snap_factor = 25;
	layer->cam_opts.zoom_move_factor = 125;
	layer->cam_opts.pan_speed = 3;
	layer->cam_opts.pan_accel_speed = 10;
	layer->cam_opts.pan_accel_min = 50;
	layer->cam_opts.zoom_speed = 3;
	layer->cam_opts.zoom_accel_speed = 10;
	layer->cam_opts.zoom_accel_min = 50;
}


void conference_video_reset_layer_cam(mcu_layer_t *layer)
{
	layer->crop_x = 0;
	layer->crop_y = 0;
	layer->crop_w = 0;
	layer->crop_h = 0;
	layer->last_w = 0;
	layer->last_h = 0;
	layer->img_count = 0;

	memset(&layer->bug_frame, 0, sizeof(layer->bug_frame));
	memset(&layer->auto_geometry, 0, sizeof(layer->auto_geometry));
	memset(&layer->pan_geometry, 0, sizeof(layer->pan_geometry));
	memset(&layer->zoom_geometry, 0, sizeof(layer->zoom_geometry));
	memset(&layer->last_geometry, 0, sizeof(layer->last_geometry));

	set_default_cam_opts(layer);


}

void conference_video_reset_layer(mcu_layer_t *layer)
{
	switch_img_free(&layer->banner_img);
	switch_img_free(&layer->logo_img);

	layer->bugged = 0;
	layer->mute_patched = 0;
	layer->banner_patched = 0;
	layer->is_avatar = 0;
	layer->need_patch = 0;
	layer->manual_border = 0;
	
	conference_video_reset_layer_cam(layer);

	if (layer->geometry.overlap) {
		layer->canvas->refresh = 1;
	}

	switch_mutex_lock(layer->overlay_mutex);
	if (layer->img && (layer->img->d_w != layer->screen_w || layer->img->d_h != layer->screen_h)) {
		switch_img_free(&layer->img);
	}

	if (!layer->img && layer->screen_w && layer->screen_h) {
		layer->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, layer->screen_w, layer->screen_h, 1);
		switch_assert(layer->img);
	}

	conference_video_clear_layer(layer);
	switch_img_free(&layer->cur_img);

	switch_img_free(&layer->overlay_img);
	switch_mutex_unlock(layer->overlay_mutex);
}

static void set_pan(int crop_point, int *target_point, int accel_speed, int accel_min, int speed)
{

	if (crop_point > *target_point) {
		if ((crop_point - *target_point) > accel_min) {
			*target_point += accel_speed;
		} else {
			*target_point += speed;
		}

		if (*target_point > crop_point) {
			*target_point = crop_point;
		}
	} else if (crop_point < *target_point) {

		if ((*target_point - crop_point) > accel_min) {
			*target_point -= accel_speed;
		} else {
			*target_point -= speed;
		}

		if (*target_point < crop_point) {
			*target_point = crop_point;
		}
	}
}

static void set_bounds(int *x, int *y, int img_w, int img_h, int crop_w, int crop_h)
{
	int crop_x = *x;
	int crop_y = *y;
	
	if (crop_x < 0) {
		crop_x = 0;
	}

	if (crop_y < 0) {
		crop_y = 0;
	}
	
	if (crop_x + crop_w > img_w) {
		crop_x = img_w - crop_w;
	}
	
	if (crop_y + crop_h > img_h) {
		crop_y = img_h - crop_h;
	}

	if (crop_x < 0) {
		crop_x = 0;
	}

	if (crop_y < 0) {
		crop_y = 0;
	}

	*x = crop_x;
	*y = crop_y;


}

void conference_video_scale_and_patch(mcu_layer_t *layer, switch_image_t *ximg, switch_bool_t freeze)
{
	switch_image_t *IMG, *img;
	int img_changed = 0, want_w = 0, want_h = 0, border = 0;

	switch_mutex_lock(layer->canvas->mutex);

	IMG = layer->canvas->img;
	img = ximg ? ximg : layer->cur_img;

	switch_assert(IMG);

	if (!img) {
		switch_mutex_unlock(layer->canvas->mutex);
		return;
	}
	//printf("RAW %dx%d\n", img->d_w, img->d_h);

	if (layer->img_count++ == 0 || layer->last_w != img->d_w || layer->last_h != img->d_h) {
		double change_scale;

		if (img->d_w && layer->last_w) {
			if (img->d_w < layer->last_w) {
				change_scale = (double) layer->last_w / img->d_w;
			} else {
				change_scale = (double) img->d_w / layer->last_w;
			}

			layer->crop_x = (int)(layer->crop_x * change_scale);
			layer->crop_y = (int)(layer->crop_y * change_scale);
			layer->crop_w = (int)(layer->crop_w * change_scale);
			layer->crop_h = (int)(layer->crop_h * change_scale);

			layer->zoom_geometry.x = (int)(layer->zoom_geometry.x * change_scale);
			layer->zoom_geometry.y = (int)(layer->zoom_geometry.y * change_scale);
			layer->zoom_geometry.w = (int)(layer->zoom_geometry.w * change_scale);
			layer->zoom_geometry.h = (int)(layer->zoom_geometry.h * change_scale);


			layer->pan_geometry.x = (int)(layer->pan_geometry.x * change_scale);
			layer->pan_geometry.y = (int)(layer->pan_geometry.y * change_scale);
			layer->pan_geometry.w = (int)(layer->pan_geometry.w * change_scale);
			layer->pan_geometry.h = (int)(layer->pan_geometry.h * change_scale);

		}

		memset(&layer->auto_geometry, 0, sizeof(layer->auto_geometry));
		//memset(&layer->zoom_geometry, 0, sizeof(layer->zoom_geometry));
		//memset(&layer->pan_geometry, 0, sizeof(layer->pan_geometry));
		memset(&layer->last_geometry, 0, sizeof(layer->last_geometry));

		img_changed = 1;
	}

	layer->last_w = img->d_w;
	layer->last_h = img->d_h;


	if (layer->bugged) {
		if (layer->member_id > -1 && layer->member && switch_thread_rwlock_tryrdlock(layer->member->rwlock) == SWITCH_STATUS_SUCCESS) {

			layer->bug_frame.img = img;
			switch_core_media_bug_patch_video(layer->member->session, &layer->bug_frame);
			layer->bug_frame.img = NULL;
			switch_thread_rwlock_unlock(layer->member->rwlock);
		}

		if ((!layer->manual_geometry.w || 
			 (layer->last_geometry.x && abs(layer->manual_geometry.x - layer->last_geometry.x) > layer->cam_opts.zoom_move_factor) ||
			 (layer->last_geometry.y && abs(layer->manual_geometry.y - layer->last_geometry.y) > layer->cam_opts.zoom_move_factor) ||
			 (layer->last_geometry.w && abs(layer->manual_geometry.w - layer->last_geometry.w) > layer->cam_opts.zoom_move_factor / 2))) {
			switch_event_t *event;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "action", "movement-detection");
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "member_id", "%d", layer->member_id);

				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "last_x", "%d", layer->manual_geometry.x);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "last_y", "%d", layer->manual_geometry.y);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "last_w", "%d", layer->manual_geometry.w);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "last_h", "%d", layer->manual_geometry.h);

				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "new_x", "%d", layer->bug_frame.geometry.x);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "new_y", "%d", layer->bug_frame.geometry.y);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "new_w", "%d", layer->bug_frame.geometry.w);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "new_h", "%d", layer->bug_frame.geometry.h);

				switch_event_fire(&event);
			}
			
			layer->manual_geometry = layer->bug_frame.geometry;
		}
		
		layer->bugged = 0;
	} else {
		if (layer->bug_frame.geometry.w) {
			memset(&layer->bug_frame, 0, sizeof(layer->bug_frame));
		}
		layer->cam_opts.autozoom = 0;
		layer->cam_opts.autopan = 0;
	}

	if (layer->clear) {
		conference_video_clear_layer(layer);
		layer->clear = 0;
	}

	if (layer->refresh) {
		switch_img_fill(layer->canvas->img, layer->x_pos, layer->y_pos, layer->screen_w, layer->screen_h, &layer->canvas->letterbox_bgcolor);
		layer->banner_patched = 0;
		layer->refresh = 0;
	}

	if (layer->geometry.scale) {
		uint32_t img_w = 0, img_h = 0;
		double screen_aspect = 0, img_aspect = 0;
		int x_pos = layer->x_pos;
		int y_pos = layer->y_pos;
		switch_frame_geometry_t *use_geometry = &layer->auto_geometry;
		img_w = layer->screen_w = (uint32_t)(IMG->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE);
		img_h = layer->screen_h = (uint32_t)(IMG->d_h * layer->geometry.hscale / VIDEO_LAYOUT_SCALE);


		screen_aspect = (double) layer->screen_w / layer->screen_h;
		img_aspect = (double) img->d_w / img->d_h;

		if ((layer->geometry.zoom || layer->cam_opts.autozoom || 
												 layer->cam_opts.autopan || layer->cam_opts.manual_pan || layer->cam_opts.manual_zoom)) {

			double scale = 1;
			int crop_x = 0, crop_y = 0, crop_w = 0, crop_h = 0, zoom_w = 0, zoom_h = 0;
			int can_pan = 0;
			int can_zoom = 0;
			int did_zoom = 0;

			if (screen_aspect <= img_aspect) {
				if (img->d_h != layer->screen_h) {
					scale = (double)layer->screen_h / img->d_h;
				}
			} else if (screen_aspect > img_aspect) {
				if (img->d_w != layer->screen_w) {
					scale = (double)layer->screen_w / img->d_w;
				}
			}

			if (scale == 1) {
				crop_w = layer->screen_w;
				crop_h = layer->screen_h;
			} else {
				crop_w = (uint32_t)((double)layer->screen_w / scale);
				crop_h = (uint32_t)((double)layer->screen_h / scale);
			}

			//if (layer->bug_frame.geometry.X > 90) {
			//	memset(&layer->auto_geometry, 0, sizeof(layer->auto_geometry));
			//}
			
			if (layer->cam_opts.autopan) {
				can_pan = layer->bug_frame.geometry.w && (layer->geometry.zoom || layer->cam_opts.manual_zoom);
			} else {
				can_pan = layer->cam_opts.manual_pan && (layer->geometry.zoom || layer->cam_opts.manual_zoom);
			}

			if (layer->cam_opts.autozoom) {
				can_zoom = layer->bug_frame.geometry.w;
			} else {
				can_zoom = layer->cam_opts.manual_zoom && layer->zoom_geometry.w;
			}

			//printf("CHECK %d %d,%d %d,%d %d/%d\n", layer->auto_geometry.w, 
			//	   layer->last_geometry.x, layer->last_geometry.y,
			//	   layer->auto_geometry.x, layer->auto_geometry.y,
			//	   abs(layer->auto_geometry.x - layer->last_geometry.x), 
			//	   abs(layer->auto_geometry.y - layer->last_geometry.y));

			if ((layer->cam_opts.autozoom || layer->cam_opts.autopan) &&
				(!layer->auto_geometry.w || 
				 (layer->last_geometry.x && abs(layer->auto_geometry.x - layer->last_geometry.x) > layer->cam_opts.zoom_move_factor) ||
				 (layer->last_geometry.y && abs(layer->auto_geometry.y - layer->last_geometry.y) > layer->cam_opts.zoom_move_factor) ||
				 (layer->last_geometry.w && abs(layer->auto_geometry.w - layer->last_geometry.w) > layer->cam_opts.zoom_move_factor / 2))) {
				
				layer->auto_geometry = layer->bug_frame.geometry;
			}

			if (can_zoom) {

				if (layer->cam_opts.autozoom) {
					use_geometry = &layer->auto_geometry;
				} else {
					use_geometry = &layer->zoom_geometry;
				}

				zoom_w = use_geometry->w * layer->cam_opts.zoom_factor;
				zoom_h = zoom_w / screen_aspect; 
				
				if (zoom_w < crop_w && zoom_h < crop_h) {
					int c_x = use_geometry->x;
					int c_y = use_geometry->y;
					
					crop_w = zoom_w;
					crop_h = zoom_h;
					
					//crop_w = switch_round_to_step(crop_w, layer->cam_opts.snap_factor);
					//crop_h = switch_round_to_step(crop_h, layer->cam_opts.snap_factor);

					if (layer->cam_opts.autozoom) {
						did_zoom = 1;
					}
					

					if (layer->cam_opts.autozoom) {
						c_x = switch_round_to_step(c_x, layer->cam_opts.snap_factor);
						c_y = switch_round_to_step(c_y, layer->cam_opts.snap_factor);
						
						crop_x = c_x - (crop_w / 2);
						crop_y = c_y - (crop_h / 2);
					} else {
						crop_x = c_x;
						crop_y = c_y;
					}
					
					set_bounds(&crop_x, &crop_y, img->w, img->h, crop_w, crop_h);

					//printf("ZOOM %d,%d %d,%d %dx%d\n", crop_x, crop_y, c_x, c_y, zoom_w, zoom_h);
				}
			}
				

			if (!did_zoom) {

				if (layer->cam_opts.autopan) {
					use_geometry = &layer->auto_geometry;
				} else {
					use_geometry = &layer->pan_geometry;
				}

				if (can_pan) {
					if (layer->cam_opts.autopan) {
						crop_x = use_geometry->x - (crop_w / 2);
					} else {
						crop_x = use_geometry->x;
					}
				} else if (screen_aspect <= img_aspect) {
					crop_x = img->w / 4;
				}

				if (can_pan) {
					if (layer->cam_opts.autopan) {
						crop_y = use_geometry->y - (crop_h / 2);
					} else {
						crop_y = use_geometry->y;
					}
				} else if (screen_aspect > img_aspect) {
					crop_y = img->h / 4;
				}

				crop_x = switch_round_to_step(crop_x, layer->cam_opts.snap_factor);
				crop_y = switch_round_to_step(crop_y, layer->cam_opts.snap_factor);
			}

			//printf("BOUNDS B4 %d,%d %dx%d %dx%d\n", crop_x, crop_y, img->d_w, img->d_h, crop_w, crop_h);
			set_bounds(&crop_x, &crop_y, img->w, img->h, crop_w, crop_h);
			//printf("BOUNDS AF %d,%d %dx%d %dx%d\n", crop_x, crop_y, img->d_w, img->d_h, crop_w, crop_h);
				
			if (img_changed) {
				layer->crop_x = crop_x;
				layer->crop_y = crop_y;
				layer->crop_w = crop_w;
				layer->crop_h = crop_h;
			}

			//printf("B4 %d,%d %d,%d\n", crop_x, crop_y, layer->crop_x, layer->crop_y);


			set_pan(crop_x, &layer->crop_x, layer->cam_opts.pan_accel_speed, layer->cam_opts.pan_accel_min, layer->cam_opts.pan_speed);
			set_pan(crop_y, &layer->crop_y, layer->cam_opts.pan_accel_speed, layer->cam_opts.pan_accel_min, layer->cam_opts.pan_speed);

			//printf("AF %d,%d\n", layer->crop_x, layer->crop_y);


			//printf("B4 %dx%d %dx%d\n", crop_w, crop_h, layer->crop_w, layer->crop_h);
			set_pan(crop_w, &layer->crop_w, layer->cam_opts.zoom_accel_speed, layer->cam_opts.zoom_accel_min, layer->cam_opts.zoom_speed);
			if (layer->crop_w > img->d_w) layer->crop_w = img->d_w;

			layer->crop_h = layer->crop_w / screen_aspect;
			if (layer->crop_h > img->d_h) layer->crop_h = img->d_h;

			set_bounds(&layer->crop_x, &layer->crop_y, img->w, img->h, layer->crop_w, layer->crop_h);

			assert(layer->crop_w > 0);

			//printf("RECT %d,%d %dx%d (%dx%d) [%dx%d] [%dx%d]\n", layer->crop_x, layer->crop_y, layer->crop_w, layer->crop_h, layer->crop_w + layer->crop_x, layer->crop_h + layer->crop_y, img->d_w, img->d_h, layer->screen_w, layer->screen_h);
			
			switch_img_set_rect(img, layer->crop_x, layer->crop_y, layer->crop_w, layer->crop_h);
			switch_assert(img->d_w == layer->crop_w);
				
			img_aspect = (double) img->d_w / img->d_h;

		}

		layer->last_geometry = layer->bug_frame.geometry;

		if (freeze) {
			switch_mutex_lock(layer->overlay_mutex);
			switch_img_free(&layer->img);
			switch_mutex_unlock(layer->overlay_mutex);
		}
		
		if (screen_aspect > img_aspect) {
			img_w = (uint32_t)ceil((double)img_aspect * layer->screen_h);
			x_pos += (layer->screen_w - img_w) / 2;
		} else if (screen_aspect < img_aspect) {
			img_h = (uint32_t)ceil((double)layer->screen_w / img_aspect);
			y_pos += (layer->screen_h - img_h) / 2;
		}

		if (layer->manual_border) {
			border = layer->manual_border;
		} if (layer->geometry.border) {
			border = layer->geometry.border;
		}
		
		if (layer->img) {
			if (layer->banner_img) {
				want_h = img_h - layer->banner_img->d_h;
			} else {
				want_h = img_h;
			}
			
			want_w = img_w - (border * 2);
			want_h -= (border * 2);
				
			if (layer->img->d_w != img_w || layer->img->d_h != img_h) {
				switch_img_free(&layer->img);
				conference_video_clear_layer(layer);
			}
		}

		switch_mutex_lock(layer->overlay_mutex);
		if (!layer->img) {
			layer->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, img_w, img_h, 1);
		}
		switch_mutex_unlock(layer->overlay_mutex);

		switch_assert(layer->img);

		if (border) {
			switch_img_fill(IMG, x_pos, y_pos, img_w, img_h, &layer->canvas->border_color);
		}

		//img_w -= (border * 2);
		//img_h -= (border * 2);

		//printf("SCALE %d,%d %dx%d\n", x_pos, y_pos, img_w, img_h);

		switch_img_scale(img, &layer->img, img_w, img_h);
		
		if (layer->logo_img) {
			//int ew = layer->screen_w - (border * 2), eh = layer->screen_h - (layer->banner_img ? layer->banner_img->d_h : 0) - (border * 2);
			int ew = layer->img->d_w - (border * 2), eh = layer->img->d_h - (border * 2);
			int ex = 0, ey = 0;

			switch_img_fit(&layer->logo_img, ew, eh, layer->logo_fit);

			switch_img_find_position(layer->logo_pos, ew, eh, layer->logo_img->d_w, layer->logo_img->d_h, &ex, &ey);
			
			switch_img_patch(layer->img, layer->logo_img, ex + border, ey + border);
			//switch_img_patch(IMG, layer->logo_img, layer->x_pos + ex + border, layer->y_pos + ey + border);
		}


		if (layer->banner_img && !layer->banner_patched) {
			int ew = layer->img->d_w, eh = layer->img->d_h;
			int ex = 0, ey = 0;

			switch_img_fit(&layer->banner_img, layer->screen_w, layer->screen_h, SWITCH_FIT_SIZE);
			switch_img_find_position(POS_LEFT_BOT, ew, eh, layer->banner_img->d_w, layer->banner_img->d_h, &ex, &ey);
			switch_img_patch(IMG, layer->banner_img, layer->x_pos + border,
							 layer->y_pos + (layer->screen_h - layer->banner_img->d_h) + border);
			layer->banner_patched = 1;
		}



		if (layer->img) {
			//switch_img_copy(img, &layer->img);

			switch_mutex_lock(layer->overlay_mutex);
			if (layer->overlay_img) {
				switch_img_fit(&layer->overlay_img, layer->img->d_w, layer->img->d_h, SWITCH_FIT_SCALE);

				if (layer->overlay_filters & SCV_FILTER_GRAY_FG) {
					switch_img_gray(layer->img, 0, 0, layer->img->d_w, layer->img->d_h);
				}

				if (layer->overlay_filters & SCV_FILTER_SEPIA_FG) {
					switch_img_sepia(layer->img, 0, 0, layer->img->d_w, layer->img->d_h);
				}

				if (layer->overlay_filters & SCV_FILTER_GRAY_BG) {
					switch_img_gray(layer->overlay_img, 0, 0, layer->overlay_img->d_w, layer->overlay_img->d_h);
				}

				if (layer->overlay_filters & SCV_FILTER_SEPIA_BG) {
					switch_img_sepia(layer->overlay_img, 0, 0, layer->overlay_img->d_w, layer->overlay_img->d_h);
				}

				switch_img_patch(layer->img, layer->overlay_img, 0, 0);
						 
			}
			switch_mutex_unlock(layer->overlay_mutex);
			
			switch_img_patch_rect(IMG, x_pos + border, y_pos + border, layer->img, 0, 0, want_w, want_h);

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

	conference_utils_member_clear_flag(member, MFLAG_DED_VID_LAYER);
	
	if (!(canvas = conference_video_get_canvas_locked(member))) {
		return;
	}

	switch_mutex_lock(canvas->mutex);

	if (member->video_layer_id < 0) {
		goto end;
	}

	if (member->id == member->conference->last_video_floor_holder) {
		if (conference_utils_member_test_flag(member, MFLAG_VIDEO_BRIDGE)) {
			conference_utils_set_flag(member->conference, CFLAG_VID_FLOOR_LOCK);
		}
	}

	layer = &canvas->layers[member->video_layer_id];

	if (layer->geometry.audio_position) {
		conference_api_sub_position(member, NULL, "0:0:0");
	}

	if (layer->txthandle) {
		switch_img_txt_handle_destroy(&layer->txthandle);
	}

	member->cam_opts = layer->cam_opts;
	
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

 end:

	switch_mutex_unlock(canvas->mutex);
	conference_video_release_canvas(&canvas);

}


void conference_video_layer_set_logo(conference_member_t *member, mcu_layer_t *layer)
{
	switch_mutex_lock(layer->canvas->mutex);

	switch_img_free(&layer->logo_img);

	switch_mutex_lock(member->flag_mutex);

	if (member->video_logo) {
		switch_img_copy(member->video_logo, &layer->logo_img);

		if (layer->logo_img) {
			layer->logo_pos = member->logo_pos;
			layer->logo_fit = member->logo_fit;
		}
	}

	switch_mutex_unlock(member->flag_mutex);

	switch_mutex_unlock(layer->canvas->mutex);
}

void conference_member_set_logo(conference_member_t *member, const char *path)
{
	const char *var = NULL;
	char *dup = NULL;
	switch_event_t *params = NULL;
	char *parsed = NULL;
	char *tmp;
	switch_img_position_t pos = POS_LEFT_TOP;
	switch_img_fit_t fit = SWITCH_FIT_SIZE;

	switch_mutex_lock(member->flag_mutex);
	switch_img_free(&member->video_logo);


	if (!path || !strcasecmp(path, "clear")) {
		switch_mutex_unlock(member->flag_mutex);
		return;
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



	if (path) {
		member->video_logo = switch_img_read_png(path, SWITCH_IMG_FMT_ARGB);

		if (member->video_logo) {
			member->logo_pos = pos;
			member->logo_fit = fit;

			if (params && (var = switch_event_get_header(params, "text"))) {
				switch_image_t *img = NULL;
				const char *tmp;
				int x = 0, y = 0, center = 0, center_off = 0;

				if ((tmp = switch_event_get_header(params, "center_offset"))) {
					center_off = atoi(tmp);
					if (center_off < 0) {
						center_off = 0;
					}
				}
				
				if ((tmp = switch_event_get_header(params, "text_x"))) {
					if (!strcasecmp(tmp, "center")) {
						center = 1;
					} else {
						x = atoi(tmp);
						if (x < 0) x = 0;
					}
				}

				if ((tmp = switch_event_get_header(params, "text_y"))) {
					y = atoi(tmp);
					if (y < 0) y = 0;
				}

				if ((img = switch_img_write_text_img(member->video_logo->d_w, member->video_logo->d_h, SWITCH_FALSE, var))) {
					switch_img_fit(&img, member->video_logo->d_w, member->video_logo->d_h, SWITCH_FIT_NECESSARY);
					switch_img_attenuate(member->video_logo);


					if (center) {
						x = center_off + ((member->video_logo->d_w - center_off - img->d_w) / 2);
					}

					switch_img_patch(member->video_logo, img, x, y);
					switch_img_free(&img);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to write text on image!\n");
				}
			}

			if (params && (var = switch_event_get_header(params, "alt_text"))) {
				switch_image_t *img = NULL;
				const char *tmp;
				int x = 0, y = 0, center = 0, center_off = 0;

				if ((tmp = switch_event_get_header(params, "alt_center_offset"))) {
					center_off = atoi(tmp);
					if (center_off < 0) {
						center_off = 0;
					}
				}
				
				if ((tmp = switch_event_get_header(params, "alt_text_x"))) {
					if (!strcasecmp(tmp, "center")) {
						center = 1;
					} else {
						x = atoi(tmp);
						if (x < 0) x = 0;
					}
				}

				if ((tmp = switch_event_get_header(params, "alt_text_y"))) {
					y = atoi(tmp);
					if (y < 0) y = 0;
				}
				
				if ((img = switch_img_write_text_img(member->video_logo->d_w, member->video_logo->d_h, SWITCH_FALSE, var))) {
					switch_img_fit(&img, member->video_logo->d_w, member->video_logo->d_h, SWITCH_FIT_NECESSARY);
					switch_img_attenuate(member->video_logo);
					
					if (center) {
						x = center_off + ((member->video_logo->d_w - center_off - img->d_w) / 2);
					}

					switch_img_patch(member->video_logo, img, x, y);
					switch_img_free(&img);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to write text on image!\n");
				}
						 
			}
			
		}
	}

	if (params) switch_event_destroy(&params);

	switch_safe_free(dup);

	switch_mutex_unlock(member->flag_mutex);

	return;
}

void conference_video_layer_set_banner(conference_member_t *member, mcu_layer_t *layer, const char *text)
{
	switch_rgb_color_t fgcolor, bgcolor;
	float font_scale = 1;
	uint16_t min_font_size = 5, max_font_size = 24, font_size = 0;
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

		if ((var = switch_event_get_header(params, "min_font_size"))) {
			int tmp = atoi(var);
			if (tmp >= min_font_size && tmp <= max_font_size) {
				min_font_size = tmp;
			}
		}

		if ((var = switch_event_get_header(params, "max_font_size"))) {
			int tmp = atoi(var);
			if (tmp >= min_font_size && tmp <= max_font_size) {
				max_font_size = tmp;
			}
		}

		if ((var = switch_event_get_header(params, "font_scale"))) {
			float tmp = atof(var);

			if (tmp >= 0 && tmp <= 50) {
				font_scale = tmp;
			}
		}
	}

	if (!text) text = "N/A";

	font_size =  (uint16_t)(((double)layer->screen_w / ((double)strlen(text) / 1.2f)) * font_scale);

	if (font_size <= min_font_size) font_size = min_font_size;
	if (font_size >= max_font_size) font_size = max_font_size;

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
		conference_utils_member_clear_flag(member, MFLAG_DED_VID_LAYER);
		return SWITCH_STATUS_FALSE;
	}

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		conference_utils_member_clear_flag(member, MFLAG_DED_VID_LAYER);
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_channel_test_flag(channel, CF_VIDEO_READY) && !member->avatar_png_img) {
		conference_utils_member_clear_flag(member, MFLAG_DED_VID_LAYER);
		return SWITCH_STATUS_FALSE;
	}

	if ((switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY || switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) && !member->avatar_png_img) {
		conference_utils_member_clear_flag(member, MFLAG_DED_VID_LAYER);
		return SWITCH_STATUS_FALSE;
	}

	

	switch_mutex_lock(canvas->mutex);

	layer = &canvas->layers[idx];

	layer->tagged = 0;

	if (!zstr(member->video_role_id) && !zstr(layer->geometry.role_id) && !strcmp(layer->geometry.role_id, member->video_role_id)) {
		conference_utils_member_set_flag(member, MFLAG_DED_VID_LAYER);
	}

	if (conference_utils_member_test_flag(member, MFLAG_DED_VID_LAYER)) {
		if (member->id == member->conference->floor_holder) {
			conference_member_set_floor_holder(member->conference, NULL, 0);
		}
	}
	
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

	conference_video_layer_set_logo(member, layer);

	layer->member_id = member->id;
	layer->member = member;
	member->video_layer_id = idx;
	member->canvas_id = canvas->canvas_id;
	member->layer_timeout = DEFAULT_LAYER_TIMEOUT;
	conference_utils_member_set_flag_locked(member, MFLAG_VIDEO_JOIN);
	switch_channel_set_flag(member->channel, CF_VIDEO_REFRESH_REQ);
	layer->manual_border = member->video_manual_border;
	canvas->send_keyframe = 30;

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

void conference_video_init_canvas_layers(conference_obj_t *conference, mcu_canvas_t *canvas, video_layout_t *vlayout, switch_bool_t force)
{
	int i = 0;

	if (!canvas) return;


	switch_mutex_lock(canvas->mutex);
	switch_mutex_lock(canvas->write_mutex);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		mcu_layer_t *layer = &canvas->layers[i];
		if (!layer->overlay_mutex) {
			switch_mutex_init(&layer->overlay_mutex, SWITCH_MUTEX_NESTED, canvas->pool);
		}
	}

	if (canvas->vlayout && canvas->vlayout->transition_out) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play transition out [%s]\n", canvas->vlayout->transition_out);
		conference_file_play(conference, canvas->vlayout->transition_out, 0, NULL, 0);
	}

	if (vlayout && canvas->vlayout == vlayout && !force) {
		switch_mutex_unlock(canvas->write_mutex);
		switch_mutex_unlock(canvas->mutex);
		return;
	}

	canvas->layout_floor_id = -1;

	if (!vlayout) {
		vlayout = canvas->new_vlayout;
		canvas->new_vlayout = NULL;
	}

	if (!vlayout) {
		switch_mutex_unlock(canvas->write_mutex);
		switch_mutex_unlock(canvas->mutex);
		return;
	}

	canvas->vlayout = vlayout;

	canvas->res_count = 0;
	canvas->role_count = 0;

	for (i = 0; i < vlayout->layers; i++) {
		mcu_layer_t *layer = &canvas->layers[i];

		conference_video_reset_layer(layer);

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
		layer->geometry.flooronly = vlayout->images[i].flooronly;
		layer->geometry.fileonly = vlayout->images[i].fileonly;
		layer->geometry.overlap = vlayout->images[i].overlap;
		layer->idx = i;
		layer->refresh = 1;

		layer->screen_w = (uint32_t)(canvas->img->d_w * layer->geometry.scale / VIDEO_LAYOUT_SCALE);
		layer->screen_h = (uint32_t)(canvas->img->d_h * layer->geometry.hscale / VIDEO_LAYOUT_SCALE);

		// if (layer->screen_w % 2) layer->screen_w++; // round to even
		// if (layer->screen_h % 2) layer->screen_h++; // round to even

		layer->x_pos = (int)(canvas->img->d_w * layer->geometry.x / VIDEO_LAYOUT_SCALE);
		layer->y_pos = (int)(canvas->img->d_h * layer->geometry.y / VIDEO_LAYOUT_SCALE);

		set_default_cam_opts(layer);


		if (layer->geometry.floor) {
			canvas->layout_floor_id = i;
		}
		
		/* if we ever decided to reload layers config on demand the pointer assignment below  will lead to segs but we
		   only load them once forever per conference so these pointers are valid for the life of the conference */

		if ((layer->geometry.res_id = vlayout->images[i].res_id)) {
			canvas->res_count++;
		}

		if ((layer->geometry.role_id = vlayout->images[i].role_id)) {
			canvas->role_count++;
		}

		layer->geometry.audio_position = vlayout->images[i].audio_position;
	}

	conference_video_reset_image(canvas->img, &canvas->bgcolor);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		mcu_layer_t *layer = &canvas->layers[i];

		if (layer->member) {
			conference_video_clear_managed_kps(layer->member);
			layer->member->video_layer_id = -1;

			conference_video_detach_video_layer(layer->member);
			
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

	if (vlayout->fgimg) {
		conference_video_set_canvas_fgimg(canvas, vlayout->fgimg);
	} else if (canvas->fgimg) {
		switch_img_free(&canvas->fgimg);
	}

	if (conference->video_canvas_bgimg && !vlayout->bgimg) {
		conference_video_set_canvas_bgimg(canvas, conference->video_canvas_bgimg);
	}

	switch_mutex_lock(conference->file_mutex);
	if (conference->fnode && (conference->fnode->canvas_id == canvas->canvas_id || conference->fnode->canvas_id == -1)) {
		conference_video_canvas_del_fnode_layer(conference, conference->fnode);
		conference_video_fnode_check(conference->fnode, canvas->canvas_id);
	}
	switch_mutex_unlock(conference->file_mutex);

	switch_mutex_unlock(canvas->write_mutex);
	switch_mutex_unlock(canvas->mutex);

	conference_event_adv_layout(conference, canvas, vlayout);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Canvas position %d applied layout %s\n", canvas->canvas_id + 1, vlayout->name);

	if (vlayout->transition_in) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play transition in [%s]\n", vlayout->transition_in);
		conference_file_play(conference, vlayout->transition_in, 0, NULL, 0);
	}
}

switch_status_t conference_video_set_canvas_bgimg(mcu_canvas_t *canvas, const char *img_path)
{

	int x = 0, y = 0, scaled = 0, i = 0;

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

	for (i = 0; i < canvas->total_layers; i++) {
		canvas->layers[i].banner_patched = 0;
		canvas->layers[i].mute_patched = 0;
	}

	return SWITCH_STATUS_SUCCESS;
}


switch_status_t conference_video_set_canvas_fgimg(mcu_canvas_t *canvas, const char *img_path)
{

	int x = 0, y = 0, scaled = 0;

	if (img_path) {
		switch_img_free(&canvas->fgimg);
		canvas->fgimg = switch_img_read_png(img_path, SWITCH_IMG_FMT_ARGB);
	} else {
		scaled = 1;
	}

	if (!canvas->fgimg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot open image for fgimg\n");
		return SWITCH_STATUS_FALSE;
	}

	if (!scaled) {
		switch_img_fit(&canvas->fgimg, canvas->img->d_w, canvas->img->d_h, SWITCH_FIT_SIZE);
	}
	switch_img_find_position(POS_CENTER_MID, canvas->img->d_w, canvas->img->d_h, canvas->fgimg->d_w, canvas->fgimg->d_h, &x, &y);
	switch_img_patch(canvas->img, canvas->fgimg, x, y);

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
	switch_mutex_init(&canvas->write_mutex, SWITCH_MUTEX_NESTED, conference->pool);
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
	conference_video_set_canvas_border_color(canvas, conference->video_border_color);
	conference_video_init_canvas_layers(conference, canvas, vlayout, SWITCH_TRUE);
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

	switch_mutex_lock(canvas->mutex);
	switch_img_free(&canvas->img);
	switch_img_free(&canvas->bgimg);
	switch_img_free(&canvas->fgimg);
	conference_video_flush_queue(canvas->video_queue, 0);

	for (i = 0; i < MCU_MAX_LAYERS; i++) {
		mcu_layer_t *layer = &canvas->layers[i];
		switch_mutex_lock(layer->overlay_mutex);
		switch_img_free(&layer->img);
		switch_mutex_unlock(layer->overlay_mutex);
	}
	switch_mutex_unlock(canvas->mutex);

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
				switch_set_flag(frame, SFF_RAW_RTP_PARSE_FRAME|SFF_USE_VIDEO_TIMESTAMP);
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

				if (codec_set->video_codec_group && (!imember->video_codec_group || strcmp(codec_set->video_codec_group, imember->video_codec_group))) {
					continue;
				}

				if (imember->video_codec_index != codec_index) {
					continue;
				}
				
				if (conference_utils_member_test_flag(imember, MFLAG_VIDEO_JOIN) && !send_keyframe) {
					continue;
				}

				conference_utils_member_clear_flag(imember, MFLAG_VIDEO_JOIN);
				
				if (!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ||
					switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS) {
					continue;
				}

				if (need_refresh) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY || 
					!conference_utils_member_test_flag(imember, MFLAG_CAN_SEE) ||
					switch_channel_test_flag(imember->channel, CF_VIDEO_WRITING) ||
					switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) {
					switch_core_session_rwunlock(imember->session);
					continue;
				}

				//switch_core_session_write_encoded_video_frame(imember->session, frame, 0, 0);
				switch_set_flag(frame, SFF_ENCODED);

				if (switch_frame_buffer_dup(imember->fb, frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					if (switch_frame_buffer_trypush(imember->fb, dupframe) != SWITCH_STATUS_SUCCESS) {
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

video_layout_t *conference_video_find_best_layout(conference_obj_t *conference, layout_group_t *lg, uint32_t count, uint32_t file_count)
{
	video_layout_node_t *vlnode = NULL, *last = NULL, *least = NULL;

	if (count == 1 && file_count == 1) file_count = 0;

	if (!count) {
		count = conference->members_with_video;
		file_count = 0;

		if (!conference_utils_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
			count += conference->members_with_avatar;
		}
	}

	if (!lg) {
		return NULL;
	}

	for (vlnode = lg->layouts; vlnode; vlnode = vlnode->next) {
		int x, file_layers = 0, member_count = (int)count, total = vlnode->vlayout->layers;


		for (x = total; x >= 0; x--) {
			if (vlnode->vlayout->images[x].fileonly) {
				file_layers++;
			}
		}
		
		if ((vlnode->vlayout->layers - file_layers >= member_count && file_layers >= file_count)) {
			break;
		}

		if (vlnode->vlayout->layers - file_layers >= (int)count + file_count) {
			if (!least || least->vlayout->layers > vlnode->vlayout->layers) {
				least = vlnode;
			}
		}

		last = vlnode;
	}

	if (least) {
		vlnode = least;
	}

	return vlnode? vlnode->vlayout : last ? last->vlayout : NULL;
}

video_layout_t *conference_video_get_layout(conference_obj_t *conference, const char *video_layout_name, const char *video_layout_group)
{
	layout_group_t *lg = NULL;
	video_layout_t *vlayout = NULL;

	if (!video_layout_name) return NULL;

	if (video_layout_group) {
		lg = switch_core_hash_find(conference->layout_group_hash, video_layout_group);
		vlayout = conference_video_find_best_layout(conference, lg, 0, 0);
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

		switch_mutex_lock(xlayer->overlay_mutex);
		switch_img_free(&xlayer->overlay_img);
		if (fnode->layer_lock < 0) {
			conference_video_reset_layer(xlayer);
		}
		switch_mutex_unlock(xlayer->overlay_mutex);
	}
	switch_mutex_unlock(canvas->mutex);
}

void conference_video_canvas_set_fnode_layer(mcu_canvas_t *canvas, conference_file_node_t *fnode, int idx)
{
	mcu_layer_t *layer = NULL;
	mcu_layer_t *xlayer = NULL;

	switch_mutex_lock(canvas->mutex);

	if (fnode->layer_lock > -1) {
		layer = &canvas->layers[fnode->layer_lock];
		layer->fnode = fnode;
		fnode->layer_id = fnode->layer_lock;
		fnode->canvas_id = canvas->canvas_id;
		goto end;
	}

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

	if (layer->member_id > 0) {
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
		//switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
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
		//switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		conference_utils_set_flag(conference, CFLAG_VIDEO_MUXING);
		switch_thread_create(&canvas->video_muxing_thread, thd_attr,
							 super ? conference_video_super_muxing_thread_run : conference_video_muxing_thread_run, canvas, conference->pool);
	}
	switch_mutex_unlock(conference_globals.hash_mutex);
}

void *SWITCH_THREAD_FUNC conference_video_layer_thread_run(switch_thread_t *thread, void *obj)
{
	conference_member_t *member = (conference_member_t *) obj;

	if (switch_thread_rwlock_tryrdlock(member->rwlock) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	//switch_core_autobind_cpu();
	member->layer_thread_running = 1;

	switch_mutex_lock(member->layer_cond_mutex);
	
	while(conference_utils_member_test_flag(member, MFLAG_RUNNING) && member->layer_thread_running) {
		mcu_layer_t *layer = NULL;
		mcu_canvas_t *canvas = NULL;
		

		switch_thread_cond_wait(member->layer_cond, member->layer_cond_mutex);

		if (!conference_utils_member_test_flag(member, MFLAG_RUNNING)) {
			break;
		}


		if (member->video_layer_id > -1 && member->canvas_id > -1) {
			canvas = member->conference->canvases[member->canvas_id];
			layer = &canvas->layers[member->video_layer_id];
		}

		if (layer) {
			if (layer->need_patch) {
				conference_video_scale_and_patch(layer, NULL, SWITCH_FALSE);
				layer->need_patch = 0;
			}
		}
	}

	switch_mutex_unlock(member->layer_cond_mutex);

	member->layer_thread_running = 0;

	switch_thread_rwlock_unlock(member->rwlock);

	return NULL;
}

void conference_video_wake_layer_thread(conference_member_t *member)
{
	if (member->layer_cond) {
		if (switch_mutex_trylock(member->layer_cond_mutex) == SWITCH_STATUS_SUCCESS) {
			switch_thread_cond_signal(member->layer_cond);
			switch_mutex_unlock(member->layer_cond_mutex);
		}
		
	}
}
	

void conference_video_launch_layer_thread(conference_member_t *member)
{
	switch_threadattr_t *thd_attr = NULL;

	if (switch_core_cpu_count() < 3) {
		return;
	}

	if (!member->layer_cond) {
		switch_thread_cond_create(&member->layer_cond, member->pool);
		switch_mutex_init(&member->layer_cond_mutex, SWITCH_MUTEX_NESTED, member->pool);
	}

	switch_mutex_lock(conference_globals.hash_mutex);
	if (!member->video_layer_thread) {
		switch_threadattr_create(&thd_attr, member->pool);
		//switch_threadattr_priority_set(thd_attr, SWITCH_PRI_REALTIME);
		switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
		switch_thread_create(&member->video_layer_thread, thd_attr, conference_video_layer_thread_run, member, member->pool);
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
	switch_status_t pop_status;

	if (switch_thread_rwlock_tryrdlock(member->rwlock) != SWITCH_STATUS_SUCCESS) {
		return NULL;
	}

	//switch_core_autobind_cpu();

	while(conference_utils_member_test_flag(member, MFLAG_RUNNING)) {

		pop_status = switch_frame_buffer_pop(member->fb, &pop);

		if (pop_status == SWITCH_STATUS_SUCCESS) {

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
		}
	}

	while (switch_frame_buffer_trypop(member->fb, &pop) == SWITCH_STATUS_SUCCESS) {
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

void conference_video_member_video_mute_banner(switch_image_t *img, conference_member_t *member)
{
	const char *text = "VIDEO MUTED";
	char *dup = NULL;
	const char *var, *tmp = NULL;
	const char *fg = "";
	const char *bg = "";
	const char *font_face = "";
	const char *font_scale = "";
	const char *font_scale_percentage = "";
	char *parsed = NULL;
	switch_event_t *params = NULL;
	switch_image_t *text_img;
	char text_str[256] = "";

	if ((var = switch_channel_get_variable_dup(member->channel, "video_mute_banner", SWITCH_FALSE, -1))) {
		text = var;
	} else if (member->conference->video_mute_banner) {
		text = member->conference->video_mute_banner;
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
			font_scale = var;
			font_scale_percentage = "%";
		}
	}

	switch_snprintf(text_str, sizeof(text_str), "%s:%s:%s:%s%s:%s", fg, bg, font_face, font_scale, font_scale_percentage, text);
	text_img = switch_img_write_text_img(img->d_w, img->d_h, SWITCH_TRUE, text_str);
	switch_img_patch(img, text_img, 0, 0);
	switch_img_free(&text_img);

	if (params) switch_event_destroy(&params);

	switch_safe_free(dup);
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

		if (!conference_utils_test_flag(conference, CFLAG_PERSONAL_CANVAS) && canvas && imember->canvas_id != canvas->canvas_id) {
			continue;
		}

		if (switch_test_flag((&imember->rec->fh), SWITCH_FILE_OPEN) && !switch_test_flag((&imember->rec->fh), SWITCH_FILE_PAUSE) && 
			switch_core_file_has_video(&imember->rec->fh, SWITCH_TRUE)) {
			if (switch_core_file_write_video(&imember->rec->fh, frame) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Video Write Failed\n");
				conference_utils_member_clear_flag_locked(imember, MFLAG_RUNNING);
			}
		}
	}

	switch_mutex_unlock(conference->member_mutex);

}

void conference_video_check_avatar(conference_member_t *member, switch_bool_t force)
{
	const char *avatar = NULL, *var = NULL;
	mcu_canvas_t *canvas;
	int novid = 0;
    switch_event_t *event;

	if (member->canvas_id < 0) {
		return;
	}

	if (conference_utils_member_test_flag(member, MFLAG_SECOND_SCREEN)) {
		return;
	}

	canvas = conference_video_get_canvas_locked(member);

	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS) &&
		(!switch_channel_test_flag(member->channel, CF_VIDEO_READY) ||
		 (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY  ||
		  switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE))) {

		if (canvas) {
			conference_video_release_canvas(&canvas);
		}
		return;
	}

	if (canvas) {
		switch_mutex_lock(canvas->mutex);
	}

	member->avatar_patched = 0;

	if (!force && switch_channel_test_flag(member->channel, CF_VIDEO_READY) &&
		switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY && switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_INACTIVE) {
		conference_utils_member_set_flag_locked(member, MFLAG_ACK_VIDEO);
		switch_core_session_request_video_refresh(member->session);
		conference_video_check_flush(member, SWITCH_TRUE);
	} else {
		if (member->conference->no_video_avatar) {
			avatar = member->conference->no_video_avatar;
		}

		if ((var = switch_channel_get_variable_dup(member->channel, "video_no_video_avatar_png", SWITCH_FALSE, -1))) {
			avatar = var;
		}
		novid++;
	}

	if ((var = switch_channel_get_variable_dup(member->channel, "video_avatar_png", SWITCH_FALSE, -1))) {
		avatar = var;
	}

	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS) || conference_utils_test_flag(member->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS)) {
		avatar = NULL;
		force = 0;
	}
	
	switch_mutex_lock(member->flag_mutex);
	switch_img_free(&member->avatar_png_img);


	if (avatar) {
		member->avatar_png_img = switch_img_read_png(avatar, SWITCH_IMG_FMT_I420);
	}

	if (force && !member->avatar_png_img && member->video_mute_img) {
        switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, CONF_EVENT_MAINT);
        if (member->conference) {
            conference_event_add_data(member->conference, event);
        }       
        conference_member_add_event_data(member, event);
        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Action", "vfi-triggered-member");
        switch_event_fire(&event);

		switch_img_copy(member->video_mute_img, &member->avatar_png_img);
	}

	if (member->avatar_png_img && novid) {
		member->auto_avatar = 1;
	}

	switch_mutex_unlock(member->flag_mutex);

	if (canvas) {
		switch_mutex_unlock(canvas->mutex);
		conference_video_release_canvas(&canvas);
	}
}

void conference_video_check_flush(conference_member_t *member, switch_bool_t force)
{
	int flushed;

	if (!member->channel || !switch_channel_test_flag(member->channel, CF_VIDEO)) {
		return;
	}

	flushed = conference_video_flush_queue(member->video_queue, 1);

	if ((flushed || force) && member->auto_avatar) {
		switch_channel_video_sync(member->channel);

		switch_mutex_lock(member->flag_mutex);
		switch_img_free(&member->avatar_png_img);
		switch_mutex_unlock(member->flag_mutex);
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
			if (fnode->layer_lock > -1 && layer->member_id > 0) {
				switch_mutex_lock(layer->overlay_mutex);
				switch_img_free(&layer->overlay_img);
				layer->overlay_img = file_frame.img;
				layer->overlay_filters = fnode->filters;
				switch_mutex_unlock(layer->overlay_mutex);
			} else {
				switch_img_free(&layer->cur_img);
				if (file_frame.img && file_frame.img->fmt != SWITCH_IMG_FMT_I420) {
					switch_image_t *tmp = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, file_frame.img->d_w, file_frame.img->d_h, 1);
					switch_img_copy(file_frame.img, &tmp);
					switch_img_free(&file_frame.img);
					file_frame.img = tmp;
				}
				layer->cur_img = file_frame.img;
			}

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
			canvas->send_keyframe = 1;
			if (canvas->play_file == 0) {
				canvas->play_file = 1;
			}
			if (fnode->fh.mm.fmt == SWITCH_IMG_FMT_ARGB) {
				canvas->overlay_video_file = 1;
			} else {
				canvas->playing_video_file = 1;
			}
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
	mcu_layer_t *xlayer;
	int i;

	if (conference_utils_test_flag(conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS) &&
		!conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN)) {
		return SWITCH_STATUS_FALSE;
	}

	if (conference_utils_member_test_flag(member, MFLAG_HOLD)) {
		return SWITCH_STATUS_FALSE;
	}
	
	switch_mutex_lock(canvas->mutex);

	for (i = 0; i < canvas->total_layers; i++) {
		xlayer = &canvas->layers[i];

		if (xlayer->is_avatar && xlayer->member_id != (int)conference->video_floor_holder) {
			avatar_layers++;
		}
	}

	if (!layer &&
		(canvas->layers_used < canvas->total_layers ||
		 (avatar_layers && !member->avatar_png_img) || conference_utils_member_test_flag(member, MFLAG_MOD)) &&
		(member->avatar_png_img || (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY &&
		  switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_INACTIVE))) {

		/* find an empty layer */
		for (i = 0; i < canvas->total_layers; i++) {
			xlayer = &canvas->layers[i];

			if (xlayer->geometry.res_id) {
				if (member->video_reservation_id && !strcmp(xlayer->geometry.res_id, member->video_reservation_id)) {
					layer = xlayer;
					conference_utils_member_set_flag(member, MFLAG_DED_VID_LAYER);
					conference_video_attach_video_layer(member, canvas, i);
					break;
				}
			}
		}

		if (!layer) {
			for (i = 0; i < canvas->total_layers; i++) {
				xlayer = &canvas->layers[i];
				
				if (xlayer->geometry.flooronly && !xlayer->fnode && !xlayer->geometry.fileonly && !xlayer->geometry.res_id) {
					if (member->id == conference->video_floor_holder) {
						layer = xlayer;
						conference_video_attach_video_layer(member, canvas, i);
						break;
					}
				}
			}
		}

		if (!layer) {
			for (i = 0; i < canvas->total_layers; i++) {
				xlayer = &canvas->layers[i];
				

				if ((!xlayer->member_id || (!member->avatar_png_img &&
											xlayer->is_avatar && !xlayer->geometry.role_id &&
											(conference->canvas_count > 1 || xlayer->member_id != (int)conference->video_floor_holder))) &&
					!xlayer->fnode && !xlayer->geometry.fileonly && !xlayer->geometry.res_id && !xlayer->geometry.flooronly) {
					switch_status_t lstatus = conference_video_attach_video_layer(member, canvas, i);

					if (lstatus == SWITCH_STATUS_SUCCESS || lstatus == SWITCH_STATUS_BREAK) {						
						layer = xlayer;
						break;
					}
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
	int x = 0, y = 0;

	if (imember->conference->canvas_count < 2) {
		return;
	}
	
	y = imember->canvas_id;

	for (x = 0; x < imember->conference->canvas_count; x++) {
		if (y == (int)imember->conference->canvas_count - 1) {
			y = 0;
		} else {
			y++;
		}

		if (imember->conference->canvases[y]->video_count < imember->conference->canvases[y]->total_layers) {
			imember->canvas_id = y;
			break;
		}
	}

	imember->layer_timeout = DEFAULT_LAYER_TIMEOUT;
}

void conference_video_pop_next_image(conference_member_t *member, switch_image_t **imgP)
{
	switch_image_t *img = *imgP;
	int size = 0;
	void *pop;
	//if (member->avatar_png_img && switch_channel_test_flag(member->channel, CF_VIDEO_READY) && conference_utils_member_test_flag(member, MFLAG_ACK_VIDEO)) {
	//	switch_img_free(&member->avatar_png_img);
	//}


	if (switch_channel_test_flag(member->channel, CF_VIDEO_READY)) {
		do {
			pop = NULL;
			if (switch_queue_trypop(member->video_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
				switch_img_free(&img);
				img = (switch_image_t *)pop;
				member->blanks = 0;
			} else {
				break;
			}
			size = switch_queue_size(member->video_queue);
		} while(size > 1);

		if (conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) && !conference_utils_member_test_flag(member, MFLAG_HOLD) &&
			member->video_layer_id > -1 &&
			switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY &&
			switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_INACTIVE
			) {
			switch_vid_params_t vid_params = { 0 };

			switch_core_media_get_vid_params(member->session, &vid_params);

			if (!vid_params.fps) {
				vid_params.fps = member->conference->video_fps.fps;
			}

			if (img) {
				member->good_img++;
				if ((member->good_img % (int)(vid_params.fps * 10)) == 0) {
					conference_video_reset_video_bitrate_counters(member);
				}

				if (member->auto_avatar && member->good_img > 1) {
					conference_video_check_flush(member, SWITCH_TRUE);
				}

			} else if (!conference_utils_member_test_flag(member, MFLAG_NO_VIDEO_BLANKS)) {
				member->blanks++;


				if (member->blanks == member->conference->video_fps.fps || (member->blanks % (int)(member->conference->video_fps.fps * 10)) == 0) {
					switch_core_session_request_video_refresh(member->session);
					member->good_img = 0;
				}

				if (member->blanks == member->conference->video_fps.fps * 5) {
					member->blackouts++;
					conference_video_check_avatar(member, SWITCH_TRUE);
					conference_video_clear_managed_kps(member);
				}
			}
		}
	} else {
		conference_video_check_flush(member, SWITCH_FALSE);
	}

	if (img) {
		if (member->video_filters & SCV_FILTER_GRAY_FG) {
			switch_img_gray(img, 0, 0, img->d_w, img->d_h);
		}

		if (member->video_filters & SCV_FILTER_SEPIA_FG) {
			switch_img_sepia(img, 0, 0, img->d_w, img->d_h);
		}

		if (member->video_filters & SCV_FILTER_8BIT_FG) {
			switch_image_t *tmp = NULL;
			int w = img->d_w, h = img->d_h;

			switch_img_scale(img, &tmp, w/8 ,h/8);
			switch_img_scale(tmp, &img, w,h);
			switch_img_8bit(img);
		}
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

	if (member->managed_kps_set == kps) {
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s sending message to set bitrate to %dkps\n",
					  switch_channel_get_name(member->channel), kps);

	msg.message_id = SWITCH_MESSAGE_INDICATE_BITRATE_REQ;
	msg.numeric_arg = kps * 1024;
	msg.from = __FILE__;

	switch_core_session_receive_message(member->session, &msg);

	member->managed_kps_set = kps;
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
	int screen_w = 0, screen_h = 0;

	if (layer) {
		screen_w = layer->screen_w;
		screen_h = layer->screen_h;
	}
	

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

	if (member->vid_params.width && member->vid_params.height && (screen_w > member->vid_params.width || screen_h > member->vid_params.height)) {
		//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s Layer is bigger than input res, limit size to %dx%d\n",
		//switch_channel_get_name(member->channel), member->vid_params.width, member->vid_params.height);
		screen_w = member->vid_params.width;
		screen_h = member->vid_params.height;
	}
	
	if (member->managed_kps_set) {
		return;
	}

	if ((kps_in = switch_calc_bitrate(vid_params.width, vid_params.height,
									  member->conference->video_quality, (int)(member->conference->video_fps.fps))) < 512) {
		kps_in = 512;
	}

	if (layer) {
		kps = switch_calc_bitrate(screen_w, screen_h, member->conference->video_quality, (int)(member->conference->video_fps.fps));
	} else {
		kps = kps_in;
	}

	min_layer = kps / 8;
	min = kps_in / 8;

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
		if (layer && conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) && !conference_utils_member_test_flag(member, MFLAG_HOLD)) {
			if (layer->screen_w != screen_w) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps (max res %dx%d) to accommodate %dx%d resolution\n",
								  switch_channel_get_name(member->channel), kps, screen_w, screen_h, layer->screen_w, layer->screen_h);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "%s auto-setting bitrate to %dkps to accommodate %dx%d resolution\n",
								  switch_channel_get_name(member->channel), kps, screen_w, screen_h);
			}
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
	switch_mutex_lock(canvas->write_mutex);
	for (;;) {
		int x = 0;
		int i = 0;

		for (i = 0; i < canvas->total_layers; i++) {
			mcu_layer_t *layer = &canvas->layers[i];

			if (layer->need_patch) {
				if (layer->member_id && layer->member && conference_utils_member_test_flag(layer->member, MFLAG_RUNNING) && layer->member->fb) {
					conference_video_wake_layer_thread(layer->member);
					x++;
				} else {
					layer->need_patch = 0;
				}
			}
		}

		if (!x) break;

		switch_cond_next();
	}
	switch_mutex_unlock(canvas->write_mutex);
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

		if (member->channel) {
			var = NULL;
			if (member->video_banner_text ||
				(var = switch_channel_get_variable_dup(member->channel, "video_banner_text", SWITCH_FALSE, -1))) {
				conference_video_layer_set_banner(member, layer, var);
			}

			conference_video_layer_set_logo(member, layer);
		}
	}

	layer->member_id = member->id;
}

switch_status_t conference_video_change_res(conference_obj_t *conference, int w, int h, int id)
{
	mcu_canvas_t *canvas = NULL;

	switch_mutex_lock(conference->canvas_mutex);
	canvas = conference->canvases[id];
	switch_mutex_lock(canvas->mutex);
	canvas->width = w;
	canvas->height = h;
	switch_img_free(&canvas->img);
	canvas->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, canvas->width, canvas->height, 0);
	conference_video_init_canvas_layers(conference, canvas, canvas->vlayout, SWITCH_TRUE);
	switch_mutex_unlock(canvas->mutex);
	switch_mutex_unlock(conference->canvas_mutex);

	return SWITCH_STATUS_SUCCESS;
}


void *SWITCH_THREAD_FUNC conference_video_muxing_thread_run(switch_thread_t *thread, void *obj)
{
	mcu_canvas_t *canvas = (mcu_canvas_t *) obj;
	conference_obj_t *conference = canvas->conference;
	conference_member_t *imember;
	switch_codec_t *check_codec = NULL;
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	int i = 0;
	uint32_t video_key_freq = 0;
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
	int last_video_count = 0;
	int watchers = 0, last_watchers = 0;

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
				conference_video_init_canvas_layers(conference, canvas, NULL, SWITCH_TRUE);
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

		switch_mutex_lock(conference->file_mutex);
		if (conference->async_fnode && switch_core_file_has_video(&conference->async_fnode->fh, SWITCH_TRUE)) {
			check_async_file = 1;
			file_count++;
			video_count++;
			if (!files_playing) {
				send_keyframe = 1;
			}
			files_playing = 1;
		}

		if (conference->fnode && switch_core_file_has_video(&conference->fnode->fh, SWITCH_TRUE)) {
			check_file = 1;
			file_count++;
			video_count++;
			if (!files_playing) {
				send_keyframe = 1;
			}
			files_playing = 1;
		}
		switch_mutex_unlock(conference->file_mutex);

		if (conference_utils_test_flag(conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
			if (conference->members_seeing_video < 3 && !file_count) {
				conference->mux_paused = 1;
				files_playing = 0;
				switch_yield(20000);
				continue;
			} else {
				conference->mux_paused = 0;
			}
		}

		switch_mutex_lock(conference->member_mutex);
		watchers = 0;

		for (imember = conference->members; imember; imember = imember->next) {
			int no_muted = conference_utils_test_flag(imember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS);
			int no_av = conference_utils_test_flag(imember->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS);
			int seen = conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN);
			int hold = conference_utils_member_test_flag(imember, MFLAG_HOLD);

			if (imember->channel && switch_channel_ready(imember->channel) && switch_channel_test_flag(imember->channel, CF_VIDEO_READY) &&
				imember->watching_canvas_id == canvas->canvas_id) {
				watchers++;
			}

			if (imember->channel && switch_channel_ready(imember->channel) && switch_channel_test_flag(imember->channel, CF_VIDEO_READY) &&
				!conference_utils_member_test_flag(imember, MFLAG_SECOND_SCREEN) && !hold &&
				conference_utils_member_test_flag(imember, MFLAG_RUNNING) && (!no_muted || seen) && (!no_av || (no_av && !imember->avatar_png_img))
				&& imember->canvas_id == canvas->canvas_id && imember->video_media_flow != SWITCH_MEDIA_FLOW_SENDONLY && imember->video_media_flow != SWITCH_MEDIA_FLOW_INACTIVE) {
				video_count++;
			}

		}

		if (video_count != canvas->video_count || video_count != last_video_count) {
			count_changed = 1;
		}

		canvas->video_count = last_video_count = video_count;
		switch_mutex_unlock(conference->member_mutex);

		if (canvas->playing_video_file) {
			switch_core_timer_next(&canvas->timer);
		}

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

		if (canvas->playing_video_file) {
			file_count = 0;
		}

		if (file_count != last_file_count) {
			count_changed = 1;
		}

		if (count_changed || watchers != last_watchers) {
			canvas->send_keyframe = 1;
		}

		if (count_changed && !personal) {
			layout_group_t *lg = NULL;
			video_layout_t *vlayout = NULL;

			if (canvas->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, canvas->video_layout_group))) {
				if ((vlayout = conference_video_find_best_layout(conference, lg, canvas->video_count - file_count, file_count)) && vlayout != canvas->vlayout) {
					switch_mutex_lock(conference->member_mutex);
					canvas->new_vlayout = vlayout;
					switch_mutex_unlock(conference->member_mutex);
				}
			}
		}

		last_watchers = watchers;
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
						for (i = 0; canvas->write_codecs[i] && switch_core_codec_ready(&canvas->write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
							if (check_codec->implementation->codec_id == canvas->write_codecs[i]->codec.implementation->codec_id) {
								if ((zstr(imember->video_codec_group) && zstr(canvas->write_codecs[i]->video_codec_group)) || 
									(!strcmp(switch_str_nil(imember->video_codec_group), switch_str_nil(canvas->write_codecs[i]->video_codec_group)))) {
								
									imember->video_codec_index = i;
									imember->video_codec_id = check_codec->implementation->codec_id;
									need_refresh = SWITCH_TRUE;
									break;
								}
							}
						}
						
						if (imember->video_codec_index < 0) {
							canvas->write_codecs[i] = switch_core_alloc(conference->pool, sizeof(codec_set_t));
							canvas->write_codecs_count = i+1;

							if (switch_core_codec_copy(check_codec, &canvas->write_codecs[i]->codec,
													   &conference->video_codec_settings, conference->pool) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "Setting up video write codec %s at slot %d group %s\n", 
												  canvas->write_codecs[i]->codec.implementation->iananame, i, 
												  imember->video_codec_group ? imember->video_codec_group : "_none_");
								
								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								if (imember->video_codec_group) {
									const char *gname = switch_core_sprintf(conference->pool, "group-%s", imember->video_codec_group);
									const char *val = NULL;

									canvas->write_codecs[i]->video_codec_group = switch_core_strdup(conference->pool, imember->video_codec_group);
									
									if ((val = conference_get_variable(conference, gname))) {
										switch_stream_handle_t stream = { 0 };
										char *argv[5] = {0};
										char cid[32] = "";

										SWITCH_STANDARD_STREAM(stream);
										switch_snprintf(cid, sizeof(cid), "%d", canvas->canvas_id + 1);
									
										argv[2] = (char *)val;
										argv[3] = imember->video_codec_group;
										argv[4] = cid;
										conference_api_sub_vid_bandwidth(conference, &stream, 5, argv);

										switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "codec group init [%s]\n", (char *)stream.data);
										free(stream.data);
									}

								}
								canvas->write_codecs[i]->frame.packet = switch_core_alloc(conference->pool, buflen);
								canvas->write_codecs[i]->frame.data = ((uint8_t *)canvas->write_codecs[i]->frame.packet) + 12;
								canvas->write_codecs[i]->frame.packetlen = buflen;
								canvas->write_codecs[i]->frame.buflen = buflen - 12;
								if (conference->scale_h264_canvas_width > 0 && conference->scale_h264_canvas_height > 0 && !strcmp(check_codec->implementation->iananame, "H264")) {
									int32_t bw = -1;

									canvas->write_codecs[i]->fps_divisor = conference->scale_h264_canvas_fps_divisor;
									canvas->write_codecs[i]->scaled_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, conference->scale_h264_canvas_width, conference->scale_h264_canvas_height, 16);

									if (conference->scale_h264_canvas_bandwidth) {
										if (strcasecmp(conference->scale_h264_canvas_bandwidth, "auto")) {
											bw = switch_parse_bandwidth_string(conference->scale_h264_canvas_bandwidth);
										}
									}

									if (bw == -1) {
										float fps = conference->video_fps.fps;

										if (canvas->write_codecs[i]->fps_divisor) fps /= canvas->write_codecs[i]->fps_divisor;

										bw = switch_calc_bitrate(conference->scale_h264_canvas_width, conference->scale_h264_canvas_height, conference->video_quality, fps);
									}

									switch_core_codec_control(&canvas->write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &bw, SCCT_NONE, NULL, NULL, NULL);
								}
								switch_set_flag((&canvas->write_codecs[i]->frame), SFF_RAW_RTP);

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

			if ((conference_utils_member_test_flag(imember, MFLAG_HOLD) ||
				(conference_utils_test_flag(imember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS) &&
				 !conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN))) && imember->video_layer_id > -1) {
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
			if (conference->conference_video_mode == CONF_VIDEO_MODE_MUX &&
				conference->canvas_count == 1 && canvas->layout_floor_id > -1 && imember->id == conference->video_floor_holder &&
				imember->video_layer_id != canvas->layout_floor_id) {
				conference_video_attach_video_layer(imember, canvas, canvas->layout_floor_id);
			}

			if (canvas->playing_video_file) {
				switch_img_free(&img);
				switch_core_session_rwunlock(imember->session);
				continue;
			}

			conference_video_pop_next_image(imember, &img);
			layer = NULL;

			switch_mutex_lock(canvas->mutex);

			
			if (zstr(imember->video_role_id) || !canvas->role_count) {
				if (canvas->layout_floor_id > -1 && imember->id == conference->video_floor_holder &&
					imember->video_layer_id != canvas->layout_floor_id) {
					conference_video_attach_video_layer(imember, canvas, canvas->layout_floor_id);
				}
			}
			
			//printf("MEMBER %d layer_id %d canvas: %d/%d\n", imember->id, imember->video_layer_id,
			//	   canvas->layers_used, canvas->total_layers);

			if (!zstr(imember->video_role_id) && canvas->role_count) {
				mcu_layer_t *tlayer = NULL;

				if (imember->video_layer_id > -1) {
					tlayer = &canvas->layers[imember->video_layer_id];
				}

				if (!tlayer || (zstr(tlayer->geometry.role_id) || strcmp(tlayer->geometry.role_id, imember->video_role_id))) {
					for (i = 0; i < canvas->total_layers; i++) {
						mcu_layer_t *xlayer = &canvas->layers[i];
						
						if (!zstr(imember->video_role_id) && !zstr(xlayer->geometry.role_id) && !strcmp(xlayer->geometry.role_id, imember->video_role_id)) {
							conference_utils_member_set_flag(imember, MFLAG_DED_VID_LAYER);
							conference_video_attach_video_layer(imember, canvas, i);
						}
					}
				}
			}

			if (imember->video_layer_id > -1) {
				layer = &canvas->layers[imember->video_layer_id];
				
				if (layer->member_id != (int)imember->id) {
					imember->video_layer_id = -1;
					layer = NULL;
					imember->layer_timeout = DEFAULT_LAYER_TIMEOUT;
				}
			}

			switch_mutex_lock(imember->flag_mutex);
			if (imember->avatar_png_img) {
				if (layer) {
					if (!imember->avatar_patched || !layer->cur_img) {
						layer->tagged = 1;
						//layer->is_avatar = 1;
						switch_img_free(&layer->cur_img);
						switch_img_letterbox(imember->avatar_png_img,
											 &layer->cur_img, layer->screen_w, layer->screen_h, conference->video_letterbox_bgcolor);
						imember->avatar_patched = 1;
					}
				}
				switch_img_free(&img);
			}
			switch_mutex_unlock(imember->flag_mutex);

			if (imember->video_layer_id < 0) {
				layer = NULL;
			}

			if (!layer && (!conference_utils_test_flag(imember->conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS) || ((switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY && switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_INACTIVE)))) {
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

				if ((conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN) && !conference_utils_member_test_flag(imember, MFLAG_HOLD)) || switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY || switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE || conference_utils_test_flag(imember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS)) {
					layer->mute_patched = 0;
				} else {

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
									switch_img_letterbox(imember->video_mute_img, 
														 &layer->mute_img, layer->screen_w, layer->screen_h, conference->video_letterbox_bgcolor);
									//switch_img_copy(imember->video_mute_img, &layer->mute_img);
								}
							}

							if (layer->mute_img) {
								conference_video_member_video_mute_banner(layer->mute_img, imember);
								conference_video_scale_and_patch(layer, layer->mute_img, SWITCH_FALSE);
							}
						}

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

			switch_core_timer_next(&canvas->timer);
			
			switch_mutex_lock(conference->member_mutex);

			for (imember = conference->members; imember; imember = imember->next) {

				if (!imember->rec &&
					(!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) ||
					 switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS)) {
					continue;
				}

				if (!imember->canvas) {
					if ((vlayout = conference_video_get_layout(conference, conference->video_layout_name, canvas->video_layout_group))) {
						conference_video_init_canvas(conference, vlayout, &imember->canvas);
						//conference_video_init_canvas_layers(conference, imember->canvas, vlayout, SWITCH_TRUE);
					} else {
						continue;
					}
				}

				if (conference->new_personal_vlayout) {
					conference_video_init_canvas_layers(conference, imember->canvas, conference->new_personal_vlayout, SWITCH_FALSE);
					layout_applied++;
				}

				if (imember->channel && switch_channel_test_flag(imember->channel, CF_VIDEO_REFRESH_REQ)) {
					switch_channel_clear_flag(imember->channel, CF_VIDEO_REFRESH_REQ);
					send_keyframe = SWITCH_TRUE;
				}

				if (count_changed) {
					int total = last_video_count;
					int kps;
					switch_vid_params_t vid_params = { 0 };

					if (!conference_utils_test_flag(conference, CFLAG_VIDEO_REQUIRED_FOR_CANVAS)) {
						total += conference->members_with_avatar;
					}

					if (total > 0 &&
						(!conference_utils_test_flag(imember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS) ||
						 (conference_utils_member_test_flag(imember, MFLAG_CAN_BE_SEEN) && !conference_utils_member_test_flag(imember, MFLAG_HOLD))) &&
						imember->session && switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY &&
						imember->session && switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_INACTIVE) {

						total--;
					}

					if (total < 1) total = 1;

					if (conference->members_with_video == 1 && file_count) {
						total = 0;
					}

					if (conference->video_layout_group && (lg = switch_core_hash_find(conference->layout_group_hash, conference->video_layout_group))) {
						if ((vlayout = conference_video_find_best_layout(conference, lg, total, 0))) {
							conference_video_init_canvas_layers(conference, imember->canvas, vlayout, SWITCH_FALSE);
						}
					}
					
					if (imember->channel && !switch_channel_test_flag(imember->channel, CF_VIDEO_BITRATE_UNMANAGABLE) && 
						conference_utils_test_flag(conference, CFLAG_MANAGE_INBOUND_VIDEO_BITRATE)) {
						switch_core_media_get_vid_params(imember->session, &vid_params);
						kps = switch_calc_bitrate(vid_params.width, vid_params.height, conference->video_quality, (int)(imember->conference->video_fps.fps));
						conference_video_set_incoming_bitrate(imember, kps, SWITCH_TRUE);
					}
				}

				if (imember->session && switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_SENDONLY && switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) != SWITCH_MEDIA_FLOW_INACTIVE) {
					conference_video_pop_next_image(imember, &imember->pcanvas_img);
				}

				if (imember->session) {
					switch_core_session_rwunlock(imember->session);
				}
			}

			if (conference->new_personal_vlayout && layout_applied) {
				conference->new_personal_vlayout = NULL;
				layout_applied = 0;
			}

			switch_mutex_lock(conference->file_mutex);

			if (check_async_file && conference->async_fnode) {
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

			if (check_file && conference->fnode) {
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
			switch_mutex_unlock(conference->file_mutex);

			for (imember = conference->members; imember; imember = imember->next) {
				int i = 0;
				mcu_layer_t *floor_layer = NULL;

				if (!imember->rec &&
					(!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO) || !imember->canvas ||
					 (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY ||
					 switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) ||
					 (switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS))) {
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
						switch_core_session_media_flow(omember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY || switch_core_session_media_flow(omember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) {
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

					if (layer && !file_count) {
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
								if (omember->avatar_png_img) {
									switch_img_letterbox(omember->avatar_png_img,
														 &layer->cur_img, layer->screen_w, layer->screen_h, conference->video_letterbox_bgcolor);
								}
								layer->avatar_patched = 1;
							}
							use_img = NULL;
							layer = NULL;
						}

						if (layer) {
							if (conference_utils_member_test_flag(omember, MFLAG_CAN_BE_SEEN) && !conference_utils_member_test_flag(imember, MFLAG_HOLD)) {
								layer->mute_patched = 0;
							} else if (!conference_utils_test_flag(omember->conference, CFLAG_VIDEO_MUTE_EXIT_CANVAS)) {
								if (!layer->mute_patched) {
									switch_image_t *mute_img = omember->video_mute_img ? omember->video_mute_img : omember->pcanvas_img;

									if (mute_img) {
										switch_image_t *tmp;

										switch_img_copy(mute_img, &tmp);
										switch_img_fit(&tmp, layer->screen_w, layer->screen_h, SWITCH_FIT_SIZE);
										//conference_video_member_video_mute_banner(imember->canvas, layer, imember);
										conference_video_member_video_mute_banner(tmp, omember);
										switch_img_copy(tmp, &layer->cur_img);
									}
									
									layer->mute_patched = 1;
								}

								use_img = NULL;
								layer = NULL;
							}
						}

						if (layer && use_img) {
							//switch_img_copy(use_img, &layer->cur_img);
							conference_video_scale_and_patch(layer, use_img, SWITCH_FALSE);
						}
						
					}					
				}

				for (j = 0; j < file_count; j++) {
					switch_image_t *img = file_imgs[j];
					layer = NULL;

					if (!img) continue;

					if (j == 0 && imember->canvas->layout_floor_id > -1) {
						layer = &imember->canvas->layers[imember->canvas->layout_floor_id];
					} else if (i < imember->canvas->total_layers) {
						layer = &imember->canvas->layers[i++];
					}

					if (layer) {
						switch_img_free(&layer->banner_img);
						switch_img_free(&layer->logo_img);
						layer->member_id = -1;
						//switch_img_copy(img, &layer->cur_img);
						conference_video_scale_and_patch(layer, img, SWITCH_FALSE);
					}
				}

				if (imember->session) {
					switch_core_session_rwunlock(imember->session);
				}
			}

			for (j = 0; j < file_count; j++) {
				switch_img_free(&file_imgs[j]);
			}

			if (files_playing && !file_count) {
				files_playing = 0;
			}

			for (imember = conference->members; imember; imember = imember->next) {
				switch_frame_t *dupframe;
				
				if (!imember->rec &&
					(!imember->session || !switch_channel_test_flag(imember->channel, CF_VIDEO_READY) || !imember->canvas ||
					 switch_channel_test_flag(imember->channel, CF_VIDEO_WRITING) ||
					 switch_core_session_read_lock(imember->session) != SWITCH_STATUS_SUCCESS)) {
					continue;
				}
				
				if (conference_utils_member_test_flag(imember, MFLAG_VIDEO_JOIN)) {
					send_keyframe = SWITCH_TRUE;
				}

				if (need_refresh && imember->session) {
					switch_core_session_request_video_refresh(imember->session);
				}

				if (send_keyframe && imember->session) {
					switch_core_media_gen_key_frame(imember->session);
				}

				write_frame.img = imember->canvas->img;

				if (imember->rec) {
					if (switch_core_file_write_video(&imember->rec->fh, &write_frame) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Video Write Failed\n");
						conference_utils_member_clear_flag_locked(imember, MFLAG_RUNNING);
					}
				} else {
					switch_set_flag(&write_frame, SFF_RAW_RTP);
					write_frame.packet = packet;
					write_frame.data = ((uint8_t *)packet) + 12;
					write_frame.datalen = 0;
					write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
					write_frame.packetlen = 0;

					if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
						if (switch_frame_buffer_trypush(imember->fb, dupframe) != SWITCH_STATUS_SUCCESS) {
							switch_frame_buffer_free(imember->fb, &dupframe);
						}
						dupframe = NULL;
					}
				}

				switch_img_free(&imember->pcanvas_img);

				if (imember->session) {
					switch_core_session_rwunlock(imember->session);
				}
			}

			switch_mutex_unlock(conference->member_mutex);
		} else {
			switch_mutex_lock(conference->file_mutex);
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
			switch_mutex_unlock(conference->file_mutex);

			if (!canvas->playing_video_file) {
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
								conference_video_wake_layer_thread(layer->member);
							} else {
								conference_video_scale_and_patch(layer, NULL, SWITCH_FALSE);
							}
						}

						layer->tagged = 0;
					}
				}

				switch_core_timer_next(&canvas->timer);
				wait_for_canvas(canvas);
				
				for (i = 0; i < canvas->total_layers; i++) {
					mcu_layer_t *layer = &canvas->layers[i];
					
					if ((layer->member_id > -1 || layer->fnode) && layer->cur_img && layer->geometry.overlap) {

						layer->mute_patched = 0;
						layer->banner_patched = 0;

						if (canvas->refresh) {
							layer->refresh = 1;
							canvas->refresh++;
						}

						if (layer->cur_img) {
							if (layer->member && switch_core_cpu_count() > 2) {
								layer->need_patch = 1;
								conference_video_wake_layer_thread(layer->member);
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
					//need_refresh = SWITCH_TRUE;
				}
				canvas->send_keyframe--;
			}

			if (video_key_freq && (now - last_key_time) > video_key_freq) {
				send_keyframe = SWITCH_TRUE;
				last_key_time = now;
			}

			write_img = canvas->img;
			timestamp = canvas->timer.samplecount;

			if (canvas->play_file == 1) {
				canvas->send_keyframe = 1;
				canvas->play_file = -1;
			}

			switch_mutex_lock(conference->file_mutex);
			if (conference->fnode && switch_test_flag(&conference->fnode->fh, SWITCH_FILE_OPEN)) {
				if (canvas->overlay_video_file) {
					if (switch_core_file_read_video(&conference->fnode->fh, &write_frame, SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {										
						switch_img_free(&file_img);
						switch_img_fit(&write_frame.img, canvas->img->d_w, canvas->img->d_h, SWITCH_FIT_SIZE);
						file_img = write_frame.img;
						
						if (file_img->fmt == SWITCH_IMG_FMT_ARGB) {
							switch_image_t *overlay_img = NULL;
							switch_img_copy(canvas->img, &overlay_img);

							if (conference->fnode->filters & SCV_FILTER_GRAY_BG) {
								switch_img_gray(overlay_img, 0, 0, overlay_img->d_w, overlay_img->d_h);
							}

							if (conference->fnode->filters & SCV_FILTER_SEPIA_BG) {
								switch_img_sepia(overlay_img, 0, 0, overlay_img->d_w, overlay_img->d_h);
							}

							if (conference->fnode->filters & SCV_FILTER_GRAY_FG) {
								switch_img_gray(file_img, 0, 0, file_img->d_w, file_img->d_h);
							}

							if (conference->fnode->filters & SCV_FILTER_SEPIA_FG) {
								switch_img_sepia(file_img, 0, 0, file_img->d_w, file_img->d_h);
							}

							write_img = overlay_img;
							switch_img_patch(write_img, file_img, 0, 0);
							switch_img_free(&file_img);
							file_img = overlay_img;
						} else {
							write_img = file_img;
						}
					
						//switch_core_timer_sync(&canvas->timer);
						//timestamp = canvas->timer.samplecount;
					} else if (file_img) {
						write_img = file_img;
					}
				} else if (canvas->playing_video_file) {
					if (switch_core_file_read_video(&conference->fnode->fh, &write_frame, SVR_FLUSH) == SWITCH_STATUS_SUCCESS) {
						switch_image_t *tmp = NULL;

						switch_img_free(&file_img);
						switch_img_letterbox(write_frame.img, &tmp, canvas->img->d_w, canvas->img->d_h, "#000000");
						if (tmp) {
							switch_img_free(&write_frame.img);
							file_img = write_img = write_frame.img = tmp;
						} else {
							file_img = write_img = write_frame.img;
						}

						//switch_core_timer_sync(&canvas->timer);
						//timestamp = canvas->timer.samplecount;
					} else if (file_img) {
						write_img = file_img;
					}
				} else if (file_img) {
					switch_img_free(&file_img);
				}
			}
			switch_mutex_unlock(conference->file_mutex);

			write_frame.img = write_img;

			if (!canvas->playing_video_file && !canvas->overlay_video_file) {
				wait_for_canvas(canvas);
			}

			if (canvas->fgimg) {
				conference_video_set_canvas_fgimg(canvas, NULL);
			}

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
				for (i = 0; canvas->write_codecs[i] && switch_core_codec_ready(&canvas->write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
					canvas->write_codecs[i]->frame.img = write_img;
					conference_video_write_canvas_image_to_codec_group(conference, canvas, canvas->write_codecs[i], i,
																	   timestamp, need_refresh, send_keyframe, need_reset);
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

				if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY || 
					switch_channel_test_flag(imember->channel, CF_VIDEO_WRITING) ||
					!conference_utils_member_test_flag(imember, MFLAG_CAN_SEE) ||
					switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) {
					switch_core_session_rwunlock(imember->session);
					continue;
				}


				if (send_keyframe) {
					switch_core_media_gen_key_frame(imember->session);
				}


				if (canvas->fgimg) {
					conference_video_set_canvas_fgimg(canvas, NULL);
				}

				switch_set_flag(&write_frame, SFF_RAW_RTP|SFF_USE_VIDEO_TIMESTAMP|SFF_RAW_RTP_PARSE_FRAME);
				write_frame.img = write_img;
				write_frame.packet = packet;
				write_frame.data = ((uint8_t *)packet) + 12;
				write_frame.datalen = 0;
				write_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
				write_frame.packetlen = 0;
				write_frame.timestamp = timestamp;

				//switch_core_session_write_video_frame(imember->session, &write_frame, SWITCH_IO_FLAG_NONE, 0);

				if (switch_frame_buffer_dup(imember->fb, &write_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
					if (switch_frame_buffer_trypush(imember->fb, dupframe) != SWITCH_STATUS_SUCCESS) {
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
		switch_mutex_lock(layer->overlay_mutex);
		switch_img_free(&layer->cur_img);
		switch_img_free(&layer->overlay_img);
		switch_img_free(&layer->img);
		layer->banner_patched = 0;
		switch_img_free(&layer->banner_img);
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->mute_img);
		switch_mutex_unlock(layer->overlay_mutex);
		switch_mutex_unlock(canvas->mutex);

		if (layer->txthandle) {
			switch_img_txt_handle_destroy(&layer->txthandle);
		}
	}

	for (i = 0; i < MAX_MUX_CODECS; i++) {
		if (canvas->write_codecs[i] && switch_core_codec_ready(&canvas->write_codecs[i]->codec)) {
			switch_core_codec_destroy(&canvas->write_codecs[i]->codec);
			switch_img_free(&(canvas->write_codecs[i]->scaled_img));
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
	int buflen = SWITCH_RTP_MAX_BUF_LEN;
	int i = 0;
	switch_time_t last_key_time = 0;
	uint32_t video_key_freq = 0;
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
			conference_video_init_canvas_layers(conference, canvas, NULL, SWITCH_TRUE);
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
				if ((vlayout = conference_video_find_best_layout(conference, lg, total, 0))) {
					conference_video_init_canvas_layers(conference, canvas, vlayout, SWITCH_TRUE);
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
						for (i = 0; canvas->write_codecs[i] && switch_core_codec_ready(&canvas->write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
							if (check_codec->implementation->codec_id == canvas->write_codecs[i]->codec.implementation->codec_id) {
								if ((zstr(imember->video_codec_group) && zstr(canvas->write_codecs[i]->video_codec_group)) || 
									(!strcmp(switch_str_nil(imember->video_codec_group), switch_str_nil(canvas->write_codecs[i]->video_codec_group)))) {
								
									imember->video_codec_index = i;
									imember->video_codec_id = check_codec->implementation->codec_id;
									need_refresh = SWITCH_TRUE;
									break;
								}
							}
						}
						
						if (imember->video_codec_index < 0) {
							canvas->write_codecs[i] = switch_core_alloc(conference->pool, sizeof(codec_set_t));
							canvas->write_codecs_count = i+1;
							
							if (switch_core_codec_copy(check_codec, &canvas->write_codecs[i]->codec,
													   &conference->video_codec_settings, conference->pool) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
												  "Setting up video write codec %s at slot %d group %s\n", 
												  canvas->write_codecs[i]->codec.implementation->iananame, i, 
												  imember->video_codec_group ? imember->video_codec_group : "_none_");
								
								imember->video_codec_index = i;
								imember->video_codec_id = check_codec->implementation->codec_id;
								need_refresh = SWITCH_TRUE;
								if (imember->video_codec_group) {
									canvas->write_codecs[i]->video_codec_group = switch_core_strdup(conference->pool, imember->video_codec_group);
								}
								canvas->write_codecs[i]->frame.packet = switch_core_alloc(conference->pool, buflen);
								canvas->write_codecs[i]->frame.data = ((uint8_t *)canvas->write_codecs[i]->frame.packet) + 12;
								canvas->write_codecs[i]->frame.packetlen = buflen;
								canvas->write_codecs[i]->frame.buflen = buflen - 12;
								if (conference->scale_h264_canvas_width > 0 && conference->scale_h264_canvas_height > 0 && !strcmp(check_codec->implementation->iananame, "H264")) {
									int32_t bw = -1;

									canvas->write_codecs[i]->fps_divisor = conference->scale_h264_canvas_fps_divisor;
									canvas->write_codecs[i]->scaled_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, conference->scale_h264_canvas_width, conference->scale_h264_canvas_height, 16);

									if (conference->scale_h264_canvas_bandwidth) {
										if (strcasecmp(conference->scale_h264_canvas_bandwidth, "auto")) {
											bw = switch_parse_bandwidth_string(conference->scale_h264_canvas_bandwidth);
										}
									}

									if (bw == -1) {
										float fps = conference->video_fps.fps;

										if (canvas->write_codecs[i]->fps_divisor) fps /= canvas->write_codecs[i]->fps_divisor;

										bw = switch_calc_bitrate(conference->scale_h264_canvas_width, conference->scale_h264_canvas_height, conference->video_quality, fps);
									}

									switch_core_codec_control(&canvas->write_codecs[i]->codec, SCC_VIDEO_BANDWIDTH, SCCT_INT, &bw, SCCT_NONE, NULL, NULL, NULL);
								}
								switch_set_flag((&canvas->write_codecs[i]->frame), SFF_RAW_RTP);

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
			for (i = 0; canvas->write_codecs[i] && switch_core_codec_ready(&canvas->write_codecs[i]->codec) && i < MAX_MUX_CODECS; i++) {
				canvas->write_codecs[i]->frame.img = write_img;
				conference_video_write_canvas_image_to_codec_group(conference, canvas, canvas->write_codecs[i], i, timestamp, need_refresh, send_keyframe, need_reset);
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

			if (switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_RECVONLY || 
				switch_channel_test_flag(imember->channel, CF_VIDEO_WRITING) ||
				!conference_utils_member_test_flag(imember, MFLAG_CAN_SEE) ||
				switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) {
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
				if (switch_frame_buffer_trypush(imember->fb, dupframe) != SWITCH_STATUS_SUCCESS) {
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
		switch_mutex_lock(layer->overlay_mutex);
		switch_img_free(&layer->cur_img);
		switch_img_free(&layer->overlay_img);
		switch_img_free(&layer->img);
		layer->banner_patched = 0;
		switch_img_free(&layer->banner_img);
		switch_img_free(&layer->logo_img);
		switch_img_free(&layer->mute_img);
		switch_mutex_unlock(layer->overlay_mutex);
		switch_mutex_unlock(canvas->mutex);

		if (layer->txthandle) {
			switch_img_txt_handle_destroy(&layer->txthandle);
		}
	}

	for (i = 0; i < MAX_MUX_CODECS; i++) {
		if (canvas->write_codecs[i] && switch_core_codec_ready(&canvas->write_codecs[i]->codec)) {
			switch_core_codec_destroy(&canvas->write_codecs[i]->codec);
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

		if (conference_utils_member_test_flag(imember, MFLAG_DED_VID_LAYER)) {
			continue;
		}
		
		if (!(imember->session)) {
			continue;
		}

		if ((switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY || switch_core_session_media_flow(imember->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) && !imember->avatar_png_img) {
			continue;
		}

		if (!switch_channel_test_flag(imember->channel, CF_VIDEO_READY) && !imember->avatar_png_img) {
			continue;
		}

		if (!entering && imember->id == member->id) {
			continue;
		}

		if (conference->floor_holder && imember->id == conference->floor_holder) {
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

	if (member && conference_utils_member_test_flag(member, MFLAG_DED_VID_LAYER)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Setting floor not allowed on a member in a dedicated layer\n");
	}
	
	if ((!force && conference_utils_test_flag(conference, CFLAG_VID_FLOOR_LOCK))) {
		return;
	}

	if (member && (switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY || switch_core_session_media_flow(member->session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) && !member->avatar_png_img) {
		return;
	}

	if (conference->video_floor_holder) {
		if (member && conference->video_floor_holder == member->id) {
			return;
		} else {
			if (member) {
				conference->last_video_floor_holder = conference->video_floor_holder;
			}

			if (conference->conference_video_mode == CONF_VIDEO_MODE_MUX &&
				conference->last_video_floor_holder && (imember = conference_member_get(conference, conference->last_video_floor_holder))) {
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Adding video floor %s\n",
						  switch_channel_get_name(member->channel));

		conference_video_check_flush(member, SWITCH_FALSE);
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
		switch_img_fit(&tmp_img, conference->canvases[0]->width, conference->canvases[0]->height, SWITCH_FIT_SIZE_AND_SCALE);

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

		if (!conference_utils_member_test_flag(imember, MFLAG_CAN_SEE)) {
			switch_core_session_rwunlock(isession);
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
						switch_frame_t *dupframe;
						
						tmp_frame.packet = buf;
						tmp_frame.packetlen = 0;
						tmp_frame.buflen = SWITCH_RTP_MAX_BUF_LEN - 12;
						tmp_frame.data = buf + 12;
						
						if (imember->fb) {
							if (switch_frame_buffer_dup(imember->fb, &tmp_frame, &dupframe) == SWITCH_STATUS_SUCCESS) {
								if (switch_frame_buffer_trypush(imember->fb, dupframe) != SWITCH_STATUS_SUCCESS) {
									switch_frame_buffer_free(imember->fb, &dupframe);
								}
								dupframe = NULL;
							}
						} else {
							switch_core_session_write_video_frame(imember->session, &tmp_frame, SWITCH_IO_FLAG_NONE, 0);
						}
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
	int files_playing = 0;
	
	switch_assert(member);

	if (switch_test_flag(frame, SFF_CNG) || !frame->packet) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_SENDONLY || switch_core_session_media_flow(session, SWITCH_MEDIA_TYPE_VIDEO) == SWITCH_MEDIA_FLOW_INACTIVE) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (switch_thread_rwlock_tryrdlock(member->conference->rwlock) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}


	switch_mutex_lock(member->conference->file_mutex);
	if (member->conference->async_fnode && switch_core_file_has_video(&member->conference->async_fnode->fh, SWITCH_TRUE)) {
		files_playing = 1;
	}
	
	if (member->conference->fnode && switch_core_file_has_video(&member->conference->fnode->fh, SWITCH_TRUE)) {
		files_playing = 1;
	}
	switch_mutex_unlock(member->conference->file_mutex);

	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_BRIDGE_FIRST_TWO)) {
		if (member->conference->members_seeing_video < 3 && !files_playing && member->conference->mux_paused) {
			conference_video_write_frame(member->conference, member, frame);
			conference_video_check_recording(member->conference, NULL, frame);
			switch_thread_rwlock_unlock(member->conference->rwlock);
			return SWITCH_STATUS_SUCCESS;
		}
	}


	if (conference_utils_test_flag(member->conference, CFLAG_VIDEO_MUXING)) {
		switch_image_t *img_copy = NULL;

		int canvas_id = member->canvas_id;

		if (frame->img && (((member->video_layer_id > -1) && canvas_id > -1) || member->canvas) &&
			conference_utils_member_test_flag(member, MFLAG_CAN_BE_SEEN) &&
			!conference_utils_member_test_flag(member, MFLAG_HOLD) &&
			switch_queue_size(member->video_queue) < member->conference->video_fps.fps &&
			!member->conference->canvases[canvas_id]->playing_video_file) {

			if (conference_utils_member_test_flag(member, MFLAG_FLIP_VIDEO) ||
				conference_utils_member_test_flag(member, MFLAG_ROTATE_VIDEO) || conference_utils_member_test_flag(member, MFLAG_MIRROR_VIDEO)) {
				if (conference_utils_member_test_flag(member, MFLAG_ROTATE_VIDEO)) {
					if (member->flip_count++ > (int)(member->conference->video_fps.fps / 2)) {
						member->flip += 90;
						if (member->flip > 270) {
							member->flip = 0;
						}
						member->flip_count = 0;
					}

					switch_img_rotate_copy(frame->img, &img_copy, member->flip);
				} else if (conference_utils_member_test_flag(member, MFLAG_MIRROR_VIDEO)) {
					switch_img_mirror(frame->img, &img_copy);
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

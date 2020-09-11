/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 *
 * mod_imagick -- play pdf/gif as video
 *
 * use the magick-core API since the magick-wand API is more different on different versions
 * http://www.imagemagick.org/script/magick-core.php
 *
 */


#include <switch.h>

#if defined(__clang__)
/* the imagemagick header files are very badly broken on clang.  They really should be fixing this, in the mean time, this dirty hack works */
#  define __attribute__(x) /*nothing*/
#define restrict restrict
#endif

#ifndef MAGICKCORE_QUANTUM_DEPTH
#define MAGICKCORE_QUANTUM_DEPTH 8
#endif

#ifndef MAGICKCORE_HDRI_ENABLE
#define MAGICKCORE_HDRI_ENABLE   0
#endif

#ifdef HAVE_MAGIC7
#include <MagickCore/MagickCore.h>
#else
#include <magick/MagickCore.h>
#endif

#ifdef _MSC_VER
// Disable MSVC warnings that suggest making code non-portable.
#pragma warning(disable : 4996)
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_imagick_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_imagick_shutdown);
SWITCH_MODULE_DEFINITION(mod_imagick, mod_imagick_load, mod_imagick_shutdown, NULL);

typedef enum pdf_loading_state_s {
	PLS_LOADING,
	PLS_BREAK,
	PLS_DONE
} pdf_loading_state_t;

struct pdf_file_context {
	switch_memory_pool_t *pool;
	switch_mutex_t *mutex;
	switch_image_t *img;
	int reads;
	int sent;
	int max;
	int samples;
	int same_page;
	int pagenumber;
	int pagecount;
	ImageInfo *image_info;
	Image *images;
	ExceptionInfo *exception;
	int autoplay;
	const char *path;
	int lazy;
	char *lazy_cookie;
	pdf_loading_state_t loading_state;
	switch_time_t next_play_time;
};

typedef struct pdf_file_context pdf_file_context_t;

static void *SWITCH_THREAD_FUNC open_pdf_thread_run(switch_thread_t *thread, void *obj)
{
	pdf_file_context_t *context = (pdf_file_context_t *)obj;
	int pagenumber = context->lazy;
	char path[1024];

	while (context->loading_state == PLS_LOADING) {
		Image *tmp_images;
		switch_snprintf(path, sizeof(path), "%s[%d]", context->path, pagenumber);
		switch_set_string(context->image_info->filename, path);

#ifdef HAVE_MAGIC7
		if ((tmp_images = ReadImages(context->image_info, path, context->exception))) {
#else
		if ((tmp_images = ReadImages(context->image_info, context->exception))) {
#endif

			pagenumber++;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s page %d loaded\n", context->path, pagenumber);
			AppendImageToList(&context->images, tmp_images);
			context->pagecount = pagenumber;
		} else {
			switch_event_t *event = NULL;

			if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, "imagick::info") == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "loaded");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", context->path);
				switch_event_add_header(event, SWITCH_STACK_BOTTOM, "pagecount", "%d", context->pagecount);
				if (context->lazy_cookie) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "cookie", context->lazy_cookie);
				}
				switch_event_fire(&event);
			}

			break;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "read file: %s %s, pagecount: %d\n",
		context->path, context->loading_state == PLS_BREAK ? "break" : "done", pagenumber);

	switch_mutex_lock(context->mutex);
	context->loading_state = PLS_DONE;
	switch_mutex_unlock(context->mutex);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Read Images Thread Ended.\n");
	return NULL;
}

static switch_status_t imagick_file_open(switch_file_handle_t *handle, const char *path)
{
	pdf_file_context_t *context;
	char *ext;
	char range_path[1024];

	if ((ext = strrchr((char *)path, '.')) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Format\n");
		return SWITCH_STATUS_GENERR;
	}

	ext++;
	/*
	  Prevents playing files to a conference like a slide show using conference_play api.
	if (!switch_test_flag(handle, SWITCH_FILE_FLAG_VIDEO)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Video only\n");
		return SWITCH_STATUS_GENERR;
	}
	*/
	if ((context = (pdf_file_context_t *)switch_core_alloc(handle->memory_pool, sizeof(pdf_file_context_t))) == 0) {
		return SWITCH_STATUS_MEMERR;
	}

	memset(context, 0, sizeof(pdf_file_context_t));

	if (switch_test_flag(handle, SWITCH_FILE_FLAG_WRITE)) {
		return SWITCH_STATUS_GENERR;
	}

	if (ext && !strcmp(ext, "gif")) {
		context->autoplay = 1;
	}

	context->max = 10000;

	context->exception = AcquireExceptionInfo();
	context->image_info = AcquireImageInfo();
	context->path = switch_core_strdup(handle->memory_pool, path);

	if (handle->params) {
		const char *max = switch_event_get_header(handle->params, "img_ms");
		const char *autoplay = switch_event_get_header(handle->params, "autoplay");
		const char *density = switch_event_get_header(handle->params, "density");
		const char *quality = switch_event_get_header(handle->params, "quality");
		const char *lazy = switch_event_get_header(handle->params, "lazy");
		const char *lazy_cookie = switch_event_get_header(handle->params, "cookie");
		int tmp;

		if (max) {
			tmp = atol(max);
			context->max = tmp;
		}

		if (autoplay) {
			context->autoplay = atoi(autoplay);
		}

		if (density) {
			context->image_info->density = strdup(density);
		}

		if (quality) {
			tmp = atoi(quality);

			if (tmp > 0) context->image_info->quality = tmp;
		}

		if (lazy) {
			int tmp = atoi(lazy);

			if (tmp >= 0) {
				context->lazy = tmp;
			} else {
				context->lazy = 1;
			}
		}

		if (lazy_cookie) {
			context->lazy_cookie = switch_core_strdup(handle->memory_pool, lazy_cookie);
		}
	}

	if (context->lazy) {
		switch_snprintf(range_path, sizeof(range_path), "%s[0-%d]", path, context->lazy - 1);
		switch_set_string(context->image_info->filename, range_path);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "loading first %d page%s\n", context->lazy, context->lazy > 1 ? "s" : "");
	} else {
		switch_set_string(context->image_info->filename, path);
	}

#ifdef HAVE_MAGIC7
	context->images = ReadImages(context->image_info, context->lazy ? range_path : path, context->exception);
#else
	context->images = ReadImages(context->image_info, context->exception);
#endif

	if (context->exception->severity != UndefinedException) {
		CatchException(context->exception);
	}

	if (context->images == (Image *)NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Fail to read file: %s\n", path);
		return SWITCH_STATUS_GENERR;
	}

	context->pagecount = GetImageListLength(context->images);
	handle->duration = context->pagecount;

	if (context->max) {
		context->samples = (handle->samplerate / 1000) * context->max;
	}

	handle->format = 0;
	handle->sections = 0;
	handle->seekable = 1;
	handle->speed = 0;
	handle->pos = 0;
	handle->private_info = context;
	context->pool = handle->memory_pool;

	if (context->lazy) {
		switch_thread_t *thread;
		switch_threadattr_t *thd_attr = NULL;

		switch_mutex_init(&context->mutex, SWITCH_MUTEX_NESTED, context->pool);
		context->loading_state = PLS_LOADING;
		switch_thread_create(&thread, thd_attr, open_pdf_thread_run, context, context->pool);
	}

	if (context->lazy) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File %s, read the first %d page(s)", path, context->lazy);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Opening File %s", path);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t imagick_file_close(switch_file_handle_t *handle)
{
	pdf_file_context_t *context = (pdf_file_context_t *)handle->private_info;

	if (context->lazy) {
		switch_mutex_lock(context->mutex);
		if (context->loading_state == PLS_LOADING) context->loading_state = PLS_BREAK;
		switch_mutex_unlock(context->mutex);

		while (context->loading_state != PLS_DONE) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "waiting for pdf loading thread done, loading_state: %d\n", context->loading_state);
			switch_yield(1000000);
			switch_cond_next();
		}
	}

	switch_img_free(&context->img);

	if (context->images) DestroyImageList(context->images);
	if (context->exception) DestroyExceptionInfo(context->exception);
	if (context->image_info) DestroyImageInfo(context->image_info);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t imagick_file_read(switch_file_handle_t *handle, void *data, size_t *len)
{
	pdf_file_context_t *context = (pdf_file_context_t *)handle->private_info;

	if (!context->images || !context->samples) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->samples > 0) {
		if (*len >= context->samples) {
			*len = context->samples;
		}

		context->samples -= *len;
	}

	if (!context->samples) {
		return SWITCH_STATUS_FALSE;
	}

	memset(data, 0, *len * 2 * handle->channels);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t read_page(pdf_file_context_t *context)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	Image *image = GetImageFromList(context->images, context->pagenumber);
	int W, H, w, h, x, y;
	MagickBooleanType ret;
	uint8_t *storage;

	if (!image) return SWITCH_STATUS_FALSE;

	W = image->page.width;
	H = image->page.height;
	w = image->columns;
	h = image->rows;
	x = image->page.x;
	y = image->page.y;

	switch_assert(W > 0 && H > 0);

#if 0
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
		"page: %dx%d image: %dx%d pos: (%d,%d) pagenumber: %d,"
		" delay: %" SWITCH_SIZE_T_FMT " ticks_per_second: %" SWITCH_SSIZE_T_FMT
		" autoplay: %d\n",
		W, H, w, h, x, y,
		context->pagenumber, image->delay, image->ticks_per_second, context->autoplay);
#endif

	if (context->autoplay) {
		if (image->delay && image->ticks_per_second) {
			context->next_play_time = switch_micro_time_now() / 1000 + image->delay * (1000 / image->ticks_per_second);
		} else {
			context->next_play_time = switch_micro_time_now() / 1000 + context->autoplay;
		}
	}

	if (context->img && (context->img->d_w != W || context->img->d_h != H)) {
		switch_img_free(&context->img);
	}

	if (!context->img) {
		context->img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, W, H, 0);
		switch_assert(context->img);
	}

	if (W == w && H == h) {
		storage = malloc(w * h * 3); switch_assert(storage);

		ret = ExportImagePixels(image, 0, 0, w, h, "RGB", CharPixel, storage, context->exception);

		if (ret == MagickFalse && context->exception->severity != UndefinedException) {
			CatchException(context->exception);
			free(storage);
			return SWITCH_STATUS_FALSE;
		}

		switch_img_from_raw(context->img, storage, SWITCH_IMG_FMT_BGR24, w, h);
		free(storage);
	} else {
		switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_ARGB, image->columns, image->rows, 0);
		switch_assert(img);

		ret = ExportImagePixels(image, 0, 0, w, h, "ARGB", CharPixel, img->planes[SWITCH_PLANE_PACKED], context->exception);

		if (ret == MagickFalse && context->exception->severity != UndefinedException) {
			CatchException(context->exception);
			return SWITCH_STATUS_FALSE;
		}

		switch_img_patch(context->img, img, x, y);
		switch_img_free(&img);
	}

	return status;
}

static switch_status_t imagick_file_read_video(switch_file_handle_t *handle, switch_frame_t *frame, switch_video_read_flag_t flags)
{
	pdf_file_context_t *context = (pdf_file_context_t *)handle->private_info;
	switch_image_t *dup = NULL;
	switch_status_t status;

	if ((flags & SVR_CHECK)) {
		return SWITCH_STATUS_BREAK;
	}

	if (!context->images || !context->samples) {
		return SWITCH_STATUS_FALSE;
	}

	if (context->autoplay && context->next_play_time && (switch_micro_time_now() / 1000 > context->next_play_time)) {
		context->pagenumber++;
		if (context->pagenumber >= context->pagecount) context->pagenumber = 0;
		context->same_page = 0;
	}

	if (!context->same_page) {
		status = read_page(context);
		if (status != SWITCH_STATUS_SUCCESS) return status;
		context->same_page = 1;
	}

	if (!context->img) return SWITCH_STATUS_FALSE;

	if ((context->reads++ % 20) == 0) {
		switch_img_copy(context->img, &dup);
		frame->img = dup;
		context->sent++;
	} else {
		if ((flags && SVR_BLOCK)) {
			switch_yield(5000);
		}
		return SWITCH_STATUS_BREAK;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t imagick_file_seek(switch_file_handle_t *handle, unsigned int *cur_sample, int64_t samples, int whence)
{
	pdf_file_context_t *context = (pdf_file_context_t *)handle->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	int page = samples / (handle->samplerate / 1000);

	if (whence == SEEK_SET) {
		// page = page;
	} else if (whence == SEEK_CUR) {
		page += context->pagenumber;
	} else if (whence == SEEK_END) {
		page = context->pagecount - page;
	}

	if (page < 0) page = 0;
	if (page > context->pagecount - 1) page = context->pagecount - 1;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "seeking to sample=%" SWITCH_UINT64_T_FMT " cur_sample=%d where:=%d page=%d\n", samples, *cur_sample, whence, page);

	if (page != context->pagenumber) {
		context->pagenumber = page;
		context->same_page = 0;
		*cur_sample = page;
		handle->vpos = page;
		handle->pos = page * (handle->samplerate / 1000);
	}

	return status;
}

static void myErrorHandler(const ExceptionType t, const char *reason, const char *description)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s: %s\n", reason, description);
}

static void myFatalErrorHandler(const ExceptionType t, const char *reason, const char *description)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "%s: %s\n", reason, description);
}

static void myWarningHandler(const ExceptionType t, const char *reason, const char *description)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s: %s\n", reason, description);
}

static char *supported_formats[SWITCH_MAX_CODECS] = { 0 };

SWITCH_MODULE_LOAD_FUNCTION(mod_imagick_load)
{
	switch_file_interface_t *file_interface;
	int i = 0;

	supported_formats[i++] = (char *)"imgk";
	supported_formats[i++] = (char *)"pdf";
	supported_formats[i++] = (char *)"gif";

	MagickCoreGenesis(NULL, MagickFalse);

	SetErrorHandler(myErrorHandler);
	SetWarningHandler(myWarningHandler);
	SetFatalErrorHandler(myFatalErrorHandler);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	file_interface = (switch_file_interface_t *)switch_loadable_module_create_interface(*module_interface, SWITCH_FILE_INTERFACE);
	file_interface->interface_name = modname;
	file_interface->extens = supported_formats;
	file_interface->file_open = imagick_file_open;
	file_interface->file_close = imagick_file_close;
	file_interface->file_read = imagick_file_read;
	file_interface->file_read_video = imagick_file_read_video;
	file_interface->file_seek = imagick_file_seek;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_imagick_shutdown)
{
	MagickCoreTerminus();
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

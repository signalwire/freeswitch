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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 *
 * switch_core_video.c -- Core Video
 *
 */

#ifdef SWITCH_HAVE_VPX
#include "vpx/vpx_image.h"
#if VPX_IMAGE_ABI_VERSION != (4)
#error VPX_IMAGE_ABI_VERSION is not (4)
#endif
#endif

#include <switch.h>
#include <switch_utf8.h>

#ifdef SWITCH_HAVE_YUV
#include <libyuv.h>
#endif

// #define HAVE_LIBGD
#ifdef HAVE_LIBGD
#include <gd.h>
#endif

#ifdef SWITCH_HAVE_YUV
static inline void switch_img_get_yuv_pixel(switch_image_t *img, switch_yuv_color_t *yuv, int x, int y);
#endif

static inline void switch_img_get_rgb_pixel(switch_image_t *img, switch_rgb_color_t *rgb, int x, int y);


/*!\brief Convert RGB color to YUV
*
* \param[in]    rgb       RGB color pointer
* \param[out]   yuv       YUV color pointer
*/
#ifdef SWITCH_HAVE_YUV
static inline void switch_color_rgb2yuv(switch_rgb_color_t *rgb, switch_yuv_color_t *yuv);
#endif

/*!\brief Convert YUV color to RGB
*
* \param[in]    yuv       YUV color pointer
* \param[out]   rgb       RGB color pointer
*/
#ifdef SWITCH_HAVE_YUV
static inline void switch_color_yuv2rgb(switch_yuv_color_t *yuv, switch_rgb_color_t *rgb);
#endif

/*!\brief Draw a pixel on an image
*
* \param[in]    img       Image descriptor
* \param[in]    x         leftmost pos
* \param[in]    y         topmost pos
* \param[in]    color     RGB color
*/
static inline void switch_img_draw_pixel(switch_image_t *img, int x, int y, switch_rgb_color_t *color);


struct pos_el {
	switch_img_position_t pos;
	const char *name;
};


static struct pos_el POS_TABLE[] = {
	{POS_LEFT_TOP, "left-top"},
	{POS_LEFT_MID, "left-mid"},
	{POS_LEFT_BOT, "left-bot"},
	{POS_CENTER_TOP, "center-top"},
	{POS_CENTER_MID, "center-mid"},
	{POS_CENTER_BOT, "center-bot"},
	{POS_RIGHT_TOP, "right-top"},
	{POS_RIGHT_MID, "right-mid"},
	{POS_RIGHT_BOT, "right-bot"},
	{POS_NONE, "none"},
	{POS_NONE, NULL}
};


SWITCH_DECLARE(switch_img_position_t) parse_img_position(const char *name)
{
	switch_img_position_t r = POS_NONE;
	int i;

	switch_assert(name);
	
	for(i = 0; POS_TABLE[i].name; i++) {
		if (!strcasecmp(POS_TABLE[i].name, name)) {
			r = POS_TABLE[i].pos;
			break;
		}
	}
	
	return r;
}


struct fit_el {
	switch_img_fit_t fit;
	const char *name;
};


static struct fit_el IMG_FIT_TABLE[] = {
	{SWITCH_FIT_SIZE, "fit-size"},
	{SWITCH_FIT_SCALE, "fit-scale"},
	{SWITCH_FIT_SIZE_AND_SCALE, "fit-size-and-scale"},
	{SWITCH_FIT_NONE, NULL}
};


SWITCH_DECLARE(switch_img_fit_t) parse_img_fit(const char *name)
{
	switch_img_fit_t r = SWITCH_FIT_SIZE;
	int i;

	switch_assert(name);
	
	for(i = 0; IMG_FIT_TABLE[i].name; i++) {
		if (!strcasecmp(IMG_FIT_TABLE[i].name, name)) {
			r = IMG_FIT_TABLE[i].fit;
			break;
		}
	}
	
	return r;
}

SWITCH_DECLARE(switch_bool_t) switch_core_has_video(void)
{
#ifdef SWITCH_HAVE_VPX
#ifdef SWITCH_HAVE_YUV
	return SWITCH_TRUE;
#else
	return SWITCH_FALSE;
#endif
#else
	return SWITCH_FALSE;
#endif
}
							  
SWITCH_DECLARE(switch_image_t *)switch_img_alloc(switch_image_t  *img,
						 switch_img_fmt_t fmt,
						 unsigned int d_w,
						 unsigned int d_h,
						 unsigned int align)
{
#ifdef SWITCH_HAVE_VPX
#ifdef HAVE_LIBGD
	if (fmt == SWITCH_IMG_FMT_GD) {
		gdImagePtr gd = gdImageCreateTrueColor(d_w, d_h);

		if (!gd) return NULL;

		switch_img_free(&img);
		img = (switch_image_t *)vpx_img_alloc(NULL, SWITCH_IMG_FMT_ARGB, 1, 1, 1);

		if (!img) {
			gdImageDestroy(gd);
			return NULL;
		}

		img->user_priv = gd;
		img->d_w = d_w;
		img->d_h = d_h;
		img->fmt = SWITCH_IMG_FMT_GD;
		return img;
	}
#endif

	return (switch_image_t *)vpx_img_alloc((vpx_image_t *)img, (vpx_img_fmt_t)fmt, d_w, d_h, align);
#else
	return NULL;
#endif
}

SWITCH_DECLARE(switch_image_t *)switch_img_wrap(switch_image_t  *img,
						switch_img_fmt_t fmt,
						unsigned int d_w,
						unsigned int d_h,
						unsigned int align,
						unsigned char      *img_data)
{
#ifdef SWITCH_HAVE_VPX
	return (switch_image_t *)vpx_img_wrap((vpx_image_t *)img, (vpx_img_fmt_t)fmt, d_w, d_h, align, img_data);
#else
	return NULL;
#endif
}

SWITCH_DECLARE(int) switch_img_set_rect(switch_image_t  *img,
				   unsigned int  x,
				   unsigned int  y,
				   unsigned int  w,
				   unsigned int  h)
{
#ifdef SWITCH_HAVE_VPX
	return vpx_img_set_rect((vpx_image_t *)img, x, y, w, h);
#else
	return 0;
#endif
}

SWITCH_DECLARE(void) switch_img_rotate(switch_image_t **img, switch_image_rotation_mode_t mode)
{
#ifdef SWITCH_HAVE_YUV
	switch_image_t *tmp_img;

	switch_assert(img);


	if ((*img)->fmt != SWITCH_IMG_FMT_I420) return;

	if (mode == SRM_90 || mode == SRM_270) {
		tmp_img = switch_img_alloc(NULL, (*img)->fmt, (*img)->d_h, (*img)->d_w, 1);
	} else {
		tmp_img = switch_img_alloc(NULL, (*img)->fmt, (*img)->d_w, (*img)->d_h, 1);
	}

	switch_assert(tmp_img);

	I420Rotate((*img)->planes[SWITCH_PLANE_Y], (*img)->stride[SWITCH_PLANE_Y],
			   (*img)->planes[SWITCH_PLANE_U], (*img)->stride[SWITCH_PLANE_U],
			   (*img)->planes[SWITCH_PLANE_V], (*img)->stride[SWITCH_PLANE_V],
			   tmp_img->planes[SWITCH_PLANE_Y], tmp_img->stride[SWITCH_PLANE_Y],
			   tmp_img->planes[SWITCH_PLANE_U], tmp_img->stride[SWITCH_PLANE_U],
			   tmp_img->planes[SWITCH_PLANE_V], tmp_img->stride[SWITCH_PLANE_V],
			   (*img)->d_w, (*img)->d_h, (int)mode);


	switch_img_free(img);
	*img = tmp_img;

#endif
}

SWITCH_DECLARE(void) switch_img_free(switch_image_t **img)
{
#ifdef SWITCH_HAVE_VPX
	if (img && *img) {
		if ((*img)->fmt == SWITCH_IMG_FMT_GD) {
#ifdef HAVE_LIBGD
			gdImageDestroy((gdImagePtr)(*img)->user_priv);
#endif
		} else {
			switch_safe_free((*img)->user_priv);
		}
		vpx_img_free((vpx_image_t *)*img);
		*img = NULL;
	}
#endif
}

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

SWITCH_DECLARE(void) switch_img_patch(switch_image_t *IMG, switch_image_t *img, int x, int y)
{
	int i, len, max_h;
	int xoff = 0, yoff = 0;

	switch_assert(IMG->fmt == SWITCH_IMG_FMT_I420);

	if (img->fmt == SWITCH_IMG_FMT_ARGB) {
		int max_w = MIN(img->d_w, IMG->d_w - abs(x));
		int max_h = MIN(img->d_h, IMG->d_h - abs(y));
		int j;
		uint8_t alpha;
		switch_rgb_color_t *rgb;

		for (i = 0; i < max_h; i++) {
			for (j = 0; j < max_w; j++) {
				alpha = img->planes[SWITCH_PLANE_PACKED][i * img->stride[SWITCH_PLANE_PACKED] + j * 4];

				if (alpha > 0) {
					switch_rgb_color_t RGB = { 0 };

					switch_img_get_rgb_pixel(IMG, &RGB, x + j, y + i);
					rgb = (switch_rgb_color_t *)(img->planes[SWITCH_PLANE_PACKED] + i * img->stride[SWITCH_PLANE_PACKED] + j * 4);

					if (alpha < 255) {
						RGB.a = 255;
						RGB.r = ((RGB.r * (255 - alpha)) >> 8) + ((rgb->r * alpha) >> 8);
						RGB.g = ((RGB.g * (255 - alpha)) >> 8) + ((rgb->g * alpha) >> 8);
						RGB.b = ((RGB.b * (255 - alpha)) >> 8) + ((rgb->b * alpha) >> 8);

						switch_img_draw_pixel(IMG, x + j, y + i, &RGB);
					} else {
						switch_img_draw_pixel(IMG, x + j, y + i, rgb);
					}
				}
			}
		}

		return;

#ifdef HAVE_LIBGD
	} else if (img->fmt == SWITCH_IMG_FMT_GD) {
		gdImagePtr gd = (gdImagePtr)img->user_priv;
		switch_rgb_color_t rgb_color;
		int pixel;
		int i, j;

		switch_assert(gd);

		if (!gd->trueColor) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "GD is experimental, only true color image is supported\n");
			return;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "truecolor: %d alpha: %d, transparent? %d\n", gd->trueColor, gd->saveAlphaFlag, gd->transparent);

		for(i = 0; i < img->d_h; i++) {
			for(j = 0; j < img->d_w; j++) {
				pixel = gd->tpixels[i][j];
				rgb_color.a = 255; // TODO: handle transparent
				rgb_color.r = gdTrueColorGetRed(pixel);
				rgb_color.g = gdTrueColorGetGreen(pixel);
				rgb_color.b = gdTrueColorGetBlue(pixel);
				switch_img_draw_pixel(IMG, x + j, y + i, &rgb_color);
			}
		}

		return;
#endif

	}

	if (x < 0) {
		xoff = -x;
		x = 0;
	}

	if (y < 0) {
		yoff = -y;
		y = 0;
	}

	max_h = MIN(y + img->d_h - yoff, IMG->d_h);
	len = MIN(img->d_w - xoff, IMG->d_w - x);


	if (x & 0x1) { x++; len--; }
	if (y & 0x1) y++;
	if (len <= 0) return;

	for (i = y; i < max_h; i++) {
		memcpy(IMG->planes[SWITCH_PLANE_Y] + IMG->stride[SWITCH_PLANE_Y] * i + x, img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * (i - y + yoff) + xoff, len);
	}

	if ((len & 1) && (x + len) < img->d_w - 1) len++;

	len /= 2;

	for (i = y; i < max_h; i += 2) {
		memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * (i / 2) + x / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * ((i - y + yoff) / 2) + xoff / 2, len);
		memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * (i / 2) + x / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * ((i - y + yoff) / 2) + xoff / 2, len);
	}
}

SWITCH_DECLARE(void) switch_img_patch_rect(switch_image_t *IMG, int X, int Y, switch_image_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
#ifdef SWITCH_HAVE_VPX
	switch_image_t *tmp;
	uint8_t *data;

	if (x >= img->d_w || y >= img->d_h) return;

	if (!(img->fmt & SWITCH_IMG_FMT_PLANAR)) {
		data = img->planes[SWITCH_PLANE_PACKED];
	} else {
		data = img->planes[SWITCH_PLANE_Y];
	}

	tmp = (switch_image_t *)vpx_img_wrap(NULL, img->fmt, img->d_w, img->d_h, 1, data);
	if (!tmp) return;

	w = MIN(img->d_w - x, w);
	h = MIN(img->d_h - y, h);

	if (!switch_img_set_rect(tmp, x, y, w, h)) {
		switch_img_patch(IMG, tmp, X, Y);
	}

	switch_img_free(&tmp);
#endif
}

SWITCH_DECLARE(void) switch_img_copy(switch_image_t *img, switch_image_t **new_img)
{
	switch_assert(img);
	switch_assert(new_img);

#ifdef SWITCH_HAVE_YUV
	if (img->fmt != SWITCH_IMG_FMT_I420 && img->fmt != SWITCH_IMG_FMT_ARGB) return;

	if (*new_img != NULL) {
		if (img->fmt != (*new_img)->fmt || img->d_w != (*new_img)->d_w || img->d_h != (*new_img)->d_w) {
			switch_img_free(new_img);
		}
	}

	if (*new_img == NULL) {
		*new_img = switch_img_alloc(NULL, img->fmt, img->d_w, img->d_h, 1);
	}

	switch_assert(*new_img);

	if (img->fmt == SWITCH_IMG_FMT_I420) {
		I420Copy(img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
				 img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
				 img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
				 (*new_img)->planes[SWITCH_PLANE_Y], (*new_img)->stride[SWITCH_PLANE_Y],
				 (*new_img)->planes[SWITCH_PLANE_U], (*new_img)->stride[SWITCH_PLANE_U],
				 (*new_img)->planes[SWITCH_PLANE_V], (*new_img)->stride[SWITCH_PLANE_V],
				 img->d_w, img->d_h);
	} else if (img->fmt == SWITCH_IMG_FMT_ARGB) {
		ARGBCopy(img->planes[SWITCH_PLANE_PACKED], img->stride[SWITCH_PLANE_PACKED],
				 (*new_img)->planes[SWITCH_PLANE_PACKED], (*new_img)->stride[SWITCH_PLANE_PACKED],
				 img->d_w, img->d_h);
	}
#else
	return;
#endif
}


SWITCH_DECLARE(void) switch_img_rotate_copy(switch_image_t *img, switch_image_t **new_img, switch_image_rotation_mode_t mode)
{
	switch_assert(img);
	switch_assert(new_img);

#ifdef SWITCH_HAVE_YUV
	if (img->fmt != SWITCH_IMG_FMT_I420) abort();

	if (*new_img != NULL) {
		if (img->fmt != (*new_img)->fmt || img->d_w != (*new_img)->d_w || img->d_h != (*new_img)->d_w) {
			switch_img_free(new_img);
		}
	}

	if (*new_img == NULL) {
		if (mode == SRM_90 || mode == SRM_270) {
			*new_img = switch_img_alloc(NULL, img->fmt, img->d_h, img->d_w, 1);
		} else {
			*new_img = switch_img_alloc(NULL, img->fmt, img->d_w, img->d_h, 1);
		}
	}

	switch_assert(*new_img);


	I420Rotate(img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
			   img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
			   img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
			   (*new_img)->planes[SWITCH_PLANE_Y], (*new_img)->stride[SWITCH_PLANE_Y],
			   (*new_img)->planes[SWITCH_PLANE_U], (*new_img)->stride[SWITCH_PLANE_U],
			   (*new_img)->planes[SWITCH_PLANE_V], (*new_img)->stride[SWITCH_PLANE_V],
			   img->d_w, img->d_h, (int)mode);
#else
	return;
#endif
}

SWITCH_DECLARE(switch_image_t *) switch_img_copy_rect(switch_image_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
#ifdef SWITCH_HAVE_VPX
	switch_image_t *new_img = NULL, *tmp;
	uint8_t *data;

	switch_assert(img);

	if (x >= img->d_w || y >= img->d_h) return NULL;

	if (!(img->fmt & SWITCH_IMG_FMT_PLANAR)) {
		data = img->planes[SWITCH_PLANE_PACKED];
	} else {
		data = img->planes[SWITCH_PLANE_Y];
	}

	tmp = (switch_image_t *)vpx_img_wrap(NULL, img->fmt, img->d_w, img->d_h, 1, data);
	if (!tmp) return NULL;

	w = MIN(img->d_w - x, w);
	h = MIN(img->d_h - y, h);

	if (!switch_img_set_rect(tmp, x, y, w, h)) {
		switch_img_copy(tmp, &new_img);
	}

	switch_img_free(&tmp);

	return new_img;
#else
	return NULL;
#endif
}

static inline void switch_img_draw_pixel(switch_image_t *img, int x, int y, switch_rgb_color_t *color)
{
#ifdef SWITCH_HAVE_YUV	
	switch_yuv_color_t yuv;

	if (x < 0 || y < 0 || x >= img->d_w || y >= img->d_h) return;

	if (img->fmt == SWITCH_IMG_FMT_I420) {
		switch_color_rgb2yuv(color, &yuv);

		img->planes[SWITCH_PLANE_Y][y * img->stride[SWITCH_PLANE_Y] + x] = yuv.y;

		if (((x & 0x1) == 0) && ((y & 0x1) == 0)) {// only draw on even position
			img->planes[SWITCH_PLANE_U][y / 2 * img->stride[SWITCH_PLANE_U] + x / 2] = yuv.u;
			img->planes[SWITCH_PLANE_V][y / 2 * img->stride[SWITCH_PLANE_V] + x / 2] = yuv.v;
		}
	} else if (img->fmt == SWITCH_IMG_FMT_ARGB) {
		uint8_t *alpha = img->planes[SWITCH_PLANE_PACKED] + img->d_w * 4 * y + x * 4;
		*(alpha    ) = color->a;
		*(alpha + 1) = color->r;
		*(alpha + 2) = color->g;
		*(alpha + 3) = color->b;
	}
#endif
}

SWITCH_DECLARE(void) switch_img_fill(switch_image_t *img, int x, int y, int w, int h, switch_rgb_color_t *color)
{
#ifdef SWITCH_HAVE_YUV	
	int len, i, max_h;
	switch_yuv_color_t yuv_color;

	if (x < 0 || y < 0 || x >= img->d_w || y >= img->d_h) return;

	if (img->fmt == SWITCH_IMG_FMT_I420) {
		switch_color_rgb2yuv(color, &yuv_color);

		max_h = MIN(y + h, img->d_h);
		len = MIN(w, img->d_w - x);

		if (x & 1) { x++; len--; }
		if (y & 1) y++;
		if (len <= 0) return;

		for (i = y; i < max_h; i++) {
			memset(img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * i + x, yuv_color.y, len);
		}

		if ((len & 1) && (x + len) < img->d_w - 1) len++;

		len /= 2;

		for (i = y; i < max_h; i += 2) {
			memset(img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * (i / 2) + x / 2, yuv_color.u, len);
			memset(img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * (i / 2) + x / 2, yuv_color.v, len);
		}
	} else if (img->fmt == SWITCH_IMG_FMT_ARGB) {
		for (i = 0; i < img->d_w; i++) {
			*(img->planes[SWITCH_PLANE_PACKED] + i * 4    ) = color->a;
			*(img->planes[SWITCH_PLANE_PACKED] + i * 4 + 1) = color->r;
			*(img->planes[SWITCH_PLANE_PACKED] + i * 4 + 2) = color->g;
			*(img->planes[SWITCH_PLANE_PACKED] + i * 4 + 3) = color->b;
		}

		for (i = 1; i < img->d_h; i++) {
			memcpy( img->planes[SWITCH_PLANE_PACKED] + i * img->d_w * 4,
					img->planes[SWITCH_PLANE_PACKED], img->d_w * 4);
		}
	}
#endif
}

#ifdef SWITCH_HAVE_YUV
static inline void switch_img_get_yuv_pixel(switch_image_t *img, switch_yuv_color_t *yuv, int x, int y)
{
	// switch_assert(img->fmt == SWITCH_IMG_FMT_I420);
	if (x < 0 || y < 0 || x >= img->d_w || y >= img->d_h) return;

	yuv->y = *(img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * y + x);
	yuv->u = *(img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * (y / 2) + x / 2);
	yuv->v = *(img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * (y / 2) + x / 2);
}
#endif

static inline void switch_img_get_rgb_pixel(switch_image_t *img, switch_rgb_color_t *rgb, int x, int y)
{
#ifdef SWITCH_HAVE_YUV		
	if (x < 0 || y < 0 || x >= img->d_w || y >= img->d_h) return;

	if (img->fmt == SWITCH_IMG_FMT_I420) {
		switch_yuv_color_t yuv;

		switch_img_get_yuv_pixel(img, &yuv, x, y);
		switch_color_yuv2rgb(&yuv, rgb);
	} else if (img->fmt == SWITCH_IMG_FMT_ARGB) {
		uint8_t *a = img->planes[SWITCH_PLANE_PACKED] + img->d_w * 4 * y + 4 * x;
		rgb->a = *a;
		rgb->r = *(++a);
		rgb->g = *(++a);
		rgb->b = *(++a);
	}
#endif	
}

SWITCH_DECLARE(void) switch_img_overlay(switch_image_t *IMG, switch_image_t *img, int x, int y, uint8_t percent)
{
	int i, j, len, max_h;
	switch_rgb_color_t RGB = {0}, rgb = {0}, c = {0};
	int xoff = 0, yoff = 0;
	uint8_t alpha = (int8_t)((255 * percent) / 100);


	switch_assert(IMG->fmt == SWITCH_IMG_FMT_I420);

	if (x < 0) {
		xoff = -x;
		x = 0;
	}

	if (y < 0) {
		yoff = -y;
		y = 0;
	}

	max_h = MIN(y + img->d_h - yoff, IMG->d_h);
	len = MIN(img->d_w - xoff, IMG->d_w - x);

	if (x & 1) { x++; len--; }
	if (y & 1) y++;
	if (len <= 0) return;

	for (i = y; i < max_h; i++) {
		for (j = 0; j < len; j++) {
			switch_img_get_rgb_pixel(IMG, &RGB, x + j, i);
			switch_img_get_rgb_pixel(img, &rgb, j + xoff, i - y + yoff);

			if (rgb.a > 0) {
				c.r = ((RGB.r * (255 - alpha)) >> 8) + ((rgb.r * alpha) >> 8);
				c.g = ((RGB.g * (255 - alpha)) >> 8) + ((rgb.g * alpha) >> 8);
				c.b = ((RGB.b * (255 - alpha)) >> 8) + ((rgb.b * alpha) >> 8);
			} else {
				c.r = RGB.r;
				c.g = RGB.g;
				c.b = RGB.b;
			}

			switch_img_draw_pixel(IMG, x + j, i, &c);
		}
	}
}

static uint8_t scv_art[14][16] = {
	{0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00},
	{0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00},
	{0x00, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x7E, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x00},
	{0x00, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x7E, 0x00},
	{0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00},
	{0x00, 0x7E, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x7E, 0x00},
	{0x00, 0x7E, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00},
	{0x00, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00},
	{0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x00},
	{0x00, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x42, 0x7E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x7E, 0x00},
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00}, /*.*/
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /*:*/
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /*-*/
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* */
};

static void scv_tag(void *buffer, int w, int x, int y, uint8_t n)
{
	int i = 0, j=0;
	uint8_t *p = buffer;

	if (n > 13) return;

	for(i=0; i<8; i++) {
		for (j=0; j<16; j++) {
			*( p + (y + j) * w + (x + i)) = (scv_art[n][j] & 0x80 >> i) ? 0xFF : 0x00;
		}
	}
}

SWITCH_DECLARE(void) switch_img_add_text(void *buffer, int w, int x, int y, char *s)
{
	while (*s) {
		int index;

		if (x > w - 8) break;

		switch (*s) {
			case '.': index = 10; break;
			case ':': index = 11; break;
			case '-': index = 12; break;
			case ' ': index = 13; break;
			default:
				index = *s - 0x30;
		}

		scv_tag(buffer, w, x, y, index);
		x += 8;
		s++;
	}
}

SWITCH_DECLARE(void) switch_color_set_rgb(switch_rgb_color_t *color, const char *str)
{
	if (zstr(str)) return;

	if ((*str) == '#' && strlen(str) == 7) {
		unsigned int r, g, b;
		sscanf(str, "#%02x%02x%02x", &r, &g, &b);
		color->r = r;
		color->g = g;
		color->b = b;
	} else {
		if (!strcmp(str, "red")) {
			color->r = 255;
			color->g = 0;
			color->b = 0;
		} else if (!strcmp(str, "green")) {
			color->r = 0;
			color->g = 255;
			color->b = 0;
		} else if (!strcmp(str, "blue")) {
			color->r = 0;
			color->g = 0;
			color->b = 255;
		}
	}
}

#ifdef SWITCH_HAVE_YUV
static inline void switch_color_rgb2yuv(switch_rgb_color_t *rgb, switch_yuv_color_t *yuv)
{
	yuv->y = (uint8_t)(((rgb->r * 4897) >> 14) + ((rgb->g * 9611) >> 14) + ((rgb->b * 1876) >> 14));
	yuv->u = (uint8_t)(- ((rgb->r * 2766) >> 14)  - ((5426 * rgb->g) >> 14) + rgb->b / 2 + 128);
	yuv->v = (uint8_t)(rgb->r / 2 -((6855 * rgb->g) >> 14) - ((rgb->b * 1337) >> 14) + 128);
}
#endif

#define CLAMP(val) MAX(0, MIN(val, 255))

#ifdef SWITCH_HAVE_YUV
static inline void switch_color_yuv2rgb(switch_yuv_color_t *yuv, switch_rgb_color_t *rgb)
{
#if 0
	int C = yuv->y - 16;
	int D = yuv->u - 128;
	int E = yuv->v - 128;

	rgb->r = CLAMP((298 * C           + 409 * E + 128) >> 8);
	rgb->g = CLAMP((298 * C - 100 * D - 208 * E + 128) >> 8);
	rgb->b = CLAMP((298 * C + 516 * D           + 128) >> 8);
#endif

	rgb->a = 255;
	rgb->r = CLAMP( yuv->y + ((22457 * (yuv->v-128)) >> 14));
	rgb->g = CLAMP((yuv->y - ((715   * (yuv->v-128)) >> 10) - ((5532 * (yuv->u-128)) >> 14)));
	rgb->b = CLAMP((yuv->y + ((28384 * (yuv->u-128)) >> 14)));
 }
#endif

SWITCH_DECLARE(void) switch_color_set_yuv(switch_yuv_color_t *color, const char *str)
{
#ifdef SWITCH_HAVE_YUV
	switch_rgb_color_t rgb = { 0 };

	switch_color_set_rgb(&rgb, str);
	switch_color_rgb2yuv(&rgb, color);
#endif	
}

#if SWITCH_HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#endif

#define MAX_GRADIENT 8

struct switch_img_txt_handle_s {
#if SWITCH_HAVE_FREETYPE
	FT_Library library;
	FT_Face face;
#endif
	char *font_family;
	double angle;
	uint16_t font_size;
	switch_rgb_color_t color;
	switch_rgb_color_t bgcolor;
	switch_image_t *img;
	switch_memory_pool_t *pool;
	int free_pool;
	switch_rgb_color_t gradient_table[MAX_GRADIENT];
	switch_bool_t use_bgcolor;
};

static void init_gradient_table(switch_img_txt_handle_t *handle)
{
	int i;
	switch_rgb_color_t *color;

	switch_rgb_color_t *c1 = &handle->bgcolor;
	switch_rgb_color_t *c2 = &handle->color;

	for (i = 0; i < MAX_GRADIENT; i++) {
		color = &handle->gradient_table[i];
		color->r = c1->r + (c2->r - c1->r) * i / MAX_GRADIENT;
		color->g = c1->g + (c2->g - c1->g) * i / MAX_GRADIENT;
		color->b = c1->b + (c2->b - c1->b) * i / MAX_GRADIENT;
	}
}

SWITCH_DECLARE(switch_status_t) switch_img_txt_handle_create(switch_img_txt_handle_t **handleP, const char *font_family,
															 const char *font_color, const char *bgcolor, uint16_t font_size, double angle, switch_memory_pool_t *pool)
{
	int free_pool = 0;
	switch_img_txt_handle_t *new_handle;

	if (!pool) {
		free_pool = 1;
		switch_core_new_memory_pool(&pool);
	}

	new_handle = switch_core_alloc(pool, sizeof(*new_handle));

#if SWITCH_HAVE_FREETYPE
	if (FT_Init_FreeType(&new_handle->library)) {
		return SWITCH_STATUS_FALSE;
	}
/*#else
	return SWITCH_STATUS_FALSE; */
#endif

	new_handle->pool = pool;
	new_handle->free_pool = free_pool;

	if (zstr(font_family)) {
		font_family = switch_core_sprintf(new_handle->pool, "%s%s%s",SWITCH_GLOBAL_dirs.fonts_dir, SWITCH_PATH_SEPARATOR, "FreeMono.ttf");
	}

	if (!switch_is_file_path(font_family)) {
		new_handle->font_family = switch_core_sprintf(new_handle->pool, "%s%s%s",SWITCH_GLOBAL_dirs.fonts_dir, SWITCH_PATH_SEPARATOR, font_family);
	} else {
		new_handle->font_family = switch_core_strdup(new_handle->pool, font_family);
	}

	if (switch_file_exists(new_handle->font_family, new_handle->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Font %s does not exist\n", new_handle->font_family);
		if (free_pool) {
			switch_core_destroy_memory_pool(&pool);
		}
		*handleP = NULL;
		return SWITCH_STATUS_FALSE;
	}

	new_handle->font_size = font_size;
	new_handle->angle = angle;

	switch_color_set_rgb(&new_handle->color, font_color);
	switch_color_set_rgb(&new_handle->bgcolor, bgcolor);

	init_gradient_table(new_handle);

	*handleP = new_handle;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(void) switch_img_txt_handle_destroy(switch_img_txt_handle_t **handleP)
{
	switch_img_txt_handle_t *old_handle;
	switch_memory_pool_t *pool;

	switch_assert(handleP);
	
	old_handle = *handleP;
	*handleP = NULL;
	if (!old_handle) return;
	

#if SWITCH_HAVE_FREETYPE
	if (old_handle->library) {
		FT_Done_FreeType(old_handle->library);
		old_handle->library = NULL;
	}
#endif
	pool = old_handle->pool;

	if (old_handle->free_pool) {
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
		old_handle = NULL;
	}

}

#if SWITCH_HAVE_FREETYPE
static void draw_bitmap(switch_img_txt_handle_t *handle, switch_image_t *img, FT_Bitmap* bitmap, FT_Int x, FT_Int y)
{
	FT_Int  i, j, p, q;
	FT_Int  x_max = x + bitmap->width;
	FT_Int  y_max = y + bitmap->rows;

	if (bitmap->width == 0) return;

	switch (bitmap->pixel_mode) {
		case FT_PIXEL_MODE_GRAY: // it should always be GRAY since we use FT_LOAD_RENDER?
			break;
		case FT_PIXEL_MODE_NONE:
		case FT_PIXEL_MODE_MONO:
		{
			for ( j = y, q = 0; j < y_max; j++, q++ ) {
				for ( i = x, p = 0; i < x_max; i++, p++ ) {
					uint8_t byte;
					int linesize = ((bitmap->width - 1) / 8 + 1) * 8;

					if ( i < 0 || j < 0 || i >= img->d_w || j >= img->d_h) continue;

					byte = bitmap->buffer[(q * linesize + p) / 8];
					if ((byte >> (7 - (p % 8))) & 0x1) {
						switch_img_draw_pixel(img, i, j, &handle->color);
					}
				}
			}
			return;
		}
		case FT_PIXEL_MODE_GRAY2:
		case FT_PIXEL_MODE_GRAY4:
		case FT_PIXEL_MODE_LCD:
		case FT_PIXEL_MODE_LCD_V:
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "unsupported pixel mode %d\n", bitmap->pixel_mode);
			return;
	}

	for ( i = x, p = 0; i < x_max; i++, p++ ) {
		for ( j = y, q = 0; j < y_max; j++, q++ ) {
			int gradient = bitmap->buffer[q * bitmap->width + p];
			if ( i < 0 || j < 0 || i >= img->d_w || j >= img->d_h) continue;

			if (handle->use_bgcolor) {
				switch_img_draw_pixel(img, i, j, &handle->gradient_table[gradient * MAX_GRADIENT / 256]);
			} else {
				switch_rgb_color_t rgb_color = {0};
				switch_rgb_color_t c;
				switch_img_get_rgb_pixel(img, &rgb_color, i, j);

				if (rgb_color.a > 0) {
					c.a = rgb_color.a * gradient / 255;
					c.r = ((rgb_color.r * (255 - gradient)) >> 8) + ((handle->color.r * gradient) >> 8);
					c.g = ((rgb_color.g * (255 - gradient)) >> 8) + ((handle->color.g * gradient) >> 8);
					c.b = ((rgb_color.b * (255 - gradient)) >> 8) + ((handle->color.b * gradient) >> 8);
				} else {
					c.a = gradient;
					c.r = handle->color.r;
					c.g = handle->color.g;
					c.b = handle->color.b;
				}

				switch_img_draw_pixel(img, i, j, &c);
			}
		}
	}
}
#endif


SWITCH_DECLARE(uint32_t) switch_img_txt_handle_render(switch_img_txt_handle_t *handle, switch_image_t *img,
															 int x, int y, const char *text,
															 const char *font_family, const char *font_color, 
															 const char *bgcolor, uint16_t font_size, double angle)
{
#if SWITCH_HAVE_FREETYPE
	FT_GlyphSlot  slot;
	FT_Matrix     matrix; /* transformation matrix */
	FT_Vector     pen;    /* untransformed origin  */
	FT_Error      error;
	//int           target_height;
	int           index = 0;
	FT_ULong      ch;
	FT_Face face;
	uint32_t width = 0;
	int this_x = 0, last_x = 0, space = 0;
	uint32_t ret;

	if (zstr(text)) return 0;

	if (!handle) return 0;

	switch_assert(!img || img->fmt == SWITCH_IMG_FMT_I420 || img->fmt == SWITCH_IMG_FMT_ARGB);

	if (font_family) {
		handle->font_family = switch_core_strdup(handle->pool, font_family);
	} else {
		font_family = handle->font_family;
	}

	if (font_size) {
		handle->font_size = font_size;
	} else {
		font_size = handle->font_size;
	}

	if (font_color) {
		switch_color_set_rgb(&handle->color, font_color);
	}

	if (bgcolor) {
		switch_color_set_rgb(&handle->bgcolor, bgcolor);
		handle->use_bgcolor = SWITCH_TRUE;
	} else {
		handle->use_bgcolor = SWITCH_FALSE;
	}

	handle->angle = angle;

	//angle         = 0; (45.0 / 360 ) * 3.14159 * 2;

	//target_height = img->d_h;

	error = FT_New_Face(handle->library, font_family, 0, &face); /* create face object */
	if (error) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open font %s\n", font_family);
		return 0;
	}

	/* use 50pt at 100dpi */
	error = FT_Set_Char_Size(face, 64 * font_size, 0, 96, 96); /* set character size */
	if (error) return 0;

	slot = face->glyph;

	if (handle->use_bgcolor && slot->bitmap.pixel_mode != FT_PIXEL_MODE_MONO) {
		init_gradient_table(handle);
	}

	/* set up matrix */
	matrix.xx = (FT_Fixed)( cos( angle ) * 0x10000L );
	matrix.xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
	matrix.yx = (FT_Fixed)( sin( angle ) * 0x10000L );
	matrix.yy = (FT_Fixed)( cos( angle ) * 0x10000L );

	pen.x = x;
	pen.y = y;

	while(*(text + index)) {
		ch = switch_u8_get_char((char *)text, &index);

		if (ch == '\n') {
			pen.x = x;
			pen.y += (font_size + font_size / 4);
			continue;
		}

		/* set transformation */
		FT_Set_Transform(face, &matrix, &pen);

		/* load glyph image into the slot (erase previous one) */
		error = FT_Load_Char(face, ch, FT_LOAD_RENDER);

		if (error) continue;

		this_x = pen.x + slot->bitmap_left;
		
		if (img) {
			/* now, draw to our target surface (convert position) */
			draw_bitmap(handle, img, &slot->bitmap, this_x, pen.y - slot->bitmap_top + font_size);
		}

		if (last_x) {
			space = this_x - last_x;
		} else {
			space = 0;
		}

		last_x = this_x;

		width += space;

		/* increment pen position */
		pen.x += slot->advance.x >> 6;
		pen.y += slot->advance.y >> 6;
	}

	ret = width + slot->bitmap.width * 5;

	FT_Done_Face(face);

	return ret;
#else
	return 0;
#endif
}

SWITCH_DECLARE(switch_image_t *) switch_img_write_text_img(int w, int h, switch_bool_t full, const char *text)
{
	const char *fg ="#cccccc";
	const char *bg = "#142e55";
	// const char *bg = NULL; // use a NULL bg for transparent
	const char *font_face = NULL;
	const char *fontsz = "4%";
	char *txt = "Value Optimized Out!";
	int argc = 0;
	char *argv[6] = { 0 };
	switch_rgb_color_t bgcolor = { 0 };
	int pre_width = 0, width = 0, font_size = 0, height = 0;
	int len = 0;
	char *duptxt = strdup(text);
	switch_img_txt_handle_t *txthandle = NULL;
	switch_image_t *txtimg = NULL;
	int x = 0, y = 0;

	if (strchr(text, ':')) {
		argc = switch_split(duptxt, ':', argv);
		
		if (argc > 0 && !zstr(argv[0])) {
			fg = argv[0];
		}

		if (argc > 1 && !zstr(argv[1])) {
			bg = argv[1];
		}
		
		if (argc > 2 && !zstr(argv[2])) {
			font_face = argv[2];
		}
		
		if (argc > 3 && !zstr(argv[3])) {
			fontsz = argv[3];
		}

		if (argc > 4) {
			txt = argv[4];
		}
	} else txt = duptxt;
	
	if (!txt) txt = duptxt;

	if (strrchr(fontsz, '%')) {
		font_size = 1 + ((int) (float)h * (atof(fontsz) / 100.0f));
	} else {
		font_size = atoi(fontsz);
	}

	while (*txt == ' ') txt++;
	while (end_of(txt) == ' ') end_of(txt) = '\0';
	
	len = strlen(txt);

	if (len < 5) len = 5;


	switch_img_txt_handle_create(&txthandle, font_face, fg, bg, font_size, 0, NULL);
	switch_color_set_rgb(&bgcolor, bg);

	pre_width = switch_img_txt_handle_render(txthandle,
											 NULL,
											 font_size / 2, font_size / 2,
											 txt, NULL, fg, bg, 0, 0);
	
	height = font_size * 2;
	
	if (full && w > width) {
		width = w;
	} else {
		width = pre_width;
	}

	if (bg) {
		txtimg = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
		switch_assert(txtimg);
		switch_img_fill(txtimg, 0, 0, txtimg->d_w, txtimg->d_h, &bgcolor);
	} else {
		txtimg = switch_img_alloc(NULL, SWITCH_IMG_FMT_ARGB, width, height, 1);
		switch_assert(txtimg);
		memset(txtimg->planes[SWITCH_PLANE_PACKED], 0, width * height * 4);
	}

	x = font_size / 2;
	y = font_size / 2;

	if (full) {
		x = (txtimg->d_w / 2) - (pre_width / 2);
	}

	switch_img_txt_handle_render(txthandle,
								txtimg,
								x, y,
								txt, NULL, fg, bg, 0, 0);
	switch_img_txt_handle_destroy(&txthandle);

	switch_safe_free(duptxt);

	return txtimg;
}

/* WARNING:
   patch a big IMG with a rect hole, note this function is WIP ......
   It ONLY works when the hole is INSIDE the big IMG and the place the small img will patch to,
   more sanity checks need to be decided
*/
SWITCH_DECLARE(void) switch_img_patch_hole(switch_image_t *IMG, switch_image_t *img, int x, int y, switch_image_rect_t *rect)
{
	int i, len;

	switch_assert(img->fmt == SWITCH_IMG_FMT_I420);
	switch_assert(IMG->fmt == SWITCH_IMG_FMT_I420);

	len = MIN(img->d_w, IMG->d_w - x);
	if (len <= 0) return;

	for (i = y; i < (y + img->d_h) && i < IMG->d_h; i++) {
		if (rect && i >= rect->y && i < (rect->y + rect->h)) {
			int size = rect->x > x ? rect->x - x : 0;
			memcpy(IMG->planes[SWITCH_PLANE_Y] + IMG->stride[SWITCH_PLANE_Y] * i + x, img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * (i - y), size);
			size = MIN(img->d_w - rect->w - size, IMG->d_w - (rect->x + rect->w));
			memcpy(IMG->planes[SWITCH_PLANE_Y] + IMG->stride[SWITCH_PLANE_Y] * i + rect->x + rect->w, img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * (i - y) + rect->w + (rect->x - x), size);
		} else {
			memcpy(IMG->planes[SWITCH_PLANE_Y] + IMG->stride[SWITCH_PLANE_Y] * i + x, img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * (i - y), len);
		}
	}

	len /= 2;

	for (i = y; i < (y + img->d_h) && i < IMG->d_h; i += 2) {
		if (rect && i > rect->y && i < (rect->y + rect->h)) {
			int size = rect->x > x ? rect->x - x : 0;

			size /= 2;
			memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * (i / 2) + x / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * ((i - y) / 2), size);
			memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * (i / 2) + x / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * ((i - y) / 2), size);
			size = MIN(img->d_w - rect->w - size, IMG->d_w - (rect->x + rect->w)) / 2;
			memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * (i / 2) + (rect->x + rect->w) / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * ((i - y) / 2) + (rect->w + (rect->x - x)) / 2, size);
			memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * (i / 2) + (rect->x + rect->w) / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * ((i - y) / 2) + (rect->w + (rect->x - x)) / 2, size);
		} else {
			memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * (i / 2) + x / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * ((i - y) / 2), len);
			memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * (i / 2) + x / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * ((i - y) / 2), len);
		}
	}
}

#define SWITCH_IMG_MAX_WIDTH  1920 * 2
#define SWITCH_IMG_MAX_HEIGHT 1080 * 2

#if !defined(SWITCH_HAVE_YUV)
#undef SWITCH_HAVE_PNG
#endif

#ifdef SWITCH_HAVE_PNG
// WIP png functions, need furthur tweak/check to make sure it works on all png files and errors are properly detected and reported
// #define PNG_DEBUG 3
#define PNG_SKIP_SETJMP_CHECK
#include <png.h>


#ifdef PNG_SIMPLIFIED_READ_SUPPORTED /* available from libpng 1.6.0 */

struct switch_png_opaque_s {
	png_image png;
	png_bytep buffer;
};



SWITCH_DECLARE(switch_status_t) switch_png_open(switch_png_t **pngP, const char *file_name)
{
	switch_png_t *use_png;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_zmalloc(use_png, sizeof(*use_png));
	switch_zmalloc(use_png->pvt, sizeof(struct switch_png_opaque_s));
	use_png->pvt->png.version = PNG_IMAGE_VERSION;

	if (!png_image_begin_read_from_file(&use_png->pvt->png, file_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error read PNG %s\n", file_name);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	use_png->pvt->png.format = PNG_FORMAT_ARGB;

	use_png->pvt->buffer = malloc(PNG_IMAGE_SIZE(use_png->pvt->png));
	switch_assert(use_png->pvt->buffer);

	if (!png_image_finish_read(&use_png->pvt->png, NULL/*background*/, use_png->pvt->buffer, 0, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error read PNG %s\n", file_name);
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}


	use_png->w = use_png->pvt->png.width;
	use_png->h = use_png->pvt->png.height;

end:

	if (status == SWITCH_STATUS_SUCCESS) {
		*pngP = use_png;
	} else {
		switch_png_free(&use_png);
		*pngP = NULL;
	}

	return status;
}

SWITCH_DECLARE(void) switch_png_free(switch_png_t **pngP)
{
	switch_png_t *use_png;

	if (pngP) {
		use_png = *pngP;
		*pngP = NULL;
		png_image_free(&use_png->pvt->png);
		switch_safe_free(use_png->pvt->buffer);
		switch_safe_free(use_png->pvt);
		switch_safe_free(use_png);
	}
}


SWITCH_DECLARE(switch_status_t) switch_png_patch_img(switch_png_t *use_png, switch_image_t *img, int x, int y)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_rgb_color_t *rgb_color;
	uint8_t alpha;
	int i, j;

	switch_assert(use_png);

	for (i = 0; i < use_png->pvt->png.height; i++) {
		for (j = 0; j < use_png->pvt->png.width; j++) {
			//alpha = use_png->pvt->buffer[i * use_png->pvt->png.width * 4 + j * 4 + 3];
			alpha = use_png->pvt->buffer[i * use_png->pvt->png.width * 4 + j * 4];
			// printf("%d, %d alpha: %d\n", j, i, alpha);

			if (alpha) { // todo, mux alpha with the underlying pixel
				rgb_color = (switch_rgb_color_t *)(use_png->pvt->buffer + i * use_png->pvt->png.width * 4 + j * 4);
				switch_img_draw_pixel(img, x + j, y + i, rgb_color);
			}
		}
	}

	return status;
}

#else /* libpng < 1.6.0 */

SWITCH_DECLARE(switch_status_t) switch_png_open(switch_png_t **pngP, const char *file_name)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NOT IMPLEMENTED\n");
	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(void) switch_png_free(switch_png_t **pngP)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NOT IMPLEMENTED\n");
}

SWITCH_DECLARE(switch_status_t) switch_png_patch_img(switch_png_t *use_png, switch_image_t *img, int x, int y)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "NOT IMPLEMENTED\n");
	return SWITCH_STATUS_FALSE;
}

#endif


#ifdef PNG_SIMPLIFIED_READ_SUPPORTED /* available from libpng 1.6.0 */

SWITCH_DECLARE(switch_image_t *) switch_img_read_png(const char* file_name, switch_img_fmt_t img_fmt)
{
	png_image png = { 0 };
	png_bytep buffer = NULL;
	switch_image_t *img = NULL;

	png.version = PNG_IMAGE_VERSION;

	if (!png_image_begin_read_from_file(&png, file_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error open png: %s\n", file_name);
		goto err;
	}

	if (img_fmt == SWITCH_IMG_FMT_I420) {
		png.format = PNG_FORMAT_RGB;
	} else if (img_fmt == SWITCH_IMG_FMT_ARGB) {
		png.format = PNG_FORMAT_ARGB;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported image format: %x\n", img_fmt);
		goto err;
	}

	buffer = malloc(PNG_IMAGE_SIZE(png));
	switch_assert(buffer);

	if (!png_image_finish_read(&png, NULL/*background*/, buffer, 0, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error read png: %s\n", file_name);
		goto err;
	}

	if (png.width > SWITCH_IMG_MAX_WIDTH || png.height > SWITCH_IMG_MAX_HEIGHT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PNG is too large! %dx%d\n", png.width, png.height);
		goto err;
	}

	img = switch_img_alloc(NULL, img_fmt, png.width, png.height, 1);
	switch_assert(img);

	if (img_fmt == SWITCH_IMG_FMT_I420) {
		RAWToI420(buffer, png.width * 3,
			img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
			img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
			img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
			png.width, png.height);
	} else if (img_fmt == SWITCH_IMG_FMT_ARGB){
		ARGBToARGB(buffer, png.width * 4,
			img->planes[SWITCH_PLANE_PACKED], png.width * 4,
			png.width, png.height);
	}

err:
	png_image_free(&png);
	switch_safe_free(buffer);
	return img;
}

#else /* libpng < 1.6.0 */

// ref: most are out-dated, man libpng :)
// http://zarb.org/~gc/html/libpng.html
// http://www.libpng.org/pub/png/book/toc.html
// http://www.vias.org/pngguide/chapter01_03_02.html
// http://www.libpng.org/pub/png/libpng-1.2.5-manual.html
// ftp://ftp.oreilly.com/examples/9781565920583/CDROM/SOFTWARE/SOURCE/LIBPNG/EXAMPLE.C

SWITCH_DECLARE(switch_image_t *) switch_img_read_png(const char* file_name, switch_img_fmt_t img_fmt)
{
	png_byte header[8];    // 8 is the maximum size that can be checked
	png_bytep *row_pointers = NULL;
	int y;

	int width, height;
	png_byte color_type;
	png_byte bit_depth;

	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	//int number_of_passes;
	int row_bytes;
	png_color_8p sig_bit;

	png_byte *buffer = NULL;
	switch_image_t *img = NULL;

	FILE *fp;

	if (img_fmt != SWITCH_IMG_FMT_I420 && img_fmt != SWITCH_IMG_FMT_ARGB) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Only ARGB and I420 are supported, you want 0x%x\n", img_fmt);
		return NULL;
	}

	/* open file and test for it being a png */
	fp = fopen(file_name, "rb");
	if (!fp) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File %s could not be opened for reading\n", file_name);
		goto end;
	}

	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File %s is not recognized as a PNG file\n", file_name);
		goto end;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "png_create_read_struct failed\n");
		goto end;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "png_create_info_struct failed\n");
		goto end;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error during init_io\n");
		goto end;
	}

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);
	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	//number_of_passes = png_set_interlace_handling(png_ptr);

	/* set up the transformations you want.  Note that these are
	all optional.  Only call them if you want them */

	/* expand paletted colors into true rgb */
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_expand(png_ptr);
	}

	/* expand grayscale images to the full 8 bits */
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand(png_ptr);
	}

	/* expand images with transparency to full alpha channels */
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_expand(png_ptr);
	}

	/* Set the background color to draw transparent and alpha images over */
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_bKGD)) {
		// png_get_bKGD(png_ptr, info_ptr, &my_background);
		// png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
	} else {
		// png_color_16 my_background = { 0 }; //{index,r, g, b, grey}
		// png_color_16 my_background = {0, 99, 99, 99, 0};
		// png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
	}

	/* tell libpng to handle the gamma conversion for you */
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_gAMA)) {
		// png_set_gamma(png_ptr, screen_gamma, info_ptr->gamma);
	} else {
		// png_set_gamma(png_ptr, screen_gamma, 0.45);
	}

	/* tell libpng to strip 16 bit depth files down to 8 bits */
	if (bit_depth == 16) {
		png_set_strip_16(png_ptr);
	}

#if 0
	/* dither rgb files down to 8 bit palettes & reduce palettes
	   to the number of colors available on your screen */
	if (0 && color_type & PNG_COLOR_MASK_COLOR) {
		if (png_get_valid(png_ptr, info_ptr, & PNG_INFO_PLTE)) {
			png_set_dither(png_ptr, info_ptr->palette,
						info_ptr->num_palette, max_screen_colors,
						info_ptr->histogram);
		} else {
			png_color std_color_cube[MAX_SCREEN_COLORS] =
						{/* ... colors ... */};

			png_set_dither(png_ptr, std_color_cube, MAX_SCREEN_COLORS,
						MAX_SCREEN_COLORS, NULL);
		}
	}
#endif

	/* invert monocrome files */
	if (bit_depth == 1 && color_type == PNG_COLOR_TYPE_GRAY) {
	// png_set_invert(png_ptr);
	}

	png_get_sBIT(png_ptr, info_ptr, &sig_bit);

	/* shift the pixels down to their true bit depth */
	// if (png_get_valid(png_ptr, info_ptr, PNG_INFO_sBIT) && (bit_depth > (*sig_bit).red)) {
	//	png_set_shift(png_ptr, sig_bit);
	// }

	/* pack pixels into bytes */
	if (bit_depth < 8) {
		png_set_packing(png_ptr);
	}

	/* flip the rgb pixels to bgr */
	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
		// png_set_bgr(png_ptr);
	}

	/* swap bytes of 16 bit files to least significant bit first */
	if (bit_depth == 16) {
		png_set_swap(png_ptr);
	}

	if (0 && color_type & PNG_COLOR_MASK_ALPHA) {
		if (setjmp(png_jmpbuf(png_ptr))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!!!!\n");
			goto end;
		}

		png_set_strip_alpha(png_ptr);
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error during read_updated_info\n");
		goto end;
	}

	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
	}

	png_read_update_info(png_ptr, info_ptr);

	color_type = png_get_color_type(png_ptr, info_ptr);
	// switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "color_type: 0x%x\n", color_type);

	if (width > SWITCH_IMG_MAX_WIDTH || height > SWITCH_IMG_MAX_HEIGHT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PNG is too large! %dx%d\n", width, height);
	}

	row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "size: %dx%d row_bytes:%d color_type:%d bit_dept:%d\n", width, height, row_bytes, color_type, bit_depth);

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
	switch_assert(row_pointers);

	buffer = (png_byte *)malloc(row_bytes * height);
	switch_assert(buffer);

	for (y = 0; y < height; y++) {
		row_pointers[y] = buffer + row_bytes * y;
	}

	/* read file */
	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error during read_image");
		goto end;
	}

	png_read_image(png_ptr, row_pointers);

	if (color_type == PNG_COLOR_TYPE_RGBA) {
		if (row_bytes > width * 4) {
			for(y = 1; y < height; y++) {
				memcpy(buffer + y * width * 4, row_pointers[y], width * 4);
			}
		}

		img = switch_img_alloc(NULL, img_fmt, width, height, 1);
		switch_assert(img);

		if (img_fmt == SWITCH_IMG_FMT_I420) {
			ABGRToI420(buffer, width * 4,
					img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
					img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
					img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
					width, height);
		} else if (img_fmt == SWITCH_IMG_FMT_ARGB) {
			ARGBToRGBA(buffer, width * 4,
					img->planes[SWITCH_PLANE_PACKED], width * 4,
					width, height);
		}
	} else if (color_type == PNG_COLOR_TYPE_RGB) {
		if (row_bytes > width * 3) {
			for(y = 1; y < height; y++) {
				memcpy(buffer + y * width * 3, row_pointers[y], width * 3);
			}
		}

		if (img_fmt == SWITCH_IMG_FMT_ARGB) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No alpha channel in image [%s], fallback to I420\n", file_name);
			img_fmt = SWITCH_IMG_FMT_I420;
		}

		img = switch_img_alloc(NULL, img_fmt, width, height, 1);
		switch_assert(img);

		RAWToI420(buffer, width * 3,
				img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
				img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
				img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
				width, height);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported color type: %d\n", png_get_color_type(png_ptr, info_ptr));
	}

end:
	switch_safe_free(buffer);
	switch_safe_free(row_pointers);
	if (fp) fclose(fp);
	if (info_ptr) png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	return img;
}

#endif


#ifdef PNG_SIMPLIFIED_WRITE_SUPPORTED /* available from libpng 1.6.0 */

SWITCH_DECLARE(switch_status_t) switch_img_write_png(switch_image_t *img, char* file_name)
{
	png_image png = { 0 };
	png_bytep buffer = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	buffer = malloc(img->d_w * img->d_h * 3);
	switch_assert(buffer);

	I420ToRAW(  img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
				img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
				img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
				buffer, img->d_w * 3,
				img->d_w, img->d_h);

	png.version = PNG_IMAGE_VERSION;
	png.format = PNG_FORMAT_RGB;
	png.width = img->d_w;
	png.height = img->d_h;

	if (!png_image_write_to_file(&png, file_name, 0, buffer, 0, NULL)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error write PNG %s\n", file_name);
		status = SWITCH_STATUS_FALSE;
	}

	switch_safe_free(buffer);
	return status;
}

#else

SWITCH_DECLARE(switch_status_t) switch_img_write_png(switch_image_t *img, char* file_name)
{
	int width, height;
	png_byte color_type;
	png_byte bit_depth;
	png_structp png_ptr;
	png_infop info_ptr;
	png_bytep *row_pointers = NULL;
	int row_bytes;
	int y;
	png_byte *buffer = NULL;
	FILE *fp = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	width = img->d_w;
	height = img->d_h;
	bit_depth = 8;
	color_type = PNG_COLOR_TYPE_RGB;

	fp = fopen(file_name, "wb");
	if (!fp) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File %s could not be opened for writing", file_name);
		goto end;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "png_create_write_struct failed");
		goto end;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "png_create_info_struct failed");
		goto end;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error during init_io");
		goto end;
	}

	png_init_io(png_ptr, fp);

	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error during writing header");
		goto end;
	}

	png_set_IHDR(png_ptr, info_ptr, width, height,
				 bit_depth, color_type, PNG_INTERLACE_NONE,
				 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);

	row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "size: %dx%d row_bytes:%d color_type:%d bit_dept:%d\n", width, height, row_bytes, color_type, bit_depth);

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
	switch_assert(row_pointers);

	buffer = (png_byte *)malloc(row_bytes * height);
	switch_assert(buffer);

	for (y = 0; y < height; y++) {
		row_pointers[y] = buffer + row_bytes * y;
	}

	I420ToRAW(  img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
				img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
				img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
				buffer, width * 3,
				width, height);

	for(y = height - 1; y > 0; y--) {
		// todo, check overlaps
		memcpy(row_pointers[y], buffer + row_bytes * y, width * 3);
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error during writing bytes");
		goto end;
	}

	png_write_image(png_ptr, row_pointers);

	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error during end of write");
		goto end;
	}

	png_write_end(png_ptr, NULL);

	status = SWITCH_STATUS_SUCCESS;

end:

	switch_safe_free(buffer);
	switch_safe_free(row_pointers);
	fclose(fp);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	return status;
}

#endif

#else

SWITCH_DECLARE(switch_status_t) switch_img_patch_png(switch_image_t *img, int x, int y, const char *file_name)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This function is not available, libpng not installed\n");
	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_image_t *) switch_img_read_png(const char* file_name, switch_img_fmt_t img_fmt)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This function is not available, libpng not installed\n");
	return NULL;
}

SWITCH_DECLARE(switch_status_t) switch_img_write_png(switch_image_t *img, char* file_name)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "This function is not available, libpng not installed\n");
	return SWITCH_STATUS_FALSE;
}

#endif

SWITCH_DECLARE(switch_status_t) switch_img_letterbox(switch_image_t *img, switch_image_t **imgP, int width, int height, const char *color)
{
	int img_w = 0, img_h = 0;
	double screen_aspect = 0, img_aspect = 0;
	int x_pos = 0;
	int y_pos = 0;
	switch_image_t *IMG = NULL, *scale_img = NULL;
	switch_rgb_color_t bgcolor = { 0 };
	
	switch_assert(imgP);
	*imgP = NULL;

	if (img->d_w == width && img->d_h == height) {
		switch_img_copy(img, imgP);
		return SWITCH_STATUS_SUCCESS;
	}

	IMG = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
	switch_color_set_rgb(&bgcolor, color);
	switch_img_fill(IMG, 0, 0, IMG->d_w, IMG->d_h, &bgcolor);

	img_w = IMG->d_w;
	img_h = IMG->d_h;

	screen_aspect = (double) IMG->d_w / IMG->d_h;
	img_aspect = (double) img->d_w / img->d_h;
	

	if (screen_aspect > img_aspect) {
		img_w = img_aspect * IMG->d_h;
		x_pos = (IMG->d_w - img_w) / 2;
	} else if (screen_aspect < img_aspect) {
		img_h = IMG->d_w / img_aspect;
		y_pos = (IMG->d_h - img_h) / 2;
	}
	
	switch_img_scale(img, &scale_img, img_w, img_h);
	switch_img_patch(IMG, scale_img, x_pos, y_pos);
	switch_img_free(&scale_img);

	*imgP = IMG;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_img_fit(switch_image_t **srcP, int width, int height, switch_img_fit_t fit)
{
	switch_image_t *src, *tmp = NULL;
	int new_w = 0, new_h = 0;

	switch_assert(srcP);
	switch_assert(width && height);

	src = *srcP;

	if (!src || (src->d_w == width && src->d_h == height)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (fit == SWITCH_FIT_SCALE) {
		switch_img_scale(src, &tmp, width, height);
		switch_img_free(&src);
		*srcP = tmp;
		return SWITCH_STATUS_SUCCESS;
	}

	new_w = src->d_w;
	new_h = src->d_h;
	
	if (src->d_w < width && src->d_h < height) {
		float rw = (float)new_w / width;
		float rh = (float)new_h / height;

		if (rw > rh) {
			new_h = (int)((float)new_h / rw);
			new_w = width;
		} else {
			new_w = (int)((float)new_w / rh);
			new_h = height;
		}
	} else {
		while(new_w > width || new_h > height) { 
			if (new_w > width) {
				double m = (double) width / new_w;
				new_w = width;
				new_h = (int) (new_h * m);
			} else {
				double m = (double) height / new_h;
				new_h = height;
				new_w = (int) (new_w * m);
			}
		}
	}

	if (new_w && new_h) {
		if (switch_img_scale(src, &tmp, new_w, new_h) == SWITCH_STATUS_SUCCESS) {
			switch_img_free(&src);
			*srcP = tmp;

			if (fit == SWITCH_FIT_SIZE_AND_SCALE) {
				src = *srcP;
				switch_img_scale(src, &tmp, width, height);
				switch_img_free(&src);
				*srcP = tmp;
			}

			return SWITCH_STATUS_SUCCESS;
		}
	}

	return SWITCH_STATUS_FALSE;
}

#ifdef SWITCH_HAVE_YUV
static inline uint32_t switch_img_fmt2fourcc(switch_img_fmt_t fmt)
{
	uint32_t fourcc;

	switch(fmt) {
		case SWITCH_IMG_FMT_NONE:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_RGB24:     fourcc = FOURCC_24BG; break;
		case SWITCH_IMG_FMT_RGB32:     fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_RGB565:    fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_RGB555:    fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_UYVY:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_YUY2:      fourcc = FOURCC_YUY2; break;
		case SWITCH_IMG_FMT_YVYU:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_BGR24:     fourcc = FOURCC_RAW ; break;
		case SWITCH_IMG_FMT_RGB32_LE:  fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_ARGB:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_ARGB_LE:   fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_RGB565_LE: fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_RGB555_LE: fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_YV12:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I420:      fourcc = FOURCC_I420; break;
		case SWITCH_IMG_FMT_VPXYV12:   fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_VPXI420:   fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I422:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I444:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I440:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_444A:      fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I42016:    fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I42216:    fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I44416:    fourcc = FOURCC_ANY ; break;
		case SWITCH_IMG_FMT_I44016:    fourcc = FOURCC_ANY ; break;
		default: fourcc = FOURCC_ANY;
	}

	return fourcc;
}
#endif

SWITCH_DECLARE(switch_status_t) switch_img_to_raw(switch_image_t *src, void *dest, switch_size_t size, switch_img_fmt_t fmt)
{
#ifdef SWITCH_HAVE_YUV
	uint32_t fourcc;
	int ret;

	switch_assert(src->fmt == SWITCH_IMG_FMT_I420); // todo: support other formats
	switch_assert(dest);

	fourcc = switch_img_fmt2fourcc(fmt);

	if (fourcc == FOURCC_ANY) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported format: %d\n", fmt);
		return SWITCH_STATUS_FALSE;
	}

	ret = ConvertFromI420(src->planes[0], src->stride[0],
					src->planes[1], src->stride[1],
					src->planes[2], src->stride[2],
					dest, size,
					src->d_w, src->d_h,
					fourcc);

	return ret == 0 ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
#else
	return SWITCH_STATUS_FALSE;
#endif
}

SWITCH_DECLARE(switch_status_t) switch_img_from_raw(switch_image_t *dest, void *src, switch_img_fmt_t fmt, int width, int height)
{
#ifdef SWITCH_HAVE_YUV
	uint32_t fourcc;
	int ret;

	fourcc = switch_img_fmt2fourcc(fmt);

	if (fourcc == FOURCC_ANY) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unsupported format: %d\n", fmt);
		return SWITCH_STATUS_FALSE;
	}

	if (!dest && width > 0 && height > 0) dest = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
	if (!dest) return SWITCH_STATUS_FALSE;

	if (width == 0 || height == 0) {
		width = dest->d_w;
		height = dest->d_h;
	}

/*
	int ConvertToI420(const uint8* src_frame, size_t src_size,
			uint8* dst_y, int dst_stride_y,
			uint8* dst_u, int dst_stride_u,
			uint8* dst_v, int dst_stride_v,
			int crop_x, int crop_y,
			int src_width, int src_height,
			int crop_width, int crop_height,
			enum RotationMode rotation,
			uint32 format);

	src_size is only used when FOURCC_MJPG which we don't support so always 0
*/

	ret = ConvertToI420(src, 0,
					dest->planes[0], dest->stride[0],
					dest->planes[1], dest->stride[1],
					dest->planes[2], dest->stride[2],
					0, 0,
					width, height,
					width, height,
					0, fourcc);

	return ret == 0 ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
#else
	return SWITCH_STATUS_FALSE;
#endif
}

SWITCH_DECLARE(switch_status_t) switch_img_scale(switch_image_t *src, switch_image_t **destP, int width, int height)
{
#ifdef SWITCH_HAVE_YUV
	switch_image_t *dest = NULL;
	int ret = 0;

	if (destP) {
		dest = *destP;
	}

	if (!dest) dest = switch_img_alloc(NULL, src->fmt, width, height, 1);

	switch_assert(src->fmt == dest->fmt);

	if (src->fmt == SWITCH_IMG_FMT_I420) {
		ret = I420Scale(src->planes[0], src->stride[0],
						src->planes[1], src->stride[1],
						src->planes[2], src->stride[2],
						src->d_w, src->d_h,
						dest->planes[0], dest->stride[0],
						dest->planes[1], dest->stride[1],
						dest->planes[2], dest->stride[2],
						width, height,
						kFilterBox);
	} else if (src->fmt == SWITCH_IMG_FMT_ARGB) {
		ret = ARGBScale(src->planes[SWITCH_PLANE_PACKED], src->d_w * 4,
				src->d_w, src->d_h,
				dest->planes[SWITCH_PLANE_PACKED], width * 4,
				width, height,
				kFilterBox);
	}

	if (ret != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Scaling Error: ret: %d\n", ret);
		return SWITCH_STATUS_FALSE;
	}

	if (destP) {
		*destP = dest;
	}
	
	return SWITCH_STATUS_SUCCESS;
#else
	return SWITCH_STATUS_FALSE;
#endif
}

SWITCH_DECLARE(void) switch_img_find_position(switch_img_position_t pos, int sw, int sh, int iw, int ih, int *xP, int *yP)
{
	switch(pos) {
	case POS_NONE:
	case POS_LEFT_TOP:
		*xP = 0;
		*yP = 0;
		break;
	case POS_LEFT_MID:
		*xP = 0;
		*yP = (sh - ih) / 2;
		break;
	case POS_LEFT_BOT:
		*xP = 0;
		*yP = (sh - ih);
		break;
	case POS_CENTER_TOP:
		*xP = (sw - iw) / 2;
		*yP = 0;
		break;
	case POS_CENTER_MID:
		*xP = (sw - iw) / 2;
		*yP = (sh - ih) / 2;
		break;
	case POS_CENTER_BOT:
		*xP = (sw - iw) / 2;
		*yP = (sh - ih);
		break;
	case POS_RIGHT_TOP:
		*xP = (sw - iw);
		*yP = 0;
		break;
	case POS_RIGHT_MID:
		*xP = (sw - iw);
		*yP = (sh - ih) / 2;
		break;
	case POS_RIGHT_BOT:	
		*xP = (sw - iw);
		*yP = (sh - ih);
		break;
	};

}

#ifdef HAVE_LIBGD
SWITCH_DECLARE(switch_image_t *) switch_img_read_file(const char* file_name)
{
	switch_image_t *img = switch_img_alloc(NULL, SWITCH_IMG_FMT_ARGB, 1, 1, 1);
	gdImagePtr gd = NULL;
	char *ext;
	FILE *fp;

	if (!img) return NULL;

	// gd = gdImageCreateFromFile(file_name); // only available in 2.1.1

	ext = strrchr(file_name, '.');
	if (!ext) goto err;

	fp = fopen(file_name, "rb");
	if (!fp) goto err;

	if (!strcmp(ext, ".png")) {
		gd = gdImageCreateFromPng(fp);
	} else if (!strcmp(ext, ".gif")) {
		gd = gdImageCreateFromGif(fp);
	} else if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg")) {
		gd = gdImageCreateFromJpeg(fp);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not supported file type: %s\n", ext);
	}

	fclose(fp);
	if (!gd) goto err;

	img->fmt = SWITCH_IMG_FMT_GD;
	img->d_w = gd->sx;
	img->d_h = gd->sy;
	img->user_priv = gd;
	return img;

err:
	switch_img_free(&img);
	return NULL;
}
#else
SWITCH_DECLARE(switch_image_t *) switch_img_read_file(const char* file_name)
{
	return NULL;
}
#endif

SWITCH_DECLARE(switch_status_t) switch_I420_copy(const uint8_t *src_y, int src_stride_y,
												 const uint8_t *src_u, int src_stride_u,
												 const uint8_t *src_v, int src_stride_v,
												 uint8_t *dst_y, int dst_stride_y,
												 uint8_t *dst_u, int dst_stride_u,
												 uint8_t *dst_v, int dst_stride_v,
												 int width, int height)
{
#ifdef SWITCH_HAVE_YUV
	int ret = I420Copy(src_y, src_stride_y, src_u, src_stride_u, src_v, src_stride_v,
					   dst_y, dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
					   width, height);
	return ret == 0 ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
#else
	return SWITCH_STATUS_FALSE;
#endif
}

SWITCH_DECLARE(switch_status_t) switch_I420_copy2(uint8_t *src_planes[], int src_stride[],
												  uint8_t *dst_planes[], int dst_stride[],
												  int width, int height)
{
#ifdef SWITCH_HAVE_YUV
	int ret = I420Copy(src_planes[SWITCH_PLANE_Y], src_stride[SWITCH_PLANE_Y],
					   src_planes[SWITCH_PLANE_U], src_stride[SWITCH_PLANE_U],
					   src_planes[SWITCH_PLANE_V], src_stride[SWITCH_PLANE_V],
					   dst_planes[SWITCH_PLANE_Y], dst_stride[SWITCH_PLANE_Y],
					   dst_planes[SWITCH_PLANE_U], dst_stride[SWITCH_PLANE_U],
					   dst_planes[SWITCH_PLANE_V], dst_stride[SWITCH_PLANE_V],
					   width, height);
	return ret == 0 ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
#else
	return SWITCH_STATUS_FALSE;
#endif
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

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

#include <switch.h>
#include <switch_utf8.h>
#include <libyuv.h>

SWITCH_DECLARE(switch_image_t *)switch_img_alloc(switch_image_t  *img,
						 switch_img_fmt_t fmt,
						 unsigned int d_w,
						 unsigned int d_h,
						 unsigned int align)
{
	return (switch_image_t *)vpx_img_alloc((vpx_image_t *)img, (vpx_img_fmt_t)fmt, d_w, d_h, align);
}

SWITCH_DECLARE(switch_image_t *)switch_img_wrap(switch_image_t  *img,
						switch_img_fmt_t fmt,
						unsigned int d_w,
						unsigned int d_h,
						unsigned int align,
						unsigned char      *img_data)
{
	return (switch_image_t *)vpx_img_wrap((vpx_image_t *)img, (vpx_img_fmt_t)fmt, d_w, d_h, align, img_data);
}

SWITCH_DECLARE(int) switch_img_set_rect(switch_image_t  *img,
				   unsigned int  x,
				   unsigned int  y,
				   unsigned int  w,
				   unsigned int  h)
{
	return vpx_img_set_rect((vpx_image_t *)img, x, y, w, h);
}

SWITCH_DECLARE(void) switch_img_flip(switch_image_t *img)
{
	vpx_img_flip((vpx_image_t *)img);
}

SWITCH_DECLARE(void) switch_img_free(switch_image_t **img)
{
	if (img && *img) {
		vpx_img_free((vpx_image_t *)*img);
		*img = NULL;
	}
}

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

// simple implementation to patch a small img to a big IMG at position x,y
SWITCH_DECLARE(void) switch_img_patch(switch_image_t *IMG, switch_image_t *img, int x, int y)
{
	int i, len, max_h;

	switch_assert(img->fmt == SWITCH_IMG_FMT_I420);
	switch_assert(IMG->fmt == SWITCH_IMG_FMT_I420);

	max_h = MIN(y + img->d_h, IMG->d_h);
	len = MIN(img->d_w, IMG->d_w - x);

	if (x & 0x1) { x++; len--; }
	if (y & 0x1) y++;
	if (len <= 0) return;

	for (i = y; i < (y + img->d_h) && i < IMG->d_h; i++) {
		memcpy(IMG->planes[SWITCH_PLANE_Y] + IMG->stride[SWITCH_PLANE_Y] * i + x, img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * (i - y), len);
	}

	if ((len & 1) && (x + len) < img->d_w) len++;

	len /= 2;

	for (i = y; i < max_h; i += 2) {
		memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * i / 2 + x / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * (i - y) / 2, len);
		memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * i / 2 + x / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * (i - y) / 2, len);
	}
}

SWITCH_DECLARE(void) switch_img_copy(switch_image_t *img, switch_image_t **new_img)
{
	int i = 0;

	switch_assert(img);
	switch_assert(new_img);

	if (!img->fmt == SWITCH_IMG_FMT_I420) return;

	if (*new_img != NULL) {
		if (img->d_w != (*new_img)->d_w || img->d_h != (*new_img)->d_w) {
			switch_img_free(new_img);
		}
	}

	if (*new_img == NULL) {
		*new_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, img->d_w, img->d_h, 1);
	}

	switch_assert(*new_img);

	for (i = 0; i < (*new_img)->h; i++) {
		memcpy((*new_img)->planes[SWITCH_PLANE_Y] + (*new_img)->stride[SWITCH_PLANE_Y] * i, img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * i, img->d_w);
	}

	for (i = 0; i < (*new_img)->h / 2; i++) {
		memcpy((*new_img)->planes[SWITCH_PLANE_U] + (*new_img)->stride[SWITCH_PLANE_U] * i, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * i, img->d_w / 2);
		memcpy((*new_img)->planes[SWITCH_PLANE_V] + (*new_img)->stride[SWITCH_PLANE_V] * i, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * i, img->d_w /2);
	}

}

SWITCH_DECLARE(switch_image_t *) switch_img_copy_rect(switch_image_t *img, int x, int y, int w, int h)
{
	switch_image_t *new_img = NULL;
	int i = 0;
	int len;

	switch_assert(img);
	switch_assert(x >= 0 && y >= 0 && w >= 0 && h >= 0);

	if (!img->fmt == SWITCH_IMG_FMT_I420) return NULL;

	new_img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, w, h, 1);
	if (new_img == NULL) return NULL;

	len = MIN(img->d_w - x, w);
	if (len <= 0) return NULL;

	for (i = 0; i < (img->d_h - y) && i < h; i++) {
		memcpy(new_img->planes[SWITCH_PLANE_Y] + new_img->stride[SWITCH_PLANE_Y] * i, img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * (y + i) + x, len);
	}

	len /= 2;

	for (i = 0; i < (img->d_h - y) && i < h; i += 2) {
		memcpy(new_img->planes[SWITCH_PLANE_U] + new_img->stride[SWITCH_PLANE_U] * i / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * (y + i) / 2 + x / 2, len);
		memcpy(new_img->planes[SWITCH_PLANE_V] + new_img->stride[SWITCH_PLANE_V] * i / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * (y + i) / 2 + x / 2, len);
	}
	return new_img;
}

SWITCH_DECLARE(void) switch_img_draw_pixel(switch_image_t *img, int x, int y, switch_yuv_color_t *color)
{
	if (x < 0 || y < 0 || x >= img->d_w || y >= img->d_h) return;

	img->planes[SWITCH_PLANE_Y][y * img->stride[SWITCH_PLANE_Y] + x] = color->y;

	if (((x & 0x1) == 0) && ((y & 0x1) == 0)) {// only draw on even position
		img->planes[SWITCH_PLANE_U][y / 2 * img->stride[SWITCH_PLANE_U] + x / 2] = color->u;
		img->planes[SWITCH_PLANE_V][y / 2 * img->stride[SWITCH_PLANE_V] + x / 2] = color->v;
	}
}

SWITCH_DECLARE(void) switch_img_fill(switch_image_t *img, int x, int y, int w, int h, switch_rgb_color_t *color)
{
	int len, i, max_h;
	switch_yuv_color_t yuv_color;

	if (x < 0 || y < 0 || x >= img->d_w || y >= img->d_h) return;

	switch_color_rgb2yuv(color, &yuv_color);

	max_h = MIN(y + h, img->d_h);
	len = MIN(w, img->d_w - x);

	if (x & 1) { x++; len--; }
	if (y & 1) y++;
	if (len <= 0) return;

	for (i = y; i < (y + h) && i < img->d_h; i++) {
		memset(img->planes[SWITCH_PLANE_Y] + img->stride[SWITCH_PLANE_Y] * i + x, yuv_color.y, len);
	}

	if ((len & 1) && (x + len) < img->d_w) len++;

	len /= 2;

	for (i = y; i < max_h; i += 2) {
		memset(img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * i / 2 + x / 2, yuv_color.u, len);
		memset(img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * i / 2 + x / 2, yuv_color.v, len);
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

	if (n < 0 || n > 13) return;

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

SWITCH_DECLARE(void) switch_color_rgb2yuv(switch_rgb_color_t *rgb, switch_yuv_color_t *yuv)
{
	yuv->y = (uint8_t)(((rgb->r * 4897) >> 14) + ((rgb->g * 9611) >> 14) + ((rgb->b * 1876) >> 14));
	yuv->u = (uint8_t)(- ((rgb->r * 2766) >> 14)  - ((5426 * rgb->g) >> 14) + rgb->b / 2 + 128);
	yuv->v = (uint8_t)(rgb->r / 2 -((6855 * rgb->g) >> 14) - ((rgb->b * 1337) >> 14) + 128);
}

#define CLAMP(val) MAX(0, MIN(val, 255))

SWITCH_DECLARE(void) switch_color_yuv2rgb(switch_yuv_color_t *yuv, switch_rgb_color_t *rgb)
{
#if 0
	int C = yuv->y - 16;
	int D = yuv->u - 128;
	int E = yuv->v - 128;

	rgb->r = CLAMP((298 * C           + 409 * E + 128) >> 8);
	rgb->g = CLAMP((298 * C - 100 * D - 208 * E + 128) >> 8);
	rgb->b = CLAMP((298 * C + 516 * D           + 128) >> 8);
#endif

	rgb->r = CLAMP( yuv->y + ((22457 * (yuv->v-128)) >> 14));
	rgb->g = CLAMP((yuv->y - ((715   * (yuv->v-128)) >> 10) - ((5532 * (yuv->u-128)) >> 14)));
	rgb->b = CLAMP((yuv->y + ((28384 * (yuv->u-128)) >> 14)));
}

SWITCH_DECLARE(void) switch_color_set_yuv(switch_yuv_color_t *color, const char *str)
{
	switch_rgb_color_t rgb = { 0 };

	switch_color_set_rgb(&rgb, str);
	switch_color_rgb2yuv(&rgb, color);
}

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#define MAX_GRADIENT 8

struct switch_img_txt_handle_s {
	FT_Library library;
	FT_Face face;
	char *font_family;
	double angle;
	uint16_t font_size;
	switch_rgb_color_t color;
	switch_rgb_color_t bgcolor;
	switch_image_t *img;
	switch_memory_pool_t *pool;
	int free_pool;
	switch_yuv_color_t gradient_table[MAX_GRADIENT];
	switch_bool_t use_bgcolor;
};

static void init_gradient_table(switch_img_txt_handle_t *handle)
{
	int i;
	switch_rgb_color_t color;

	switch_rgb_color_t *c1 = &handle->bgcolor;
	switch_rgb_color_t *c2 = &handle->color;

	for (i = 0; i < MAX_GRADIENT; i++) {
		color.r = c1->r + (c2->r - c1->r) * i / MAX_GRADIENT;
		color.g = c1->g + (c2->g - c1->g) * i / MAX_GRADIENT;
		color.b = c1->b + (c2->b - c1->b) * i / MAX_GRADIENT;

		switch_color_rgb2yuv(&color, &handle->gradient_table[i]);
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

	if (FT_Init_FreeType(&new_handle->library)) {
		return SWITCH_STATUS_FALSE;
	}

	new_handle->pool = pool;
	new_handle->free_pool = free_pool;
	new_handle->font_family = switch_core_strdup(new_handle->pool, font_family);
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
	switch_img_txt_handle_t *old_handle = *handleP;
	switch_memory_pool_t *pool;

	*handleP = NULL;

	if (old_handle->library) {
		FT_Done_FreeType(old_handle->library);
		old_handle->library = NULL;
	}

	pool = old_handle->pool;

	if (old_handle->free_pool) {
		switch_core_destroy_memory_pool(&pool);
		pool = NULL;
		old_handle = NULL;
	}

}

static void draw_bitmap(switch_img_txt_handle_t *handle, switch_image_t *img, FT_Bitmap* bitmap, FT_Int x, FT_Int y)
{
	FT_Int  i, j, p, q;
	FT_Int  x_max = x + bitmap->width;
	FT_Int  y_max = y + bitmap->rows;
	switch_yuv_color_t yuv_color;

	if (bitmap->width == 0) return;

	switch_color_rgb2yuv(&handle->color, &yuv_color);

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
						switch_img_draw_pixel(img, i, j, &yuv_color);
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
				if (gradient > 128) {
					switch_img_draw_pixel(img, i, j, &yuv_color);
				}
			}
		}
	}
}


SWITCH_DECLARE(switch_status_t) switch_img_txt_handle_render(switch_img_txt_handle_t *handle, switch_image_t *img,
															 int x, int y, const char *text,
															 const char *font_family, const char *font_color, const char *bgcolor, uint16_t font_size, double angle)
{
	FT_GlyphSlot  slot;
	FT_Matrix     matrix; /* transformation matrix */
	FT_Vector     pen;    /* untransformed origin  */
	FT_Error      error;
	//int           target_height;
	int           index = 0;
	FT_ULong      ch;
	FT_Face face;

	if (zstr(text)) return SWITCH_STATUS_FALSE;

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
		return SWITCH_STATUS_FALSE;
	}

	/* use 50pt at 100dpi */
	error = FT_Set_Char_Size(face, 64 * font_size, 0, 96, 96); /* set character size */
	if (error) {printf("WTF %d\n", __LINE__); return SWITCH_STATUS_FALSE;}

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

		/* now, draw to our target surface (convert position) */
		draw_bitmap(handle, img, &slot->bitmap, pen.x + slot->bitmap_left, pen.y - slot->bitmap_top + font_size);

		/* increment pen position */
		pen.x += slot->advance.x >> 6;
		pen.y += slot->advance.y >> 6;
	}

	FT_Done_Face(face);

	return SWITCH_STATUS_SUCCESS;
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
			memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * i / 2 + x / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * (i - y) / 2, size);
			memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * i / 2 + x / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * (i - y) / 2, size);
			size = MIN(img->d_w - rect->w - size, IMG->d_w - (rect->x + rect->w)) / 2;
			memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * i / 2 + (rect->x + rect->w) / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * (i - y) / 2 + (rect->w + (rect->x - x)) / 2, size);
			memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * i / 2 + (rect->x + rect->w) / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * (i - y) / 2 + (rect->w + (rect->x - x)) / 2, size);
		} else {
			memcpy(IMG->planes[SWITCH_PLANE_U] + IMG->stride[SWITCH_PLANE_U] * i / 2 + x / 2, img->planes[SWITCH_PLANE_U] + img->stride[SWITCH_PLANE_U] * (i - y) / 2, len);
			memcpy(IMG->planes[SWITCH_PLANE_V] + IMG->stride[SWITCH_PLANE_V] * i / 2 + x / 2, img->planes[SWITCH_PLANE_V] + img->stride[SWITCH_PLANE_V] * (i - y) / 2, len);
		}
	}
}

#define SWITCH_IMG_MAX_WIDTH  1920 * 2
#define SWITCH_IMG_MAX_HEIGHT 1080 * 2

// WIP png functions, need furthur tweak/check to make sure it works on all png files and errors are properly detected and reported
// #define PNG_DEBUG 3
#define PNG_SKIP_SETJMP_CHECK
#include <png.h>

// ref: most are out-dated, man libpng :)
// http://zarb.org/~gc/html/libpng.html
// http://www.libpng.org/pub/png/book/toc.html
// http://www.vias.org/pngguide/chapter01_03_02.html
// http://www.libpng.org/pub/png/libpng-1.2.5-manual.html
// ftp://ftp.oreilly.com/examples/9781565920583/CDROM/SOFTWARE/SOURCE/LIBPNG/EXAMPLE.C

SWITCH_DECLARE(switch_image_t *) switch_img_read_png(const char* file_name)
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
    // png_color_16 my_background = { 0 }; //{index,r, g, b, grey}
    png_color_16 my_background = {0, 99, 99, 99, 0};

	png_byte *buffer = NULL;
	switch_image_t *img = NULL;

	/* open file and test for it being a png */
	FILE *fp = fopen(file_name, "rb");
	if (!fp) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File %s could not be opened for reading", file_name);
		goto end;
	}

	fread(header, 1, 8, fp);
	if (png_sig_cmp(header, 0, 8)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File %s is not recognized as a PNG file", file_name);
		goto end;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "png_create_read_struct failed");
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

	/* Set the background color to draw transparent and alpha
	images over */
	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_bKGD)) {
		// png_get_bKGD(png_ptr, info_ptr, &my_background);
		// png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
	} else {
		png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
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

	if (color_type & PNG_COLOR_MASK_ALPHA) {
		if (setjmp(png_jmpbuf(png_ptr))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error!!!!\n");
			goto end;
		}

		png_set_strip_alpha(png_ptr);
	}

	png_read_update_info(png_ptr, info_ptr);

	if (width > SWITCH_IMG_MAX_WIDTH || height > SWITCH_IMG_MAX_HEIGHT) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PNG is too large! %dx%d\n", width, height);
	}

	row_bytes = png_get_rowbytes(png_ptr, info_ptr);
	//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "size: %dx%d row_bytes:%d color_type:%d bit_dept:%d\n", width, height, row_bytes, color_type, bit_depth);

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
	switch_assert(row_pointers);

	buffer = (png_byte *)malloc(row_bytes * height);
	switch_assert(buffer);

	for (y = 0; y< height; y++) {
		row_pointers[y] = buffer + row_bytes * y;
	}

	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
	}

	img = switch_img_alloc(NULL, SWITCH_IMG_FMT_I420, width, height, 1);
	switch_assert(img);

	/* read file */
	if (setjmp(png_jmpbuf(png_ptr))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error during read_image");
		goto end;
	}

	png_read_image(png_ptr, row_pointers);

	if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA) {
		// should never get here since we already use png_set_strip_alpha() ?
		switch_assert(1 == 2);

		switch_assert(row_bytes >= width * 4);

		for(y = 1; y < height; y++) {
			memcpy(buffer + y * width * 4, row_pointers[y], width * 4);
		}

		// ABGRToI420(buffer, width * 4,
		RGBAToI420(buffer, width * 4,
				img->planes[SWITCH_PLANE_Y], img->stride[SWITCH_PLANE_Y],
				img->planes[SWITCH_PLANE_U], img->stride[SWITCH_PLANE_U],
				img->planes[SWITCH_PLANE_V], img->stride[SWITCH_PLANE_V],
				width, height);
	} else if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {
		switch_assert(row_bytes >= width * 3);

		for(y = 1; y < height; y++) {
			memcpy(buffer + y * width * 3, row_pointers[y], width * 3);
		}
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

SWITCH_DECLARE(void) switch_img_write_png(switch_image_t *img, char* file_name)
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

end:

	switch_safe_free(buffer);
	switch_safe_free(row_pointers);
	fclose(fp);
	png_destroy_write_struct(&png_ptr, &info_ptr);
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

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
 *
 *
 * switch_core_video.c -- Core Video
 *
 */

#include <switch.h>


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

// simple implementation to patch a small img to a big IMG at position x,y
SWITCH_DECLARE(void) switch_img_patch(switch_image_t *IMG, switch_image_t *img, int x, int y)
{
	int i, j, k;
	int W = IMG->d_w;
	int H = IMG->d_h;
	int w = img->d_w;
	int h = img->d_h;

	switch_assert(img->fmt == SWITCH_IMG_FMT_I420);
	switch_assert(IMG->fmt == SWITCH_IMG_FMT_I420);

	for (i = y; i < (y + h) && i < H; i++) {
		for (j = x; j < (x + w) && j < W; j++) {
			IMG->planes[0][i * IMG->stride[0] + j] = img->planes[0][(i - y) * img->stride[0] + (j - x)];
		}
	}

	for (i = y; i < (y + h) && i < H; i+=4) {
		for (j = x; j < (x + w) && j < W; j+=4) {
			for (k = 1; k <= 2; k++) {
				IMG->planes[k][i/2 * IMG->stride[k] + j/2]         = img->planes[k][(i-y)/2 * img->stride[k] + (j-x)/2];
				IMG->planes[k][i/2 * IMG->stride[k] + j/2 + 1]     = img->planes[k][(i-y)/2 * img->stride[k] + (j-x)/2 + 1];
				IMG->planes[k][(i+2)/2 * IMG->stride[k] + j/2]     = img->planes[k][(i+2-y)/2 * img->stride[k] + (j-x)/2];
				IMG->planes[k][(i+2)/2 * IMG->stride[k] + j/2 + 1] = img->planes[k][(i+2-y)/2 * img->stride[k] + (j-x)/2 + 1];
			}
		}
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

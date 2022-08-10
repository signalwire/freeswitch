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
 * Andrey Volk <andrey@signalwire.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Andrey Volk <andrey@signalwire.com>
 *
 *
 * switch_image.c -- Image
 *
 */

#ifdef SWITCH_HAVE_VPX
#include "vpx/vpx_integer.h"
#include "vpx_mem/vpx_mem.h"
#endif

#include <switch.h>
#include <switch_image.h>

#ifdef SWITCH_HAVE_VPX
static switch_image_t *img_alloc_helper(switch_image_t *img, switch_img_fmt_t fmt,
	unsigned int d_w, unsigned int d_h,
	unsigned int buf_align,
	unsigned int stride_align,
	unsigned char *img_data)
{
	unsigned int h, w, s, xcs, ycs, bps;
	unsigned int stride_in_bytes;
	unsigned int align;

	if (img != NULL) memset(img, 0, sizeof(switch_image_t));

	/* Treat align==0 like align==1 */
	if (!buf_align) buf_align = 1;

	/* Validate alignment (must be power of 2) */
	if (buf_align & (buf_align - 1)) goto fail;

	/* Treat align==0 like align==1 */
	if (!stride_align) stride_align = 1;

	/* Validate alignment (must be power of 2) */
	if (stride_align & (stride_align - 1)) goto fail;

	/* Get sample size for this format */
	switch (fmt) {
	case SWITCH_IMG_FMT_ARGB:
	case SWITCH_IMG_FMT_ARGB_LE: bps = 32; break;
	case SWITCH_IMG_FMT_RGB24:
	case SWITCH_IMG_FMT_BGR24: bps = 24; break;
	case SWITCH_IMG_FMT_YUY2: bps = 16; break;
	case SWITCH_IMG_FMT_I420:
	case SWITCH_IMG_FMT_YV12:
	case SWITCH_IMG_FMT_NV12: bps = 12; break;
	case SWITCH_IMG_FMT_I422:
	case SWITCH_IMG_FMT_I440: bps = 16; break;
	case SWITCH_IMG_FMT_I444: bps = 24; break;
	case SWITCH_IMG_FMT_I42016: bps = 24; break;
	case SWITCH_IMG_FMT_I42216:
	case SWITCH_IMG_FMT_I44016: bps = 32; break;
	case SWITCH_IMG_FMT_I44416: bps = 48; break;
	default: bps = 16; break;
	}

	/* Get chroma shift values for this format */
	// For VPX_IMG_FMT_NV12, xcs needs to be 0 such that UV data is all read at
	// one time.
	switch (fmt) {
	case SWITCH_IMG_FMT_I420:
	case SWITCH_IMG_FMT_YV12:
	case SWITCH_IMG_FMT_I422:
	case SWITCH_IMG_FMT_I42016:
	case SWITCH_IMG_FMT_I42216: xcs = 1; break;
	default: xcs = 0; break;
	}

	switch (fmt) {
	case SWITCH_IMG_FMT_I420:
	case SWITCH_IMG_FMT_NV12:
	case SWITCH_IMG_FMT_I440:
	case SWITCH_IMG_FMT_YV12:
	case SWITCH_IMG_FMT_I42016:
	case SWITCH_IMG_FMT_I44016: ycs = 1; break;
	default: ycs = 0; break;
	}

	/* Calculate storage sizes. If the buffer was allocated externally, the width
	 * and height shouldn't be adjusted. */
	w = d_w;
	h = d_h;
	s = (fmt & SWITCH_IMG_FMT_PLANAR) ? w : bps * w / 8;
	s = (s + stride_align - 1) & ~(stride_align - 1);
	stride_in_bytes = (fmt & SWITCH_IMG_FMT_HIGHBITDEPTH) ? s * 2 : s;

	/* Allocate the new image */
	if (!img) {
		img = (switch_image_t *)calloc(1, sizeof(switch_image_t));

		if (!img) goto fail;

		img->self_allocd = 1;
	}

	img->img_data = img_data;

	if (!img_data) {
		uint64_t alloc_size;
		/* Calculate storage sizes given the chroma subsampling */
		align = (1 << xcs) - 1;
		w = (d_w + align) & ~align;
		align = (1 << ycs) - 1;
		h = (d_h + align) & ~align;

		s = (fmt & SWITCH_IMG_FMT_PLANAR) ? w : bps * w / 8;
		s = (s + stride_align - 1) & ~(stride_align - 1);
		stride_in_bytes = (fmt & SWITCH_IMG_FMT_HIGHBITDEPTH) ? s * 2 : s;
		alloc_size = (fmt & SWITCH_IMG_FMT_PLANAR) ? (uint64_t)h * s * bps / 8
			: (uint64_t)h * s;

		if (alloc_size != (size_t)alloc_size) goto fail;

		img->img_data = (uint8_t *)vpx_memalign(buf_align, (size_t)alloc_size);
		img->img_data_owner = 1;
	}

	if (!img->img_data) goto fail;

	img->fmt = fmt;
	img->bit_depth = (fmt & SWITCH_IMG_FMT_HIGHBITDEPTH) ? 16 : 8;
	img->w = w;
	img->h = h;
	img->x_chroma_shift = xcs;
	img->y_chroma_shift = ycs;
	img->bps = bps;

	/* Calculate strides */
	img->stride[SWITCH_PLANE_Y] = img->stride[SWITCH_PLANE_ALPHA] = stride_in_bytes;
	img->stride[SWITCH_PLANE_U] = img->stride[SWITCH_PLANE_V] = stride_in_bytes >> xcs;

	/* Default viewport to entire image */
	if (!switch_img_set_rect(img, 0, 0, d_w, d_h)) return img;

fail:
	switch_img_raw_free(img);
	return NULL;
}

switch_image_t *switch_img_raw_alloc(switch_image_t *img, switch_img_fmt_t fmt,
	unsigned int d_w, unsigned int d_h,
	unsigned int align)
{
	return img_alloc_helper(img, fmt, d_w, d_h, align, align, NULL);
}

switch_image_t *switch_img_raw_wrap(switch_image_t *img, switch_img_fmt_t fmt, unsigned int d_w,
	unsigned int d_h, unsigned int stride_align,
	unsigned char *img_data)
{
	/* By setting buf_align = 1, we don't change buffer alignment in this function. */
	return img_alloc_helper(img, fmt, d_w, d_h, 1, stride_align, img_data);
}
#endif /* SWITCH_HAVE_VPX */

SWITCH_DECLARE(int) switch_img_set_rect(switch_image_t *img,
	unsigned int  x,
	unsigned int  y,
	unsigned int  w,
	unsigned int  h)
{
#ifdef SWITCH_HAVE_VPX
	if (x <= UINT_MAX - w && x + w <= img->w && y <= UINT_MAX - h &&
		y + h <= img->h) {
		img->d_w = w;
		img->d_h = h;

			/* Calculate plane pointers */
			if (!(img->fmt & SWITCH_IMG_FMT_PLANAR)) {
				img->planes[SWITCH_PLANE_PACKED] =
					img->img_data + x * img->bps / 8 + y * img->stride[SWITCH_PLANE_PACKED];
			} else {
				const int bytes_per_sample =
					(img->fmt & SWITCH_IMG_FMT_HIGHBITDEPTH) ? 2 : 1;
				unsigned char *data = img->img_data;

				if (img->fmt & SWITCH_IMG_FMT_HAS_ALPHA) {
					img->planes[SWITCH_PLANE_ALPHA] =
						data + x * bytes_per_sample + y * img->stride[SWITCH_PLANE_ALPHA];
					data += img->h * img->stride[SWITCH_PLANE_ALPHA];
				}

				img->planes[SWITCH_PLANE_Y] =
					data + x * bytes_per_sample + y * img->stride[SWITCH_PLANE_Y];
				data += img->h * img->stride[SWITCH_PLANE_Y];

				if (img->fmt == SWITCH_IMG_FMT_NV12) {
					img->planes[SWITCH_PLANE_U] =
						data + (x >> img->x_chroma_shift) +
						(y >> img->y_chroma_shift) * img->stride[SWITCH_PLANE_U];
					img->planes[SWITCH_PLANE_V] = img->planes[SWITCH_PLANE_U] + 1;
				} else if (!(img->fmt & SWITCH_IMG_FMT_UV_FLIP)) {
					img->planes[SWITCH_PLANE_U] =
						data + (x >> img->x_chroma_shift) * bytes_per_sample +
						(y >> img->y_chroma_shift) * img->stride[SWITCH_PLANE_U];
					data += (img->h >> img->y_chroma_shift) * img->stride[SWITCH_PLANE_U];
					img->planes[SWITCH_PLANE_V] =
						data + (x >> img->x_chroma_shift) * bytes_per_sample +
						(y >> img->y_chroma_shift) * img->stride[SWITCH_PLANE_V];
				} else {
					img->planes[SWITCH_PLANE_V] =
						data + (x >> img->x_chroma_shift) * bytes_per_sample +
						(y >> img->y_chroma_shift) * img->stride[SWITCH_PLANE_V];
					data += (img->h >> img->y_chroma_shift) * img->stride[SWITCH_PLANE_V];
					img->planes[SWITCH_PLANE_U] =
						data + (x >> img->x_chroma_shift) * bytes_per_sample +
						(y >> img->y_chroma_shift) * img->stride[SWITCH_PLANE_U];
				}
			}
		return 0;
}
	return -1;
#else
	return 0;
#endif
}

void switch_img_raw_free(switch_image_t *img)
{
#ifdef SWITCH_HAVE_VPX
	if (img) {
		if (img->img_data && img->img_data_owner) vpx_free(img->img_data);

		if (img->self_allocd) free(img);
	}
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

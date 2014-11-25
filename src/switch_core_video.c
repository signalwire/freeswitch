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
	}

	*img = NULL;
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

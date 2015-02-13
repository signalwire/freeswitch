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
 * switch_core_video.h -- Core Video header
 *
 */
/*! \file switch_core_video.h
	\brief video includes header

	The things powered by libvpx are renamed into the switch_ namespace to provide a cleaner
	look to things and helps me to document what parts of video I am using I'd like to take this
	opportunity to thank libvpx for all the awesome stuff it does and for making my life much easier.

*/

#ifndef SWITCH_VIDEO_H
#define SWITCH_VIDEO_H

#include <switch.h>

SWITCH_BEGIN_EXTERN_C

typedef struct switch_yuv_color_s {
	uint8_t y;
	uint8_t u;
	uint8_t v;
} switch_yuv_color_t;

typedef struct switch_rgb_color_s {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} switch_rgb_color_t;

/**\brief Representation of a rectangle on a surface */
typedef struct switch_image_rect {
	unsigned int x; /**< leftmost column */
	unsigned int y; /**< topmost row */
	unsigned int w; /**< width */
	unsigned int h; /**< height */
} switch_image_rect_t;


/*!\brief Open a descriptor, allocating storage for the underlying image
*
* Returns a descriptor for storing an image of the given format. The
* storage for the descriptor is allocated on the heap.
*
* \param[in]    img       Pointer to storage for descriptor. If this parameter
*                         is NULL, the storage for the descriptor will be
*                         allocated on the heap.
* \param[in]    fmt       Format for the image
* \param[in]    d_w       Width of the image
* \param[in]    d_h       Height of the image
* \param[in]    align     Alignment, in bytes, of the image buffer and
*                         each row in the image(stride).
*
* \return Returns a pointer to the initialized image descriptor. If the img
*         parameter is non-null, the value of the img parameter will be
*         returned.
*/
SWITCH_DECLARE(switch_image_t *)switch_img_alloc(switch_image_t  *img,
						 switch_img_fmt_t fmt,
						 unsigned int d_w,
						 unsigned int d_h,
						 unsigned int align);

/*!\brief Open a descriptor, using existing storage for the underlying image
*
* Returns a descriptor for storing an image of the given format. The
* storage for descriptor has been allocated elsewhere, and a descriptor is
* desired to "wrap" that storage.
*
* \param[in]    img       Pointer to storage for descriptor. If this parameter
*                         is NULL, the storage for the descriptor will be
*                         allocated on the heap.
* \param[in]    fmt       Format for the image
* \param[in]    d_w       Width of the image
* \param[in]    d_h       Height of the image
* \param[in]    align     Alignment, in bytes, of each row in the image.
* \param[in]    img_data  Storage to use for the image
*
* \return Returns a pointer to the initialized image descriptor. If the img
*         parameter is non-null, the value of the img parameter will be
*         returned.
*/
SWITCH_DECLARE(switch_image_t *)switch_img_wrap(switch_image_t  *img,
						switch_img_fmt_t fmt,
						unsigned int d_w,
						unsigned int d_h,
						unsigned int align,
						unsigned char      *img_data);


/*!\brief Set the rectangle identifying the displayed portion of the image
*
* Updates the displayed rectangle (aka viewport) on the image surface to
* match the specified coordinates and size.
*
* \param[in]    img       Image descriptor
* \param[in]    x         leftmost column
* \param[in]    y         topmost row
* \param[in]    w         width
* \param[in]    h         height
*
* \return 0 if the requested rectangle is valid, nonzero otherwise.
*/
SWITCH_DECLARE(int) switch_img_set_rect(switch_image_t  *img,
				   unsigned int  x,
				   unsigned int  y,
				   unsigned int  w,
				   unsigned int  h);


SWITCH_DECLARE(void) switch_img_patch(switch_image_t *IMG, switch_image_t *img, int x, int y);

/*!\brief Copy image to a new image
*
* if new_img is NULL, a new image is allocated
* if new_img is not NULL but not the same size as img,
*    new_img is destroyed and a new new_img is allocated
* else, copy the img data to the new_img
*
* \param[in]    img       Image descriptor
*/

SWITCH_DECLARE(void) switch_img_copy(switch_image_t *img, switch_image_t **new_img);


/*!\brief Flip the image vertically (top for bottom)
*
* Adjusts the image descriptor's pointers and strides to make the image
* be referenced upside-down.
*
* \param[in]    img       Image descriptor
*/
SWITCH_DECLARE(void) switch_img_flip(switch_image_t *img);

/*!\brief Close an image descriptor
*
* Frees all allocated storage associated with an image descriptor.
*
* \param[in]    img       pointer to pointer of Image descriptor
*/
SWITCH_DECLARE(void) switch_img_free(switch_image_t **img);

SWITCH_DECLARE(void) switch_img_draw_text(switch_image_t *IMG, int x, int y, switch_rgb_color_t color, uint16_t font_size, char *text);

SWITCH_DECLARE(void) switch_img_add_text(void *buffer, int w, int x, int y, char *s);

SWITCH_DECLARE(switch_image_t *) switch_img_copy_rect(switch_image_t *img, int x, int y, int w, int h);

SWITCH_DECLARE(void) switch_img_fill(switch_image_t *img, int x, int y, int w, int h, switch_rgb_color_t *color);

SWITCH_DECLARE(void) switch_img_draw_pixel(switch_image_t *img, int x, int y, switch_yuv_color_t *color);

SWITCH_DECLARE(void) switch_color_set_rgb(switch_rgb_color_t *color, const char *color_str);
SWITCH_DECLARE(void) switch_color_set_yuv(switch_yuv_color_t *color, const char *color_str);
SWITCH_DECLARE(void) switch_color_rgb2yuv(switch_rgb_color_t *rgb, switch_yuv_color_t *yuv);
SWITCH_DECLARE(void) switch_color_yuv2rgb(switch_yuv_color_t *yuv, switch_rgb_color_t *rgb);

SWITCH_DECLARE(switch_status_t) switch_img_txt_handle_create(switch_img_txt_handle_t **handleP, const char *font_family,
															 const char *font_color, const char *bgcolor, uint16_t font_size, double angle, switch_memory_pool_t *pool);

SWITCH_DECLARE(void) switch_img_txt_handle_destroy(switch_img_txt_handle_t **handleP);

SWITCH_DECLARE(switch_status_t) switch_img_txt_handle_render(switch_img_txt_handle_t *handle, switch_image_t *img,
															 int x, int y, const char *text,
															 const char *font_family, const char *font_color, const char *bgcolor, uint16_t font_size, double angle);


SWITCH_DECLARE(void) switch_img_patch_hole(switch_image_t *IMG, switch_image_t *img, int x, int y, switch_image_rect_t *rect);
SWITCH_DECLARE(switch_image_t *) switch_img_read_png(const char* file_name);
SWITCH_DECLARE(void) switch_img_write_png(switch_image_t *img, char* file_name);

/** @} */

SWITCH_END_EXTERN_C
#endif
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

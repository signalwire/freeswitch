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

typedef enum {
	POS_LEFT_TOP = 0,
	POS_LEFT_MID,
	POS_LEFT_BOT,
	POS_CENTER_TOP,
	POS_CENTER_MID,
	POS_CENTER_BOT,
	POS_RIGHT_TOP,
	POS_RIGHT_MID,
	POS_RIGHT_BOT,
	POS_NONE
} switch_img_position_t;

typedef enum {
	SWITCH_FIT_SIZE,
	SWITCH_FIT_SCALE,
	SWITCH_FIT_SIZE_AND_SCALE,
	SWITCH_FIT_NONE
} switch_img_fit_t;

typedef struct switch_yuv_color_s {
	uint8_t y;
	uint8_t u;
	uint8_t v;
} switch_yuv_color_t;

typedef struct switch_rgb_color_s {
	uint8_t a;
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

typedef enum {
	SWITCH_CONVERT_FMT_YUYV = 0
} switch_convert_fmt_t;

struct switch_png_opaque_s;
typedef struct switch_png_opaque_s switch_png_opaque_t;
typedef struct switch_png_s {
	switch_png_opaque_t *pvt;
	int w;
	int h;
} switch_png_t;

typedef enum {
	SRM_NONE = 0,  // No rotation.
	SRM_90 = 90,  // Rotate 90 degrees clockwise.
	SRM_180 = 180,  // Rotate 180 degrees.
	SRM_270 = 270,  // Rotate 270 degrees clockwise.
} switch_image_rotation_mode_t;
	

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
						unsigned char *img_data);


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

/*!\brief patch a small img to a big IMG at position x,y
*
* Both IMG and img must be non-NULL
*
* \param[in]    IMG       The BIG Image descriptor
* \param[in]    img       The small Image descriptor
* \param[in]    x         Leftmost pos to patch to
* \param[in]    y         Topmost pos to patch to
*/
SWITCH_DECLARE(void) switch_img_patch(switch_image_t *IMG, switch_image_t *img, int x, int y);


/*!\brief patch part of a small img (x,y,w,h) to a big IMG at position X,Y
*
* Both IMG and img must be non-NULL
*
* \param[in]    IMG       The BIG Image descriptor
* \param[in]    X         Leftmost pos to patch to IMG
* \param[in]    Y         Topmost pos to patch to IMG
* \param[in]    img       The small Image descriptor
* \param[in]    x         Leftmost pos to be read from img
* \param[in]    y         Topmost pos to be read from
* \param[in]    w         Max width to be read from img
* \param[in]    h         Max height to be read from img
*/

SWITCH_DECLARE(void) switch_img_patch_rect(switch_image_t *IMG, int X, int Y, switch_image_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/*!\brief Copy image to a new image
*
* if new_img is NULL, a new image is allocated
* if new_img is not NULL but not the same size as img,
*    new_img is destroyed and a new new_img is allocated
* else, copy the img data to the new_img
*
* \param[in]    img       Image descriptor
* \param[out]   new_img   New Image descriptor, NULL if out of memory
*/

SWITCH_DECLARE(void) switch_img_copy(switch_image_t *img, switch_image_t **new_img);
SWITCH_DECLARE(void) switch_img_rotate_copy(switch_image_t *img, switch_image_t **new_img, switch_image_rotation_mode_t mode);

/*!\brief Flip the image vertically (top for bottom)
*
* Adjusts the image descriptor's pointers and strides to make the image
* be referenced upside-down.
*
* \param[in]    img       Image descriptor
*
* \return 0 if the requested rectangle is valid, nonzero otherwise.
*/
SWITCH_DECLARE(void) switch_img_rotate(switch_image_t **img, switch_image_rotation_mode_t mode);

/*!\brief Close an image descriptor
*
* Frees all allocated storage associated with an image descriptor.
*
* \param[in]    img       pointer to pointer of Image descriptor
*/
SWITCH_DECLARE(void) switch_img_free(switch_image_t **img);

SWITCH_DECLARE(void) switch_img_draw_text(switch_image_t *IMG, int x, int y, switch_rgb_color_t color, uint16_t font_size, char *text);

SWITCH_DECLARE(void) switch_img_add_text(void *buffer, int w, int x, int y, char *s);

/*!\brief Copy part of an image to a new image
*
*
* \param[in]    img       Image descriptor
* \param[in]    x         Leftmost pos to be read from
* \param[in]    y         Topmost pos to be read from
* \param[in]    w         Max width to be read from
* \param[in]    h         Max height to be read from
*
* \return NULL if failed to copy, otherwise a valid image descriptor.
*/
SWITCH_DECLARE(switch_image_t *) switch_img_copy_rect(switch_image_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/*!\brief Fill image with color
*
* \param[in]    img       Image descriptor
* \param[in]    x         Leftmost pos to be read from
* \param[in]    y         Topmost pos to be read from
* \param[in]    w         Max width to be read from
* \param[in]    h         Max height to be read from
* \param[in]    color     RGB color
*/
SWITCH_DECLARE(void) switch_img_fill(switch_image_t *img, int x, int y, int w, int h, switch_rgb_color_t *color);

/*!\brief Set RGB color with a string
*
* Color string should be in #RRGGBB format
*
* \param[out]   color     RGB color pointer
* \param[in]    color_str Color string in #RRGGBB format
*/
SWITCH_DECLARE(void) switch_color_set_rgb(switch_rgb_color_t *color, const char *color_str);

/*!\brief Set YUV color with a string
*
* Color string should be in #RRGGBB format
*
* \param[out]   color     YUV color pointer
* \param[in]    color_str Color string in #RRGGBB format
*/
SWITCH_DECLARE(void) switch_color_set_yuv(switch_yuv_color_t *color, const char *color_str);

/*!\brief Created a text handle
*
* \param[out]   handleP     Pointer to the text handle pointer
* \param[in]    font_family Font family
* \param[in]    font_color  Font color in #RRGGBB format
* \param[in]    bgcolor     Background color in #RRGGBB format
* \param[in]    font_size   Font size in point
* \param[in]    angle       Angle to rotate
* \param[in]    pool        APR memory pool
*/
SWITCH_DECLARE(switch_status_t) switch_img_txt_handle_create(switch_img_txt_handle_t **handleP, const char *font_family,
															 const char *font_color, const char *bgcolor, uint16_t font_size, double angle, switch_memory_pool_t *pool);

/*!\brief Free a text handle
*
* \param[in]   handleP     Pointer to the text handle pointer
*/
SWITCH_DECLARE(void) switch_img_txt_handle_destroy(switch_img_txt_handle_t **handleP);

/*!\brief Render text to an img
*
* \param[in]    handle      Pointer to the text handle pointer
* \param[in]    img         The image to be render text on
* \param[in]    x           Leftmost position
* \param[in]    y           Topmost position
* \param[in]    text        Text to render
* \param[in]    font_family Font to use, NULL to use the handle font
* \param[in]    font_color  Font color, NULL to use the handle color
* \param[in]    bgcolor     Background color, NULL for transparency
* \param[in]    font_size   Font size in point
* \param[in]    angle       Angle to rotate
*/

SWITCH_DECLARE(uint32_t) switch_img_txt_handle_render(switch_img_txt_handle_t *handle, switch_image_t *img,
													  int x, int y, const char *text,
													  const char *font_family, const char *font_color, const char *bgcolor, uint16_t font_size, double angle);
						 

SWITCH_DECLARE(void) switch_img_patch_hole(switch_image_t *IMG, switch_image_t *img, int x, int y, switch_image_rect_t *rect);

SWITCH_DECLARE(switch_status_t) switch_png_patch_img(switch_png_t *use_png, switch_image_t *img, int x, int y);
SWITCH_DECLARE(switch_image_t *) switch_img_read_png(const char *file_name, switch_img_fmt_t img_fmt);
SWITCH_DECLARE(switch_status_t) switch_img_write_png(switch_image_t *img, char *file_name);
SWITCH_DECLARE(switch_status_t) switch_png_open(switch_png_t **pngP, const char *file_name);
SWITCH_DECLARE(void) switch_png_free(switch_png_t **pngP);

/*!\brief put a small img over a big IMG at position x,y, with alpha transparency
*
* Both IMG and img must be non-NULL
*
* \param[in]    IMG       The BIG Image descriptor
* \param[in]    img       The small Image descriptor
* \param[in]    x         Leftmost pos
* \param[in]    y         Topmost pos
* \param[in]    percent   Alaha value from 0(completely transparent) to 100(opaque)
*/
SWITCH_DECLARE(void) switch_img_overlay(switch_image_t *IMG, switch_image_t *img, int x, int y, uint8_t percent);

SWITCH_DECLARE(switch_status_t) switch_img_scale(switch_image_t *src, switch_image_t **destP, int width, int height);
SWITCH_DECLARE(switch_status_t) switch_img_fit(switch_image_t **srcP, int width, int height, switch_img_fit_t fit);
SWITCH_DECLARE(switch_img_position_t) parse_img_position(const char *name);
SWITCH_DECLARE(switch_img_fit_t) parse_img_fit(const char *name);
SWITCH_DECLARE(void) switch_img_find_position(switch_img_position_t pos, int sw, int sh, int iw, int ih, int *xP, int *yP);

/*!\brief convert img to raw format
*
* dest should be pre-allocated and big enough for the target fmt
*
* \param[in]    src       The image descriptor
* \param[in]    dest      The target memory address
* \param[in]    size      The size of target memory address used for bounds check
* \param[in]    fmt       The target format
*/
SWITCH_DECLARE(switch_status_t) switch_img_to_raw(switch_image_t *src, void *dest, switch_size_t size, switch_img_fmt_t fmt);
/*!\brief convert raw memory to switch_img_t
*
* if dest is NULL then a new img is created, user should destroy it later,
* otherwize it will re-used the dest img, and the dest img size must match the src width and height,
* width and height can be 0 in the latter case and it will figure out according to the dest img
*
* \param[in]    dest      The image descriptor
* \param[in]    src       The raw data memory address
* \param[in]    fmt       The raw data format
* \param[in]    width     The raw data width
* \param[in]    height    The raw data height
*/
SWITCH_DECLARE(switch_status_t) switch_img_from_raw(switch_image_t *dest, void *src, switch_img_fmt_t fmt, int width, int height);
SWITCH_DECLARE(switch_image_t *) switch_img_write_text_img(int w, int h, switch_bool_t full, const char *text);

SWITCH_DECLARE(switch_image_t *) switch_img_read_file(const char* file_name);
SWITCH_DECLARE(switch_status_t) switch_img_letterbox(switch_image_t *img, switch_image_t **imgP, int width, int height, const char *color);
SWITCH_DECLARE(switch_bool_t) switch_core_has_video(void);

/*!\brief I420 to I420 Copy*/

SWITCH_DECLARE(switch_status_t) switch_I420_copy(const uint8_t* src_y, int src_stride_y,
												 const uint8_t* src_u, int src_stride_u,
												 const uint8_t* src_v, int src_stride_v,
												 uint8_t* dst_y, int dst_stride_y,
												 uint8_t* dst_u, int dst_stride_u,
												 uint8_t* dst_v, int dst_stride_v,
												 int width, int height);
SWITCH_DECLARE(switch_status_t) switch_I420_copy2(uint8_t *src_planes[], int src_stride[],
												  uint8_t *dst_planes[], int dst_stride[],
												  int width, int height);
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

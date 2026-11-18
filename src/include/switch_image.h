/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*!\file
 * \brief Describes the vpx image descriptor and associated operations
 *
 */
#ifndef SWITCH_IMAGE_H
#define SWITCH_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/*!\brief Current ABI version number
 *
 * \internal
 * If this file is altered in any way that changes the ABI, this value
 * must be bumped.  Examples include, but are not limited to, changing
 * types, removing or reassigning enums, adding/removing/rearranging
 * fields to structures
 */
#define SWITCH_IMAGE_ABI_VERSION (5) /**<\hideinitializer*/

#define SWITCH_IMG_FMT_PLANAR 0x100       /**< Image is a planar format. */
#define SWITCH_IMG_FMT_UV_FLIP 0x200      /**< V plane precedes U in memory. */
#define SWITCH_IMG_FMT_HAS_ALPHA 0x400    /**< Image has an alpha channel. */
#define SWITCH_IMG_FMT_HIGHBITDEPTH 0x800 /**< Image uses 16bit framebuffer. */

/*!\brief List of supported image formats */
typedef enum switch_img_fmt {
  SWITCH_IMG_FMT_NONE,
  SWITCH_IMG_FMT_GD = SWITCH_IMG_FMT_NONE, /* experimental */
  SWITCH_IMG_FMT_RGB24,     /**< 24 bit per pixel packed RGB */
  SWITCH_IMG_FMT_YUY2,      /**< YUYV packed YUV */
  SWITCH_IMG_FMT_BGR24,     /**< 24 bit per pixel packed BGR */
  SWITCH_IMG_FMT_ARGB,      /**< 32 bit packed ARGB, alpha=255 */
  SWITCH_IMG_FMT_ARGB_LE,   /**< 32 bit packed BGRA, alpha=255 */
  SWITCH_IMG_FMT_YV12 =
      SWITCH_IMG_FMT_PLANAR | SWITCH_IMG_FMT_UV_FLIP | 1, /**< planar YVU */
  SWITCH_IMG_FMT_I420 = SWITCH_IMG_FMT_PLANAR | 2,
  SWITCH_IMG_FMT_I422 = SWITCH_IMG_FMT_PLANAR | 5,
  SWITCH_IMG_FMT_I444 = SWITCH_IMG_FMT_PLANAR | 6,
  SWITCH_IMG_FMT_I440 = SWITCH_IMG_FMT_PLANAR | 7,
  SWITCH_IMG_FMT_NV12 = SWITCH_IMG_FMT_PLANAR | 9,
  SWITCH_IMG_FMT_444A = SWITCH_IMG_FMT_PLANAR | SWITCH_IMG_FMT_HAS_ALPHA | 6,
  SWITCH_IMG_FMT_I42016 = SWITCH_IMG_FMT_I420 | SWITCH_IMG_FMT_HIGHBITDEPTH,
  SWITCH_IMG_FMT_I42216 = SWITCH_IMG_FMT_I422 | SWITCH_IMG_FMT_HIGHBITDEPTH,
  SWITCH_IMG_FMT_I44416 = SWITCH_IMG_FMT_I444 | SWITCH_IMG_FMT_HIGHBITDEPTH,
  SWITCH_IMG_FMT_I44016 = SWITCH_IMG_FMT_I440 | SWITCH_IMG_FMT_HIGHBITDEPTH
} switch_img_fmt_t; /**< alias for enum vpx_img_fmt */

/*!\brief List of supported color spaces */
typedef enum switch_color_space {
  SWITCH_CS_UNKNOWN = 0,   /**< Unknown */
  SWITCH_CS_BT_601 = 1,    /**< BT.601 */
  SWITCH_CS_BT_709 = 2,    /**< BT.709 */
  SWITCH_CS_SMPTE_170 = 3, /**< SMPTE.170 */
  SWITCH_CS_SMPTE_240 = 4, /**< SMPTE.240 */
  SWITCH_CS_BT_2020 = 5,   /**< BT.2020 */
  SWITCH_CS_RESERVED = 6,  /**< Reserved */
  SWITCH_CS_SRGB = 7       /**< sRGB */
} switch_color_space_t;    /**< alias for enum vpx_color_space */

/*!\brief List of supported color range */
typedef enum switch_color_range {
  SWITCH_CR_STUDIO_RANGE = 0, /**< Y [16..235], UV [16..240] */
  SWITCH_CR_FULL_RANGE = 1    /**< YUV/RGB [0..255] */
} switch_color_range_t;       /**< alias for enum vpx_color_range */

/**\brief Image Descriptor */
typedef struct switch_image {
  switch_img_fmt_t fmt;       /**< Image Format */
  switch_color_space_t cs;    /**< Color Space */
  switch_color_range_t range; /**< Color Range */

  /* Image storage dimensions */
  unsigned int w;         /**< Stored image width */
  unsigned int h;         /**< Stored image height */
  unsigned int bit_depth; /**< Stored image bit-depth */

  /* Image display dimensions */
  unsigned int d_w; /**< Displayed image width */
  unsigned int d_h; /**< Displayed image height */

  /* Image intended rendering dimensions */
  unsigned int r_w; /**< Intended rendering image width */
  unsigned int r_h; /**< Intended rendering image height */

  /* Chroma subsampling info */
  unsigned int x_chroma_shift; /**< subsampling order, X */
  unsigned int y_chroma_shift; /**< subsampling order, Y */

/* Image data pointers. */
#define SWITCH_PLANE_PACKED 0  /**< To be used for all packed formats */
#define SWITCH_PLANE_Y 0       /**< Y (Luminance) plane */
#define SWITCH_PLANE_U 1       /**< U (Chroma) plane */
#define SWITCH_PLANE_V 2       /**< V (Chroma) plane */
#define SWITCH_PLANE_ALPHA 3   /**< A (Transparency) plane */
  unsigned char *planes[4]; /**< pointer to the top left pixel for each plane */
  int stride[4];            /**< stride between rows for each plane */

  int bps; /**< bits per sample (for packed formats) */

  /*!\brief The following member may be set by the application to associate
   * data with this image.
   */
  void *user_priv;

  /* The following members should be treated as private. */
  unsigned char *img_data; /**< private */
  int img_data_owner;      /**< private */
  int self_allocd;         /**< private */

  void *fb_priv; /**< Frame buffer data associated with the image. */
} switch_image_t;   /**< alias for struct vpx_image */

/**\brief Representation of a rectangle on a surface */
typedef struct switch_image_rect {
  unsigned int x;   /**< leftmost column */
  unsigned int y;   /**< topmost row */
  unsigned int w;   /**< width */
  unsigned int h;   /**< height */
} switch_image_rect_t; /**< alias for struct vpx_image_rect */

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
switch_image_t *switch_img_raw_alloc(switch_image_t *img, switch_img_fmt_t fmt,
                           unsigned int d_w, unsigned int d_h,
                           unsigned int align);

/*!\brief Open a descriptor, using existing storage for the underlying image
 *
 * Returns a descriptor for storing an image of the given format. The
 * storage for descriptor has been allocated elsewhere, and a descriptor is
 * desired to "wrap" that storage.
 *
 * \param[in]    img           Pointer to storage for descriptor. If this
 *                             parameter is NULL, the storage for the descriptor
 *                             will be allocated on the heap.
 * \param[in]    fmt           Format for the image
 * \param[in]    d_w           Width of the image
 * \param[in]    d_h           Height of the image
 * \param[in]    stride_align  Alignment, in bytes, of each row in the image.
 * \param[in]    img_data      Storage to use for the image
 *
 * \return Returns a pointer to the initialized image descriptor. If the img
 *         parameter is non-null, the value of the img parameter will be
 *         returned.
 */
switch_image_t *switch_img_raw_wrap(switch_image_t *img,switch_img_fmt_t fmt, unsigned int d_w,
                          unsigned int d_h, unsigned int stride_align,
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
SWITCH_DECLARE(int) switch_img_set_rect(switch_image_t *img, unsigned int x, unsigned int y,
                     unsigned int w, unsigned int h);

/*!\brief Close an image descriptor
 *
 * Frees all allocated storage associated with an image descriptor.
 *
 * \param[in]    img       Image descriptor
 */
void switch_img_raw_free(switch_image_t *img);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SWITCH_IMAGE_H

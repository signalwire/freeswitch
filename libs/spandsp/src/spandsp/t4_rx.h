/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_rx.h - definitions for T.4 FAX receive processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*! \file */

#if !defined(_SPANDSP_T4_RX_H_)
#define _SPANDSP_T4_RX_H_

/*! \page t4_page T.4 image compression and decompression

\section t4_page_sec_1 What does it do?
The T.4 image compression and decompression routines implement the 1D and 2D
encoding methods defined in ITU specification T.4. They also implement the pure
2D encoding method defined in T.6. These are image compression algorithms used
for FAX transmission.

\section t4_page_sec_1 How does it work?
*/

/*! This function is a callback from the image decoders, to write the decoded bi-level image,
    row by row. It is called for each row, with len set to the number of bytes per row. At the
    end of the image it is called with len set to zero, to indicate the end of image condition.
    \return 0 for OK, or non-zero for a problem that requires the image be interrupted. */
typedef int (*t4_row_write_handler_t)(void *user_data, const uint8_t buf[], size_t len);

/*! Supported compression modes. */
typedef enum
{
    /*! No compression */
    T4_COMPRESSION_NONE = 0x01,
    /*! T.4 1D compression */
    T4_COMPRESSION_T4_1D = 0x02,
    /*! T.4 2D compression */
    T4_COMPRESSION_T4_2D = 0x04,
    /*! T.6 2D compression */
    T4_COMPRESSION_T6 = 0x08,
    /*! T.85 monochrome JBIG coding with L0 fixed. */
    T4_COMPRESSION_T85 = 0x10,
    /*! T.85 monochrome JBIG coding with L0 variable. */
    T4_COMPRESSION_T85_L0 = 0x20,
    /*! T.43 gray-scale/colour JBIG coding */
    T4_COMPRESSION_T43 = 0x40,
    /*! T.45 run length colour compression */
    T4_COMPRESSION_T45 = 0x80,
    /*! T.42 + T.81 + T.30 Annex E colour JPEG coding */
    T4_COMPRESSION_T42_T81 = 0x100,
    /*! T.42 + T.81 + T.30 Annex K colour sYCC-JPEG coding */
    T4_COMPRESSION_SYCC_T81 = 0x200,
    /*! T.88 monochrome JBIG2 compression */
    T4_COMPRESSION_T88 = 0x400,
    /*! Support solour compression without sub-sampling */
    T4_COMPRESSION_NO_SUBSAMPLING = 0x800000,
    /*! Gray-scale support by multi-level codecs */
    T4_COMPRESSION_GRAYSCALE = 0x1000000,
    /*! Colour support by multi-level codecs */
    T4_COMPRESSION_COLOUR = 0x2000000,
    /*! 12 bit mode for gray-scale and colour */
    T4_COMPRESSION_12BIT = 0x4000000,
    /*! Convert a colour image to a gray-scale one */
    T4_COMPRESSION_COLOUR_TO_GRAY = 0x8000000,
    /*! Dither a gray-scale image down a simple bilevel image, with rescaling to fit a FAX page */
    T4_COMPRESSION_GRAY_TO_BILEVEL = 0x10000000,
    /*! Dither a colour image down a simple bilevel image, with rescaling to fit a FAX page */
    T4_COMPRESSION_COLOUR_TO_BILEVEL = 0x20000000,
    /*! Rescale an image (except a bi-level image) to fit a permitted FAX width when necessary */
    T4_COMPRESSION_RESCALING = 0x40000000
} t4_image_compression_t;

/*! Image type */
typedef enum
{
    /* Traditional black and white FAX */
    T4_IMAGE_TYPE_BILEVEL = 0,
    /* RGB or CMY image */
    T4_IMAGE_TYPE_COLOUR_BILEVEL = 1,
    /* CMYK image */
    T4_IMAGE_TYPE_4COLOUR_BILEVEL = 2,
    /* 2 to 8 bits per pixel gray-scale image */
    T4_IMAGE_TYPE_GRAY_8BIT = 3,
    /* 9 to 12 bits per pixel gray-scale image */
    T4_IMAGE_TYPE_GRAY_12BIT = 4,
    /* 2 to 8 bits per pixel RGB or CMY colour image */
    T4_IMAGE_TYPE_COLOUR_8BIT = 5,
    /* 2 to 8 bits per pixel CMYK colour image */
    T4_IMAGE_TYPE_4COLOUR_8BIT = 6,
    /* 9 to 12 bits per pixel RGB or CMY colour image */
    T4_IMAGE_TYPE_COLOUR_12BIT = 7,
    /* 9 to 12 bits per pixel CMYK colour image */
    T4_IMAGE_TYPE_4COLOUR_12BIT = 8
} t4_image_types_t;

/*! Supported X resolutions, in pixels per metre. */
typedef enum
{
    T4_X_RESOLUTION_100 = 3937,
    T4_X_RESOLUTION_R4 = 4020,
    T4_X_RESOLUTION_200 = 7874,
    T4_X_RESOLUTION_R8 = 8040,
    T4_X_RESOLUTION_300 = 11811,
    T4_X_RESOLUTION_400 = 15748,
    T4_X_RESOLUTION_R16 = 16080,
    T4_X_RESOLUTION_600 = 23622,
    T4_X_RESOLUTION_1200 = 47244
} t4_image_x_resolution_t;

/*! Supported Y resolutions, in pixels per metre. */
typedef enum
{
    T4_Y_RESOLUTION_STANDARD = 3850,
    T4_Y_RESOLUTION_100 = 3937,
    T4_Y_RESOLUTION_FINE = 7700,
    T4_Y_RESOLUTION_200 = 7874,
    T4_Y_RESOLUTION_300 = 11811,
    T4_Y_RESOLUTION_SUPERFINE = 15400,
    T4_Y_RESOLUTION_400 = 15748,
    T4_Y_RESOLUTION_600 = 23622,
    T4_Y_RESOLUTION_800 = 31496,
    T4_Y_RESOLUTION_1200 = 47244
} t4_image_y_resolution_t;

/* Only the symmetric resolutions are valid for gray-scale and colour use. The asymmetric
   ones are bi-level only. */
enum
{
    /*! Standard FAX resolution 204dpi x 98dpi - bi-level only */
    T4_RESOLUTION_R8_STANDARD = 0x1,
    /*! Fine FAX resolution 204dpi x 196dpi - bi-level only */
    T4_RESOLUTION_R8_FINE = 0x2,
    /*! Super-fine FAX resolution 204dpi x 391dpi - bi-level only */
    T4_RESOLUTION_R8_SUPERFINE = 0x4,
    /*! Double FAX resolution 408dpi x 391dpi - bi-level only */
    T4_RESOLUTION_R16_SUPERFINE = 0x8,

    /*! 100dpi x 100dpi - gray-scale and colour only */
    T4_RESOLUTION_100_100 = 0x10,
    /*! 200dpi x 100dpi - bi-level only */
    T4_RESOLUTION_200_100 = 0x20,
    /*! 200dpi x 200dpi */
    T4_RESOLUTION_200_200 = 0x40,
    /*! 200dpi x 400dpi - bi-level only */
    T4_RESOLUTION_200_400 = 0x80,
    /*! 300dpi x 300dpi */
    T4_RESOLUTION_300_300 = 0x100,
    /*! 300dpi x 600dpi - bi-level only */
    T4_RESOLUTION_300_600 = 0x200,
    /*! 400dpi x 400dpi */
    T4_RESOLUTION_400_400 = 0x400,
    /*! 400dpi x 800dpi - bi-level only */
    T4_RESOLUTION_400_800 = 0x800,
    /*! 600dpi x 600dpi */
    T4_RESOLUTION_600_600 = 0x1000,
    /*! 600dpi x 1200dpi - bi-level only */
    T4_RESOLUTION_600_1200 = 0x2000,
    /*! 1200dpi x 1200dpi */
    T4_RESOLUTION_1200_1200 = 0x4000
};

/*!
    Exact widths in PELs for the difference resolutions, and page widths.
    Note:
        The A4 widths also apply to North American letter and legal.
        The R4 resolution widths are not supported in recent versions of T.30
        Only images of exactly these widths are acceptable for FAX transmisson.

     R4     864 pels/215mm    for ISO A4, North American Letter and Legal
     R4    1024 pels/255mm    for ISO B4
     R4    1216 pels/303mm    for ISO A3
     R8    1728 pels/215mm    for ISO A4, North American Letter and Legal
     R8    2048 pels/255mm    for ISO B4
     R8    2432 pels/303mm    for ISO A3
     R16   3456 pels/215mm    for ISO A4, North American Letter and Legal
     R16   4096 pels/255mm    for ISO B4
     R16   4864 pels/303mm    for ISO A3

     100    864 pels/219.46mm for ISO A4, North American Letter and Legal
     100   1024 pels/260.10mm for ISO B4
     100   1216 pels/308.86mm for ISO A3
     200   1728 pels/219.46mm for ISO A4, North American Letter and Legal
     200   2048 pels/260.10mm for ISO B4
     200   2432 pels/308.86mm for ISO A3
     300   2592 pels/219.46mm for ISO A4, North American Letter and Legal
     300   3072 pels/260.10mm for ISO B4
     300   3648 pels/308.86mm for ISO A3
     400   3456 pels/219.46mm for ISO A4, North American Letter and Legal
     400   4096 pels/260.10mm for ISO B4
     400   4864 pels/308.86mm for ISO A3
     600   5184 pels/219.46mm for ISO A4, North American Letter and Legal
     600   6144 pels/260.10mm for ISO B4
     600   7296 pels/308.86mm for ISO A3
    1200  10368 pels/219.46mm for ISO A4, North American Letter and Legal
    1200  12288 pels/260.10mm for ISO B4
    1200  14592 pels/308.86mm for ISO A3
    
    Note that R4, R8 and R16 widths are 5mm wider than the actual paper sizes.
    The 100, 200, 300, 400, 600, and 1200 widths are 9.46mm, 10.1mm and 11.86mm
    wider than the paper sizes.
*/
typedef enum
{
    T4_WIDTH_100_A4 = 864,
    T4_WIDTH_100_B4 = 1024,
    T4_WIDTH_100_A3 = 1216,
    T4_WIDTH_200_A4 = 1728,
    T4_WIDTH_200_B4 = 2048,
    T4_WIDTH_200_A3 = 2432,
    T4_WIDTH_300_A4 = 2592,
    T4_WIDTH_300_B4 = 3072,
    T4_WIDTH_300_A3 = 3648,
    T4_WIDTH_400_A4 = 3456,
    T4_WIDTH_400_B4 = 4096,
    T4_WIDTH_400_A3 = 4864,
    T4_WIDTH_600_A4 = 5184,
    T4_WIDTH_600_B4 = 6144,
    T4_WIDTH_600_A3 = 7296,
    T4_WIDTH_1200_A4 = 10368,
    T4_WIDTH_1200_B4 = 12288,
    T4_WIDTH_1200_A3 = 14592
} t4_image_width_t;

#define T4_WIDTH_R4_A4 T4_WIDTH_100_A4
#define T4_WIDTH_R4_B4 T4_WIDTH_100_B4
#define T4_WIDTH_R4_A3 T4_WIDTH_100_A3

#define T4_WIDTH_R8_A4 T4_WIDTH_200_A4
#define T4_WIDTH_R8_B4 T4_WIDTH_200_B4
#define T4_WIDTH_R8_A3 T4_WIDTH_200_A3

#define T4_WIDTH_R16_A4 T4_WIDTH_400_A4
#define T4_WIDTH_R16_B4 T4_WIDTH_400_B4
#define T4_WIDTH_R16_A3 T4_WIDTH_400_A3

/*!
    Length of the various supported paper sizes, in pixels at the various Y resolutions.
    Paper sizes are
        A4 (210mm x 297mm)
        B4 (250mm x 353mm)
        A3 (297mm x 420mm)
        North American Letter (215.9mm x 279.4mm or 8.5"x11")
        North American Legal (215.9mm x 355.6mm or 8.4"x14")
        Unlimited

    T.4 does not accurately define the maximum number of scan lines in a page. A wide
    variety of maximum row counts are used in the real world. It is important not to
    set our sending limit too high, or a receiving machine might split pages. It is
    important not to set it too low, or we might clip pages.

    Values seen for standard resolution A4 pages include 1037, 1045, 1109, 1126 and 1143.
    1109 seems the most-popular.  At fine res 2150, 2196, 2200, 2237, 2252-2262, 2264,
    2286, and 2394 are used. 2255 seems the most popular. We try to use balanced choices
    here. 1143 pixels at 3.85/mm is 296.9mm, and an A4 page is 297mm long.
*/
typedef enum
{
    /* A4 is 297mm long */
    T4_LENGTH_STANDARD_A4 = 1143,
    T4_LENGTH_FINE_A4 = 2286,
    T4_LENGTH_300_A4 = 4665,
    T4_LENGTH_SUPERFINE_A4 = 4573,
    T4_LENGTH_600_A4 = 6998,
    T4_LENGTH_800_A4 = 9330,
    T4_LENGTH_1200_A4 = 13996,
    /* B4 is 353mm long */
    T4_LENGTH_STANDARD_B4 = 1359,
    T4_LENGTH_FINE_B4 = 2718,
    T4_LENGTH_300_B4 = 4169,
    T4_LENGTH_SUPERFINE_B4 = 5436,
    T4_LENGTH_600_B4 = 8338,
    T4_LENGTH_800_B4 = 11118,
    T4_LENGTH_1200_B4 = 16677,
    /* A3 is 420mm long */
    T4_LENGTH_STANDARD_A3 = 1617,
    T4_LENGTH_FINE_A3 = 3234,
    T4_LENGTH_300_A3 = 4960,
    T4_LENGTH_SUPERFINE_A3 = 6468,
    T4_LENGTH_600_A3 = 9921,
    T4_LENGTH_800_A3 = 13228,
    T4_LENGTH_1200_A3 = 19842,
    /* North American letter is 279.4mm long */
    T4_LENGTH_STANDARD_US_LETTER = 1075,
    T4_LENGTH_FINE_US_LETTER = 2151,
    T4_LENGTH_300_US_LETTER = 3300,
    T4_LENGTH_SUPERFINE_US_LETTER = 4302,
    T4_LENGTH_600_US_LETTER = 6700,
    T4_LENGTH_800_US_LETTER = 8800,
    T4_LENGTH_1200_US_LETTER = 13200,
    /* North American legal is 355.6mm long */
    T4_LENGTH_STANDARD_US_LEGAL = 1369,
    T4_LENGTH_FINE_US_LEGAL = 2738,
    T4_LENGTH_300_US_LEGAL = 4200,
    T4_LENGTH_SUPERFINE_US_LEGAL = 5476,
    T4_LENGTH_600_US_LEGAL = 8400,
    T4_LENGTH_800_US_LEGAL = 11200,
    T4_LENGTH_1200_US_LEGAL = 16800
} t4_image_length_t;

enum
{
    T4_SUPPORT_WIDTH_215MM = 0x01,
    T4_SUPPORT_WIDTH_255MM = 0x02,
    T4_SUPPORT_WIDTH_303MM = 0x04,

    T4_SUPPORT_LENGTH_UNLIMITED = 0x10000,
    T4_SUPPORT_LENGTH_A4 = 0x20000,
    T4_SUPPORT_LENGTH_B4 = 0x40000,
    T4_SUPPORT_LENGTH_US_LETTER = 0x80000,
    T4_SUPPORT_LENGTH_US_LEGAL = 0x100000
};

/*! Return values from the T.85 decoder */
typedef enum
{
    /*! More image data is needed */
    T4_DECODE_MORE_DATA = 0,
    /*! Image completed successfully */
    T4_DECODE_OK = -1,
    /*! The decoder has interrupted */
    T4_DECODE_INTERRUPT = -2,
    /*! An abort was found in the image data */
    T4_DECODE_ABORTED = -3,
    /*! A memory allocation error occurred */
    T4_DECODE_NOMEM = -4,
    /*! The image data is invalid. */
    T4_DECODE_INVALID_DATA = -5
} t4_decoder_status_t;

/*!
    T.4 FAX compression/decompression descriptor. This defines the working state
    for a single instance of a T.4 FAX compression or decompression channel.
*/
typedef struct t4_rx_state_s t4_rx_state_t;

/*!
    T.4 FAX compression/decompression statistics.
*/
typedef struct
{
    /*! \brief The number of pages transferred so far. */
    int pages_transferred;
    /*! \brief The number of pages in the file (<0 if unknown). */
    int pages_in_file;
    /*! \brief The number of bad pixel rows in the most recent page. */
    int bad_rows;
    /*! \brief The largest number of bad pixel rows in a block in the most recent page. */
    int longest_bad_row_run;
    /*! \brief The type of image in the file page */
    int image_type;
    /*! \brief The horizontal resolution of the file page in pixels per metre */
    int image_x_resolution;
    /*! \brief The vertical resolution of the file page in pixels per metre */
    int image_y_resolution;
    /*! \brief The number of horizontal pixels in the file page. */
    int image_width;
    /*! \brief The number of vertical pixels in the file page. */
    int image_length;
    /*! \brief The type of image in the exchanged page */
    int type;
    /*! \brief The horizontal resolution of the exchanged page in pixels per metre */
    int x_resolution;
    /*! \brief The vertical resolution of the exchanged page in pixels per metre */
    int y_resolution;
    /*! \brief The number of horizontal pixels in the exchanged page. */
    int width;
    /*! \brief The number of vertical pixels in the exchanged page. */
    int length;
    /*! \brief The type of compression used between the FAX machines */
    int compression;
    /*! \brief The size of the image on the line, in bytes */
    int line_image_size;
} t4_stats_t;

#if defined(__cplusplus)
extern "C" {
#endif

/*! \brief Prepare to receive the next page of the current document.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
SPAN_DECLARE(int) t4_rx_start_page(t4_rx_state_t *s);

/*! \brief Put a bit of the current document page.
    \param s The T.4 context.
    \param bit The data bit.
    \return Decode status. */
SPAN_DECLARE(int) t4_rx_put_bit(t4_rx_state_t *s, int bit);

/*! \brief Put a byte of the current document page.
    \param s The T.4 context.
    \param buf The buffer containing the chunk.
    \param len The length of the chunk.
    \return Decode status. */
SPAN_DECLARE(int) t4_rx_put(t4_rx_state_t *s, const uint8_t buf[], size_t len);

/*! \brief Complete the reception of a page.
    \param s The T.4 receive context.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_rx_end_page(t4_rx_state_t *s);

/*! \brief Set the row write handler for a T.4 receive context.
    \param s The T.4 receive context.
    \param handler A pointer to the handler routine.
    \param user_data An opaque pointer passed to the handler routine.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_rx_set_row_write_handler(t4_rx_state_t *s, t4_row_write_handler_t handler, void *user_data);

/*! \brief Set the encoding for the received data.
    \param s The T.4 context.
    \param encoding The encoding.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_rx_set_rx_encoding(t4_rx_state_t *s, int encoding);

/*! \brief Set the expected width of the received image, in pixel columns.
    \param s The T.4 context.
    \param width The number of pixels across the image. */
SPAN_DECLARE(void) t4_rx_set_image_width(t4_rx_state_t *s, int width);

/*! \brief Set the row-to-row (y) resolution to expect for a received image.
    \param s The T.4 context.
    \param resolution The resolution, in pixels per metre. */
SPAN_DECLARE(void) t4_rx_set_y_resolution(t4_rx_state_t *s, int resolution);

/*! \brief Set the column-to-column (x) resolution to expect for a received image.
    \param s The T.4 context.
    \param resolution The resolution, in pixels per metre. */
SPAN_DECLARE(void) t4_rx_set_x_resolution(t4_rx_state_t *s, int resolution);

/*! \brief Set the DCS information of the fax, for inclusion in the file.
    \param s The T.4 context.
    \param dcs The DCS information, formatted as an ASCII string. */
SPAN_DECLARE(void) t4_rx_set_dcs(t4_rx_state_t *s, const char *dcs);

/*! \brief Set the sub-address of the fax, for inclusion in the file.
    \param s The T.4 context.
    \param sub_address The sub-address string. */
SPAN_DECLARE(void) t4_rx_set_sub_address(t4_rx_state_t *s, const char *sub_address);

/*! \brief Set the identity of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param ident The identity string. */
SPAN_DECLARE(void) t4_rx_set_far_ident(t4_rx_state_t *s, const char *ident);

/*! \brief Set the vendor of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param vendor The vendor string, or NULL. */
SPAN_DECLARE(void) t4_rx_set_vendor(t4_rx_state_t *s, const char *vendor);

/*! \brief Set the model of the remote machine, for inclusion in the file.
    \param s The T.4 context.
    \param model The model string, or NULL. */
SPAN_DECLARE(void) t4_rx_set_model(t4_rx_state_t *s, const char *model);

/*! Get the current image transfer statistics.
    \brief Get the current transfer statistics.
    \param s The T.4 context.
    \param t A pointer to a statistics structure. */
SPAN_DECLARE(void) t4_rx_get_transfer_statistics(t4_rx_state_t *s, t4_stats_t *t);

/*! Get the short text name of a compression format.
    \brief Get the short text name of an encoding format.
    \param compression The compression type.
    \return A pointer to the string. */
SPAN_DECLARE(const char *) t4_compression_to_str(int compression);

/*! Get the short text name of an image format.
    \brief Get the short text name of an image format.
    \param type The image format.
    \return A pointer to the string. */
SPAN_DECLARE(const char *) t4_image_type_to_str(int type);

/*! Get the short text name of an image resolution.
    \brief Get the short text name of an image resolution.
    \param resolution_code The image resolution code.
    \return A pointer to the string. */
SPAN_DECLARE(const char *) t4_image_resolution_to_str(int resolution_code);

/*! Get the logging context associated with a T.4 receive context.
    \brief Get the logging context associated with a T.4 receive context.
    \param s The T.4 receive context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t4_rx_get_logging_state(t4_rx_state_t *s);

/*! \brief Prepare for reception of a document.
    \param s The T.4 context.
    \param file The name of the file to be received.
    \param supported_output_compressions The compression schemes supported for output to a TIFF file.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t4_rx_state_t *) t4_rx_init(t4_rx_state_t *s, const char *file, int supported_output_compressions);

/*! \brief End reception of a document. Tidy up and close the file.
           This should be used to end T.4 reception started with t4_rx_init.
    \param s The T.4 receive context.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_rx_release(t4_rx_state_t *s);

/*! \brief End reception of a document. Tidy up, close the file and
           free the context. This should be used to end T.4 reception
           started with t4_rx_init.
    \param s The T.4 receive context.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_rx_free(t4_rx_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

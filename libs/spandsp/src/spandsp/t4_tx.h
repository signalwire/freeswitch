/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_tx.h - definitions for T.4 FAX transmit processing
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

#if !defined(_SPANDSP_T4_TX_H_)
#define _SPANDSP_T4_TX_H_

/*! This function is a callback from the image decoders, to read the unencoded bi-level image,
    row by row. It is called for each row, with len set to the number of bytes per row expected.
    \return len for OK, or zero to indicate the end of the image data. */
typedef int (*t4_row_read_handler_t)(void *user_data, uint8_t buf[], size_t len);

/*!
    T.4 FAX compression/decompression descriptor. This defines the working state
    for a single instance of a T.4 FAX compression or decompression channel.
*/
typedef struct t4_tx_state_s t4_tx_state_t;

/* TIFF-FX related extensions to the TIFF tag set */

/*
Indexed(346) = 0, 1.                                                SHORT
        0: not a palette-color image.
        1: palette-color image.
    This field is used to indicate that each sample value is an index
    into an array of color values specified in the image data stream.
    Because the color map is embedded in the image data stream, the
    ColorMap field is not used in Profile L.  Lossless color fax
    profile supports palette-color images with the ITULAB encoding.
    The SamplesPerPixel value must be 1.

GlobalParametersIFD (400)                                           IFD/LONG
    An IFD containing global parameters. It is recommended that a TIFF
    writer place this field in the first IFD, where a TIFF reader would
    find it quickly.

    Each field in the GlobalParametersIFD is a TIFF field that is legal
    in any IFD. Required baseline fields should not be located in the
    GlobalParametersIFD, but should be in each image IFD. If a conflict
    exists between fields in the GlobalParametersIFD and in the image
    IFDs, then the data in the image IFD shall prevail.

    Among the GlobalParametersIFD entries is a new ProfileType field
    which generally describes information in this IFD and in the TIFF
    file.

ProfileType(401)                                                    LONG
    The type of image data stored in this IFD.
        0 = Unspecified
        1 = Group 3 fax
    No default

    The following new global fields are defined in this document as IFD
    entries for use with fax applications.

FaxProfile(402) = 0 - 6.                                            BYTE
    The profile that applies to this file; a profile is subset of the
    full set of permitted fields and field values of TIFF for facsimile.
    The currently defined values are:
        0: does not conform to a profile defined for TIFF for facsimile
        1: minimal black & white lossless, Profile S
        2: extended black & white lossless, Profile F
        3: lossless JBIG black & white, Profile J
        4: lossy color and grayscale, Profile C
        5: lossless color and grayscale, Profile L
        6: Mixed Raster Content, Profile M

CodingMethods(403)                                                  LONG
    This field indicates which coding methods are used in the file. A
    bit value of 1 indicates which of the following coding methods is
    used:
        Bit 0: unspecified compression,
        Bit 1: 1-dimensional coding, ITU-T Rec. T.4 (MH - Modified Huffman),
        Bit 2: 2-dimensional coding, ITU-T Rec. T.4 (MR - Modified Read),
        Bit 3: 2-dimensional coding, ITU-T Rec. T.6 (MMR - Modified MR),
        Bit 4: ITU-T Rec. T.82 coding, using ITU-T Rec. T.85 (JBIG),
        Bit 5: ITU-T Rec. T.81 (Baseline JPEG),
        Bit 6: ITU-T Rec. T.82 coding, using ITU-T Rec. T.43 (JBIG color),
        Bits 7-31: reserved for future use
    Note: There is a limit of 32 compression types to identify standard
    compression methods.

VersionYear(404)                                                    BYTE
    Count: 4
    The year of the standard specified by the FaxProfile field, given as
    4 characters, e.g. '1997'; used in lossy and lossless color modes.

ModeNumber (405)                                                    BYTE
    The mode of the standard specified by the FaxProfile field. A
    value of 0 indicates Mode 1.0; used in Mixed Raster Content mode.

Decode(433)                                                         SRATIONAL
    Count = 2 * SamplesPerPixel
    Describes how to map image sample values into the range of values
    appropriate for the current color space.  In general, the values
    are taken in pairs and specify the minimum and maximum output
    value for each color component.  For the base color fax profile,
    Decode has a count of 6 values and maps the unsigned ITULAB-
    encoded sample values (Lsample, asample, bsample) to signed L*a*b*
    values, as follows:
        L* = Decode[0] + Lsample x (Decode[1]-Decode[0])/(2^n -1)
        a* = Decode[2] + asample x (Decode[3]-Decode[2])/(2^n -1)
        b* = Decode[4] + bsample x (Decode[5]-Decode[4])/(2^n -1)
    where Decode[0], Decode[2] and Decode[4] are the minimum values
    for L*, a*, and b*; Decode[1], Decode[3] and Decode[5] are the
    maximum values for L*, a*, and b*; and n is the BitsPerSample.
    When n=8,=20  L*=Decode[0] when Lsample=0 and L*=Decode[1] when
    Lsample=255.

ImageBaseColor(434)                                                 SHORT
    Count = SamplesPerPixel
    In areas of an image layer where no image data is available (i.e.,
    where no strips are defined, or where the StripByteCounts entry for
    a given strip is 0), the color specified by ImageBaseColor will be
    used.

StripRowCounts(559)                                                 LONG
    Count = number of strips.
    The number of scanlines stored in a strip. Profile M allows each
    fax strip to store a different number of scanlines.  For strips
    with more than one layer, the maximum strip size is either 256
    scanlines or full page size. The 256 maximum SHOULD be used
    unless the capability to receive longer strips has been
    negotiated.  This field replaces RowsPerStrip for IFDs with
    variable-size strips. Only one of the two fields, StripRowCounts
    and RowsPerStrip, may be used in an IFD.

ImageLayer(34732)                                                   LONG
    Count = 2.
    Image layers are defined such that layer 1 is the Background
    layer, layer 3 is the Foreground layer, and layer 2 is the Mask
    layer, which selects pixels from the Background and Foreground
    layers. The ImageLayer tag contains two values, which describe
    the layer to which the image belongs and the order in which it is
    imaged.

    ImageLayer[0] = 1, 2, 3.
        1: Image is a Background image, i.e. the image that will appear
           whenever the Mask contains a value of 0. Background images
           typically contain low-resolution, continuous-tone imagery.
        2: Image is the Mask layer. In MRC, if the Mask layer is present,
           it must be the Primary IFD and be full page in extent.
        3: Image is a Foreground image, i.e. the image that will appear
           whenever the Mask contains a value of 1. The Foreground image
           generally defines the color of text or lines but may also
           contain high-resolution imagery.

    ImageLayer[1]:
        1: first image to be imaged in this layer
        2: second image to be imaged in this layer
        3: ...
*/

/* Define the TIFF/FX tags to extend libtiff, when using a version of libtiff where this
   stuff has not been merged. We only need to define these things for older versions of
   libtiff. */
#if defined(SPANDSP_SUPPORT_TIFF_FX)  &&  !defined(TIFFTAG_FAXPROFILE)
#define TIFFTAG_INDEXED                 346
#define TIFFTAG_GLOBALPARAMETERSIFD     400
#define TIFFTAG_PROFILETYPE             401
#define     PROFILETYPE_UNSPECIFIED     0
#define     PROFILETYPE_G3_FAX          1
#define TIFFTAG_FAXPROFILE              402
#define     FAXPROFILE_S                1
#define     FAXPROFILE_F                2
#define     FAXPROFILE_J                3
#define     FAXPROFILE_C                4
#define     FAXPROFILE_L                5
#define     FAXPROFILE_M                6
#define TIFFTAG_CODINGMETHODS           403
#define     CODINGMETHODS_T4_1D         (1 << 1)
#define     CODINGMETHODS_T4_2D         (1 << 2)
#define     CODINGMETHODS_T6            (1 << 3)
#define     CODINGMETHODS_T85           (1 << 4)
#define     CODINGMETHODS_T42           (1 << 5)
#define     CODINGMETHODS_T43           (1 << 6)
#define TIFFTAG_VERSIONYEAR             404
#define TIFFTAG_MODENUMBER              405
#define TIFFTAG_DECODE                  433
#define TIFFTAG_IMAGEBASECOLOR          434
#define TIFFTAG_T82OPTIONS              435
#define TIFFTAG_STRIPROWCOUNTS          559
#define TIFFTAG_IMAGELAYER              34732
#endif

#if !defined(COMPRESSION_T85)
#define     COMPRESSION_T85             9
#endif
#if !defined(COMPRESSION_T43)
#define     COMPRESSION_T43             10
#endif

typedef enum
{
    T4_IMAGE_FORMAT_OK = 0,
    T4_IMAGE_FORMAT_INCOMPATIBLE = -1,
    T4_IMAGE_FORMAT_NOSIZESUPPORT = -2,
    T4_IMAGE_FORMAT_NORESSUPPORT = -3
} t4_image_format_status_t;

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(SPANDSP_SUPPORT_TIFF_FX)
/*! \brief Configure libtiff so it recognises the extended tag set for TIFF-FX. */
SPAN_DECLARE(void) TIFF_FX_init(void);
#endif

/*! \brief Prepare to send the next page of the current document.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
SPAN_DECLARE(int) t4_tx_start_page(t4_tx_state_t *s);

/*! \brief Prepare the current page for a resend.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
SPAN_DECLARE(int) t4_tx_restart_page(t4_tx_state_t *s);

/*! \brief Check for the existance of the next page, and whether its format is like the
    current one. This information can be needed before it is determined that the current
    page is finished with.
    \param s The T.4 context.
    \return 0 for next page found with the same format as the current page.
            1 for next page found with different format from the current page.
            -1 for no page found, or file failure. */
SPAN_DECLARE(int) t4_tx_next_page_has_different_format(t4_tx_state_t *s);

/*! \brief Complete the sending of a page.
    \param s The T.4 context.
    \return zero for success, -1 for failure. */
SPAN_DECLARE(int) t4_tx_end_page(t4_tx_state_t *s);

/*! \brief Return the next bit of the current document page, without actually
           moving forward in the buffer. The document will be padded for the
           current minimum scan line time.
    \param s The T.4 context.
    \return 0 for more data to come. SIG_STATUS_END_OF_DATA for no more data. */
SPAN_DECLARE(int) t4_tx_image_complete(t4_tx_state_t *s);

/*! \brief Get the next bit of the current document page. The document will
           be padded for the current minimum scan line time.
    \param s The T.4 context.
    \return The next bit (i.e. 0 or 1). SIG_STATUS_END_OF_DATA for no more data. */
SPAN_DECLARE(int) t4_tx_get_bit(t4_tx_state_t *s);

/*! \brief Get the next chunk of the current document page. The document will
           be padded for the current minimum scan line time.
    \param s The T.4 context.
    \param buf The buffer into which the chunk is to written.
    \param max_len The maximum length of the chunk.
    \return The actual length of the chunk. If this is less than max_len it
            indicates that the end of the document has been reached. */
SPAN_DECLARE(int) t4_tx_get(t4_tx_state_t *s, uint8_t buf[], size_t max_len);

/*! \brief Get the compression for the encoded data.
    \param s The T.4 context.
    \return the chosen compression for success, otherwise -1. */
SPAN_DECLARE(int) t4_tx_get_tx_compression(t4_tx_state_t *s);

/*! \brief Get the image type of the encoded data.
    \param s The T.4 context.
    \return the chosen image type for success, otherwise -1. */
SPAN_DECLARE(int) t4_tx_get_tx_image_type(t4_tx_state_t *s);

/*! \brief Get the X and Y resolution code of the current page.
    \param s The T.4 context.
    \return The resolution code,. */
SPAN_DECLARE(int) t4_tx_get_tx_resolution(t4_tx_state_t *s);

/*! \brief Get the column-to-column (x) resolution of the current page.
    \param s The T.4 context.
    \return The resolution, in pixels per metre. */
SPAN_DECLARE(int) t4_tx_get_tx_x_resolution(t4_tx_state_t *s);

/*! \brief Get the row-to-row (y) resolution of the current page.
    \param s The T.4 context.
    \return The resolution, in pixels per metre. */
SPAN_DECLARE(int) t4_tx_get_tx_y_resolution(t4_tx_state_t *s);

/*! \brief Get the width of the encoded data.
    \param s The T.4 context.
    \return the width, in pixels, for success, otherwise -1. */
SPAN_DECLARE(int) t4_tx_get_tx_image_width(t4_tx_state_t *s);

/*! \brief Get the width code of the encoded data.
    \param s The T.4 context.
    \return the width code, for success, otherwise -1. */
SPAN_DECLARE(int) t4_tx_get_tx_image_width_code(t4_tx_state_t *s);

/*! \brief Auto-select the format in which to send the image.
    \param s The T.4 context.
    \param supported_compressions The set of compressions supported for this transmission
    \param supported_image_sizes The set of image sizes supported for this transmission
    \param supported_bilevel_resolutions The set of bi-level resolutions supported for this transmission
    \param supported_colour_resolutions The set of gray scale and colour resolutions supported for this transmission
    \return A t4_image_format_status_t result code */
SPAN_DECLARE(int) t4_tx_set_tx_image_format(t4_tx_state_t *s,
                                            int supported_compressions,
                                            int supported_image_sizes,
                                            int supported_bilevel_resolutions,
                                            int supported_colour_resolutions);

/*! \brief Set the minimum number of encoded bits per row. This allows the
           makes the encoding process to be set to comply with the minimum row
           time specified by a remote receiving machine.
    \param s The T.4 context.
    \param bits The minimum number of bits per row. */
SPAN_DECLARE(void) t4_tx_set_min_bits_per_row(t4_tx_state_t *s, int bits);

/*! \brief Set the maximum number of 2D encoded rows between 1D encoded rows. This
           is only valid for T.4 2D encoding.
    \param s The T.4 context.
    \param max The maximum number of 2D rows. */
SPAN_DECLARE(void) t4_tx_set_max_2d_rows_per_1d_row(t4_tx_state_t *s, int max);

/*! \brief Set the identity of the local machine, for inclusion in page headers.
    \param s The T.4 context.
    \param ident The identity string. */
SPAN_DECLARE(void) t4_tx_set_local_ident(t4_tx_state_t *s, const char *ident);

/*! Set the info field, included in the header line included in each page of an encoded
    FAX. This is a string of up to 50 characters. Other information (date, local ident, etc.)
    are automatically included in the header. If the header info is set to NULL or a zero
    length string, no header lines will be added to the encoded FAX.
    \brief Set the header info.
    \param s The T.4 context.
    \param info A string, of up to 50 bytes, which will form the info field. */
SPAN_DECLARE(void) t4_tx_set_header_info(t4_tx_state_t *s, const char *info);

/*! Set the time zone for the time stamp in page header lines. If this function is not used
    the current time zone of the program's environment is used.
    \brief Set the header timezone.
    \param s The T.4 context.
    \param tz A time zone descriptor. */
SPAN_DECLARE(void) t4_tx_set_header_tz(t4_tx_state_t *s, tz_t *tz);

/*! Set page header extends or overlays the image mode.
    \brief Set page header overlay mode.
    \param s The T.4 context.
    \param header_overlays_image True for overlay, or false to extend the page. */
SPAN_DECLARE(void) t4_tx_set_header_overlays_image(t4_tx_state_t *s, bool header_overlays_image);

/*! \brief Set the row read handler for a T.4 transmit context.
    \param s The T.4 transmit context.
    \param handler A pointer to the handler routine.
    \param user_data An opaque pointer passed to the handler routine.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_tx_set_row_read_handler(t4_tx_state_t *s, t4_row_read_handler_t handler, void *user_data);

/*! \brief Get the number of pages in the file.
    \param s The T.4 context.
    \return The number of pages, or -1 if there is an error. */
SPAN_DECLARE(int) t4_tx_get_pages_in_file(t4_tx_state_t *s);

/*! \brief Get the currnet page number in the file.
    \param s The T.4 context.
    \return The page number, or -1 if there is an error. */
SPAN_DECLARE(int) t4_tx_get_current_page_in_file(t4_tx_state_t *s);

/*! Get the current image transfer statistics.
    \brief Get the current transfer statistics.
    \param s The T.4 context.
    \param t A pointer to a statistics structure. */
SPAN_DECLARE(void) t4_tx_get_transfer_statistics(t4_tx_state_t *s, t4_stats_t *t);

/*! Get the logging context associated with a T.4 transmit context.
    \brief Get the logging context associated with a T.4 transmit context.
    \param s The T.4 transmit context.
    \return A pointer to the logging context */
SPAN_DECLARE(logging_state_t *) t4_tx_get_logging_state(t4_tx_state_t *s);

/*! \brief Prepare for transmission of a document.
    \param s The T.4 context.
    \param file The name of the file to be sent.
    \param start_page The first page to send. -1 for no restriction.
    \param stop_page The last page to send. -1 for no restriction.
    \return A pointer to the context, or NULL if there was a problem. */
SPAN_DECLARE(t4_tx_state_t *) t4_tx_init(t4_tx_state_t *s, const char *file, int start_page, int stop_page);

/*! \brief End the transmission of a document. Tidy up and close the file.
           This should be used to end T.4 transmission started with t4_tx_init.
    \param s The T.4 context.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_tx_release(t4_tx_state_t *s);

/*! \brief End the transmission of a document. Tidy up, close the file and
           free the context. This should be used to end T.4 transmission
           started with t4_tx_init.
    \param s The T.4 context.
    \return 0 for success, otherwise -1. */
SPAN_DECLARE(int) t4_tx_free(t4_tx_state_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

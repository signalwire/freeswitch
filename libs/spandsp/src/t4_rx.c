/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t4_rx.c - ITU T.4 FAX image receive processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003, 2007, 2010 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <memory.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"
#include <tiffio.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/bit_operations.h"
#include "spandsp/async.h"
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/image_translate.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#include "spandsp/t42.h"
#include "spandsp/t43.h"
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"
#include "spandsp/version.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#include "spandsp/private/t43.h"
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/image_translate.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"

/*! The number of centimetres in one inch */
#define CM_PER_INCH                 2.54f

typedef struct
{
    uint8_t *buf;
    int ptr;
} packer_t;

#if defined(SPANDSP_SUPPORT_TIFF_FX)
#if TIFFLIB_VERSION >= 20120922  &&  defined(HAVE_TIF_DIR_H)
extern TIFFFieldArray tiff_fx_field_array;
#endif
#endif

SPAN_DECLARE(const char *) t4_compression_to_str(int compression)
{
    switch (compression)
    {
    case T4_COMPRESSION_NONE:
        return "None";
    case T4_COMPRESSION_T4_1D:
        return "T.4 1-D";
    case T4_COMPRESSION_T4_2D:
        return "T.4 2-D";
    case T4_COMPRESSION_T6:
        return "T.6";
    case T4_COMPRESSION_T85:
        return "T.85";
    case T4_COMPRESSION_T85_L0:
        return "T.85(L0)";
    case T4_COMPRESSION_T88:
        return "T.88";
    case T4_COMPRESSION_T42_T81:
        return "T.81+T.42";
    case T4_COMPRESSION_SYCC_T81:
        return "T.81+sYCC";
    case T4_COMPRESSION_T43:
        return "T.43";
    case T4_COMPRESSION_T45:
        return "T.45";
    /* Compressions which can only be used in TIFF files */
    case T4_COMPRESSION_UNCOMPRESSED:
        return "Uncompressed";
    case T4_COMPRESSION_JPEG:
        return "JPEG";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t4_image_type_to_str(int type)
{
    switch (type)
    {
    case T4_IMAGE_TYPE_BILEVEL:
        return "bi-level";
    case T4_IMAGE_TYPE_COLOUR_BILEVEL:
        return "bi-level colour";
    case T4_IMAGE_TYPE_4COLOUR_BILEVEL:
        return "CMYK bi-level colour";
    case T4_IMAGE_TYPE_GRAY_8BIT:
        return "8-bit gray scale";
    case T4_IMAGE_TYPE_GRAY_12BIT:
        return "12-bit gray scale";
    case T4_IMAGE_TYPE_COLOUR_8BIT:
        return "8-bit colour";
    case T4_IMAGE_TYPE_4COLOUR_8BIT:
        return "CMYK 8-bit colour";
    case T4_IMAGE_TYPE_COLOUR_12BIT:
        return "12-bit colour";
    case T4_IMAGE_TYPE_4COLOUR_12BIT:
        return "CMYK 12-bit colour";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) t4_image_resolution_to_str(int resolution_code)
{
    switch (resolution_code)
    {
    case T4_RESOLUTION_R8_STANDARD:
        return "204dpi x 98dpi";
    case T4_RESOLUTION_R8_FINE:
        return "204dpi x 196dpi";
    case T4_RESOLUTION_R8_SUPERFINE:
        return "204dpi x 391dpi";
    case T4_RESOLUTION_R16_SUPERFINE:
        return "408dpi x 391dpi";
    case T4_RESOLUTION_100_100:
        return "100dpi x 100dpi";
    case T4_RESOLUTION_200_100:
        return "200dpi x 100dpi";
    case T4_RESOLUTION_200_200:
        return "200dpi x 200dpi";
    case T4_RESOLUTION_200_400:
        return "200dpi x 400dpi";
    case T4_RESOLUTION_300_300:
        return "300dpi x 300dpi";
    case T4_RESOLUTION_300_600:
        return "300dpi x 600dpi";
    case T4_RESOLUTION_400_400:
        return "400dpi x 400dpi";
    case T4_RESOLUTION_400_800:
        return "400dpi x 800dpi";
    case T4_RESOLUTION_600_600:
        return "600dpi x 600dpi";
    case T4_RESOLUTION_600_1200:
        return "600dpi x 1200dpi";
    case T4_RESOLUTION_1200_1200:
        return "1200dpi x 1200dpi";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

static int set_tiff_directory_info(t4_rx_state_t *s)
{
    time_t now;
    struct tm *tm;
    char buf[256 + 1];
    uint16_t resunit;
    float x_resolution;
    float y_resolution;
    t4_rx_tiff_state_t *t;
    int32_t output_compression;
    int32_t output_t4_options;
    int bits_per_sample;
    int samples_per_pixel;
    int photometric;
    uint32_t width;
    uint32_t length;

    t = &s->tiff;
    /* Prepare the directory entry fully before writing the image, or libtiff complains */
    bits_per_sample = 1;
    samples_per_pixel = 1;
    photometric = PHOTOMETRIC_MINISWHITE;
    output_t4_options = 0;
    switch (t->compression)
    {
    case T4_COMPRESSION_T4_1D:
    default:
        output_compression = COMPRESSION_CCITT_T4;
        output_t4_options = GROUP3OPT_FILLBITS;
        break;
    case T4_COMPRESSION_T4_2D:
        output_compression = COMPRESSION_CCITT_T4;
        output_t4_options = GROUP3OPT_FILLBITS | GROUP3OPT_2DENCODING;
        break;
    case T4_COMPRESSION_T6:
        output_compression = COMPRESSION_CCITT_T6;
        break;
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        output_compression = COMPRESSION_T85;
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        output_compression = COMPRESSION_T88;
        break;
#endif
    case T4_COMPRESSION_JPEG:
        output_compression = COMPRESSION_JPEG;
        bits_per_sample = 8;
        if (t->image_type == T4_IMAGE_TYPE_COLOUR_8BIT)
        {
            samples_per_pixel = 3;
            photometric = PHOTOMETRIC_YCBCR;
        }
        else
        {
            samples_per_pixel = 1;
            photometric = PHOTOMETRIC_MINISBLACK;
        }
        break;
    case T4_COMPRESSION_T42_T81:
        output_compression = COMPRESSION_JPEG;
        bits_per_sample = 8;
        if (t->image_type == T4_IMAGE_TYPE_COLOUR_8BIT)
        {
            samples_per_pixel = 3;
            photometric = PHOTOMETRIC_ITULAB;
        }
        else
        {
            samples_per_pixel = 1;
            photometric = PHOTOMETRIC_MINISBLACK;
        }
        break;
    case T4_COMPRESSION_SYCC_T81:
        output_compression = COMPRESSION_JPEG;
        bits_per_sample = 8;
        if (t->image_type == T4_IMAGE_TYPE_COLOUR_8BIT)
        {
            samples_per_pixel = 3;
            photometric = PHOTOMETRIC_YCBCR;
        }
        else
        {
            samples_per_pixel = 1;
            photometric = PHOTOMETRIC_MINISBLACK;
        }
        break;
    case T4_COMPRESSION_T43:
        output_compression = COMPRESSION_T43;
        bits_per_sample = 8;
        samples_per_pixel = 3;
        photometric = PHOTOMETRIC_ITULAB;
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        output_compression = COMPRESSION_T45;
        bits_per_sample = 8;
        samples_per_pixel = 3;
        photometric = PHOTOMETRIC_ITULAB;
        break;
#endif
    }

    TIFFSetField(t->tiff_file, TIFFTAG_COMPRESSION, output_compression);
    switch (output_compression)
    {
    case COMPRESSION_CCITT_T4:
        TIFFSetField(t->tiff_file, TIFFTAG_T4OPTIONS, output_t4_options);
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        break;
    case COMPRESSION_CCITT_T6:
        TIFFSetField(t->tiff_file, TIFFTAG_T6OPTIONS, 0);
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        break;
    case COMPRESSION_T85:
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        break;
    case COMPRESSION_JPEG:
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        break;
    case COMPRESSION_T43:
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        break;
    }
    TIFFSetField(t->tiff_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFSetField(t->tiff_file, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
    TIFFSetField(t->tiff_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(t->tiff_file, TIFFTAG_PHOTOMETRIC, photometric);
    TIFFSetField(t->tiff_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
    switch (t->compression)
    {
    case T4_COMPRESSION_JPEG:
        TIFFSetField(t->tiff_file, TIFFTAG_YCBCRSUBSAMPLING, 2, 2);
        //TIFFSetField(t->tiff_file, TIFFTAG_YCBCRSUBSAMPLING, 1, 1);
        TIFFSetField(t->tiff_file, TIFFTAG_JPEGQUALITY, 75);
        TIFFSetField(t->tiff_file, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        break;
    case T4_COMPRESSION_T42_T81:
        TIFFSetField(t->tiff_file, TIFFTAG_YCBCRSUBSAMPLING, 2, 2);
        //TIFFSetField(t->tiff_file, TIFFTAG_YCBCRSUBSAMPLING, 1, 1);
        TIFFSetField(t->tiff_file, TIFFTAG_JPEGQUALITY, 75);
        TIFFSetField(t->tiff_file, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);
        break;
    }
    /* TIFFTAG_STRIPBYTECOUNTS and TIFFTAG_STRIPOFFSETS are added automatically */

    x_resolution = s->metadata.x_resolution/100.0f;
    y_resolution = s->metadata.y_resolution/100.0f;
    /* Metric seems the sane thing to use in the 21st century, but a lot of lousy software
       gets FAX resolutions wrong, and more get it wrong using metric than using inches. */
#if 0
    TIFFSetField(t->tiff_file, TIFFTAG_XRESOLUTION, x_resolution);
    TIFFSetField(t->tiff_file, TIFFTAG_YRESOLUTION, y_resolution);
    resunit = RESUNIT_CENTIMETER;
    TIFFSetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, resunit);
#else
    TIFFSetField(t->tiff_file, TIFFTAG_XRESOLUTION, floorf(x_resolution*CM_PER_INCH + 0.5f));
    TIFFSetField(t->tiff_file, TIFFTAG_YRESOLUTION, floorf(y_resolution*CM_PER_INCH + 0.5f));
    resunit = RESUNIT_INCH;
    TIFFSetField(t->tiff_file, TIFFTAG_RESOLUTIONUNIT, resunit);
#endif
    TIFFSetField(t->tiff_file, TIFFTAG_SOFTWARE, "Spandsp " SPANDSP_RELEASE_DATETIME_STRING);
    if (gethostname(buf, sizeof(buf)) == 0)
        TIFFSetField(t->tiff_file, TIFFTAG_HOSTCOMPUTER, buf);

#if defined(TIFFTAG_FAXDCS)
    if (s->metadata.dcs)
        TIFFSetField(t->tiff_file, TIFFTAG_FAXDCS, s->metadata.dcs);
#endif
    if (s->metadata.sub_address)
        TIFFSetField(t->tiff_file, TIFFTAG_FAXSUBADDRESS, s->metadata.sub_address);
    if (s->metadata.far_ident)
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGEDESCRIPTION, s->metadata.far_ident);
    if (s->metadata.vendor)
        TIFFSetField(t->tiff_file, TIFFTAG_MAKE, s->metadata.vendor);
    if (s->metadata.model)
        TIFFSetField(t->tiff_file, TIFFTAG_MODEL, s->metadata.model);

    time(&now);
    tm = localtime(&now);
    sprintf(buf,
            "%4d/%02d/%02d %02d:%02d:%02d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec);
    TIFFSetField(t->tiff_file, TIFFTAG_DATETIME, buf);
    TIFFSetField(t->tiff_file, TIFFTAG_FAXRECVTIME, now - s->tiff.page_start_time);

    TIFFSetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, s->metadata.image_width);
    /* Set the total pages to 1. For any one page document we will get this
       right. For multi-page documents we will need to come back and fill in
       the right answer when we know it. */
    TIFFSetField(t->tiff_file, TIFFTAG_PAGENUMBER, s->current_page, 1);
    /* TIFF page numbers start from zero, so the number of pages in the file
       is always one greater than the highest page number in the file. */
    s->tiff.pages_in_file = s->current_page + 1;
    s->metadata.image_length = 0;
    switch (s->current_decoder)
    {
    case 0:
        switch (t->compression)
        {
        case T4_COMPRESSION_T42_T81:
        case T4_COMPRESSION_SYCC_T81:
            t42_analyse_header(&width, &length, s->decoder.no_decoder.buf, s->decoder.no_decoder.buf_ptr);
            s->metadata.image_width = width;
            s->metadata.image_length = length;
            break;
        case T4_COMPRESSION_T85:
        case T4_COMPRESSION_T85_L0:
            t85_analyse_header(&width, &length, s->decoder.no_decoder.buf, s->decoder.no_decoder.buf_ptr);
            s->metadata.image_width = width;
            s->metadata.image_length = length;
            break;
        }
        break;
    case T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6:
        if ((s->metadata.compression & (T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D)))
        {
            /* We only get bad row info from pages received in non-ECM mode. */
            if (output_compression == COMPRESSION_CCITT_T4)
            {
                if (s->decoder.t4_t6.bad_rows)
                {
                    TIFFSetField(t->tiff_file, TIFFTAG_BADFAXLINES, s->decoder.t4_t6.bad_rows);
                    TIFFSetField(t->tiff_file, TIFFTAG_CONSECUTIVEBADFAXLINES, s->decoder.t4_t6.longest_bad_row_run);
                    TIFFSetField(t->tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_REGENERATED);
                }
                else
                {
                    TIFFSetField(t->tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_CLEAN);
                }
            }
        }
        s->metadata.image_length = t4_t6_decode_get_image_length(&s->decoder.t4_t6);
        break;
    case T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0:
        s->metadata.image_length = t85_decode_get_image_length(&s->decoder.t85);
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        s->metadata.image_length = t88_decode_get_image_length(&s->decoder.t88);
        break;
#endif
    case T4_COMPRESSION_T42_T81:
        s->metadata.image_length = t42_decode_get_image_length(&s->decoder.t42);
        break;
    case T4_COMPRESSION_T43:
        s->metadata.image_length = t43_decode_get_image_length(&s->decoder.t43);
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        s->metadata.image_length = t45_decode_get_image_length(&s->decoder.t45);
        break;
#endif
    }
    TIFFSetField(t->tiff_file, TIFFTAG_IMAGELENGTH, s->metadata.image_length);
    TIFFSetField(t->tiff_file, TIFFTAG_ROWSPERSTRIP, s->metadata.image_length);
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    TIFFSetField(t->tiff_file, TIFFTAG_PROFILETYPE, PROFILETYPE_G3_FAX);
    TIFFSetField(t->tiff_file, TIFFTAG_FAXPROFILE, FAXPROFILE_S);
    TIFFSetField(t->tiff_file, TIFFTAG_CODINGMETHODS, CODINGMETHODS_T4_1D | CODINGMETHODS_T4_2D | CODINGMETHODS_T6);
    TIFFSetField(t->tiff_file, TIFFTAG_VERSIONYEAR, "1998");
    if (s->current_page == 0)
    {
        /* Create a placeholder for the global parameters IFD, to be filled in later */
        TIFFSetField(t->tiff_file, TIFFTAG_GLOBALPARAMETERSIFD, 0);
    }

#if 0
    /* Paletised image? */
    TIFFSetField(t->tiff_file, TIFFTAG_INDEXED, 1);
    /* T.44 mode */
    TIFFSetField(t->tiff_file, TIFFTAG_MODENUMBER, 0);
    span_log(&s->logging, SPAN_LOG_FLOW, "TIFF/FX stuff 2\n");
    {
        float xxx[] = {20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0};
        TIFFSetField(t->tiff_file, TIFFTAG_DECODE, (uint16) 2*samples_per_pixel, xxx);
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "TIFF/FX stuff 3\n");
    {
        uint16_t xxx[] = {12, 34, 45, 67};
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGEBASECOLOR, (uint16_t) samples_per_pixel, xxx);
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "TIFF/FX stuff 4\n");
    TIFFSetField(t->tiff_file, TIFFTAG_T82OPTIONS, 0);
    {
        uint32_t xxx[] = {34, 56, 78, 90};
        TIFFSetField(t->tiff_file, TIFFTAG_STRIPROWCOUNTS, (uint16_t) 5, xxx);
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "TIFF/FX stuff 5\n");
    {
        uint32_t xxx[] = {2, 3};
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGELAYER, xxx);
    }
#endif
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int open_tiff_output_file(t4_rx_state_t *s, const char *file)
{
    if ((s->tiff.tiff_file = TIFFOpen(file, "w")) == NULL)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int row_read_handler(void *user_data, uint8_t row[], size_t len)
{
    packer_t *s;

    s = (packer_t *) user_data;
    memcpy(row, &s->buf[s->ptr], len);
    s->ptr += len;
    return len;
}
/*- End of function --------------------------------------------------------*/

static int write_tiff_t85_image(t4_rx_state_t *s)
{
    uint8_t *buf;
    uint8_t *buf2;
    int buf_len;
    int len;
    int image_len;
    t85_encode_state_t t85;
    packer_t packer;

    /* We need to perform this compression here, as libtiff does not understand it. */
    packer.buf = s->tiff.image_buffer;
    packer.ptr = 0;
    if (t85_encode_init(&t85, s->metadata.image_width, s->metadata.image_length, row_read_handler, &packer) == NULL)
        return -1;
    //if (t->compression == T4_COMPRESSION_T85_L0)
    //    t85_encode_set_options(&t85, 256, -1, -1);
    buf = NULL;
    buf_len = 0;
    image_len = 0;
    do
    {
        if (buf_len < image_len + 65536)
        {
            buf_len += 65536;
            if ((buf2 = span_realloc(buf, buf_len)) == NULL)
            {
                if (buf)
                    span_free(buf);
                return -1;
            }
            buf = buf2;
        }
        len = t85_encode_get(&t85, &buf[image_len], buf_len - image_len);
        image_len += len;
    }
    while (len > 0);
    if (TIFFWriteRawStrip(s->tiff.tiff_file, 0, buf, image_len) < 0)
    {
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Error writing TIFF strip.\n", s->tiff.file);
        return -1;
    }
    t85_encode_release(&t85);
    span_free(buf);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int write_tiff_t43_image(t4_rx_state_t *s)
{
    uint8_t *buf;
    uint8_t *buf2;
    int buf_len;
    int len;
    int image_len;
    t43_encode_state_t t43;
    packer_t packer;

    packer.buf = s->tiff.image_buffer;
    packer.ptr = 0;
    if (t43_encode_init(&t43, s->metadata.image_width, s->metadata.image_length, row_read_handler, &packer) == NULL)
        return -1;
    buf = NULL;
    buf_len = 0;
    image_len = 0;
    do
    {
        if (buf_len < image_len + 65536)
        {
            buf_len += 65536;
            if ((buf2 = span_realloc(buf, buf_len)) == NULL)
            {
                if (buf)
                    span_free(buf);
                return -1;
            }
            buf = buf2;
        }
        len = t43_encode_get(&t43, &buf[image_len], buf_len - image_len);
        image_len += len;
    }
    while (len > 0);
    if (TIFFWriteRawStrip(s->tiff.tiff_file, 0, buf, image_len) < 0)
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Error writing TIFF strip.\n", s->tiff.file);
    t43_encode_release(&t43);
    span_free(buf);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int write_tiff_image(t4_rx_state_t *s)
{
    t4_rx_tiff_state_t *t;
#if defined(SPANDSP_SUPPORT_TIFF_FX)  &&  TIFFLIB_VERSION >= 20120922  &&  defined(HAVE_TIF_DIR_H)
    toff_t diroff;
#endif

    t = &s->tiff;
    if (s->decoder.no_decoder.buf_ptr <= 0  &&  (t->image_buffer == NULL  ||  t->image_size <= 0))
        return -1;
    /* Set up the TIFF directory info... */
    set_tiff_directory_info(s);
    /* ...Put the directory in the file before the image data, to get them in the order specified
       for TIFF/F files... */
    //if (!TIFFCheckpointDirectory(t->tiff_file))
    //    span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to checkpoint directory for page %d.\n", t->file, s->current_page);
    /* ...and write out the image... */
    if (s->current_decoder == 0)
    {
        if (TIFFWriteRawStrip(s->tiff.tiff_file, 0, s->decoder.no_decoder.buf, s->decoder.no_decoder.buf_ptr) < 0)
            span_log(&s->logging, SPAN_LOG_WARNING, "%s: Error writing TIFF strip.\n", s->tiff.file);
    }
    else
    {
        switch (t->compression)
        {
        case T4_COMPRESSION_T85:
        case T4_COMPRESSION_T85_L0:
            /* We need to perform this compression here, as libtiff does not understand it. */
            if (write_tiff_t85_image(s) < 0)
                return -1;
            break;
#if defined(SPANDSP_SUPPORT_T88)
        case T4_COMPRESSION_T88:
            /* We need to perform this compression here, as libtiff does not understand it. */
            if (write_tiff_t88_image(s) < 0)
                return -1;
            break;
#endif
        case T4_COMPRESSION_T43:
            /* We need to perform this compression here, as libtiff does not understand it. */
            if (write_tiff_t43_image(s) < 0)
                return -1;
            break;
#if defined(SPANDSP_SUPPORT_T45)
        case T4_COMPRESSION_T45:
            /* We need to perform this compression here, as libtiff does not understand it. */
            if (write_tiff_t45_image(s) < 0)
                return -1;
            break;
#endif
        default:
            /* Let libtiff do the compression */
            if (TIFFWriteEncodedStrip(t->tiff_file, 0, t->image_buffer, t->image_size) < 0)
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Error writing TIFF strip.\n", t->file);
            break;
        }
    }
    /* ...then finalise the directory entry, and libtiff is happy. */
    if (!TIFFWriteDirectory(t->tiff_file))
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to write directory for page %d.\n", t->file, s->current_page);
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    /* According to the TIFF/FX spec, a global parameters IFD should only be inserted into
       the first page in the file */
    if (s->current_page == 0)
    {
#if TIFFLIB_VERSION >= 20120922  &&  defined(HAVE_TIF_DIR_H)
        if (!TIFFCreateCustomDirectory(t->tiff_file, &tiff_fx_field_array))
        {
            TIFFSetField(t->tiff_file, TIFFTAG_FAXPROFILE, PROFILETYPE_G3_FAX);
            TIFFSetField(t->tiff_file, TIFFTAG_PROFILETYPE, FAXPROFILE_F);
            TIFFSetField(t->tiff_file, TIFFTAG_CODINGMETHODS, CODINGMETHODS_T4_1D | CODINGMETHODS_T4_2D | CODINGMETHODS_T6);
            TIFFSetField(t->tiff_file, TIFFTAG_VERSIONYEAR, "1998");
            TIFFSetField(t->tiff_file, TIFFTAG_MODENUMBER, 3);

            diroff = 0;
            if (!TIFFWriteCustomDirectory(t->tiff_file, &diroff))
                span_log(&s->logging, SPAN_LOG_WARNING, "Failed to write custom directory.\n");

            /* Now go back and patch in the pointer to the new IFD */
            if (!TIFFSetDirectory(t->tiff_file, s->current_page))
                span_log(&s->logging, SPAN_LOG_WARNING, "Failed to set directory.\n");
            if (!TIFFSetField(t->tiff_file, TIFFTAG_GLOBALPARAMETERSIFD, diroff))
                span_log(&s->logging, SPAN_LOG_WARNING, "Failed to set field.\n");
            if (!TIFFWriteDirectory(t->tiff_file))
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to write directory for page %d.\n", t->file, s->current_page);
        }
#endif
    }
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int close_tiff_output_file(t4_rx_state_t *s)
{
    int i;
    t4_rx_tiff_state_t *t;

    t = &s->tiff;
    /* Perform any operations needed to tidy up a written TIFF file before
       closure. */
    if (s->current_page > 1)
    {
        /* We need to edit the TIFF directories. Until now we did not know
           the total page count, so the TIFF file currently says one. Now we
           need to set the correct total page count associated with each page. */
        for (i = 0;  i < s->current_page;  i++)
        {
            if (!TIFFSetDirectory(t->tiff_file, (tdir_t) i))
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to set directory to page %d.\n", s->tiff.file, i);
            TIFFSetField(t->tiff_file, TIFFTAG_PAGENUMBER, i, s->current_page);
            if (!TIFFWriteDirectory(t->tiff_file))
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to write directory for page %d.\n", s->tiff.file, i);
        }
    }
    TIFFClose(t->tiff_file);
    t->tiff_file = NULL;
    if (s->tiff.file)
    {
        /* Try not to leave a file behind, if we didn't receive any pages to
           put in it. */
        if (s->current_page == 0)
        {
            if (remove(s->tiff.file) < 0)
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to remove file.\n", s->tiff.file);
        }
        span_free((char *) s->tiff.file);
    }
    s->tiff.file = NULL;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void tiff_rx_release(t4_rx_state_t *s)
{
    if (s->tiff.tiff_file)
        close_tiff_output_file(s);
    if (s->tiff.image_buffer)
    {
        span_free(s->tiff.image_buffer);
        s->tiff.image_buffer = NULL;
        s->tiff.image_size = 0;
        s->tiff.image_buffer_size = 0;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_put_bit(t4_rx_state_t *s, int bit)
{
    /* We only put bit by bit for T.4-1D and T.4-2D */
    s->line_image_size += 1;
    return t4_t6_decode_put_bit(&s->decoder.t4_t6, bit);
}
/*- End of function --------------------------------------------------------*/

static void pre_encoded_restart(no_decoder_state_t *s)
{
    s->buf_ptr = 0;
}
/*- End of function --------------------------------------------------------*/

static void pre_encoded_init(no_decoder_state_t *s)
{
    s->buf = NULL;
    s->buf_len = 0;
    s->buf_ptr = 0;
}
/*- End of function --------------------------------------------------------*/

static int pre_encoded_release(no_decoder_state_t *s)
{
    if (s->buf)
        span_free(s->buf);
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int pre_encoded_put(no_decoder_state_t *s, const uint8_t data[], size_t len)
{
    uint8_t *buf;

    if (s->buf_len < s->buf_ptr + len)
    {
        s->buf_len += 65536;
        if ((buf = span_realloc(s->buf, s->buf_len)) == NULL)
        {
            if (s->buf)
            {
                span_free(s->buf);
                s->buf = NULL;
                s->buf_len = 0;
            }
            return -1;
        }
        s->buf = buf;
    }
    memcpy(&s->buf[s->buf_ptr], data, len);
    s->buf_ptr += len;
    return T4_DECODE_MORE_DATA;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_put(t4_rx_state_t *s, const uint8_t buf[], size_t len)
{
    s->line_image_size += 8*len;

    if (s->image_put_handler)
        return s->image_put_handler((void *) &s->decoder, buf, len);

    return T4_DECODE_OK;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_y_resolution(t4_rx_state_t *s, int resolution)
{
    s->metadata.y_resolution = resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_x_resolution(t4_rx_state_t *s, int resolution)
{
    s->metadata.x_resolution = resolution;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_dcs(t4_rx_state_t *s, const char *dcs)
{
    s->metadata.dcs = (dcs  &&  dcs[0])  ?  dcs  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_sub_address(t4_rx_state_t *s, const char *sub_address)
{
    s->metadata.sub_address = (sub_address  &&  sub_address[0])  ?  sub_address  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_far_ident(t4_rx_state_t *s, const char *ident)
{
    s->metadata.far_ident = (ident  &&  ident[0])  ?  ident  :  NULL;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_vendor(t4_rx_state_t *s, const char *vendor)
{
    s->metadata.vendor = vendor;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_model(t4_rx_state_t *s, const char *model)
{
    s->metadata.model = model;
}
/*- End of function --------------------------------------------------------*/

static bool select_tiff_compression(t4_rx_state_t *s, int output_image_type)
{
    s->tiff.image_type = output_image_type;
    /* The only compression schemes where we can really avoid decoding and
       recoding the images are those where the width an length of the image
       can be readily extracted from the image data (e.g. from its header) */
    if ((s->metadata.compression & (s->supported_tiff_compressions & (T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0 | T4_COMPRESSION_T42_T81 | T4_COMPRESSION_SYCC_T81))))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Image can be written without recoding\n");
        s->tiff.compression = s->metadata.compression;
        return false;
    }

    if (output_image_type == T4_IMAGE_TYPE_BILEVEL)
    {
        /* Only provide for one form of coding throughout the file, even though the
           coding on the wire could change between pages. */
        if ((s->supported_tiff_compressions & T4_COMPRESSION_T88))
            s->tiff.compression = T4_COMPRESSION_T88;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_T85))
            s->tiff.compression = T4_COMPRESSION_T85;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_T6))
            s->tiff.compression = T4_COMPRESSION_T6;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_T4_2D))
            s->tiff.compression = T4_COMPRESSION_T4_2D;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_T4_1D))
            s->tiff.compression = T4_COMPRESSION_T4_1D;
    }
    else
    {
        if ((s->supported_tiff_compressions & T4_COMPRESSION_JPEG))
            s->tiff.compression = T4_COMPRESSION_JPEG;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_T42_T81))
            s->tiff.compression = T4_COMPRESSION_T42_T81;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_T43))
            s->tiff.compression = T4_COMPRESSION_T43;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_T45))
            s->tiff.compression = T4_COMPRESSION_T45;
        else if ((s->supported_tiff_compressions & T4_COMPRESSION_UNCOMPRESSED))
            s->tiff.compression = T4_COMPRESSION_UNCOMPRESSED;
    }
    return true;
}
/*- End of function --------------------------------------------------------*/

static int release_current_decoder(t4_rx_state_t *s)
{
    switch (s->current_decoder)
    {
    case 0:
        return pre_encoded_release(&s->decoder.no_decoder);
    case T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6:
        return t4_t6_decode_release(&s->decoder.t4_t6);
    case T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0:
        return t85_decode_release(&s->decoder.t85);
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        return t88_decode_release(&s->decoder.t88);
#endif
    case T4_COMPRESSION_T42_T81:
        return t42_decode_release(&s->decoder.t42);
    case T4_COMPRESSION_T43:
        return t43_decode_release(&s->decoder.t43);
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        return t45_decode_release(&s->decoder.t45);
#endif
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_set_rx_encoding(t4_rx_state_t *s, int compression)
{
    switch (compression)
    {
    case T4_COMPRESSION_T4_1D:
    case T4_COMPRESSION_T4_2D:
    case T4_COMPRESSION_T6:
        switch (s->metadata.compression)
        {
        case T4_COMPRESSION_T4_1D:
        case T4_COMPRESSION_T4_2D:
        case T4_COMPRESSION_T6:
            break;
        default:
            release_current_decoder(s);
            t4_t6_decode_init(&s->decoder.t4_t6, compression, s->metadata.image_width, s->row_handler, s->row_handler_user_data);
            s->current_decoder = T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6;
            break;
        }
        s->metadata.compression = compression;
        if (!select_tiff_compression(s, T4_IMAGE_TYPE_BILEVEL))
        {
            release_current_decoder(s);
            s->current_decoder = 0;
            pre_encoded_init(&s->decoder.no_decoder);
        }
        return t4_t6_decode_set_encoding(&s->decoder.t4_t6, compression);
    case T4_COMPRESSION_T85:
    case T4_COMPRESSION_T85_L0:
        switch (s->metadata.compression)
        {
        case T4_COMPRESSION_T85:
        case T4_COMPRESSION_T85_L0:
            break;
        default:
            release_current_decoder(s);
            t85_decode_init(&s->decoder.t85, s->row_handler, s->row_handler_user_data);
            s->current_decoder = T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0;
            /* Constrain received images to the maximum width of any FAX. This will
               avoid one potential cause of trouble, where a bad received image has
               a gigantic dimension that sucks our memory dry. */
            t85_decode_set_image_size_constraints(&s->decoder.t85, T4_WIDTH_1200_A3, 0);
            break;
        }
        s->metadata.compression = compression;
        if (!select_tiff_compression(s, T4_IMAGE_TYPE_BILEVEL))
        {
            release_current_decoder(s);
            s->current_decoder = 0;
            pre_encoded_init(&s->decoder.no_decoder);
        }
        return 0;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        switch (s->metadata.compression)
        {
        case T4_COMPRESSION_T88:
            break;
        default:
            release_current_decoder(s);
            t88_decode_init(&s->decoder.t88, s->row_handler, s->row_handler_user_data);
            s->current_decoder = T4_COMPRESSION_T88;
            break;
        }
        s->metadata.compression = compression;
        if (!select_tiff_compression(s, T4_IMAGE_TYPE_BILEVEL))
        {
            release_current_decoder(s);
            s->current_decoder = 0;
            pre_encoded_init(&s->decoder.no_decoder);
        }
        return 0;
#endif
    case T4_COMPRESSION_T42_T81:
    case T4_COMPRESSION_SYCC_T81:
        switch (s->metadata.compression)
        {
        case T4_COMPRESSION_T42_T81:
        case T4_COMPRESSION_SYCC_T81:
            break;
        default:
            release_current_decoder(s);
            t42_decode_init(&s->decoder.t42, s->row_handler, s->row_handler_user_data);
            s->current_decoder = T4_COMPRESSION_T42_T81;
            /* Constrain received images to the maximum width of any FAX. This will
               avoid one potential cause of trouble, where a bad received image has
               a gigantic dimension that sucks our memory dry. */
            t42_decode_set_image_size_constraints(&s->decoder.t42, T4_WIDTH_1200_A3, 0);
            break;
        }
        s->metadata.compression = compression;
        if (!select_tiff_compression(s, T4_IMAGE_TYPE_COLOUR_8BIT))
        {
            release_current_decoder(s);
            s->current_decoder = 0;
            pre_encoded_init(&s->decoder.no_decoder);
        }
        return 0;
    case T4_COMPRESSION_T43:
        switch (s->metadata.compression)
        {
        case T4_COMPRESSION_T43:
            break;
        default:
            release_current_decoder(s);
            t43_decode_init(&s->decoder.t43, s->row_handler, s->row_handler_user_data);
            s->current_decoder = T4_COMPRESSION_T43;
            /* Constrain received images to the maximum width of any FAX. This will
               avoid one potential cause of trouble, where a bad received image has
               a gigantic dimension that sucks our memory dry. */
            t43_decode_set_image_size_constraints(&s->decoder.t43, T4_WIDTH_1200_A3, 0);
            break;
        }
        s->metadata.compression = compression;
        if (!select_tiff_compression(s, T4_IMAGE_TYPE_COLOUR_8BIT))
        {
            release_current_decoder(s);
            s->current_decoder = 0;
            pre_encoded_init(&s->decoder.no_decoder);
        }
        return 0;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        switch (s->metadata.compression)
        {
        case T4_COMPRESSION_T45:
            break;
        default:
            release_current_decoder(s);
            t45_decode_init(&s->decoder.t45, s->row_handler, s->row_handler_user_data);
            s->current_decoder = T4_COMPRESSION_T45;
            break;
        }
        s->metadata.compression = compression;
        if (!select_tiff_compression(s, T4_IMAGE_TYPE_COLOUR_8BIT))
        {
            release_current_decoder(s);
            s->current_decoder = 0;
            pre_encoded_init(&s->decoder.no_decoder);
        }
        return 0;
#endif
    }

    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_image_width(t4_rx_state_t *s, int width)
{
    s->metadata.image_width = width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_set_row_write_handler(t4_rx_state_t *s, t4_row_write_handler_t handler, void *user_data)
{
    s->row_handler = handler;
    s->row_handler_user_data = user_data;
    switch (s->current_decoder)
    {
    case T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6:
        return t4_t6_decode_set_row_write_handler(&s->decoder.t4_t6, handler, user_data);
    case T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0:
        return t85_decode_set_row_write_handler(&s->decoder.t85, handler, user_data);
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        return t88_decode_set_row_write_handler(&s->decoder.t88, handler, user_data);
#endif
    case T4_COMPRESSION_T42_T81:
        return t42_decode_set_row_write_handler(&s->decoder.t42, handler, user_data);
    case T4_COMPRESSION_T43:
        return t43_decode_set_row_write_handler(&s->decoder.t43, handler, user_data);
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        return t45_decode_set_row_write_handler(&s->decoder.t45, handler, user_data);
#endif
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_get_transfer_statistics(t4_rx_state_t *s, t4_stats_t *t)
{
    memset(t, 0, sizeof(*t));
    t->pages_transferred = s->current_page;
    t->pages_in_file = s->tiff.pages_in_file;

    t->image_x_resolution = s->metadata.x_resolution;
    t->image_y_resolution = s->metadata.y_resolution;
    t->x_resolution = s->metadata.x_resolution;
    t->y_resolution = s->metadata.y_resolution;

    t->compression = s->metadata.compression;
    switch (s->current_decoder)
    {
    case 0:
        t->type = 0;
        t->width = s->metadata.image_width;
        t->length = s->metadata.image_length;
        t->image_type = 0;
        t->image_width = t->width;
        t->image_length = t->length;
        t->line_image_size = s->line_image_size;
        break;
    case T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6:
        t->type = T4_IMAGE_TYPE_BILEVEL;
        t->width = t4_t6_decode_get_image_width(&s->decoder.t4_t6);
        t->length = t4_t6_decode_get_image_length(&s->decoder.t4_t6);
        t->image_type = t->type;
        t->image_width = t->width;
        t->image_length = t->length;
        t->line_image_size = t4_t6_decode_get_compressed_image_size(&s->decoder.t4_t6)/8;
        t->bad_rows = s->decoder.t4_t6.bad_rows;
        t->longest_bad_row_run = s->decoder.t4_t6.longest_bad_row_run;
        break;
    case T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0:
        t->type = T4_IMAGE_TYPE_BILEVEL;
        t->width = t85_decode_get_image_width(&s->decoder.t85);
        t->length = t85_decode_get_image_length(&s->decoder.t85);
        t->image_type = t->type;
        t->image_width = t->width;
        t->image_length = t->length;
        t->line_image_size = t85_decode_get_compressed_image_size(&s->decoder.t85)/8;
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        break;
#endif
    case T4_COMPRESSION_T42_T81:
        t->type = T4_IMAGE_TYPE_COLOUR_8BIT; //T4_IMAGE_TYPE_GRAY_8BIT;
        t->width = t42_decode_get_image_width(&s->decoder.t42);
        t->length = t42_decode_get_image_length(&s->decoder.t42);
        t->image_type = t->type;
        t->image_width = t->width;
        t->image_length = t->length;
        t->line_image_size = t42_decode_get_compressed_image_size(&s->decoder.t42)/8;
        break;
    case T4_COMPRESSION_T43:
        t->type = T4_IMAGE_TYPE_COLOUR_8BIT;
        t->width = t43_decode_get_image_width(&s->decoder.t43);
        t->length = t43_decode_get_image_length(&s->decoder.t43);
        t->image_type = t->type;
        t->image_width = t->width;
        t->image_length = t->length;
        t->line_image_size = t43_decode_get_compressed_image_size(&s->decoder.t43)/8;
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        break;
#endif
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_start_page(t4_rx_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start rx page %d - compression %s\n", s->current_page, t4_compression_to_str(s->metadata.compression));

    switch (s->current_decoder)
    {
    case 0:
        pre_encoded_restart(&s->decoder.no_decoder);
        s->image_put_handler = (t4_image_put_handler_t) pre_encoded_put;
        break;
    case T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6:
        t4_t6_decode_restart(&s->decoder.t4_t6, s->metadata.image_width);
        s->image_put_handler = (t4_image_put_handler_t) t4_t6_decode_put;
        break;
    case T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0:
        t85_decode_restart(&s->decoder.t85);
        s->image_put_handler = (t4_image_put_handler_t) t85_decode_put;
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        t88_decode_restart(&s->decoder.t88);
        s->image_put_handler = (t4_image_put_handler_t) t88_decode_put;
        break;
#endif
    case T4_COMPRESSION_T42_T81:
        t42_decode_restart(&s->decoder.t42);
        s->image_put_handler = (t4_image_put_handler_t) t42_decode_put;
        break;
    case T4_COMPRESSION_T43:
        t43_decode_restart(&s->decoder.t43);
        s->image_put_handler = (t4_image_put_handler_t) t43_decode_put;
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        t45_decode_restart(&s->decoder.t45);
        s->image_put_handler = (t4_image_put_handler_t) t45_decode_put;
        break;
#endif
    }
    s->line_image_size = 0;
    s->tiff.image_size = 0;

    time (&s->tiff.page_start_time);

    return 0;
}
/*- End of function --------------------------------------------------------*/

static int tiff_row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    t4_rx_state_t *s;
    uint8_t *t;

    s = (t4_rx_state_t *) user_data;
    if (buf  &&  len > 0)
    {
        if (s->tiff.image_size + len >= s->tiff.image_buffer_size)
        {
            if ((t = span_realloc(s->tiff.image_buffer, s->tiff.image_buffer_size + 100*len)) == NULL)
                return -1;
            s->tiff.image_buffer_size += 100*len;
            s->tiff.image_buffer = t;
        }
        memcpy(&s->tiff.image_buffer[s->tiff.image_size], buf, len);
        s->tiff.image_size += len;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_end_page(t4_rx_state_t *s)
{
    int length;

    length = 0;

    if (s->image_put_handler)
        s->image_put_handler((void *) &s->decoder, NULL, 0);

    switch (s->current_decoder)
    {
    case 0:
        length = s->decoder.no_decoder.buf_ptr;
        break;
    case T4_COMPRESSION_T4_1D | T4_COMPRESSION_T4_2D | T4_COMPRESSION_T6:
        length = t4_t6_decode_get_image_length(&s->decoder.t4_t6);
        break;
    case T4_COMPRESSION_T85 | T4_COMPRESSION_T85_L0:
        length = t85_decode_get_image_length(&s->decoder.t85);
        break;
#if defined(SPANDSP_SUPPORT_T88)
    case T4_COMPRESSION_T88:
        length = t88_decode_get_image_length(&s->decoder.t88);
        break;
#endif
    case T4_COMPRESSION_T42_T81:
        length = t42_decode_get_image_length(&s->decoder.t42);
        if (s->decoder.t42.samples_per_pixel == 3)
            s->tiff.image_type = T4_IMAGE_TYPE_COLOUR_8BIT;
        else
            s->tiff.image_type = T4_IMAGE_TYPE_GRAY_8BIT;
        break;
    case T4_COMPRESSION_T43:
        length = t43_decode_get_image_length(&s->decoder.t43);
        break;
#if defined(SPANDSP_SUPPORT_T45)
    case T4_COMPRESSION_T45:
        length = t45_decode_get_image_length(&s->decoder.t45);
        break;
#endif
    }

    if (length == 0)
        return -1;

    if (s->tiff.tiff_file)
    {
        if (write_tiff_image(s) == 0)
            s->current_page++;
        s->tiff.image_size = 0;
    }
    else
    {
        s->current_page++;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t4_rx_get_logging_state(t4_rx_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t4_rx_state_t *) t4_rx_init(t4_rx_state_t *s, const char *file, int supported_output_compressions)
{
    bool alloced;

    alloced = false;
    if (s == NULL)
    {
        if ((s = (t4_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
        alloced = true;
    }
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    TIFF_FX_init();
#endif
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.4");

    span_log(&s->logging, SPAN_LOG_FLOW, "Start rx document\n");

    s->supported_tiff_compressions = supported_output_compressions;
#if !defined(SPANDSP_SUPPORT_T88)
    s->supported_tiff_compressions &= ~T4_COMPRESSION_T88;
#endif
#if !defined(SPANDSP_SUPPORT_T43)
    s->supported_tiff_compressions &= ~T4_COMPRESSION_T43;
#endif
#if !defined(SPANDSP_SUPPORT_T45)
    s->supported_tiff_compressions &= ~T4_COMPRESSION_T45;
#endif

    /* Set some default values */
    s->metadata.x_resolution = T4_X_RESOLUTION_R8;
    s->metadata.y_resolution = T4_Y_RESOLUTION_FINE;

    s->current_page = 0;
    s->current_decoder = 0;

    /* Default handler */
    s->row_handler = tiff_row_write_handler;
    s->row_handler_user_data = s;

    if (file)
    {
        s->tiff.pages_in_file = 0;
        if (open_tiff_output_file(s, file) < 0)
        {
            if (alloced)
                span_free(s);
            return NULL;
        }
        /* Save the file name for logging reports. */
        s->tiff.file = strdup(file);
    }
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_release(t4_rx_state_t *s)
{
    if (s->tiff.file)
        tiff_rx_release(s);
    release_current_decoder(s);
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_free(t4_rx_state_t *s)
{
    int ret;

    ret = t4_rx_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

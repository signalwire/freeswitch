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
#include "floating_fudge.h"
#include <tiffio.h>

#include "spandsp/telephony.h"
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
#if defined(SPANDSP_SUPPORT_T43)
#include "spandsp/t43.h"
#endif
#include "spandsp/t4_t6_decode.h"
#include "spandsp/t4_t6_encode.h"
#include "spandsp/version.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"
#if defined(SPANDSP_SUPPORT_T43)
#include "spandsp/private/t43.h"
#endif
#include "spandsp/private/t4_t6_decode.h"
#include "spandsp/private/t4_t6_encode.h"
#include "spandsp/private/image_translate.h"
#include "spandsp/private/t4_rx.h"
#include "spandsp/private/t4_tx.h"

/*! The number of centimetres in one inch */
#define CM_PER_INCH                 2.54f

#if defined(SPANDSP_SUPPORT_TIFF_FX)
extern TIFFFieldArray tiff_fx_field_array;
#endif

SPAN_DECLARE(const char *) t4_encoding_to_str(int encoding)
{
    switch (encoding)
    {
    case T4_COMPRESSION_NONE:
        return "None";
    case T4_COMPRESSION_ITU_T4_1D:
        return "T.4 1-D";
    case T4_COMPRESSION_ITU_T4_2D:
        return "T.4 2-D";
    case T4_COMPRESSION_ITU_T6:
        return "T.6";
    case T4_COMPRESSION_ITU_T42:
        return "T.42";
    case T4_COMPRESSION_ITU_SYCC_T42:
        return "sYCC T.42";
    case T4_COMPRESSION_ITU_T43:
        return "T.43";
    case T4_COMPRESSION_ITU_T45:
        return "T.45";
    case T4_COMPRESSION_ITU_T85:
        return "T.85";
    case T4_COMPRESSION_ITU_T85_L0:
        return "T.85(L0)";
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

    t = &s->tiff;
    /* Prepare the directory entry fully before writing the image, or libtiff complains */
    switch (t->output_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    default:
        output_compression = COMPRESSION_CCITT_T4;
        output_t4_options = GROUP3OPT_FILLBITS;
        break;
    case T4_COMPRESSION_ITU_T4_2D:
        output_compression = COMPRESSION_CCITT_T4;
        output_t4_options = GROUP3OPT_FILLBITS | GROUP3OPT_2DENCODING;
        break;
    case T4_COMPRESSION_ITU_T6:
        output_compression = COMPRESSION_CCITT_T6;
        break;
    case T4_COMPRESSION_ITU_T85:
        output_compression = COMPRESSION_T85;
        break;
    }

    TIFFSetField(t->tiff_file, TIFFTAG_COMPRESSION, output_compression);
    switch (output_compression)
    {
    case COMPRESSION_CCITT_T4:
        TIFFSetField(t->tiff_file, TIFFTAG_T4OPTIONS, output_t4_options);
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        TIFFSetField(t->tiff_file, TIFFTAG_ROWSPERSTRIP, -1L);
        break;
    case COMPRESSION_CCITT_T6:
        TIFFSetField(t->tiff_file, TIFFTAG_T6OPTIONS, 0);
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        TIFFSetField(t->tiff_file, TIFFTAG_ROWSPERSTRIP, -1L);
        break;
    case COMPRESSION_T85:
        TIFFSetField(t->tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        TIFFSetField(t->tiff_file, TIFFTAG_ROWSPERSTRIP, -1L);
        break;
    default:
        TIFFSetField(t->tiff_file,
                     TIFFTAG_ROWSPERSTRIP,
                     TIFFDefaultStripSize(t->tiff_file, 0));
        break;
    }
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    TIFFSetField(t->tiff_file, TIFFTAG_PROFILETYPE, PROFILETYPE_G3_FAX);
    TIFFSetField(t->tiff_file, TIFFTAG_FAXPROFILE, FAXPROFILE_F);
    TIFFSetField(t->tiff_file, TIFFTAG_CODINGMETHODS, CODINGMETHODS_T4_1D | CODINGMETHODS_T4_2D | CODINGMETHODS_T6);
    TIFFSetField(t->tiff_file, TIFFTAG_VERSIONYEAR, "1998");
    /* TIFFSetField(t->tiff_file, TIFFTAG_MODENUMBER, 0); */
#endif
    TIFFSetField(t->tiff_file, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(t->tiff_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(t->tiff_file, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(t->tiff_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(t->tiff_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(t->tiff_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
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

    TIFFSetField(t->tiff_file, TIFFTAG_IMAGEWIDTH, s->image_width);
    /* Set the total pages to 1. For any one page document we will get this
       right. For multi-page documents we will need to come back and fill in
       the right answer when we know it. */
    TIFFSetField(t->tiff_file, TIFFTAG_PAGENUMBER, s->current_page, 1);
    /* TIFF page numbers start from zero, so the number of pages in the file
       is always one greater than the highest page number in the file. */
    s->tiff.pages_in_file = s->current_page + 1;
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
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
        /* Fall through */
    case T4_COMPRESSION_ITU_T6:
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGELENGTH, t4_t6_decode_get_image_length(&s->decoder.t4_t6));
        break;
    case T4_COMPRESSION_ITU_T42:
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGELENGTH, t42_decode_get_image_length(&s->decoder.t42));
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGELENGTH, t43_decode_get_image_length(&s->decoder.t43));
        break;
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        TIFFSetField(t->tiff_file, TIFFTAG_IMAGELENGTH, t85_decode_get_image_length(&s->decoder.t85));
        break;
    }
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    if (s->current_page == 0)
    {
        /* Create a placeholder for the global parameters IFD, to be filled in later */
        TIFFSetField(t->tiff_file, TIFFTAG_GLOBALPARAMETERSIFD, 0);
    }
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

static int write_tiff_image(t4_rx_state_t *s)
{
    t4_rx_tiff_state_t *t;
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    uint64_t offset;
#endif

    t = &s->tiff;
    if (t->image_buffer == NULL  ||  t->image_size <= 0)
        return -1;
    /* Set up the TIFF directory info... */
    set_tiff_directory_info(s);
    /* ...Put the directory in the file before the image data, to get them in the order specified
       for TIFF/F files... */
    if (!TIFFCheckpointDirectory(t->tiff_file))
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to checkpoint directory for page %d.\n", t->file, s->current_page);
    /* ...and write out the image... */
    if (TIFFWriteEncodedStrip(t->tiff_file, 0, t->image_buffer, t->image_size) < 0)
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Error writing TIFF strip.\n", t->file);
    /* ...then finalise the directory entry, and libtiff is happy. */
    if (!TIFFWriteDirectory(t->tiff_file))
        span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to write directory for page %d.\n", t->file, s->current_page);
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    if (s->current_page == 0)
    {
        if (!TIFFCreateCustomDirectory(t->tiff_file, &tiff_fx_field_array))
        {
            TIFFSetField(t->tiff_file, TIFFTAG_FAXPROFILE, PROFILETYPE_G3_FAX);
            TIFFSetField(t->tiff_file, TIFFTAG_PROFILETYPE, FAXPROFILE_F);
            TIFFSetField(t->tiff_file, TIFFTAG_VERSIONYEAR, "1998");

            offset = 0;
            if (!TIFFWriteCustomDirectory(t->tiff_file, &offset))
                printf("Failed to write custom directory.\n");

            /* Now go back and patch in the pointer to the new IFD */
            if (!TIFFSetDirectory(t->tiff_file, s->current_page))
                printf("Failed to set directory.\n");
            if (!TIFFSetField(t->tiff_file, TIFFTAG_GLOBALPARAMETERSIFD, offset))
                printf("Failed to set field.\n");
            if (!TIFFWriteDirectory(t->tiff_file))
                span_log(&s->logging, SPAN_LOG_WARNING, "%s: Failed to write directory for page %d.\n", t->file, s->current_page);
        }
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
            remove(s->tiff.file);
        free((char *) s->tiff.file);
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
        free(s->tiff.image_buffer);
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

SPAN_DECLARE(int) t4_rx_put(t4_rx_state_t *s, const uint8_t buf[], size_t len)
{
    s->line_image_size += 8*len;
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        return t4_t6_decode_put(&s->decoder.t4_t6, buf, len);
    case T4_COMPRESSION_ITU_T42:
        return t42_decode_put(&s->decoder.t42, buf, len);
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        return t43_decode_put(&s->decoder.t43, buf, len);
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        return t85_decode_put(&s->decoder.t85, buf, len);
    }
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

SPAN_DECLARE(int) t4_rx_set_rx_encoding(t4_rx_state_t *s, int encoding)
{
    switch (encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        switch (s->line_encoding)
        {
        case T4_COMPRESSION_ITU_T4_1D:
        case T4_COMPRESSION_ITU_T4_2D:
        case T4_COMPRESSION_ITU_T6:
            break;
        default:
            t4_t6_decode_init(&s->decoder.t4_t6, encoding, s->image_width, s->row_handler, s->row_handler_user_data);
            break;
        }
        s->line_encoding = encoding;
        return t4_t6_decode_set_encoding(&s->decoder.t4_t6, encoding);
    case T4_COMPRESSION_ITU_T42:
        switch (s->line_encoding)
        {
        case T4_COMPRESSION_ITU_T42:
            break;
        default:
            t42_decode_init(&s->decoder.t42, s->row_handler, s->row_handler_user_data);
            /* Constrain received images to the maximum width of any FAX. This will
               avoid one potential cause of trouble, where a bad received image has
               a gigantic dimension that sucks our memory dry. */
            t42_decode_set_image_size_constraints(&s->decoder.t42, T4_WIDTH_1200_A3, 0);
            break;
        }
        s->line_encoding = encoding;
        return 0;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        switch (s->line_encoding)
        {
        case T4_COMPRESSION_ITU_T43:
            break;
        default:
            t43_decode_init(&s->decoder.t43, s->row_handler, s->row_handler_user_data);
            /* Constrain received images to the maximum width of any FAX. This will
               avoid one potential cause of trouble, where a bad received image has
               a gigantic dimension that sucks our memory dry. */
            t43_decode_set_image_size_constraints(&s->decoder.t43, T4_WIDTH_1200_A3, 0);
            break;
        }
        s->line_encoding = encoding;
        return 0;
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        switch (s->line_encoding)
        {
        case T4_COMPRESSION_ITU_T85:
        case T4_COMPRESSION_ITU_T85_L0:
            break;
        default:
            t85_decode_init(&s->decoder.t85, s->row_handler, s->row_handler_user_data);
            /* Constrain received images to the maximum width of any FAX. This will
               avoid one potential cause of trouble, where a bad received image has
               a gigantic dimension that sucks our memory dry. */
            t85_decode_set_image_size_constraints(&s->decoder.t85, T4_WIDTH_1200_A3, 0);
            break;
        }
        s->line_encoding = encoding;
        return 0;
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_set_image_width(t4_rx_state_t *s, int width)
{
    s->image_width = width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_set_row_write_handler(t4_rx_state_t *s, t4_row_write_handler_t handler, void *user_data)
{
    s->row_handler = handler;
    s->row_handler_user_data = user_data;
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        return t4_t6_decode_set_row_write_handler(&s->decoder.t4_t6, handler, user_data);
    case T4_COMPRESSION_ITU_T42:
        return t42_decode_set_row_write_handler(&s->decoder.t42, handler, user_data);
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        return t43_decode_set_row_write_handler(&s->decoder.t43, handler, user_data);
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        return t85_decode_set_row_write_handler(&s->decoder.t85, handler, user_data);
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t4_rx_get_transfer_statistics(t4_rx_state_t *s, t4_stats_t *t)
{
    memset(t, 0, sizeof(*t));
    t->pages_transferred = s->current_page;
    t->pages_in_file = s->tiff.pages_in_file;
    t->x_resolution = s->metadata.x_resolution;
    t->y_resolution = s->metadata.y_resolution;
    t->encoding = s->line_encoding;
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        t->width = t4_t6_decode_get_image_width(&s->decoder.t4_t6);
        t->length = t4_t6_decode_get_image_length(&s->decoder.t4_t6);
        t->line_image_size = t4_t6_decode_get_compressed_image_size(&s->decoder.t4_t6)/8;
        t->bad_rows = s->decoder.t4_t6.bad_rows;
        t->longest_bad_row_run = s->decoder.t4_t6.longest_bad_row_run;
        break;
    case T4_COMPRESSION_ITU_T42:
        t->width = t42_decode_get_image_width(&s->decoder.t42);
        t->length = t42_decode_get_image_length(&s->decoder.t42);
        t->line_image_size = t42_decode_get_compressed_image_size(&s->decoder.t42)/8;
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        t->width = t43_decode_get_image_width(&s->decoder.t43);
        t->length = t43_decode_get_image_length(&s->decoder.t43);
        t->line_image_size = t43_decode_get_compressed_image_size(&s->decoder.t43)/8;
        break;
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        t->width = t85_decode_get_image_width(&s->decoder.t85);
        t->length = t85_decode_get_image_length(&s->decoder.t85);
        t->line_image_size = t85_decode_get_compressed_image_size(&s->decoder.t85)/8;
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_start_page(t4_rx_state_t *s)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Start rx page %d - compression %s\n", s->current_page, t4_encoding_to_str(s->line_encoding));

    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        t4_t6_decode_restart(&s->decoder.t4_t6, s->image_width);
        break;
    case T4_COMPRESSION_ITU_T42:
        t42_decode_restart(&s->decoder.t42);
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        t43_decode_restart(&s->decoder.t43);
        break;
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        t85_decode_restart(&s->decoder.t85);
        break;
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
            if ((t = realloc(s->tiff.image_buffer, s->tiff.image_buffer_size + 100*len)) == NULL)
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
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        t4_t6_decode_put(&s->decoder.t4_t6, NULL, 0);
        length = t4_t6_decode_get_image_length(&s->decoder.t4_t6);
        break;
    case T4_COMPRESSION_ITU_T42:
        t42_decode_put(&s->decoder.t42, NULL, 0);
        length = t42_decode_get_image_length(&s->decoder.t42);
        break;
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        t43_decode_put(&s->decoder.t43, NULL, 0);
        length = t43_decode_get_image_length(&s->decoder.t43);
        break;
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        t85_decode_put(&s->decoder.t85, NULL, 0);
        length = t85_decode_get_image_length(&s->decoder.t85);
        break;
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

SPAN_DECLARE(t4_rx_state_t *) t4_rx_init(t4_rx_state_t *s, const char *file, int output_encoding)
{
    int allocated;

    allocated = FALSE;
    if (s == NULL)
    {
        if ((s = (t4_rx_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
        allocated = TRUE;
    }
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    TIFF_FX_init();
#endif
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.4");

    span_log(&s->logging, SPAN_LOG_FLOW, "Start rx document\n");

    /* Only provide for one form of coding throughout the file, even though the
       coding on the wire could change between pages. */
    s->tiff.output_encoding = output_encoding;

    /* Set some default values */
    s->metadata.x_resolution = T4_X_RESOLUTION_R8;
    s->metadata.y_resolution = T4_Y_RESOLUTION_FINE;

    s->current_page = 0;

    /* Default handler */
    s->row_handler = tiff_row_write_handler;
    s->row_handler_user_data = s;

    if (file)
    {
        s->tiff.pages_in_file = 0;
        if (open_tiff_output_file(s, file) < 0)
        {
            if (allocated)
                free(s);
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
    switch (s->line_encoding)
    {
    case T4_COMPRESSION_ITU_T4_1D:
    case T4_COMPRESSION_ITU_T4_2D:
    case T4_COMPRESSION_ITU_T6:
        return t4_t6_decode_release(&s->decoder.t4_t6);
    case T4_COMPRESSION_ITU_T42:
        return t42_decode_release(&s->decoder.t42);
#if defined(SPANDSP_SUPPORT_T43)
    case T4_COMPRESSION_ITU_T43:
        return t43_decode_release(&s->decoder.t43);
#endif
    case T4_COMPRESSION_ITU_T85:
    case T4_COMPRESSION_ITU_T85_L0:
        return t85_decode_release(&s->decoder.t85);
    }
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t4_rx_free(t4_rx_state_t *s)
{
    int ret;

    ret = t4_rx_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

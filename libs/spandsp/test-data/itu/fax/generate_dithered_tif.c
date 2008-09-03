/*
 * SpanDSP - a series of DSP components for telephony
 *
 * generate_dithered_tif.c - Create a fine checkerboard TIFF file for test purposes.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: generate_dithered_tif.c,v 1.2 2008/07/10 13:34:01 steveu Exp $
 */

/*! \file */

/*
    This program generates an A4 sized FAX image of a fine checkerboard. This doesn't
    compress well, so it results in a rather large file for a single page. This is
    good for testing the handling of extreme pages.
    
    Note that due to a bug in FAX image handling, versions of libtiff up to 3.8.2 fail
    to handle this complex image properly, if 2-D compression is used. The bug should
    be fixed in CVS at the time of writing, and so should be fixed in released versions
    after 3.8.2. This code uses 1-D compression to avoid the issue.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <memory.h>
#include <string.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include <tiffio.h>

#include "spandsp.h"

int main(int argc, char *argv[])
{
    int image_width;
    int row;
    int resunit;
    int output_compression;
    int output_t4_options;
    uint8_t image_buffer[1024];
    TIFF *tiff_file;
    struct tm *tm;
    time_t now;
    char buf[133];
    float x_resolution;
    float y_resolution;
    int x_res;
    int y_res;
    int image_length;

    if ((tiff_file = TIFFOpen("dithered.tif", "w")) == NULL)
        exit(2);

    output_compression = COMPRESSION_CCITT_T4;
    /* Use 1-D compression until a fixed libtiff is the norm. */
    //output_t4_options = GROUP3OPT_FILLBITS | GROUP3OPT_2DENCODING;
    output_t4_options = GROUP3OPT_FILLBITS;

    x_res = T4_X_RESOLUTION_R8;
    y_res = T4_Y_RESOLUTION_FINE;
    image_width = 1728;
    image_length = 2200;

    /* Prepare the directory entry fully before writing the image, or libtiff complains */
    TIFFSetField(tiff_file, TIFFTAG_COMPRESSION, output_compression);
    if (output_compression == COMPRESSION_CCITT_T4)
    {
        TIFFSetField(tiff_file, TIFFTAG_T4OPTIONS, output_t4_options);
        TIFFSetField(tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
    }
    TIFFSetField(tiff_file, TIFFTAG_IMAGEWIDTH, image_width);
    TIFFSetField(tiff_file, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(tiff_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiff_file, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tiff_file, TIFFTAG_ROWSPERSTRIP, -1L);
    TIFFSetField(tiff_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(tiff_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);

    x_resolution = x_res/100.0f;
    y_resolution = y_res/100.0f;
    TIFFSetField(tiff_file, TIFFTAG_XRESOLUTION, floorf(x_resolution*2.54f + 0.5f));
    TIFFSetField(tiff_file, TIFFTAG_YRESOLUTION, floorf(y_resolution*2.54f + 0.5f));
    resunit = RESUNIT_INCH;
    TIFFSetField(tiff_file, TIFFTAG_RESOLUTIONUNIT, resunit);

    TIFFSetField(tiff_file, TIFFTAG_SOFTWARE, "spandsp");
    if (gethostname(buf, sizeof(buf)) == 0)
        TIFFSetField(tiff_file, TIFFTAG_HOSTCOMPUTER, buf);

    TIFFSetField(tiff_file, TIFFTAG_IMAGEDESCRIPTION, "Checkerboard or dithered ones");
    TIFFSetField(tiff_file, TIFFTAG_MAKE, "soft-switch.org");
    TIFFSetField(tiff_file, TIFFTAG_MODEL, "test data");

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
    TIFFSetField(tiff_file, TIFFTAG_DATETIME, buf);

    TIFFSetField(tiff_file, TIFFTAG_IMAGELENGTH, image_length);
    TIFFSetField(tiff_file, TIFFTAG_PAGENUMBER, 0, 1);
    TIFFSetField(tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_CLEAN);
    TIFFSetField(tiff_file, TIFFTAG_IMAGEWIDTH, image_width);

    /* Write the image first.... */
    for (row = 0;  row < image_length;  row++)
    {
        if ((row & 1) == 0)
            memset(image_buffer, 0xAA, image_width/8 + 1);
        else
            memset(image_buffer, 0x55, image_width/8 + 1);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }
    /* ....then the directory entry, and libtiff is happy. */
    TIFFWriteDirectory(tiff_file);
    TIFFClose(tiff_file);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

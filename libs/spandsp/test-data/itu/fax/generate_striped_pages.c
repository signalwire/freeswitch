/*
 * SpanDSP - a series of DSP components for telephony
 *
 * generate_striped_pages.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2010 Steve Underwood
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
 */

/*! \file */

/*
    This program generates an TIFF image as a number of small image striped, rather than
    the usual all in one page FAX images usually consist of in TIFF files.
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

#define IMAGE_WIDTH         1728
#define IMAGE_LENGTH        2600
#define ROWS_PER_STRIPE     37

int main(int argc, char *argv[])
{
    TIFF *tiff_file;
    uint8_t image_buffer[10000];
    int image_size;
    time_t now;
    struct tm *tm;
    char buf[256 + 1];
    int i;

    if ((tiff_file = TIFFOpen("striped.tif", "w")) == NULL)
        return -1;

    TIFFSetField(tiff_file, TIFFTAG_COMPRESSION, COMPRESSION_CCITT_T6);
    TIFFSetField(tiff_file, TIFFTAG_BITSPERSAMPLE, 1);
    TIFFSetField(tiff_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiff_file, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tiff_file, TIFFTAG_ROWSPERSTRIP, (int32_t) ROWS_PER_STRIPE);
    TIFFSetField(tiff_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff_file, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
    TIFFSetField(tiff_file, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
    TIFFSetField(tiff_file, TIFFTAG_XRESOLUTION, 204.0f);
    TIFFSetField(tiff_file, TIFFTAG_YRESOLUTION, 196.0f);
    TIFFSetField(tiff_file, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tiff_file, TIFFTAG_SOFTWARE, "Spandsp");
    TIFFSetField(tiff_file, TIFFTAG_HOSTCOMPUTER, "host");
    TIFFSetField(tiff_file, TIFFTAG_FAXSUBADDRESS, "1111");
    TIFFSetField(tiff_file, TIFFTAG_IMAGEDESCRIPTION, "Image in stripes");
    TIFFSetField(tiff_file, TIFFTAG_MAKE, "spandsp");
    TIFFSetField(tiff_file, TIFFTAG_MODEL, "testy");

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
    TIFFSetField(tiff_file, TIFFTAG_FAXRECVTIME, 10);
    TIFFSetField(tiff_file, TIFFTAG_IMAGEWIDTH, IMAGE_WIDTH);
    TIFFSetField(tiff_file, TIFFTAG_IMAGELENGTH, IMAGE_LENGTH);
    TIFFSetField(tiff_file, TIFFTAG_PAGENUMBER, 0, 1);
    TIFFCheckpointDirectory(tiff_file);

    image_size = IMAGE_WIDTH*ROWS_PER_STRIPE/8;
    memset(image_buffer, 0x18, image_size);

    for (i = 0;  i < IMAGE_LENGTH/ROWS_PER_STRIPE;  i++)
    {
        if (IMAGE_LENGTH > (i + 1)*ROWS_PER_STRIPE)
            image_size = IMAGE_WIDTH*ROWS_PER_STRIPE/8;
        else
            image_size = IMAGE_WIDTH*(IMAGE_LENGTH - i*ROWS_PER_STRIPE)/8;
        if (TIFFWriteEncodedStrip(tiff_file, i, image_buffer, image_size) < 0)
            return -1;
    }

    TIFFWriteDirectory(tiff_file);
    TIFFClose(tiff_file);
    return 0;
}

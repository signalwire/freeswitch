/*
 * SpanDSP - a series of DSP components for telephony
 *
 * generate_etsi_300_242_pages.c - Create the test pages defined in ETSI ETS 300 242.
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
 */

/*! \file */

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

#include "spandsp.h"

struct
{
    const char *name;
    int x_res;
    int y_res;
    int width;
    int length;
    int compression;
    int type;
} sequence[] =
{
    {
        "etsi_300_242_a4_diago1.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        0
    },
    {
        "etsi_300_242_a4_diago2.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        1
    },
    {
        "etsi_300_242_a4_duration1.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        2
    },
    {
        "etsi_300_242_a4_duration2.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        3
    },
    {
        "etsi_300_242_a4_error.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        4
    },
    {
        "etsi_300_242_a4_impress.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        5
    },
    {
        "etsi_300_242_a4_stairstep.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        6
    },
    {
        "etsi_300_242_a4_white.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        7
    },
    {
        "etsi_300_242_a4_white_2p.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        7
    },
    {   /* Second page of the above file */
        "",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        7
    },
    {
        "etsi_300_242_a4_impress_white.tif",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        5
    },
    {   /* Second page of the above file */
        "",
        T4_X_RESOLUTION_R8,
        T4_Y_RESOLUTION_STANDARD,
        T4_WIDTH_R8_A4,
        1100,
        COMPRESSION_CCITT_T4,
        7
    },
    {
        NULL,
        0,
        0,
        0,
        0
    },
};

int photo_metric = PHOTOMETRIC_MINISWHITE;
int fill_order = FILLORDER_LSB2MSB;

static void clear_row(uint8_t buf[], int width)
{
    memset(buf, 0, width/8 + 1);
}
/*- End of function --------------------------------------------------------*/

static void set_pixel(uint8_t buf[], int row, int pixel)
{
    row--;
    buf[row*1728/8 + pixel/8] |= (0x80 >> (pixel & 0x07));
}
/*- End of function --------------------------------------------------------*/

static void clear_pixel(uint8_t buf[], int row, int pixel)
{
    row--;
    buf[row*1728/8 + pixel/8] &= ~(0x80 >> (pixel & 0x07));
}
/*- End of function --------------------------------------------------------*/

static void set_pixel_range(uint8_t buf[], int row, int start, int end)
{
    int i;

    for (i = start;  i <= end;  i++)
        set_pixel(buf, row, i);
}
/*- End of function --------------------------------------------------------*/

static void clear_pixel_range(uint8_t buf[], int row, int start, int end)
{
    int i;
    
    for (i = start;  i <= end;  i++)
        clear_pixel(buf, row, i);
}
/*- End of function --------------------------------------------------------*/

static int create_white_page(TIFF *tiff_file)
{
    uint8_t image_buffer[8192];
    int row;

    /* TSB-85 WHITE page. */
    for (row = 0;  row < 1100;  row++)
    {
        clear_row(image_buffer, 1728);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }
    return 1100;
}
/*- End of function --------------------------------------------------------*/

static int create_stairstep_page(TIFF *tiff_file)
{
    uint8_t image_buffer[8192];
    int row;
    int start_pixel;
    int i;

    /* TSB-85 STAIRSTEP page. */
    start_pixel = 0;
    for (row = 0;  row < 1728;  row++)
    {
        clear_row(image_buffer, 1728);
        set_pixel_range(image_buffer, 1, start_pixel, start_pixel + 63);
        if (photo_metric != PHOTOMETRIC_MINISWHITE)
        {
            for (i = 0;  i < 1728/8;  i++)
                image_buffer[i] = ~image_buffer[i];
        }
#if 0
        if (fill_order != FILLORDER_LSB2MSB)
            bit_reverse(image_buffer, image_buffer, 1728/8);
#endif
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
        start_pixel += 64;
        if (start_pixel >= 1728)
            start_pixel = 0;
    }
    return 1728;
}
/*- End of function --------------------------------------------------------*/

static int create_diago1_page(TIFF *tiff_file)
{
    uint8_t image_buffer[1728/8 + 1];
    int row;

    /* ETSI ETS 300 242 B.5.1 One dimensional coding test chart - the DIAGO1 page. */
    for (row = 0;  row < 1001;  row++)
    {
        clear_row(image_buffer, 1728);
        set_pixel_range(image_buffer, 1, row, 1727);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }
    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }
    return 1002;
}
/*- End of function --------------------------------------------------------*/

static int create_diago2_page(TIFF *tiff_file)
{
    uint8_t image_buffer[1728/8 + 1];
    int row;

    /* ETSI ETS 300 242 B.5.1 One dimensional coding test chart - the DIAGO2 page. */
    for (row = 0;  row < 1001;  row++)
    {
        clear_row(image_buffer, 1728);
        set_pixel_range(image_buffer, 1, row + 728, 1727);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }
    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }
    return 1002;
}
/*- End of function --------------------------------------------------------*/

static int create_impress_page(TIFF *tiff_file)
{
    int j;
    int row;
    uint8_t *page;

    /* ETSI ETS 300 242 B.5.2 Printing resolution - the IMPRESS page */
    if ((page = malloc(1079*1728/8)) == NULL)
        return 0;
    memset(page, 0, 1079*1728/8);

    set_pixel_range(page, 1, 0, 1727);
    for (row = 2;  row <= 78;  row++)
    {
        set_pixel_range(page, row, 850, 850 + 27);
        set_pixel_range(page, row, 850 + 27 + 745, 850 + 27 + 745 + 26);
    }
    for (row = 80;  row <= 117;  row++)
    {
        for (j = 0;  j < 1728;  j += 2)
            set_pixel(page, row, j);
    }
    for (row = 118;  row <= 155;  row++)
    {
        for (j = 1;  j < 1728;  j += 2)
            set_pixel(page, row, j);
    }
    for (row = 194;  row <= 231;  row += 2)
        set_pixel_range(page, row, 0, 1727);
    for (row = 270;  row <= 276;  row++)
        set_pixel_range(page, row, 60, 60 + 1607);
    for (j = 0;  j < 1728;  j += 27)
        set_pixel(page, 315, j);
    for (row = 354;  row <= 480;  row++)
        set_pixel_range(page, row, 209, 768);
    for (row = 358;  row <= 476;  row++)
        clear_pixel_range(page, row, 488, 489);
    clear_pixel_range(page, 417, 217, 760);

    for (row = 354;  row <= 357;  row++)
        set_pixel_range(page, row, 962, 1521);
    for (row = 477;  row <= 480;  row++)
        set_pixel_range(page, row, 962, 1521);
    for (row = 358;  row <= 476;  row++)
        set_pixel_range(page, row, 962, 969);
    for (row = 358;  row <= 476;  row++)
        set_pixel_range(page, row, 1514, 1521);
    for (row = 358;  row <= 476;  row++)
        set_pixel(page, row, 1241);
    set_pixel_range(page, 417, 970, 1513);

    for (row = 354;  row <= 1079;  row++)
        set_pixel(page, row, 864);
    for (row = 157;  row <= 926;  row++)
        set_pixel_range(page, row, 884, 899);
    for (row = 0;  row < 1079;  row++)
    {
        if (TIFFWriteScanline(tiff_file, page + row*1728/8, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }
    free(page);
    return 1079;
}
/*- End of function --------------------------------------------------------*/

static int create_duration1_page(TIFF *tiff_file)
{
    uint8_t image_buffer[1728/8 + 1];
    int row;
    int i;

    /* ETSI ETS 300 242 B.5.3 Acceptance of total coded scan line duration - the DURATION1 page */
    row = 0;
    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }
    for (  ;  row < 117;  row++)
    {
        clear_row(image_buffer, 1728);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }
    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }
    clear_row(image_buffer, 1728);
    for (i = 1;  i < 1728;  i += 2)
        set_pixel(image_buffer, 1, i);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }
    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }
    for (  ;  row < 236;  row++)
    {
        clear_row(image_buffer, 1728);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }
    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }
    return 237;
}
/*- End of function --------------------------------------------------------*/

static int create_duration2_page(TIFF *tiff_file)
{
    return create_duration1_page(tiff_file);
}
/*- End of function --------------------------------------------------------*/

static int create_error_page(TIFF *tiff_file)
{
    uint8_t image_buffer[1728/8 + 1];
    int row;
    int start_pixel;
    int i;

    /* ETSI ETS 300 242 B.5.4 Copy quality criteria - the ERROR page. */
    for (row = 0;  row < 68;  row++)
    {
        clear_row(image_buffer, 1728);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }

    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }

    clear_row(image_buffer, 1728);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }

    for (i = 0;  i < 10;  i++)
    {
        for (start_pixel = 16;  start_pixel <= 1616;  start_pixel += 64)
        {
            clear_row(image_buffer, 1728);
            set_pixel_range(image_buffer, 1, start_pixel, start_pixel + 63);
            if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
            {
                printf("Write error at row %d.\n", row);
                exit(2);
            }
            row++;
        }
    }

    clear_row(image_buffer, 1728);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }

    clear_row(image_buffer, 1728);
    set_pixel_range(image_buffer, 1, 0, 1727);
    if (TIFFWriteScanline(tiff_file, image_buffer, row++, 0) < 0)
    {
        printf("Write error at row %d.\n", row);
        exit(2);
    }

    for (row = 332;  row < 400;  row++)
    {
        clear_row(image_buffer, 1728);
        if (TIFFWriteScanline(tiff_file, image_buffer, row, 0) < 0)
        {
            printf("Write error at row %d.\n", row);
            exit(2);
        }
    }

    return 400;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    TIFF *tiff_file;
    struct tm *tm;
    time_t now;
    char buf[133];
    float x_resolution;
    float y_resolution;
    int i;
    int image_length;
    int opt;

    photo_metric = PHOTOMETRIC_MINISWHITE;
    fill_order = FILLORDER_LSB2MSB;
    while ((opt = getopt(argc, argv, "ir")) != -1)
    {
        switch (opt)
        {
        case 'i':
            photo_metric = PHOTOMETRIC_MINISBLACK;
            break;
        case 'r':
            fill_order = FILLORDER_MSB2LSB;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    tiff_file = NULL;
    for (i = 0;  sequence[i].name;  i++)
    {
        if (sequence[i].name[0])
        {
            if (tiff_file)
                TIFFClose(tiff_file);
            if ((tiff_file = TIFFOpen(sequence[i].name, "w")) == NULL)
                exit(2);
        }
        /* Prepare the directory entry fully before writing the image, or libtiff complains */
        TIFFSetField(tiff_file, TIFFTAG_COMPRESSION, sequence[i].compression);
        if (sequence[i].compression == COMPRESSION_CCITT_T4)
        {
            TIFFSetField(tiff_file, TIFFTAG_T4OPTIONS, GROUP3OPT_FILLBITS); // | GROUP3OPT_2DENCODING);
            TIFFSetField(tiff_file, TIFFTAG_FAXMODE, FAXMODE_CLASSF);
        }
        TIFFSetField(tiff_file, TIFFTAG_IMAGEWIDTH, sequence[i].width);
        TIFFSetField(tiff_file, TIFFTAG_BITSPERSAMPLE, 1);
        TIFFSetField(tiff_file, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
        TIFFSetField(tiff_file, TIFFTAG_SAMPLESPERPIXEL, 1);
        TIFFSetField(tiff_file, TIFFTAG_ROWSPERSTRIP, -1L);
        TIFFSetField(tiff_file, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tiff_file, TIFFTAG_PHOTOMETRIC, photo_metric);
        TIFFSetField(tiff_file, TIFFTAG_FILLORDER, fill_order);
        x_resolution = sequence[i].x_res/100.0f;
        y_resolution = sequence[i].y_res/100.0f;
        TIFFSetField(tiff_file, TIFFTAG_XRESOLUTION, floorf(x_resolution*2.54f + 0.5f));
        TIFFSetField(tiff_file, TIFFTAG_YRESOLUTION, floorf(y_resolution*2.54f + 0.5f));
        TIFFSetField(tiff_file, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    
        TIFFSetField(tiff_file, TIFFTAG_SOFTWARE, "spandsp");
        if (gethostname(buf, sizeof(buf)) == 0)
            TIFFSetField(tiff_file, TIFFTAG_HOSTCOMPUTER, buf);
    
        TIFFSetField(tiff_file, TIFFTAG_IMAGEDESCRIPTION, "Blank test image");
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
        image_length = sequence[i].length;
        
        /* Write the image first.... */
        switch (sequence[i].type)
        {
        case 0:
            /* The DIAGO1 page */
            image_length = create_diago1_page(tiff_file);
            break;
        case 1:
            /* The DIAGO2 page */
            image_length = create_diago2_page(tiff_file);
            break;
        case 2:
            /* The DURATION1 page */
            image_length = create_duration1_page(tiff_file);
            break;
        case 3:
            /* The DURATION2 page */
            image_length = create_duration2_page(tiff_file);
            break;
        case 4:
            /* The ERROR page */
            image_length = create_error_page(tiff_file);
            break;
        case 5:
            /* The IMPRESS page */
            image_length = create_impress_page(tiff_file);
            break;
        case 6:
            /* A stairstep of 64 pixel dashes */
            image_length = create_stairstep_page(tiff_file);
            break;
        case 7:
            /* A white A4 page */
            image_length = create_white_page(tiff_file);
            break;
        }
        /* ....then the directory entry, and libtiff is happy. */
        TIFFSetField(tiff_file, TIFFTAG_IMAGELENGTH, image_length);
        TIFFSetField(tiff_file, TIFFTAG_PAGENUMBER, 0, 1);
        TIFFSetField(tiff_file, TIFFTAG_CLEANFAXDATA, CLEANFAXDATA_CLEAN);

        TIFFWriteDirectory(tiff_file);
    }
    if (tiff_file)
        TIFFClose(tiff_file);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

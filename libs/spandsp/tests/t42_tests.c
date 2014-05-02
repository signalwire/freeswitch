/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t42_tests.c - ITU T.42 JPEG for FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

/*! \page t42_tests_page T.42 tests
\section t42_tests_page_sec_1 What does it do
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <memory.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

#if defined(SPANDSP_SUPPORT_TIFF_FX)
#include <tif_dir.h>
#endif

#define IN_FILE_NAME    "../test-data/itu/t24/F21B400.TIF"
#define OUT_FILE_NAME   "t42_tests_receive.tif"

uint8_t data5[50000000];
int data5_ptr = 0;
int plane = 0;
int bit_mask;

uint8_t colour_map[3*256];

lab_params_t lab_param;

int write_row = 0;

static __inline__ uint16_t pack_16(uint8_t *s)
{
    uint16_t value;

    value = ((uint16_t) s[0] << 8) | (uint16_t) s[1];
    return value;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t pack_32(uint8_t *s)
{
    uint32_t value;

    value = ((uint32_t) s[0] << 24) | ((uint32_t) s[1] << 16) | ((uint32_t) s[2] << 8) | (uint32_t) s[3];
    return value;
}
/*- End of function --------------------------------------------------------*/

static int t85_row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    int i;
    int j;

    for (i = 0;  i < len;  i++)
    {
        for (j = 0;  j < 8;  j++)
        {
            if ((buf[i] & (0x80 >> j)))
                data5[data5_ptr + 3*(8*i + j)] |= bit_mask;
            else
                data5[data5_ptr + 3*(8*i + j)] &= ~bit_mask;
        }
    }
    data5_ptr += 3*8*len;
    write_row++;
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int t85_comment_handler(void *user_data, const uint8_t buf[], size_t len)
{
    if (buf)
        printf("Comment (%lu): %s\n", (unsigned long int) len, buf);
    else
        printf("Comment (%lu): ---\n", (unsigned long int) len);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    TIFF *tif;
    uint32_t w;
    uint32_t h;
    tstrip_t nstrips;
    uint32_t totdata;
    tsize_t off;
    uint8_t *data;
    uint8_t *data2;
    int row;
    uint16_t compression;
    int16_t photometric;
    int16_t YCbCrSubsampleHoriz;
    int16_t YCbCrSubsampleVert;
    int16_t bits_per_pixel;
    int16_t samples_per_pixel;
    int16_t planar_config;
    int bytes_per_row;
    tsize_t outsize;
    char *outptr;
    const char *source_file;
    int i;
    int j;
    int len;
    tsize_t total_image_len;
    tsize_t total_len;
    int process_raw;
    int result;
    t85_decode_state_t t85_dec;
    uint64_t start;
    uint64_t end;
    uint16_t *map_L;
    uint16_t *map_a;
    uint16_t *map_b;
    uint16_t *map_z;
    uint32_t jpeg_table_len;
#if 0
    logging_state_t *logging;
#endif

    printf("Demo of ITU/Lab library.\n");

#if 0
    logging = span_log_init(NULL, SPAN_LOG_FLOW, "T.42");
#endif

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    TIFF_FX_init();
#endif

    /* The default luminant is D50 */
    set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
    set_lab_gamut(&lab_param, 0, 100, -85, 85, -75, 125, false);

    source_file = (argc > 1)  ?  argv[1]  :  IN_FILE_NAME;
    /* sRGB to ITU */
    if ((tif = TIFFOpen(source_file, "r")) == NULL)
    {
        printf("Unable to open '%s'!\n", source_file);
        return 1;
    }
    if (TIFFSetDirectory(tif, (tdir_t) 0) < 0)
    {
        printf("Unable to set directory '%s'!\n", source_file);
        return 1;
    }

    w = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
    h = 0;
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
    bits_per_pixel = 0;
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_pixel);
    samples_per_pixel = 0;
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    compression = 0;
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
    photometric = 0;
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    YCbCrSubsampleHoriz = 0;
    YCbCrSubsampleVert = 0;
    TIFFGetField(tif, TIFFTAG_YCBCRSUBSAMPLING, &YCbCrSubsampleHoriz, &YCbCrSubsampleVert);
    planar_config = PLANARCONFIG_CONTIG;
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planar_config);
    off = 0;

    map_L = NULL;
    map_a = NULL;
    map_b = NULL;
    map_z = NULL;
    if (TIFFGetField(tif, TIFFTAG_COLORMAP, &map_L, &map_a, &map_b, &map_z))
    {
#if 0
        /* Sweep the colormap in the proper order */
        for (i = 0;  i < (1 << bits_per_pixel);  i++)
        {
            colour_map[3*i] = (map_L[i] >> 8) & 0xFF;
            colour_map[3*i + 1] = (map_a[i] >> 8) & 0xFF;
            colour_map[3*i + 2] = (map_b[i] >> 8) & 0xFF;
            printf("Map %3d - %5d %5d %5d\n", i, colour_map[3*i], colour_map[3*i + 1], colour_map[3*i + 2]);
        }
#else
        /* Sweep the colormap in the order that seems to work for l04x_02x.tif */
        for (i = 0;  i < (1 << bits_per_pixel);  i++)
        {
            colour_map[i] = (map_L[i] >> 8) & 0xFF;
            colour_map[256 + i] = (map_a[i] >> 8) & 0xFF;
            colour_map[2*256 + i] = (map_b[i] >> 8) & 0xFF;
        }
#endif
        lab_params_t lab;

        /* The default luminant is D50 */
        set_lab_illuminant(&lab, 96.422f, 100.000f,  82.521f);
        set_lab_gamut(&lab, 0, 100, -85, 85, -75, 125, false);
        lab_to_srgb(&lab, colour_map, colour_map, 256);
        for (i = 0;  i < (1 << bits_per_pixel);  i++)
            printf("Map %3d - %5d %5d %5d\n", i, colour_map[3*i], colour_map[3*i + 1], colour_map[3*i + 2]);
    }
    else
    {
        printf("There is no colour map\n");
    }
    process_raw = false;
    printf("Compression is ");
    switch (compression)
    {
    case COMPRESSION_CCITT_T4:
        printf("T.4\n");
        return 0;
    case COMPRESSION_CCITT_T6:
        printf("T.6\n");
        return 0;
    case COMPRESSION_T85:
        printf("T.85\n");
        process_raw = true;
        break;
    case COMPRESSION_T43:
        printf("T.43\n");
        process_raw = true;
        break;
    case COMPRESSION_JPEG:
        printf("JPEG");
        if (photometric == PHOTOMETRIC_ITULAB)
        {
            printf(" ITULAB");
            process_raw = true;
        }
        printf("\n");
        break;
    case COMPRESSION_NONE:
        printf("No compression\n");
        break;
    default:
        printf("Unexpected compression %d\n", compression);
        break;
    }

    outsize = 0;
    if (process_raw)
    {
        uint8_t *jpeg_table;

        nstrips = TIFFNumberOfStrips(tif);

        total_image_len = 0;
        jpeg_table_len = 0;
        if (TIFFGetField(tif, TIFFTAG_JPEGTABLES, &jpeg_table_len, &jpeg_table))
        {
            total_image_len += (jpeg_table_len - 4);
            printf("JPEG tables %u\n", jpeg_table_len);
            printf("YYY %d - %x %x %x %x\n", jpeg_table_len, jpeg_table[0], jpeg_table[1], jpeg_table[2], jpeg_table[3]);
        }

        for (i = 0, total_image_len = 0;  i < nstrips;  i++)
            total_image_len += TIFFRawStripSize(tif, i);
        data = malloc(total_image_len);
        for (i = 0, total_len = 0;  i < nstrips;  i++, total_len += len)
        {
            if ((len = TIFFReadRawStrip(tif, i, &data[total_len], total_image_len - total_len)) < 0)
            {
                printf("TIFF read error.\n");
                return -1;
            }
        }
        if (jpeg_table_len > 0)
            memcpy(data, jpeg_table, jpeg_table_len - 2);

        if (total_len != total_image_len)
            printf("Size mismatch %ld %ld\n", (long int) total_len, (long int) total_image_len);
        off = total_len;
        switch (compression)
        {
        case COMPRESSION_CCITT_T4:
            break;
        case COMPRESSION_CCITT_T6:
            break;
        case COMPRESSION_T85:
            printf("T.85 image %ld bytes\n", (long int) total_len);
            for (i = 0;  i < 16;  i++)
                printf("0x%02x\n", data[i]);
            t85_decode_init(&t85_dec, t85_row_write_handler, NULL);
            t85_decode_set_comment_handler(&t85_dec, 1000, t85_comment_handler, NULL);
            result = t85_decode_put(&t85_dec, data, total_len);
            if (result == T4_DECODE_MORE_DATA)
                result = t85_decode_put(&t85_dec, NULL, 0);
            len = t85_decode_get_compressed_image_size(&t85_dec);
            printf("Compressed image is %d bytes, %d rows\n", len/8, write_row);
            t85_decode_release(&t85_dec);
            return 0;
        case COMPRESSION_T43:
            printf("T.43 image %ld bytes\n", (long int) total_len);
            if (pack_16(data) == 0xFFA8)
            {
                data += 2;
                total_len -= 2;
                for (;;)
                {
                    if (pack_16(data) == 0xFFE1)
                    {
                        data += 2;
                        total_len -= 2;
                        len = pack_16(data);
                        data += len;
                        total_len -= len;
                    }
                    else if (pack_16(data) == 0xFFE3)
                    {
                        data += 2;
                        total_len -= 2;
                        len = pack_32(data);
                        data += len;
                        total_len -= len;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            bit_mask = 0x80;
            t85_decode_init(&t85_dec, t85_row_write_handler, NULL);
            t85_decode_set_comment_handler(&t85_dec, 1000, t85_comment_handler, NULL);
            t85_dec.min_bit_planes = 1;
            t85_dec.max_bit_planes = 8;
            data5_ptr = 0;
            result = t85_decode_put(&t85_dec, data, total_len);
            len = t85_decode_get_compressed_image_size(&t85_dec);
            printf("Compressed image is %d bytes, %d rows\n", len/8, write_row);

            for (j = 1;  j < t85_dec.bit_planes;  j++)
            {
                bit_mask >>= 1;
                data += len/8;
                total_len -= len/8;
                t85_decode_new_plane(&t85_dec);
                data5_ptr = 0;
                t85_decode_set_comment_handler(&t85_dec, 1000, t85_comment_handler, NULL);
                result = t85_decode_put(&t85_dec, data, total_len);
                len = t85_decode_get_compressed_image_size(&t85_dec);
                printf("Compressed image is %d bytes, %d rows\n", len/8, write_row);
            }
            if (result == T4_DECODE_MORE_DATA)
            {
                printf("More\n");
                result = t85_decode_put(&t85_dec, NULL, 0);
            }
            len = t85_decode_get_compressed_image_size(&t85_dec);
            printf("Compressed image is %d bytes, %d rows\n", len/8, write_row);
            t85_decode_release(&t85_dec);

            for (j = 0;  j < data5_ptr;  j += 3)
            {
                i = data5[j] & 0xFF;
//printf("%d %d %d %d %d %d\n", data5_ptr, j, i, colour_map[3*i], colour_map[3*i + 1], colour_map[3*i + 2]);
                data5[j] = colour_map[3*i];
                data5[j + 1] = colour_map[3*i + 1];
                data5[j + 2] = colour_map[3*i + 2];
            }

            if ((tif = TIFFOpen(OUT_FILE_NAME, "w")) == NULL)
            {
                printf("Unable to open '%s'!\n", OUT_FILE_NAME);
                return 1;
            }
            TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
            TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
            // libtiff requires IMAGELENGTH to be set before SAMPLESPERPIXEL,
            // or StripOffsets and StripByteCounts will have SAMPLESPERPIXEL values
            TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);
            TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_JPEG);
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
            TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
            TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
            TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, (uint32) -1);
            TIFFSetField(tif, TIFFTAG_XRESOLUTION, 200.0f);
            TIFFSetField(tif, TIFFTAG_YRESOLUTION, 200.0f);
            TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
            TIFFSetField(tif, TIFFTAG_SOFTWARE, "spandsp");
            TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, "Test");
            TIFFSetField(tif, TIFFTAG_DATETIME, "2012/07/03 12:30:45");
            TIFFSetField(tif, TIFFTAG_MAKE, "soft-switch.org");
            TIFFSetField(tif, TIFFTAG_MODEL, "spandsp");
            TIFFSetField(tif, TIFFTAG_HOSTCOMPUTER, "i7.coppice.org");

            for (off = 0, i = 0;  i < h;  off += w*3, i++)
            {
                TIFFWriteScanline(tif, data5 + off, i, 0);
            }
            TIFFWriteDirectory(tif);
            TIFFClose(tif);
            return 0;
        case COMPRESSION_JPEG:
            break;
        }
    }
    else
    {
        printf("Width %d, height %d, bits %d, samples %d\n", w, h, bits_per_pixel, samples_per_pixel);

        bytes_per_row = (bits_per_pixel + 7)/8;
        bytes_per_row *= w*samples_per_pixel;
        totdata = h*bytes_per_row;
        printf("total %d\n", totdata);

        /* Read the image into memory. */
        data = malloc(totdata);
        off = 0;
        for (row = 0;  row < h;  row++)
        {
            if (TIFFReadScanline(tif, data + off, row, 0) < 0)
                return 1;
            off += bytes_per_row;
        }
        printf("total %u, off %ld\n", totdata, (long int) off);

        /* We now have the image in memory in RGB form */

        if (photometric == PHOTOMETRIC_ITULAB)
        {
            printf("YYY ITULAB\n");
#if 0
            if (!t42_itulab_to_itulab(logging, (tdata_t) &outptr, &outsize, data, off, w, h, 3))
            {
                printf("Failed to convert to ITULAB\n");
                return 1;
            }
#else
            outptr = 0;
#endif
            free(data);
            data = (uint8_t *) outptr;
            off = outsize;
        }
        else
        {
            start = rdtscll();
            switch (photometric)
            {
            case PHOTOMETRIC_CIELAB:
                printf("CIELAB\n");
                /* The default luminant is D50 */
                set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
                set_lab_gamut(&lab_param, 0, 100, -128, 127, -128, 127, true);
                lab_to_srgb(&lab_param, data, data, w*h);
                break;
            case PHOTOMETRIC_ITULAB:
                /* The default luminant is D50 */
                set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
                set_lab_gamut(&lab_param, 0, 100, -85, 85, -75, 125, false);
                break;
            }
            //if (!t42_srgb_to_itulab_jpeg(logging, &lab_param, (tdata_t) &outptr, &outsize, data, off, w, h, 3))
            {
                printf("Failed to convert to ITULAB\n");
                return 1;
            }
            end = rdtscll();
            printf("Duration %" PRIu64 "\n", end - start);
            free(data);
            data = (uint8_t *) outptr;
            off = outsize;
        }
    }
    TIFFClose(tif);

    printf("XXX - image is %d by %d, %ld bytes\n", w, h, (long int) off);

    /* We now have the image in memory in ITULAB form */

    if ((tif = TIFFOpen(OUT_FILE_NAME, "w")) == NULL)
    {
        printf("Unable to open '%s'!\n", OUT_FILE_NAME);
        return 1;
    }
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
    /* libtiff requires IMAGELENGTH to be set before SAMPLESPERPIXEL,
       or StripOffsets and StripByteCounts will have SAMPLESPERPIXEL values */
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_JPEG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, (uint32) -1);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, 200.0f);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, 200.0f);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "spandsp");
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, "Test");
    TIFFSetField(tif, TIFFTAG_DATETIME, "2012/07/03 12:30:45");
    TIFFSetField(tif, TIFFTAG_MAKE, "soft-switch.org");
    TIFFSetField(tif, TIFFTAG_MODEL, "spandsp");
    TIFFSetField(tif, TIFFTAG_HOSTCOMPUTER, "i7.coppice.org");

    if (1)
    {
        /* Most image processors won't know what to do with the ITULAB colorspace.
           So we'll be converting it to RGB for portability. */
#if 1
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
#else
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR);
#endif
        if (YCbCrSubsampleHoriz  ||  YCbCrSubsampleVert)
            TIFFSetField(tif, TIFFTAG_YCBCRSUBSAMPLING, YCbCrSubsampleHoriz, YCbCrSubsampleVert);
        bytes_per_row = (bits_per_pixel + 7)/8;
        bytes_per_row *= w*samples_per_pixel;
        totdata = h*bytes_per_row;
        /* The default luminant is D50 */
        set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
        set_lab_gamut(&lab_param, 0, 100, -85, 85, -75, 125, false);
#if 0
        start = rdtscll();
        data2 = NULL;
        totdata = 0;
        t42_itulab_to_jpeg(logging, &lab_param, (void **) &data2, &totdata, data, off);
        end = rdtscll();
        printf("Duration %" PRIu64 "\n", end - start);
        printf("Compressed length %d (%p)\n", totdata, data2);
        if (TIFFWriteRawStrip(tif, 0, data2, totdata) < 0)
        {
            printf("Failed to convert from ITULAB\n");
            return 1;
        }
        free(data);
#else
        data2 = malloc(totdata);
        start = rdtscll();
        //if (!t42_itulab_jpeg_to_srgb(logging, &lab_param, data2, &off, data, off, &w, &h, &samples_per_pixel))
        {
            printf("Failed to convert from ITULAB\n");
            return 1;
        }
        end = rdtscll();
        printf("Duration %" PRIu64 "\n", end - start);
        free(data);

        off = 0;
        bytes_per_row = (8 + 7)/8;
        bytes_per_row *= (w*3);
        for (row = 0;  row < h;  row++)
        {
            if (TIFFWriteScanline(tif, data2 + off, row, 0) < 0)
                return 1;
            off += bytes_per_row;
        }
#endif
        free(data2);
    }
    else
    {
#if 1
        /* If PHOTOMETRIC_ITULAB is not available the admin cannot enable color fax anyway.
           This is done so that older libtiffs without it can build fine. */
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_ITULAB);
#else
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_YCBCR);
#endif
        if (YCbCrSubsampleHoriz  ||  YCbCrSubsampleVert)
            TIFFSetField(tif, TIFFTAG_YCBCRSUBSAMPLING, YCbCrSubsampleHoriz, YCbCrSubsampleVert);
        if (TIFFWriteRawStrip(tif, 0, (tdata_t) data, off) == -1)
        {
            printf("Write error to TIFF file\n");
            return 1;
        }
        free(data);
    }
    TIFFWriteDirectory(tif);
    TIFFClose(tif);
    printf("Done!\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

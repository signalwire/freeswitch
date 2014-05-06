/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t43_tests.c - ITU T.43 JBIG for grey and colour FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011, 2013 Steve Underwood
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

/*! \page t43_tests_page T.43 tests
\section t43_tests_page_sec_1 What does it do
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

#define IN_FILE_NAME    "../test-data/itu/tiff-fx/l04x_02x.tif"
#define OUT_FILE_NAME   "t43_tests_receive.tif"

t43_decode_state_t t43;
t85_decode_state_t t85;

lab_params_t lab_param;

int write_row = 0;

typedef struct
{
    uint8_t *buf;
    int ptr;
} packer_t;

#if defined(SPANDSP_SUPPORT_TIFF_FX)
/* TIFF-FX related extensions to the tag set supported by libtiff */
static const TIFFFieldInfo tiff_fx_tiff_field_info[] =
{
    {TIFFTAG_INDEXED, 1, 1, TIFF_SHORT, FIELD_CUSTOM, false, false, (char *) "Indexed"},
	{TIFFTAG_GLOBALPARAMETERSIFD, 1, 1, TIFF_IFD8, FIELD_CUSTOM, false, false, (char *) "GlobalParametersIFD"},
    {TIFFTAG_PROFILETYPE, 1, 1, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "ProfileType"},
    {TIFFTAG_FAXPROFILE, 1, 1, TIFF_BYTE, FIELD_CUSTOM, false, false, (char *) "FaxProfile"},
    {TIFFTAG_CODINGMETHODS, 1, 1, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "CodingMethods"},
    {TIFFTAG_VERSIONYEAR, 4, 4, TIFF_BYTE, FIELD_CUSTOM, false, false, (char *) "VersionYear"},
    {TIFFTAG_MODENUMBER, 1, 1, TIFF_BYTE, FIELD_CUSTOM, false, false, (char *) "ModeNumber"},
    {TIFFTAG_DECODE, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SRATIONAL, FIELD_CUSTOM, false, true, (char *) "Decode"},
    {TIFFTAG_IMAGEBASECOLOR, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_SHORT, FIELD_CUSTOM, false, true, (char *) "ImageBaseColor"},
    {TIFFTAG_T82OPTIONS, 1, 1, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "T82Options"},
    {TIFFTAG_STRIPROWCOUNTS, TIFF_VARIABLE, TIFF_VARIABLE, TIFF_LONG, FIELD_CUSTOM, false, true, (char *) "StripRowCounts"},
    {TIFFTAG_IMAGELAYER, 2, 2, TIFF_LONG, FIELD_CUSTOM, false, false, (char *) "ImageLayer"},
};

static TIFFFieldArray tifffxFieldArray;

static TIFFField tiff_fx_tiff_fields[] =
{
    { TIFFTAG_INDEXED, 1, 1, TIFF_SHORT, 0, TIFF_SETGET_UINT16, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "Indexed" },
	{ TIFFTAG_GLOBALPARAMETERSIFD, 1, 1, TIFF_IFD8, 0, TIFF_SETGET_IFD8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 0, 0, (char *) "GlobalParametersIFD", &tifffxFieldArray },
    { TIFFTAG_PROFILETYPE, 1, 1, TIFF_LONG, 0, TIFF_SETGET_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "ProfileType", NULL },
    { TIFFTAG_FAXPROFILE, 1, 1, TIFF_BYTE, 0, TIFF_SETGET_UINT8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "FaxProfile", NULL },
    { TIFFTAG_CODINGMETHODS, 1, 1, TIFF_LONG, 0, TIFF_SETGET_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "CodingMethods", NULL },
    { TIFFTAG_VERSIONYEAR, 4, 4, TIFF_BYTE, 0, TIFF_SETGET_C0_UINT8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "VersionYear", NULL },
    { TIFFTAG_MODENUMBER, 1, 1, TIFF_BYTE, 0, TIFF_SETGET_UINT8, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "ModeNumber", NULL },
    { TIFFTAG_DECODE, -1, -1, TIFF_SRATIONAL, 0, TIFF_SETGET_C16_FLOAT, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 1, (char *) "Decode", NULL },
    { TIFFTAG_IMAGEBASECOLOR, -1, -1, TIFF_SHORT, 0, TIFF_SETGET_C16_UINT16, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 1, (char *) "ImageBaseColor", NULL },
    { TIFFTAG_T82OPTIONS, 1, 1, TIFF_LONG, 0, TIFF_SETGET_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "T82Options", NULL },
    { TIFFTAG_STRIPROWCOUNTS, -1, -1, TIFF_LONG, 0, TIFF_SETGET_C16_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 1, (char *) "StripRowCounts", NULL },
    { TIFFTAG_IMAGELAYER, 2, 2, TIFF_LONG, 0, TIFF_SETGET_C0_UINT32, TIFF_SETGET_UNDEFINED, FIELD_CUSTOM, 1, 0, (char *) "ImageLayer", NULL },
};

static TIFFFieldArray tiff_fx_field_array = { tfiatOther, 0, 12, tiff_fx_tiff_fields };
#endif

typedef struct
{
    TIFF *tif;
    int pre_compressed;
    uint32_t compressed_image_len;
    uint32_t image_width;
    uint32_t image_length;
    float x_resolution;
    float y_resolution;
    uint16_t resolution_unit;
    uint16_t bits_per_sample;
    uint16_t samples_per_pixel;
    uint16_t compression;
    uint16_t photometric;
    int16_t YCbCrSubsampleHoriz;
    int16_t YCbCrSubsampleVert;
    int16_t planar_config;
    int32_t tile_width;
    int32_t tile_length;
    uint8_t *colour_map;
    float lmin;
    float lmax;
    float amin;
    float amax;
    float bmin;
    float bmax;
} meta_t;

int write_file(meta_t *meta, int page, const uint8_t buf[]);
int read_file(meta_t *meta, int page);
int read_compressed_image(meta_t *meta, uint8_t **buf);
int read_decompressed_image(meta_t *meta, uint8_t **buf);

static int row_write_handler(void *user_data, const uint8_t buf[], size_t len)
{
    packer_t *s;

    s = (packer_t *) user_data;
    memcpy(&s->buf[s->ptr], buf, len);
    s->ptr += len;
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

static int row_read_handler(void *user_data, uint8_t row[], size_t len)
{
    packer_t *s;

    s = (packer_t *) user_data;
    memcpy(row, &s->buf[s->ptr], len);
    s->ptr += len;
    return len;
}
/*- End of function --------------------------------------------------------*/

int write_file(meta_t *meta, int page, const uint8_t buf[])
{
    TIFF *tif;
    int off;
    int i;
    time_t now;
    struct tm *tm;
    char date_buf[50 + 1];
    int bytes_per_row;
    t85_encode_state_t t85;
    t43_encode_state_t t43;
    int out_buf_len;
    int out_len;
    int chunk_len;
    uint8_t *out_buf;
    uint8_t *out_buf2;
    packer_t packer;
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    toff_t diroff;
#endif

    tif = meta->tif;
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, meta->image_width);
    /* libtiff requires IMAGELENGTH to be set before SAMPLESPERPIXEL,
       or StripOffsets and StripByteCounts will have SAMPLESPERPIXEL values */
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, meta->image_length);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, meta->compression);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, meta->bits_per_sample);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, meta->samples_per_pixel);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, meta->image_length);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, meta->x_resolution);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, meta->y_resolution);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, meta->resolution_unit);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, meta->photometric);
    if (meta->samples_per_pixel > 1  &&  (meta->YCbCrSubsampleHoriz  ||  meta->YCbCrSubsampleVert))
        TIFFSetField(tif, TIFFTAG_YCBCRSUBSAMPLING, meta->YCbCrSubsampleHoriz, meta->YCbCrSubsampleVert);
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "spandsp");
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, "Test");
    time(&now);
    tm = localtime(&now);
    sprintf(date_buf,
            "%4d/%02d/%02d %02d:%02d:%02d",
            tm->tm_year + 1900,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min,
            tm->tm_sec);
    TIFFSetField(tif, TIFFTAG_DATETIME, date_buf);
    TIFFSetField(tif, TIFFTAG_MAKE, "soft-switch.org");
    TIFFSetField(tif, TIFFTAG_MODEL, "spandsp");
    TIFFSetField(tif, TIFFTAG_HOSTCOMPUTER, "i7.coppice.org");
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    /* Make space for this to be filled in later */
    TIFFSetField(tif, TIFFTAG_GLOBALPARAMETERSIFD, 0);
#endif

    if (meta->pre_compressed)
    {
        if (TIFFWriteRawStrip(tif, 0, (tdata_t) buf, meta->compressed_image_len) < 0)
            printf("Error writing TIFF strip.\n");
    }
    else
    {
        switch (meta->compression)
        {
        case COMPRESSION_T85:
            packer.buf = (uint8_t *) buf;
            packer.ptr = 0;
            t85_encode_init(&t85, meta->image_width, meta->image_length, row_read_handler, &packer);
            //if (meta->compression == T4_COMPRESSION_T85_L0)
            //    t85_encode_set_options(&t85, 256, -1, -1);
            out_len = 0;
            out_buf_len = 0;
            out_buf = NULL;
            do
            {
                if (out_buf_len < out_len + 50000)
                {
                    out_buf_len += 50000;
                    if ((out_buf2 = realloc(out_buf, out_buf_len)) == NULL)
                    {
                        if (out_buf)
                            free(out_buf);
                        return -1;
                    }
                    out_buf = out_buf2;
                }
                chunk_len = t85_encode_get(&t85, &out_buf[out_len], 50000);
                out_len += chunk_len;
            }
            while (chunk_len > 0);
            if (TIFFWriteRawStrip(tif, 0, out_buf, out_len) < 0)
                printf("Error writing TIFF strip.\n");
            t85_encode_release(&t85);
            free(out_buf);
            break;
        case COMPRESSION_T43:
            packer.buf = (uint8_t *) buf;
            packer.ptr = 0;
            t43_encode_init(&t43, meta->image_width, meta->image_length, row_read_handler, &packer);
            out_len = 0;
            out_buf_len = 0;
            out_buf = NULL;
            do
            {
                if (out_buf_len < out_len + 50000)
                {
                    out_buf_len += 50000;
                    if ((out_buf2 = realloc(out_buf, out_buf_len)) == NULL)
                    {
                        if (out_buf)
                            free(out_buf);
                        return -1;
                    }
                    out_buf = out_buf2;
                }
                chunk_len = t43_encode_get(&t43, &out_buf[out_len], 50000);
                out_len += chunk_len;
            }
            while (chunk_len > 0);
            if (TIFFWriteRawStrip(tif, 0, out_buf, out_len) < 0)
                printf("Error writing TIFF strip.\n");
            t43_encode_release(&t43);
            free(out_buf);
            break;
        default:
            bytes_per_row = TIFFScanlineSize(tif);
            for (off = 0, i = 0;  i < meta->image_length;  off += bytes_per_row, i++)
            {
                if (TIFFWriteScanline(tif, (tdata_t) &buf[off], i, 0) < 0)
                    printf("Error writing TIFF scan line.\n");
            }
            break;
        }
    }

    if (!TIFFWriteDirectory(tif))
        printf("Failed to write directory.\n");

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    if (!TIFFCreateCustomDirectory(tif, &tiff_fx_field_array))
    {
        TIFFSetField(tif, TIFFTAG_PROFILETYPE, PROFILETYPE_G3_FAX);
        TIFFSetField(tif, TIFFTAG_FAXPROFILE, FAXPROFILE_F);
        TIFFSetField(tif, TIFFTAG_CODINGMETHODS, CODINGMETHODS_T4_1D | CODINGMETHODS_T4_2D | CODINGMETHODS_T6);
        TIFFSetField(tif, TIFFTAG_VERSIONYEAR, "1998");
        TIFFSetField(tif, TIFFTAG_MODENUMBER, 3);

        diroff = 0;
        if (!TIFFWriteCustomDirectory(tif, &diroff))
            printf("Failed to write custom directory.\n");

        if (!TIFFSetDirectory(tif, (tdir_t) page))
            printf("Failed to set directory.\n");
        if (!TIFFSetField(tif, TIFFTAG_GLOBALPARAMETERSIFD, diroff))
            printf("Failed to set global parameters IFD.\n");
        if (!TIFFWriteDirectory(tif))
            printf("Failed to write directory.\n");
    }
#endif
    return 0;
}
/*- End of function --------------------------------------------------------*/

int read_file(meta_t *meta, int page)
{
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    static const char *tiff_fx_fax_profiles[] =
    {
        "???",
        "profile S",
        "profile F",
        "profile J",
        "profile C",
        "profile L",
        "profile M"
    };
    uint8_t parm8;
    uint16_t parm16;
    uint32_t parm32;
    float *fl_parms;
    char uu[10];
    char *u;
    toff_t diroff;
#endif
    TIFF *tif;
    uint16_t *map_L;
    uint16_t *map_a;
    uint16_t *map_b;
    uint16_t *map_z;
    lab_params_t lab;
    int entries;
    int i;

    tif = meta->tif;
    printf("Read %d\n", page);
    if (!TIFFSetDirectory(tif, (tdir_t) page))
    {
        printf("Unable to set TIFF directory %d!\n", page);
        return -1;
    }
    meta->image_width = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &meta->image_width);
    meta->image_length = 0;
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &meta->image_length);
    meta->x_resolution = 200.0f;
    TIFFGetField(tif, TIFFTAG_XRESOLUTION, &meta->x_resolution);
    meta->y_resolution = 200.0f;
    TIFFGetField(tif, TIFFTAG_YRESOLUTION, &meta->y_resolution);
    meta->resolution_unit = RESUNIT_INCH;
    TIFFGetField(tif, TIFFTAG_RESOLUTIONUNIT, &meta->resolution_unit);
    meta->bits_per_sample = 0;
    TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &meta->bits_per_sample);
    meta->samples_per_pixel = 0;
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &meta->samples_per_pixel);
    meta->compression = 0;
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &meta->compression);
    meta->photometric = 0;
    TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &meta->photometric);
    meta->YCbCrSubsampleHoriz = 0;
    meta->YCbCrSubsampleVert = 0;
    TIFFGetField(tif, TIFFTAG_YCBCRSUBSAMPLING, &meta->YCbCrSubsampleHoriz, &meta->YCbCrSubsampleVert);
    meta->planar_config = PLANARCONFIG_CONTIG;
    TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &meta->planar_config);
    meta->tile_width = 0;
    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &meta->tile_width);
    meta->tile_length = 0;
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &meta->tile_length);

    switch (meta->photometric)
    {
    case PHOTOMETRIC_ITULAB:
        meta->lmin = 0.0f;
        meta->lmax = 100.0f;
        meta->amin = -21760.0f/255.0f;  // For 12 bit -348160.0f/4095.0f
        meta->amax = 21590.0f/255.0f;   // For 12 bit 347990.0f/4095.0f
        meta->bmin = -19200.0f/255.0f;  // For 12 bit -307200.0f/4095.0f
        meta->bmax = 31800.0f/255.0f;   // For 12 bit 511800.0f/4095.0f
        break;
    default:
        meta->lmin = 0.0f;
        meta->lmax = 0.0f;
        meta->amin = 0.0f;
        meta->amax = 0.0f;
        meta->bmin = 0.0f;
        meta->bmax = 0.0f;
        break;
    }
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    if (TIFFGetField(tif, TIFFTAG_DECODE, &parm16, &fl_parms))
    {
        meta->lmin = fl_parms[0];
        meta->lmax = fl_parms[1];
        meta->amin = fl_parms[2];
        meta->amax = fl_parms[3];
        meta->bmin = fl_parms[4];
        meta->bmax = fl_parms[5];
        printf("Got decode tag %f %f %f %f %f %f\n", meta->lmin, meta->lmax, meta->amin, meta->amax, meta->bmin, meta->bmax);
    }
#endif

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    printf("Trying to get global parameters\n");
    if (TIFFGetField(tif, TIFFTAG_GLOBALPARAMETERSIFD, &diroff))
    {
        printf("Got global parameters - %" PRIu64 "\n", (uint64_t) diroff);
        if (!TIFFReadCustomDirectory(tif, diroff, &tiff_fx_field_array))
        {
            printf("Failed to set global parameters IFD.\n");
        }
        else
        {
            if (TIFFGetField(tif, TIFFTAG_PROFILETYPE, &parm32))
                printf("  Profile type %u\n", parm32);
            if (TIFFGetField(tif, TIFFTAG_FAXPROFILE, &parm8))
                printf("  FAX profile %s (%u)\n", tiff_fx_fax_profiles[parm8], parm8);
            if (TIFFGetField(tif, TIFFTAG_CODINGMETHODS, &parm32))
                printf("  Coding methods 0x%x\n", parm32);
            if (TIFFGetField(tif, TIFFTAG_VERSIONYEAR, &u))
            {
                memcpy(uu, u, 4);
                uu[4] = '\0';
                printf("  Version year \"%s\"\n", uu);
            }
            if (TIFFGetField(tif, TIFFTAG_MODENUMBER, &parm8))
                printf("  Mode number %u\n", parm8);
        }
        TIFFSetDirectory(tif, (tdir_t) page);
    }

    if (TIFFGetField(tif, TIFFTAG_PROFILETYPE, &parm32))
        printf("Profile type %u\n", parm32);
    if (TIFFGetField(tif, TIFFTAG_FAXPROFILE, &parm8))
        printf("FAX profile %s (%u)\n", tiff_fx_fax_profiles[parm8], parm8);
    if (TIFFGetField(tif, TIFFTAG_CODINGMETHODS, &parm32))
        printf("Coding methods 0x%x\n", parm32);
    if (TIFFGetField(tif, TIFFTAG_VERSIONYEAR, &u))
    {
        memcpy(uu, u, 4);
        uu[4] = '\0';
        printf("Version year \"%s\"\n", uu);
    }
    if (TIFFGetField(tif, TIFFTAG_MODENUMBER, &parm8))
        printf("Mode number %u\n", parm8);
    if (TIFFGetField(tif, TIFFTAG_T82OPTIONS, &parm32))
        printf("T.82 options 0x%x\n", parm32);
#endif

    map_L = NULL;
    map_a = NULL;
    map_b = NULL;
    map_z = NULL;
    if (TIFFGetField(tif, TIFFTAG_COLORMAP, &map_L, &map_a, &map_b, &map_z))
    {
        entries = 1 << meta->bits_per_sample;
        meta->colour_map = malloc(3*entries);
        if (meta->colour_map)
        {
#if 0
            /* Sweep the colormap in the proper order */
            for (i = 0;  i < entries;  i++)
            {
                meta->colour_map[3*i] = (map_L[i] >> 8) & 0xFF;
                meta->colour_map[3*i + 1] = (map_a[i] >> 8) & 0xFF;
                meta->colour_map[3*i + 2] = (map_b[i] >> 8) & 0xFF;
                printf("Map %3d - %5d %5d %5d\n", i, meta->colour_map[3*i], meta->colour_map[3*i + 1], meta->colour_map[3*i + 2]);
            }
#else
            /* Sweep the colormap in the order that seems to work for l04x_02x.tif */
            for (i = 0;  i < entries;  i++)
            {
                meta->colour_map[i] = (map_L[i] >> 8) & 0xFF;
                meta->colour_map[256 + i] = (map_a[i] >> 8) & 0xFF;
                meta->colour_map[2*256 + i] = (map_b[i] >> 8) & 0xFF;
            }
#endif
            /* The default luminant is D50 */
            set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
            set_lab_gamut(&lab, 0, 100, -85, 85, -75, 125, false);
            lab_to_srgb(&lab, meta->colour_map, meta->colour_map, 256);
            for (i = 0;  i < entries;  i++)
                printf("Map %3d - %5d %5d %5d\n", i, meta->colour_map[3*i], meta->colour_map[3*i + 1], meta->colour_map[3*i + 2]);
        }
    }
    meta->tif = tif;
    return 0;
}
/*- End of function --------------------------------------------------------*/

int read_compressed_image(meta_t *meta, uint8_t **buf)
{
    int i;
    int len;
    int total_len;
    int read_len;
    int num_strips;
    uint8_t *data;

    num_strips = TIFFNumberOfStrips(meta->tif);
    for (i = 0, total_len = 0;  i < num_strips;  i++)
    {
        total_len += TIFFRawStripSize(meta->tif, i);
    }
    if ((data = malloc(total_len)) == NULL)
        return -1;
    for (i = 0, read_len = 0;  i < num_strips;  i++, read_len += len)
    {
        if ((len = TIFFReadRawStrip(meta->tif, i, &data[read_len], total_len - read_len)) < 0)
        {
            printf("TIFF read error.\n");
            return -1;
        }
    }
    *buf = data;
    return total_len;
}
/*- End of function --------------------------------------------------------*/

int read_decompressed_image(meta_t *meta, uint8_t **buf)
{
    int bytes_per_row;
    int x;
    int y;
    int xx;
    int yy;
    int xxx;
    int yyy;
    int i;
    int j;
    int result;
    int total_raw;
    int total_data;
    uint8_t *raw_buf;
    uint8_t *image_buf;
    t85_decode_state_t t85;
    t43_decode_state_t t43;
    packer_t pack;
    logging_state_t *logging;
    logging_state_t logging2;
#if 0
    uint8_t *jpeg_table;
    uint32_t jpeg_table_len;
    tsize_t off;
    uint32_t w;
    uint32_t h;
#endif

    image_buf = NULL;
    total_data = 0;
    switch (meta->compression)
    {
    case COMPRESSION_T85:
        bytes_per_row = (meta->image_width + 7)/8;
        total_data = meta->image_length*bytes_per_row;
        printf("Total decompressed data %d, %d per row\n", total_data, bytes_per_row);

        /* Read the image into memory. */
        if ((image_buf = malloc(total_data)) == NULL)
        {
            printf("Failed to allocated image buffer\n");
            return -1;
        }
        total_raw = read_compressed_image(meta, &raw_buf);
        t85_decode_init(&t85, row_write_handler, &pack);
        t85_decode_set_comment_handler(&t85, 1000, t85_comment_handler, NULL);
        logging = t85_decode_get_logging_state(&t85);
        span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

        pack.buf = image_buf;
        pack.ptr = 0;

        result = t85_decode_put(&t85, raw_buf, total_raw);
        if (result == T4_DECODE_MORE_DATA)
            result = t85_decode_put(&t85, NULL, 0);
        total_data = t85_decode_get_compressed_image_size(&t85);
        printf("Compressed image is %d/%d bytes, %d rows\n", total_raw, total_data/8, write_row);
        t85_decode_release(&t85);
        free(raw_buf);
        break;
    case COMPRESSION_T43:
        bytes_per_row = meta->samples_per_pixel*meta->image_width;
        total_data = meta->image_length*bytes_per_row;
        printf("Total decompressed data %d, %d per row\n", total_data, bytes_per_row);

total_data *= 8;
        /* Read the image into memory. */
        if ((image_buf = malloc(total_data)) == NULL)
            printf("Failed to allocated image buffer\n");

        total_raw = read_compressed_image(meta, &raw_buf);
        t43_decode_init(&t43, row_write_handler, &pack);
        t43_decode_set_comment_handler(&t43, 1000, t85_comment_handler, NULL);
        logging = t43_decode_get_logging_state(&t43);
        span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);

        pack.buf = image_buf;
        pack.ptr = 0;

        result = t43_decode_put(&t43, raw_buf, total_raw);
        if (result == T4_DECODE_MORE_DATA)
            result = t43_decode_put(&t43, NULL, 0);
        t43_decode_release(&t43);
        free(raw_buf);

        meta->samples_per_pixel = 1;
        meta->photometric = PHOTOMETRIC_RGB;
        printf("Image %d x %d pixels\n", meta->image_width, meta->image_length);
        break;
    case COMPRESSION_JPEG:
        if (meta->photometric == PHOTOMETRIC_ITULAB)
        {
            printf(" ITULAB");

            span_log_init(&logging2, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW, "lab");
            bytes_per_row = TIFFScanlineSize(meta->tif);
            total_data = meta->image_length*bytes_per_row;
            printf("Total decompressed data %d, %d per row\n", total_data, bytes_per_row);

            /* Read the image into memory. */
            if ((image_buf = malloc(total_data)) == NULL)
                printf("Failed to allocated image buffer\n");

#if 0
            jpeg_table_len = 0;
            if (TIFFGetField(meta->tif, TIFFTAG_JPEGTABLES, &jpeg_table_len, &jpeg_table))
            {
                total_image_len += (jpeg_table_len - 4);
                printf("JPEG tables %u\n", jpeg_table_len);
{
int ii;
printf("YYY1 %d - ", jpeg_table_len);
for (ii = 0;  ii < jpeg_table_len;  ii++)
    printf(" %02x", jpeg_table[ii]);
printf("\n");
}
            }
#endif
            total_raw = read_compressed_image(meta, &raw_buf);
            //if (!t42_itulab_jpeg_to_srgb(&logging2, &lab_param, (tdata_t) image_buf, &off, raw_buf, total_raw, &w, &h, &samples_per_pixel))
            {
                printf("Failed to convert from ITULAB.\n");
                return 1;
            }
            meta->photometric = PHOTOMETRIC_RGB;

#if 0
            total_len = 0;
            if (jpeg_table_len > 0)
                total_len += jpeg_table_len - 4;

printf("nstrips %d\n", nstrips);
            data2 = NULL;
            for (i = 0;  i < nstrips;  i++, total_len += len)
            {
                total_len = 0;
                if (jpeg_table_len > 0)
                    total_len += jpeg_table_len - 4;
                if ((len = TIFFReadRawStrip(tif, i, &data[total_len], total_image_len - total_len)) < 0)
                {
                    printf("TIFF read error.\n");
                    return -1;
                }
                if (jpeg_table_len > 0)
                {
                    memcpy(data, jpeg_table, jpeg_table_len - 2);
printf("%02x %02x %02x %02x\n", data[total_len], data[total_len + 1], data[total_len + 2], data[total_len + 3]);
                }
                totdata = meta->image_width*3000*meta->samples_per_pixel;
                data2 = realloc(data2, totdata);
                off = total_len;
                if (!t42_itulab_jpeg_to_srgb(&logging2, &lab_param, data2, &off, data, off, &w, &h, &samples_per_pixel))
                {
                    printf("Failed to convert from ITULAB.\n");
                    return 1;
                }
            }
            if (data2)
                free(data2);
            //exit(2);
            if (jpeg_table_len > 0)
                memcpy(data, jpeg_table, jpeg_table_len - 2);

            if (total_len != total_image_len)
                printf("Size mismatch %d %d\n", (int) total_len, (int) total_image_len);
{
int ii;

printf("YYY2 %d - ", jpeg_table_len);
for (ii = 0;  ii < 800;  ii++)
    printf(" %02x", data[ii]);
printf("\n");
}
            off = total_len;
            len = total_len;
#endif
            break;
        }
        /* Fall through */
    default:
        if (meta->tile_width > 0)
        {
            /* The image is tiled, so we need to patch together a bunch of tiles */
            switch (meta->planar_config)
            {
            case PLANARCONFIG_CONTIG:
                bytes_per_row = TIFFScanlineSize(meta->tif);
                total_data = meta->image_length*bytes_per_row;
                printf("Total decompressed data %d, %d per row\n", total_data, bytes_per_row);

                /* Read the image into memory. */
                if ((image_buf = malloc(total_data)) == NULL)
                    printf("Failed to allocated image buffer\n");

                for (y = 0;  y < meta->image_length;  y += meta->tile_length)
                {
                    for (x = 0;  x < meta->image_width;  x += meta->tile_width)
                    {
                        uint8_t data[meta->tile_width*meta->tile_length*meta->samples_per_pixel];

                        TIFFReadTile(meta->tif, data, x, y, 0, 0);
                        yyy = meta->tile_length;
                        if (y + meta->tile_length > meta->image_length)
                            yyy = meta->image_length - y;
                        xxx = meta->tile_width;
                        if (x + meta->tile_width > meta->image_width)
                            xxx = meta->image_width - x;
                        for (yy = 0;  yy < yyy;  yy++)
                        {
                            for (xx = 0;  xx < xxx;  xx++)
                            {
                                for (j = 0;  j < meta->samples_per_pixel;  j++)
                                    image_buf[meta->samples_per_pixel*((y + yy)*meta->image_width + x + xx) + j] = data[meta->samples_per_pixel*(yy*meta->tile_width + xx) + j];
                            }
                        }
                    }
                }
                break;
            case PLANARCONFIG_SEPARATE:
                bytes_per_row = TIFFScanlineSize(meta->tif);
                total_data = meta->samples_per_pixel*meta->image_length*bytes_per_row;
                printf("Total decompressed data %d, %d per row\n", total_data, bytes_per_row);

                /* Read the image into memory. */
                if ((image_buf = malloc(total_data)) == NULL)
                    printf("Failed to allocated image buffer\n");

                for (j = 0;  j < meta->samples_per_pixel;  j++)
                {
                    for (y = 0;  y < meta->image_length;  y += meta->tile_length)
                    {
                        for (x = 0;  x < meta->image_width;  x += meta->tile_width)
                        {
                            uint8_t data[meta->tile_width*meta->tile_length*meta->samples_per_pixel];

                            TIFFReadTile(meta->tif, data, x, y, 0, j);
                            yyy = meta->tile_length;
                            if (y + meta->tile_length > meta->image_length)
                                yyy = meta->image_length - y;
                            xxx = meta->tile_width;
                            if (x + meta->tile_width > meta->image_width)
                                xxx = meta->image_width - x;
                            for (yy = 0;  yy < yyy;  yy++)
                            {
                                for (xx = 0;  xx < xxx;  xx++)
                                {
                                    image_buf[meta->samples_per_pixel*((y + yy)*meta->image_width + x + xx) + j] = data[yy*meta->tile_width + xx];
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
        else
        {
            /* There is no tiling to worry about, but we might have planar issues to resolve */
            switch (meta->planar_config)
            {
            case PLANARCONFIG_CONTIG:
                bytes_per_row = TIFFScanlineSize(meta->tif);
                total_data = meta->image_length*bytes_per_row;
                printf("Total decompressed data %d, %d per row\n", total_data, bytes_per_row);

                /* Read the image into memory. */
                if ((image_buf = malloc(total_data)) == NULL)
                    printf("Failed to allocated image buffer\n");

                for (y = 0;  y < meta->image_length;  y++)
                {
                    if (TIFFReadScanline(meta->tif, &image_buf[y*bytes_per_row], y, 0) < 0)
                        return 1;
                }
                break;
            case PLANARCONFIG_SEPARATE:
                bytes_per_row = TIFFScanlineSize(meta->tif);
                total_data = meta->samples_per_pixel*meta->image_length*bytes_per_row;
                printf("Total decompressed data %d, %d per row\n", total_data, bytes_per_row);

                /* Read the image into memory. */
                if ((image_buf = malloc(total_data)) == NULL)
                    printf("Failed to allocated image buffer\n");

                for (j = 0;  j < meta->samples_per_pixel;  j++)
                {
                    uint8_t data[bytes_per_row];

                    for (y = 0;  y < meta->image_length;  y++)
                    {
                        if (TIFFReadScanline(meta->tif, data, y, j) < 0)
                            return 1;
                        for (x = 0;  x < meta->image_width;  x++)
                            image_buf[meta->samples_per_pixel*(y*bytes_per_row + x) + j] = data[x];
                    }
                }
                break;
            }
        }
        break;
    }
    /* Normalise bi-level images, so they are always in PHOTOMETRIC_MINISWHITE form */
    if (image_buf  &&  meta->samples_per_pixel == 1  &&  meta->bits_per_sample == 1)
    {
        if (meta->photometric != PHOTOMETRIC_MINISWHITE)
        {
            for (i = 0;  i < total_data;  i++)
                image_buf[i] = ~image_buf[i];
            meta->photometric = PHOTOMETRIC_MINISWHITE;
        }
    }

    *buf = image_buf;
    return total_data;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    const char *source_file;
    const char *destination_file;
    TIFF *tif;
    tstrip_t nstrips;
    uint32_t totdata;
    tsize_t off;
    uint8_t *data;
    uint8_t *data2;
    int row;
    int bytes_per_row;
    tsize_t outsize;
    char *outptr;
    int i;
    int k;
    int x;
    int y;
    uint64_t start;
    uint64_t end;
    logging_state_t logging2;
    meta_t in_meta;
    meta_t meta;
    int output_compression;
    int page_no;
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    toff_t diroff;
#endif

    source_file = (argc > 1)  ?  argv[1]  :  IN_FILE_NAME;
    printf("Processing '%s'\n", source_file);
    destination_file = OUT_FILE_NAME;
    output_compression = (argc > 2)  ?  atoi(argv[2]) : COMPRESSION_CCITT_T6;

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    TIFF_FX_init();
#endif

    if ((in_meta.tif = TIFFOpen(source_file, "r")) == NULL)
    {
        printf("Unable to open '%s'!\n", source_file);
        return 1;
    }

    if ((meta.tif = TIFFOpen(destination_file, "w")) == NULL)
    {
        printf("Unable to open '%s'!\n", destination_file);
        return 1;
    }
    span_log_init(&logging2, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW, "lab");

    /* The default luminant is D50 */
    set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
    set_lab_gamut(&lab_param, 0, 100, -85, 85, -75, 125, false);

    outptr = NULL;
    for (page_no = 0;   ;  page_no++)
    {
        if (read_file(&in_meta, page_no) < 0)
        {
            printf("Failed to read from %s\n", source_file);
            TIFFClose(in_meta.tif);
            TIFFClose(meta.tif);
            exit(2);
        }

        tif = in_meta.tif;

        nstrips = TIFFNumberOfStrips(tif);
        if (in_meta.compression == output_compression  &&  nstrips == 1  &&  in_meta.tile_width == 0)
        {
            /* There might be no need to re-compress the image */
        }
        else
        {
            /* It looks like we need to decompress and recompress the image */
        }

        printf("Width %d, height %d, bits %d, samples %d\n", in_meta.image_width, in_meta.image_length, in_meta.bits_per_sample, in_meta.samples_per_pixel);

        totdata = read_decompressed_image(&in_meta, &data);
        off = totdata;

        bytes_per_row = TIFFScanlineSize(tif);

        printf("bits_per_sample %d, samples_per_pixel %d, w %d, h %d\n", in_meta.bits_per_sample, in_meta.samples_per_pixel, in_meta.image_width, in_meta.image_length);


        printf("total %d, off %d\n", totdata, (int) off);

        switch (in_meta.samples_per_pixel)
        {
        case 1:
            if (in_meta.bits_per_sample == 1)
            {
                printf("Bi-level\n");

                /* We have finished acquiring the image. Now we need to push it out */
                meta.pre_compressed = false;
                meta.image_width = in_meta.image_width;
                meta.image_length = in_meta.image_length;
                meta.x_resolution = in_meta.x_resolution;
                meta.y_resolution = in_meta.y_resolution;
                meta.resolution_unit = in_meta.resolution_unit;
                meta.bits_per_sample = in_meta.bits_per_sample;
                meta.samples_per_pixel = in_meta.samples_per_pixel;
                meta.compression = COMPRESSION_CCITT_T6;
                meta.photometric = PHOTOMETRIC_MINISWHITE;

                write_file(&meta, page_no, data);
            }
            else
            {
                printf("Gray scale, %d bits\n", in_meta.bits_per_sample);
                if (in_meta.bits_per_sample == 8)
                {
                    /* Nothing needs to be done */
                }
                else if (in_meta.bits_per_sample == 16)
                {
                    if ((outptr = malloc(in_meta.image_width*in_meta.image_length)) == NULL)
                        printf("Failed to allocate buffer\n");
                    for (i = 0;  i < in_meta.image_width*in_meta.image_length;  i++)
                        outptr[i] = data[2*i];
                    free(data);
                    data = (uint8_t *) outptr;
                }
                else
                {
                    uint32_t bitstream;
                    int bits;
                    int j;

                    /* Deal with the messy cases where the number of bits is not a whole
                       number of bytes. */                
                    if ((outptr = malloc(in_meta.image_width*in_meta.image_length)) == NULL)
                        printf("Failed to allocate buffer\n");
                    bitstream = 0;
                    bits = 0;
                    j = 0;
                    for (i = 0;  i < in_meta.image_width*in_meta.image_length;  i++)
                    {
                        while (bits < in_meta.bits_per_sample)
                        {
                            bitstream = (bitstream << 8) | data[j++];
                            bits += 8;
                        }
                        outptr[i] = bitstream >> (bits - 8);
                        bits -= in_meta.bits_per_sample;
                    }
                    free(data);
                    data = (uint8_t *) outptr;
                }
                off = in_meta.samples_per_pixel*in_meta.image_width*in_meta.image_length;

                /* We have finished acquiring the image. Now we need to push it out */
                meta.pre_compressed = false;
                meta.image_width = in_meta.image_width;
                meta.image_length = in_meta.image_length;
                meta.x_resolution = in_meta.x_resolution;
                meta.y_resolution = in_meta.y_resolution;
                meta.resolution_unit = in_meta.resolution_unit;
                meta.bits_per_sample = 8;
                meta.samples_per_pixel = in_meta.samples_per_pixel;
                meta.compression = COMPRESSION_JPEG;
                meta.photometric = PHOTOMETRIC_MINISBLACK;

                write_file(&meta, page_no, data);
            }
            break;
        case 3:
            printf("Photometric is %d\n", in_meta.photometric);

            /* We now have the image in memory in RGB form */

            if (in_meta.photometric == PHOTOMETRIC_ITULAB)
            {
                printf("ITU Lab\n");
                /* We are already in the ITULAB color space */
                if ((outptr = malloc(totdata)) == NULL)
                    printf("Failed to allocate buffer\n");
                lab_to_srgb(&lab_param, (tdata_t) outptr, data, totdata/3);
                free(data);
                data = (uint8_t *) outptr;

                meta.pre_compressed = false;
                meta.image_width = in_meta.image_width;
                meta.image_length = in_meta.image_length;
                meta.x_resolution = in_meta.x_resolution;
                meta.y_resolution = in_meta.y_resolution;
                meta.resolution_unit = in_meta.resolution_unit;
                meta.bits_per_sample = 8;
                meta.samples_per_pixel = in_meta.samples_per_pixel;
                meta.compression = COMPRESSION_JPEG;
                meta.photometric = PHOTOMETRIC_RGB;
            }
            else
            {
#if 1
                start = rdtscll();
                switch (in_meta.photometric)
                {
                case PHOTOMETRIC_CIELAB:
                    printf("CIELAB\n");
                    /* Convert this to sRGB first */
                    /* The default luminant is D50 */
                    set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
                    set_lab_gamut(&lab_param, 0, 100, -128, 127, -128, 127, true);
                    lab_to_srgb(&lab_param, data, data, in_meta.image_width*in_meta.image_length);
                    break;
                case PHOTOMETRIC_RGB:
                    printf("RGB\n");
                    if (in_meta.bits_per_sample == 8)
                    {
                    }
                    else if (in_meta.bits_per_sample == 16)
                    {
                        printf("Pack %d to %d\n", totdata, in_meta.samples_per_pixel*in_meta.image_width*in_meta.image_length);
                        if ((outptr = malloc(in_meta.samples_per_pixel*in_meta.image_width*in_meta.image_length)) == NULL)
                            printf("Failed to allocate buffer\n");
                        for (i = 0;  i < in_meta.image_width*in_meta.image_length;  i++)
                        {
                            outptr[in_meta.samples_per_pixel*i + 0] = (data[in_meta.samples_per_pixel*2*i + 1] << 4) | (data[in_meta.samples_per_pixel*2*i + 0] >> 4);
                            outptr[in_meta.samples_per_pixel*i + 1] = (data[in_meta.samples_per_pixel*2*i + 3] << 4) | (data[in_meta.samples_per_pixel*2*i + 2] >> 4);
                            outptr[in_meta.samples_per_pixel*i + 2] = (data[in_meta.samples_per_pixel*2*i + 5] << 4) | (data[in_meta.samples_per_pixel*2*i + 4] >> 4);
                        }
                        free(data);
                        data = (uint8_t *) outptr;
                        off = in_meta.samples_per_pixel*in_meta.image_width*in_meta.image_length;
                        in_meta.bits_per_sample = 8;
                    }
                    else
                    {
                        uint32_t bitstream;
                        int bits;
                        int j;

                        /* Deal with the messy cases where the number of bits is not a whole number of bytes. */                
                        printf("Pack %d to %d\n", totdata, in_meta.samples_per_pixel*in_meta.image_width*in_meta.image_length);
                        if ((outptr = malloc(in_meta.samples_per_pixel*in_meta.image_width*in_meta.image_length)) == NULL)
                            printf("Failed to allocate buffer\n");
                        bitstream = 0;
                        bits = 0;
                        j = 0;
                        for (i = 0;  i < in_meta.image_width*in_meta.image_length;  i++)
                        {
                            for (k = 0;  k < in_meta.samples_per_pixel;  k++)
                            {
                                while (bits < in_meta.bits_per_sample)
                                {
                                    bitstream = (bitstream << 8) | data[j++];
                                    bits += 8;
                                }
                                outptr[in_meta.samples_per_pixel*i + k] = bitstream >> (bits - 8);
                                bits -= in_meta.bits_per_sample;
                            }
                        }
                        free(data);
                        data = (uint8_t *) outptr;
                        off = in_meta.samples_per_pixel*in_meta.image_width*in_meta.image_length;
                        in_meta.bits_per_sample = 8;
                    }
                    break;
                }
#if 0
                /* The default luminant is D50 */
                set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
                set_lab_gamut(&lab_param, 0, 100, -85, 85, -75, 125, false);
                if (!t42_srgb_to_itulab_jpeg(&logging2, &lab_param, (tdata_t) &outptr, &outsize, data, off, in_meta.image_width, in_meta.image_length, 3))
                {
                    printf("Failed to convert to ITULAB (B).\n");
                    return 1;
                }
                end = rdtscll();
                printf("Duration %" PRIu64 "\n", end - start);
                free(data);
                data = (uint8_t *) outptr;
                off = outsize;
#endif
#endif
                meta.pre_compressed = false;
                meta.image_width = in_meta.image_width;
                meta.image_length = in_meta.image_length;
                meta.x_resolution = in_meta.x_resolution;
                meta.y_resolution = in_meta.y_resolution;
                meta.resolution_unit = in_meta.resolution_unit;
                meta.bits_per_sample = 8;
                meta.samples_per_pixel = in_meta.samples_per_pixel;
                meta.compression = COMPRESSION_JPEG;
                meta.photometric = PHOTOMETRIC_RGB;
            }
            write_file(&meta, page_no, data);
            break;
        case 4:
            printf("Photometric is %d\n", in_meta.photometric);

            /* We now have the image in memory in RGB form */

            if (in_meta.photometric == PHOTOMETRIC_ITULAB)
            {
                /* We are already in the ITULAB color space */
#if 0
                if (!t42_itulab_to_itulab(&logging2, (tdata_t) &outptr, &outsize, data, off, in_meta.image_width, in_meta.image_length, 3))
                {
                    printf("Failed to convert to ITULAB (C).\n");
                    return 1;
                }
#else
                outsize = 0;
#endif
                free(data);
                data = (uint8_t *) outptr;
                off = outsize;
            }
            else
            {
                start = rdtscll();
                switch (in_meta.photometric)
                {
                case PHOTOMETRIC_CIELAB:
                    printf("CIELAB\n");
                    /* TODO: This doesn't work yet */
                    /* Convert this to sRGB first */
                    /* The default luminant is D50 */
                    set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
                    set_lab_gamut(&lab_param, 0, 100, -128, 127, -128, 127, true);
                    lab_to_srgb(&lab_param, data, data, in_meta.image_width*in_meta.image_length);
                    break;
                case PHOTOMETRIC_SEPARATED:
                    for (y = 0;  y < in_meta.image_length;  y++)
                    {
                        for (x = 0;  x < in_meta.image_width;  x++)
                        {
                            k = data[(y*in_meta.image_width + x)*4 + 0] + data[(y*in_meta.image_width + x)*4 + 3];
                            if (k > 255)
                                k = 255;
                            data[(y*in_meta.image_width + x)*3 + 0] = 255 - k;
                            k = data[(y*in_meta.image_width + x)*4 + 1] + data[(y*in_meta.image_width + x)*4 + 3];
                            if (k > 255)
                                k = 255;
                            data[(y*in_meta.image_width + x)*3 + 1] = 255 - k;
                            k = data[(y*in_meta.image_width + x)*4 + 2] + data[(y*in_meta.image_width + x)*4 + 3];
                            if (k > 255)
                                k = 255;
                            data[(y*in_meta.image_width + x)*3 + 2] = 255 - k;
                        }
                    }
                    off = 3*in_meta.image_width*in_meta.image_length;
                    in_meta.bits_per_sample = 8;
                    break;
                }

                /* The default luminant is D50 */
                set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
                set_lab_gamut(&lab_param, 0, 100, -85, 85, -75, 125, false);
                //if (!t42_srgb_to_itulab_jpeg(&logging2, &lab_param, (tdata_t) &outptr, &outsize, data, off, in_meta.image_width, in_meta.image_length, 3))
                {
                    printf("Failed to convert to ITULAB (D).\n");
                    return 1;
                }
                end = rdtscll();
                printf("Duration %" PRIu64 "\n", end - start);
                off = outsize;
                in_meta.bits_per_sample = 8;
            }
            meta.pre_compressed = false;
            meta.image_width = in_meta.image_width;
            meta.image_length = in_meta.image_length;
            meta.x_resolution = in_meta.x_resolution;
            meta.y_resolution = in_meta.y_resolution;
            meta.resolution_unit = in_meta.resolution_unit;
            meta.bits_per_sample = 8;
            meta.samples_per_pixel = 3;
            meta.compression = COMPRESSION_JPEG;
            meta.photometric = PHOTOMETRIC_RGB;

            write_file(&meta, page_no, data);
            break;
        }
    }



    printf("XXX - image is %d by %d, %d bytes\n", in_meta.image_width, in_meta.image_length, (int) off);

    /* We now have the image in memory in ITULAB form */

    meta.pre_compressed = false;
    meta.compressed_image_len = off;
    meta.image_width = in_meta.image_width;
    meta.image_length = in_meta.image_length;
    meta.x_resolution = in_meta.x_resolution;
    meta.y_resolution = in_meta.y_resolution;
    meta.resolution_unit = in_meta.resolution_unit;
    meta.bits_per_sample = 8;
    meta.samples_per_pixel = 3;
    meta.compression = COMPRESSION_JPEG;
#if 1
    meta.photometric = PHOTOMETRIC_RGB;
#elif 1
    /* Most image processors won't know what to do with the ITULAB colorspace.
       So we'll be converting it to RGB for portability. */
    /* If PHOTOMETRIC_ITULAB is not available the admin cannot enable color fax anyway.
       This is done so that older libtiffs without it can build fine. */
    meta.photometric = PHOTOMETRIC_ITULAB;
#else
    meta.photometric = PHOTOMETRIC_YCBCR;
#endif
    meta.YCbCrSubsampleHoriz = in_meta.YCbCrSubsampleHoriz;
    meta.YCbCrSubsampleVert = in_meta.YCbCrSubsampleVert;

    if ((tif = TIFFOpen(destination_file, "w")) == NULL)
    {
        printf("Unable to open '%s'!\n", destination_file);
        return 1;
    }
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, FILETYPE_PAGE);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, meta.image_width);
    /* libtiff requires IMAGELENGTH to be set before SAMPLESPERPIXEL,
       or StripOffsets and StripByteCounts will have SAMPLESPERPIXEL values */
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, meta.image_length);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, meta.compression);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, meta.bits_per_sample);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, meta.samples_per_pixel);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, meta.image_length);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, meta.x_resolution);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, meta.y_resolution);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, meta.resolution_unit);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, meta.photometric);
    if (meta.samples_per_pixel > 1  &&  (meta.YCbCrSubsampleHoriz  ||  meta.YCbCrSubsampleVert))
        TIFFSetField(tif, TIFFTAG_YCBCRSUBSAMPLING, meta.YCbCrSubsampleHoriz, meta.YCbCrSubsampleVert);
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "spandsp");
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, "Test");
    TIFFSetField(tif, TIFFTAG_DATETIME, "2011/02/03 12:30:45");
    TIFFSetField(tif, TIFFTAG_MAKE, "soft-switch.org");
    TIFFSetField(tif, TIFFTAG_MODEL, "spandsp");
    TIFFSetField(tif, TIFFTAG_HOSTCOMPUTER, "i7.coppice.org");
#if defined(SPANDSP_SUPPORT_TIFF_FX)
    /* Make space for this to be filled in later */
    TIFFSetField(tif, TIFFTAG_GLOBALPARAMETERSIFD, 0);
#endif

    if (meta.pre_compressed)
    {
        if (TIFFWriteRawStrip(tif, 0, (tdata_t) data, meta.compressed_image_len) == -1)
        {
            printf("Write error to TIFF file\n");
            return 1;
        }
        free(data);
    }
    else
    {
        if (in_meta.samples_per_pixel > 1)
        {
            bytes_per_row = ((meta.bits_per_sample + 7)/8)*meta.image_width*meta.samples_per_pixel;
            totdata = meta.image_length*bytes_per_row;
            /* The default luminant is D50 */
            set_lab_illuminant(&lab_param, 96.422f, 100.000f,  82.521f);
            set_lab_gamut(&lab_param, 0, 100, -85, 85, -75, 125, false);
#if 0
            start = rdtscll();
            data2 = NULL;
            totdata = 0;
            if (!t42_itulab_to_JPEG(&logging2, &lab_param, (void **) &data2, &totdata, data, off))
            {
                printf("Failed to convert from ITULAB (A).\n");
                return 1;
            }
            end = rdtscll();
            printf("Duration %" PRIu64 "\n", end - start);
            printf("Compressed length %d (%p)\n", totdata, data2);
            if (TIFFWriteRawStrip(tif, 0, data2, totdata) < 0)
            {
                printf("Failed to convert from ITULAB (B).\n");
                return 1;
            }
            free(data);
            data = data2;
#elif 1
            data2 = malloc(totdata);
            start = rdtscll();
            //if (!t42_itulab_jpeg_to_srgb(&logging2, &lab_param, data2, &off, data, off, &meta.image_width, &meta.image_length, &meta.samples_per_pixel))
            {
                printf("Failed to convert from ITULAB.\n");
                return 1;
            }
            end = rdtscll();
            printf("Duration %" PRIu64 "\n", end - start);
            free(data);
            data = data2;
#endif
        }
        off = 0;
        bytes_per_row = ((meta.bits_per_sample + 7)/8)*meta.image_width*meta.samples_per_pixel;
        for (row = 0;  row < meta.image_length;  row++)
        {
            if (TIFFWriteScanline(tif, &data[off], row, 0) < 0)
                return 1;
            off += bytes_per_row;
        }
        free(data);
    }

    if (!TIFFWriteDirectory(tif))
        printf("Failed to write directory.\n");

#if defined(SPANDSP_SUPPORT_TIFF_FX)
    if (!TIFFCreateCustomDirectory(tif, &tiff_fx_field_array))
    {
        TIFFSetField(tif, TIFFTAG_PROFILETYPE, PROFILETYPE_G3_FAX);
        TIFFSetField(tif, TIFFTAG_FAXPROFILE, FAXPROFILE_F);
        TIFFSetField(tif, TIFFTAG_CODINGMETHODS, CODINGMETHODS_T4_1D | CODINGMETHODS_T4_2D | CODINGMETHODS_T6);
        TIFFSetField(tif, TIFFTAG_VERSIONYEAR, "1998");
        TIFFSetField(tif, TIFFTAG_MODENUMBER, 3);

        diroff = 0;
        if (!TIFFWriteCustomDirectory(tif, &diroff))
            printf("Failed to write custom directory.\n");

        if (!TIFFSetDirectory(tif, (tdir_t) page_no))
            printf("Failed to set directory.\n");
        if (!TIFFSetField(tif, TIFFTAG_GLOBALPARAMETERSIFD, diroff))
            printf("Failed to set global parameters IFD.\n");
        if (!TIFFWriteDirectory(tif))
            printf("Failed to write directory.\n");
    }
#endif
    TIFFClose(tif);
    printf("Done!\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

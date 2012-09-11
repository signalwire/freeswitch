/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t42.c - ITU T.42 JPEG for FAX image processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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
#include <assert.h>
#include <jpeglib.h>
#include <setjmp.h>

#include "spandsp/telephony.h"
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/saturated.h"
#include "spandsp/async.h"
#include "spandsp/timezone.h"
#include "spandsp/t4_rx.h"
#include "spandsp/t4_tx.h"
#include "spandsp/t81_t82_arith_coding.h"
#include "spandsp/t85.h"
#include "spandsp/t42.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/t81_t82_arith_coding.h"
#include "spandsp/private/t85.h"
#include "spandsp/private/t42.h"

#define T42_USE_LUTS

#include "t42_t43_local.h"
#if defined(T42_USE_LUTS)
#include "cielab_luts.h"
#endif

typedef struct
{
    float L;
    float a;
    float b;
} cielab_t;

typedef struct
{
    uint8_t tag[5];
    const char *name;
    float xn;
    float yn;
    float zn;
} illuminant_t;

typedef struct
{
    jmp_buf escape;
    char error_message[JMSG_LENGTH_MAX];
} escape_route_t;

static const illuminant_t illuminants[] =
{
    {"\0D50",  "CIE D50/2°",   96.422f, 100.000f,  82.521f},
    {"",       "CIE D50/10°",  96.720f, 100.000f,  81.427f},
    {"",       "CIE D55/2°",   95.682f, 100.000f,  92.149f},
    {"",       "CIE D55/10°",  95.799f, 100.000f,  90.926f},
    {"\0D65",  "CIE D65/2°",   95.047f, 100.000f, 108.883f},
    {"",       "CIE D65/10°",  94.811f, 100.000f, 107.304f},
    {"\0D75",  "CIE D75/2°",   94.972f, 100.000f, 122.638f},
    {"",       "CIE D75/10°",  94.416f, 100.000f, 120.641f},
    {"\0\0F2", "F02/2°",       99.186f, 100.000f,  67.393f},
    {"",       "F02/10°",     103.279f, 100.000f,  69.027f},
    {"\0\0F7", "F07/2°",       95.041f, 100.000f, 108.747f},
    {"",       "F07/10°",      95.792f, 100.000f, 107.686f},
    {"\0F11",  "F11/2°",      100.962f, 100.000f,  64.350f},
    {"",       "F11/10°",     103.863f, 100.000f,  65.607f},
    {"\0\0SA", "A/2°",        109.850f, 100.000f,  35.585f},
    {"",       "A/10°",       111.144f, 100.000f,  35.200f},
    {"\0\0SC", "C/2°",         98.074f, 100.000f, 118.232f},
    {"",       "C/10°",        97.285f, 100.000f, 116.145f},
    {"",       "",              0.000f,   0.000f,   0.000f}
};

/* This is the error catcher */
static struct jpeg_error_mgr error_handler;

static __inline__ uint16_t pack_16(const uint8_t *s)
{
    uint16_t value;

    value = ((uint16_t) s[0] << 8) | (uint16_t) s[1];
    return value;
}
/*- End of function --------------------------------------------------------*/

static __inline__ uint32_t pack_32(const uint8_t *s)
{
    uint32_t value;

    value = ((uint32_t) s[0] << 24) | ((uint32_t) s[1] << 16) | ((uint32_t) s[2] << 8) | (uint32_t) s[3];
    return value;
}
/*- End of function --------------------------------------------------------*/

static __inline__ int unpack_16(uint8_t *s, uint16_t value)
{
    s[0] = (value >> 8) & 0xFF;
    s[1] = value & 0xFF;
    return sizeof(uint16_t);
}
/*- End of function --------------------------------------------------------*/

/* Error handler for IJG library */
static void jpg_error_exit(j_common_ptr cinfo)
{
    escape_route_t *escape;

    escape = (escape_route_t *) cinfo->client_data;
    (*cinfo->err->format_message)(cinfo, escape->error_message);
    longjmp(escape->escape, 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) set_lab_illuminant(lab_params_t *s, float new_xn, float new_yn, float new_zn)
{
    if (new_yn > 10.0f)
    {
        s->x_n = new_xn/100.0f;
        s->y_n = new_yn/100.0f;
        s->z_n = new_zn/100.0f;
    }
    else
    {
        s->x_n = new_xn;
        s->y_n = new_yn;
        s->z_n = new_zn;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) set_lab_gamut(lab_params_t *s, int L_min, int L_max, int a_min, int a_max, int b_min, int b_max, int ab_are_signed)
{
    s->range_L = L_max - L_min;
    s->range_a = a_max - a_min;
    s->range_b = b_max - b_min;

    s->offset_L = -256.0f*L_min/s->range_L;
    s->offset_a = -256.0f*a_min/s->range_a;
    s->offset_b = -256.0f*b_min/s->range_b;

    s->range_L /= (256.0f - 1.0f);
    s->range_a /= (256.0f - 1.0f);
    s->range_b /= (256.0f - 1.0f);

    s->ab_are_signed = ab_are_signed;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) set_lab_gamut2(lab_params_t *s, int L_P, int L_Q, int a_P, int a_Q, int b_P, int b_Q)
{
    s->range_L = L_Q/(256.0f - 1.0f);
    s->range_a = a_Q/(256.0f - 1.0f);
    s->range_b = b_Q/(256.0f - 1.0f);

    s->offset_L = L_P;
    s->offset_a = a_P;
    s->offset_b = b_P;

    s->ab_are_signed = FALSE;
}
/*- End of function --------------------------------------------------------*/

void set_illuminant_from_code(logging_state_t *logging, lab_params_t *s, const uint8_t code[4])
{
    int i;
    int colour_temp;

    if (code[0] == 'C'  &&  code[1] == 'T')
    {
        colour_temp = pack_16(&code[2]);
        span_log(logging, SPAN_LOG_FLOW, "Illuminant colour temp %dK\n", colour_temp);
        return;
    }
    for (i = 0;  illuminants[i].name[0];  i++)
    {
        if (memcmp(code, illuminants[i].tag, 4) == 0)
        {
            span_log(logging, SPAN_LOG_FLOW, "Illuminant %s\n", illuminants[i].name);
            set_lab_illuminant(s, illuminants[i].xn, illuminants[i].yn, illuminants[i].zn);
            break;
        }
    }
    if (illuminants[i].name[0] == '\0')
        span_log(logging, SPAN_LOG_FLOW, "Unrecognised illuminant 0x%x 0x%x 0x%x 0x%x\n", code[0], code[1], code[2], code[3]);
}
/*- End of function --------------------------------------------------------*/

void set_gamut_from_code(logging_state_t *logging, lab_params_t *s, const uint8_t code[12])
{
    int i;
    int val[6];

    for (i = 0;  i < 6;  i++)
        val[i] = pack_16(&code[2*i]);
    span_log(logging,
             SPAN_LOG_FLOW,
             "Gamut L=[%d,%d], a*=[%d,%d], b*=[%d,%d]\n",
             val[0],
             val[1],
             val[2],
             val[3],
             val[4],
             val[5]);
    set_lab_gamut2(s, val[0], val[1], val[2], val[3], val[4], val[5]);
}
/*- End of function --------------------------------------------------------*/

static int is_itu_fax(logging_state_t *logging, lab_params_t *s, jpeg_saved_marker_ptr ptr)
{
    const uint8_t *data;
    int ok;
    int val[2];
    int i;

    ok = FALSE;
    while (ptr)
    {
        if (ptr->marker == (JPEG_APP0 + 1)  &&  ptr->data_length >= 6)
        {
            /* Markers are:
                JPEG_RST0
                JPEG_EOI
                JPEG_APP0
                JPEG_COM */
            data = (const uint8_t *) ptr->data;
            if (strncmp((const char *) data, "G3FAX", 5) == 0)
            {
                switch (data[5])
                {
                case 0:
                    for (i = 0;  i < 2;  i++)
                        val[i] = pack_16(&data[6 + 2*i]);
                    span_log(logging, SPAN_LOG_FLOW, "Version %d, resolution %d dpi\n", val[0], val[1]);
                    ok = TRUE;
                    break;
                case 1:
                    span_log(logging, SPAN_LOG_FLOW, "Set gamut\n");
                    if (ptr->data_length >= 6 + 12)
                    {
                        set_gamut_from_code(logging, s, &data[6]);
                        ok = TRUE;
                    }
                    else
                    {
                        span_log(logging, SPAN_LOG_FLOW, "Got bad G3FAX1 length - %d\n", ptr->data_length);
                    }
                    break;
                case 2:
                    span_log(logging, SPAN_LOG_FLOW, "Set illuminant\n");
                    if (ptr->data_length >= 6 + 4)
                    {
                        set_illuminant_from_code(logging, s, &data[6]);
                        ok = TRUE;
                    }
                    else
                    {
                        span_log(logging, SPAN_LOG_FLOW, "Got bad G3FAX2 length - %d\n", ptr->data_length);
                    }
                    break;
                case 3:
                    /* Colour palette table */
                    span_log(logging, SPAN_LOG_FLOW, "Set colour palette\n");
                    if (ptr->data_length >= 6 + 2)
                    {
                        val[0] = pack_16(&data[6]);
                        span_log(logging, SPAN_LOG_FLOW, "Colour palette %d\n", val[0]);
                    }
                    else
                    {
                        span_log(logging, SPAN_LOG_FLOW, "Got bad G3FAX3 length - %d\n", ptr->data_length);
                    }
                    break;
                default:
                    span_log(logging, SPAN_LOG_FLOW, "Got unexpected G3FAX%d length - %d\n", data[5], ptr->data_length);
                    break;
                }
            }
        }
        else
        {
            span_log(logging, SPAN_LOG_FLOW, "Got marker 0x%x, length %d\n", ptr->marker, ptr->data_length);
            span_log_buf(logging, SPAN_LOG_FLOW, "Got marker", (const uint8_t *) ptr->data, ptr->data_length);
        }
        ptr = ptr->next;
    }

    return ok;
}
/*- End of function --------------------------------------------------------*/

static void set_itu_fax(j_compress_ptr cinfo)
{
    uint8_t marker[10] =
    {
        'G', '3', 'F', 'A', 'X', '\x00', '\x07', '\xCA', '\x00', '\x00'
    };

    unpack_16(marker + 8, 200);

    jpeg_write_marker(cinfo, (JPEG_APP0 + 1), marker, 10);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void itu_to_lab(lab_params_t *s, cielab_t *lab, const uint8_t in[3])
{
    uint8_t a;
    uint8_t b;

    /* T.4 E.6.4 */
    lab->L = s->range_L*(in[0] - s->offset_L);
    a = in[1];
    b = in[2];
    if (s->ab_are_signed)
    {
        a += 128;
        b += 128;
    }
    lab->a = s->range_a*(a - s->offset_a);
    lab->b = s->range_b*(b - s->offset_b);
}
/*- End of function --------------------------------------------------------*/

static __inline__ void lab_to_itu(lab_params_t *s, uint8_t out[3], const cielab_t *lab)
{
    /* T.4 E.6.4 */
    out[0] = saturateu8(floorf(lab->L/s->range_L + s->offset_L));
    out[1] = saturateu8(floorf(lab->a/s->range_a + s->offset_a));
    out[2] = saturateu8(floorf(lab->b/s->range_b + s->offset_b));
    if (s->ab_are_signed)
    {
        out[1] -= 128;
        out[2] -= 128;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) srgb_to_lab(lab_params_t *s, uint8_t lab[], const uint8_t srgb[], int pixels)
{
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float xx;
    float yy;
    float zz;
    cielab_t l;
    int i;

    for (i = 0;  i < 3*pixels;  i += 3)
    {
#if defined(T42_USE_LUTS)
        r = srgb_to_linear[srgb[i]];
        g = srgb_to_linear[srgb[i + 1]];
        b = srgb_to_linear[srgb[i + 2]];
#else
        r = srgb[i]/256.0f;
        g = srgb[i + 1]/256.0f;
        b = srgb[i + 2]/256.0f;

        /* sRGB to linear RGB */
        r = (r > 0.04045f)  ?  powf((r + 0.055f)/1.055f, 2.4f)  :  r/12.92f;
        g = (g > 0.04045f)  ?  powf((g + 0.055f)/1.055f, 2.4f)  :  g/12.92f;
        b = (b > 0.04045f)  ?  powf((b + 0.055f)/1.055f, 2.4f)  :  b/12.92f;
#endif

        /* Linear RGB to XYZ */
        x = 0.4124f*r + 0.3576f*g + 0.1805f*b;
        y = 0.2126f*r + 0.7152f*g + 0.0722f*b;
        z = 0.0193f*r + 0.1192f*g + 0.9505f*b;

        /* Normalise for the illuminant */
        x /= s->x_n;
        y /= s->y_n;
        z /= s->z_n;
    
        /* XYZ to Lab */
        xx = (x <= 0.008856f)  ?  (7.787f*x + 0.1379f)  :  cbrtf(x);
        yy = (y <= 0.008856f)  ?  (7.787f*y + 0.1379f)  :  cbrtf(y);
        zz = (z <= 0.008856f)  ?  (7.787f*z + 0.1379f)  :  cbrtf(z);
        l.L = 116.0f*yy - 16.0f;
        l.a = 500.0f*(xx - yy);
        l.b = 200.0f*(yy - zz);

        lab_to_itu(s, lab, &l);

        lab += 3;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) lab_to_srgb(lab_params_t *s, uint8_t srgb[], const uint8_t lab[], int pixels)
{
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float ll;
    cielab_t l;
    int val;
    int i;

    for (i = 0;  i < 3*pixels;  i += 3)
    {
        itu_to_lab(s, &l, lab);

        /* Lab to XYZ */
        ll = (1.0f/116.0f)*(l.L + 16.0f); 
        y = ll;
        y = (y <= 0.2068f)  ?  (0.1284f*(y - 0.1379f))  :  y*y*y;
        x = ll + (1.0f/500.0f)*l.a;
        x = (x <= 0.2068f)  ?  (0.1284f*(x - 0.1379f))  :  x*x*x;
        z = ll - (1.0f/200.0f)*l.b;
        z = (z <= 0.2068f)  ?  (0.1284f*(z - 0.1379f))  :  z*z*z;

        /* Normalise for the illuminant */
        x *= s->x_n;
        y *= s->y_n;
        z *= s->z_n;

        /* XYZ to linear RGB */
        r =  3.2406f*x - 1.5372f*y - 0.4986f*z;
        g = -0.9689f*x + 1.8758f*y + 0.0415f*z;
        b =  0.0557f*x - 0.2040f*y + 1.0570f*z;

#if defined(T42_USE_LUTS)
        val = r*4096.0f;
        srgb[i] = linear_to_srgb[(val < 0)  ?  0  :  (val < 4095)  ?  val  :  4095];
        val = g*4096.0f;
        srgb[i + 1] = linear_to_srgb[(val < 0)  ?  0  :  (val < 4095)  ?  val  :  4095];
        val = b*4096.0f;
        srgb[i + 2] = linear_to_srgb[(val < 0)  ?  0  :  (val < 4095)  ?  val  :  4095];
#else
        /* Linear RGB to sRGB */
        r = (r > 0.0031308f)  ?  (1.055f*powf(r, 1.0f/2.4f) - 0.055f)  :  r*12.92f;
        g = (g > 0.0031308f)  ?  (1.055f*powf(g, 1.0f/2.4f) - 0.055f)  :  g*12.92f;
        b = (b > 0.0031308f)  ?  (1.055f*powf(b, 1.0f/2.4f) - 0.055f)  :  b*12.92f;

        srgb[i] = saturateu8(floorf(r*256.0f));
        srgb[i + 1] = saturateu8(floorf(g*256.0f));
        srgb[i + 2] = saturateu8(floorf(b*256.0f));
#endif
        lab += 3;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_itulab_to_jpeg(logging_state_t *logging, lab_params_t *s, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen)
{
    struct jpeg_decompress_struct decompressor;
    struct jpeg_compress_struct compressor;
    FILE *in;
    FILE *out;
    int m;
    JSAMPROW scan_line_in;
    JSAMPROW scan_line_out;
    escape_route_t escape;
#if defined(HAVE_OPEN_MEMSTREAM)
    char *outptr;
    size_t outsize;
#endif

    escape.error_message[0] = '\0';

#if defined(HAVE_OPEN_MEMSTREAM)
    if ((in = fmemopen(src, srclen, "r")) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to fmemopen().\n");
        return FALSE;
    }
    outsize = 0;
    if ((out = open_memstream(&outptr, &outsize)) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to open_memstream().\n");
        fclose(in);
        return FALSE;
    }
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(in);
        fclose(out);
        return FALSE;
    }
#else
    if ((in = tmpfile()) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        return FALSE;
    }
    if (fwrite(src, 1, srclen, in) != srclen)
    {
        fclose(in);
        return FALSE;
    }
    if (fseek(in, 0, SEEK_SET) != 0)
    {
        fclose(in);
        return FALSE;
    }
    if ((out = tmpfile()) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        fclose(in);
        return FALSE;
    }
#endif
    scan_line_out = NULL;

    if (setjmp(escape.escape))
    {
        if (escape.error_message[0])
            span_log(logging, SPAN_LOG_FLOW, "%s\n", escape.error_message);
        else
            span_log(logging, SPAN_LOG_FLOW, "Unspecified libjpeg error.\n");
        if (scan_line_out)
            free(scan_line_out);
        fclose(in);
        fclose(out);
        return FALSE;
    }

    /* Create input decompressor. */
    decompressor.err = jpeg_std_error(&error_handler);
    decompressor.client_data = (void *) &escape;
    error_handler.error_exit = jpg_error_exit;
    error_handler.output_message = jpg_error_exit;

    jpeg_create_decompress(&decompressor);
    jpeg_stdio_src(&decompressor, in);

    /* Needed in the case of ITU Lab input */
    for (m = 0;  m < 16;  m++)
        jpeg_save_markers(&decompressor, JPEG_APP0 + m, 0xFFFF);

    /* Rewind the file */
    if (fseek(in, 0, SEEK_SET) != 0)
        return FALSE;

    /* Take the header */
    jpeg_read_header(&decompressor, TRUE);

    /* Now we can force the input colour space. For ITULab, we use YCbCr as a "don't touch" marker */
    decompressor.out_color_space = JCS_YCbCr;

    /* Sanity check and parameter check */
    if (!is_itu_fax(logging, s, decompressor.marker_list))
    {
        span_log(logging, SPAN_LOG_FLOW, "Is not an ITU FAX.\n");
        return FALSE;
    }

    /* Create compressor */
    compressor.err = jpeg_std_error(&error_handler);
    compressor.client_data = (void *) &escape;
    error_handler.error_exit = jpg_error_exit;
    error_handler.output_message = jpg_error_exit;

    jpeg_create_compress(&compressor);
    jpeg_stdio_dest(&compressor, out);

    /* Force the destination colour space */
    compressor.in_color_space = JCS_RGB;
    compressor.input_components = 3;

    jpeg_set_defaults(&compressor);
    /* Limit to baseline-JPEG values */
    //jpeg_set_quality(&compressor, quality, TRUE);

    /* Copy size, resolution, etc */
    jpeg_copy_critical_parameters(&decompressor, &compressor);

    /* We need to keep these */
    compressor.density_unit = decompressor.density_unit;
    compressor.X_density = decompressor.X_density;
    compressor.Y_density = decompressor.Y_density;

    jpeg_start_decompress(&decompressor);
    jpeg_start_compress(&compressor, TRUE);

    if ((scan_line_in = (JSAMPROW) malloc(decompressor.output_width*decompressor.num_components)) == NULL)
        return FALSE;

    if ((scan_line_out = (JSAMPROW) malloc(compressor.image_width*compressor.num_components)) == NULL)
    {
        free(scan_line_in);
        return FALSE;
    }

    while (decompressor.output_scanline < decompressor.output_height)
    {
        jpeg_read_scanlines(&decompressor, &scan_line_in, 1);
        lab_to_srgb(s, scan_line_out, scan_line_in, decompressor.output_width);
        jpeg_write_scanlines(&compressor, &scan_line_out, 1);
    }
    free(scan_line_in);
    free(scan_line_out);
    jpeg_finish_decompress(&decompressor);
    jpeg_finish_compress(&compressor);
    jpeg_destroy_decompress(&decompressor);
    jpeg_destroy_compress(&compressor);
    fclose(in);

#if defined(HAVE_OPEN_MEMSTREAM)
    fclose(out);
    *dst = outptr;
    *dstlen = outsize;
#else
    *dstlen = ftell(out);
    *dst = malloc(*dstlen);
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(out);
        return FALSE;
    }
    if (fread(*dst, 1, *dstlen, out) != *dstlen)
    {
        free(*dst);
        fclose(out);
        return FALSE;
    }    
    fclose(out);
#endif

    return TRUE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_jpeg_to_itulab(logging_state_t *logging, lab_params_t *s, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen)
{
    struct jpeg_decompress_struct decompressor;
    struct jpeg_compress_struct compressor;
    FILE *in;
    FILE *out;
    int m;
    JSAMPROW scan_line_in;
    JSAMPROW scan_line_out;
    escape_route_t escape;
#if defined(HAVE_OPEN_MEMSTREAM)
    char *outptr;
    size_t outsize;
#endif

    escape.error_message[0] = '\0';

#if defined(HAVE_OPEN_MEMSTREAM)
    if ((in = fmemopen(src, srclen, "r")) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to fmemopen().\n");
        return FALSE;
    }
    outsize = 0;
    if ((out = open_memstream(&outptr, &outsize)) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to open_memstream().\n");
        fclose(in);
        return FALSE;
    }
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(in);
        fclose(out);
        return FALSE;
    }
#else
    if ((in = tmpfile()) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        return FALSE;
    }
    if (fwrite(src, 1, srclen, in) != srclen)
    {
        fclose(in);
        return FALSE;
    }
    if (fseek(in, 0, SEEK_SET) != 0)
    {
        fclose(in);
        return FALSE;
    }
    if ((out = tmpfile()) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        fclose(in);
        return FALSE;
    }
#endif
    scan_line_out = NULL;

    if (setjmp(escape.escape))
    {
        if (escape.error_message[0])
            span_log(logging, SPAN_LOG_FLOW, "%s\n", escape.error_message);
        else
            span_log(logging, SPAN_LOG_FLOW, "Unspecified libjpeg error.\n");
        if (scan_line_out)
            free(scan_line_out);
        fclose(in);
        fclose(out);
        return FALSE;
    }
    /* Create input decompressor. */
    decompressor.err = jpeg_std_error(&error_handler);
    decompressor.client_data = (void *) &escape;
    error_handler.error_exit = jpg_error_exit;
    error_handler.output_message = jpg_error_exit;

    jpeg_create_decompress(&decompressor);
    jpeg_stdio_src(&decompressor, in);

    /* Needed in the case of ITU Lab input */
    for (m = 0;  m < 16;  m++)
        jpeg_save_markers(&decompressor, JPEG_APP0 + m, 0xFFFF);

    /* Rewind the file */
    if (fseek(in, 0, SEEK_SET) != 0)
        return FALSE;

    /* Take the header */
    jpeg_read_header(&decompressor, TRUE);

    /* Now we can force the input colour space. For ITULab, we use YCbCr as a "don't touch" marker */
    decompressor.out_color_space = JCS_RGB;

    compressor.err = jpeg_std_error(&error_handler);
    compressor.client_data = (void *) &escape;
    error_handler.error_exit = jpg_error_exit;
    error_handler.output_message = jpg_error_exit;

    jpeg_create_compress(&compressor);
    jpeg_stdio_dest(&compressor, out);

    /* Force the destination colour space */
    compressor.in_color_space = JCS_YCbCr;
    compressor.input_components = 3;

    jpeg_set_defaults(&compressor);
    /* Limit to baseline-JPEG values */
    //jpeg_set_quality(&compressor, quality, TRUE);

    jpeg_copy_critical_parameters(&decompressor, &compressor);

    /* We need to keep these */
    compressor.density_unit = decompressor.density_unit;
    compressor.X_density = decompressor.X_density;
    compressor.Y_density = decompressor.Y_density;

    jpeg_start_decompress(&decompressor);
    jpeg_start_compress(&compressor, TRUE);

    set_itu_fax(&compressor);

    if ((scan_line_in = (JSAMPROW) malloc(decompressor.output_width*decompressor.num_components)) == NULL)
        return FALSE;

    if ((scan_line_out = (JSAMPROW) malloc(compressor.image_width*compressor.num_components)) == NULL)
    {
        free(scan_line_in);
        return FALSE;
    }

    while (decompressor.output_scanline < decompressor.output_height)
    {
        jpeg_read_scanlines(&decompressor, &scan_line_in, 1);
        srgb_to_lab(s, scan_line_out, scan_line_in, decompressor.output_width);
        jpeg_write_scanlines(&compressor, &scan_line_out, 1);
    }

    free(scan_line_in);
    free(scan_line_out);
    jpeg_finish_decompress(&decompressor);
    jpeg_finish_compress(&compressor);
    jpeg_destroy_decompress(&decompressor);
    jpeg_destroy_compress(&compressor);
    fclose(in);

#if defined(HAVE_OPEN_MEMSTREAM)
    fclose(out);
    *dst = outptr;
    *dstlen = outsize;
#else
    *dstlen = ftell(out);
    *dst = malloc(*dstlen);
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(out);
        return FALSE;
    }
    if (fread(*dst, 1, *dstlen, out) != *dstlen)
    {
        free(*dst);
        fclose(out);
        return FALSE;
    }
    fclose(out);
#endif

    return TRUE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_srgb_to_itulab(logging_state_t *logging, lab_params_t *s, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen, uint32_t width, uint32_t height)
{
    struct jpeg_compress_struct compressor;
    FILE *out;
    JSAMPROW scan_line_out;
    JSAMPROW scan_line_in;
    tsize_t pos;
    escape_route_t escape;
#if defined(HAVE_OPEN_MEMSTREAM)
    char *outptr;
    size_t outsize;
#endif

    escape.error_message[0] = '\0';

#if defined(HAVE_OPEN_MEMSTREAM)
    outsize = 0;
    if ((out = open_memstream(&outptr, &outsize)) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to open_memstream().\n");
        return FALSE;
    }
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(out);
        return FALSE;
    }
#else
    if ((out = tmpfile()) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        return FALSE;
    }
#endif
    scan_line_out = NULL;

    if (setjmp(escape.escape))
    {
        if (escape.error_message[0])
            span_log(logging, SPAN_LOG_FLOW, "%s\n", escape.error_message);
        else
            span_log(logging, SPAN_LOG_FLOW, "Unspecified libjpeg error.\n");
        if (scan_line_out)
            free(scan_line_out);
        fclose(out);
        return FALSE;
    }

    compressor.err = jpeg_std_error(&error_handler);
    compressor.client_data = (void *) &escape;
    error_handler.error_exit = jpg_error_exit;
    error_handler.output_message = jpg_error_exit;

    jpeg_create_compress(&compressor);
    jpeg_stdio_dest(&compressor, out);

    /* Force the destination colour space */
    compressor.in_color_space = JCS_YCbCr;
    compressor.input_components = 3;

    jpeg_set_defaults(&compressor);
    /* Limit to baseline-JPEG values */
    //jpeg_set_quality(&compressor, quality, TRUE);

    /* Size, resolution, etc */
    compressor.image_width = width;
    compressor.image_height = height;

    jpeg_start_compress(&compressor, TRUE);

    set_itu_fax(&compressor);

    if ((scan_line_out = (JSAMPROW) malloc(compressor.image_width*compressor.num_components)) == NULL)
        return FALSE;

    for (pos = 0;  pos < srclen;  pos += compressor.image_width*compressor.num_components)
    {
        scan_line_in = (JSAMPROW) src + pos;
        srgb_to_lab(s, scan_line_out, scan_line_in, compressor.image_width);
        jpeg_write_scanlines(&compressor, &scan_line_out, 1);
    }

    free(scan_line_out);
    jpeg_finish_compress(&compressor);
    jpeg_destroy_compress(&compressor);

#if defined(HAVE_OPEN_MEMSTREAM)
    fclose(out);
    *dst = outptr;
    *dstlen = outsize;
#else
    *dstlen = ftell(out);
    *dst = malloc(*dstlen);
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(out);
        return FALSE;
    }
    if (fread(*dst, 1, *dstlen, out) != *dstlen)
    {
        free(*dst);
        fclose(out);
        return FALSE;
    }
    fclose(out);
#endif

    return TRUE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_itulab_to_itulab(logging_state_t *logging, tdata_t *dst, tsize_t *dstlen, tdata_t src, tsize_t srclen, uint32_t width, uint32_t height)
{
    struct jpeg_compress_struct compressor;
    FILE *out;
    JSAMPROW scan_line_in;
    tsize_t pos;
    escape_route_t escape;
#if defined(HAVE_OPEN_MEMSTREAM)
    char *outptr;
    size_t outsize;
#endif

    escape.error_message[0] = '\0';

#if defined(HAVE_OPEN_MEMSTREAM)
    outsize = 0;
    if ((out = open_memstream(&outptr, &outsize)) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to open_memstream().\n");
        return FALSE;
    }
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(out);
        return FALSE;
    }
#else
    if ((out = tmpfile()) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        return FALSE;
    }
#endif

    if (setjmp(escape.escape))
    {
        if (escape.error_message[0])
            span_log(logging, SPAN_LOG_FLOW, "%s\n", escape.error_message);
        else
            span_log(logging, SPAN_LOG_FLOW, "Unspecified libjpeg error.\n");
        fclose(out);
        return FALSE;
    }

    compressor.err = jpeg_std_error(&error_handler);
    compressor.client_data = (void *) &escape;
    error_handler.error_exit = jpg_error_exit;
    error_handler.output_message = jpg_error_exit;

    jpeg_create_compress(&compressor);
    jpeg_stdio_dest(&compressor, out);

    /* Force the destination colour space */
    compressor.in_color_space = JCS_YCbCr;
    compressor.input_components = 3;

    jpeg_set_defaults(&compressor);
    /* Limit to baseline-JPEG values */
    //jpeg_set_quality(&compressor, quality, TRUE);

    /* Size, resolution, etc */
    compressor.image_width = width;
    compressor.image_height = height;

    jpeg_start_compress(&compressor, TRUE);

    set_itu_fax(&compressor);

    for (pos = 0;  pos < srclen;  pos += compressor.image_width*compressor.num_components)
    {
        scan_line_in = (JSAMPROW) src + pos;
        jpeg_write_scanlines(&compressor, &scan_line_in, 1);
    }

    jpeg_finish_compress(&compressor);
    jpeg_destroy_compress(&compressor);

#if defined(HAVE_OPEN_MEMSTREAM)
    fclose(out);
    *dst = outptr;
    *dstlen = outsize;
#else
    *dstlen = ftell(out);
    *dst = malloc(*dstlen);
    if (fseek(out, 0, SEEK_SET) != 0)
    {
        fclose(out);
        return FALSE;
    }
    if (fread(*dst, 1, *dstlen, out) != *dstlen)
    {
        free(*dst);
        fclose(out);
        return FALSE;
    }        
    fclose(out);
#endif

    return TRUE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_itulab_to_srgb(logging_state_t *logging, lab_params_t *s, tdata_t dst, tsize_t *dstlen, tdata_t src, tsize_t srclen, uint32_t *width, uint32_t *height)
{
    struct jpeg_decompress_struct decompressor;
    JSAMPROW scan_line_out;
    JSAMPROW scan_line_in;
    tsize_t pos;
    FILE *in;
    int m;
    escape_route_t escape;

    escape.error_message[0] = '\0';

#if defined(HAVE_OPEN_MEMSTREAM)
    if ((in = fmemopen(src, srclen, "r")) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to fmemopen().\n");
        return FALSE;
    }
#else
    if ((in = tmpfile()) == NULL)
    {
        span_log(logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        return FALSE;
    }
    if (fwrite(src, 1, srclen, in) != srclen)
    {
        fclose(in);
        return FALSE;
    }
    if (fseek(in, 0, SEEK_SET) != 0)
    {
        fclose(in);
        return FALSE;
    }
#endif
    scan_line_out = NULL;

    if (setjmp(escape.escape))
    {
        if (escape.error_message[0])
            span_log(logging, SPAN_LOG_FLOW, "%s\n", escape.error_message);
        else
            span_log(logging, SPAN_LOG_FLOW, "Unspecified libjpeg error.\n");
        if (scan_line_out)
            free(scan_line_out);
        fclose(in);
        return FALSE;
    }
    /* Create input decompressor. */
    decompressor.err = jpeg_std_error(&error_handler);
    decompressor.client_data = (void *) &escape;
    error_handler.error_exit = jpg_error_exit;
    error_handler.output_message = jpg_error_exit;

    jpeg_create_decompress(&decompressor);
    jpeg_stdio_src(&decompressor, in);

    /* Needed in the case of ITU Lab input */
    for (m = 0;  m < 16;  m++)
        jpeg_save_markers(&decompressor, JPEG_APP0 + m, 0xFFFF);

    /* Rewind the file */
    if (fseek(in, 0, SEEK_SET) != 0)
        return FALSE;
    /* Take the header */
    jpeg_read_header(&decompressor, FALSE);
    /* Now we can force the input colour space. For ITULab, we use YCbCr as a "don't touch" marker */
    decompressor.out_color_space = JCS_YCbCr;
    /* Sanity check and parameter check */
    if (!is_itu_fax(logging, s, decompressor.marker_list))
    {
        span_log(logging, SPAN_LOG_FLOW, "Is not an ITU FAX.\n");
        return FALSE;
    }
    /* Copy size, resolution, etc */
    *width = decompressor.image_width;
    *height = decompressor.image_height;

    jpeg_start_decompress(&decompressor);

    if ((scan_line_in = (JSAMPROW) malloc(decompressor.output_width*decompressor.num_components)) == NULL)
        return FALSE;

    for (pos = 0;  decompressor.output_scanline < decompressor.output_height;  pos += decompressor.output_width*decompressor.num_components)
    {
        scan_line_out = (JSAMPROW) dst + pos;
        jpeg_read_scanlines(&decompressor, &scan_line_in, 1);
        lab_to_srgb(s, scan_line_out, scan_line_in, decompressor.output_width);
    }

    free(scan_line_in);
    jpeg_finish_decompress(&decompressor);
    jpeg_destroy_decompress(&decompressor);
    fclose(in);

    *dstlen = pos;

    return TRUE;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t42_encode_set_options(t42_encode_state_t *s,
                                          uint32_t l0,
                                          int mx,
                                          int options)
{
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_set_image_width(t42_encode_state_t *s, uint32_t image_width)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_set_image_length(t42_encode_state_t *s, uint32_t length)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t42_encode_abort(t42_encode_state_t *s)
{
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t42_encode_comment(t42_encode_state_t *s, const uint8_t comment[], size_t len)
{
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_image_complete(t42_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_get(t42_encode_state_t *s, uint8_t buf[], size_t max_len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t42_encode_get_image_width(t42_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t42_encode_get_image_length(t42_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_get_compressed_image_size(t42_encode_state_t *s)
{
    return s->compressed_image_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_set_row_read_handler(t42_encode_state_t *s,
                                                  t4_row_read_handler_t handler,
                                                  void *user_data)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t42_encode_get_logging_state(t42_encode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_restart(t42_encode_state_t *s, uint32_t image_width, uint32_t image_length)
{
    //s->image_width = image_width;
    //s->image_length = image_length;
    s->compressed_image_size = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t42_encode_state_t *) t42_encode_init(t42_encode_state_t *s,
                                                   uint32_t image_width,
                                                   uint32_t image_length,
                                                   t4_row_read_handler_t handler,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t42_encode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.42");

    s->row_read_handler = handler;
    s->row_read_user_data = user_data;
    t42_encode_restart(s, image_width, image_length);

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_release(t42_encode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_free(t42_encode_state_t *s)
{
    int ret;

    ret = t42_encode_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t42_decode_rx_status(t42_decode_state_t *s, int status)
{
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_put(t42_decode_state_t *s, const uint8_t data[], size_t len)
{
    uint8_t *buf;

    if (s->compressed_image_size + len > s->buf_size)
    {
        if ((buf = (uint8_t *) realloc(s->compressed_buf, s->compressed_image_size + 1000)) == NULL)
            return -1;
        s->buf_size = s->compressed_image_size + 1000;
        s->compressed_buf = buf;
    }
    memcpy(&s->compressed_buf[s->compressed_image_size], data, len);
    s->compressed_image_size += len;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_set_row_write_handler(t42_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data)
{
    s->row_write_handler = handler;
    s->row_write_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_set_comment_handler(t42_decode_state_t *s,
                                                 uint32_t max_comment_len,
                                                 t4_row_write_handler_t handler,
                                                 void *user_data)
{
    s->max_comment_len = max_comment_len;
    s->comment_handler = handler;
    s->comment_user_data = user_data;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_set_image_size_constraints(t42_decode_state_t *s,
                                                        uint32_t max_xd,
                                                        uint32_t max_yd)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t42_decode_get_image_width(t42_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t42_decode_get_image_length(t42_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_get_compressed_image_size(t42_decode_state_t *s)
{
    return s->compressed_image_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_new_plane(t42_decode_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t42_decode_get_logging_state(t42_decode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_restart(t42_decode_state_t *s)
{
    s->compressed_image_size = 0;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t42_decode_state_t *) t42_decode_init(t42_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t42_decode_state_t *) malloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.42");

    s->row_write_handler = handler;
    s->row_write_user_data = user_data;

    s->buf_size = 0;
    s->compressed_buf = NULL;

    t42_decode_restart(s);

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_release(t42_decode_state_t *s)
{
    if (s->comment)
    {
        free(s->comment);
        s->comment = NULL;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_free(t42_decode_state_t *s)
{
    int ret;

    ret = t42_decode_release(s);
    free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

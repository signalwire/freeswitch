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
#include <float.h>
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
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
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

/* LERP(a,b,c) = linear interpolation macro, is 'a' when c == 0.0 and 'b' when c == 1.0 */
#define LERP(a,b,c)     (((b) - (a))*(c) + (a))

typedef struct UVT
{
    double u;
    double v;
    double t;
} UVT;

static const double rt[31] =
{
    /* Reciprocal temperature (K) */
     FLT_MIN,
     10.0e-6,
     20.0e-6,
     30.0e-6,
     40.0e-6,
     50.0e-6,
     60.0e-6,
     70.0e-6,
     80.0e-6,
     90.0e-6,
    100.0e-6,
    125.0e-6,
    150.0e-6,
    175.0e-6,
    200.0e-6,
    225.0e-6,
    250.0e-6,
    275.0e-6,
    300.0e-6,
    325.0e-6,
    350.0e-6,
    375.0e-6,
    400.0e-6,
    425.0e-6,
    450.0e-6,
    475.0e-6,
    500.0e-6,
    525.0e-6,
    550.0e-6,
    575.0e-6,
    600.0e-6
};

static const UVT uvt[31] =
{
    {0.18006, 0.26352, -0.24341},
    {0.18066, 0.26589, -0.25479},
    {0.18133, 0.26846, -0.26876},
    {0.18208, 0.27119, -0.28539},
    {0.18293, 0.27407, -0.30470},
    {0.18388, 0.27709, -0.32675},
    {0.18494, 0.28021, -0.35156},
    {0.18611, 0.28342, -0.37915},
    {0.18740, 0.28668, -0.40955},
    {0.18880, 0.28997, -0.44278},
    {0.19032, 0.29326, -0.47888},
    {0.19462, 0.30141, -0.58204},
    {0.19962, 0.30921, -0.70471},
    {0.20525, 0.31647, -0.84901},
    {0.21142, 0.32312, -1.01820},
    {0.21807, 0.32909, -1.21680},
    {0.22511, 0.33439, -1.45120},
    {0.23247, 0.33904, -1.72980},
    {0.24010, 0.34308, -2.06370},
    {0.24792, 0.34655, -2.46810},   /* Note: 0.24792 is a corrected value for the error found in W&S as 0.24702 */
    {0.25591, 0.34951, -2.96410},
    {0.26400, 0.35200, -3.58140},
    {0.27218, 0.35407, -4.36330},
    {0.28039, 0.35577, -5.37620},
    {0.28863, 0.35714, -6.72620},
    {0.29685, 0.35823, -8.59550},
    {0.30505, 0.35907, -11.3240},
    {0.31320, 0.35968, -15.6280},
    {0.32129, 0.36011, -23.3250},
    {0.32931, 0.36038, -40.7700},
    {0.33724, 0.36051, -116.450}
};

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

SPAN_DECLARE(int) xyz_to_corrected_color_temp(float *temp, float xyz[3])
{
    float us;
    float vs;
    float p;
    float di;
    float dm;
    int i;

    /* Protect against possible divide-by-zero failure */
    if ((xyz[0] < 1.0e-20f)  &&  (xyz[1] < 1.0e-20f)  &&  (xyz[2] < 1.0e-20f))
        return -1;
    us = (4.0f*xyz[0])/(xyz[0] + 15.0f*xyz[1] + 3.0f*xyz[2]);
    vs = (6.0f*xyz[1])/(xyz[0] + 15.0f*xyz[1] + 3.0f*xyz[2]);
    dm = 0.0f;
    for (i = 0;  i < 31;  i++)
    {
        di = (vs - uvt[i].v) - uvt[i].t*(us - uvt[i].u);
        if ((i > 0)  &&  (((di < 0.0f)  &&  (dm >= 0.0f))  ||  ((di >= 0.0f)  &&  (dm < 0.0f))))
            break;  /* found lines bounding (us, vs) : i-1 and i */
        dm = di;
    }
    if (i == 31)
    {
        /* Bad XYZ input, color temp would be less than minimum of 1666.7 degrees, or too far towards blue */
        return -1;
    }
    di = di/sqrtf(1.0f + uvt[i    ].t*uvt[i    ].t);
    dm = dm/sqrtf(1.0f + uvt[i - 1].t*uvt[i - 1].t);
    p = dm/(dm - di);     /* p = interpolation parameter, 0.0 : i-1, 1.0 : i */
    p = 1.0f/(LERP(rt[i - 1], rt[i], p));
    *temp = p;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) colour_temp_to_xyz(float xyz[3], float temp)
{
    float x;
    float y;

    /* Should be good for 1667K to 25000K according to Wikipedia */
    if (temp < 1667.0f  ||  temp > 25000.0f)
        return -1;

    if (temp < 4000.0f)
        x = -0.2661239e9f/(temp*temp*temp) - 0.2343580e6f/(temp*temp) + 0.8776956e3f/temp + 0.179910f;
    else
        x = -3.0258469e9f/(temp*temp*temp) + 2.1070379e6f/(temp*temp) + 0.2226347e3f/temp + 0.240390f;

    if (temp < 2222.0f)
        y = -1.1063814f*x*x*x - 1.34811020f*x*x + 2.18555832f*x - 0.20219683f;
    else if (temp < 4000.0f)
        y = -0.9549476f*x*x*x - 1.37418593f*x*x + 2.09137015f*x - 0.16748867f;
    else
        y =  3.0817580f*x*x*x - 5.87338670f*x*x + 3.75112997f*x - 0.37001483f;

    xyz[0] = x/y;
    xyz[1] = 1.0f;
    xyz[2] = (1.0f - x - y)/y;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) set_lab_illuminant(lab_params_t *lab, float new_xn, float new_yn, float new_zn)
{
    if (new_yn > 10.0f)
    {
        lab->x_n = new_xn/100.0f;
        lab->y_n = new_yn/100.0f;
        lab->z_n = new_zn/100.0f;
    }
    else
    {
        lab->x_n = new_xn;
        lab->y_n = new_yn;
        lab->z_n = new_zn;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) set_lab_gamut(lab_params_t *lab, int L_min, int L_max, int a_min, int a_max, int b_min, int b_max, int ab_are_signed)
{
    lab->range_L = L_max - L_min;
    lab->range_a = a_max - a_min;
    lab->range_b = b_max - b_min;

    lab->offset_L = -256.0f*L_min/lab->range_L;
    lab->offset_a = -256.0f*a_min/lab->range_a;
    lab->offset_b = -256.0f*b_min/lab->range_b;

    lab->range_L /= (256.0f - 1.0f);
    lab->range_a /= (256.0f - 1.0f);
    lab->range_b /= (256.0f - 1.0f);

    lab->ab_are_signed = ab_are_signed;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) set_lab_gamut2(lab_params_t *lab, int L_P, int L_Q, int a_P, int a_Q, int b_P, int b_Q)
{
    lab->range_L = L_Q/(256.0f - 1.0f);
    lab->range_a = a_Q/(256.0f - 1.0f);
    lab->range_b = b_Q/(256.0f - 1.0f);

    lab->offset_L = L_P;
    lab->offset_a = a_P;
    lab->offset_b = b_P;

    lab->ab_are_signed = false;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) get_lab_gamut2(lab_params_t *lab, int *L_P, int *L_Q, int *a_P, int *a_Q, int *b_P, int *b_Q)
{
    *L_Q = lab->range_L*(256.0f - 1.0f);
    *a_Q = lab->range_a*(256.0f - 1.0f);
    *b_Q = lab->range_b*(256.0f - 1.0f);

    *L_P = lab->offset_L;
    *a_P = lab->offset_a;
    *b_P = lab->offset_b;
}
/*- End of function --------------------------------------------------------*/

int set_illuminant_from_code(logging_state_t *logging, lab_params_t *lab, const uint8_t code[4])
{
    int i;
    int colour_temp;
    float xyz[3];

    if (memcmp(code, "CT", 2) == 0)
    {
        colour_temp = pack_16(&code[2]);
        span_log(logging, SPAN_LOG_FLOW, "Illuminant colour temp %dK\n", colour_temp);
        colour_temp_to_xyz(xyz, (float) colour_temp);
        set_lab_illuminant(lab, xyz[0], xyz[1], xyz[2]);
        return colour_temp;
    }
    for (i = 0;  illuminants[i].name[0];  i++)
    {
        if (memcmp(code, illuminants[i].tag, 4) == 0)
        {
            span_log(logging, SPAN_LOG_FLOW, "Illuminant %s\n", illuminants[i].name);
            set_lab_illuminant(lab, illuminants[i].xn, illuminants[i].yn, illuminants[i].zn);
            return 0;
        }
    }
    if (illuminants[i].name[0] == '\0')
        span_log(logging, SPAN_LOG_FLOW, "Unrecognised illuminant 0x%x 0x%x 0x%x 0x%x\n", code[0], code[1], code[2], code[3]);
    return -1;
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

static int is_itu_fax(t42_decode_state_t *s, jpeg_saved_marker_ptr ptr)
{
    const uint8_t *data;
    int ok;
    int val[6];

    ok = false;
    for (  ;  ptr;  ptr = ptr->next)
    {
        if (ptr->marker != (JPEG_APP0 + 1))
            continue;
        if (ptr->data_length < 6)
            return false;
        /* Markers are:
            JPEG_RST0
            JPEG_EOI
            JPEG_APP0
            JPEG_COM */
        data = (const uint8_t *) ptr->data;
        if (strncmp((const char *) data, "G3FAX", 5))
            return false;
        switch (data[5])
        {
        case 0:
            if (ptr->data_length < 6 + 4)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX0 length - %d\n", ptr->data_length);
                return false;
            }
            val[0] = pack_16(&data[6]);
            s->spatial_resolution = pack_16(&data[6 + 2]);
            span_log(&s->logging, SPAN_LOG_FLOW, "Version %d, resolution %ddpi\n", val[0], s->spatial_resolution);
            ok = true;
            break;
        case 1:
            span_log(&s->logging, SPAN_LOG_FLOW, "Set gamut\n");
            if (ptr->data_length < 6 + 12)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX1 length - %d\n", ptr->data_length);
                return false;
            }
            set_gamut_from_code(&s->logging, &s->lab, &data[6]);
            break;
        case 2:
            span_log(&s->logging, SPAN_LOG_FLOW, "Set illuminant\n");
            if (ptr->data_length < 6 + 4)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Got bad G3FAX2 length - %d\n", ptr->data_length);
                return false;
            }
            s->illuminant_colour_temperature = set_illuminant_from_code(&s->logging, &s->lab, &data[6]);
            break;
        default:
            span_log(&s->logging, SPAN_LOG_FLOW, "Got unexpected G3FAX%d length - %d\n", data[5], ptr->data_length);
            return false;
        }
    }

    return ok;
}
/*- End of function --------------------------------------------------------*/

static void set_itu_fax(t42_encode_state_t *s)
{
    uint8_t data[50];
    int val[6];

    memcpy(data, "G3FAX\0", 6);
    unpack_16(&data[6 + 0], 1994);
    unpack_16(&data[6 + 2], s->spatial_resolution);
    jpeg_write_marker(&s->compressor, (JPEG_APP0 + 1), data, 6 + 4);

    if (s->lab.offset_L != 0
        ||
        s->lab.range_L != 100
        ||
        s->lab.offset_a != 128
        ||
        s->lab.range_a != 170
        ||
        s->lab.offset_b != 96
        ||
        s->lab.range_b != 200)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Putting G3FAX1\n");
        memcpy(data, "G3FAX\1", 6);
        get_lab_gamut2(&s->lab, &val[0], &val[1], &val[2], &val[3], &val[4], &val[5]);
        unpack_16(&data[6 + 0], val[0]);
        unpack_16(&data[6 + 2], val[1]);
        unpack_16(&data[6 + 4], val[2]);
        unpack_16(&data[6 + 6], val[3]);
        unpack_16(&data[6 + 8], val[4]);
        unpack_16(&data[6 + 10], val[5]);
        jpeg_write_marker(&s->compressor, (JPEG_APP0 + 1), data, 6 + 12);
    }

    if (memcmp(s->illuminant_code, "\0\0\0\0", 4) != 0
        ||
        s->illuminant_colour_temperature > 0)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Putting G3FAX2\n");
        memcpy(data, "G3FAX\2", 6);
        if (memcmp(s->illuminant_code, "\0\0\0\0", 4) != 0)
        {
            memcpy(&data[6], s->illuminant_code, 4);
        }
        else
        {
            memcpy(&data[6 + 0], "CT", 2);
            unpack_16(&data[6 + 2], s->illuminant_colour_temperature);
        }
        jpeg_write_marker(&s->compressor, (JPEG_APP0 + 1), data, 6 + 4);
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t42_encode_set_options(t42_encode_state_t *s,
                                          uint32_t l0,
                                          int quality,
                                          int options)
{
    s->quality = quality;
    s->no_subsampling = (options & 1);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_set_image_width(t42_encode_state_t *s, uint32_t image_width)
{
    s->image_width = image_width;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_set_image_length(t42_encode_state_t *s, uint32_t image_length)
{
    s->image_length = image_length;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_set_image_type(t42_encode_state_t *s, int image_type)
{
    s->image_type = image_type;
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
    //if (????)
    //    return SIG_STATUS_END_OF_DATA;
    return 0;
}
/*- End of function --------------------------------------------------------*/

/* Error handler for IJG library */
static void jpg_encode_error_exit(j_common_ptr cinfo)
{
    t42_encode_state_t *s;

    s = (t42_encode_state_t *) cinfo->client_data;
    (*cinfo->err->format_message)(cinfo, s->error_message);
    longjmp(s->escape, 1);
}
/*- End of function --------------------------------------------------------*/

/* This is the error catcher */
static struct jpeg_error_mgr encode_error_handler =
{
#if defined(_MSC_VER)  ||  defined(__sunos)  ||  defined(__solaris)  ||  defined(__sun)
    jpg_encode_error_exit,
    0,
    jpg_encode_error_exit
#else
    .error_exit = jpg_encode_error_exit,
    .output_message = jpg_encode_error_exit
#endif
};

static int t42_srgb_to_itulab_jpeg(t42_encode_state_t *s)
{
    int i;

    if (setjmp(s->escape))
    {
        if (s->error_message[0])
            span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", s->error_message);
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Unspecified libjpeg error.\n");
        if (s->scan_line_out)
        {
            span_free(s->scan_line_out);
            s->scan_line_out = NULL;
        }
        if (s->out)
        {
            fclose(s->out);
            s->out = NULL;
        }
        return -1;
    }

    s->compressor.err = jpeg_std_error(&encode_error_handler);
    s->compressor.client_data = (void *) s;

    jpeg_create_compress(&s->compressor);
    jpeg_stdio_dest(&s->compressor, s->out);

    /* Force the destination colour space */
    if (s->image_type == T4_IMAGE_TYPE_COLOUR_8BIT)
    {
        s->samples_per_pixel = 3;
        s->compressor.in_color_space = JCS_YCbCr;
        s->compressor.input_components = s->samples_per_pixel;
    }
    else
    {
        s->samples_per_pixel = 1;
        s->compressor.in_color_space = JCS_GRAYSCALE;
        s->compressor.input_components = s->samples_per_pixel;
    }

    jpeg_set_defaults(&s->compressor);
    /* Limit to baseline-JPEG values */
    //jpeg_set_quality(&s->compressor, s->quality, true);

    if (s->no_subsampling)
    {
        /* Set 1:1:1 */
        s->compressor.comp_info[0].h_samp_factor = 1;
        s->compressor.comp_info[0].v_samp_factor = 1;
    }
    else
    {
        /* Set 4:1:1 */
        s->compressor.comp_info[0].h_samp_factor = 2;
        s->compressor.comp_info[0].v_samp_factor = 2;
    }
    s->compressor.comp_info[1].h_samp_factor = 1;
    s->compressor.comp_info[1].v_samp_factor = 1;
    s->compressor.comp_info[2].h_samp_factor = 1;
    s->compressor.comp_info[2].v_samp_factor = 1;

    /* Size, resolution, etc */
    s->compressor.image_width = s->image_width;
    s->compressor.image_height = s->image_length;

    jpeg_start_compress(&s->compressor, true);

    set_itu_fax(s);

    if ((s->scan_line_in = (JSAMPROW) span_alloc(s->samples_per_pixel*s->image_width)) == NULL)
        return -1;

    if (s->image_type == T4_IMAGE_TYPE_COLOUR_8BIT)
    {
        if ((s->scan_line_out = (JSAMPROW) span_alloc(s->samples_per_pixel*s->image_width)) == NULL)
            return -1;

        for (i = 0;  i < s->compressor.image_height;  i++)
        {
            s->row_read_handler(s->row_read_user_data, s->scan_line_in, s->samples_per_pixel*s->image_width);
            srgb_to_lab(&s->lab, s->scan_line_out, s->scan_line_in, s->image_width);
            jpeg_write_scanlines(&s->compressor, &s->scan_line_out, 1);
        }
    }
    else
    {
        for (i = 0;  i < s->compressor.image_height;  i++)
        {
            s->row_read_handler(s->row_read_user_data, s->scan_line_in, s->image_width);
            jpeg_write_scanlines(&s->compressor, &s->scan_line_in, 1);
        }
    }

    if (s->scan_line_out)
    {
        span_free(s->scan_line_out);
        s->scan_line_out = NULL;
    }
    jpeg_finish_compress(&s->compressor);
    jpeg_destroy_compress(&s->compressor);

#if defined(HAVE_OPEN_MEMSTREAM)
    fclose(s->out);
    s->buf_size =
    s->compressed_image_size = s->outsize;
#else
    s->buf_size =
    s->compressed_image_size = ftell(s->out);
    if ((s->compressed_buf = span_alloc(s->compressed_image_size)) == NULL)
        return -1;
    if (fseek(s->out, 0, SEEK_SET) != 0)
    {
        fclose(s->out);
        s->out = NULL;
        span_free(s->compressed_buf);
        s->compressed_buf = NULL;
        return -1;
    }
    if (fread(s->compressed_buf, 1, s->compressed_image_size, s->out) != s->compressed_image_size)
    {
        fclose(s->out);
        s->out = NULL;
        span_free(s->compressed_buf);
        s->compressed_buf = NULL;
        return -1;
    }
    if (s->out)
    {
        fclose(s->out);
        s->out = NULL;
    }
#endif

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_encode_get(t42_encode_state_t *s, uint8_t buf[], size_t max_len)
{
    int len;

    if (s->compressed_image_size == 0)
    {
        if (t42_srgb_to_itulab_jpeg(s))
        {
            span_log(&s->logging, SPAN_LOG_FLOW, "Failed to convert to ITULAB.\n");
            return -1;
        }
    }
    if (s->compressed_image_size >= s->compressed_image_ptr + max_len)
        len = max_len;
    else
        len = s->compressed_image_size - s->compressed_image_ptr;
    memcpy(buf, &s->compressed_buf[s->compressed_image_ptr], len);
    s->compressed_image_ptr += len;
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t42_encode_get_image_width(t42_encode_state_t *s)
{
    return s->image_width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t42_encode_get_image_length(t42_encode_state_t *s)
{
    return s->image_length;
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
    s->row_read_handler = handler;
    s->row_read_user_data = user_data;
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
    s->image_width = image_width;
    s->image_length = image_length;

    if (s->itu_ycc)
    {
        /* ITU-YCC */
        /* Illuminant D65 */
        set_lab_illuminant(&s->lab, 95.047f, 100.000f, 108.883f);
        set_lab_gamut(&s->lab, 0, 100, -127, 127, -127, 127, false);
    }
    else
    {
        /* ITULAB */
        /* Illuminant D50 */
        set_lab_illuminant(&s->lab, 96.422f, 100.000f,  82.521f);
        set_lab_gamut(&s->lab, 0, 100, -85, 85, -75, 125, false);
    }
    s->compressed_image_size = 0;
    s->compressed_image_ptr = 0;

    s->spatial_resolution = 200;

    s->error_message[0] = '\0';

#if defined(HAVE_OPEN_MEMSTREAM)
    s->outsize = 0;
    if ((s->out = open_memstream((char **) &s->compressed_buf, &s->outsize)) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Failed to open_memstream().\n");
        return -1;
    }
    if (fseek(s->out, 0, SEEK_SET) != 0)
    {
        fclose(s->out);
        s->out = NULL;
        return -1;
    }
#else
    if ((s->out = tmpfile()) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        return -1;
    }
#endif
    s->scan_line_out = NULL;

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
        if ((s = (t42_encode_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "T.42");

    s->quality = 90;
    s->image_type = T4_IMAGE_TYPE_COLOUR_8BIT;

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
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/

/* Error handler for IJG library */
static void jpg_decode_error_exit(j_common_ptr cinfo)
{
    t42_decode_state_t *s;

    s = (t42_decode_state_t *) cinfo->client_data;
    (*cinfo->err->format_message)(cinfo, s->error_message);
    longjmp(s->escape, 1);
}
/*- End of function --------------------------------------------------------*/

/* This is the error catcher */
static struct jpeg_error_mgr decode_error_handler =
{
#if defined(_MSC_VER)  ||  defined(__sunos)  ||  defined(__solaris)  ||  defined(__sun)
    jpg_decode_error_exit,
    0,
    jpg_decode_error_exit
#else
    .error_exit = jpg_decode_error_exit,
    .output_message = jpg_decode_error_exit
#endif
};

static int t42_itulab_jpeg_to_srgb(t42_decode_state_t *s)
{
    int i;

    if (s->compressed_buf == NULL)
        return -1;

#if defined(HAVE_OPEN_MEMSTREAM)
    if ((s->in = fmemopen(s->compressed_buf, s->compressed_image_size, "r")) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Failed to fmemopen().\n");
        return -1;
    }
#else
    if ((s->in = tmpfile()) == NULL)
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Failed to tmpfile().\n");
        return -1;
    }
    if (fwrite(s->compressed_buf, 1, s->compressed_image_size, s->in) != s->compressed_image_size)
    {
        fclose(s->in);
        s->in = NULL;
        return -1;
    }
    if (fseek(s->in, 0, SEEK_SET) != 0)
    {
        fclose(s->in);
        s->in = NULL;
        return -1;
    }
#endif
    s->scan_line_out = NULL;

    if (setjmp(s->escape))
    {
        if (s->error_message[0])
            span_log(&s->logging, SPAN_LOG_FLOW, "%s\n", s->error_message);
        else
            span_log(&s->logging, SPAN_LOG_FLOW, "Unspecified libjpeg error.\n");
        if (s->scan_line_out)
        {
            span_free(s->scan_line_out);
            s->scan_line_out = NULL;
        }
        if (s->in)
        {
            fclose(s->in);
            s->in = NULL;
        }
        return -1;
    }
    /* Create input decompressor. */
    s->decompressor.err = jpeg_std_error(&decode_error_handler);
    s->decompressor.client_data = (void *) s;

    jpeg_create_decompress(&s->decompressor);
    jpeg_stdio_src(&s->decompressor, s->in);

    /* Get the FAX tags */
    for (i = 0;  i < 16;  i++)
        jpeg_save_markers(&s->decompressor, JPEG_APP0 + i, 0xFFFF);

    /* Rewind the file */
    if (fseek(s->in, 0, SEEK_SET) != 0)
    {
        fclose(s->in);
        s->in = NULL;
        return -1;
    }

    /* Take the header */
    jpeg_read_header(&s->decompressor, false);
    /* Sanity check and parameter check */
    if (!is_itu_fax(s, s->decompressor.marker_list))
    {
        span_log(&s->logging, SPAN_LOG_FLOW, "Is not an ITU FAX.\n");
        return -1;
    }
    /* Copy size, resolution, etc */
    s->image_width = s->decompressor.image_width;
    s->image_length = s->decompressor.image_height;
    s->samples_per_pixel = s->decompressor.num_components;

    if (s->samples_per_pixel == 3)
    {
        /* Now we can force the input colour space. For ITULab, we use YCbCr as a "don't touch" marker */
        s->decompressor.out_color_space = JCS_YCbCr;
        span_log(&s->logging,
                 SPAN_LOG_FLOW, 
                 "Sampling %d %d %d %d %d %d\n",
                 s->decompressor.comp_info[0].h_samp_factor,
                 s->decompressor.comp_info[0].v_samp_factor,
                 s->decompressor.comp_info[1].h_samp_factor,
                 s->decompressor.comp_info[1].v_samp_factor,
                 s->decompressor.comp_info[2].h_samp_factor,
                 s->decompressor.comp_info[2].v_samp_factor); 
    }
    else
    {
        s->decompressor.out_color_space = JCS_GRAYSCALE;
        span_log(&s->logging,
                 SPAN_LOG_FLOW, 
                 "Sampling %d %d\n",
                 s->decompressor.comp_info[0].h_samp_factor,
                 s->decompressor.comp_info[0].v_samp_factor); 
    }

    jpeg_start_decompress(&s->decompressor);

    if ((s->scan_line_in = span_alloc(s->samples_per_pixel*s->image_width)) == NULL)
        return -1;

    if (s->samples_per_pixel == 3)
    {
        if ((s->scan_line_out = span_alloc(s->samples_per_pixel*s->image_width)) == NULL)
            return -1;

        while (s->decompressor.output_scanline < s->image_length)
        {
            jpeg_read_scanlines(&s->decompressor, &s->scan_line_in, 1);
            lab_to_srgb(&s->lab, s->scan_line_out, s->scan_line_in, s->image_width);
            s->row_write_handler(s->row_write_user_data, s->scan_line_out, s->samples_per_pixel*s->image_width);
        }
    }
    else
    {
        while (s->decompressor.output_scanline < s->image_length)
        {
            jpeg_read_scanlines(&s->decompressor, &s->scan_line_in, 1);
            s->row_write_handler(s->row_write_user_data, s->scan_line_in, s->image_width);
        }
    }

    if (s->scan_line_in)
    {
        span_free(s->scan_line_in);
        s->scan_line_in = NULL;
    }
    if (s->scan_line_out)
    {
        span_free(s->scan_line_out);
        s->scan_line_out = NULL;
    }
    jpeg_finish_decompress(&s->decompressor);
    jpeg_destroy_decompress(&s->decompressor);
    fclose(s->in);
    s->in = NULL;

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) t42_decode_rx_status(t42_decode_state_t *s, int status)
{
    span_log(&s->logging, SPAN_LOG_FLOW, "Signal status is %s (%d)\n", signal_status_to_str(status), status);
    switch (status)
    {
    case SIG_STATUS_TRAINING_IN_PROGRESS:
    case SIG_STATUS_TRAINING_FAILED:
    case SIG_STATUS_TRAINING_SUCCEEDED:
    case SIG_STATUS_CARRIER_UP:
        /* Ignore these */
        break;
    case SIG_STATUS_CARRIER_DOWN:
    case SIG_STATUS_END_OF_DATA:
        /* Finalise the image */
        if (!s->end_of_data)
        {
            if (t42_itulab_jpeg_to_srgb(s))
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to convert from ITULAB.\n");
            s->end_of_data = 1;
        }
        break;
    default:
        span_log(&s->logging, SPAN_LOG_WARNING, "Unexpected rx status - %d!\n", status);
        break;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_put(t42_decode_state_t *s, const uint8_t data[], size_t len)
{
    uint8_t *buf;

    if (len == 0)
    {
        if (!s->end_of_data)
        {
            if (t42_itulab_jpeg_to_srgb(s))
                span_log(&s->logging, SPAN_LOG_FLOW, "Failed to convert from ITULAB.\n");
            s->end_of_data = 1;
        }
        return T4_DECODE_OK;
    }

    if (s->compressed_image_size + len > s->buf_size)
    {
        if ((buf = (uint8_t *) span_realloc(s->compressed_buf, s->compressed_image_size + len + 10000)) == NULL)
            return -1;
        s->buf_size = s->compressed_image_size + len + 10000;
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
    return s->image_width;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(uint32_t) t42_decode_get_image_length(t42_decode_state_t *s)
{
    return s->image_length;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_get_compressed_image_size(t42_decode_state_t *s)
{
    return s->compressed_image_size;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) t42_decode_get_logging_state(t42_decode_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_restart(t42_decode_state_t *s)
{
    if (s->itu_ycc)
    {
        /* ITU-YCC */
        /* Illuminant D65 */
        set_lab_illuminant(&s->lab, 95.047f, 100.000f, 108.883f);
        set_lab_gamut(&s->lab, 0, 100, -127, 127, -127, 127, false);
    }
    else
    {
        /* ITULAB */
        /* Illuminant D50 */
        set_lab_illuminant(&s->lab, 96.422f, 100.000f,  82.521f);
        set_lab_gamut(&s->lab, 0, 100, -85, 85, -75, 125, false);
    }

    s->end_of_data = 0;
    s->compressed_image_size = 0;

    s->error_message[0] = '\0';

    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(t42_decode_state_t *) t42_decode_init(t42_decode_state_t *s,
                                                   t4_row_write_handler_t handler,
                                                   void *user_data)
{
    if (s == NULL)
    {
        if ((s = (t42_decode_state_t *) span_alloc(sizeof(*s))) == NULL)
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
    if (s->scan_line_in)
    {
        span_free(s->scan_line_in);
        s->scan_line_in = NULL;
    }
    if (s->scan_line_out)
    {
        span_free(s->scan_line_out);
        s->scan_line_out = NULL;
    }
    jpeg_destroy_decompress(&s->decompressor);
    if (s->in)
    {
        fclose(s->in);
        s->in = NULL;
    }
    if (s->comment)
    {
        span_free(s->comment);
        s->comment = NULL;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) t42_decode_free(t42_decode_state_t *s)
{
    int ret;

    ret = t42_decode_release(s);
    span_free(s);
    return ret;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

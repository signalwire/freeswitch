/*
 * SpanDSP - a series of DSP components for telephony
 *
 * vector_float_tests.c
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "spandsp.h"

static void vec_copyf_dumb(float z[], const float x[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i];
}
/*- End of function --------------------------------------------------------*/

static int test_vec_copyf(void)
{
    int i;
    float x[100];
    float za[100];
    float zb[100];

    printf("Testing vec_copyf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = i;
        za[i] = -0.5f;
        zb[i] = -0.5f;
    }
    vec_copyf_dumb(za + 3, x + 1, 0);
    vec_copyf(zb + 3, x + 1, 0);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_copyf() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_copyf_dumb(za + 3, x + 1, 1);
    vec_copyf(zb + 3, x + 1, 1);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_copyf() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_copyf_dumb(za + 3, x + 1, 29);
    vec_copyf(zb + 3, x + 1, 29);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_copyf() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_negatef_dumb(float z[], const float x[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = -x[i];
}
/*- End of function --------------------------------------------------------*/

static int test_vec_negatef(void)
{
    int i;
    float x[100];
    float za[100];
    float zb[100];

    printf("Testing vec_negatef()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = i;
        za[i] = -0.5f;
        zb[i] = -0.5f;
    }
    vec_negatef_dumb(za + 3, x + 1, 0);
    vec_negatef(zb + 3, x + 1, 0);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_negatef() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_negatef_dumb(za + 3, x + 1, 1);
    vec_negatef(zb + 3, x + 1, 1);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_megatef() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_negatef_dumb(za + 3, x + 1, 29);
    vec_negatef(zb + 3, x + 1, 29);
    for (i = 0;  i < 99;  i++)
    {
printf("C %d %f %f %f\n", i, x[i], za[i], zb[i]);
        if (za[i] != zb[i])
        {
            printf("vec_negatef() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_zerof_dumb(float z[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = 0.0f;
}
/*- End of function --------------------------------------------------------*/

static int test_vec_zerof(void)
{
    int i;
    float za[100];
    float zb[100];

    printf("Testing vec_zerof()\n");
    for (i = 0;  i < 99;  i++)
    {
        za[i] = -1.0f;
        zb[i] = -1.0f;
    }
    vec_zerof_dumb(za + 3, 0);
    vec_zerof(zb + 3, 0);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_zerof() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_zerof_dumb(za + 3, 1);
    vec_zerof(zb + 3, 1);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_zerof() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_zerof_dumb(za + 3, 29);
    vec_zerof(zb + 3, 29);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_zerof() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_setf_dumb(float z[], float x, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x;
}
/*- End of function --------------------------------------------------------*/

static int test_vec_setf(void)
{
    int i;
    float za[100];
    float zb[100];

    printf("Testing vec_setf()\n");
    for (i = 0;  i < 99;  i++)
    {
        za[i] = -1.0f;
        zb[i] = -1.0f;
    }
    vec_setf_dumb(za + 3, 42.0f, 0);
    vec_setf(zb + 3, 42.0f, 0);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_setf() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_setf_dumb(za + 3, 42.0f, 1);
    vec_setf(zb + 3, 42.0f, 1);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_setf() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    vec_setf_dumb(za + 3, 42.0f, 29);
    vec_setf(zb + 3, 42.0f, 29);
    for (i = 0;  i < 99;  i++)
    {
        if (za[i] != zb[i])
        {
            printf("vec_setf() - %d %f %f\n", i, za[i], zb[i]);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static double vec_dot_prod_dumb(const double x[], const double y[], int n)
{
    int i;
    double z;

    z = 0.0;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return z;
}
/*- End of function --------------------------------------------------------*/

static int test_vec_dot_prod(void)
{
    int i;
    double x[100];
    double y[100];
    double zsa;
    double zsb;
    double ratio;

    printf("Testing vec_dot_prod()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    for (i = 1;  i < 99;  i++)
    {
        zsa = vec_dot_prod(x, y, i);
        zsb = vec_dot_prod_dumb(x, y, i);
        ratio = zsa/zsb;
        if (ratio < 0.9999  ||  ratio > 1.0001)
        {
            printf("vec_dot_prod() - %f %f\n", zsa, zsb);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static float vec_dot_prodf_dumb(const float x[], const float y[], int n)
{
    int i;
    float z;

    z = 0.0;
    for (i = 0;  i < n;  i++)
        z += x[i]*y[i];
    return z;
}
/*- End of function --------------------------------------------------------*/

static int test_vec_dot_prodf(void)
{
    int i;
    float x[100];
    float y[100];
    float zsa;
    float zsb;
    float ratio;

    printf("Testing vec_dot_prodf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    for (i = 1;  i < 99;  i++)
    {
        zsa = vec_dot_prodf(x, y, i);
        zsb = vec_dot_prodf_dumb(x, y, i);
        ratio = zsa/zsb;
        if (ratio < 0.9999f  ||  ratio > 1.0001f)
        {
            printf("vec_dot_prodf() - %e %e\n", zsa, zsb);
            printf("Tests failed\n");
            exit(2);
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_addf_dumb(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i];
}
/*- End of function --------------------------------------------------------*/

static int test_vec_addf(void)
{
    int i;
    int j;
    float x[100];
    float y[100];
    float zsa[100];
    float zsb[100];
    float ratio;

    printf("Testing vec_addf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    for (i = 1;  i < 90;  i++)
    {
        /* Force address misalignment, to check this works OK */
        vec_addf(zsa + 1, x + 1, y + 1, i);
        vec_addf_dumb(zsb + 1, x + 1, y + 1, i);
        for (j = 1;  j <= i;  j++)
        {
            ratio = zsa[j]/zsb[j];
            if (ratio < 0.9999f  ||  ratio > 1.0001f)
            {
                printf("vec_mulf() - %d %e %e\n", j, zsa[j], zsb[j]);
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_subf_dumb(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] - y[i];
}
/*- End of function --------------------------------------------------------*/

static int test_vec_subf(void)
{
    int i;
    int j;
    float x[100];
    float y[100];
    float zsa[100];
    float zsb[100];
    float ratio;

    printf("Testing vec_subf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    for (i = 1;  i < 90;  i++)
    {
        /* Force address misalignment, to check this works OK */
        vec_subf(zsa + 1, x + 1, y + 1, i);
        vec_subf_dumb(zsb + 1, x + 1, y + 1, i);
        for (j = 1;  j <= i;  j++)
        {
            ratio = zsa[j]/zsb[j];
            if (ratio < 0.9999f  ||  ratio > 1.0001f)
            {
                printf("vec_mulf() - %d %e %e\n", j, zsa[j], zsb[j]);
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_mulf_dumb(float z[], const float x[], const float y[], int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*y[i];
}
/*- End of function --------------------------------------------------------*/

static int test_vec_mulf(void)
{
    int i;
    int j;
    float x[100];
    float y[100];
    float zsa[100];
    float zsb[100];
    float ratio;

    printf("Testing vec_mulf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    for (i = 1;  i < 90;  i++)
    {
        /* Force address misalignment, to check this works OK */
        vec_mulf(zsa + 1, x + 1, y + 1, i);
        vec_mulf_dumb(zsb + 1, x + 1, y + 1, i);
        for (j = 1;  j <= i;  j++)
        {
            ratio = zsa[j]/zsb[j];
            if (ratio < 0.9999f  ||  ratio > 1.0001f)
            {
                printf("vec_mulf() - %d %e %e\n", j, zsa[j], zsb[j]);
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

#define LMS_LEAK_RATE 0.9999f

static void vec_lmsf_dumb(const float x[], float y[], int n, float error)
{
    int i;

    for (i = 0;  i < n;  i++)
    {
        /* Leak a little to tame uncontrolled wandering */
        y[i] = y[i]*LMS_LEAK_RATE + x[i]*error;
    }
}
/*- End of function --------------------------------------------------------*/

static int test_vec_lmsf(void)
{
    int i;
    int j;
    float x[100];
    float ya[100];
    float yb[100];
    float ratio;

    printf("Testing vec_lmsf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        ya[i] =
        yb[i] = rand();
    }
    for (i = 1;  i < 99;  i++)
    {
        vec_lmsf(x, ya, i, 0.1f);
        vec_lmsf_dumb(x, yb, i, 0.1f);
        for (j = 0;  j < i;  j++)
        {
            ratio = ya[j]/yb[j];
            if (ratio < 0.9999f  ||  ratio > 1.0001f)
            {
                printf("vec_lmsf() - %d %e %e\n", j, ya[j], yb[j]);
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_scaledxy_addf_dumb(float z[], const float x[], float x_scale, const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i]*x_scale + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

static int test_vec_scaledxy_addf(void)
{
    int i;
    int j;
    float x[100];
    float y[100];
    float za[100];
    float zb[100];
    float ratio;

    printf("Testing vec_scaledxy_addf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    for (i = 1;  i < 99;  i++)
    {
        vec_scaledxy_addf(za, x, 2.5f, y, 1.5f, i);
        vec_scaledxy_addf_dumb(zb, x, 2.5f, y, 1.5f, i);
        for (j = 0;  j < i;  j++)
        {
            ratio = za[j]/zb[j];
            if (ratio < 0.9999f  ||  ratio > 1.0001f)
            {
                printf("vec_scaledxy_addf() - %d %e %e\n", j, za[j], zb[j]);
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void vec_scaledy_addf_dumb(float z[], const float x[], const float y[], float y_scale, int n)
{
    int i;

    for (i = 0;  i < n;  i++)
        z[i] = x[i] + y[i]*y_scale;
}
/*- End of function --------------------------------------------------------*/

static int test_vec_scaledy_addf(void)
{
    int i;
    int j;
    float x[100];
    float y[100];
    float za[100];
    float zb[100];
    float ratio;

    printf("Testing vec_scaledy_addf()\n");
    for (i = 0;  i < 99;  i++)
    {
        x[i] = rand();
        y[i] = rand();
    }
    for (i = 1;  i < 99;  i++)
    {
        vec_scaledy_addf(za, x, y, 1.5f, i);
        vec_scaledy_addf_dumb(zb, x, y, 1.5f, i);
        for (j = 0;  j < i;  j++)
        {
            ratio = za[j]/zb[j];
            if (ratio < 0.9999f  ||  ratio > 1.0001f)
            {
                printf("vec_scaledy_addf() - %d %e %e\n", j, za[j], zb[j]);
                printf("Tests failed\n");
                exit(2);
            }
        }
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    test_vec_copyf();
    test_vec_negatef();
    test_vec_zerof();
    test_vec_setf();
    test_vec_addf();
    test_vec_subf();
    test_vec_mulf();
    test_vec_scaledxy_addf();
    test_vec_scaledy_addf();
    test_vec_dot_prod();
    test_vec_dot_prodf();
    test_vec_lmsf();

    printf("Tests passed.\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

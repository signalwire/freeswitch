/*
 * iLBC - a library for the iLBC codec
 *
 * lsf.c - The iLBC low bit rate speech codec.
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * iLBC code supplied in RFC3951.
 *
 * Original code Copyright (C) The Internet Society (2004).
 * All changes to produce this version Copyright (C) 2008 by Steve Underwood
 * All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: lsf.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <string.h>
#include <math.h>

#include "iLBC_define.h"
#include "lsf.h"

/*----------------------------------------------------------------*
 *  conversion from lpc coefficients to lsf coefficients
 *---------------------------------------------------------------*/

void a2lsf(float *freq,     /* (o) lsf coefficients */
           float *a)        /* (i) lpc coefficients */
{
    static const float steps[LSF_NUMBER_OF_STEPS] =
    {
        0.00635f, 0.003175f, 0.0015875f, 0.00079375f
    };
    float step;
    int step_idx;
    int lsp_index;
    float p[LPC_HALFORDER];
    float q[LPC_HALFORDER];
    float p_pre[LPC_HALFORDER];
    float q_pre[LPC_HALFORDER];
    float old_p;
    float old_q;
    float *old;
    float *pq_coef;
    float omega;
    float old_omega;
    int i;
    float hlp;
    float hlp1;
    float hlp2;
    float hlp3;
    float hlp4;
    float hlp5;

    for (i = 0;  i < LPC_HALFORDER;  i++)
    {
        p[i] = -1.0f*(a[i + 1] + a[ILBC_LPC_FILTERORDER - i]);
        q[i] = a[ILBC_LPC_FILTERORDER - i] - a[i + 1];
    }

    p_pre[0] = -1.0f - p[0];
    p_pre[1] = -p_pre[0] - p[1];
    p_pre[2] = -p_pre[1] - p[2];
    p_pre[3] = -p_pre[2] - p[3];
    p_pre[4] = -p_pre[3] - p[4];
    p_pre[4] = p_pre[4]/2.0f;

    q_pre[0] = 1.0f - q[0];
    q_pre[1] = q_pre[0] - q[1];
    q_pre[2] = q_pre[1] - q[2];
    q_pre[3] = q_pre[2] - q[3];
    q_pre[4] = q_pre[3] - q[4];
    q_pre[4] = q_pre[4]/2.0f;

    omega = 0.0f;
    old_omega = 0.0f;

    old_p = FLOAT_MAX;
    old_q = FLOAT_MAX;

    /* Here we loop through lsp_index to find all the ILBC_LPC_FILTERORDER roots for omega. */
    for (lsp_index = 0;  lsp_index < ILBC_LPC_FILTERORDER;  lsp_index++)
    {
        /* Depending on lsp_index being even or odd, we
           alternatively solve the roots for the two LSP equations. */
        if ((lsp_index & 0x1) == 0)
        {
            pq_coef = p_pre;
            old = &old_p;
        }
        else
        {
            pq_coef = q_pre;
            old = &old_q;
        }

        /* Start with low resolution grid */
        for (step_idx = 0, step = steps[step_idx];  step_idx < LSF_NUMBER_OF_STEPS;  )
        {
            /*  cos(10piw) + pq(0)cos(8piw) + pq(1)cos(6piw) +
                pq(2)cos(4piw) + pq(3)cod(2piw) + pq(4) */
            hlp = cosf(omega*TWO_PI);
            hlp1 = 2.0f*hlp+pq_coef[0];
            hlp2 = 2.0f*hlp*hlp1 - 1.0f + pq_coef[1];
            hlp3 = 2.0f*hlp*hlp2 - hlp1 + pq_coef[2];
            hlp4 = 2.0f*hlp*hlp3 - hlp2 + pq_coef[3];
            hlp5 = hlp*hlp4 - hlp3 + pq_coef[4];


            if (((hlp5 * (*old)) <= 0.0f) || (omega >= 0.5f))
            {
                if (step_idx == (LSF_NUMBER_OF_STEPS - 1))
                {
                    if (fabsf(hlp5) >= fabsf(*old))
                        freq[lsp_index] = omega - step;
                    else
                        freq[lsp_index] = omega;

                    if ((*old) >= 0.0f)
                        *old = -1.0f*FLOAT_MAX;
                    else
                        *old = FLOAT_MAX;

                    omega = old_omega;
                    step_idx = 0;

                    step_idx = LSF_NUMBER_OF_STEPS;
                }
                else
                {
                    if (step_idx == 0)
                        old_omega = omega;

                    step_idx++;
                    omega -= steps[step_idx];

                    /* Go back one grid step */
                    step = steps[step_idx];
                }
            }
            else
            {
                /* increment omega until they are of different sign,
                   and we know there is at least one root between omega
                   and old_omega */
                *old = hlp5;
                omega += step;
            }
        }
    }

    for (i = 0;  i < ILBC_LPC_FILTERORDER;  i++)
        freq[i] *= TWO_PI;
}

/*----------------------------------------------------------------*
 *  conversion from lsf coefficients to lpc coefficients
 *---------------------------------------------------------------*/

void lsf2a(float *a_coef,   /* (o) lpc coefficients */
           float *freq)     /* (i) lsf coefficients */
{
    int i;
    int j;
    float hlp;
    float p[LPC_HALFORDER];
    float q[LPC_HALFORDER];
    float a[LPC_HALFORDER + 1];
    float a1[LPC_HALFORDER];
    float a2[LPC_HALFORDER];
    float b[LPC_HALFORDER + 1];
    float b1[LPC_HALFORDER];
    float b2[LPC_HALFORDER];

    for (i = 0;  i < ILBC_LPC_FILTERORDER;  i++)
        freq[i] *= PI2;

    /* Check input for ill-conditioned cases.  This part is not
       found in the TIA standard.  It involves the following 2 IF
       blocks.  If "freq" is judged ill-conditioned, then we first
       modify freq[0] and freq[LPC_HALFORDER-1] (normally
       LPC_HALFORDER = 10 for LPC applications), then we adjust
       the other "freq" values slightly */

    if ((freq[0] <= 0.0f)  ||  (freq[ILBC_LPC_FILTERORDER - 1] >= 0.5f))
    {
        if (freq[0] <= 0.0f)
            freq[0] = 0.022f;


        if (freq[ILBC_LPC_FILTERORDER - 1] >= 0.5f)
            freq[ILBC_LPC_FILTERORDER - 1] = 0.499f;

        hlp = (freq[ILBC_LPC_FILTERORDER - 1] - freq[0])/(float) (ILBC_LPC_FILTERORDER - 1);

        for (i = 1;  i < ILBC_LPC_FILTERORDER;  i++)
            freq[i] = freq[i - 1] + hlp;
    }

    memset(a1, 0, LPC_HALFORDER*sizeof(float));
    memset(a2, 0, LPC_HALFORDER*sizeof(float));
    memset(b1, 0, LPC_HALFORDER*sizeof(float));
    memset(b2, 0, LPC_HALFORDER*sizeof(float));
    memset(a, 0, (LPC_HALFORDER+1)*sizeof(float));
    memset(b, 0, (LPC_HALFORDER+1)*sizeof(float));

    /* p[i] and q[i] compute cos(2*pi*omega_{2j}) and
       cos(2*pi*omega_{2j-1} in eqs. 4.2.2.2-1 and 4.2.2.2-2.
       Note that for this code p[i] specifies the coefficients
       used in .Q_A(z) while q[i] specifies the coefficients used
       in .P_A(z) */

    for (i = 0;  i < LPC_HALFORDER;  i++)
    {
        p[i] = cosf(TWO_PI*freq[2*i]);
        q[i] = cosf(TWO_PI*freq[2*i + 1]);
    }

    a[0] = 0.25f;
    b[0] = 0.25f;

    for (i = 0;  i < LPC_HALFORDER;  i++)
    {
        a[i + 1] = a[i] - 2*p[i]*a1[i] + a2[i];
        b[i + 1] = b[i] - 2*q[i]*b1[i] + b2[i];
        a2[i] = a1[i];
        a1[i] = a[i];
        b2[i] = b1[i];
        b1[i] = b[i];
    }

    for (j = 0;  j < ILBC_LPC_FILTERORDER;  j++)
    {
        if (j == 0)
        {
            a[0] = 0.25f;
            b[0] = -0.25f;
        }
        else
        {
            a[0] =
            b[0] = 0.0f;
        }

        for (i = 0;  i < LPC_HALFORDER;  i++)
        {
            a[i + 1] = a[i] - 2.0f*p[i]*a1[i] + a2[i];
            b[i + 1] = b[i] - 2.0f*q[i]*b1[i] + b2[i];
            a2[i] = a1[i];
            a1[i] = a[i];
            b2[i] = b1[i];
            b1[i] = b[i];
        }

        a_coef[j + 1] = 2.0f*(a[LPC_HALFORDER] + b[LPC_HALFORDER]);
    }

    a_coef[0] = 1.0f;
}

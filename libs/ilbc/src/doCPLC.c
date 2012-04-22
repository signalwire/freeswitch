/*
 * iLBC - a library for the iLBC codec
 *
 * doCPLC.c - The iLBC low bit rate speech codec.
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
 * $Id: doCPLC.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "ilbc.h"
#include "doCPLC.h"

/*----------------------------------------------------------------*
 *  Compute cross correlation and pitch gain for pitch prediction
 *  of last subframe at given lag.
 *---------------------------------------------------------------*/

static void compCorr(float *cc,        /* (o) cross correlation coefficient */
                     float *gc,        /* (o) gain */
                     float *pm,
                     float *buffer,    /* (i) signal buffer */
                     int lag,          /* (i) pitch lag */
                     int bLen,         /* (i) length of buffer */
                     int sRange)       /* (i) correlation search length */
{
    int i;
    float ftmp1;
    float ftmp2;
    float ftmp3;

    /* Guard against getting outside buffer */
    if ((bLen - sRange - lag) < 0)
        sRange = bLen - lag;

    ftmp1 = 0.0f;
    ftmp2 = 0.0f;
    ftmp3 = 0.0f;
    for (i = 0;  i < sRange;  i++)
    {
        ftmp1 += buffer[bLen - sRange + i]*buffer[bLen - sRange + i - lag];
        ftmp2 += buffer[bLen - sRange + i - lag]*buffer[bLen - sRange + i - lag];
        ftmp3 += buffer[bLen - sRange + i]*buffer[bLen - sRange + i];
    }

    if (ftmp2 > 0.0f)
    {
        *cc = ftmp1*ftmp1/ftmp2;
        *gc = fabsf(ftmp1/ftmp2);
        *pm = fabsf(ftmp1)/(sqrtf(ftmp2)*sqrtf(ftmp3));
    }
    else
    {
        *cc = 0.0f;
        *gc = 0.0f;
        *pm = 0.0f;
    }
}

/*----------------------------------------------------------------*
 *  Packet loss concealment routine. Conceals a residual signal
 *  and LP parameters. If no packet loss, update state.
 *---------------------------------------------------------------*/

void doThePLC(float *PLCresidual,                   /* (o) concealed residual */
              float *PLClpc,                        /* (o) concealed LP parameters */
              int PLI,                              /* (i) packet loss indicator
                                                           0 - no PL, 1 = PL */
              float *decresidual,                   /* (i) decoded residual */
              float *lpc,                           /* (i) decoded LPC (only used for no PL) */
              int inlag,                            /* (i) pitch lag */
              ilbc_decode_state_t *iLBCdec_inst)    /* (i/o) decoder instance */
{
    int lag = 20l;
    int randlag;
    float gain;
    float maxcc;
    float use_gain;
    float gain_comp, maxcc_comp, per, max_per = 0;
    int i;
    int pick;
    int use_lag;
    float ftmp;
    float randvec[ILBC_BLOCK_LEN_MAX];
    float pitchfact;
    float energy;

    /* Packet Loss */
    if (PLI == 1)
    {
        iLBCdec_inst->consPLICount += 1;

        if (iLBCdec_inst->prevPLI != 1)
        {
            /* previous frame not lost, determine pitch pred. gain */
            /* Search around the previous lag to find the best pitch period */

            lag = inlag - 3;
            compCorr(&maxcc,
                     &gain,
                     &max_per,
                     iLBCdec_inst->prevResidual,
                     lag,
                     iLBCdec_inst->blockl,
                     60);
            for (i = inlag - 2;  i <= inlag + 3;  i++)
            {
                compCorr(&maxcc_comp,
                         &gain_comp,
                         &per,
                         iLBCdec_inst->prevResidual,
                         i,
                         iLBCdec_inst->blockl,
                         60);
                if (maxcc_comp>maxcc)
                {
                    maxcc = maxcc_comp;
                    gain = gain_comp;
                    lag = i;
                    max_per = per;
                }
            }
        }
        else
        {
            /* previous frame lost, use recorded lag and periodicity */
            lag = iLBCdec_inst->prevLag;
            max_per = iLBCdec_inst->per;
        }

        /* downscaling */
        use_gain = 1.0f;
        if (iLBCdec_inst->consPLICount*iLBCdec_inst->blockl > 320)
            use_gain = 0.9f;
        else if (iLBCdec_inst->consPLICount*iLBCdec_inst->blockl > 2*320)
            use_gain = 0.7f;
        else if (iLBCdec_inst->consPLICount*iLBCdec_inst->blockl > 3*320)
            use_gain = 0.5f;
        else if (iLBCdec_inst->consPLICount*iLBCdec_inst->blockl > 4*320)
            use_gain = 0.0f;

        /* mix noise and pitch repeatition */
        ftmp = sqrtf(max_per);
        if (ftmp > 0.7f)
            pitchfact = 1.0f;
        else if (ftmp > 0.4f)
            pitchfact = (ftmp - 0.4f)/(0.7f - 0.4f);
        else
            pitchfact = 0.0f;

        /* avoid repetition of same pitch cycle */
        use_lag = lag;
        if (lag < 80)
            use_lag = 2*lag;

        /* compute concealed residual */
        energy = 0.0f;
        for (i = 0;  i < iLBCdec_inst->blockl;  i++)
        {
            /* noise component */
            iLBCdec_inst->seed = (iLBCdec_inst->seed*69069L + 1) & (0x80000000L - 1);
            randlag = 50 + ((signed long) iLBCdec_inst->seed)%70;
            pick = i - randlag;

            if (pick < 0)
                randvec[i] = iLBCdec_inst->prevResidual[iLBCdec_inst->blockl + pick];
            else
                randvec[i] = randvec[pick];

            /* pitch repeatition component */
            pick = i - use_lag;

            if (pick < 0)
                PLCresidual[i] = iLBCdec_inst->prevResidual[iLBCdec_inst->blockl + pick];
            else
                PLCresidual[i] = PLCresidual[pick];

            /* mix random and periodicity component */
            if (i < 80)
                PLCresidual[i] = use_gain*(pitchfact*PLCresidual[i] + (1.0f - pitchfact) * randvec[i]);
            else if (i < 160)
                PLCresidual[i] = 0.95f*use_gain*(pitchfact*PLCresidual[i] + (1.0f - pitchfact) * randvec[i]);
            else
                PLCresidual[i] = 0.9f*use_gain*(pitchfact*PLCresidual[i] + (1.0f - pitchfact) * randvec[i]);
            energy += PLCresidual[i] * PLCresidual[i];
        }

        /* less than 30 dB, use only noise */
        if (sqrt(energy/(float) iLBCdec_inst->blockl) < 30.0f)
        {
            gain = 0.0f;
            for (i = 0;  i < iLBCdec_inst->blockl;  i++)
                PLCresidual[i] = randvec[i];
        }

        /* use old LPC */
        memcpy(PLClpc, iLBCdec_inst->prevLpc, (ILBC_LPC_FILTERORDER + 1)*sizeof(float));
    }
    else
    {
        /* no packet loss, copy input */
        memcpy(PLCresidual, decresidual, iLBCdec_inst->blockl*sizeof(float));
        memcpy(PLClpc, lpc, (ILBC_LPC_FILTERORDER + 1)*sizeof(float));
        iLBCdec_inst->consPLICount = 0;
    }

    /* update state */
    if (PLI)
    {
        iLBCdec_inst->prevLag = lag;
        iLBCdec_inst->per = max_per;
    }

    iLBCdec_inst->prevPLI = PLI;
    memcpy(iLBCdec_inst->prevLpc, PLClpc, (ILBC_LPC_FILTERORDER + 1)*sizeof(float));
    memcpy(iLBCdec_inst->prevResidual, PLCresidual, iLBCdec_inst->blockl*sizeof(float));
}

/*
 * iLBC - a library for the iLBC codec
 *
 * iCBSearch.c - The iLBC low bit rate speech codec.
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
 * $Id: iCBSearch.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "ilbc.h"
#include "gainquant.h"
#include "createCB.h"
#include "filter.h"
#include "constants.h"
#include "iCBSearch.h"

/*----------------------------------------------------------------*
 *  Search routine for codebook encoding and gain quantization.
 *---------------------------------------------------------------*/

void iCBSearch(ilbc_encode_state_t *iLBCenc_inst,   /* (i) the encoder state structure */
               int *index,          /* (o) Codebook indices */
               int *gain_index,     /* (o) Gain quantization indices */
               float *intarget,     /* (i) Target vector for encoding */
               float *mem,          /* (i) Buffer for codebook construction */
               int lMem,            /* (i) Length of buffer */
               int lTarget,         /* (i) Length of vector */
               int nStages,         /* (i) Number of codebook stages */
               float *weightDenum,  /* (i) weighting filter coefficients */
               float *weightState,  /* (i) weighting filter state */
               int block)           /* (i) the sub-block number */
{
    int i;
    int j;
    int icount;
    int stage;
    int best_index;
    int range;
    int counter;
    float max_measure;
    float gain;
    float measure;
    float crossDot;
    float ftmp;
    float gains[CB_NSTAGES];
    float target[SUBL];
    int base_index;
    int sInd;
    int eInd;
    int base_size;
    int sIndAug = 0;
    int eIndAug = 0;
    float buf[CB_MEML + SUBL + 2*ILBC_LPC_FILTERORDER];
    float invenergy[CB_EXPAND*128];
    float energy[CB_EXPAND*128];
    float *pp;
    float *ppi = 0;
    float *ppo = 0;
    float *ppe = 0;
    float cbvectors[CB_MEML];
    float tene;
    float cene;
    float cvec[SUBL];
    float aug_vec[SUBL];

    memset(cvec, 0, SUBL*sizeof(float));

    /* Determine size of codebook sections */
    base_size = lMem - lTarget + 1;

    if (lTarget == SUBL)
        base_size = lMem - lTarget + 1 + lTarget/2;

    /* setup buffer for weighting */
    memcpy(buf, weightState, sizeof(float)*ILBC_LPC_FILTERORDER);
    memcpy(buf + ILBC_LPC_FILTERORDER, mem, lMem*sizeof(float));
    memcpy(buf + ILBC_LPC_FILTERORDER + lMem, intarget, lTarget*sizeof(float));

    /* weighting */
    AllPoleFilter(buf + ILBC_LPC_FILTERORDER, weightDenum, lMem + lTarget, ILBC_LPC_FILTERORDER);

    /* Construct the codebook and target needed */
    memcpy(target, buf + ILBC_LPC_FILTERORDER + lMem, lTarget*sizeof(float));

    tene = 0.0f;
    for (i = 0;  i < lTarget;  i++)
        tene += target[i]*target[i];

    /* Prepare search over one more codebook section. This section
       is created by filtering the original buffer with a filter. */

    filteredCBvecs(cbvectors, buf + ILBC_LPC_FILTERORDER, lMem);

    /* The Main Loop over stages */
    for (stage = 0;  stage < nStages;  stage++)
    {
        range = search_rangeTbl[block][stage];

        /* initialize search measure */
        max_measure = -10000000.0f;
        gain = 0.0f;
        best_index = 0;

        /* Compute cross dot product between the target and the CB memory */

        crossDot = 0.0f;
        pp = buf + ILBC_LPC_FILTERORDER + lMem - lTarget;
        for (j = 0;  j < lTarget;  j++)
        {
            crossDot += target[j]*(*pp++);
        }

        if (stage == 0)
        {
            /* Calculate energy in the first block of 'lTarget' samples. */
            ppe = energy;
            ppi = buf + ILBC_LPC_FILTERORDER + lMem - lTarget - 1;
            ppo = buf + ILBC_LPC_FILTERORDER + lMem - 1;

            *ppe = 0.0f;
            pp = buf + ILBC_LPC_FILTERORDER + lMem - lTarget;
            for (j = 0;  j < lTarget;  j++, pp++)
                *ppe += (*pp)*(*pp);

            if (*ppe > 0.0)
            {
                invenergy[0] = 1.0f/(*ppe + EPS);
            }
            else
            {
                invenergy[0] = 0.0f;
            }
            ppe++;

            measure = -10000000.0f;

            if (crossDot > 0.0f)
                measure = crossDot*crossDot*invenergy[0];
        }
        else
        {
            measure = crossDot*crossDot*invenergy[0];
        }

        /* Check if measure is better */
        ftmp = crossDot*invenergy[0];

        if ((measure>max_measure)  &&  (fabs(ftmp) < CB_MAXGAIN))
        {
            best_index = 0;
            max_measure = measure;
            gain = ftmp;
        }

        /* loop over the main first codebook section, full search */
        for (icount = 1;  icount < range;  icount++)
        {
            /* calculate measure */
            crossDot = 0.0f;
            pp = buf + ILBC_LPC_FILTERORDER + lMem - lTarget - icount;

            for (j = 0;  j < lTarget;  j++)
                crossDot += target[j]*(*pp++);

            if (stage == 0)
            {
                *ppe++ = energy[icount-1] + (*ppi)*(*ppi) - (*ppo)*(*ppo);
                ppo--;
                ppi--;

                if (energy[icount] > 0.0f)
                {
                    invenergy[icount] = 1.0f/(energy[icount] + EPS);
                }
                else
                {
                    invenergy[icount] = 0.0f;
                }
                measure = -10000000.0f;

                if (crossDot > 0.0f)
                    measure = crossDot*crossDot*invenergy[icount];
            }
            else
            {
                measure = crossDot*crossDot*invenergy[icount];
            }

            /* check if measure is better */
            ftmp = crossDot*invenergy[icount];

            if ((measure > max_measure)  &&  (fabs(ftmp) < CB_MAXGAIN))
            {
                best_index = icount;
                max_measure = measure;
                gain = ftmp;
            }
        }

        /* Loop over augmented part in the first codebook
         * section, full search.
         * The vectors are interpolated.
         */
        if (lTarget == SUBL)
        {
            /* Search for best possible cb vector and compute the CB-vectors' energy. */
            searchAugmentedCB(20, 39, stage, base_size - lTarget/2,
                              target, buf + ILBC_LPC_FILTERORDER + lMem,
                              &max_measure, &best_index, &gain, energy,
                              invenergy);
        }

        /* set search range for following codebook sections */
        base_index = best_index;

        /* unrestricted search */
        if (CB_RESRANGE == -1)
        {
            sInd = 0;
            eInd = range - 1;
            sIndAug = 20;
            eIndAug = 39;
        }
        /* restricted search around best index from first codebook section */
        else
        {
            /* Initialize search indices */
            sIndAug = 0;
            eIndAug = 0;
            sInd = base_index - CB_RESRANGE/2;
            eInd = sInd + CB_RESRANGE;

            if (lTarget == SUBL)
            {
                if (sInd < 0)
                {
                    sIndAug = 40 + sInd;
                    eIndAug = 39;
                    sInd=0;
                }
                else if (base_index < (base_size - 20))
                {
                    if (eInd > range)
                    {
                        sInd -= (eInd-range);
                        eInd = range;
                    }
                }
                else
                {
                    /* base_index >= (base_size-20) */
                    if (sInd < (base_size - 20))
                    {
                        sIndAug = 20;
                        sInd = 0;
                        eInd = 0;
                        eIndAug = 19 + CB_RESRANGE;

                        if (eIndAug > 39)
                        {
                            eInd = eIndAug - 39;
                            eIndAug = 39;
                        }
                    }
                    else
                    {
                        sIndAug = 20 + sInd - (base_size - 20);
                        eIndAug = 39;
                        sInd = 0;
                        eInd = CB_RESRANGE - (eIndAug - sIndAug + 1);
                    }
                }

            }
            else
            {
                /* lTarget = 22 or 23 */
                if (sInd < 0)
                {
                    eInd -= sInd;
                    sInd = 0;
                }

                if (eInd > range)
                {
                    sInd -= (eInd - range);
                    eInd = range;
                }
            }
        }

        /* search of higher codebook section */

        /* index search range */
        counter = sInd;
        sInd += base_size;
        eInd += base_size;


        if (stage == 0)
        {
            ppe = energy+base_size;
            *ppe = 0.0f;
            pp = cbvectors + lMem - lTarget;
            for (j = 0;  j < lTarget;  j++, pp++)
                *ppe += (*pp)*(*pp);

            ppi = cbvectors + lMem - 1 - lTarget;
            ppo = cbvectors + lMem - 1;

            for (j = 0;  j < (range - 1);  j++)
            {
                *(ppe+1) = *ppe + (*ppi)*(*ppi) - (*ppo)*(*ppo);
                ppo--;
                ppi--;
                ppe++;
            }
        }

        /* loop over search range */
        for (icount = sInd;  icount < eInd;  icount++)
        {
            /* calculate measure */
            crossDot = 0.0f;
            pp = cbvectors + lMem - (counter++) - lTarget;

            for (j = 0;  j < lTarget;  j++)
                crossDot += target[j]*(*pp++);

            if (energy[icount] > 0.0f)
                invenergy[icount] = 1.0f/(energy[icount] + EPS);
            else
                invenergy[icount] = 0.0f;

            if (stage == 0)
            {

                measure = -10000000.0f;

                if (crossDot > 0.0f)
                    measure = crossDot*crossDot*invenergy[icount];
            }
            else
            {
                measure = crossDot*crossDot*invenergy[icount];
            }

            /* check if measure is better */
            ftmp = crossDot*invenergy[icount];

            if ((measure > max_measure)  &&  (fabs(ftmp) < CB_MAXGAIN))
            {
                best_index = icount;
                max_measure = measure;
                gain = ftmp;
            }
        }

        /* Search the augmented CB inside the limited range. */
        if ((lTarget == SUBL)  &&  (sIndAug != 0))
        {
            searchAugmentedCB(sIndAug, eIndAug, stage,
                              2*base_size-20, target, cbvectors + lMem,
                              &max_measure, &best_index, &gain, energy,
                              invenergy);
        }

        /* record best index */
        index[stage] = best_index;

        /* gain quantization */
        if (stage == 0)
        {
            if (gain < 0.0f)
                gain = 0.0f;

            if (gain > CB_MAXGAIN)
                gain = (float) CB_MAXGAIN;
            gain = gainquant(gain, 1.0, 32, &gain_index[stage]);
        }
        else
        {
            if (stage == 1)
                gain = gainquant(gain, fabsf(gains[stage - 1]), 16, &gain_index[stage]);
            else
                gain = gainquant(gain, fabsf(gains[stage - 1]), 8, &gain_index[stage]);
        }

        /* Extract the best (according to measure) codebook vector */
        if (lTarget == (STATE_LEN - iLBCenc_inst->state_short_len))
        {
            if (index[stage] < base_size)
                pp = buf + ILBC_LPC_FILTERORDER + lMem - lTarget - index[stage];
            else
                pp = cbvectors + lMem - lTarget - index[stage] + base_size;
        }
        else
        {
            if (index[stage] < base_size)
            {
                if (index[stage] < (base_size - 20))
                {
                    pp = buf + ILBC_LPC_FILTERORDER + lMem - lTarget - index[stage];
                }
                else
                {
                    createAugmentedVec(index[stage] - base_size + 40,
                                       buf + ILBC_LPC_FILTERORDER + lMem,
                                       aug_vec);
                    pp = aug_vec;
                }
            }
            else
            {
                int filterno, position;

                filterno = index[stage]/base_size;
                position = index[stage] - filterno*base_size;
                if (position < (base_size - 20))
                {
                    pp = cbvectors + filterno*lMem - lTarget - index[stage] + filterno*base_size;
                }
                else
                {
                    createAugmentedVec(index[stage] - (filterno + 1)*base_size + 40,
                                       cbvectors + filterno*lMem,
                                       aug_vec);
                    pp = aug_vec;
                }
            }
        }

        /* Subtract the best codebook vector, according to measure, from the target vector */
        for (j = 0;  j < lTarget;  j++)
        {
            cvec[j] += gain*(*pp);
            target[j] -= gain*(*pp++);
        }

        /* record quantized gain */
        gains[stage] = gain;
    }

    /* Gain adjustment for energy matching */
    cene = 0.0f;
    for (i = 0;  i < lTarget;  i++)
        cene += cvec[i]*cvec[i];
    j = gain_index[0];

    for (i = gain_index[0];  i < 32;  i++)
    {
        ftmp = cene*gain_sq5Tbl[i]*gain_sq5Tbl[i];

        if ((ftmp < (tene*gains[0]*gains[0]))  &&  (gain_sq5Tbl[j] < (2.0*gains[0])))
            j = i;
    }
    gain_index[0] = j;
}

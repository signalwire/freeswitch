/*
 * iLBC - a library for the iLBC codec
 *
 * createCB.c - The iLBC low bit rate speech codec.
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
 * $Id: createCB.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "ilbc.h"
#include "constants.h"
#include "createCB.h"

/*----------------------------------------------------------------*
 *  Construct an additional codebook vector by filtering the
 *  initial codebook buffer. This vector is then used to expand
 *  the codebook with an additional section.
 *---------------------------------------------------------------*/

void filteredCBvecs(float *cbvectors,   /* (o) Codebook vectors for the
                                               higher section */
                    float *mem,         /* (i) Buffer to create codebook
                                               vector from */
                    int lMem)           /* (i) Length of buffer */
{
    int j;
    int k;
    const float *pp;
    const float *pp1;
    float tempbuff2[CB_MEML + CB_FILTERLEN];
    float *pos;

    memset(tempbuff2, 0, (CB_HALFFILTERLEN - 1)*sizeof(float));
    memcpy(&tempbuff2[CB_HALFFILTERLEN - 1], mem, lMem*sizeof(float));
    memset(&tempbuff2[lMem + CB_HALFFILTERLEN - 1], 0, (CB_HALFFILTERLEN + 1)*sizeof(float));

    /* Create codebook vector for higher section by filtering */

    /* do filtering */
    pos = cbvectors;
    memset(pos, 0, lMem*sizeof(float));
    for (k = 0;  k < lMem;  k++)
    {
        pp = &tempbuff2[k];
        pp1 = &cbfiltersTbl[CB_FILTERLEN - 1];
        for (j = 0;  j < CB_FILTERLEN;  j++)
            (*pos) += (*pp++)*(*pp1--);
        pos++;
    }
}

/*----------------------------------------------------------------*
 *  Search the augmented part of the codebook to find the best
 *  measure.
 *----------------------------------------------------------------*/

void searchAugmentedCB(int low,             /* (i) Start index for the search */
                       int high,            /* (i) End index for the search */
                       int stage,           /* (i) Current stage */
                       int startIndex,      /* (i) Codebook index for the first
                                                   aug vector */
                       float *target,       /* (i) Target vector for encoding */
                       float *buffer,       /* (i) Pointer to the end of the buffer for
                                                   augmented codebook construction */
                       float *max_measure,  /* (i/o) Currently maximum measure */
                       int *best_index,     /* (o) Currently the best index */
                       float *gain,         /* (o) Currently the best gain */
                       float *energy,       /* (o) Energy of augmented codebook
                                                   vectors */
                       float *invenergy)    /* (o) Inv energy of augmented codebook
                                                   vectors */
{
    int icount;
    int ilow;
    int j;
    int tmpIndex;
    float *pp;
    float *ppo;
    float *ppi;
    float *ppe;
    float crossDot;
    float alfa;
    float weighted;
    float measure;
    float nrjRecursive;
    float ftmp;

    /* Compute the energy for the first (low-5)
       noninterpolated samples */
    nrjRecursive = 0.0f;
    pp = buffer - low + 1;
    for (j = 0;  j < (low - 5);  j++)
    {
        nrjRecursive += ((*pp)*(*pp));
        pp++;
    }
    ppe = buffer - low;

    for (icount = low;  icount <= high;  icount++)
    {
        /* Index of the codebook vector used for retrieving
           energy values */
        tmpIndex = startIndex+icount - 20;

        ilow = icount - 4;

        /* Update the energy recursively to save complexity */
        nrjRecursive = nrjRecursive + (*ppe)*(*ppe);
        ppe--;
        energy[tmpIndex] = nrjRecursive;

        /* Compute cross dot product for the first (low - 5) samples */

        crossDot = 0.0f;
        pp = buffer - icount;
        for (j = 0;  j < ilow;  j++)
            crossDot += target[j]*(*pp++);

        /* interpolation */
        alfa = 0.2f;
        ppo = buffer - 4;
        ppi = buffer - icount - 4;
        for (j = ilow;  j < icount;  j++)
        {
            weighted = (1.0f - alfa)*(*ppo) +alfa*(*ppi);
            ppo++;
            ppi++;
            energy[tmpIndex] += weighted*weighted;
            crossDot += target[j]*weighted;
            alfa += 0.2f;
        }

        /* Compute energy and cross dot product for the
           remaining samples */
        pp = buffer - icount;
        for (j = icount;  j < SUBL;  j++)
        {
            energy[tmpIndex] += (*pp)*(*pp);
            crossDot += target[j]*(*pp++);
        }

        if (energy[tmpIndex] > 0.0f)
        {
            invenergy[tmpIndex] = 1.0f/(energy[tmpIndex] + EPS);
        }
        else
        {
            invenergy[tmpIndex] = 0.0f;
        }

        if (stage == 0)
        {
            measure = -10000000.0f;

            if (crossDot > 0.0f)
                measure = crossDot*crossDot*invenergy[tmpIndex];
        }
        else
        {
            measure = crossDot*crossDot*invenergy[tmpIndex];
        }

        /* check if measure is better */
        ftmp = crossDot*invenergy[tmpIndex];

        if ((measure > *max_measure) && (fabsf(ftmp) < CB_MAXGAIN))
        {
            *best_index = tmpIndex;
            *max_measure = measure;
            *gain = ftmp;
        }
    }
}

/*----------------------------------------------------------------*
 *  Recreate a specific codebook vector from the augmented part.
 *
 *----------------------------------------------------------------*/

void createAugmentedVec(int index,      /* (i) Index for the augmented vector
                                               to be created */
                        float *buffer,  /* (i) Pointer to the end of the buffer for
                                               augmented codebook construction */
                        float *cbVec)   /* (o) The construced codebook vector */
{
    int ilow;
    int j;
    float *pp;
    float *ppo;
    float *ppi;
    float alfa;
    float alfa1;
    float weighted;

    ilow = index - 5;

    /* copy the first noninterpolated part */

    pp = buffer - index;
    memcpy(cbVec, pp, sizeof(float)*index);

    /* interpolation */

    alfa1 = 0.2f;
    alfa = 0.0f;
    ppo = buffer - 5;
    ppi = buffer - index - 5;
    for (j = ilow;  j < index;  j++)
    {
        weighted = (1.0f - alfa)*(*ppo) + alfa*(*ppi);
        ppo++;
        ppi++;
        cbVec[j] = weighted;
        alfa += alfa1;
    }

    /* copy the second noninterpolated part */

    pp = buffer - index;
    memcpy(cbVec + index, pp, sizeof(float)*(SUBL - index));
}

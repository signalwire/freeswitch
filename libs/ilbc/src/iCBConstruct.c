/*
 * iLBC - a library for the iLBC codec
 *
 * iCBConstruct.c - The iLBC low bit rate speech codec.
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
 * $Id: iCBConstruct.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>

#include "ilbc.h"
#include "gainquant.h"
#include "getCBvec.h"
#include "iCBConstruct.h"

/*----------------------------------------------------------------*
 *  Convert the codebook indexes to make the search easier
 *---------------------------------------------------------------*/

void index_conv_enc(int *index)     /* (i/o) Codebook indexes */
{
    int k;

    for (k = 1;  k < CB_NSTAGES;  k++)
    {
        if ((index[k] >= 108)  &&  (index[k] < 172))
        {
            index[k] -= 64;
        }
        else if (index[k] >= 236)
        {
            index[k] -= 128;
        }
        else
        {
            /* ERROR */
        }
    }
}

void index_conv_dec(int *index)     /* (i/o) Codebook indexes */
{
    int k;

    for (k = 1;  k < CB_NSTAGES;  k++)
    {
        if ((index[k] >= 44)  &&  (index[k] < 108))
        {
            index[k] += 64;
        }
        else if ((index[k] >= 108)  &&  (index[k] < 128))
        {
            index[k] += 128;
        }
        else
        {
            /* ERROR */
        }
    }
}

/*----------------------------------------------------------------*
 *  Construct decoded vector from codebook and gains.
 *---------------------------------------------------------------*/

void iCBConstruct(float *decvector, /* (o) Decoded vector */
                  int *index,       /* (i) Codebook indices */
                  int *gain_index,  /* (i) Gain quantization indices */
                  float *mem,       /* (i) Buffer for codevector construction */
                  int lMem,         /* (i) Length of buffer */
                  int veclen,       /* (i) Length of vector */
                  int nStages)      /* (i) Number of codebook stages */
{
    int j;
    int k;
    float gain[CB_NSTAGES];
    float cbvec[SUBL];

    /* Gain de-quantization */
    gain[0] = gaindequant(gain_index[0], 1.0, 32);
    if (nStages > 1)
        gain[1] = gaindequant(gain_index[1], fabsf(gain[0]), 16);
    if (nStages > 2)
        gain[2] = gaindequant(gain_index[2], fabsf(gain[1]), 8);

    /* Codebook vector construction and construction of total vector */
    getCBvec(cbvec, mem, index[0], lMem, veclen);
    for (j = 0;  j < veclen;  j++)
        decvector[j] = gain[0]*cbvec[j];
    if (nStages > 1)
    {
        for (k = 1;  k < nStages;  k++)
        {
            getCBvec(cbvec, mem, index[k], lMem, veclen);
            for (j = 0;  j < veclen;  j++)
                decvector[j] += gain[k]*cbvec[j];
        }
    }
}

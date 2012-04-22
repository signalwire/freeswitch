/*
 * iLBC - a library for the iLBC codec
 *
 * gainquant.c - The iLBC low bit rate speech codec.
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
 * $Id: gainquant.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "constants.h"
#include "filter.h"
#include "gainquant.h"

/*----------------------------------------------------------------*
 *  quantizer for the gain in the gain-shape coding of residual
 *---------------------------------------------------------------*/

float gainquant(                /* (o) quantized gain value */
                float in,       /* (i) gain value */
                float maxIn,    /* (i) maximum of gain value */
                int cblen,      /* (i) number of quantization indices */
                int *index)     /* (o) quantization index */
{
    int i;
    int tindex;
    float minmeasure;
    float measure;
    const float *cb;
    float scale;

    /* ensure a lower bound on the scaling factor */
    scale = maxIn;

    if (scale < 0.1f)
        scale = 0.1f;

    /* select the quantization table */
    if (cblen == 8)
        cb = gain_sq3Tbl;
    else if (cblen == 16)
        cb = gain_sq4Tbl;
    else
        cb = gain_sq5Tbl;

    /* select the best index in the quantization table */
    minmeasure = 10000000.0f;
    tindex = 0;
    for (i = 0;  i < cblen;  i++)
    {
        measure = (in-scale*cb[i])*(in-scale*cb[i]);

        if (measure < minmeasure)
        {
            tindex = i;
            minmeasure = measure;
        }
    }
    *index = tindex;

    /* return the quantized value */
    return scale*cb[tindex];
}

/*----------------------------------------------------------------*
 *  decoder for quantized gains in the gain-shape coding of
 *  residual
 *---------------------------------------------------------------*/

float gaindequant(              /* (o) quantized gain value */
                  int index,    /* (i) quantization index */
                  float maxIn,  /* (i) maximum of unquantized gain */
                  int cblen)    /* (i) number of quantization indices */
{
    float scale;

    /* obtain correct scale factor */
    scale = fabsf(maxIn);

    if (scale < 0.1f)
        scale = 0.1f;

    /* select the quantization table and return the decoded value */
    if (cblen == 8)
        return scale*gain_sq3Tbl[index];
    if (cblen == 16)
        return scale*gain_sq4Tbl[index];
    if (cblen == 32)
        return scale*gain_sq5Tbl[index];

    return 0.0;
}

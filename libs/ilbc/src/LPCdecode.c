/*
 * iLBC - a library for the iLBC codec
 *
 * LPCdecode.c - The iLBC low bit rate speech codec.
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
 * $Id: LPCdecode.c,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>

#include "ilbc.h"
#include "helpfun.h"
#include "lsf.h"
#include "constants.h"
#include "LPCdecode.h"

/*---------------------------------------------------------------*
 *  interpolation of lsf coefficients for the decoder
 *--------------------------------------------------------------*/

void LSFinterpolate2a_dec(float *a,         /* (o) lpc coefficients for a sub-frame */
                          float *lsf1,      /* (i) first lsf coefficient vector */
                          float *lsf2,      /* (i) second lsf coefficient vector */
                          float coef,       /* (i) interpolation weight */
                          int length)       /* (i) length of lsf vectors */
{
    float  lsftmp[ILBC_LPC_FILTERORDER];

    interpolate(lsftmp, lsf1, lsf2, coef, length);
    lsf2a(a, lsftmp);
}

/*---------------------------------------------------------------*
 *  obtain dequantized lsf coefficients from quantization index
 *--------------------------------------------------------------*/

void SimplelsfDEQ(float *lsfdeq,        /* (o) dequantized lsf coefficients */
                  int *index,           /* (i) quantization index */
                  int lpc_n)            /* (i) number of LPCs */
{
    int i;
    int j;
    int pos;
    int cb_pos;

    /* decode first LSF */
    pos = 0;
    cb_pos = 0;
    for (i = 0;  i < LSF_NSPLIT;  i++)
    {
        for (j = 0;  j < dim_lsfCbTbl[i];  j++)
            lsfdeq[pos + j] = lsfCbTbl[cb_pos + (int32_t) (index[i])*dim_lsfCbTbl[i] + j];
        pos += dim_lsfCbTbl[i];
        cb_pos += size_lsfCbTbl[i]*dim_lsfCbTbl[i];
    }

    if (lpc_n>1)
    {
        /* decode last LSF */
        pos = 0;
        cb_pos = 0;
        for (i = 0;  i < LSF_NSPLIT;  i++)
        {
            for (j = 0;  j < dim_lsfCbTbl[i];  j++)
            {
                lsfdeq[ILBC_LPC_FILTERORDER + pos + j] =
                    lsfCbTbl[cb_pos + (int32_t) (index[LSF_NSPLIT + i])*dim_lsfCbTbl[i] + j];
            }
            pos += dim_lsfCbTbl[i];
            cb_pos += size_lsfCbTbl[i]*dim_lsfCbTbl[i];
        }
    }
}

/*----------------------------------------------------------------*
 *  obtain synthesis and weighting filters form lsf coefficients
 *---------------------------------------------------------------*/

void DecoderInterpolateLSF(float *syntdenum,                    /* (o) synthesis filter coefficients */
                           float *weightdenum,                  /* (o) weighting denumerator coefficients */
                           float *lsfdeq,                       /* (i) dequantized lsf coefficients */
                           int length,                          /* (i) length of lsf coefficient vector */
                           ilbc_decode_state_t *iLBCdec_inst)   /* (i) the decoder state structure */
{
    int i;
    int pos;
    int lp_length;
    float lp[ILBC_LPC_FILTERORDER + 1];
    float *lsfdeq2;

    lsfdeq2 = lsfdeq + length;
    lp_length = length + 1;

    if (iLBCdec_inst->mode == 30)
    {
        /* sub-frame 1: Interpolation between old and first */

        LSFinterpolate2a_dec(lp, iLBCdec_inst->lsfdeqold, lsfdeq,
                             lsf_weightTbl_30ms[0], length);
        memcpy(syntdenum,lp,lp_length*sizeof(float));
        bwexpand(weightdenum, lp, LPC_CHIRP_WEIGHTDENUM, lp_length);

        /* sub-frames 2 to 6: interpolation between first and last LSF */
        pos = lp_length;
        for (i = 1;  i < 6;  i++)
        {
            LSFinterpolate2a_dec(lp, lsfdeq, lsfdeq2, lsf_weightTbl_30ms[i], length);
            memcpy(syntdenum + pos,lp,lp_length*sizeof(float));
            bwexpand(weightdenum + pos, lp, LPC_CHIRP_WEIGHTDENUM, lp_length);
            pos += lp_length;
        }
    }
    else
    {
        pos = 0;
        for (i = 0;  i < iLBCdec_inst->nsub;  i++)
        {
            LSFinterpolate2a_dec(lp, iLBCdec_inst->lsfdeqold, lsfdeq, lsf_weightTbl_20ms[i], length);
            memcpy(syntdenum + pos, lp, lp_length*sizeof(float));
            bwexpand(weightdenum + pos, lp, LPC_CHIRP_WEIGHTDENUM, lp_length);
            pos += lp_length;
        }
    }

    /* update memory */
    if (iLBCdec_inst->mode == 30)
        memcpy(iLBCdec_inst->lsfdeqold, lsfdeq2, length*sizeof(float));
    else
        memcpy(iLBCdec_inst->lsfdeqold, lsfdeq, length*sizeof(float));
}

/*
 * iLBC - a library for the iLBC codec
 *
 * LPCdecode.h - The iLBC low bit rate speech codec.
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
 * $Id: LPCdecode.h,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

#ifndef __iLBC_LPCDECODE_H
#define __iLBC_LPCDECODE_H

void LSFinterpolate2a_dec(float *a,         /* (o) lpc coefficients for a sub-frame */
                          float *lsf1,      /* (i) first lsf coefficient vector */
                          float *lsf2,      /* (i) second lsf coefficient vector */
                          float coef,       /* (i) interpolation weight */
                          int length);      /* (i) length of lsf vectors */

void SimplelsfDEQ(float *lsfdeq,            /* (o) dequantized lsf coefficients */
                  int *index,               /* (i) quantization index */
                  int lpc_n);               /* (i) number of LPCs */

void DecoderInterpolateLSF(float *syntdenum,                    /* (o) synthesis filter coefficients */
                           float *weightdenum,                  /* (o) weighting denumerator coefficients */
                           float *lsfdeq,                       /* (i) dequantized lsf coefficients */
                           int length,                          /* (i) length of lsf coefficient vector */
                           ilbc_decode_state_t *iLBCdec_inst);  /* (i) the decoder state structure */

#endif

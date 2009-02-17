/*
 * iLBC - a library for the iLBC codec
 *
 * StateSearchW.h - The iLBC low bit rate speech codec.
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
 * $Id: StateSearchW.h,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

#ifndef __iLBC_STATESEARCHW_H
#define __iLBC_STATESEARCHW_H

void AbsQuantW(ilbc_encode_state_t *iLBCenc_inst,   /* (i) Encoder instance */
               float *in,                           /* (i) vector to encode */
               float *syntDenum,                    /* (i) denominator of synthesis filter */
               float *weightDenum,                  /* (i) denominator of weighting filter */
               int *out,                            /* (o) vector of quantizer indexes */
               int len,                             /* (i) length of vector to encode and
                                                           vector of quantizer indexes */
               int state_first);                    /* (i) position of start state in the 80 vec */

void StateSearchW(ilbc_encode_state_t *iLBCenc_inst,    /* (i) Encoder instance */
                  float *residual,                      /* (i) target residual vector */
                  float *syntDenum,                     /* (i) lpc synthesis filter */
                  float *weightDenum,                   /* (i) weighting filter denuminator */
                  int *idxForMax,                       /* (o) quantizer index for maximum
                                                               amplitude */
                  int *idxVec,                          /* (o) vector of quantization indexes */
                  int len,                              /* (i) length of all vectors */
                  int state_first);                     /* (i) position of start state in the 80 vec */

#endif

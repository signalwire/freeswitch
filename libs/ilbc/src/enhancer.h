/*
 * iLBC - a library for the iLBC codec
 *
 * enhancer.h - The iLBC low bit rate speech codec.
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
 * $Id: enhancer.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __ENHANCER_H
#define __ENHANCER_H

float xCorrCoef(float *target,                              /* (i) first array */
                float *regressor,                           /* (i) second array */
                int subl);                                  /* (i) dimension arrays */

int enhancerInterface(float *out,                           /* (o) the enhanced recidual signal */
                      float *in,                            /* (i) the recidual signal to enhance */
                      ilbc_decode_state_t *iLBCdec_inst);   /* (i/o) the decoder state structure */

#endif

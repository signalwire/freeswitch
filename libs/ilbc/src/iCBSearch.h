/*
 * iLBC - a library for the iLBC codec
 *
 * iCBSearch.h - The iLBC low bit rate speech codec.
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
 * $Id: iCBSearch.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_ICBSEARCH_H
#define __iLBC_ICBSEARCH_H

void iCBSearch(ilbc_encode_state_t *iLBCenc_inst,   /* (i) the encoder state structure */
               int *index,                          /* (o) Codebook indices */
               int *gain_index,                     /* (o) Gain quantization indices */
               float *intarget,                     /* (i) Target vector for encoding */
               float *mem,                          /* (i) Buffer for codebook construction */
               int lMem,                            /* (i) Length of buffer */
               int lTarget,                         /* (i) Length of vector */
               int nStages,                         /* (i) Number of codebook stages */
               float *weightDenum,                  /* (i) weighting filter coefficients */
               float *weightState,                  /* (i) weighting filter state */
               int block);                          /* (i) the sub-block number */

#endif

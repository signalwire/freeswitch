/*
 * iLBC - a library for the iLBC codec
 *
 * iCBConstruct.h - The iLBC low bit rate speech codec.
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
 * $Id: iCBConstruct.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_ICBCONSTRUCT_H
#define __iLBC_ICBCONSTRUCT_H

void index_conv_enc(int *index);        /* (i/o) Codebook indexes */

void index_conv_dec(int *index);        /* (i/o) Codebook indexes */

void iCBConstruct(float *decvector,     /* (o) Decoded vector */
                  int *index,           /* (i) Codebook indices */
                  int *gain_index,      /* (i) Gain quantization indices */
                  float *mem,           /* (i) Buffer for codevector construction */
                  int lMem,             /* (i) Length of buffer */
                  int veclen,           /* (i) Length of vector */
                  int nStages);         /* (i) Number of codebook stages */

#endif

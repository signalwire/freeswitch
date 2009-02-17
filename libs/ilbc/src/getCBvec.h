/*
 * iLBC - a library for the iLBC codec
 *
 * getCBvec.h - The iLBC low bit rate speech codec.
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
 * $Id: getCBvec.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_GETCBVEC_H
#define __iLBC_GETCBVEC_H

void getCBvec(float *cbvec,   /* (o) Constructed codebook vector */
              float *mem,     /* (i) Codebook buffer */
              int index,      /* (i) Codebook index */
              int lMem,       /* (i) Length of codebook buffer */
              int cbveclen);  /* (i) Codebook vector length */

#endif

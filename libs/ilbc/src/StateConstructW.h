/*
 * iLBC - a library for the iLBC codec
 *
 * StateConstructW.c - The iLBC low bit rate speech codec.
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
 * $Id: StateConstructW.h,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

#ifndef __iLBC_STATECONSTRUCTW_H
#define __iLBC_STATECONSTRUCTW_H

void StateConstructW(int idxForMax,     /* (i) 6-bit index for the quantization of
                                               max amplitude */
                     int *idxVec,       /* (i) vector of quantization indexes */
                     float *syntDenum,  /* (i) synthesis filter denumerator */
                     float *out,        /* (o) the decoded state vector */
                     int len);          /* (i) length of a state vector */

#endif

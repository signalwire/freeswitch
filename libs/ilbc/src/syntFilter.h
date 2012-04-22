/*
 * iLBC - a library for the iLBC codec
 *
 * syntFilter.h - The iLBC low bit rate speech codec.
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
 * $Id: syntFilter.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_SYNTFILTER_H
#define __iLBC_SYNTFILTER_H

void syntFilter(float *Out,     /* (i/o) Signal to be filtered */
                float *a,       /* (i) LP parameters */
                int len,        /* (i) Length of signal */
                float *mem);    /* (i/o) Filter state */

#endif

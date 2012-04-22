/*
 * iLBC - a library for the iLBC codec
 *
 * hpInput.h - The iLBC low bit rate speech codec.
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
 * $Id: hpInput.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_HPINPUT_H
#define __iLBC_HPINPUT_H

void hpInput(const float *In,   /* (i) vector to filter */
             int len,           /* (i) length of vector to filter */
             float *Out,        /* (o) the resulting filtered vector */
             float *mem);       /* (i/o) the filter state */

#endif

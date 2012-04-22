/*
 * iLBC - a library for the iLBC codec
 *
 * gainquant.h - The iLBC low bit rate speech codec.
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
 * $Id: gainquant.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_GAINQUANT_H
#define __iLBC_GAINQUANT_H

float gainquant(                /* (o) quantized gain value */
                float in,       /* (i) gain value */
                float maxIn,    /* (i) maximum of gain value */
                int cblen,      /* (i) number of quantization indices */
                int *index);    /* (o) quantization index */

float gaindequant(              /* (o) quantized gain value */
                  int index,    /* (i) quantization index */
                  float maxIn,  /* (i) maximum of unquantized gain */
                  int cblen);   /* (i) number of quantization indices */

#endif

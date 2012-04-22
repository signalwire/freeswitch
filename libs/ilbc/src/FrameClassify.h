/*
 * iLBC - a library for the iLBC codec
 *
 * FrameClassify.h - The iLBC low bit rate speech codec.
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
 * $Id: FrameClassify.h,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

#ifndef __iLBC_FRAMECLASSIFY_H
#define __iLBC_FRAMECLASSIFY_H

int FrameClassify(                                      /* index to the max-energy sub-frame */
                  ilbc_encode_state_t *iLBCenc_inst,    /* (i/o) the encoder state structure */
                  float *residual);                     /* (i) lpc residual signal */

#endif

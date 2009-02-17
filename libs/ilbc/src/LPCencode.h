/*
 * iLBC - a library for the iLBC codec
 *
 * LPCencode.h - The iLBC low bit rate speech codec.
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
 * $Id: LPCencode.h,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

#ifndef __iLBC_LPCENCODE_H
#define __iLBC_LPCENCODE_H

void LPCencode(float *syntdenum,                    /* (i/o) synthesis filter coefficients
                                                             before/after encoding */
               float *weightdenum,                  /* (i/o) weighting denumerator coefficients
                                                             before/after encoding */
               int *lsf_index,                      /* (o) lsf quantization index */
               float *data,                         /* (i) lsf coefficients to quantize */
               ilbc_encode_state_t *iLBCenc_inst);  /* (i/o) the encoder state structure */

#endif

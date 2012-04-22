/*
 * iLBC - a library for the iLBC codec
 *
 * doCPLC.h - The iLBC low bit rate speech codec.
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
 * $Id: doCPLC.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_DOCLPC_H
#define __iLBC_DOCLPC_H

void doThePLC(float *PLCresidual,                   /* (o) concealed residual */
              float *PLClpc,                        /* (o) concealed LP parameters */
              int PLI,                              /* (i) packet loss indicator, 0 - no PL, 1 = PL */
              float *decresidual,                   /* (i) decoded residual */
              float *lpc,                           /* (i) decoded LPC (only used for no PL) */
              int inlag,                            /* (i) pitch lag */
              ilbc_decode_state_t *iLBCdec_inst);   /* (i/o) decoder instance */

#endif

/*
 * iLBC - a library for the iLBC codec
 *
 * iLBC_define.h - The head guy amongst the headers
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
 * $Id: iLBC_define.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#if !defined(_ILBC_DEFINE_H_)
#define _ILBC_DEFINE_H_

#include <string.h>

#define FS                      8000.0f
#define NSUB_20MS               4
#define NSUB_30MS               6
#define NASUB_20MS              2


#define NASUB_30MS              4
#define NASUB_MAX               4
#define SUBL                    40
#define STATE_LEN               80
#define STATE_SHORT_LEN_30MS    58
#define STATE_SHORT_LEN_20MS    57

/* LPC settings */

#define ILBC_LPC_FILTERORDER    10
#define LPC_CHIRP_SYNTDENUM     0.9025f
#define LPC_CHIRP_WEIGHTDENUM   0.4222f
#define LPC_LOOKBACK            60
#define LPC_N_20MS              1
#define LPC_N_30MS              2
#define LPC_N_MAX               2
#define LPC_ASYMDIFF            20
#define LPC_BW                  60.0f
#define LPC_WN                  1.0001f
#define LSF_NSPLIT              3
#define LSF_NUMBER_OF_STEPS     4
#define LPC_HALFORDER           (ILBC_LPC_FILTERORDER/2)

/* cb settings */

#define CB_NSTAGES              3
#define CB_EXPAND               2
#define CB_MEML                 147
#define CB_FILTERLEN            2*4
#define CB_HALFFILTERLEN        4
#define CB_RESRANGE             34
#define CB_MAXGAIN              1.3f

/* enhancer */

#define ENH_BLOCKL              80  /* block length */
#define ENH_BLOCKL_HALF         (ENH_BLOCKL/2)
#define ENH_HL                  3   /* 2*ENH_HL+1 is number blocks in said second sequence */
#define ENH_SLOP                2   /* max difference estimated and correct pitch period */
#define ENH_PLOCSL              20  /* pitch-estimates and pitch-locations buffer length */
#define ENH_OVERHANG            2
#define ENH_UPS0                4   /* upsampling rate */
#define ENH_FL0                 3   /* 2*FLO+1 is the length of each filter */
#define ENH_VECTL               (ENH_BLOCKL + 2*ENH_FL0)

#define ENH_CORRDIM             (2*ENH_SLOP + 1)
#define ENH_NBLOCKS             (ILBC_BLOCK_LEN_MAX/ENH_BLOCKL)
#define ENH_NBLOCKS_EXTRA       5
#define ENH_NBLOCKS_TOT         8   /* ENH_NBLOCKS + ENH_NBLOCKS_EXTRA */
#define ENH_BUFL                (ENH_NBLOCKS_TOT*ENH_BLOCKL)
#define ENH_ALPHA0              0.05f

/* Down sampling */

#define FILTERORDER_DS          7
#define DELAY_DS                3
#define FACTOR_DS               2

/* bit stream defs */

#define STATE_BITS              3
#define BYTE_LEN                8

/* help parameters */

#define FLOAT_MAX               1.0e37f
#define EPS                     2.220446049250313e-016f
#define PI                      3.14159265358979323846f
#define MIN_SAMPLE              -32768
#define MAX_SAMPLE              32767
#define TWO_PI                  6.283185307f
#define PI2                     0.159154943f

#endif
/*- End of file ------------------------------------------------------------*/

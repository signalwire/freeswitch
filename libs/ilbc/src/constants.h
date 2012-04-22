/*
 * iLBC - a library for the iLBC codec
 *
 * constants.h - The iLBC low bit rate speech codec.
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
 * $Id: constants.h,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

#ifndef __iLBC_CONSTANTS_H
#define __iLBC_CONSTANTS_H

#include "iLBC_define.h"
#include "ilbc.h"

/* ULP bit allocation */

extern const ilbc_ulp_inst_t ULP_20msTbl;
extern const ilbc_ulp_inst_t ULP_30msTbl;

/* high pass filters */

extern const float hpi_zero_coefsTbl[];
extern const float hpi_pole_coefsTbl[];
extern const float hpo_zero_coefsTbl[];
extern const float hpo_pole_coefsTbl[];

/* low pass filters */
extern const float lpFilt_coefsTbl[];

/* LPC analysis and quantization */

extern const float lpc_winTbl[];
extern const float lpc_asymwinTbl[];
extern const float lpc_lagwinTbl[];
extern const float lsfCbTbl[];
extern const float lsfmeanTbl[];
extern const int   dim_lsfCbTbl[];
extern const int   size_lsfCbTbl[];
extern const float lsf_weightTbl_30ms[];
extern const float lsf_weightTbl_20ms[];

/* state quantization tables */

extern const float state_sq3Tbl[];
extern const float state_frgqTbl[];

/* gain quantization tables */

extern const float gain_sq3Tbl[];
extern const float gain_sq4Tbl[];
extern const float gain_sq5Tbl[];

/* adaptive codebook definitions */

extern const int search_rangeTbl[5][CB_NSTAGES];
extern const int memLfTbl[];
extern const int stMemLTbl;
extern const float cbfiltersTbl[CB_FILTERLEN];

/* enhancer definitions */

extern const float polyphaserTbl[];
extern const float enh_plocsTbl[];

#endif

/***********************************************************************
Copyright (c) 2006-2010, Skype Limited. All rights reserved. 
Redistribution and use in source and binary forms, with or without 
modification, (subject to the limitations in the disclaimer below) 
are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright 
notice, this list of conditions and the following disclaimer in the 
documentation and/or other materials provided with the distribution.
- Neither the name of Skype Limited, nor the names of specific 
contributors, may be used to endorse or promote products derived from 
this software without specific prior written permission.
NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED 
BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
CONTRIBUTORS ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND 
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF 
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

/*																		*
 * File Name:	SKP_Silk_resample_rom.h								*
 *																		*
 * Description: Header file for FIR resampling of						*
 *				32 and 44 kHz input 									*
 *                                                                      *
 * Copyright 2007 (c), Skype Limited                                    *
 * All rights reserved.													*
 *																		*
 * Date: 070807                                                         *
 *                                                                      */

#ifndef _SKP_SILK_FIX_RESAMPLE_ROM_H_
#define _SKP_SILK_FIX_RESAMPLE_ROM_H_

#include "SKP_Silk_typedef.h"

#ifdef  __cplusplus
extern "C"
{
#endif

#define SigProc_Resample_bw_1_4_NUM_INTERPOLATORS_LOG2				7
#define SigProc_Resample_bw_1_4_NUM_INTERPOLATORS					(1 << SigProc_Resample_bw_1_4_NUM_INTERPOLATORS_LOG2)
#define SigProc_Resample_bw_1_4_NUM_FIR_COEFS						6 

extern const SKP_int16 SigProc_Resample_bw_1_4_INTERPOL[SigProc_Resample_bw_1_4_NUM_INTERPOLATORS][SigProc_Resample_bw_1_4_NUM_FIR_COEFS];


#define SigProc_Resample_bw_80_441_NUM_INTERPOLATORS_LOG2			6
#define SigProc_Resample_bw_80_441_NUM_INTERPOLATORS				(1 << SigProc_Resample_bw_80_441_NUM_INTERPOLATORS_LOG2)
#define SigProc_Resample_bw_80_441_NUM_FIR_COEFS					4 

extern const SKP_int16 SigProc_Resample_bw_80_441_INTERPOL[SigProc_Resample_bw_80_441_NUM_INTERPOLATORS][SigProc_Resample_bw_80_441_NUM_FIR_COEFS];

#define SigProc_Resample_2_3_coarse_NUM_INTERPOLATORS				 2
#define SigProc_Resample_2_3_coarse_NUM_FIR_COEFS					32

extern const SKP_int16 SigProc_Resample_2_3_coarse_INTERPOL[SigProc_Resample_2_3_coarse_NUM_INTERPOLATORS][SigProc_Resample_2_3_coarse_NUM_FIR_COEFS];

#define SigProc_Resample_2_3_coarsest_NUM_INTERPOLATORS				 2
#define SigProc_Resample_2_3_coarsest_NUM_FIR_COEFS					10

extern const SKP_int16 SigProc_Resample_2_3_coarsest_INTERPOL[SigProc_Resample_2_3_coarsest_NUM_INTERPOLATORS][SigProc_Resample_2_3_coarsest_NUM_FIR_COEFS];

#define SigProc_Resample_3_2_coarse_NUM_INTERPOLATORS				 3
#define SigProc_Resample_3_2_coarse_NUM_FIR_COEFS					 8

extern const SKP_int16 SigProc_Resample_3_2_coarse_INTERPOL[SigProc_Resample_3_2_coarse_NUM_INTERPOLATORS][SigProc_Resample_3_2_coarse_NUM_FIR_COEFS];

#define SigProc_Resample_147_40_NUM_INTERPOLATORS					 147
#define SigProc_Resample_147_40_NUM_FIR_COEFS						 20

extern const SKP_int16 SigProc_Resample_147_40_INTERPOL[SigProc_Resample_147_40_NUM_INTERPOLATORS][SigProc_Resample_147_40_NUM_FIR_COEFS];

#define SigProc_Resample_147_40_alt_NUM_INTERPOLATORS				 147
#define SigProc_Resample_147_40_alt_NUM_FIR_COEFS					 10

extern const SKP_int16 SigProc_Resample_147_40_alt_INTERPOL[SigProc_Resample_147_40_alt_NUM_INTERPOLATORS][SigProc_Resample_147_40_alt_NUM_FIR_COEFS];

#define SigProc_Resample_147_40_coarse_NUM_INTERPOLATORS			 147
#define SigProc_Resample_147_40_coarse_NUM_FIR_COEFS				 16

extern const SKP_int16 SigProc_Resample_147_40_coarse_INTERPOL[SigProc_Resample_147_40_coarse_NUM_INTERPOLATORS][SigProc_Resample_147_40_coarse_NUM_FIR_COEFS];

#define SigProc_Resample_40_147_NUM_INTERPOLATORS					 40
#define SigProc_Resample_40_147_NUM_FIR_COEFS						 60

extern const SKP_int16 SigProc_Resample_40_147_INTERPOL[SigProc_Resample_40_147_NUM_INTERPOLATORS][SigProc_Resample_40_147_NUM_FIR_COEFS];

#define SigProc_Resample_40_147_coarse_NUM_INTERPOLATORS			 40
#define SigProc_Resample_40_147_coarse_NUM_FIR_COEFS				 30

extern const SKP_int16 SigProc_Resample_40_147_coarse_INTERPOL[SigProc_Resample_40_147_coarse_NUM_INTERPOLATORS][SigProc_Resample_40_147_coarse_NUM_FIR_COEFS];

#ifdef  __cplusplus
}
#endif

#endif // _SKP_SILK_FIX_RESAMPLE_ROM_H_

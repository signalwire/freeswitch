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

/*                                                                      *
 * File Name:    SKP_Silk_resample_2_3_rom.c                          *
 *                                                                      *
 * Description: Filter coefficients for FIR polyphase resampling        *
 *                                                                      *
 * Copyright 2009 (c), Skype Limited                                    *
 * All rights reserved.                                                 *
 *                                                                      *
 * Date: 090424                                                         *
 *                                                                      */

#include "SKP_Silk_resample_rom.h"

const SKP_int16 SigProc_Resample_2_3_coarse_INTERPOL[ SigProc_Resample_2_3_coarse_NUM_INTERPOLATORS ][ SigProc_Resample_2_3_coarse_NUM_FIR_COEFS ] = {
    {    0,   -74,   109,     0,  -234,   329,     0,  -610,   813,     0, -1437,  1954,     0, -4358,  8953, 21845,  8953, -4358,     0,  1954, -1437,     0,   813,  -610,     0,   329,  -234,     0,   109,   -74,     0,    45 },
    {   48,   -62,     0,   133,  -195,     0,   386,  -526,     0,   936, -1243,     0,  2311, -3417,     0, 18026, 18026,     0, -3417,  2311,     0, -1243,   936,     0,  -526,   386,     0,  -195,   133,     0,   -62,    48 },
};

const SKP_int16 SigProc_Resample_2_3_coarsest_INTERPOL[ SigProc_Resample_2_3_coarsest_NUM_INTERPOLATORS ][ SigProc_Resample_2_3_coarsest_NUM_FIR_COEFS ] = {
    {  379,     0, -3081,  8239, 21845,  8239, -3081,     0,   379,  -145 },
    {    0,   696, -1951,     0, 17659, 17659,     0, -1951,   696,     0 },
};


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
 * SKP_Silk_resample_1_2_coarsest.c                                   *
 *                                                                      *
 * Downsample by a factor 2, coarsest                                   *
 *                                                                      *
 * Copyright 2006 (c), Skype Limited                                    *
 * Date: 060221                                                         *
 *                                                                      */
#include "SKP_Silk_SigProc_FIX.h"


/* Coefficients for coarsest 2-fold resampling */
static SKP_int16 A20cst[ 1 ] = {  3786 };
static SKP_int16 A21cst[ 1 ] = { 17908 };

/* Downsample by a factor 2, coarsest */
void SKP_Silk_resample_1_2_coarsest(
    const SKP_int16     *in,                /* I:   16 kHz signal [2*len]   */
    SKP_int32           *S,                 /* I/O: State vector [2]        */
    SKP_int16           *out,               /* O:   8 kHz signal [len]      */
    SKP_int32           *scratch,           /* I:   Scratch memory [3*len]  */
    const SKP_int32     len                 /* I:   Number of OUTPUT samples*/
)
{
    SKP_int32 k, idx;

    /* De-interleave allpass inputs, and convert Q15 -> Q25 */
    for( k = 0; k < len; k++ ) {
        idx = SKP_LSHIFT( k, 1 );
        scratch[ k ]       = SKP_LSHIFT( (SKP_int32)in[ idx     ], 10 );
        scratch[ k + len ] = SKP_LSHIFT( (SKP_int32)in[ idx + 1 ], 10 );
    }

    idx = SKP_LSHIFT( len, 1 );
    /* Allpass filters */
    SKP_Silk_allpass_int( scratch,       S,     A21cst[ 0 ], scratch + idx, len );
    SKP_Silk_allpass_int( scratch + len, S + 1, A20cst[ 0 ], scratch,       len );

    /* Add two allpass outputs */
    for( k = 0; k < len; k++ ) {
        out[ k ] = (SKP_int16)SKP_SAT16( SKP_RSHIFT_ROUND( scratch[ k ] + scratch[ k + idx ], 11 ) );
    }
}


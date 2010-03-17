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
 * SKP_Silk_MA.c                                                      *
 *                                                                      *
 * Variable order MA filter                                             *
 *                                                                      *
 * Copyright 2006 (c), Skype Limited                                    *
 * Date: 060221                                                         *
 *                                                                      */
#include "SKP_Silk_SigProc_FIX.h"

/* Variable order MA filter */
void SKP_Silk_MA(
    const SKP_int16      *in,            /* I:   input signal                                */
    const SKP_int16      *B,             /* I:   MA coefficients, Q13 [order+1]              */
    SKP_int32            *S,             /* I/O: state vector [order]                        */
    SKP_int16            *out,           /* O:   output signal                               */
    const SKP_int32      len,            /* I:   signal length                               */
    const SKP_int32      order           /* I:   filter order                                */
)
{
    SKP_int   k, d, in16;
    SKP_int32 out32;
    
    for( k = 0; k < len; k++ ) {
        in16 = in[ k ];
        out32 = SKP_SMLABB( S[ 0 ], in16, B[ 0 ] );
        out32 = SKP_RSHIFT_ROUND( out32, 13 );
        
        for( d = 1; d < order; d++ ) {
            S[ d - 1 ] = SKP_SMLABB( S[ d ], in16, B[ d ] );
        }
        S[ order - 1 ] = SKP_SMULBB( in16, B[ order ] );

        /* Limit */
        out[ k ] = (SKP_int16)SKP_SAT16( out32 );
    }
}
/* Variable order MA prediction error filter */
void SKP_Silk_MA_Prediction(
    const SKP_int16      *in,            /* I:   Input signal                                */
    const SKP_int16      *B,             /* I:   MA prediction coefficients, Q12 [order]     */
    SKP_int32            *S,             /* I/O: State vector [order]                        */
    SKP_int16            *out,           /* O:   Output signal                               */
    const SKP_int32      len,            /* I:   Signal length                               */
    const SKP_int32      order           /* I:   Filter order                                */
)
{
    SKP_int   k, d, in16;
    SKP_int32 out32;
    SKP_int32 B32;

    if( ( order & 1 ) == 0 && (SKP_int32)( (SKP_int_ptr_size)B & 3 ) == 0 ) {
        /* Even order and 4-byte aligned coefficient array */

        /* NOTE: the code below loads two int16 values in an int32, and multiplies each using the    */
        /* SMLABB and SMLABT instructions. On a big-endian CPU the two int16 variables would be      */
        /* loaded in reverse order and the code will give the wrong result. In that case swapping    */
        /* the SMLABB and SMLABT instructions should solve the problem.                              */
        for( k = 0; k < len; k++ ) {
            in16 = in[ k ];
            out32 = SKP_LSHIFT( in16, 12 ) - S[ 0 ];
            out32 = SKP_RSHIFT_ROUND( out32, 12 );
            
            for( d = 0; d < order - 2; d += 2 ) {
                B32 = *( (SKP_int32*)&B[ d ] );                /* read two coefficients at once */
                S[ d ]     = SKP_SMLABB_ovflw( S[ d + 1 ], in16, B32 );
                S[ d + 1 ] = SKP_SMLABT_ovflw( S[ d + 2 ], in16, B32 );
            }
            B32 = *( (SKP_int32*)&B[ d ] );                    /* read two coefficients at once */
            S[ order - 2 ] = SKP_SMLABB_ovflw( S[ order - 1 ], in16, B32 );
            S[ order - 1 ] = SKP_SMULBT( in16, B32 );

            /* Limit */
            out[ k ] = (SKP_int16)SKP_SAT16( out32 );
        }
    } else {
        /* Odd order or not 4-byte aligned coefficient array */
        for( k = 0; k < len; k++ ) {
            in16 = in[ k ];
            out32 = SKP_LSHIFT( in16, 12 ) - S[ 0 ];
            out32 = SKP_RSHIFT_ROUND( out32, 12 );
            
            for( d = 0; d < order - 1; d++ ) {
                S[ d ] = SKP_SMLABB_ovflw( S[ d + 1 ], in16, B[ d ] );
            }
            S[ order - 1 ] = SKP_SMULBB( in16, B[ order - 1 ] );

            /* Limit */
            out[ k ] = (SKP_int16)SKP_SAT16( out32 );
        }
    }
}

void SKP_Silk_MA_Prediction_Q13(
    const SKP_int16      *in,            /* I:   input signal                                */
    const SKP_int16      *B,             /* I:   MA prediction coefficients, Q13 [order]     */
    SKP_int32            *S,             /* I/O: state vector [order]                        */
    SKP_int16            *out,           /* O:   output signal                               */
    SKP_int32            len,            /* I:   signal length                               */
    SKP_int32            order           /* I:   filter order                                */
)
{
    SKP_int   k, d, in16;
    SKP_int32 out32, B32;
    
    if( ( order & 1 ) == 0 && (SKP_int32)( (SKP_int_ptr_size)B & 3 ) == 0 ) {
        /* Even order and 4-byte aligned coefficient array */
        
        /* NOTE: the code below loads two int16 values in an int32, and multiplies each using the    */
        /* SMLABB and SMLABT instructions. On a big-endian CPU the two int16 variables would be      */
        /* loaded in reverse order and the code will give the wrong result. In that case swapping    */
        /* the SMLABB and SMLABT instructions should solve the problem.                              */
        for( k = 0; k < len; k++ ) {
            in16 = in[ k ];
            out32 = SKP_LSHIFT( in16, 13 ) - S[ 0 ];
            out32 = SKP_RSHIFT_ROUND( out32, 13 );
            
            for( d = 0; d < order - 2; d += 2 ) {
                B32 = *( (SKP_int32*)&B[ d ] );                /* read two coefficients at once */
                S[ d ]     = SKP_SMLABB( S[ d + 1 ], in16, B32 );
                S[ d + 1 ] = SKP_SMLABT( S[ d + 2 ], in16, B32 );
            }
            B32 = *( (SKP_int32*)&B[ d ] );                    /* read two coefficients at once */
            S[ order - 2 ] = SKP_SMLABB( S[ order - 1 ], in16, B32 );
            S[ order - 1 ] = SKP_SMULBT( in16, B32 );

            /* Limit */
            out[ k ] = (SKP_int16)SKP_SAT16( out32 );
        }
    } else {
        /* Odd order or not 4-byte aligned coefficient array */
        for( k = 0; k < len; k++ ) {
            in16 = in[ k ];
            out32 = SKP_LSHIFT( in16, 13 ) - S[ 0 ];
            out32 = SKP_RSHIFT_ROUND( out32, 13 );
            
            for( d = 0; d < order - 1; d++ ) {
                S[ d ] = SKP_SMLABB( S[ d + 1 ], in16, B[ d ] );
            }
            S[ order - 1 ] = SKP_SMULBB( in16, B[ order - 1 ] );

            /* Limit */
            out[ k ] = (SKP_int16)SKP_SAT16( out32 );
        }
    }
}
/* Variable order MA prediction error filter. */
/* Inverse filter of SKP_Silk_LPC_synthesis_filter */
void SKP_Silk_LPC_analysis_filter(
    const SKP_int16      *in,            /* I:   Input signal                                */
    const SKP_int16      *B,             /* I:   MA prediction coefficients, Q12 [order]     */
    SKP_int16            *S,             /* I/O: State vector [order]                        */
    SKP_int16            *out,           /* O:   Output signal                               */
    const SKP_int32      len,            /* I:   Signal length                               */
    const SKP_int32      Order           /* I:   Filter order                                */
)
{
    SKP_int   k, j, idx, Order_half = SKP_RSHIFT( Order, 1 );
    SKP_int32 Btmp, B_align_Q12[ SigProc_MAX_ORDER_LPC >> 1 ], out32_Q12, out32;
    SKP_int16 SA, SB;
    /* Order must be even */
    SKP_assert( 2 * Order_half == Order );

    /* Combine two A_Q12 values and ensure 32-bit alignment */
    for( k = 0; k < Order_half; k++ ) {
        idx = SKP_SMULBB( 2, k );
        B_align_Q12[ k ] = ( ( (SKP_int32)B[ idx ] ) & 0x0000ffff ) | SKP_LSHIFT( (SKP_int32)B[ idx + 1 ], 16 );
    }

    /* S[] values are in Q0 */
    for( k = 0; k < len; k++ ) {
        SA = S[ 0 ];
        out32_Q12 = 0;
        for( j = 0; j < ( Order_half - 1 ); j++ ) {
            idx = SKP_SMULBB( 2, j ) + 1;
            /* Multiply-add two prediction coefficients for each loop */
            Btmp = B_align_Q12[ j ];
            SB = S[ idx ];
            S[ idx ] = SA;
            out32_Q12 = SKP_SMLABB( out32_Q12, SA, Btmp );
            out32_Q12 = SKP_SMLABT( out32_Q12, SB, Btmp );
            SA = S[ idx + 1 ];
            S[ idx + 1 ] = SB;
        }

        /* Unrolled loop: epilog */
        Btmp = B_align_Q12[ Order_half - 1 ];
        SB = S[ Order - 1 ];
        S[ Order - 1 ] = SA;
        out32_Q12 = SKP_SMLABB( out32_Q12, SA, Btmp );
        out32_Q12 = SKP_SMLABT( out32_Q12, SB, Btmp );

        /* Subtract prediction */
        out32_Q12 = SKP_SUB_SAT32( SKP_LSHIFT( (SKP_int32)in[ k ], 12 ), out32_Q12 );

        /* Scale to Q0 */
        out32 = SKP_RSHIFT_ROUND( out32_Q12, 12 );

        /* Saturate output */
        out[ k ] = (SKP_int16)SKP_SAT16( out32 );

        /* Move input line */
        S[ 0 ] = in[ k ];
    }
}

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

/*
 * File Name:   SKP_Silk_pulses_to_bytes.c
 */

#include <stdlib.h>
#include "SKP_Silk_main.h"

/* nBytes = sum_over_shell_blocks( POLY_FIT_0 + POLY_FIT_1 * sum_abs_val + POLY_FIT_2 * sum_abs_val^2 ) */
#define POLY_FIT_0_Q15     12520
#define POLY_FIT_1_Q15     15862
#define POLY_FIT_2_Q20     -9222 // ToDo better training with 

/* Predict number of bytes used to encode q */
SKP_int SKP_Silk_pulses_to_bytes( /* O  Return value, predicted number of bytes used to encode q */ 
    SKP_Silk_encoder_state          *psEncC,        /* I/O  Encoder State */
    SKP_int                         q[]             /* I    Pulse signal  */
)
{
    SKP_int i, j, iter, *q_ptr;
    SKP_int32 sum_abs_val, nBytes, acc_nBytes;
    /* Take the absolute value of the pulses */
    iter = psEncC->frame_length / SHELL_CODEC_FRAME_LENGTH;
    
    /* Calculate rate as a nonlinaer mapping of sum abs value of each Shell block */
    q_ptr      = q;
    acc_nBytes = 0;
    for( j = 0; j < iter; j++ ) {
        sum_abs_val = 0;
        for(i = 0; i < SHELL_CODEC_FRAME_LENGTH; i+=4){
            sum_abs_val += SKP_abs( q_ptr[ i + 0 ] );
            sum_abs_val += SKP_abs( q_ptr[ i + 1 ] );
            sum_abs_val += SKP_abs( q_ptr[ i + 2 ] );
            sum_abs_val += SKP_abs( q_ptr[ i + 3 ] );
        }
        /* Calculate nBytes used for thi sshell frame */
        nBytes = SKP_SMULWB( SKP_SMULBB( sum_abs_val, sum_abs_val ), POLY_FIT_2_Q20 );  // Q4
        nBytes = SKP_LSHIFT_SAT32( nBytes, 11 );                                        // Q15
        nBytes += SKP_SMULBB( sum_abs_val, POLY_FIT_1_Q15 );                            // Q15
        nBytes += POLY_FIT_0_Q15;                                                       // Q15

        acc_nBytes += nBytes;

        q_ptr += SHELL_CODEC_FRAME_LENGTH; /* update pointer */
    }

    acc_nBytes = SKP_RSHIFT_ROUND( acc_nBytes, 15 );                                    // Q0
    acc_nBytes = SKP_SAT16( acc_nBytes ); // just to be sure                            // Q0
    
    return(( SKP_int )acc_nBytes);
}

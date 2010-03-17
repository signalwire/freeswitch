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


#include "SKP_Silk_define.h"
#include "SKP_Silk_main_FIX.h"
#include "SKP_Silk_SDK_API.h"
#include "SKP_Silk_control.h"
#include "SKP_Silk_typedef.h"
#include "SKP_Silk_structs.h"
#define SKP_Silk_EncodeControlStruct SKP_SILK_SDK_EncControlStruct

/****************************************/
/* Encoder functions                    */
/****************************************/

SKP_int SKP_Silk_SDK_Get_Encoder_Size( SKP_int *encSizeBytes )
{
    SKP_int ret = 0;
    
    *encSizeBytes = sizeof( SKP_Silk_encoder_state_FIX );
    
    return ret;
}


/***************************************/
/* Read control structure from encoder */
/***************************************/
SKP_int SKP_Silk_SDK_QueryEncoder(
    const void *encState,                       /* I:   State Vector                                    */
    SKP_Silk_EncodeControlStruct *encStatus     /* O:   Control Structure                               */
)
{
    SKP_Silk_encoder_state_FIX *psEnc;
    SKP_int ret = 0;    

    psEnc = ( SKP_Silk_encoder_state_FIX* )encState;

    encStatus->sampleRate = ( unsigned short )SKP_SMULBB( psEnc->sCmn.fs_kHz, 1000 );                       /* convert kHz -> Hz */ 
    encStatus->packetSize = ( unsigned short )SKP_SMULBB( psEnc->sCmn.fs_kHz, psEnc->sCmn.PacketSize_ms );  /* convert samples -> ms */
    encStatus->bitRate    = ( unsigned short )psEnc->sCmn.TargetRate_bps;
    encStatus->packetLossPercentage = psEnc->sCmn.PacketLoss_perc;
    encStatus->complexity           = psEnc->sCmn.Complexity;

    return ret;
}

/*************************/
/* Init or Reset encoder */
/*************************/
SKP_int SKP_Silk_SDK_InitEncoder(
    void                            *encState,          /* I/O: State                                           */
    SKP_Silk_EncodeControlStruct    *encStatus          /* O:   Control structure                               */
)
{
    SKP_Silk_encoder_state_FIX *psEnc;
    SKP_int ret = 0;

        
    psEnc = ( SKP_Silk_encoder_state_FIX* )encState;

    /* Reset Encoder */
    if( ret += SKP_Silk_init_encoder_FIX( psEnc ) ) {
        SKP_assert( 0 );
    }

    /* Read Control structure */
    if( ret += SKP_Silk_SDK_QueryEncoder( encState, encStatus ) ) {
        SKP_assert( 0 );
    }


    return ret;
}

/**************************/
/* Encode frame with Silk */
/**************************/
SKP_int SKP_Silk_SDK_Encode( 
    void                                *encState,      /* I/O: State                                           */
    const SKP_Silk_EncodeControlStruct  *encControl,    /* I:   Control structure                               */
    const SKP_int16                     *samplesIn,     /* I:   Speech sample input vector                      */
    SKP_int                             nSamplesIn,     /* I:   Number of samples in input vector               */
    SKP_uint8                           *outData,       /* O:   Encoded output vector                           */
    SKP_int16                           *nBytesOut      /* I/O: Number of bytes in outData (input: Max bytes)   */
)
{
    SKP_int   API_fs_kHz, PacketSize_ms, PacketLoss_perc, UseInBandFec, UseDTX, ret = 0;
    SKP_int   nSamplesToBuffer, Complexity, input_ms, nSamplesFromInput = 0;
    SKP_int32 TargetRate_bps;
    SKP_int16 MaxBytesOut;
    SKP_Silk_encoder_state_FIX *psEnc = ( SKP_Silk_encoder_state_FIX* )encState;


    SKP_assert( encControl != NULL );

    /* Check sampling frequency first, to avoid divide by zero later */
    if( ( encControl->sampleRate !=  8000 ) && ( encControl->sampleRate != 12000 ) && 
        ( encControl->sampleRate != 16000 ) && ( encControl->sampleRate != 24000 ) ) {
        ret = SKP_SILK_ENC_FS_NOT_SUPPORTED;
        SKP_assert( 0 );
        return( ret );
    }

    /* Set Encoder parameters from Control structure */
    API_fs_kHz      = SKP_DIV32_16( ( SKP_int )encControl->sampleRate, 1000 );          /* convert Hz -> kHz */
    PacketSize_ms   = SKP_DIV32_16( ( SKP_int )encControl->packetSize, API_fs_kHz );    /* convert samples -> ms */
    TargetRate_bps  =             ( SKP_int32 )encControl->bitRate;
    PacketLoss_perc =               ( SKP_int )encControl->packetLossPercentage;
    UseInBandFec    =               ( SKP_int )encControl->useInBandFEC;
    Complexity      =               ( SKP_int )encControl->complexity;
    UseDTX          =               ( SKP_int )encControl->useDTX;

    /* Only accept input lengths that are multiplum of 10 ms */
    input_ms = SKP_DIV32_16( nSamplesIn, API_fs_kHz );
    if( ( input_ms % 10) != 0 || nSamplesIn < 0 ) {
        ret = SKP_SILK_ENC_INPUT_INVALID_NO_OF_SAMPLES;
        SKP_assert( 0 );
        return( ret );
    }

    /* Make sure no more than one packet can be produced */
    if( nSamplesIn > SKP_SMULBB( psEnc->sCmn.PacketSize_ms, API_fs_kHz ) ) {
        ret = SKP_SILK_ENC_INPUT_INVALID_NO_OF_SAMPLES;
        SKP_assert( 0 );
        return( ret );
    }

    if( ( ret = SKP_Silk_control_encoder_FIX( psEnc, API_fs_kHz, PacketSize_ms, TargetRate_bps, 
                    PacketLoss_perc, UseInBandFec, UseDTX, input_ms, Complexity ) ) != 0 ) {
        SKP_assert( 0 );
        return( ret );
    }

    /* Detect energy above 8 kHz */
    if( encControl->sampleRate == 24000 && psEnc->sCmn.sSWBdetect.SWB_detected == 0 && psEnc->sCmn.sSWBdetect.WB_detected == 0 ) {
        SKP_Silk_detect_SWB_input( &psEnc->sCmn.sSWBdetect, samplesIn, ( SKP_int )nSamplesIn );
    }

    /* Input buffering/resampling and encoding */
    MaxBytesOut = 0;                    /* return 0 output bytes if no encoder called */
    while( 1 ) {
        /* Resample/buffer */
        nSamplesToBuffer = psEnc->sCmn.frame_length - psEnc->sCmn.inputBufIx;
        if( encControl->sampleRate == SKP_SMULBB( psEnc->sCmn.fs_kHz, 1000 ) ) { 
            /* Same sample frequency - copy the data */
            nSamplesToBuffer  = SKP_min_int( nSamplesToBuffer, nSamplesIn );
            nSamplesFromInput = nSamplesToBuffer;
            SKP_memcpy( &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], samplesIn, SKP_SMULBB( nSamplesToBuffer, sizeof( SKP_int16 ) ) );
        } else if( encControl->sampleRate == 24000 && psEnc->sCmn.fs_kHz == 16 ) {
            /* Resample the data from 24 kHz to 16 kHz */
            nSamplesToBuffer  = SKP_min_int( nSamplesToBuffer, SKP_SMULWB( SKP_LSHIFT( nSamplesIn, 1 ), 21846 ) ); // 21846 = ceil(2/3)*2^15
            nSamplesFromInput = SKP_RSHIFT( SKP_SMULBB( nSamplesToBuffer, 3 ), 1 );
#if LOW_COMPLEXITY_ONLY
            {
                SKP_int16 scratch[ MAX_FRAME_LENGTH + SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ];
                SKP_assert( nSamplesFromInput <= MAX_FRAME_LENGTH );
                SKP_Silk_resample_2_3_coarse( &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], psEnc->sCmn.resample24To16state, 
                    samplesIn, nSamplesFromInput, scratch );
            }
#else
            SKP_Silk_resample_2_3( &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], psEnc->sCmn.resample24To16state, 
                samplesIn, nSamplesFromInput );
#endif
        } else if( encControl->sampleRate == 24000 && psEnc->sCmn.fs_kHz == 12 ) {
            SKP_int32 scratch[ 3 * MAX_FRAME_LENGTH ];
            /* Resample the data from 24 kHz to 12 kHz */
            nSamplesToBuffer  = SKP_min_int( nSamplesToBuffer, SKP_RSHIFT( nSamplesIn, 1 ) );
            nSamplesFromInput = SKP_LSHIFT16( nSamplesToBuffer, 1 );
            SKP_Silk_resample_1_2_coarse( samplesIn, psEnc->sCmn.resample24To12state, 
                &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], scratch, nSamplesToBuffer );
        } else if( encControl->sampleRate == 24000 && psEnc->sCmn.fs_kHz == 8 ) {
            /* Resample the data from 24 kHz to 8 kHz */
            nSamplesToBuffer  = SKP_min_int( nSamplesToBuffer, SKP_DIV32_16( nSamplesIn, 3 ) );
            nSamplesFromInput = SKP_SMULBB( nSamplesToBuffer, 3 );
            SKP_Silk_resample_1_3( &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], psEnc->sCmn.resample24To8state, 
                samplesIn, nSamplesFromInput);
        } else if( encControl->sampleRate == 16000 && psEnc->sCmn.fs_kHz == 12 ) {
            /* Resample the data from 16 kHz to 12 kHz */
            nSamplesToBuffer  = SKP_min_int( nSamplesToBuffer, SKP_RSHIFT( SKP_SMULBB( nSamplesIn, 3 ), 2 ) );
            nSamplesFromInput = SKP_SMULWB( SKP_LSHIFT16( nSamplesToBuffer, 2 ), 21846 ); // 21846 = ceil((1/3)*2^16)
            SKP_Silk_resample_3_4( &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], psEnc->sCmn.resample16To12state, 
                samplesIn, nSamplesFromInput );
        } else if( encControl->sampleRate == 16000 && psEnc->sCmn.fs_kHz == 8 ) {
            SKP_int32 scratch[ 3 * MAX_FRAME_LENGTH ];
            /* Resample the data from 16 kHz to 8 kHz */
            nSamplesToBuffer  = SKP_min_int( nSamplesToBuffer, SKP_RSHIFT( nSamplesIn, 1 ) );
            nSamplesFromInput = SKP_LSHIFT16( nSamplesToBuffer, 1 );
            SKP_Silk_resample_1_2_coarse( samplesIn, psEnc->sCmn.resample16To8state, 
                &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], scratch, nSamplesToBuffer );
        } else if( encControl->sampleRate == 12000 && psEnc->sCmn.fs_kHz == 8 ) {
            /* Resample the data from 12 kHz to 8 kHz */
            nSamplesToBuffer  = SKP_min_int( nSamplesToBuffer, SKP_SMULWB( SKP_LSHIFT( nSamplesIn, 1 ), 21846 ) );
            nSamplesFromInput = SKP_RSHIFT( SKP_SMULBB( nSamplesToBuffer, 3 ), 1 );
#if LOW_COMPLEXITY_ONLY
            {
                SKP_int16 scratch[ MAX_FRAME_LENGTH + SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ];
                SKP_assert( nSamplesFromInput <= MAX_FRAME_LENGTH );
                SKP_Silk_resample_2_3_coarse( &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], psEnc->sCmn.resample12To8state, 
                    samplesIn, nSamplesFromInput, scratch );
            }
#else
            SKP_Silk_resample_2_3( &psEnc->sCmn.inputBuf[ psEnc->sCmn.inputBufIx ], psEnc->sCmn.resample12To8state, 
                samplesIn, nSamplesFromInput );
#endif
        }
        samplesIn  += nSamplesFromInput;
        nSamplesIn -= nSamplesFromInput;
        psEnc->sCmn.inputBufIx += nSamplesToBuffer;

        /* Silk encoder */
        if( psEnc->sCmn.inputBufIx >= psEnc->sCmn.frame_length ) {
            /* Enough data in input buffer, so encode */
            if( MaxBytesOut == 0 ) {
                /* No payload obtained so far */
                MaxBytesOut = *nBytesOut;
                if( ( ret = SKP_Silk_encode_frame_FIX( psEnc, outData, &MaxBytesOut, psEnc->sCmn.inputBuf ) ) != 0 ) {
                    SKP_assert( 0 );
                }
            } else {
                /* outData already contains a payload */
                if( ( ret = SKP_Silk_encode_frame_FIX( psEnc, outData, nBytesOut, psEnc->sCmn.inputBuf ) ) != 0 ) {
                    SKP_assert( 0 );
                }
                /* Check that no second payload was created */
                SKP_assert( *nBytesOut == 0 );
            }
            psEnc->sCmn.inputBufIx = 0;
        } else {
            break;
        }
    }

    *nBytesOut = MaxBytesOut;
    if( psEnc->sCmn.useDTX && psEnc->sCmn.inDTX ) {
        /* Dtx simulation */
        *nBytesOut = 0;
    }


    return ret;
}


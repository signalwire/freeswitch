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

#include "SKP_Silk_main_FIX.h"

/* Control encoder SNR */
SKP_int SKP_Silk_control_encoder_FIX( 
    SKP_Silk_encoder_state_FIX  *psEnc,             /* I/O  Pointer to Silk encoder state                   */
    const SKP_int               API_fs_kHz,         /* I    External (API) sampling rate (kHz)              */
    const SKP_int               PacketSize_ms,      /* I    Packet length (ms)                              */
    SKP_int32                   TargetRate_bps,     /* I    Target max bitrate (bps) (used if SNR_dB == 0)  */
    const SKP_int               PacketLoss_perc,    /* I    Packet loss rate (in percent)                   */
    const SKP_int               INBandFec_enabled,  /* I    Enable (1) / disable (0) inband FEC             */
    const SKP_int               DTX_enabled,        /* I    Enable / disable DTX                            */
    const SKP_int               InputFramesize_ms,  /* I    Inputframe in ms                                */
    const SKP_int               Complexity          /* I    Complexity (0->low; 1->medium; 2->high)         */
)
{
    SKP_int32 LBRRRate_thres_bps;
    SKP_int   k, fs_kHz, ret = 0;
    SKP_int32 frac_Q6;
    const SKP_int32 *rateTable;

    /* State machine for the SWB/WB switching */
    fs_kHz = psEnc->sCmn.fs_kHz;
    
    /* Only switch during low speech activity, when no frames are sitting in the payload buffer */
    if( API_fs_kHz == 8 || fs_kHz == 0 || API_fs_kHz < fs_kHz ) {
        // Switching is not possible, encoder just initialized, or internal mode higher than external
        fs_kHz = API_fs_kHz;
    } else {

        /* Resample all valid data in x_buf. Resampling the last part gets rid of a click, 5ms after switching  */
        /* this is because the same state is used when downsampling in API.c and is then up to date             */
        /* the click immidiatly after switching is most of the time still there                                 */

        if( psEnc->sCmn.fs_kHz == 24 ) {
            /* Accumulate the difference between the target rate and limit */
            if( psEnc->sCmn.fs_kHz_changed == 0 ) {
                psEnc->sCmn.bitrateDiff += SKP_MUL( InputFramesize_ms, TargetRate_bps - SWB2WB_BITRATE_BPS_INITIAL );
            } else {
                psEnc->sCmn.bitrateDiff += SKP_MUL( InputFramesize_ms, TargetRate_bps - SWB2WB_BITRATE_BPS );
            }
            psEnc->sCmn.bitrateDiff = SKP_min( psEnc->sCmn.bitrateDiff, 0 );

            /* Check if we should switch from 24 to 16 kHz */
#if SWITCH_TRANSITION_FILTERING
            if( ( psEnc->sCmn.sLP.transition_frame_no == 0 ) && /* Transition phase not active */
                ( psEnc->sCmn.bitrateDiff <= -ACCUM_BITS_DIFF_THRESHOLD || psEnc->sCmn.sSWBdetect.WB_detected == 1 ) &&
                ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {
                psEnc->sCmn.sLP.transition_frame_no = 1; /* Begin transition phase */
                psEnc->sCmn.sLP.mode = 0; /* Switch down */
            }

            if( ( psEnc->sCmn.sLP.transition_frame_no >= TRANSITION_FRAMES_DOWN ) && ( psEnc->sCmn.sLP.mode == 0 ) && /* Transition phase complete, ready to switch */
#else
            if( ( psEnc->sCmn.bitrateDiff <= -ACCUM_BITS_DIFF_THRESHOLD || psEnc->sCmn.sSWBdetect.WB_detected == 1 ) &&
#endif
                ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {

                    SKP_int16 x_buf[    2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ]; 
                    SKP_int16 x_bufout[ 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ];
                    
                    psEnc->sCmn.bitrateDiff = 0;
                    fs_kHz = 16;

                    SKP_memcpy( x_buf, psEnc->x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                    SKP_memset( psEnc->sCmn.resample24To16state, 0, sizeof( psEnc->sCmn.resample24To16state ) );
                    
#if LOW_COMPLEXITY_ONLY
                    {
                        SKP_int16 scratch[ ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) + SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ];
                        SKP_Silk_resample_2_3_coarse( &x_bufout[ 0 ], psEnc->sCmn.resample24To16state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape, (SKP_int16*)scratch );
                    }
#else
                    SKP_Silk_resample_2_3( &x_bufout[ 0 ], psEnc->sCmn.resample24To16state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );
#endif

                    /* set the first frame to zero, no performance difference was noticed though */
                    SKP_memset( x_bufout, 0, 320 * sizeof( SKP_int16 ) );
                    SKP_memcpy( psEnc->x_buf, x_bufout, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

#if SWITCH_TRANSITION_FILTERING
                    psEnc->sCmn.sLP.transition_frame_no = 0; /* Transition phase complete */
#endif
            }
        } else if( psEnc->sCmn.fs_kHz == 16 ) {

            /* Check if we should switch from 16 to 24 kHz */
#if SWITCH_TRANSITION_FILTERING
            if( ( psEnc->sCmn.sLP.transition_frame_no == 0 ) && /* No transition phase running, ready to switch */
#else
            if(
#endif
                ( API_fs_kHz > psEnc->sCmn.fs_kHz && TargetRate_bps >= WB2SWB_BITRATE_BPS && psEnc->sCmn.sSWBdetect.WB_detected == 0 ) && 
                ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {

                SKP_int16 x_buf[          2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ]; 
                SKP_int16 x_bufout[ 3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) / 2 ]; 
                SKP_int32 resample16To24state[ 11 ];

                psEnc->sCmn.bitrateDiff = 0;
                fs_kHz = 24;
                
                SKP_memcpy( x_buf, psEnc->x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                SKP_memset( resample16To24state, 0, sizeof(resample16To24state) );
                
                SKP_Silk_resample_3_2( &x_bufout[ 0 ], resample16To24state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );

                /* set the first frame to zero, no performance difference was noticed though */
                SKP_memset( x_bufout, 0, 480 * sizeof( SKP_int16 ) );
                SKP_memcpy( psEnc->x_buf, x_bufout, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );
#if SWITCH_TRANSITION_FILTERING
                psEnc->sCmn.sLP.mode = 1; /* Switch up */
#endif
            } else { 
                /* accumulate the difference between the target rate and limit */
                psEnc->sCmn.bitrateDiff += SKP_MUL( InputFramesize_ms, TargetRate_bps - WB2MB_BITRATE_BPS );
                psEnc->sCmn.bitrateDiff = SKP_min( psEnc->sCmn.bitrateDiff, 0 );

                /* Check if we should switch from 16 to 12 kHz */
#if SWITCH_TRANSITION_FILTERING
                if( ( psEnc->sCmn.sLP.transition_frame_no == 0 ) && /* Transition phase not active */
                    ( psEnc->sCmn.bitrateDiff <= -ACCUM_BITS_DIFF_THRESHOLD ) &&
                    ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {
                    psEnc->sCmn.sLP.transition_frame_no = 1; /* Begin transition phase */
                    psEnc->sCmn.sLP.mode = 0; /* Switch down */
                }

                if( ( psEnc->sCmn.sLP.transition_frame_no >= TRANSITION_FRAMES_DOWN ) && ( psEnc->sCmn.sLP.mode == 0 ) && /* Transition phase complete, ready to switch */
#else
                if( ( psEnc->sCmn.bitrateDiff <= -ACCUM_BITS_DIFF_THRESHOLD ) &&
#endif
                    ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {

                    SKP_int16 x_buf[ 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ]; 

                    SKP_memcpy( x_buf, psEnc->x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );
    
                    psEnc->sCmn.bitrateDiff = 0;
                    fs_kHz = 12;
                    
                    if( API_fs_kHz == 24 ) {

                        /* Intermediate upsampling of x_bufFIX from 16 to 24 kHz */
                        SKP_int16 x_buf24[ 3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) / 2 ]; 
                        SKP_int32 scratch[    3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ];
                        SKP_int32 resample16To24state[ 11 ];

                        SKP_memset( resample16To24state, 0, sizeof( resample16To24state ) );
                        SKP_Silk_resample_3_2( &x_buf24[ 0 ], resample16To24state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );

                        /* Update the state of the resampler used in API.c, from 24 to 12 kHz */
                        SKP_memset( psEnc->sCmn.resample24To12state, 0, sizeof( psEnc->sCmn.resample24To12state ) );
                        SKP_Silk_resample_1_2_coarse( &x_buf24[ 0 ], psEnc->sCmn.resample24To12state, &x_buf[ 0 ], scratch, SKP_RSHIFT( SKP_SMULBB( 3, SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape ), 2 ) );

                        /* set the first frame to zero, no performance difference was noticed though */
                        SKP_memset( x_buf, 0, 240 * sizeof( SKP_int16 ) );
                        SKP_memcpy( psEnc->x_buf, x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                    } else if( API_fs_kHz == 16 ) {
                        SKP_int16 x_bufout[ 3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) / 4 ]; 
                        SKP_memset( psEnc->sCmn.resample16To12state, 0, sizeof( psEnc->sCmn.resample16To12state ) );
                        
                        SKP_Silk_resample_3_4( &x_bufout[ 0 ], psEnc->sCmn.resample16To12state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );
                    
                        /* set the first frame to zero, no performance difference was noticed though */
                        SKP_memset( x_bufout, 0, 240 * sizeof( SKP_int16 ) );
                        SKP_memcpy( psEnc->x_buf, x_bufout, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );
                    }
#if SWITCH_TRANSITION_FILTERING
                    psEnc->sCmn.sLP.transition_frame_no = 0; /* Transition phase complete */
#endif
                }
            }
        } else if( psEnc->sCmn.fs_kHz == 12 ) {
        
            /* Check if we should switch from 12 to 16 kHz */
#if SWITCH_TRANSITION_FILTERING
            if( ( psEnc->sCmn.sLP.transition_frame_no == 0 ) && /* No transition phase running, ready to switch */
#else
            if(
#endif
                ( API_fs_kHz > psEnc->sCmn.fs_kHz && TargetRate_bps >= MB2WB_BITRATE_BPS ) &&
                ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {

                SKP_int16 x_buf[ 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ]; 

                SKP_memcpy( x_buf, psEnc->x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                psEnc->sCmn.bitrateDiff = 0;
                fs_kHz = 16;

                /* Reset state of the resampler to be used */
                if( API_fs_kHz == 24 ) {
            
                    SKP_int16 x_bufout[ 2 * 2 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) / 3 ]; 

                    /* Intermediate upsampling of x_bufFIX from 12 to 24 kHz */
                    SKP_int16 x_buf24[ 2 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ]; 
                    SKP_int32 scratch[    3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ];
                    SKP_int32 resample12To24state[6];

                    SKP_memset( resample12To24state, 0, sizeof( resample12To24state ) );
                    SKP_Silk_resample_2_1_coarse( &x_buf[ 0 ], resample12To24state, &x_buf24[ 0 ], scratch, SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );

                    SKP_memset( psEnc->sCmn.resample24To16state, 0, sizeof( psEnc->sCmn.resample24To16state ) );
                
#if LOW_COMPLEXITY_ONLY
                    SKP_assert( sizeof( SKP_int16 ) * ( 2 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) + SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ) <= sizeof( scratch ) );
                    SKP_Silk_resample_2_3_coarse( &x_bufout[ 0 ], psEnc->sCmn.resample24To16state, &x_buf24[ 0 ], SKP_LSHIFT( SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape, 1 ), (SKP_int16*)scratch );
#else
                    SKP_Silk_resample_2_3( &x_bufout[ 0 ], psEnc->sCmn.resample24To16state, &x_buf24[ 0 ], SKP_LSHIFT( SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape, 1 ) );
#endif
                
                    /* set the first frame to zero, no performance difference was noticed though */
                    SKP_memset( x_bufout, 0, 320 * sizeof( SKP_int16 ) );
                    SKP_memcpy( psEnc->x_buf, x_bufout, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );
                }
#if SWITCH_TRANSITION_FILTERING
                psEnc->sCmn.sLP.mode = 1; /* Switch up */
#endif
            } else { 
                /* accumulate the difference between the target rate and limit */
                psEnc->sCmn.bitrateDiff += SKP_MUL( InputFramesize_ms, TargetRate_bps - MB2NB_BITRATE_BPS );
                psEnc->sCmn.bitrateDiff  = SKP_min( psEnc->sCmn.bitrateDiff, 0 );

                /* Check if we should switch from 12 to 8 kHz */
#if SWITCH_TRANSITION_FILTERING
                if( ( psEnc->sCmn.sLP.transition_frame_no == 0 ) && /* Transition phase not active */
                    ( psEnc->sCmn.bitrateDiff <= -ACCUM_BITS_DIFF_THRESHOLD ) &&
                    ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {
                    psEnc->sCmn.sLP.transition_frame_no = 1; /* Begin transition phase */
                    psEnc->sCmn.sLP.mode = 0; /* Switch down */
                }

                if( ( psEnc->sCmn.sLP.transition_frame_no >= TRANSITION_FRAMES_DOWN ) && ( psEnc->sCmn.sLP.mode == 0 ) &&
#else
                if( ( psEnc->sCmn.bitrateDiff <= -ACCUM_BITS_DIFF_THRESHOLD ) &&
#endif
                    ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {
                
                    SKP_int16 x_buf[ 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ]; 

                    SKP_memcpy( x_buf, psEnc->x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                    psEnc->sCmn.bitrateDiff = 0;
                    fs_kHz = 8;

                    if( API_fs_kHz == 24 ) {

                        SKP_int32 scratch[    3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ];
                        /* Intermediate upsampling of x_buf from 12 to 24 kHz */
                        SKP_int16 x_buf24[ 2 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ];
                        SKP_int32 resample12To24state[ 6 ];

                        SKP_memset( resample12To24state, 0, sizeof( resample12To24state ) );
                        SKP_Silk_resample_2_1_coarse( &x_buf[ 0 ], resample12To24state, &x_buf24[ 0 ], scratch, SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );

                        /* Update the state of the resampler used in API.c, from 24 to 8 kHz */
                        SKP_memset( psEnc->sCmn.resample24To8state, 0, sizeof( psEnc->sCmn.resample24To8state ) );
                        SKP_Silk_resample_1_3( &x_buf[ 0 ], psEnc->sCmn.resample24To8state, &x_buf24[ 0 ], SKP_LSHIFT( SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape, 1 ) );

                        /* set the first frame to zero, no performance difference was noticed though */
                        SKP_memset( x_buf, 0, 160 * sizeof( SKP_int16 ) );
                        SKP_memcpy( psEnc->x_buf, x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                    } else if( API_fs_kHz == 16 ) {
                        /* Intermediate upsampling of x_bufFIX from 12 to 16 kHz */
                        SKP_int16 x_buf16[  3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) / 2 ]; 
                        SKP_int32 resample12To16state[11];
                        
                        SKP_memset( resample12To16state, 0, sizeof( resample12To16state ) );
                        SKP_Silk_resample_3_2( &x_buf16[ 0 ], resample12To16state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );
                        
                        /* set the first frame to zero, no performance difference was noticed though */
                        SKP_memset( x_buf, 0, 160 * sizeof( SKP_int16 ) );
                        SKP_memcpy( psEnc->x_buf, x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                    } else if( API_fs_kHz == 12 ) {
                        SKP_int16 x_bufout[ 2 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) / 3 ]; 
                        SKP_memset( psEnc->sCmn.resample12To8state, 0, sizeof( psEnc->sCmn.resample12To8state ) );
#if LOW_COMPLEXITY_ONLY
                        {
                            SKP_int16 scratch[ ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) + SigProc_Resample_2_3_coarse_NUM_FIR_COEFS - 1 ];
                            SKP_Silk_resample_2_3_coarse( &x_bufout[ 0 ], psEnc->sCmn.resample12To8state, &x_buf[ 0 ], 
                                SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape, scratch );
                        }
#else
                        SKP_Silk_resample_2_3( &x_bufout[ 0 ], psEnc->sCmn.resample12To8state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );
#endif
                        /* set the first frame to zero, no performance difference was noticed though */
                        SKP_memset( x_bufout, 0, 160 * sizeof( SKP_int16 ) );
                        SKP_memcpy( psEnc->x_buf, x_bufout, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );
                    }
#if SWITCH_TRANSITION_FILTERING
                    psEnc->sCmn.sLP.transition_frame_no = 0; /* Transition phase complete */
#endif
                }
            }
        } else if( psEnc->sCmn.fs_kHz == 8 ) {

            /* Check if we should switch from 8 to 12 kHz */
#if SWITCH_TRANSITION_FILTERING
            if( ( psEnc->sCmn.sLP.transition_frame_no == 0 ) && /* No transition phase running, ready to switch */
#else
            if(
#endif
                ( API_fs_kHz > psEnc->sCmn.fs_kHz && TargetRate_bps >= NB2MB_BITRATE_BPS ) &&
                ( psEnc->speech_activity_Q8 < 128 && psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {

                SKP_int16 x_buf[ 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ]; 

                SKP_memcpy( x_buf, psEnc->x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );

                psEnc->sCmn.bitrateDiff = 0;
                fs_kHz = 12;

                /* Reset state of the resampler to be used */
                if( API_fs_kHz == 24 ) {
                    SKP_int16 x_buf24[  3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ]; 
                    SKP_int32 scratch[ 3 * 3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) / 2 ];
                    SKP_int32 resample8To24state[ 7 ];

                    /* Intermediate upsampling of x_bufFIX from 8 to 24 kHz */
                    SKP_memset( resample8To24state, 0, sizeof( resample8To24state ) );
                    SKP_Silk_resample_3_1( &x_buf24[ 0 ], resample8To24state, &x_buf[ 0 ], SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );

                    SKP_memset( psEnc->sCmn.resample24To12state, 0, sizeof( psEnc->sCmn.resample24To12state ) );
                
                    SKP_Silk_resample_1_2_coarse( &x_buf24[ 0 ], psEnc->sCmn.resample24To12state, &x_buf[ 0 ], scratch, SKP_RSHIFT( SKP_SMULBB( 3, SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape ), 1 ) );
                
                    /* set the first frame to zero, no performance difference was noticed though */
                    SKP_memset( x_buf, 0, 240 * sizeof( SKP_int16 ) );
                    SKP_memcpy( psEnc->x_buf, x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );
                
                } else if( API_fs_kHz == 16 ) {
                    SKP_int16 x_buf16[ 2 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ]; 
                    SKP_int32 scratch[ 3 * ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) ];
                    SKP_int32 resample8To16state[ 6 ];

                    /* Intermediate upsampling of x_bufFIX from 8 to 16 kHz */
                    SKP_memset( resample8To16state, 0, sizeof( resample8To16state ) );
                    SKP_Silk_resample_2_1_coarse( &x_buf[ 0 ], resample8To16state, &x_buf16[ 0 ], scratch, SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape );

                    SKP_memset( psEnc->sCmn.resample16To12state, 0, sizeof( psEnc->sCmn.resample16To12state ) );
                
                    SKP_Silk_resample_3_4( &x_buf[ 0 ], psEnc->sCmn.resample16To12state, &x_buf16[ 0 ], SKP_LSHIFT( SKP_LSHIFT( psEnc->sCmn.frame_length, 1 ) + psEnc->sCmn.la_shape, 1 ) );
                
                    /* set the first frame to zero, no performance difference was noticed though */
                    SKP_memset( x_buf, 0, 240 * sizeof( SKP_int16 ) );
                    SKP_memcpy( psEnc->x_buf, x_buf, ( 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX ) * sizeof( SKP_int16 ) );
                }
#if SWITCH_TRANSITION_FILTERING
                psEnc->sCmn.sLP.mode = 1; /* Switch up */
#endif
            } 
        } else {
            // Internal sample frequency not supported!
            SKP_assert( 0 );
        }
    }

#if SWITCH_TRANSITION_FILTERING
    /* After switching up, stop transition filter during speech inactivity */
    if( ( psEnc->sCmn.sLP.mode == 1 ) &&
        ( psEnc->sCmn.sLP.transition_frame_no >= TRANSITION_FRAMES_UP ) && 
        ( psEnc->speech_activity_Q8 < 128 ) && 
        ( psEnc->sCmn.nFramesInPayloadBuf == 0 ) ) {
        
        psEnc->sCmn.sLP.transition_frame_no = 0;

        /* Reset transition filter state */
        SKP_memset( psEnc->sCmn.sLP.In_LP_State, 0, 2 * sizeof( SKP_int32 ) );
    }
#endif



    /* Set internal sampling frequency */
    if( psEnc->sCmn.fs_kHz != fs_kHz ) {
        /* reset part of the state */
        SKP_memset( &psEnc->sShape,          0, sizeof( SKP_Silk_shape_state_FIX ) );
        SKP_memset( &psEnc->sPrefilt,        0, sizeof( SKP_Silk_prefilter_state_FIX ) );
        SKP_memset( &psEnc->sNSQ,            0, sizeof( SKP_Silk_nsq_state ) );
        SKP_memset( &psEnc->sPred,           0, sizeof( SKP_Silk_predict_state_FIX ) );
        SKP_memset( psEnc->sNSQ.xq,          0, ( 2 * MAX_FRAME_LENGTH ) * sizeof( SKP_int16 ) );
        SKP_memset( psEnc->sNSQ_LBRR.xq,     0, ( 2 * MAX_FRAME_LENGTH ) * sizeof( SKP_int16 ) );
        SKP_memset( psEnc->sCmn.LBRR_buffer, 0, MAX_LBRR_DELAY * sizeof( SKP_SILK_LBRR_struct ) );
#if SWITCH_TRANSITION_FILTERING
        SKP_memset( psEnc->sCmn.sLP.In_LP_State, 0, 2 * sizeof( SKP_int32 ) );
        if( psEnc->sCmn.sLP.mode == 1 ) {
            /* Begin transition phase */
            psEnc->sCmn.sLP.transition_frame_no = 1;
        } else {
            /* End transition phase */
            psEnc->sCmn.sLP.transition_frame_no = 0;
        }
#endif
        psEnc->sCmn.inputBufIx          = 0;
        psEnc->sCmn.nFramesInPayloadBuf = 0;
        psEnc->sCmn.nBytesInPayloadBuf  = 0;
        psEnc->sCmn.oldest_LBRR_idx     = 0;
        psEnc->sCmn.TargetRate_bps      = 0; /* ensures that psEnc->SNR_dB is recomputed */

        SKP_memset( psEnc->sPred.prev_NLSFq_Q15, 0, MAX_LPC_ORDER * sizeof( SKP_int ) );

        /* Initialize non-zero parameters */
        psEnc->sCmn.prevLag                 = 100;
        psEnc->sCmn.prev_sigtype            = SIG_TYPE_UNVOICED;
        psEnc->sCmn.first_frame_after_reset = 1;
        psEnc->sPrefilt.lagPrev             = 100;
        psEnc->sShape.LastGainIndex        = 1;
        psEnc->sNSQ.lagPrev                = 100;
        psEnc->sNSQ.prev_inv_gain_Q16      = 65536;
        psEnc->sNSQ_LBRR.prev_inv_gain_Q16 = 65536;
        psEnc->sCmn.fs_kHz = fs_kHz;
        if( psEnc->sCmn.fs_kHz == 8 ) {
            psEnc->sCmn.predictLPCOrder = MIN_LPC_ORDER;
            psEnc->sCmn.psNLSF_CB[ 0 ]  = &SKP_Silk_NLSF_CB0_10;
            psEnc->sCmn.psNLSF_CB[ 1 ]  = &SKP_Silk_NLSF_CB1_10;
        } else {
            psEnc->sCmn.predictLPCOrder = MAX_LPC_ORDER;
            psEnc->sCmn.psNLSF_CB[ 0 ]  = &SKP_Silk_NLSF_CB0_16;
            psEnc->sCmn.psNLSF_CB[ 1 ]  = &SKP_Silk_NLSF_CB1_16;
        }
        psEnc->sCmn.frame_length   = SKP_SMULBB( FRAME_LENGTH_MS, fs_kHz );
        psEnc->sCmn.subfr_length   = SKP_DIV32_16( psEnc->sCmn.frame_length, NB_SUBFR );
        psEnc->sCmn.la_pitch       = SKP_SMULBB( LA_PITCH_MS, fs_kHz );
        psEnc->sCmn.la_shape       = SKP_SMULBB( LA_SHAPE_MS, fs_kHz );
        psEnc->sPred.min_pitch_lag = SKP_SMULBB(  3, fs_kHz );
        psEnc->sPred.max_pitch_lag = SKP_SMULBB( 18, fs_kHz );
        psEnc->sPred.pitch_LPC_win_length = SKP_SMULBB( FIND_PITCH_LPC_WIN_MS, fs_kHz );
        if( psEnc->sCmn.fs_kHz == 24 ) {
            psEnc->mu_LTP_Q8 = MU_LTP_QUANT_SWB_Q8;
        } else if( psEnc->sCmn.fs_kHz == 16 ) {
            psEnc->mu_LTP_Q8 = MU_LTP_QUANT_WB_Q8;
        } else if( psEnc->sCmn.fs_kHz == 12 ) {
            psEnc->mu_LTP_Q8 = MU_LTP_QUANT_MB_Q8;
        } else {
            psEnc->mu_LTP_Q8 = MU_LTP_QUANT_NB_Q8;
        }
        psEnc->sCmn.fs_kHz_changed = 1;
        
        /* Check that settings are valid */
        SKP_assert( ( psEnc->sCmn.subfr_length * NB_SUBFR ) == psEnc->sCmn.frame_length );
    } 
   
    /* Set encoding complexity */
    if( Complexity == 0 || LOW_COMPLEXITY_ONLY ) {
        /* Low complexity */
        psEnc->sCmn.Complexity                  = 0;
        psEnc->sCmn.pitchEstimationComplexity   = PITCH_EST_COMPLEXITY_LC_MODE;
        psEnc->pitchEstimationThreshold_Q16     = FIND_PITCH_CORRELATION_THRESHOLD_Q16_LC_MODE;
        psEnc->sCmn.pitchEstimationLPCOrder     = 8;
        psEnc->sCmn.shapingLPCOrder             = 12;
        psEnc->sCmn.nStatesDelayedDecision      = 1;
        psEnc->NoiseShapingQuantizer            = SKP_Silk_NSQ;
        psEnc->sCmn.useInterpolatedNLSFs        = 0;
        psEnc->sCmn.LTPQuantLowComplexity       = 1;
        psEnc->sCmn.NLSF_MSVQ_Survivors         = MAX_NLSF_MSVQ_SURVIVORS_LC_MODE;
    } else if( Complexity == 1 ) {
        /* Medium complexity */
        psEnc->sCmn.Complexity                  = 1;
        psEnc->sCmn.pitchEstimationComplexity   = PITCH_EST_COMPLEXITY_MC_MODE;
        psEnc->pitchEstimationThreshold_Q16     = FIND_PITCH_CORRELATION_THRESHOLD_Q16_MC_MODE;
        psEnc->sCmn.pitchEstimationLPCOrder     = 12;
        psEnc->sCmn.shapingLPCOrder             = 16;
        psEnc->sCmn.nStatesDelayedDecision      = 2;
        psEnc->NoiseShapingQuantizer            = SKP_Silk_NSQ_del_dec;
        psEnc->sCmn.useInterpolatedNLSFs        = 0;
        psEnc->sCmn.LTPQuantLowComplexity       = 0;
        psEnc->sCmn.NLSF_MSVQ_Survivors         = MAX_NLSF_MSVQ_SURVIVORS_MC_MODE;
    } else if( Complexity == 2 ) {
        /* High complexity */
        psEnc->sCmn.Complexity                  = 2;
        psEnc->sCmn.pitchEstimationComplexity   = PITCH_EST_COMPLEXITY_HC_MODE;
        psEnc->pitchEstimationThreshold_Q16     = FIND_PITCH_CORRELATION_THRESHOLD_Q16_HC_MODE;
        psEnc->sCmn.pitchEstimationLPCOrder     = 16;
        psEnc->sCmn.shapingLPCOrder             = 16;
        psEnc->sCmn.nStatesDelayedDecision      = 4;
        psEnc->NoiseShapingQuantizer            = SKP_Silk_NSQ_del_dec;
        psEnc->sCmn.useInterpolatedNLSFs        = 1;
        psEnc->sCmn.LTPQuantLowComplexity       = 0;
        psEnc->sCmn.NLSF_MSVQ_Survivors         = MAX_NLSF_MSVQ_SURVIVORS;
    } else {
        ret = SKP_SILK_ENC_WRONG_COMPLEXITY_SETTING;
    }

    /* Dont have higher Pitch estimation LPC order than predict LPC order */
    psEnc->sCmn.pitchEstimationLPCOrder = SKP_min_int( psEnc->sCmn.pitchEstimationLPCOrder, psEnc->sCmn.predictLPCOrder );

    SKP_assert( psEnc->sCmn.pitchEstimationLPCOrder <= FIND_PITCH_LPC_ORDER_MAX );
    SKP_assert( psEnc->sCmn.shapingLPCOrder         <= SHAPE_LPC_ORDER_MAX );
    SKP_assert( psEnc->sCmn.nStatesDelayedDecision  <= DEL_DEC_STATES_MAX );

    /* Set bitrate/coding quality */
    TargetRate_bps = SKP_min( TargetRate_bps, 100000 );
    if( psEnc->sCmn.fs_kHz == 8 ) {
        TargetRate_bps = SKP_max( TargetRate_bps, MIN_TARGET_RATE_NB_BPS );
    } else if( psEnc->sCmn.fs_kHz == 12 ) {
        TargetRate_bps = SKP_max( TargetRate_bps, MIN_TARGET_RATE_MB_BPS );
    } else if( psEnc->sCmn.fs_kHz == 16 ) {
        TargetRate_bps = SKP_max( TargetRate_bps, MIN_TARGET_RATE_WB_BPS );
    } else {
        TargetRate_bps = SKP_max( TargetRate_bps, MIN_TARGET_RATE_SWB_BPS );
    }
    if( TargetRate_bps != psEnc->sCmn.TargetRate_bps ) {
        psEnc->sCmn.TargetRate_bps = TargetRate_bps;

        /* if new TargetRate_bps, translate to SNR_dB value */
        if( psEnc->sCmn.fs_kHz == 8 ) {
            rateTable = TargetRate_table_NB;
        } else if( psEnc->sCmn.fs_kHz == 12 ) {
            rateTable = TargetRate_table_MB;
        } else if( psEnc->sCmn.fs_kHz == 16 ) {
            rateTable = TargetRate_table_WB;
        } else {
            rateTable = TargetRate_table_SWB;
        }
        for( k = 1; k < TARGET_RATE_TAB_SZ; k++ ) {
            /* find bitrate interval in table and interpolate */
            if( TargetRate_bps < rateTable[ k ] ) {
                frac_Q6 = SKP_DIV32( SKP_LSHIFT( TargetRate_bps - rateTable[ k - 1 ], 6 ), rateTable[ k ] - rateTable[ k - 1 ] );
                psEnc->SNR_dB_Q7 = SKP_LSHIFT( SNR_table_Q1[ k - 1 ], 6 ) + SKP_MUL( frac_Q6, SNR_table_Q1[ k ] - SNR_table_Q1[ k - 1 ] );
                break;
            }
        }
    }

    /* Set packet size */
    if( ( PacketSize_ms !=  20 ) && 
        ( PacketSize_ms !=  40 ) && 
        ( PacketSize_ms !=  60 ) && 
        ( PacketSize_ms !=  80 ) && 
        ( PacketSize_ms != 100 ) ) {
        ret = SKP_SILK_ENC_PACKET_SIZE_NOT_SUPPORTED;
    } else {
        if( PacketSize_ms != psEnc->sCmn.PacketSize_ms ) {
            psEnc->sCmn.PacketSize_ms = PacketSize_ms;

            /* Packet length changes. Reset LBRR buffer */
            SKP_Silk_LBRR_reset( &psEnc->sCmn );
        }
    }

    /* Set packet loss rate measured by farend */
    if( ( PacketLoss_perc < 0 ) || ( PacketLoss_perc > 100 ) ) {
        ret = SKP_SILK_ENC_WRONG_LOSS_RATE;
    }
    psEnc->sCmn.PacketLoss_perc = PacketLoss_perc;

#if USE_LBRR
    if( INBandFec_enabled < 0 || INBandFec_enabled > 1 ) {
        ret = SKP_SILK_ENC_WRONG_INBAND_FEC_SETTING;
    }
    
    /* Only change settings if first frame in packet */
    if( psEnc->sCmn.nFramesInPayloadBuf == 0 ) {
        
        psEnc->sCmn.LBRR_enabled = INBandFec_enabled;
        if( psEnc->sCmn.fs_kHz == 8 ) {
            LBRRRate_thres_bps = INBAND_FEC_MIN_RATE_BPS - 9000;
        } else if( psEnc->sCmn.fs_kHz == 12 ) {
            LBRRRate_thres_bps = INBAND_FEC_MIN_RATE_BPS - 6000;;
        } else if( psEnc->sCmn.fs_kHz == 16 ) {
            LBRRRate_thres_bps = INBAND_FEC_MIN_RATE_BPS - 3000;
        } else {
            LBRRRate_thres_bps = INBAND_FEC_MIN_RATE_BPS;
        }

        if( psEnc->sCmn.TargetRate_bps >= LBRRRate_thres_bps ) {
            /* Set gain increase / rate reduction for LBRR usage */
            /* Coarse tuned with pesq for now. */
            /* Linear regression coefs G = 8 - 0.5 * loss */
            /* Meaning that at 16% loss main rate and redundant rate is the same, -> G = 0 */
            psEnc->sCmn.LBRR_GainIncreases = SKP_max_int( 8 - SKP_RSHIFT( psEnc->sCmn.PacketLoss_perc, 1 ), 0 );

            /* Set main stream rate compensation */
            if( psEnc->sCmn.LBRR_enabled && psEnc->sCmn.PacketLoss_perc > LBRR_LOSS_THRES ) {
                /* Tuned to give aprox same mean / weighted bitrate as no inband FEC */
                psEnc->inBandFEC_SNR_comp_Q8 = ( 6 << 8 ) - SKP_LSHIFT( psEnc->sCmn.LBRR_GainIncreases, 7 );
            } else {
                psEnc->inBandFEC_SNR_comp_Q8 = 0;
                psEnc->sCmn.LBRR_enabled     = 0;
            }
        } else {
            psEnc->inBandFEC_SNR_comp_Q8     = 0;
            psEnc->sCmn.LBRR_enabled         = 0;
        }
    }
#else
    psEnc->sCmn.LBRR_enabled = 0;
#endif

    /* Set DTX mode */
    if( DTX_enabled < 0 || DTX_enabled > 1 ) {
        ret = SKP_SILK_ENC_WRONG_DTX_SETTING;
    }
    psEnc->sCmn.useDTX = DTX_enabled;
    
    return ret;
}

/* Control low bitrate redundancy usage */
void SKP_Silk_LBRR_ctrl_FIX(
    SKP_Silk_encoder_state_FIX      *psEnc,     /* I/O  encoder state                               */
    SKP_Silk_encoder_control_FIX    *psEncCtrl  /* I/O  encoder control                             */
)
{
    SKP_int LBRR_usage;

    if( psEnc->sCmn.LBRR_enabled ) {
        /* Control LBRR */

        /* Usage Control based on sensitivity and packet loss caracteristics */
        /* For now only enable adding to next for active frames. Make more complex later */
        LBRR_usage = SKP_SILK_NO_LBRR;
        if( psEnc->speech_activity_Q8 > LBRR_SPEECH_ACTIVITY_THRES_Q8 && psEnc->sCmn.PacketLoss_perc > LBRR_LOSS_THRES ) { // nb! maybe multiply loss prob and speech activity 
            //if( psEnc->PacketLoss_burst > BURST_THRES )
            //  psEncCtrl->LBRR_usage = SKP_SILK_ADD_LBRR_TO_PLUS2;
            //} else {
                LBRR_usage = SKP_SILK_ADD_LBRR_TO_PLUS1;//SKP_SILK_NO_LBRR
            //}
        }
        psEncCtrl->sCmn.LBRR_usage = LBRR_usage;
    } else {
        psEncCtrl->sCmn.LBRR_usage = SKP_SILK_NO_LBRR;
    }
}

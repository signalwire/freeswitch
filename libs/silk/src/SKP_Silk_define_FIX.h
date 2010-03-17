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

#ifndef SKP_SILK_DEFINE_FIX_H
#define SKP_SILK_DEFINE_FIX_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Head room for correlations                           */
#define LTP_CORRS_HEAD_ROOM                             2
#define LPC_CORRS_HEAD_ROOM                             10

#define WB_DETECT_ACTIVE_SPEECH_LEVEL_THRES_Q8          179     // 179.2_Q8 = 0.7f required speech activity for counting frame as active

/* DTX settings */
#define SPEECH_ACTIVITY_DTX_THRES_Q8                    26      // 25.60_Q8 = 0.1f

#define LBRR_SPEECH_ACTIVITY_THRES_Q8                   128

/* level of noise floor for whitening filter LPC analysis in pitch analysis */
#define FIND_PITCH_WHITE_NOISE_FRACTION_Q16             66

/* bandwdith expansion for whitening filter in pitch analysis */
#define FIND_PITCH_BANDWITH_EXPANSION_Q16               64881

/* Threshold used by pitch estimator for early escape */
#define FIND_PITCH_CORRELATION_THRESHOLD_Q16_HC_MODE    45875       // 0.7
#define FIND_PITCH_CORRELATION_THRESHOLD_Q16_MC_MODE    49152       // 0.75
#define FIND_PITCH_CORRELATION_THRESHOLD_Q16_LC_MODE    52429       // 0.8

/* Regualarization factor for correlation matrix. Equivalent to adding noise at -50 dB */
#define FIND_LTP_COND_FAC_Q31                           21475
#define FIND_LPC_COND_FAC_Q32                           257698       // 6e-5

/* Find Pred Coef defines */
#define INACTIVE_BWExp_Q16                              64225       // 0.98
#define ACTIVE_BWExp_Q16                                65470       // 0.999
#define LTP_DAMPING_Q16                                 66
#define LTP_SMOOTHING_Q26                               6710886

/* LTP quantization settings */
#define MU_LTP_QUANT_NB_Q8                              8
#define MU_LTP_QUANT_MB_Q8                              6
#define MU_LTP_QUANT_WB_Q8                              5
#define MU_LTP_QUANT_SWB_Q8                             4

/***********************/
/* High pass filtering */
/***********************/
/* Smoothing parameters for low end of pitch frequency range estimation */
#define VARIABLE_HP_SMTH_COEF1_Q16                      6554    // 0.1
#define VARIABLE_HP_SMTH_COEF2_Q16                      983     // 0.015

/* Min and max values for low end of pitch frequency range estimation */
#define VARIABLE_HP_MIN_FREQ_Q0                         80
#define VARIABLE_HP_MAX_FREQ_Q0                         150

/* Max absolute difference between log2 of pitch frequency and smoother state, to enter the smoother */
#define VARIABLE_HP_MAX_DELTA_FREQ_Q7                   51      // 0.4 in Q7

/* Defines for CN generation */
#define CNG_BUF_MASK_MAX                                255             /* 2^floor(log2(MAX_FRAME_LENGTH))  */
#define CNG_GAIN_SMTH_Q16                               4634            /* 0.25^(1/4)                       */
#define CNG_NLSF_SMTH_Q16                               16348           /* 0.25                             */

#ifdef __cplusplus
}
#endif

#endif

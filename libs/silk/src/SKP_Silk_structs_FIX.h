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

#ifndef SKP_SILK_STRUCTS_FIX_H
#define SKP_SILK_STRUCTS_FIX_H

#include "SKP_Silk_typedef.h"
#include "SKP_Silk_define_FIX.h"
#include "SKP_Silk_main.h"
#include "SKP_Silk_structs.h"

#if LOW_COMPLEXITY_ONLY
#include "SKP_Silk_resample_rom.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/********************************/
/* Noise shaping analysis state */
/********************************/
typedef struct {
    SKP_int     LastGainIndex;
    SKP_int32   HarmBoost_smth_Q16;
    SKP_int32   HarmShapeGain_smth_Q16;
    SKP_int32   Tilt_smth_Q16;
} SKP_Silk_shape_state_FIX;

/********************************/
/* Prefilter state              */
/********************************/
typedef struct {
    SKP_int16   sLTP_shp1[ LTP_BUF_LENGTH ];
    SKP_int32   sAR_shp2_Q14[ SHAPE_LPC_ORDER_MAX ];
    SKP_int16   sLTP_shp2_FIX[ LTP_BUF_LENGTH ];
    SKP_int     sLTP_shp_buf_idx1;
    SKP_int     sAR_shp_buf_idx2;
    SKP_int     sLTP_shp_buf_idx2;
    SKP_int32   sLF_AR_shp1_Q12;
    SKP_int32   sLF_MA_shp1_Q12;
    SKP_int32   sLF_AR_shp2_Q12;
    SKP_int32   sLF_MA_shp2_Q12;
    SKP_int     sHarmHP;
    SKP_int32   rand_seed;
    SKP_int     lagPrev;
} SKP_Silk_prefilter_state_FIX;

/*****************************/
/* Prediction analysis state */
/*****************************/
typedef struct {
    SKP_int   pitch_LPC_win_length;
    SKP_int   min_pitch_lag;                                        /* Lowest possible pitch lag (samples)  */
    SKP_int   max_pitch_lag;                                        /* Highest possible pitch lag (samples) */
    SKP_int   prev_NLSFq_Q15[ MAX_LPC_ORDER ];                      /* Prev. quant. normalized LSF vector   */
} SKP_Silk_predict_state_FIX;

#if( defined( SAVE_ALL_INTERNAL_DATA ) || defined( USE_FLT2FIX_WRAPPER ) )
/*******************************************/
/* Structure containing NLSF MSVQ codebook */
/*******************************************/
/* structure for one stage of MSVQ */
typedef struct {
    const SKP_int32     nVectors;
    const SKP_float     *CB;
    const SKP_float     *Rates;
} SKP_Silk_NLSF_CBS_FLP_TMP;

typedef struct {
    const SKP_int32                 nStages;

    /* fields for (de)quantizing */
    const SKP_Silk_NLSF_CBS_FLP_TMP *CBStages;
    const SKP_float                 *NDeltaMin;

    /* fields for arithmetic (de)coding */
    const SKP_uint16                *CDF;
    const SKP_uint16 * const        *StartPtr;
    const SKP_int                   *MiddleIx;
} SKP_Silk_NLSF_CB_struct_FLP_TMP;
#endif

/********************************/
/* Encoder state FIX            */
/********************************/
typedef struct {
    SKP_Silk_encoder_state          sCmn;                           /* Common struct, shared with floating-point code */

#if HIGH_PASS_INPUT
    SKP_int32                       variable_HP_smth1_Q15;          /* State of first smoother                                              */
    SKP_int32                       variable_HP_smth2_Q15;          /* State of second smoother                                             */
#endif
    SKP_Silk_shape_state_FIX        sShape;                         /* Shape state                                                          */
    SKP_Silk_prefilter_state_FIX    sPrefilt;                       /* Prefilter State                                                      */
    SKP_Silk_predict_state_FIX      sPred;                          /* Prediction state                                                     */
    SKP_Silk_nsq_state              sNSQ;                           /* Noise Shape Quantizer State                                          */
    SKP_Silk_nsq_state              sNSQ_LBRR;                      /* Noise Shape Quantizer State ( for low bitrate redundancy )           */
    
    /* Function pointer to noise shaping quantizer (will be set to SKP_Silk_NSQ or SKP_Silk_NSQ_del_dec) */
    void    (* NoiseShapingQuantizer)( SKP_Silk_encoder_state *, SKP_Silk_encoder_control *, SKP_Silk_nsq_state *, const SKP_int16 *, 
                                        SKP_int *, const SKP_int, const SKP_int16 *, const SKP_int16 *, const SKP_int16 *, const SKP_int *, 
                                        const SKP_int *, const SKP_int32 *, const SKP_int32 *, SKP_int, const SKP_int
    );

    /* Buffer for find pitch and noise shape analysis */
    SKP_array_of_int16_4_byte_aligned( x_buf, 2 * MAX_FRAME_LENGTH + LA_SHAPE_MAX );    
    SKP_int                         LTPCorr_Q15;                    /* Normalized correlation from pitch lag estimator                      */
    SKP_int                         mu_LTP_Q8;                      /* Rate-distortion tradeoff in LTP quantization                         */
    SKP_int32                       SNR_dB_Q7;                      /* Quality setting                                                      */
    SKP_int32                       avgGain_Q16;                    /* average gain during active speech                                    */
    SKP_int32                       avgGain_Q16_one_bit_per_sample; /* average gain during active speech                                    */
    SKP_int                         BufferedInChannel_ms;           /* Simulated number of ms buffer because of exceeded TargetRate_bps     */
    SKP_int                         speech_activity_Q8;             /* Speech activity in Q8                                                */
    SKP_int32                       pitchEstimationThreshold_Q16;   /* Threshold for pitch estimator                                        */
    
    /* Parameters For LTP scaling Control */
    SKP_int                         prevLTPredCodGain_Q7;
    SKP_int                         HPLTPredCodGain_Q7;

    SKP_int32                       inBandFEC_SNR_comp_Q8;          /* Compensation to SNR_dB when using inband FEC Voiced      */

#ifdef USE_FLT2FIX_WRAPPER
    const SKP_Silk_NLSF_CB_struct_FLP_TMP *psNLSF_CB_FLP[ 2 ];      /* Pointers to voiced/unvoiced NLSF codebooks */    
#endif
} SKP_Silk_encoder_state_FIX;

/************************/
/* Encoder control FIX  */
/************************/
typedef struct {
    SKP_Silk_encoder_control        sCmn;                           /* Common struct, shared with floating-point code */

    /* Prediction and coding parameters */
    SKP_int32                   Gains_Q16[ NB_SUBFR ];
    SKP_array_of_int16_4_byte_aligned( PredCoef_Q12[ 2 ], MAX_LPC_ORDER );
    SKP_int16                   LTPCoef_Q14[ LTP_ORDER * NB_SUBFR ];
    SKP_int                     LTP_scale_Q14;

    /* Noise shaping parameters */
    /* Testing */
    SKP_array_of_int16_4_byte_aligned( AR1_Q13, NB_SUBFR * SHAPE_LPC_ORDER_MAX );
    SKP_array_of_int16_4_byte_aligned( AR2_Q13, NB_SUBFR * SHAPE_LPC_ORDER_MAX );
    SKP_int32   LF_shp_Q14[        NB_SUBFR ];          /* Packs two int16 coefficients per int32 value             */
    SKP_int     GainsPre_Q14[      NB_SUBFR ];
    SKP_int     HarmBoost_Q14[     NB_SUBFR ];
    SKP_int     Tilt_Q14[          NB_SUBFR ];
    SKP_int     HarmShapeGain_Q14[ NB_SUBFR ];
    SKP_int     Lambda_Q10;
    SKP_int     input_quality_Q14;
    SKP_int     coding_quality_Q14;
    SKP_int32   pitch_freq_low_Hz;
    SKP_int     current_SNR_dB_Q7;

    /* measures */
    SKP_int     sparseness_Q8;
    SKP_int     LTPredCodGain_Q7;
    SKP_int     input_quality_bands_Q15[ VAD_N_BANDS ];
    SKP_int     input_tilt_Q15;
    SKP_int32   ResNrg[ NB_SUBFR ];             /* Residual energy per subframe                             */
    SKP_int     ResNrgQ[ NB_SUBFR ];            /* Q domain for the residual energy > 0                     */
    
} SKP_Silk_encoder_control_FIX;


#ifdef __cplusplus
}
#endif

#endif

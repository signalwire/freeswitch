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

#ifndef SKP_SILK_PERCEPTUAL_PARAMETERS_FIX_H
#define SKP_SILK_PERCEPTUAL_PARAMETERS_FIX_H

#ifdef __cplusplus
extern "C"
{
#endif

/* reduction in coding SNR during low speech activity */
#define BG_SNR_DECR_dB_Q7                               (3<<7)

/* factor for reducing quantization noise during voiced speech */
#define HARM_SNR_INCR_dB_Q7                             (2<<7)

/* factor for reducing quantization noise for unvoiced sparse signals */
#define SPARSE_SNR_INCR_dB_Q7                           (2<<7)

/* threshold for sparseness measure above which to use lower quantization offset during unvoiced */
#define SPARSENESS_THRESHOLD_QNT_OFFSET_Q8              (3<<6) // 0.75


/* noise shaping filter chirp factor */
#define BANDWIDTH_EXPANSION_Q16                         61604 // 0.94

/* difference between chirp factors for analysis and synthesis noise shaping filters at low bitrates */
#define LOW_RATE_BANDWIDTH_EXPANSION_DELTA_Q16          655 //0.01f

/* factor to reduce all bandwidth expansion coefficients for super wideband, relative to wideband */
#define SWB_BANDWIDTH_EXPANSION_REDUCTION_Q16           (1<<16) // 1.0f;

/* gain reduction for fricatives */
#define DE_ESSER_COEF_SWB_dB_Q7                         (2 << 7)
#define DE_ESSER_COEF_WB_dB_Q7                          (1 << 7)


/* extra harmonic boosting (signal shaping) at low bitrates */
#define LOW_RATE_HARMONIC_BOOST_Q16                     6554 // 0.1

/* extra harmonic boosting (signal shaping) for noisy input signals */
#define LOW_INPUT_QUALITY_HARMONIC_BOOST_Q16            6554 // 0.1

/* harmonic noise shaping */
#define HARMONIC_SHAPING_Q16                            19661 // 0.3

/* extra harmonic noise shaping for high bitrates or noisy input */
#define HIGH_RATE_OR_LOW_QUALITY_HARMONIC_SHAPING_Q16   13107 // 0.2


/* parameter for shaping noise towards higher frequencies */
#define HP_NOISE_COEF_Q16                               19661 // 0.3

/* parameter for shaping noise extra towards higher frequencies during voiced speech */
#define HARM_HP_NOISE_COEF_Q24                          7549747 // 0.45

/* parameter for applying a high-pass tilt to the input signal */
#define INPUT_TILT_Q26                                  2684355 // 0.04

/* parameter for extra high-pass tilt to the input signal at high rates */
#define HIGH_RATE_INPUT_TILT_Q12                        246 // 0.06

/* parameter for reducing noise at the very low frequencies */
#define LOW_FREQ_SHAPING_Q0                             3

/* less reduction of noise at the very low frequencies for signals with low SNR at low frequencies */
#define LOW_QUALITY_LOW_FREQ_SHAPING_DECR_Q1            1 // 0.5_Q0

/* fraction added to first autocorrelation value */
#define SHAPE_WHITE_NOISE_FRACTION_Q20                  50 // 50_Q20 = 4.7684e-5

/* fraction of first autocorrelation value added to residual energy value; limits prediction gain */
#define SHAPE_MIN_ENERGY_RATIO_Q24                      256

/* noise floor to put a low limit on the quantization step size */
#define NOISE_FLOOR_dB_Q7                               (4 << 7)

/* noise floor relative to active speech gain level */
#define RELATIVE_MIN_GAIN_dB_Q7                         -6400 // -50_Q0 = -6400_Q7

/* subframe smoothing coefficient for determining active speech gain level (lower -> more smoothing) */
#define GAIN_SMOOTHING_COEF_Q10                         1 // 1e-3_Q0 = 1.024_Q10

/* subframe smoothing coefficient for HarmBoost, HarmShapeGain, Tilt (lower -> more smoothing) */
#define SUBFR_SMTH_COEF_Q16                             26214 // 0.4

#define NOISE_GAIN_VL_Q16                               7864
#define NOISE_GAIN_VH_Q16                               7864
#define NOISE_GAIN_UVL_Q16                              6554
#define NOISE_GAIN_UVH_Q16                              9830

#ifdef __cplusplus
}
#endif

#endif //SKP_SILK_PERCEPTUAL_PARAMETERS_FIX_H

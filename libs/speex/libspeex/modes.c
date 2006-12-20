/* Copyright (C) 2002-2006 Jean-Marc Valin 
   File: modes.c

   Describes the different modes of the codec

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modes.h"
#include "ltp.h"
#include "quant_lsp.h"
#include "cb_search.h"
#include "sb_celp.h"
#include "nb_celp.h"
#include "vbr.h"
#include "misc.h"
#include <math.h>

#ifndef NULL
#define NULL 0
#endif

#define MAX_IN_SAMPLES 640

const SpeexMode * const speex_mode_list[SPEEX_NB_MODES] = {&speex_nb_mode, &speex_wb_mode, &speex_uwb_mode};

/* Extern declarations for all codebooks we use here */
extern const signed char gain_cdbk_nb[];
extern const signed char gain_cdbk_lbr[];
extern const signed char hexc_table[];
extern const signed char exc_5_256_table[];
extern const signed char exc_5_64_table[];
extern const signed char exc_8_128_table[];
extern const signed char exc_10_32_table[];
extern const signed char exc_10_16_table[];
extern const signed char exc_20_32_table[];
extern const signed char hexc_10_32_table[];


/* Parameters for Long-Term Prediction (LTP)*/
static const ltp_params ltp_params_nb = {
   gain_cdbk_nb,
   7,
   7
};

/* Parameters for Long-Term Prediction (LTP)*/
static const ltp_params ltp_params_vlbr = {
   gain_cdbk_lbr,
   5,
   0
};

/* Parameters for Long-Term Prediction (LTP)*/
static const ltp_params ltp_params_lbr = {
   gain_cdbk_lbr,
   5,
   7
};

/* Parameters for Long-Term Prediction (LTP)*/
static const ltp_params ltp_params_med = {
   gain_cdbk_lbr,
   5,
   7
};

/* Split-VQ innovation parameters for very low bit-rate narrowband */
static const split_cb_params split_cb_nb_vlbr = {
   10,               /*subvect_size*/
   4,               /*nb_subvect*/
   exc_10_16_table, /*shape_cb*/
   4,               /*shape_bits*/
   0,
};

/* Split-VQ innovation parameters for very low bit-rate narrowband */
static const split_cb_params split_cb_nb_ulbr = {
   20,               /*subvect_size*/
   2,               /*nb_subvect*/
   exc_20_32_table, /*shape_cb*/
   5,               /*shape_bits*/
   0,
};

/* Split-VQ innovation parameters for low bit-rate narrowband */
static const split_cb_params split_cb_nb_lbr = {
   10,              /*subvect_size*/
   4,               /*nb_subvect*/
   exc_10_32_table, /*shape_cb*/
   5,               /*shape_bits*/
   0,
};


/* Split-VQ innovation parameters narrowband */
static const split_cb_params split_cb_nb = {
   5,               /*subvect_size*/
   8,               /*nb_subvect*/
   exc_5_64_table, /*shape_cb*/
   6,               /*shape_bits*/
   0,
};

/* Split-VQ innovation parameters narrowband */
static const split_cb_params split_cb_nb_med = {
   8,               /*subvect_size*/
   5,               /*nb_subvect*/
   exc_8_128_table, /*shape_cb*/
   7,               /*shape_bits*/
   0,
};

/* Split-VQ innovation for low-band wideband */
static const split_cb_params split_cb_sb = {
   5,               /*subvect_size*/
   8,              /*nb_subvect*/
   exc_5_256_table,    /*shape_cb*/
   8,               /*shape_bits*/
   0,
};

#ifndef DISABLE_WIDEBAND

/* Split-VQ innovation for high-band wideband */
static const split_cb_params split_cb_high = {
   8,               /*subvect_size*/
   5,               /*nb_subvect*/
   hexc_table,       /*shape_cb*/
   7,               /*shape_bits*/
   1,
};


/* Split-VQ innovation for high-band wideband */
static const split_cb_params split_cb_high_lbr = {
   10,               /*subvect_size*/
   4,               /*nb_subvect*/
   hexc_10_32_table,       /*shape_cb*/
   5,               /*shape_bits*/
   0,
};

#endif

/* 2150 bps "vocoder-like" mode for comfort noise */
static const SpeexSubmode nb_submode1 = {
   0,
   1,
   0,
   0,
   /* LSP quantization */
   lsp_quant_lbr,
   lsp_unquant_lbr,
   /* No pitch quantization */
   forced_pitch_quant,
   forced_pitch_unquant,
   NULL,
   /* No innovation quantization (noise only) */
   noise_codebook_quant,
   noise_codebook_unquant,
   NULL,
   -1,
   43
};

/* 3.95 kbps very low bit-rate mode */
static const SpeexSubmode nb_submode8 = {
   0,
   1,
   0,
   0,
   /*LSP quantization*/
   lsp_quant_lbr,
   lsp_unquant_lbr,
   /*No pitch quantization*/
   forced_pitch_quant,
   forced_pitch_unquant,
   NULL,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_nb_ulbr,
   QCONST16(.5,15),
   79
};

/* 5.95 kbps very low bit-rate mode */
static const SpeexSubmode nb_submode2 = {
   0,
   0,
   0,
   0,
   /*LSP quantization*/
   lsp_quant_lbr,
   lsp_unquant_lbr,
   /*No pitch quantization*/
   pitch_search_3tap,
   pitch_unquant_3tap,
   &ltp_params_vlbr,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_nb_vlbr,
   QCONST16(.6,15),
   119
};

/* 8 kbps low bit-rate mode */
static const SpeexSubmode nb_submode3 = {
   -1,
   0,
   1,
   0,
   /*LSP quantization*/
   lsp_quant_lbr,
   lsp_unquant_lbr,
   /*Pitch quantization*/
   pitch_search_3tap,
   pitch_unquant_3tap,
   &ltp_params_lbr,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_nb_lbr,
   QCONST16(.55,15),
   160
};

/* 11 kbps medium bit-rate mode */
static const SpeexSubmode nb_submode4 = {
   -1,
   0,
   1,
   0,
   /*LSP quantization*/
   lsp_quant_lbr,
   lsp_unquant_lbr,
   /*Pitch quantization*/
   pitch_search_3tap,
   pitch_unquant_3tap,
   &ltp_params_med,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_nb_med,
   QCONST16(.45,15),
   220
};

/* 15 kbps high bit-rate mode */
static const SpeexSubmode nb_submode5 = {
   -1,
   0,
   3,
   0,
   /*LSP quantization*/
   lsp_quant_nb,
   lsp_unquant_nb,
   /*Pitch quantization*/
   pitch_search_3tap,
   pitch_unquant_3tap,
   &ltp_params_nb,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_nb,
   QCONST16(.3,15),
   300
};

/* 18.2 high bit-rate mode */
static const SpeexSubmode nb_submode6 = {
   -1,
   0,
   3,
   0,
   /*LSP quantization*/
   lsp_quant_nb,
   lsp_unquant_nb,
   /*Pitch quantization*/
   pitch_search_3tap,
   pitch_unquant_3tap,
   &ltp_params_nb,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_sb,
   QCONST16(.2,15),
   364
};

/* 24.6 kbps high bit-rate mode */
static const SpeexSubmode nb_submode7 = {
   -1,
   0,
   3,
   1,
   /*LSP quantization*/
   lsp_quant_nb,
   lsp_unquant_nb,
   /*Pitch quantization*/
   pitch_search_3tap,
   pitch_unquant_3tap,
   &ltp_params_nb,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_nb,
   QCONST16(.1,15),
   492
};


/* Default mode for narrowband */
static const SpeexNBMode nb_mode = {
   160,    /*frameSize*/
   40,     /*subframeSize*/
   10,     /*lpcSize*/
   17,     /*pitchStart*/
   144,    /*pitchEnd*/
#ifdef FIXED_POINT
   29491, 19661, /* gamma1, gamma2 */
#else
   0.9, 0.6, /* gamma1, gamma2 */
#endif
   .012,   /*lag_factor*/
   QCONST16(.0002,15), /*lpc_floor*/
#ifdef EPIC_48K
   0,
#endif
   {NULL, &nb_submode1, &nb_submode2, &nb_submode3, &nb_submode4, &nb_submode5, &nb_submode6, &nb_submode7,
   &nb_submode8, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
   5,
   {1, 8, 2, 3, 3, 4, 4, 5, 5, 6, 7}
};


/* Default mode for narrowband */
const SpeexMode speex_nb_mode = {
   &nb_mode,
   nb_mode_query,
   "narrowband",
   0,
   4,
   &nb_encoder_init,
   &nb_encoder_destroy,
   &nb_encode,
   &nb_decoder_init,
   &nb_decoder_destroy,
   &nb_decode,
   &nb_encoder_ctl,
   &nb_decoder_ctl,
};


/* Wideband part */

static const SpeexSubmode wb_submode1 = {
   0,
   0,
   1,
   0,
   /*LSP quantization*/
   lsp_quant_high,
   lsp_unquant_high,
   /*Pitch quantization*/
   NULL,
   NULL,
   NULL,
   /*No innovation quantization*/
   NULL,
   NULL,
   NULL,
   -1,
   36
};


static const SpeexSubmode wb_submode2 = {
   0,
   0,
   1,
   0,
   /*LSP quantization*/
   lsp_quant_high,
   lsp_unquant_high,
   /*Pitch quantization*/
   NULL,
   NULL,
   NULL,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
#ifdef DISABLE_WIDEBAND
   NULL,
#else
   &split_cb_high_lbr,
#endif
   -1,
   112
};


static const SpeexSubmode wb_submode3 = {
   0,
   0,
   1,
   0,
   /*LSP quantization*/
   lsp_quant_high,
   lsp_unquant_high,
   /*Pitch quantization*/
   NULL,
   NULL,
   NULL,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
#ifdef DISABLE_WIDEBAND
   NULL,
#else
   &split_cb_high,
#endif
   -1,
   192
};

static const SpeexSubmode wb_submode4 = {
   0,
   0,
   1,
   1,
   /*LSP quantization*/
   lsp_quant_high,
   lsp_unquant_high,
   /*Pitch quantization*/
   NULL,
   NULL,
   NULL,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
#ifdef DISABLE_WIDEBAND
   NULL,
#else
   &split_cb_high,
#endif
   -1,
   352
};


/* Split-band wideband CELP mode*/
static const SpeexSBMode sb_wb_mode = {
   &speex_nb_mode,
   160,    /*frameSize*/
   40,     /*subframeSize*/
   8,     /*lpcSize*/
   640,    /*bufSize*/
#ifdef FIXED_POINT
   29491, 19661, /* gamma1, gamma2 */
#else
   0.9, 0.6, /* gamma1, gamma2 */
#endif
   .012,   /*lag_factor*/
   QCONST16(.0002,15), /*lpc_floor*/
   0.9,
   {NULL, &wb_submode1, &wb_submode2, &wb_submode3, &wb_submode4, NULL, NULL, NULL},
   3,
   {1, 8, 2, 3, 4, 5, 5, 6, 6, 7, 7},
   {1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 4},
   vbr_hb_thresh,
   5
};


const SpeexMode speex_wb_mode = {
   &sb_wb_mode,
   wb_mode_query,
   "wideband (sub-band CELP)",
   1,
   4,
   &sb_encoder_init,
   &sb_encoder_destroy,
   &sb_encode,
   &sb_decoder_init,
   &sb_decoder_destroy,
   &sb_decode,
   &sb_encoder_ctl,
   &sb_decoder_ctl,
};



/* "Ultra-wideband" mode stuff */



/* Split-band "ultra-wideband" (32 kbps) CELP mode*/
static const SpeexSBMode sb_uwb_mode = {
   &speex_wb_mode,
   320,    /*frameSize*/
   80,     /*subframeSize*/
   8,     /*lpcSize*/
   1280,    /*bufSize*/
#ifdef FIXED_POINT
   29491, 19661, /* gamma1, gamma2 */
#else
   0.9, 0.6, /* gamma1, gamma2 */
#endif
   .012,   /*lag_factor*/
   QCONST16(.0002,15), /*lpc_floor*/
   0.7,
   {NULL, &wb_submode1, NULL, NULL, NULL, NULL, NULL, NULL},
   1,
   {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
   {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
   vbr_uhb_thresh,
   2
};


const SpeexMode speex_uwb_mode = {
   &sb_uwb_mode,
   wb_mode_query,
   "ultra-wideband (sub-band CELP)",
   2,
   4,
   &sb_encoder_init,
   &sb_encoder_destroy,
   &sb_encode,
   &sb_decoder_init,
   &sb_decoder_destroy,
   &sb_decode,
   &sb_encoder_ctl,
   &sb_decoder_ctl,
};




#ifdef EPIC_48K

extern const signed char gain_cdbk_ulbr[];
extern const signed char exc_12_32_table[];

/* Parameters for Long-Term Prediction (LTP)*/
static const ltp_params ltp_params_48k = {
   gain_cdbk_ulbr,
   3,
   0
};

static const split_cb_params split_cb_nb_48k = {
   12,               /*subvect_size*/
   4,               /*nb_subvect*/
   exc_12_32_table, /*shape_cb*/
   5,               /*shape_bits*/
   0,
};


/* 4.8 kbps very low bit-rate mode */
static const SpeexSubmode nb_48k_submode = {
   0,
   0,
   0,
   0,
   /*LSP quantization*/
   lsp_quant_48k,
   lsp_unquant_48k,
   /*No pitch quantization*/
   pitch_search_3tap,
   pitch_unquant_3tap,
   &ltp_params_48k,
   /*Innovation quantization*/
   split_cb_search_shape_sign,
   split_cb_shape_sign_unquant,
   &split_cb_nb_48k,
#ifdef FIXED_POINT
   22938, 16384, 11796, 18022,
#else
   0.7, 0.5, .36, .55,
#endif
   144
};


/* Special, non-standard 4.8 kbps mode */
static const SpeexNBMode nb_48k_mode = {
   240,    /*frameSize*/
   48,     /*subframeSize*/
   10,     /*lpcSize*/
   640,    /*bufSize*/
   17,     /*pitchStart*/
   144,    /*pitchEnd*/
   0.9,    /*gamma1*/
   0.6,    /*gamma2*/
   .01,   /*lag_factor*/
   QCONST16(.0003,15), /*lpc_floor*/
   1,
   {NULL, NULL, &nb_48k_submode, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
   2,
   {2,2,2,2,2,2,2,2,2,2,2}
};


/* Default mode for narrowband */
const SpeexMode speex_nb_48k_mode = {
   &nb_48k_mode,
   nb_mode_query,
   "narrowband 4.8 kbps",
   1000,
   4,
   &nb_encoder_init,
   &nb_encoder_destroy,
   &nb_encode,
   &nb_decoder_init,
   &nb_decoder_destroy,
   &nb_decode,
   &nb_encoder_ctl,
   &nb_decoder_ctl,
};


#endif

int speex_mode_query(const SpeexMode *mode, int request, void *ptr)
{
   return mode->query(mode->mode, request, ptr);
}

const SpeexMode * speex_lib_get_mode (int mode)
{
#ifdef EPIC_48K
  if (mode == SPEEX_MODEID_NB_48K) return &speex_nb_48k_mode;
#endif

  if (mode < 0 || mode > SPEEX_NB_MODES) return NULL;

  return speex_mode_list[mode];
}

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo.c - An echo cancellor, suitable for electrical and acoustic
 *          cancellation. This code does not currently comply with
 *          any relevant standards (e.g. G.164/5/7/8). One day....
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2003 Steve Underwood
 *
 * Based on a bit from here, a bit from there, eye of toad,
 * ear of bat, etc - plus, of course, my own 2 cents.
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: echo.c,v 1.20 2006/12/01 18:00:48 steveu Exp $
 */

/*! \file */

/* TODO:
   Finish the echo suppressor option, however nasty suppression may be.
   Add an option to reintroduce side tone at -24dB under appropriate conditions.
   Improve double talk detector (iterative!)
*/

/* We need to differentiate between transmitted energy which will train the echo
   canceller well (voice, white noise, and other broadband sources) and energy
   which will train it badly (supervisory tones, DTMF, whistles, and other
   narrowband sources). There are many ways this might be done. This canceller uses
   a method based on the autocorrelation qualities of the transmitted signal. A rather
   peaky autocorrelation function is a clear sign of a narrowband signal. We only need
   perform the autocorrelation at well spaced intervals, so the compute load is not too
   great. Multiple successive autocorrelation functions with a similar peaky shape are a
   clear indication of a stationary narrowband signal. Using TKEO, it should be possible to
   greatly reduce the compute requirement for narrowband detection. */

/* The FIR taps must be adapted as 32 bit values, to get the necessary finesse
   in the adaption process. However, they are applied as 16 bit values (bits 30-15
   of the 32 bit values) in the FIR. For the working 16 bit values, we need 4 sets.
   
   3 of the 16 bit sets are used on a rotating basis. Normally the canceller steps
   round these 3 sets at regular intervals. Any time we detect double talk, we can go
   back to the set from two steps ago with reasonable assurance it is a well adapted
   set. We cannot just go back one step, as we may have rotated the sets just before
   double talk or tone was detected, and that set may already be somewhat corrupted.
   
   When narrowband energy is detected we need to continue adapting to it, to echo
   cancel it. However, the adaption will almost certainly be going astray. Broadband
   (or even complex sequences of narrowband) energy will normally lead to a well
   trained cancellor, with taps matching the impulse response of the channel.
   For stationary narrowband energy, there is usually has an infinite number of
   alternative tap sets which will cancel it well. A previously well trained set of
   taps will tend to drift amongst the alternatives. When broadband energy resumes, the
   taps may be a total mismatch for the signal, and could even amplify rather than
   attenuate the echo. The solution is to use a fourth set of 16 bit taps. When we first
   detect the narrowband energy we save the oldest of the group of three sets, but do
   not change back to an older set. We let the canceller cancel, and it adaption drift
   while the narrowband energy is present. When we detect the narrowband energy has ceased,
   we switch to using the fourth set of taps which was saved.

   When we revert to an older set of taps, we must replace both the 16 bit and 32 bit
   working tap sets. The saved 16 bit values are good enough to also be used as a replacement
   for the 32 bit values. We loose the fractions, but they should soon settle down in a
   reasonable way. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "celliax_spandsp.h"

//#include "spandsp/telephony.h"
//#include "spandsp/logging.h"
//#include "spandsp/bit_operations.h"
//#include "spandsp/echo.h"

//#include "bit_operations.h"
//#include "giova.h"

#if !defined(NULL)
#define NULL (void *) 0
#endif
#if !defined(FALSE)
#define FALSE 0
#endif
#if !defined(TRUE)
#define TRUE (!FALSE)
#endif

#if 0
#define MIN_TX_POWER_FOR_ADAPTION   64*64
#define MIN_RX_POWER_FOR_ADAPTION   64*64

static int narrowband_detect(echo_can_state_t * ec)
{
  int k;
  int i;
  float temp;
  float scale;
  float sf[128];
  float f_acf[128];
  int32_t acf[28];
  int score;
  int len = 32;
  int alen = 9;

  k = ec->curr_pos;
  for (i = 0; i < len; i++) {
    sf[i] = ec->fir_state.history[k++];
    if (k >= 256)
      k = 0;
  }
  for (k = 0; k < alen; k++) {
    temp = 0;
    for (i = k; i < len; i++)
      temp += sf[i] * sf[i - k];
    f_acf[k] = temp;
  }
  scale = 0x1FFFFFFF / f_acf[0];
  for (k = 0; k < alen; k++)
    acf[k] = (int32_t) (f_acf[k] * scale);
  score = 0;
  for (i = 0; i < 9; i++) {
    if (ec->last_acf[i] >= 0 && acf[i] >= 0) {
      if ((ec->last_acf[i] >> 1) < acf[i] && acf[i] < (ec->last_acf[i] << 1))
        score++;
    } else if (ec->last_acf[i] < 0 && acf[i] < 0) {
      if ((ec->last_acf[i] >> 1) > acf[i] && acf[i] > (ec->last_acf[i] << 1))
        score++;
    }
  }
  memcpy(ec->last_acf, acf, alen * sizeof(ec->last_acf[0]));
  return score;
}

static __inline__ void lms_adapt(echo_can_state_t * ec, int factor)
{
  int i;

#if 0
  mmx_t *mmx_taps;
  mmx_t *mmx_coeffs;
  mmx_t *mmx_hist;
  mmx_t mmx;

  mmx.w[0] = mmx.w[1] = mmx.w[2] = mmx.w[3] = factor;
  mmx_hist = (mmx_t *) & fir->history[fir->curr_pos];
  mmx_taps = (mmx_t *) & fir->taps;
  mmx_coeffs = (mmx_t *) fir->coeffs;
  i = fir->taps;
  movq_m2r(mmx, mm0);
  while (i > 0) {
    movq_m2r(mmx_hist[0], mm1);
    movq_m2r(mmx_taps[0], mm0);
    movq_m2r(mmx_taps[1], mm1);
    movq_r2r(mm1, mm2);
    pmulhw(mm0, mm1);
    pmullw(mm0, mm2);

    pmaddwd_r2r(mm1, mm0);
    pmaddwd_r2r(mm3, mm2);
    paddd_r2r(mm0, mm4);
    paddd_r2r(mm2, mm4);
    movq_r2m(mm0, mmx_taps[0]);
    movq_r2m(mm1, mmx_taps[0]);
    movq_r2m(mm2, mmx_coeffs[0]);
    mmx_taps += 2;
    mmx_coeffs += 1;
    mmx_hist += 1;
    i -= 4;
    )
      emms();
#elif 0
  /* Update the FIR taps */
  for (i = ec->taps - 1; i >= 0; i--) {
    /* Leak to avoid the coefficients drifting beyond the ability of the
       adaption process to bring them back under control. */
    ec->fir_taps32[i] -= (ec->fir_taps32[i] >> 23);
    ec->fir_taps32[i] += (ec->fir_state.history[i + ec->curr_pos] * factor);
    ec->latest_correction = (ec->fir_state.history[i + ec->curr_pos] * factor);
    ec->fir_taps16[ec->tap_set][i] = ec->fir_taps32[i] >> 15;
  }
#else
  int offset1;
  int offset2;

  /* Update the FIR taps */
  offset2 = ec->curr_pos;
  offset1 = ec->taps - offset2;
  for (i = ec->taps - 1; i >= offset1; i--) {
    ec->fir_taps32[i] += (ec->fir_state.history[i - offset1] * factor);
    ec->fir_taps16[ec->tap_set][i] = (int16_t) (ec->fir_taps32[i] >> 15);
  }
  for (; i >= 0; i--) {
    ec->fir_taps32[i] += (ec->fir_state.history[i + offset2] * factor);
    ec->fir_taps16[ec->tap_set][i] = (int16_t) (ec->fir_taps32[i] >> 15);
  }
#endif
}

/*- End of function --------------------------------------------------------*/

#ifdef NOT_NEEDED
echo_can_state_t *echo_can_create(int len, int adaption_mode)
{
  echo_can_state_t *ec;
  int i;
  int j;

  ec = (echo_can_state_t *) malloc(sizeof(*ec));
  if (ec == NULL)
    return NULL;
  memset(ec, 0, sizeof(*ec));
  ec->taps = len;
  ec->curr_pos = ec->taps - 1;
  ec->tap_mask = ec->taps - 1;
  if ((ec->fir_taps32 = (int32_t *) malloc(ec->taps * sizeof(int32_t))) == NULL) {
    free(ec);
    return NULL;
  }
  memset(ec->fir_taps32, 0, ec->taps * sizeof(int32_t));
  for (i = 0; i < 4; i++) {
    if ((ec->fir_taps16[i] = (int16_t *) malloc(ec->taps * sizeof(int16_t))) == NULL) {
      for (j = 0; j < i; j++)
        free(ec->fir_taps16[j]);
      free(ec->fir_taps32);
      free(ec);
      return NULL;
    }
    memset(ec->fir_taps16[i], 0, ec->taps * sizeof(int16_t));
  }
  fir16_create(&ec->fir_state, ec->fir_taps16[0], ec->taps);
  ec->rx_power_threshold = 10000000;
  ec->geigel_max = 0;
  ec->geigel_lag = 0;
  ec->dtd_onset = FALSE;
  ec->tap_set = 0;
  ec->tap_rotate_counter = 1600;
  ec->cng_level = 1000;
  echo_can_adaption_mode(ec, adaption_mode);
  return ec;
}

/*- End of function --------------------------------------------------------*/

void echo_can_free(echo_can_state_t * ec)
{
  int i;

  fir16_free(&ec->fir_state);
  free(ec->fir_taps32);
  for (i = 0; i < 4; i++)
    free(ec->fir_taps16[i]);
  free(ec);
}

/*- End of function --------------------------------------------------------*/

void echo_can_adaption_mode(echo_can_state_t * ec, int adaption_mode)
{
  ec->adaption_mode = adaption_mode;
}

/*- End of function --------------------------------------------------------*/

void echo_can_flush(echo_can_state_t * ec)
{
  int i;

  for (i = 0; i < 4; i++)
    ec->tx_power[i] = 0;
  for (i = 0; i < 3; i++)
    ec->rx_power[i] = 0;
  ec->clean_rx_power = 0;
  ec->nonupdate_dwell = 0;

  fir16_flush(&ec->fir_state);
  ec->fir_state.curr_pos = ec->taps - 1;
  memset(ec->fir_taps32, 0, ec->taps * sizeof(int32_t));
  for (i = 0; i < 4; i++)
    memset(ec->fir_taps16[i], 0, ec->taps * sizeof(int16_t));

  ec->curr_pos = ec->taps - 1;

  ec->supp_test1 = 0;
  ec->supp_test2 = 0;
  ec->supp1 = 0;
  ec->supp2 = 0;
  ec->vad = 0;
  ec->cng_level = 1000;
  ec->cng_filter = 0;

  ec->geigel_max = 0;
  ec->geigel_lag = 0;
  ec->dtd_onset = FALSE;
  ec->tap_set = 0;
  ec->tap_rotate_counter = 1600;

  ec->latest_correction = 0;

  memset(ec->last_acf, 0, sizeof(ec->last_acf));
  ec->narrowband_count = 0;
  ec->narrowband_score = 0;
}

/*- End of function --------------------------------------------------------*/

int sample_no = 0;

int16_t echo_can_update(echo_can_state_t * ec, int16_t tx, int16_t rx)
{
  int32_t echo_value;
  int clean_rx;
  int nsuppr;
  int score;
  int i;

  sample_no++;
  ec->latest_correction = 0;
  /* Evaluate the echo - i.e. apply the FIR filter */
  /* Assume the gain of the FIR does not exceed unity. Exceeding unity
     would seem like a rather poor thing for an echo cancellor to do :)
     This means we can compute the result with a total disregard for
     overflows. 16bits x 16bits -> 31bits, so no overflow can occur in
     any multiply. While accumulating we may overflow and underflow the
     32 bit scale often. However, if the gain does not exceed unity,
     everything should work itself out, and the final result will be
     OK, without any saturation logic. */
  /* Overflow is very much possible here, and we do nothing about it because
     of the compute costs */
  /* 16 bit coeffs for the LMS give lousy results (maths good, actual sound
     bad!), but 32 bit coeffs require some shifting. On balance 32 bit seems
     best */
  echo_value = fir16(&ec->fir_state, tx);

  /* And the answer is..... */
  clean_rx = rx - echo_value;
//printf("echo is %" PRId32 "\n", echo_value);
  /* That was the easy part. Now we need to adapt! */
  if (ec->nonupdate_dwell > 0)
    ec->nonupdate_dwell--;

  /* Calculate short term power levels using very simple single pole IIRs */
  /* TODO: Is the nasty modulus approach the fastest, or would a real
     tx*tx power calculation actually be faster? Using the squares
     makes the numbers grow a lot! */
  ec->tx_power[3] += ((abs(tx) - ec->tx_power[3]) >> 5);
  ec->tx_power[2] += ((tx * tx - ec->tx_power[2]) >> 8);
  ec->tx_power[1] += ((tx * tx - ec->tx_power[1]) >> 5);
  ec->tx_power[0] += ((tx * tx - ec->tx_power[0]) >> 3);
  ec->rx_power[1] += ((rx * rx - ec->rx_power[1]) >> 6);
  ec->rx_power[0] += ((rx * rx - ec->rx_power[0]) >> 3);
  ec->clean_rx_power += ((clean_rx * clean_rx - ec->clean_rx_power) >> 6);

  score = 0;
  /* If there is very little being transmitted, any attempt to train is
     futile. We would either be training on the far end's noise or signal,
     the channel's own noise, or our noise. Either way, this is hardly good
     training, so don't do it (avoid trouble). */
  if (ec->tx_power[0] > MIN_TX_POWER_FOR_ADAPTION) {
    /* If the received power is very low, either we are sending very little or
       we are already well adapted. There is little point in trying to improve
       the adaption under these circumstances, so don't do it (reduce the
       compute load). */
    if (ec->tx_power[1] > ec->rx_power[0]) {
      /* There is no (or little) far-end speech. */
      if (ec->nonupdate_dwell == 0) {
        if (++ec->narrowband_count >= 160) {
          ec->narrowband_count = 0;
          score = narrowband_detect(ec);
//printf("Do the narrowband test %d at %d\n", score, ec->curr_pos);
          if (score > 6) {
            if (ec->narrowband_score == 0)
              memcpy(ec->fir_taps16[3], ec->fir_taps16[(ec->tap_set + 1) % 3],
                     ec->taps * sizeof(int16_t));
            ec->narrowband_score += score;
          } else {
            if (ec->narrowband_score > 200) {
//printf("Revert to %d at %d\n", (ec->tap_set + 1)%3, sample_no);
              memcpy(ec->fir_taps16[ec->tap_set], ec->fir_taps16[3],
                     ec->taps * sizeof(int16_t));
              memcpy(ec->fir_taps16[(ec->tap_set - 1) % 3], ec->fir_taps16[3],
                     ec->taps * sizeof(int16_t));
              for (i = 0; i < ec->taps; i++)
                ec->fir_taps32[i] = ec->fir_taps16[3][i] << 15;
              ec->tap_rotate_counter = 1600;
            }
            ec->narrowband_score = 0;
          }
        }
        ec->dtd_onset = FALSE;
        if (--ec->tap_rotate_counter <= 0) {
//printf("Rotate to %d at %d\n", ec->tap_set, sample_no);
          ec->tap_rotate_counter = 1600;
          ec->tap_set++;
          if (ec->tap_set > 2)
            ec->tap_set = 0;
          ec->fir_state.coeffs = ec->fir_taps16[ec->tap_set];
        }
        /* ... and we are not in the dwell time from previous speech. */
        if ((ec->adaption_mode & ECHO_CAN_USE_ADAPTION) && ec->narrowband_score == 0) {
          //nsuppr = saturate((clean_rx << 16)/ec->tx_power[1]);
          //nsuppr = clean_rx/ec->tx_power[1];
          /* If a sudden surge in signal level (e.g. the onset of a tone
             burst) cause an abnormally high instantaneous to average
             signal power ratio, we could kick the adaption badly in the
             wrong direction. This is because the tx_power takes too long
             to react and rise. We need to stop too rapid adaption to the
             new signal. We normalise to a value derived from the
             instantaneous signal if it exceeds the peak by too much. */
          nsuppr = clean_rx;
          /* Divide isn't very quick, but the "where is the top bit" and shift
             instructions are single cycle. */
          if (tx > 4 * ec->tx_power[3])
            i = top_bit(tx) - 8;
          else
            i = top_bit(ec->tx_power[3]) - 8;
          if (i > 0)
            nsuppr >>= i;
          lms_adapt(ec, nsuppr);
        }
      }
      //printf("%10d %10d %10d %10d %10d\n", rx, clean_rx, nsuppr, ec->tx_power[1], ec->rx_power[1]);
      //printf("%.4f\n", (float) ec->rx_power[1]/(float) ec->clean_rx_power);
    } else {
      if (!ec->dtd_onset) {
//printf("Revert to %d at %d\n", (ec->tap_set + 1)%3, sample_no);
        memcpy(ec->fir_taps16[ec->tap_set], ec->fir_taps16[(ec->tap_set + 1) % 3],
               ec->taps * sizeof(int16_t));
        memcpy(ec->fir_taps16[(ec->tap_set - 1) % 3],
               ec->fir_taps16[(ec->tap_set + 1) % 3], ec->taps * sizeof(int16_t));
        for (i = 0; i < ec->taps; i++)
          ec->fir_taps32[i] = ec->fir_taps16[(ec->tap_set + 1) % 3][i] << 15;
        ec->tap_rotate_counter = 1600;
        ec->dtd_onset = TRUE;
      }
      ec->nonupdate_dwell = NONUPDATE_DWELL_TIME;
    }
  }

  if (ec->rx_power[1])
    ec->vad = (8000 * ec->clean_rx_power) / ec->rx_power[1];
  else
    ec->vad = 0;
  if (ec->rx_power[1] > 2048 * 2048 && ec->clean_rx_power > 4 * ec->rx_power[1]) {
    /* The EC seems to be making things worse, instead of better. Zap it! */
    memset(ec->fir_taps32, 0, ec->taps * sizeof(int32_t));
    for (i = 0; i < 4; i++)
      memset(ec->fir_taps16[i], 0, ec->taps * sizeof(int16_t));
  }
#if defined(XYZZY)
  if ((ec->adaption_mode & ECHO_CAN_USE_SUPPRESSOR)) {
    ec->supp_test1 +=
      (ec->fir_state.history[ec->curr_pos] -
       ec->fir_state.history[(ec->curr_pos - 7) & ec->tap_mask]);
    ec->supp_test2 +=
      (ec->fir_state.history[(ec->curr_pos - 24) & ec->tap_mask] -
       ec->fir_state.history[(ec->curr_pos - 31) & ec->tap_mask]);
    if (ec->supp_test1 > 42 && ec->supp_test2 > 42)
      supp_change = 25;
    else
      supp_change = 50;
    supp = supp_change + k1 * ec->supp1 + k2 * ec->supp2;
    ec->supp2 = ec->supp1;
    ec->supp1 = supp;
    clean_rx *= (1 - supp);
  }
#endif

  if ((ec->adaption_mode & ECHO_CAN_USE_NLP)) {
    /* Non-linear processor - a fancy way to say "zap small signals, to avoid
       residual echo due to (uLaw/ALaw) non-linearity in the channel.". */
    if (ec->rx_power[1] < 30000000) {
      if (!ec->cng) {
        ec->cng_level = ec->clean_rx_power;
        ec->cng = TRUE;
      }
      if ((ec->adaption_mode & ECHO_CAN_USE_CNG)) {
        /* Very elementary comfort noise generation */
        /* Just random numbers rolled off very vaguely Hoth-like */
        ec->cng_rndnum = 1664525U * ec->cng_rndnum + 1013904223U;
        ec->cng_filter = ((ec->cng_rndnum & 0xFFFF) - 32768 + 5 * ec->cng_filter) >> 3;
        clean_rx = (ec->cng_filter * ec->cng_level) >> 17;
        /* TODO: A better CNG, with more accurate (tracking) spectral shaping! */
      } else {
        clean_rx = 0;
      }
//clean_rx = -16000;
    } else {
      ec->cng = FALSE;
    }
  } else {
    ec->cng = FALSE;
  }

//printf("Narrowband score %4d %5d at %d\n", ec->narrowband_score, score, sample_no);
  /* Roll around the rolling buffer */
  if (ec->curr_pos <= 0)
    ec->curr_pos = ec->taps;
  ec->curr_pos--;
  return (int16_t) clean_rx;
}

#endif //NOT_NEEDED
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>

//#include "spandsp/telephony.h"
//#include "spandsp/tone_detect.h"
//#include "spandsp/tone_generate.h"
//#include "spandsp/super_tone_rx.h"
//#include "giova.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

//#define USE_3DNOW

#define DEFAULT_DTMF_TX_LEVEL       -10
#define DEFAULT_DTMF_TX_ON_TIME     50
#define DEFAULT_DTMF_TX_OFF_TIME    55

#define DTMF_THRESHOLD              8.0e7f
#define DTMF_NORMAL_TWIST           6.3f    /* 8dB */
#define DTMF_REVERSE_TWIST          2.5f    /* 4dB */
#define DTMF_RELATIVE_PEAK_ROW      6.3f    /* 8dB */
#define DTMF_RELATIVE_PEAK_COL      6.3f    /* 8dB */
#define DTMF_TO_TOTAL_ENERGY        42.0f

static const float dtmf_row[] = {
  697.0f, 770.0f, 852.0f, 941.0f
};
static const float dtmf_col[] = {
  1209.0f, 1336.0f, 1477.0f, 1633.0f
};

static const char dtmf_positions[] = "123A" "456B" "789C" "*0#D";

static goertzel_descriptor_t dtmf_detect_row[4];
static goertzel_descriptor_t dtmf_detect_col[4];

//
//static int dtmf_tx_inited = 0;
//static tone_gen_descriptor_t dtmf_digit_tones[16];

#if defined(USE_3DNOW)
static __inline__ void _dtmf_goertzel_update(goertzel_state_t * s, float x[], int samples)
{
  int n;
  float v;
  int i;
  float vv[16];

  vv[4] = s[0].v2;
  vv[5] = s[1].v2;
  vv[6] = s[2].v2;
  vv[7] = s[3].v2;
  vv[8] = s[0].v3;
  vv[9] = s[1].v3;
  vv[10] = s[2].v3;
  vv[11] = s[3].v3;
  vv[12] = s[0].fac;
  vv[13] = s[1].fac;
  vv[14] = s[2].fac;
  vv[15] = s[3].fac;

  //v1 = s->v2;
  //s->v2 = s->v3;
  //s->v3 = s->fac*s->v2 - v1 + x[0];

  __asm__ __volatile__(" femms;\n" " movq        16(%%edx),%%mm2;\n"
                       " movq        24(%%edx),%%mm3;\n" " movq        32(%%edx),%%mm4;\n"
                       " movq        40(%%edx),%%mm5;\n" " movq        48(%%edx),%%mm6;\n"
                       " movq        56(%%edx),%%mm7;\n" " jmp         1f;\n"
                       " .align 32;\n" " 1: ;\n" " prefetch    (%%eax);\n"
                       " movq        %%mm3,%%mm1;\n" " movq        %%mm2,%%mm0;\n"
                       " movq        %%mm5,%%mm3;\n" " movq        %%mm4,%%mm2;\n"
                       " pfmul       %%mm7,%%mm5;\n" " pfmul       %%mm6,%%mm4;\n"
                       " pfsub       %%mm1,%%mm5;\n" " pfsub       %%mm0,%%mm4;\n"
                       " movq        (%%eax),%%mm0;\n" " movq        %%mm0,%%mm1;\n"
                       " punpckldq   %%mm0,%%mm1;\n" " add         $4,%%eax;\n"
                       " pfadd       %%mm1,%%mm5;\n" " pfadd       %%mm1,%%mm4;\n"
                       " dec         %%ecx;\n" " jnz         1b;\n"
                       " movq        %%mm2,16(%%edx);\n" " movq        %%mm3,24(%%edx);\n"
                       " movq        %%mm4,32(%%edx);\n" " movq        %%mm5,40(%%edx);\n"
                       " femms;\n"::"c"(samples), "a"(x), "d"(vv)
                       :"memory", "eax", "ecx");

  s[0].v2 = vv[4];
  s[1].v2 = vv[5];
  s[2].v2 = vv[6];
  s[3].v2 = vv[7];
  s[0].v3 = vv[8];
  s[1].v3 = vv[9];
  s[2].v3 = vv[10];
  s[3].v3 = vv[11];
}

/*- End of function --------------------------------------------------------*/
#endif

int dtmf_rx(dtmf_rx_state_t * s, const int16_t amp[], int samples)
{
  float row_energy[4];
  float col_energy[4];
  float famp;
  float v1;
  int i;
  int j;
  int sample;
  int best_row;
  int best_col;
  int limit;
  uint8_t hit;

  hit = 0;
  for (sample = 0; sample < samples; sample = limit) {
    /* The block length is optimised to meet the DTMF specs. */
    if ((samples - sample) >= (102 - s->current_sample))
      limit = sample + (102 - s->current_sample);
    else
      limit = samples;
#if defined(USE_3DNOW)
    _dtmf_goertzel_update(s->row_out, amp + sample, limit - sample);
    _dtmf_goertzel_update(s->col_out, amp + sample, limit - sample);
#else
    /* The following unrolled loop takes only 35% (rough estimate) of the 
       time of a rolled loop on the machine on which it was developed */
    for (j = sample; j < limit; j++) {
      famp = amp[j];
      if (s->filter_dialtone) {
        /* Sharp notches applied at 350Hz and 440Hz - the two common dialtone frequencies.
           These are rather high Q, to achieve the required narrowness, without using lots of
           sections. */
        v1 = 0.98356f * famp + 1.8954426f * s->z350_1 - 0.9691396f * s->z350_2;
        famp = v1 - 1.9251480f * s->z350_1 + s->z350_2;
        s->z350_2 = s->z350_1;
        s->z350_1 = v1;

        v1 = 0.98456f * famp + 1.8529543f * s->z440_1 - 0.9691396f * s->z440_2;
        famp = v1 - 1.8819938f * s->z440_1 + s->z440_2;
        s->z440_2 = s->z440_1;
        s->z440_1 = v1;
      }
      s->energy += famp * famp;
      /* With GCC 2.95, the following unrolled code seems to take about 35%
         (rough estimate) as long as a neat little 0-3 loop */
      v1 = s->row_out[0].v2;
      s->row_out[0].v2 = s->row_out[0].v3;
      s->row_out[0].v3 = s->row_out[0].fac * s->row_out[0].v2 - v1 + famp;

      v1 = s->col_out[0].v2;
      s->col_out[0].v2 = s->col_out[0].v3;
      s->col_out[0].v3 = s->col_out[0].fac * s->col_out[0].v2 - v1 + famp;

      v1 = s->row_out[1].v2;
      s->row_out[1].v2 = s->row_out[1].v3;
      s->row_out[1].v3 = s->row_out[1].fac * s->row_out[1].v2 - v1 + famp;

      v1 = s->col_out[1].v2;
      s->col_out[1].v2 = s->col_out[1].v3;
      s->col_out[1].v3 = s->col_out[1].fac * s->col_out[1].v2 - v1 + famp;

      v1 = s->row_out[2].v2;
      s->row_out[2].v2 = s->row_out[2].v3;
      s->row_out[2].v3 = s->row_out[2].fac * s->row_out[2].v2 - v1 + famp;

      v1 = s->col_out[2].v2;
      s->col_out[2].v2 = s->col_out[2].v3;
      s->col_out[2].v3 = s->col_out[2].fac * s->col_out[2].v2 - v1 + famp;

      v1 = s->row_out[3].v2;
      s->row_out[3].v2 = s->row_out[3].v3;
      s->row_out[3].v3 = s->row_out[3].fac * s->row_out[3].v2 - v1 + famp;

      v1 = s->col_out[3].v2;
      s->col_out[3].v2 = s->col_out[3].v3;
      s->col_out[3].v3 = s->col_out[3].fac * s->col_out[3].v2 - v1 + famp;
    }
#endif
    s->current_sample += (limit - sample);
    if (s->current_sample < 102)
      continue;

    /* We are at the end of a DTMF detection block */
    /* Find the peak row and the peak column */
    row_energy[0] = goertzel_result(&s->row_out[0]);
    best_row = 0;
    col_energy[0] = goertzel_result(&s->col_out[0]);
    best_col = 0;

    for (i = 1; i < 4; i++) {
      row_energy[i] = goertzel_result(&s->row_out[i]);
      if (row_energy[i] > row_energy[best_row])
        best_row = i;
      col_energy[i] = goertzel_result(&s->col_out[i]);
      if (col_energy[i] > col_energy[best_col])
        best_col = i;
    }
    hit = 0;
    /* Basic signal level test and the twist test */
    if (row_energy[best_row] >= DTMF_THRESHOLD && col_energy[best_col] >= DTMF_THRESHOLD
        && col_energy[best_col] < row_energy[best_row] * s->reverse_twist
        && col_energy[best_col] * s->normal_twist > row_energy[best_row]) {
      /* Relative peak test ... */
      for (i = 0; i < 4; i++) {
        if ((i != best_col
             && col_energy[i] * DTMF_RELATIVE_PEAK_COL > col_energy[best_col])
            || (i != best_row
                && row_energy[i] * DTMF_RELATIVE_PEAK_ROW > row_energy[best_row])) {
          break;
        }
      }
      /* ... and fraction of total energy test */
      if (i >= 4
          && (row_energy[best_row] + col_energy[best_col]) >
          DTMF_TO_TOTAL_ENERGY * s->energy) {
        hit = dtmf_positions[(best_row << 2) + best_col];
      }
    }
    /* The logic in the next test should ensure the following for different successive hit patterns:
       -----ABB = start of digit B.
       ----B-BB = start of digit B
       ----A-BB = start of digit B
       BBBBBABB = still in digit B.
       BBBBBB-- = end of digit B
       BBBBBBC- = end of digit B
       BBBBACBB = B ends, then B starts again.
       BBBBBBCC = B ends, then C starts.
       BBBBBCDD = B ends, then D starts.
       This can work with:
       - Back to back differing digits. Back-to-back digits should
       not happen. The spec. says there should be a gap between digits.
       However, many real phones do not impose a gap, and rolling across
       the keypad can produce little or no gap.
       - It tolerates nasty phones that give a very wobbly start to a digit.
       - VoIP can give sample slips. The phase jumps that produces will cause
       the block it is in to give no detection. This logic will ride over a
       single missed block, and not falsely declare a second digit. If the
       hiccup happens in the wrong place on a minimum length digit, however
       we would still fail to detect that digit. Could anything be done to
       deal with that? Packet loss is clearly a no-go zone.
       Note this is only relevant to VoIP using A-law, u-law or similar.
       Low bit rate codecs scramble DTMF too much for it to be recognised,
       and often slip in units larger than a sample. */
    if (hit != s->in_digit) {
      if (s->last_hit != s->in_digit) {
        /* We have two successive indications that something has changed. */
        /* To declare digit on, the hits must agree. Otherwise we declare tone off. */
        hit = (hit && hit == s->last_hit) ? hit : 0;
#if 0
        if (s->realtime_callback) {
          /* Avoid reporting multiple no digit conditions on flaky hits */
          if (s->in_digit || hit) {
            i = (s->in_digit
                 && !hit) ? -99 : rint(log10f(s->energy) * 10.0f - 20.08f - 90.30F +
                                       DBM0_MAX_POWER);
            s->realtime_callback(s->realtime_callback_data, hit, i);
          }
        } else {
#endif
          if (hit) {
            if (s->current_digits < MAX_DTMF_DIGITS) {
              s->digits[s->current_digits++] = (char) hit;
              s->digits[s->current_digits] = '\0';
              if (s->callback) {
                s->callback(s->callback_data, s->digits, s->current_digits);
                s->current_digits = 0;
              }
            } else {
              s->lost_digits++;
            }
          }
#if 0
        }
#endif
        s->in_digit = hit;
      }
    }
    s->last_hit = hit;
    /* Reinitialise the detector for the next block */
    for (i = 0; i < 4; i++) {
      goertzel_reset(&s->row_out[i]);
      goertzel_reset(&s->col_out[i]);
    }
    s->energy = 0.0f;
    s->current_sample = 0;
  }
  if (s->current_digits && s->callback) {
    s->callback(s->callback_data, s->digits, s->current_digits);
    s->digits[0] = '\0';
    s->current_digits = 0;
  }
  return 0;
}

/*- End of function --------------------------------------------------------*/

size_t dtmf_rx_get(dtmf_rx_state_t * s, char *buf, int max)
{
  if (max > s->current_digits)
    max = s->current_digits;
  if (max > 0) {
    memcpy(buf, s->digits, max);
    memmove(s->digits, s->digits + max, s->current_digits - max);
    s->current_digits -= max;
  }
  buf[max] = '\0';
  return max;
}

/*- End of function --------------------------------------------------------*/

#if 0
void dtmf_rx_set_realtime_callback(dtmf_rx_state_t * s, tone_report_func_t callback,
                                   void *user_data)
{
  s->realtime_callback = callback;
  s->realtime_callback_data = user_data;
}
#endif
/*- End of function --------------------------------------------------------*/

void dtmf_rx_parms(dtmf_rx_state_t * s, int filter_dialtone, int twist, int reverse_twist)
{
  if (filter_dialtone >= 0) {
    s->z350_1 = 0.0f;
    s->z350_2 = 0.0f;
    s->z440_1 = 0.0f;
    s->z440_2 = 0.0f;
    s->filter_dialtone = filter_dialtone;
  }
  if (twist >= 0)
    s->normal_twist = powf(10.0f, twist / 10.0f);
  if (reverse_twist >= 0)
    s->reverse_twist = powf(10.0f, reverse_twist / 10.0f);
}

/*- End of function --------------------------------------------------------*/

dtmf_rx_state_t *dtmf_rx_init(dtmf_rx_state_t * s, dtmf_rx_callback_t callback,
                              void *user_data)
{
  int i;
  static int initialised = 0;

  s->callback = callback;
  s->callback_data = user_data;
  s->realtime_callback = NULL;
  s->realtime_callback_data = NULL;
  s->filter_dialtone = 0;
  s->normal_twist = DTMF_NORMAL_TWIST;
  s->reverse_twist = DTMF_REVERSE_TWIST;

  s->in_digit = 0;
  s->last_hit = 0;

  if (!initialised) {
    for (i = 0; i < 4; i++) {
      make_goertzel_descriptor(&dtmf_detect_row[i], dtmf_row[i], 102);
      make_goertzel_descriptor(&dtmf_detect_col[i], dtmf_col[i], 102);
    }
    initialised = 1;
  }
  for (i = 0; i < 4; i++) {
    goertzel_init(&s->row_out[i], &dtmf_detect_row[i]);
    goertzel_init(&s->col_out[i], &dtmf_detect_col[i]);
  }
  s->energy = 0.0f;
  s->current_sample = 0;
  s->lost_digits = 0;
  s->current_digits = 0;
  s->digits[0] = '\0';
  return s;
}

/*- End of function --------------------------------------------------------*/

#if 0
static void dtmf_tx_initialise(void)
{
  int row;
  int col;

  if (dtmf_tx_inited)
    return;
  for (row = 0; row < 4; row++) {
    for (col = 0; col < 4; col++) {
      make_tone_gen_descriptor(&dtmf_digit_tones[row * 4 + col], (int) dtmf_row[row],
                               DEFAULT_DTMF_TX_LEVEL, (int) dtmf_col[col],
                               DEFAULT_DTMF_TX_LEVEL, DEFAULT_DTMF_TX_ON_TIME,
                               DEFAULT_DTMF_TX_OFF_TIME, 0, 0, FALSE);
    }
  }
  dtmf_tx_inited = TRUE;
}

/*- End of function --------------------------------------------------------*/

int dtmf_tx(dtmf_tx_state_t * s, int16_t amp[], int max_samples)
{
  int len;
  size_t dig;
  char *cp;

  len = 0;
  if (s->tones.current_section >= 0) {
    /* Deal with the fragment left over from last time */
    len = tone_gen(&(s->tones), amp, max_samples);
  }
  dig = 0;
  while (dig < s->current_digits && len < max_samples) {
    /* Step to the next digit */
    if ((cp = strchr(dtmf_positions, s->digits[dig++])) == NULL)
      continue;
    tone_gen_init(&(s->tones), &(s->tone_descriptors[cp - dtmf_positions]));
    len += tone_gen(&(s->tones), amp + len, max_samples - len);
  }
  if (dig) {
    /* Shift out the consumed digits */
    s->current_digits -= dig;
    memmove(s->digits, s->digits + dig, s->current_digits);
  }
  return len;
}

/*- End of function --------------------------------------------------------*/

size_t dtmf_tx_put(dtmf_tx_state_t * s, const char *digits)
{
  size_t len;

  /* This returns the number of characters that would not fit in the buffer.
     The buffer will only be loaded if the whole string of digits will fit,
     in which case zero is returned. */
  if ((len = strlen(digits)) > 0) {
    if (s->current_digits + len <= MAX_DTMF_DIGITS) {
      memcpy(s->digits + s->current_digits, digits, len);
      s->current_digits += len;
      len = 0;
    } else {
      len = MAX_DTMF_DIGITS - s->current_digits;
    }
  }
  return len;
}

/*- End of function --------------------------------------------------------*/

dtmf_tx_state_t *dtmf_tx_init(dtmf_tx_state_t * s)
{
  if (!dtmf_tx_inited)
    dtmf_tx_initialise();
  s->tone_descriptors = dtmf_digit_tones;
  tone_gen_init(&(s->tones), &dtmf_digit_tones[0]);
  s->current_sample = 0;
  s->current_digits = 0;
  s->tones.current_section = -1;
  return s;
}
#endif //NO TX
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

void make_goertzel_descriptor(goertzel_descriptor_t * t, float freq, int samples)
{
  //t->fac = 2.0f*cosf(2.0f*M_PI*(freq/(float) SAMPLE_RATE));
  t->fac = 2.0f * cosf(2.0f * M_PI * (freq / (float) 8000));
  t->samples = samples;
}

/*- End of function --------------------------------------------------------*/

goertzel_state_t *goertzel_init(goertzel_state_t * s, goertzel_descriptor_t * t)
{
  if (s || (s = malloc(sizeof(goertzel_state_t)))) {
    s->v2 = s->v3 = 0.0;
    s->fac = t->fac;
    s->samples = t->samples;
    s->current_sample = 0;
  }
  return s;
}

/*- End of function --------------------------------------------------------*/

void goertzel_reset(goertzel_state_t * s)
{
  s->v2 = s->v3 = 0.0;
  s->current_sample = 0;
}

/*- End of function --------------------------------------------------------*/

int goertzel_update(goertzel_state_t * s, const int16_t amp[], int samples)
{
  int i;
  float v1;

  if (samples > s->samples - s->current_sample)
    samples = s->samples - s->current_sample;
  for (i = 0; i < samples; i++) {
    v1 = s->v2;
    s->v2 = s->v3;
    s->v3 = s->fac * s->v2 - v1 + amp[i];
  }
  s->current_sample += samples;
  return samples;
}

/*- End of function --------------------------------------------------------*/

float goertzel_result(goertzel_state_t * s)
{
  float v1;

  /* Push a zero through the process to finish things off. */
  v1 = s->v2;
  s->v2 = s->v3;
  s->v3 = s->fac * s->v2 - v1;
  /* Now calculate the non-recursive side of the filter. */
  /* The result here is not scaled down to allow for the magnification
     effect of the filter (the usual DFT magnification effect). */
  return s->v3 * s->v3 + s->v2 * s->v2 - s->v2 * s->v3 * s->fac;
}

/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

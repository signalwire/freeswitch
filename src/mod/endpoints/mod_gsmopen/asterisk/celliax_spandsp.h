
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bit_operations.h - Various bit level operations, such as bit reversal
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: bit_operations.h,v 1.15 2007/02/23 13:16:13 steveu Exp $
 */

/*! \file */

#ifndef _CELLIAX_SPANDSP_H
#define _CELLIAX_SPANDSP_H

#include <math.h>

/*! \brief Find the bit position of the highest set bit in a word
    \param bits The word to be searched
    \return The bit number of the highest set bit, or -1 if the word is zero. */
static __inline__ int top_bit(unsigned int bits)
{
  int res;

#if defined(__i386__)  ||  defined(__x86_64__)
__asm__(" xorl %[res],%[res];\n" " decl %[res];\n" " bsrl %[bits],%[res]\n":[res] "=&r"
          (res)
:        [bits] "rm"(bits));
  return res;
#elif defined(__ppc__)  ||   defined(__powerpc__)
__asm__("cntlzw %[res],%[bits];\n":[res] "=&r"(res)
:        [bits] "r"(bits));
  return 31 - res;
#else
  if (bits == 0)
    return -1;
  res = 0;
  if (bits & 0xFFFF0000) {
    bits &= 0xFFFF0000;
    res += 16;
  }
  if (bits & 0xFF00FF00) {
    bits &= 0xFF00FF00;
    res += 8;
  }
  if (bits & 0xF0F0F0F0) {
    bits &= 0xF0F0F0F0;
    res += 4;
  }
  if (bits & 0xCCCCCCCC) {
    bits &= 0xCCCCCCCC;
    res += 2;
  }
  if (bits & 0xAAAAAAAA) {
    bits &= 0xAAAAAAAA;
    res += 1;
  }
  return res;
#endif
}

/*- End of function --------------------------------------------------------*/

/*! \brief Find the bit position of the lowest set bit in a word
    \param bits The word to be searched
    \return The bit number of the lowest set bit, or -1 if the word is zero. */
static __inline__ int bottom_bit(unsigned int bits)
{
  int res;

#if defined(__i386__)  ||  defined(__x86_64__)
__asm__(" xorl %[res],%[res];\n" " decl %[res];\n" " bsfl %[bits],%[res]\n":[res] "=&r"
          (res)
:        [bits] "rm"(bits));
  return res;
#else
  if (bits == 0)
    return -1;
  res = 31;
  if (bits & 0x0000FFFF) {
    bits &= 0x0000FFFF;
    res -= 16;
  }
  if (bits & 0x00FF00FF) {
    bits &= 0x00FF00FF;
    res -= 8;
  }
  if (bits & 0x0F0F0F0F) {
    bits &= 0x0F0F0F0F;
    res -= 4;
  }
  if (bits & 0x33333333) {
    bits &= 0x33333333;
    res -= 2;
  }
  if (bits & 0x55555555) {
    bits &= 0x55555555;
    res -= 1;
  }
  return res;
#endif
}

/*- End of function --------------------------------------------------------*/

/*! \brief Bit reverse a byte.
    \param data The byte to be reversed.
    \return The bit reversed version of data. */
static __inline__ uint8_t bit_reverse8(uint8_t x)
{
#if defined(__i386__)  ||  defined(__x86_64__)  ||  defined(__ppc__)  ||  defined(__powerpc__)
  /* If multiply is fast */
  return ((x * 0x0802U & 0x22110U) | (x * 0x8020U & 0x88440U)) * 0x10101U >> 16;
#else
  /* If multiply is slow, but we have a barrel shifter */
  x = (x >> 4) | (x << 4);
  x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2);
  return ((x & 0xAA) >> 1) | ((x & 0x55) << 1);
#endif
}

/*- End of function --------------------------------------------------------*/

/*! \brief Bit reverse a 16 bit word.
    \param data The word to be reversed.
    \return The bit reversed version of data. */
uint16_t bit_reverse16(uint16_t data);

/*! \brief Bit reverse a 32 bit word.
    \param data The word to be reversed.
    \return The bit reversed version of data. */
uint32_t bit_reverse32(uint32_t data);

/*! \brief Bit reverse each of the four bytes in a 32 bit word.
    \param data The word to be reversed.
    \return The bit reversed version of data. */
uint32_t bit_reverse_4bytes(uint32_t data);

#if defined(__x86_64__)
/*! \brief Bit reverse each of the eight bytes in a 64 bit word.
    \param data The word to be reversed.
    \return The bit reversed version of data. */
uint64_t bit_reverse_8bytes(uint64_t data);
#endif

/*! \brief Bit reverse each bytes in a buffer.
    \param to The buffer to place the reversed data in.
    \param from The buffer containing the data to be reversed.
    \param The length of the data in the buffer. */
void bit_reverse(uint8_t to[], const uint8_t from[], int len);

/*! \brief Find the number of set bits in a 32 bit word.
    \param x The word to be searched.
    \return The number of set bits. */
int one_bits32(uint32_t x);

/*! \brief Create a mask as wide as the number in a 32 bit word.
    \param x The word to be searched.
    \return The mask. */
uint32_t make_mask32(uint32_t x);

/*! \brief Create a mask as wide as the number in a 16 bit word.
    \param x The word to be searched.
    \return The mask. */
uint16_t make_mask16(uint16_t x);

/*! \brief Find the least significant one in a word, and return a word
           with just that bit set.
    \param x The word to be searched.
    \return The word with the single set bit. */
static __inline__ uint32_t least_significant_one32(uint32_t x)
{
  return (x & (-(int32_t) x));
}

/*- End of function --------------------------------------------------------*/

/*! \brief Find the most significant one in a word, and return a word
           with just that bit set.
    \param x The word to be searched.
    \return The word with the single set bit. */
static __inline__ uint32_t most_significant_one32(uint32_t x)
{
#if defined(__i386__)  ||  defined(__x86_64__)  ||  defined(__ppc__)  ||  defined(__powerpc__)
  return 1 << top_bit(x);
#else
  x = make_mask32(x);
  return (x ^ (x >> 1));
#endif
}

/*- End of function --------------------------------------------------------*/

/*! \brief Find the parity of a byte.
    \param x The byte to be checked.
    \return 1 for odd, or 0 for even. */
static __inline__ int parity8(uint8_t x)
{
  x = (x ^ (x >> 4)) & 0x0F;
  return (0x6996 >> x) & 1;
}

/*- End of function --------------------------------------------------------*/

/*! \brief Find the parity of a 16 bit word.
    \param x The word to be checked.
    \return 1 for odd, or 0 for even. */
static __inline__ int parity16(uint16_t x)
{
  x ^= (x >> 8);
  x = (x ^ (x >> 4)) & 0x0F;
  return (0x6996 >> x) & 1;
}

/*- End of function --------------------------------------------------------*/

/*! \brief Find the parity of a 32 bit word.
    \param x The word to be checked.
    \return 1 for odd, or 0 for even. */
static __inline__ int parity32(uint32_t x)
{
  x ^= (x >> 16);
  x ^= (x >> 8);
  x = (x ^ (x >> 4)) & 0x0F;
  return (0x6996 >> x) & 1;
}

/*- End of function --------------------------------------------------------*/

/*- End of file ------------------------------------------------------------*/
/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fir.h - General telephony FIR routines
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2002 Steve Underwood
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
 * $Id: fir.h,v 1.8 2006/10/24 13:45:28 steveu Exp $
 */

/*! \page fir_page FIR filtering
\section fir_page_sec_1 What does it do?
???.

\section fir_page_sec_2 How does it work?
???.
*/

#if 0
#if defined(USE_MMX)  ||  defined(USE_SSE2)
#include "mmx.h"
#endif

/*!
    16 bit integer FIR descriptor. This defines the working state for a single
    instance of an FIR filter using 16 bit integer coefficients.
*/
typedef struct {
  int taps;
  int curr_pos;
  const int16_t *coeffs;
  int16_t *history;
} fir16_state_t;

/*!
    32 bit integer FIR descriptor. This defines the working state for a single
    instance of an FIR filter using 32 bit integer coefficients, and filtering
    16 bit integer data.
*/
typedef struct {
  int taps;
  int curr_pos;
  const int32_t *coeffs;
  int16_t *history;
} fir32_state_t;

/*!
    Floating point FIR descriptor. This defines the working state for a single
    instance of an FIR filter using floating point coefficients and data.
*/
typedef struct {
  int taps;
  int curr_pos;
  const float *coeffs;
  float *history;
} fir_float_state_t;

static __inline__ const int16_t *fir16_create(fir16_state_t * fir, const int16_t * coeffs,
                                              int taps)
{
  fir->taps = taps;
  fir->curr_pos = taps - 1;
  fir->coeffs = coeffs;
#if defined(USE_MMX)  ||  defined(USE_SSE2)
  if ((fir->history = malloc(2 * taps * sizeof(int16_t))))
    memset(fir->history, 0, 2 * taps * sizeof(int16_t));
#else
  if ((fir->history = (int16_t *) malloc(taps * sizeof(int16_t))))
    memset(fir->history, 0, taps * sizeof(int16_t));
#endif
  return fir->history;
}

/*- End of function --------------------------------------------------------*/

static __inline__ void fir16_flush(fir16_state_t * fir)
{
#if defined(USE_MMX)  ||  defined(USE_SSE2)
  memset(fir->history, 0, 2 * fir->taps * sizeof(int16_t));
#else
  memset(fir->history, 0, fir->taps * sizeof(int16_t));
#endif
}

/*- End of function --------------------------------------------------------*/

static __inline__ void fir16_free(fir16_state_t * fir)
{
  free(fir->history);
}

/*- End of function --------------------------------------------------------*/

static __inline__ int16_t fir16(fir16_state_t * fir, int16_t sample)
{
  int i;
  int32_t y;
#if defined(USE_MMX)
  mmx_t *mmx_coeffs;
  mmx_t *mmx_hist;

  fir->history[fir->curr_pos] = sample;
  fir->history[fir->curr_pos + fir->taps] = sample;

  mmx_coeffs = (mmx_t *) fir->coeffs;
  mmx_hist = (mmx_t *) & fir->history[fir->curr_pos];
  i = fir->taps;
  pxor_r2r(mm4, mm4);
  /* 8 samples per iteration, so the filter must be a multiple of 8 long. */
  while (i > 0) {
    movq_m2r(mmx_coeffs[0], mm0);
    movq_m2r(mmx_coeffs[1], mm2);
    movq_m2r(mmx_hist[0], mm1);
    movq_m2r(mmx_hist[1], mm3);
    mmx_coeffs += 2;
    mmx_hist += 2;
    pmaddwd_r2r(mm1, mm0);
    pmaddwd_r2r(mm3, mm2);
    paddd_r2r(mm0, mm4);
    paddd_r2r(mm2, mm4);
    i -= 8;
  }
  movq_r2r(mm4, mm0);
  psrlq_i2r(32, mm0);
  paddd_r2r(mm0, mm4);
  movd_r2m(mm4, y);
  emms();
#elif defined(USE_SSE2)
  xmm_t *xmm_coeffs;
  xmm_t *xmm_hist;

  fir->history[fir->curr_pos] = sample;
  fir->history[fir->curr_pos + fir->taps] = sample;

  xmm_coeffs = (xmm_t *) fir->coeffs;
  xmm_hist = (xmm_t *) & fir->history[fir->curr_pos];
  i = fir->taps;
  pxor_r2r(xmm4, xmm4);
  /* 16 samples per iteration, so the filter must be a multiple of 16 long. */
  while (i > 0) {
    movdqu_m2r(xmm_coeffs[0], xmm0);
    movdqu_m2r(xmm_coeffs[1], xmm2);
    movdqu_m2r(xmm_hist[0], xmm1);
    movdqu_m2r(xmm_hist[1], xmm3);
    xmm_coeffs += 2;
    xmm_hist += 2;
    pmaddwd_r2r(xmm1, xmm0);
    pmaddwd_r2r(xmm3, xmm2);
    paddd_r2r(xmm0, xmm4);
    paddd_r2r(xmm2, xmm4);
    i -= 16;
  }
  movdqa_r2r(xmm4, xmm0);
  psrldq_i2r(8, xmm0);
  paddd_r2r(xmm0, xmm4);
  movdqa_r2r(xmm4, xmm0);
  psrldq_i2r(4, xmm0);
  paddd_r2r(xmm0, xmm4);
  movd_r2m(xmm4, y);
#else
  int offset1;
  int offset2;

  fir->history[fir->curr_pos] = sample;

  offset2 = fir->curr_pos;
  offset1 = fir->taps - offset2;
  y = 0;
  for (i = fir->taps - 1; i >= offset1; i--)
    y += fir->coeffs[i] * fir->history[i - offset1];
  for (; i >= 0; i--)
    y += fir->coeffs[i] * fir->history[i + offset2];
#endif
  if (fir->curr_pos <= 0)
    fir->curr_pos = fir->taps;
  fir->curr_pos--;
  return (int16_t) (y >> 15);
}

/*- End of function --------------------------------------------------------*/

static __inline__ const int16_t *fir32_create(fir32_state_t * fir, const int32_t * coeffs,
                                              int taps)
{
  fir->taps = taps;
  fir->curr_pos = taps - 1;
  fir->coeffs = coeffs;
  fir->history = (int16_t *) malloc(taps * sizeof(int16_t));
  if (fir->history)
    memset(fir->history, '\0', taps * sizeof(int16_t));
  return fir->history;
}

/*- End of function --------------------------------------------------------*/

static __inline__ void fir32_flush(fir32_state_t * fir)
{
  memset(fir->history, 0, fir->taps * sizeof(int16_t));
}

/*- End of function --------------------------------------------------------*/

static __inline__ void fir32_free(fir32_state_t * fir)
{
  free(fir->history);
}

/*- End of function --------------------------------------------------------*/

static __inline__ int16_t fir32(fir32_state_t * fir, int16_t sample)
{
  int i;
  int32_t y;
  int offset1;
  int offset2;

  fir->history[fir->curr_pos] = sample;
  offset2 = fir->curr_pos;
  offset1 = fir->taps - offset2;
  y = 0;
  for (i = fir->taps - 1; i >= offset1; i--)
    y += fir->coeffs[i] * fir->history[i - offset1];
  for (; i >= 0; i--)
    y += fir->coeffs[i] * fir->history[i + offset2];
  if (fir->curr_pos <= 0)
    fir->curr_pos = fir->taps;
  fir->curr_pos--;
  return (int16_t) (y >> 15);
}

/*- End of function --------------------------------------------------------*/

static __inline__ const float *fir_float_create(fir_float_state_t * fir,
                                                const float *coeffs, int taps)
{
  fir->taps = taps;
  fir->curr_pos = taps - 1;
  fir->coeffs = coeffs;
  fir->history = (float *) malloc(taps * sizeof(float));
  if (fir->history)
    memset(fir->history, '\0', taps * sizeof(float));
  return fir->history;
}

/*- End of function --------------------------------------------------------*/

static __inline__ void fir_float_free(fir_float_state_t * fir)
{
  free(fir->history);
}

/*- End of function --------------------------------------------------------*/

static __inline__ int16_t fir_float(fir_float_state_t * fir, int16_t sample)
{
  int i;
  float y;
  int offset1;
  int offset2;

  fir->history[fir->curr_pos] = sample;

  offset2 = fir->curr_pos;
  offset1 = fir->taps - offset2;
  y = 0;
  for (i = fir->taps - 1; i >= offset1; i--)
    y += fir->coeffs[i] * fir->history[i - offset1];
  for (; i >= 0; i--)
    y += fir->coeffs[i] * fir->history[i + offset2];
  if (fir->curr_pos <= 0)
    fir->curr_pos = fir->taps;
  fir->curr_pos--;
  return (int16_t) y;
}

/*- End of function --------------------------------------------------------*/
#endif

/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo.h - An echo cancellor, suitable for electrical and acoustic
 *	    cancellation. This code does not currently comply with
 *	    any relevant standards (e.g. G.164/5/7/8).
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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
 * $Id: echo.h,v 1.9 2006/10/24 13:45:28 steveu Exp $
 */

/*! \file */

/*! \page echo_can_page Line echo cancellation for voice

\section echo_can_page_sec_1 What does it do?
This module aims to provide G.168-2002 compliant echo cancellation, to remove
electrical echoes (e.g. from 2-4 wire hybrids) from voice calls.

\section echo_can_page_sec_2 How does it work?
The heart of the echo cancellor is FIR filter. This is adapted to match the echo
impulse response of the telephone line. It must be long enough to adequately cover
the duration of that impulse response. The signal transmitted to the telephone line
is passed through the FIR filter. Once the FIR is properly adapted, the resulting
output is an estimate of the echo signal received from the line. This is subtracted
from the received signal. The result is an estimate of the signal which originated
at the far end of the line, free from echos of our own transmitted signal. 

The least mean squares (LMS) algorithm is attributed to Widrow and Hoff, and was
introduced in 1960. It is the commonest form of filter adaption used in things
like modem line equalisers and line echo cancellers. There it works very well.
However, it only works well for signals of constant amplitude. It works very poorly
for things like speech echo cancellation, where the signal level varies widely.
This is quite easy to fix. If the signal level is normalised - similar to applying
AGC - LMS can work as well for a signal of varying amplitude as it does for a modem
signal. This normalised least mean squares (NLMS) algorithm is the commonest one used
for speech echo cancellation. Many other algorithms exist - e.g. RLS (essentially
the same as Kalman filtering), FAP, etc. Some perform significantly better than NLMS.
However, factors such as computational complexity and patents favour the use of NLMS.

A simple refinement to NLMS can improve its performance with speech. NLMS tends
to adapt best to the strongest parts of a signal. If the signal is white noise,
the NLMS algorithm works very well. However, speech has more low frequency than
high frequency content. Pre-whitening (i.e. filtering the signal to flatten
its spectrum) the echo signal improves the adapt rate for speech, and ensures the
final residual signal is not heavily biased towards high frequencies. A very low
complexity filter is adequate for this, so pre-whitening adds little to the
compute requirements of the echo canceller.

An FIR filter adapted using pre-whitened NLMS performs well, provided certain
conditions are met: 

    - The transmitted signal has poor self-correlation.
    - There is no signal being generated within the environment being cancelled.

The difficulty is that neither of these can be guaranteed.

If the adaption is performed while transmitting noise (or something fairly noise
like, such as voice) the adaption works very well. If the adaption is performed
while transmitting something highly correlative (typically narrow band energy
such as signalling tones or DTMF), the adaption can go seriously wrong. The reason
is there is only one solution for the adaption on a near random signal - the impulse
response of the line. For a repetitive signal, there are any number of solutions
which converge the adaption, and nothing guides the adaption to choose the generalised
one. Allowing an untrained canceller to converge on this kind of narrowband
energy probably a good thing, since at least it cancels the tones. Allowing a well
converged canceller to continue converging on such energy is just a way to ruin
its generalised adaption. A narrowband detector is needed, so adapation can be
suspended at appropriate times.

The adaption process is based on trying to eliminate the received signal. When
there is any signal from within the environment being cancelled it may upset the
adaption process. Similarly, if the signal we are transmitting is small, noise
may dominate and disturb the adaption process. If we can ensure that the
adaption is only performed when we are transmitting a significant signal level,
and the environment is not, things will be OK. Clearly, it is easy to tell when
we are sending a significant signal. Telling, if the environment is generating a
significant signal, and doing it with sufficient speed that the adaption will
not have diverged too much more we stop it, is a little harder. 

The key problem in detecting when the environment is sourcing significant energy
is that we must do this very quickly. Given a reasonably long sample of the
received signal, there are a number of strategies which may be used to assess
whether that signal contains a strong far end component. However, by the time
that assessment is complete the far end signal will have already caused major
mis-convergence in the adaption process. An assessment algorithm is needed which
produces a fairly accurate result from a very short burst of far end energy. 

\section echo_can_page_sec_3 How do I use it?
The echo cancellor processes both the transmit and receive streams sample by
sample. The processing function is not declared inline. Unfortunately,
cancellation requires many operations per sample, so the call overhead is only a
minor burden. 
*/

#define NONUPDATE_DWELL_TIME	600 /* 600 samples, or 75ms */

#if 0
/* Mask bits for the adaption mode */
#define ECHO_CAN_USE_NLP            0x01
#define ECHO_CAN_USE_SUPPRESSOR     0x02
#define ECHO_CAN_USE_CNG            0x04
#define ECHO_CAN_USE_ADAPTION       0x08

/*!
    G.168 echo canceller descriptor. This defines the working state for a line
    echo canceller.
*/
typedef struct {
  int tx_power[4];
  int rx_power[3];
  int clean_rx_power;

  int rx_power_threshold;
  int nonupdate_dwell;

  fir16_state_t fir_state;
  /*! Echo FIR taps (16 bit version) */
  int16_t *fir_taps16[4];
  /*! Echo FIR taps (32 bit version) */
  int32_t *fir_taps32;

  int curr_pos;

  int taps;
  int tap_mask;
  int adaption_mode;

  int32_t supp_test1;
  int32_t supp_test2;
  int32_t supp1;
  int32_t supp2;
  int vad;
  int cng;
  /* Parameters for the Hoth noise generator */
  int cng_level;
  int cng_rndnum;
  int cng_filter;

  int16_t geigel_max;
  int geigel_lag;
  int dtd_onset;
  int tap_set;
  int tap_rotate_counter;

  int32_t latest_correction;    /* Indication of the magnitude of the latest
                                   adaption, or a code to indicate why adaption
                                   was skipped, for test purposes */
  int32_t last_acf[28];
  int narrowband_count;
  int narrowband_score;
} echo_can_state_t;

/*! Create a voice echo canceller context.
    \param len The length of the canceller, in samples.
    \return The new canceller context, or NULL if the canceller could not be created.
*/
echo_can_state_t *echo_can_create(int len, int adaption_mode);

/*! Free a voice echo canceller context.
    \param ec The echo canceller context.
*/
void echo_can_free(echo_can_state_t * ec);

/*! Flush (reinitialise) a voice echo canceller context.
    \param ec The echo canceller context.
*/
void echo_can_flush(echo_can_state_t * ec);

/*! Set the adaption mode of a voice echo canceller context.
    \param ec The echo canceller context.
    \param adapt The mode.
*/
void echo_can_adaption_mode(echo_can_state_t * ec, int adaption_mode);

/*! Process a sample through a voice echo canceller.
    \param ec The echo canceller context.
    \param tx The transmitted audio sample.
    \param rx The received audio sample.
    \return The clean (echo cancelled) received sample.
*/
int16_t echo_can_update(echo_can_state_t * ec, int16_t tx, int16_t rx);

#endif
/*- End of file ------------------------------------------------------------*/

/*!
    Floating point Goertzel filter descriptor.
*/
typedef struct {
  float fac;
  int samples;
} goertzel_descriptor_t;

/*!
    Floating point Goertzel filter state descriptor.
*/
typedef struct {
  float v2;
  float v3;
  float fac;
  int samples;
  int current_sample;
} goertzel_state_t;

/*! \brief Create a descriptor for use with either a Goertzel transform */
void make_goertzel_descriptor(goertzel_descriptor_t * t, float freq, int samples);

/*! \brief Initialise the state of a Goertzel transform.
    \param s The Goertzel context. If NULL, a context is allocated with malloc.
    \param t The Goertzel descriptor.
    \return A pointer to the Goertzel state. */
goertzel_state_t *goertzel_init(goertzel_state_t * s, goertzel_descriptor_t * t);

/*! \brief Reset the state of a Goertzel transform.
    \param s The Goertzel context.
    \param t The Goertzel descriptor.
    \return A pointer to the Goertzel state. */
void goertzel_reset(goertzel_state_t * s);

/*! \brief Update the state of a Goertzel transform.
    \param s The Goertzel context
    \param amp The samples to be transformed
    \param samples The number of samples
    \return The number of samples unprocessed */
int goertzel_update(goertzel_state_t * s, const int16_t amp[], int samples);

/*! \brief Evaluate the final result of a Goertzel transform.
    \param s The Goertzel context
    \return The result of the transform. */
float goertzel_result(goertzel_state_t * s);

/*! \brief Update the state of a Goertzel transform.
    \param s The Goertzel context
    \param amp The sample to be transformed. */
static __inline__ void goertzel_sample(goertzel_state_t * s, int16_t amp)
{
  float v1;

  v1 = s->v2;
  s->v2 = s->v3;
  s->v3 = s->fac * s->v2 - v1 + amp;
  s->current_sample++;
}

/*- End of function --------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tone_detect.c - General telephony tone detection.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001-2003, 2005 Steve Underwood
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
 * $Id: tone_detect.c,v 1.31 2007/03/03 10:40:33 steveu Exp $
 */

/*! \file tone_detect.h */

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif
/*! \page dtmf_rx_page DTMF receiver
\section dtmf_rx_page_sec_1 What does it do?
The DTMF receiver detects the standard DTMF digits. It is compliant with
ITU-T Q.23, ITU-T Q.24, and the local DTMF specifications of most administrations.
Its passes the test suites. It also scores *very* well on the standard
talk-off tests. 

The current design uses floating point extensively. It is not tolerant of DC.
It is expected that a DC restore stage will be placed before the DTMF detector.
Unless the dial tone filter is switched on, the detector has poor tolerance
of dial tone. Whether this matter depends on your application. If you are using
the detector in an IVR application you will need proper echo cancellation to
get good performance in the presence of speech prompts, so dial tone will not
exist. If you do need good dial tone tolerance, a dial tone filter can be
enabled in the detector.

\section dtmf_rx_page_sec_2 How does it work?
Like most other DSP based DTMF detector's, this one uses the Goertzel algorithm
to look for the DTMF tones. What makes each detector design different is just how
that algorithm is used.

Basic DTMF specs:
    - Minimum tone on = 40ms
    - Minimum tone off = 50ms
    - Maximum digit rate = 10 per second
    - Normal twist <= 8dB accepted
    - Reverse twist <= 4dB accepted
    - S/N >= 15dB will detect OK
    - Attenuation <= 26dB will detect OK
    - Frequency tolerance +- 1.5% will detect, +-3.5% will reject

TODO:
*/

/*! \page dtmf_tx_page DTMF tone generation
\section dtmf_tx_page_sec_1 What does it do?

The DTMF tone generation module provides for the generation of the
repertoire of 16 DTMF dual tones. 

\section dtmf_tx_page_sec_2 How does it work?
*/

#define MAX_DTMF_DIGITS 128

typedef void (*dtmf_rx_callback_t) (void *user_data, const char *digits, int len);

/*!
    DTMF generator state descriptor. This defines the state of a single
    working instance of a DTMF generator.
*/
#if 0
typedef struct {
  tone_gen_descriptor_t *tone_descriptors;
  tone_gen_state_t tones;
  char digits[MAX_DTMF_DIGITS + 1];
  int current_sample;
  size_t current_digits;
} dtmf_tx_state_t;

#endif

/*!
    DTMF digit detector descriptor.
*/
typedef struct {
  /*! Optional callback funcion to deliver received digits. */
  dtmf_rx_callback_t callback;
  /*! An opaque pointer passed to the callback function. */
  void *callback_data;
  /*! Optional callback funcion to deliver real time digit state changes. */
  //tone_report_func_t realtime_callback;
  void *realtime_callback;
  /*! An opaque pointer passed to the real time callback function. */
  void *realtime_callback_data;
  /*! TRUE if dialtone should be filtered before processing */
  int filter_dialtone;
  /*! Maximum acceptable "normal" (lower bigger than higher) twist ratio */
  float normal_twist;
  /*! Maximum acceptable "reverse" (higher bigger than lower) twist ratio */
  float reverse_twist;

  /*! 350Hz filter state for the optional dialtone filter */
  float z350_1;
  float z350_2;
  /*! 440Hz filter state for the optional dialtone filter */
  float z440_1;
  float z440_2;

  /*! Tone detector working states */
  goertzel_state_t row_out[4];
  goertzel_state_t col_out[4];
  /*! The accumlating total energy on the same period over which the Goertzels work. */
  float energy;
  /*! The result of the last tone analysis. */
  uint8_t last_hit;
  /*! The confirmed digit we are currently receiving */
  uint8_t in_digit;
  /*! The current sample number within a processing block. */
  int current_sample;

  /*! The received digits buffer. This is a NULL terminated string. */
  char digits[MAX_DTMF_DIGITS + 1];
  /*! The number of digits currently in the digit buffer. */
  int current_digits;
  /*! The number of digits which have been lost due to buffer overflows. */
  int lost_digits;
} dtmf_rx_state_t;

#if 0
/*! \brief Generate a buffer of DTMF tones.
    \param s The DTMF generator context.
    \param amp The buffer for the generated signal.
    \param max_samples The required number of generated samples.
    \return The number of samples actually generated. This may be less than 
            samples if the input buffer empties. */
int dtmf_tx(dtmf_tx_state_t * s, int16_t amp[], int max_samples);

/*! \brief Put a string of digits in a DTMF generator's input buffer.
    \param s The DTMF generator context.
    \param digits The string of digits to be added.
    \return The number of digits actually added. This may be less than the
            length of the digit string, if the buffer fills up. */
size_t dtmf_tx_put(dtmf_tx_state_t * s, const char *digits);

/*! \brief Initialise a DTMF tone generator context.
    \param s The DTMF generator context.
    \return A pointer to the DTMF generator context. */
dtmf_tx_state_t *dtmf_tx_init(dtmf_tx_state_t * s);
#endif

/*! Set a optional realtime callback for a DTMF receiver context. This function
    is called immediately a confirmed state change occurs in the received DTMF. It
    is called with the ASCII value for a DTMF tone pair, or zero to indicate no tone
    is being received.
    \brief Set a realtime callback for a DTMF receiver context.
    \param s The DTMF receiver context.
    \param callback Callback routine used to report the start and end of digits.
    \param user_data An opaque pointer which is associated with the context,
           and supplied in callbacks. */
void dtmf_rx_set_realtime_callback(dtmf_rx_state_t * s,
                                   //tone_report_func_t callback,
                                   void *callback, void *user_data);

/*! \brief Adjust a DTMF receiver context.
    \param s The DTMF receiver context.
    \param filter_dialtone TRUE to enable filtering of dialtone, FALSE
           to disable, < 0 to leave unchanged.
    \param twist Acceptable twist, in dB. < 0 to leave unchanged.
    \param reverse_twist Acceptable reverse twist, in dB. < 0 to leave unchanged. */
void dtmf_rx_parms(dtmf_rx_state_t * s, int filter_dialtone, int twist,
                   int reverse_twist);

/*! Process a block of received DTMF audio samples.
    \brief Process a block of received DTMF audio samples.
    \param s The DTMF receiver context.
    \param amp The audio sample buffer.
    \param samples The number of samples in the buffer.
    \return The number of samples unprocessed. */
int dtmf_rx(dtmf_rx_state_t * s, const int16_t amp[], int samples);

/*! \brief Get a string of digits from a DTMF receiver's output buffer.
    \param s The DTMF receiver context.
    \param digits The buffer for the received digits.
    \param max The maximum  number of digits to be returned,
    \return The number of digits actually returned. */
size_t dtmf_rx_get(dtmf_rx_state_t * s, char *digits, int max);

/*! \brief Initialise a DTMF receiver context.
    \param s The DTMF receiver context.
    \param callback An optional callback routine, used to report received digits. If
           no callback routine is set, digits may be collected, using the dtmf_rx_get()
           function.
    \param user_data An opaque pointer which is associated with the context,
           and supplied in callbacks.
    \return A pointer to the DTMF receiver context. */
dtmf_rx_state_t *dtmf_rx_init(dtmf_rx_state_t * s, dtmf_rx_callback_t callback,
                              void *user_data);

/*- End of file ------------------------------------------------------------*/

#endif /* _CELLIAX_SPANDSP_H */

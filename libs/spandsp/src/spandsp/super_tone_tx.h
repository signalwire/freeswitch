/*
 * SpanDSP - a series of DSP components for telephony
 *
 * super_tone_tx.h - Flexible telephony supervisory tone generation.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: super_tone_tx.h,v 1.13 2008/04/17 14:27:00 steveu Exp $
 */

#if !defined(_SPANDSP_SUPER_TONE_TX_H_)
#define _SPANDSP_SUPER_TONE_TX_H_

/*! \page super_tone_tx_page Supervisory tone generation

\section super_tone_tx_page_sec_1 What does it do?

The supervisory tone generator may be configured to generate most of the world's
telephone supervisory tones - things like ringback, busy, number unobtainable,
and so on. It uses tree structure tone descriptions, which can deal with quite
complex cadence patterns. 

\section super_tone_tx_page_sec_2 How does it work?

*/

typedef struct super_tone_tx_step_s super_tone_tx_step_t;

struct super_tone_tx_step_s
{
    tone_gen_tone_descriptor_t tone[4];
    int tone_on;
    int length;
    int cycles;
    super_tone_tx_step_t *next;
    super_tone_tx_step_t *nest;
};

typedef struct
{
    tone_gen_tone_descriptor_t tone[4];
    uint32_t phase[4];
    int current_position;
    int level;
    super_tone_tx_step_t *levels[4];
    int cycles[4];
} super_tone_tx_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

super_tone_tx_step_t *super_tone_tx_make_step(super_tone_tx_step_t *s,
                                              float f1,
                                              float l1,
                                              float f2,
                                              float l2,
                                              int length,
                                              int cycles);

void super_tone_tx_free(super_tone_tx_step_t *s);

/*! Initialise a supervisory tone generator.
    \brief Initialise a supervisory tone generator.
    \param s The supervisory tone generator context.
    \param tree The supervisory tone tree to be generated.
    \return The supervisory tone generator context. */
super_tone_tx_state_t *super_tone_tx_init(super_tone_tx_state_t *s, super_tone_tx_step_t *tree);

/*! Generate a block of audio samples for a supervisory tone pattern.
    \brief Generate a block of audio samples for a supervisory tone pattern.
    \param s The supervisory tone context.
    \param amp The audio sample buffer.
    \param max_samples The maximum number of samples to be generated.
    \return The number of samples generated. */
int super_tone_tx(super_tone_tx_state_t *s, int16_t amp[], int max_samples);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/super_tone_tx.h - Flexible telephony supervisory tone generation.
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
 */

#if !defined(_SPANDSP_PRIVATE_SUPER_TONE_TX_H_)
#define _SPANDSP_PRIVATE_SUPER_TONE_TX_H_

struct super_tone_tx_step_s
{
    tone_gen_tone_descriptor_t tone[SUPER_TONE_TX_MAX_TONES];
    int tone_on;
    int length;
    int cycles;
    super_tone_tx_step_t *next;
    super_tone_tx_step_t *nest;
};

struct super_tone_tx_state_s
{
    tone_gen_tone_descriptor_t tone[SUPER_TONE_TX_MAX_TONES];
    uint32_t phase[SUPER_TONE_TX_MAX_TONES];
    int current_position;
    int level;
    super_tone_tx_step_t *levels[SUPER_TONE_TX_MAX_LEVELS];
    int cycles[SUPER_TONE_TX_MAX_LEVELS];
};

#endif
/*- End of file ------------------------------------------------------------*/

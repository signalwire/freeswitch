/*
 * SpanDSP - a series of DSP components for telephony
 *
 * line_model.h - Model a telephone line.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: line_model.h,v 1.7.4.1 2009/12/19 10:16:44 steveu Exp $
 */

/*! \file */

/*! \page line_model_page Telephone line model
\section line_model_page_sec_1 What does it do?
The telephone line modelling module provides simple modelling of one way and two
way telephone lines.

The path being modelled is:

    -    terminal
    -      | < hybrid echo (2-way models)
    -      |
    -      | < noise and filtering
    -      |
    -      | < hybrid echo (2-way models)
    -     CO
    -      |
    -      | < A-law distortion + bulk delay
    -      |
    -     CO
    -      | < hybrid echo (2-way models)
    -      |
    -      | < noise and filtering
    -      |
    -      | < hybrid echo (2-way models)
    -    terminal
*/

#if !defined(_SPANDSP_LINE_MODEL_H_)
#define _SPANDSP_LINE_MODEL_H_

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>

#define LINE_FILTER_SIZE 129

/*!
    One way line model descriptor. This holds the complete state of
    a line model with transmission in only one direction.
*/
typedef struct
{
    codec_munge_state_t *munge;

    /*! The coefficients for the near end analogue section simulation filter */
    const float *near_filter;
    /*! The number of coefficients for the near end analogue section simulation filter */
    int near_filter_len;
    /*! Last transmitted samples (ring buffer, used by the line filter) */
    float near_buf[LINE_FILTER_SIZE];
    /*! Pointer of the last transmitted sample in buf */
    int near_buf_ptr;
    /*! The noise source for local analogue section of the line */
    awgn_state_t near_noise;

    /*! The bulk delay of the path, in samples */
    int bulk_delay;
    /*! A pointer to the current write position in the bulk delay store. */
    int bulk_delay_ptr;
    /*! The data store for simulating the bulk delay */
    int16_t bulk_delay_buf[8000];

    /*! The coefficients for the far end analogue section simulation filter */
    const float *far_filter;
    /*! The number of coefficients for the far end analogue section simulation filter */
    int far_filter_len;
    /*! Last transmitted samples (ring buffer, used by the line filter) */
    float far_buf[LINE_FILTER_SIZE];
    /*! Pointer of the last transmitted sample in buf */
    int far_buf_ptr;
    /*! The noise source for distant analogue section of the line */
    awgn_state_t far_noise;

    /*! The scaling factor for the local CPE hybrid echo */
    float near_cpe_hybrid_echo;
    /*! The scaling factor for the local CO hybrid echo */
    float near_co_hybrid_echo;

    /*! The scaling factor for the far CPE hybrid echo */
    float far_cpe_hybrid_echo;
    /*! The scaling factor for the far CO hybrid echo */
    float far_co_hybrid_echo;
    /*! DC offset impairment */
    float dc_offset;
    
    /*! Mains pickup impairment */
    int mains_interference;
    tone_gen_state_t mains_tone;
} one_way_line_model_state_t;

/*!
    Two way line model descriptor. This holds the complete state of
    a line model with transmission in both directions.
*/
typedef struct
{
    one_way_line_model_state_t line1;
    one_way_line_model_state_t line2;
    float fout1;
    float fout2; 
} both_ways_line_model_state_t;

#ifdef __cplusplus
extern "C"
{
#endif

SPAN_DECLARE_DATA extern const float *line_models[];

SPAN_DECLARE(void) both_ways_line_model(both_ways_line_model_state_t *s, 
                                        int16_t output1[],
                                        const int16_t input1[],
                                        int16_t output2[],
                                        const int16_t input2[],
                                        int samples);

SPAN_DECLARE(void) both_ways_line_model_set_dc(both_ways_line_model_state_t *s, float dc1, float dc2);

SPAN_DECLARE(void) both_ways_line_model_set_mains_pickup(both_ways_line_model_state_t *s, int f, float level1, float level2);
    
SPAN_DECLARE(both_ways_line_model_state_t *) both_ways_line_model_init(int model1,
                                                                       float noise1,
                                                                       int model2,
                                                                       float noise2,
                                                                       int codec,
                                                                       int rbs_pattern);

SPAN_DECLARE(int) both_ways_line_model_release(both_ways_line_model_state_t *s);

SPAN_DECLARE(void) one_way_line_model(one_way_line_model_state_t *s, 
                                      int16_t output[],
                                      const int16_t input[],
                                      int samples);

SPAN_DECLARE(void) one_way_line_model_set_dc(one_way_line_model_state_t *s, float dc);

SPAN_DECLARE(void) one_way_line_model_set_mains_pickup(one_way_line_model_state_t *s, int f, float level);

SPAN_DECLARE(one_way_line_model_state_t *) one_way_line_model_init(int model, float noise, int codec, int rbs_pattern);

SPAN_DECLARE(int) one_way_line_model_release(one_way_line_model_state_t *s);

#ifdef __cplusplus
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

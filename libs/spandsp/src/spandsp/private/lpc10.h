/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/lpc10.h - LPC10 low bit rate speech codec.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: lpc10.h,v 1.1 2008/11/30 05:43:37 steveu Exp $
 */

#if !defined(_SPANDSP_PRIVATE_LPC10_H_)
#define _SPANDSP_PRIVATE_LPC10_H_

/*!
    LPC10 codec encoder state descriptor. This defines the state of
    a single working instance of the LPC10 encoder.
*/
struct lpc10_encode_state_s
{
    int error_correction;

    /* State used only by function high_pass_100hz */
    float z11;
    float z21;
    float z12;
    float z22;
    
    /* State used by function lpc10_analyse */
    float inbuf[LPC10_SAMPLES_PER_FRAME*3];
    float pebuf[LPC10_SAMPLES_PER_FRAME*3];
    float lpbuf[696];
    float ivbuf[312];
    float bias;
    int32_t osbuf[10];      /* No initial value necessary */
    int32_t osptr;          /* Initial value 1 */
    int32_t obound[3];
    int32_t vwin[3][2];     /* Initial value vwin[2][0] = 307; vwin[2][1] = 462; */
    int32_t awin[3][2];     /* Initial value awin[2][0] = 307; awin[2][1] = 462; */
    int32_t voibuf[4][2];
    float rmsbuf[3];
    float rcbuf[3][10];
    float zpre;

    /* State used by function onset */
    float n;
    float d__;          /* Initial value 1.0f */
    float fpc;          /* No initial value necessary */
    float l2buf[16];
    float l2sum1;
    int32_t l2ptr1;     /* Initial value 1 */
    int32_t l2ptr2;     /* Initial value 9 */
    int32_t lasti;      /* No initial value necessary */
    int hyst;           /* Initial value FALSE */

    /* State used by function lpc10_voicing */
    float dither;       /* Initial value 20.0f */
    float snr;
    float maxmin;
    float voice[3][2];  /* Initial value is probably unnecessary */
    int32_t lbve;
    int32_t lbue;
    int32_t fbve;
    int32_t fbue;
    int32_t ofbue;
    int32_t sfbue;
    int32_t olbue;
    int32_t slbue;

    /* State used by function dynamic_pitch_tracking */
    float s[60];
    int32_t p[2][60];
    int32_t ipoint;
    float alphax;

    /* State used by function lpc10_pack */
    int32_t isync;
};

/*!
    LPC10 codec decoder state descriptor. This defines the state of
    a single working instance of the LPC10 decoder.
*/
struct lpc10_decode_state_s
{
    int error_correction;

    /* State used by function decode */
    int32_t iptold;     /* Initial value 60 */
    int first;          /* Initial value TRUE */
    int32_t ivp2h;
    int32_t iovoic;
    int32_t iavgp;      /* Initial value 60 */
    int32_t erate;
    int32_t drc[10][3];
    int32_t dpit[3];
    int32_t drms[3];

    /* State used by function synths */
    float buf[LPC10_SAMPLES_PER_FRAME*2];
    int32_t buflen;     /* Initial value LPC10_SAMPLES_PER_FRAME */

    /* State used by function pitsyn */
    int32_t ivoico;     /* No initial value necessary as long as first_pitsyn is initially TRUE_ */
    int32_t ipito;      /* No initial value necessary as long as first_pitsyn is initially TRUE_ */
    float rmso;         /* Initial value 1.0f */
    float rco[10];      /* No initial value necessary as long as first_pitsyn is initially TRUE_ */
    int32_t jsamp;      /* Nno initial value necessary as long as first_pitsyn is initially TRUE_ */
    int first_pitsyn;   /* Initial value TRUE */

    /* State used by function bsynz */
    int32_t ipo;
    float exc[166];
    float exc2[166];
    float lpi[3];
    float hpi[3];
    float rmso_bsynz;

    /* State used by function random */
    int32_t j;
    int32_t k;
    int16_t y[5];

    /* State used by function deemp */
    float dei[2];
    float deo[3];
};

#endif
/*- End of include ---------------------------------------------------------*/

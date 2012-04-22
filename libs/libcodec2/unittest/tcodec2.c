/*---------------------------------------------------------------------------*\
                                                                          
  FILE........: tcodec2.c                                                  
  AUTHOR......: David Rowe                                            
  DATE CREATED: 24/8/10                                        
                                                               
  Test program for codec2.c functions.
                                                              
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2010 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "defines.h"
#include "codec2.h"
#include "quantise.h"
#include "interp.h"

/* CODEC2 struct copies from codec2.c to help with testing */

typedef struct {
    float  Sn[M];        /* input speech                              */
    float  w[M];	 /* time domain hamming window                */
    COMP   W[FFT_ENC];	 /* DFT of w[]                                */
    float  Pn[2*N];	 /* trapezoidal synthesis window              */
    float  Sn_[2*N];	 /* synthesised speech                        */
    float  prev_Wo;      /* previous frame's pitch estimate           */
    float  ex_phase;     /* excitation model phase track              */
    float  bg_est;       /* background noise estimate for post filter */
    MODEL  prev_model;   /* model parameters from 20ms ago            */
} CODEC2;

void analyse_one_frame(CODEC2 *c2, MODEL *model, short speech[]);
void synthesise_one_frame(CODEC2 *c2, short speech[], MODEL *model, float ak[]);

int test1()
{
    FILE   *fin, *fout;
    short   buf[N];
    void   *c2;
    CODEC2 *c3;
    MODEL   model;
    float   ak[LPC_ORD+1];
    float   lsps[LPC_ORD];

    c2 = codec2_create();
    c3 = (CODEC2*)c2;

    fin = fopen("../raw/hts1a.raw", "rb");
    assert(fin != NULL);
    fout = fopen("hts1a_test.raw", "wb");
    assert(fout != NULL);

    while(fread(buf, sizeof(short), N, fin) == N) {
	analyse_one_frame(c3, &model, buf);
	speech_to_uq_lsps(lsps, ak, c3->Sn, c3->w, LPC_ORD);
	synthesise_one_frame(c3, buf, &model, ak);
	fwrite(buf, sizeof(short), N, fout);
    }

    codec2_destroy(c2);

    fclose(fin);
    fclose(fout);

    return 0;
}
 
int test2()
{
    FILE   *fin, *fout;
    short   buf[2*N];
    void   *c2;
    CODEC2 *c3;
    MODEL   model, model_interp;
    float   ak[LPC_ORD+1];
    int     voiced1, voiced2;
    int     lsp_indexes[LPC_ORD];
    int     lpc_correction;
    int     energy_index;
    int     Wo_index;
    char    bits[CODEC2_BITS_PER_FRAME];
    int     nbit;
    int     i;

    c2 = codec2_create();
    c3 = (CODEC2*)c2;

    fin = fopen("../raw/hts1a.raw", "rb");
    assert(fin != NULL);
    fout = fopen("hts1a_test.raw", "wb");
    assert(fout != NULL);

    while(fread(buf, sizeof(short), 2*N, fin) == 2*N) {
	/* first 10ms analysis frame - we just want voicing */

	analyse_one_frame(c3, &model, buf);
	voiced1 = model.voiced;

	/* second 10ms analysis frame */

	analyse_one_frame(c3, &model, &buf[N]);
	voiced2 = model.voiced;
    
	Wo_index = encode_Wo(model.Wo);
	encode_amplitudes(lsp_indexes, 
			  &lpc_correction, 
			  &energy_index,
			  &model, 
			  c3->Sn, 
			  c3->w);   
	nbit = 0;
	pack(bits, &nbit, Wo_index, WO_BITS);
	for(i=0; i<LPC_ORD; i++) {
	    pack(bits, &nbit, lsp_indexes[i], lsp_bits(i));
	}
	pack(bits, &nbit, lpc_correction, 1);
	pack(bits, &nbit, energy_index, E_BITS);
	pack(bits, &nbit, voiced1, 1);
	pack(bits, &nbit, voiced2, 1);
 
	nbit = 0;
	Wo_index = unpack(bits, &nbit, WO_BITS);
	for(i=0; i<LPC_ORD; i++) {
	    lsp_indexes[i] = unpack(bits, &nbit, lsp_bits(i));
	}
	lpc_correction = unpack(bits, &nbit, 1);
	energy_index = unpack(bits, &nbit, E_BITS);
	voiced1 = unpack(bits, &nbit, 1);
	voiced2 = unpack(bits, &nbit, 1);

	model.Wo = decode_Wo(Wo_index);
	model.L = PI/model.Wo;
	decode_amplitudes(&model, 
			  ak,
			  lsp_indexes,
			  lpc_correction, 
			  energy_index);

	model.voiced = voiced2;
	model_interp.voiced = voiced1;
	interpolate(&model_interp, &c3->prev_model, &model);

	synthesise_one_frame(c3,  buf,     &model_interp, ak);
	synthesise_one_frame(c3, &buf[N],  &model, ak);

	memcpy(&c3->prev_model, &model, sizeof(MODEL));
	fwrite(buf, sizeof(short), 2*N, fout);
    }

    codec2_destroy(c2);

    fclose(fin);
    fclose(fout);

    return 0;
}

int test3()
{
    FILE   *fin, *fout, *fbits;
    short   buf1[2*N];
    short   buf2[2*N];
    char    bits[CODEC2_BITS_PER_FRAME];
    void   *c2;

    c2 = codec2_create();

    fin = fopen("../raw/hts1a.raw", "rb");
    assert(fin != NULL);
    fout = fopen("hts1a_test.raw", "wb");
    assert(fout != NULL);
    fbits = fopen("hts1a_test3.bit", "wb");
    assert(fout != NULL);

    while(fread(buf1, sizeof(short), 2*N, fin) == 2*N) {
	codec2_encode(c2, bits, buf1);
	fwrite(bits, sizeof(char), CODEC2_BITS_PER_FRAME, fbits);
	codec2_decode(c2, buf2, bits);
	fwrite(buf2, sizeof(short), CODEC2_SAMPLES_PER_FRAME, fout);
    }

    codec2_destroy(c2);

    fclose(fin);
    fclose(fout);
    fclose(fbits);

    return 0;
}

int main() {
    test3();
    return 0;
}

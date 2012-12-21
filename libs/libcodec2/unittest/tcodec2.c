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
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "defines.h"
#include "comp.h"
#include "codec2.h"
#include "quantise.h"
#include "interp.h"

/* CODEC2 struct copies from codec2.c to help with testing */

struct CODEC2 {
    int    mode;
    float  w[M];	        /* time domain hamming window                */
    COMP   W[FFT_ENC];	        /* DFT of w[]                                */
    float  Pn[2*N];	        /* trapezoidal synthesis window              */
    float  Sn[M];               /* input speech                              */
    float  hpf_states[2];       /* high pass filter states                   */
    void  *nlp;                 /* pitch predictor states                    */
    float  Sn_[2*N];	        /* synthesised output speech                 */
    float  ex_phase;            /* excitation model phase track              */
    float  bg_est;              /* background noise estimate for post filter */
    float  prev_Wo;             /* previous frame's pitch estimate           */
    MODEL  prev_model;          /* previous frame's model parameters         */
    float  prev_lsps_[LPC_ORD]; /* previous frame's LSPs                     */
    float  prev_energy;         /* previous frame's LPC energy               */
};

void analyse_one_frame(struct CODEC2 *c2, MODEL *model, short speech[]);
void synthesise_one_frame(struct CODEC2 *c2, short speech[], MODEL *model, float ak[]);

int test1()
{
    FILE   *fin, *fout;
    short   buf[N];
    struct CODEC2 *c2;
    MODEL   model;
    float   ak[LPC_ORD+1];
    float   lsps[LPC_ORD];

    c2 = codec2_create(CODEC2_MODE_2400);

    fin = fopen("../raw/hts1a.raw", "rb");
    assert(fin != NULL);
    fout = fopen("hts1a_test.raw", "wb");
    assert(fout != NULL);

    while(fread(buf, sizeof(short), N, fin) == N) {
	analyse_one_frame(c2, &model, buf);
	speech_to_uq_lsps(lsps, ak, c2->Sn, c2->w, LPC_ORD);
	synthesise_one_frame(c2, buf, &model, ak);
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
    struct  CODEC2 *c2;
    MODEL   model, model_interp;
    float   ak[LPC_ORD+1];
    int     voiced1, voiced2;
    int     lsp_indexes[LPC_ORD];
    int     energy_index;
    int     Wo_index;
    char    *bits;
    int     nbit;
    int     i;
    float   lsps[LPC_ORD];
    float   e;
       
    c2 = codec2_create(CODEC2_MODE_2400);
    bits = (char*)malloc(codec2_bits_per_frame(c2));
    assert(bits != NULL);
    fin = fopen("../raw/hts1a.raw", "rb");
    assert(fin != NULL);
    fout = fopen("hts1a_test.raw", "wb");
    assert(fout != NULL);

    while(fread(buf, sizeof(short), 2*N, fin) == 2*N) {
	/* first 10ms analysis frame - we just want voicing */

	analyse_one_frame(c2, &model, buf);
	voiced1 = model.voiced;

	/* second 10ms analysis frame */

	analyse_one_frame(c2, &model, &buf[N]);
	voiced2 = model.voiced;
    
	Wo_index = encode_Wo(model.Wo);
	e = speech_to_uq_lsps(lsps, ak, c2->Sn, c2->w, LPC_ORD);
	encode_lsps_scalar(lsp_indexes, lsps, LPC_ORD);
	energy_index = encode_energy(e);
	nbit = 0;
	pack((unsigned char*)bits, (unsigned *)&nbit, Wo_index, WO_BITS);
	for(i=0; i<LPC_ORD; i++) {
	    pack((unsigned char*)bits, (unsigned *)&nbit, lsp_indexes[i], lsp_bits(i));
	}
	pack((unsigned char*)bits, (unsigned *)&nbit, energy_index, E_BITS);
	pack((unsigned char*)bits, (unsigned *)&nbit, voiced1, 1);
	pack((unsigned char*)bits, (unsigned *)&nbit, voiced2, 1);
 
	nbit = 0;
	Wo_index = unpack((unsigned char*)bits, (unsigned *)&nbit, WO_BITS);
	for(i=0; i<LPC_ORD; i++) {
	    lsp_indexes[i] = unpack((unsigned char*)bits, (unsigned *)&nbit, lsp_bits(i));
	}
	energy_index = unpack((unsigned char*)bits, (unsigned *)&nbit, E_BITS);
	voiced1 = unpack((unsigned char*)bits, (unsigned *)&nbit, 1);
	voiced2 = unpack((unsigned char*)bits, (unsigned *)&nbit, 1);

	model.Wo = decode_Wo(Wo_index);
	model.L = PI/model.Wo;
	decode_amplitudes(&model, 
			  ak,
			  lsp_indexes,
			  energy_index,
			  lsps,
			  &e);

	model.voiced = voiced2;
	model_interp.voiced = voiced1;
	interpolate(&model_interp, &c2->prev_model, &model);

	synthesise_one_frame(c2,  buf,     &model_interp, ak);
	synthesise_one_frame(c2, &buf[N],  &model, ak);

	memcpy(&c2->prev_model, &model, sizeof(MODEL));
	fwrite(buf, sizeof(short), 2*N, fout);
    }

    free(bits);
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
    char   *bits;
    struct CODEC2 *c2;

    c2 = codec2_create(CODEC2_MODE_2400);
    int numBits  = codec2_bits_per_frame(c2);
    int numBytes = (numBits+7)>>3;

    bits = (char*)malloc(numBytes);

    fin = fopen("../raw/hts1a.raw", "rb");
    assert(fin != NULL);
    fout = fopen("hts1a_test.raw", "wb");
    assert(fout != NULL);
    fbits = fopen("hts1a_test3.bit", "wb");
    assert(fout != NULL);

    while(fread(buf1, sizeof(short), 2*N, fin) == 2*N) {
	codec2_encode(c2, (void*)bits, buf1);
	fwrite(bits, sizeof(char), numBytes, fbits);
	codec2_decode(c2, buf2, (void*)bits);
	fwrite(buf2, sizeof(short), numBytes, fout);
    }

    free(bits);
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

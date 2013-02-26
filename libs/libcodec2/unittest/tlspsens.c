/*---------------------------------------------------------------------------*\
                                                                          
  FILE........: tlspsens.c                                                  
  AUTHOR......: David Rowe                                            
  DATE CREATED: 31 May 2012
                                                               
  Testing bit error sensitivity of LSP bits, first step in devising an unequal
  error protection scheme.
                                                              
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2012 David Rowe

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
#include "codec2_internal.h"

float run_a_test(char raw_file_name[], int bit_to_corrupt)
{
    FILE   *fin;
    short   buf[N];
    struct  CODEC2 *c2;
    kiss_fft_cfg  fft_fwd_cfg;
    MODEL   model;
    float   ak[LPC_ORD+1];
    float   lsps[LPC_ORD], e;
    int     lsp_indexes[LPC_ORD], found_bit;
    float   snr, snr_sum;
    int     frames, i, mask, index;

    c2 = codec2_create(CODEC2_MODE_2400);
    fft_fwd_cfg = kiss_fft_alloc(FFT_ENC, 0, NULL, NULL);

    fin = fopen(raw_file_name, "rb");
    assert(fin != NULL);

    /* find bit we are corrupting */

    found_bit = 0;
    for(i=0; i<LSP_SCALAR_INDEXES; i++) {
	if (!found_bit) {
	    if (bit_to_corrupt > lsp_bits(i))
		bit_to_corrupt -= lsp_bits(i);
	    else {
		index = i;
		mask = (1 << bit_to_corrupt);
		printf(" index: %d bit: %d mask: 0x%x ", index, bit_to_corrupt, mask);
		found_bit = 1;
	    }
	}
    }
    assert(found_bit == 1);

    /* OK test a sample file, flipping bit */

    snr_sum = 0.0;
    frames = 0;
    while(fread(buf, sizeof(short), N, fin) == N) {
	analyse_one_frame(c2, &model, buf);
	e = speech_to_uq_lsps(lsps, ak, c2->Sn, c2->w, LPC_ORD);
	encode_lsps_scalar(lsp_indexes, lsps, LPC_ORD);

	/* find and flip bit we are testing */

	lsp_indexes[index] ^= mask;

	/* decode LSPs and measure SNR */

	decode_lsps_scalar(lsps, lsp_indexes, LPC_ORD);
	check_lsp_order(lsps, LPC_ORD);
	bw_expand_lsps(lsps, LPC_ORD);
	lsp_to_lpc(lsps, ak, LPC_ORD);
	aks_to_M2(fft_fwd_cfg, ak, LPC_ORD, &model, e, &snr, 0, 0, 1, 1, LPCPF_BETA, LPCPF_GAMMA); 
	snr_sum += snr;
	frames++;
    }

    codec2_destroy(c2);

    fclose(fin);

    return snr_sum/frames;
}
 
int main(int argc, char *argv[]) {
    int   i;
    int   total_lsp_bits = 0;
    float snr;

    if (argc != 2) {
	printf("usage: %s RawFile\n", argv[0]);
	exit(1);
    }

    for(i=0; i<LPC_ORD; i++)
	total_lsp_bits += lsp_bits(i);

    for(i=0; i<total_lsp_bits; i++) {
	snr = run_a_test(argv[1], i);
	printf("%d %5.2f\n", i, snr);
    }

    return 0;
}

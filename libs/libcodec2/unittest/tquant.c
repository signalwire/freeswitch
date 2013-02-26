/*---------------------------------------------------------------------------*\
                                                                          
  FILE........: tquant.c                                                  
  AUTHOR......: David Rowe                                            
  DATE CREATED: 22/8/10                                        
                                                               
  Generates quantisation curves for plotting on Octave.
                                                                   
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
#include "dump.h"
#include "quantise.h"

int test_Wo_quant();
int test_lsp_quant();
int test_lsp(int lsp_number, int levels, float max_error_hz);
int test_energy_quant(int levels, float max_error_dB);

int main() {
    quantise_init();
    test_Wo_quant();
    test_lsp_quant();
    test_energy_quant(E_LEVELS, 0.5*(E_MAX_DB - E_MIN_DB)/E_LEVELS);

    return 0;
}

int test_lsp_quant() {
    test_lsp( 1, 16,  12.5);
    test_lsp( 2, 16,  12.5);
    test_lsp( 3, 16,  25);
    test_lsp( 4, 16,  50);
    test_lsp( 5, 16,  50);
    test_lsp( 6, 16,  50);
    test_lsp( 7, 16,  50);
    test_lsp( 8,  8,  50);
    test_lsp( 9,  8,  50);
    test_lsp(10,  4, 100);

    return 0;
}

int test_energy_quant(int levels, float max_error_dB) {
    FILE  *fe;
    float  e,e_dec, error, low_e, high_e;
    int    index, index_in, index_out, i;

    /* check 1:1 match between input and output levels */

    for(i=0; i<levels; i++) {
	index_in = i;
	e = decode_energy(index_in);
	index_out = encode_energy(e);
	if (index_in != index_out) {
	    printf("edB: %f index_in: %d index_out: %d\n", 
		   10.0*log10(e), index_in, index_out);
	    exit(0);
	}	
    }

    /* check error over range of quantiser */

    low_e = decode_energy(0);
    high_e = decode_energy(levels-1);
    fe = fopen("energy_err.txt", "wt");

    for(e=low_e; e<high_e; e +=(high_e-low_e)/1000.0) {
	index = encode_energy(e);
	e_dec = decode_energy(index);
	error = 10.0*log10(e) - 10.0*log10(e_dec);
	fprintf(fe, "%f\n", error);
	if (fabs(error) > max_error_dB) {
	    printf("error: %f %f\n", error, max_error_dB);
	    exit(0);
	}
    }

    fclose(fe);
    return 0;
}

int test_lsp(int lsp_number, int levels, float max_error_hz) {
    float lsp[LPC_ORD];
    int   indexes_in[LPC_ORD];
    int   indexes_out[LPC_ORD];
    int   indexes[LPC_ORD];
    int   i;
    float lowf, highf, f, error;
    char  s[MAX_STR];
    FILE *flsp;
    float max_error_rads;

    lsp_number--;
    max_error_rads = max_error_hz*TWO_PI/FS;
    
    for(i=0; i<LPC_ORD; i++)
	indexes_in[i] = 0;

    for(i=0; i<levels; i++) {
	indexes_in[lsp_number] = i;
	decode_lsps_scalar(lsp, indexes_in, LPC_ORD);
	encode_lsps_scalar(indexes_out, lsp,LPC_ORD);
	if (indexes_in[lsp_number] != indexes_out[lsp_number]) {
	    printf("freq: %f index_in: %d index_out: %d\n", 
		   lsp[lsp_number]+1, indexes_in[lsp_number],
		   indexes_out[lsp_number]);
	    exit(0);
	}	
    }

    for(i=0; i<LPC_ORD; i++)
	indexes[i] = 0;
    indexes[lsp_number] = 0;
    decode_lsps_scalar(lsp, indexes, LPC_ORD);
    lowf = lsp[lsp_number];
    indexes[lsp_number] = levels - 1;
    decode_lsps_scalar(lsp, indexes, LPC_ORD);
    highf = lsp[lsp_number];
    sprintf(s,"lsp%d_err.txt", lsp_number+1);
    flsp = fopen(s, "wt");

    for(f=lowf; f<highf; f +=(highf-lowf)/1000.0) {
	lsp[lsp_number] = f;
	encode_lsps_scalar(indexes, lsp, LPC_ORD);
	decode_lsps_scalar(lsp, indexes, LPC_ORD);
	error = f - lsp[lsp_number];
	fprintf(flsp, "%f\n", error);
	if (fabs(error) > max_error_rads) {
	    printf("%d error: %f %f\n", lsp_number+1, error, max_error_rads);
	    exit(0);
	}
    }

    fclose(flsp);

    printf("OK\n");

    return 0;
}

int test_Wo_quant() {
    int    c;
    FILE  *f;
    float  Wo,Wo_dec, error, step_size;
    int    index, index_in, index_out;

    /* output Wo quant curve for plotting */

    f = fopen("quant_pitch.txt","wt");

    for(Wo=0.9*(TWO_PI/P_MAX); Wo<=1.1*(TWO_PI/P_MIN); Wo += 0.001) {
	index = encode_Wo(Wo);
	fprintf(f, "%f %d\n", Wo, index);
    }

    fclose(f);

    /* check for all Wo codes we get 1:1 match between encoder
       and decoder Wo levels */

    for(c=0; c<WO_LEVELS; c++) {
	index_in = c;
	Wo = decode_Wo(index_in);
        index_out = encode_Wo(Wo);
	if (index_in != index_out)
	    printf("  Wo %f index_in %d index_out %d\n", Wo, 
		   index_in, index_out);
    }

    /* measure quantisation error stats and compare to expected.  Also
       plot histogram of error file to check. */

    f = fopen("quant_pitch_err.txt","wt");
    step_size = ((TWO_PI/P_MIN) - (TWO_PI/P_MAX))/WO_LEVELS;

    for(Wo=TWO_PI/P_MAX; Wo<0.99*TWO_PI/P_MIN; Wo += 0.0001) {
	index = encode_Wo(Wo);
	Wo_dec = decode_Wo(index);
	error = Wo - Wo_dec;
	if (fabs(error) > (step_size/2.0)) {
	    printf("error: %f  step_size/2: %f\n", error, step_size/2.0);
	    exit(0);
	}
	fprintf(f,"%f\n",error);
    }
    printf("OK\n");

    fclose(f);
    return 0;
}

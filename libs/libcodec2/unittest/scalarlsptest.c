/*---------------------------------------------------------------------------*\
                                                                           
  FILE........: scalarlsptest.c   
  AUTHOR......: David Rowe                                                      
  DATE CREATED: 8/2/12                                                   
                                                                          
  Test Scalar LSP quantiser, output variance of quantisation error.
                                                                          
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
#include <stdio.h>
#include <stdlib.h>

#include "defines.h"
#include "quantise.h"

/*---------------------------------------------------------------------------*\
                                                                            
                                MAIN 
                                   
\*---------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    FILE *ftrain;       /* LSP training data text file */
    float lsp[LPC_ORD];    /* LSP input vector in rads */
    float lsp_hz[LPC_ORD]; /* LSP input vector in Hz */
    int vectors;           /* number LSP vectors processed */
    int k,m;             /* LSP vector order and codebook size */     
    int    index;
    float  wt[1];        /* weighting (not used here for scalars) */
    const float *cb;           /* LSP quantiser codebook */
    int i, ret;               
    float  total_se; 

    if (argc < 2) {
	printf("usage: %s InputFile\n", argv[0]);
	exit(1);
    }

    if ((ftrain = fopen(argv[1],"rt")) == NULL) {
	printf("Error opening input file: %s\n",argv[1]);
	exit(0);
    }

    total_se = 0.0;
    vectors = 0;
    wt[0] = 1.0;
 
    /* Main loop */

    while(!feof(ftrain)) {

	/* Read LSP input vector speech */

	for (i=0; i<LPC_ORD; i++) {
	    ret = fscanf(ftrain, "%f ", &lsp[i]);
	}
	vectors++;
	if ((vectors % 1000) == 0)
	    printf("\r%d vectors", vectors);

	/* convert from radians to Hz so we can use human readable
	   frequencies */

	for(i=0; i<LPC_ORD; i++)
	    lsp_hz[i] = (4000.0/PI)*lsp[i];
    
	/* simple uniform scalar quantisers */

	for(i=0; i<LPC_ORD; i++) {
	    k = lsp_cb[i].k;                                                                 
	    m = lsp_cb[i].m;
	    cb = lsp_cb[i].cb;
	    index = quantise(cb, &lsp_hz[i], wt, k, m, &total_se);
	    //printf("k %d m %d lsp[%d] %f %f se %f\n", k,m,i,lsp_hz[i], cb[index],se);
	}
	//printf("total se %f\n", total_se);
	//exit(0);
    }

    fclose(ftrain);

    printf("\n variance = %f\n", ((PI*PI)/(4000.0*4000.0))*total_se/vectors);

    return 0;
}


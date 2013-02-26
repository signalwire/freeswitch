/*--------------------------------------------------------------------------*\

	FILE........: vqtrainjnd.c
	AUTHOR......: David Rowe
	DATE CREATED: 10 Nov 2011

	This program trains vector quantisers for LSPs using an
	experimental, but very simple Just Noticable Difference (JND)
	algorithm:

        - we quantise each training vector to JND steps (say 100Hz for LSPs
          5-10) 
	- we then use the most popular training vectors as our VQ codebook

\*--------------------------------------------------------------------------*/

/*
  Copyright (C) 2011 David Rowe

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

/*-----------------------------------------------------------------------*\

				INCLUDES

\*-----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/*-----------------------------------------------------------------------*\

				DEFINES

\*-----------------------------------------------------------------------*/

#define PI         3.141592654	/* mathematical constant                */
#define MAX_POP    10

/*-----------------------------------------------------------------------*\

			FUNCTION PROTOTYPES

\*-----------------------------------------------------------------------*/

void zero(float v[], int k);
void acc(float v1[], float v2[], int k);
void norm(float v[], int k, long n);
void locate_lsps_jnd_steps(float lsps[], float step, int k);

/*-----------------------------------------------------------------------* \

				MAIN

\*-----------------------------------------------------------------------*/

int main(int argc, char *argv[]) {
    int     k;		/* dimension and codebook size			*/
    float  *vec;	/* current vector 				*/
    int    *n;		/* number of vectors in this interval		*/
    int     J;		/* number of vectors in training set		*/
    int     i,j;
    FILE   *ftrain;	/* file containing training set			*/
    float  *train;      /* training database                            */
    //float  *pend_train; /* last entry                                   */
    float  *pt;
    int     ntrain, match, vec_exists, vec_index=0, entry;
    int     popular[MAX_POP], pop_thresh;
    FILE   *fvq;
    float   jnd;

    /* Interpret command line arguments */

    if (argc != 6)	{
	printf("usage: %s TrainFile K(dimension) JND popThresh VQFile\n", 
	       argv[0]);
	exit(1);
    }

    /* Open training file */

    ftrain = fopen(argv[1],"rb");
    if (ftrain == NULL) {
	printf("Error opening training database file: %s\n",argv[1]);
	exit(1);
    }

    /* determine k and m, and allocate arrays */

    k = atol(argv[2]);
    jnd = atof(argv[3]);
    pop_thresh = atol(argv[4]);
    printf("dimension K=%d  popThresh=%d JND=%3.1f Hz\n", 
	   k, pop_thresh, jnd);
    vec = (float*)malloc(sizeof(float)*k);
    if (vec == NULL) {
	printf("Error in malloc.\n");
	exit(1);
    }

    /* determine size of training set */

    J = 0;
    while(fread(vec, sizeof(float), k, ftrain) == (size_t)k)
	J++;
    printf("J=%d entries in training set\n", J);
    train = (float*)malloc(sizeof(float)*k*J);
    if (train == NULL) {
	printf("Error in malloc.\n");
	exit(1);
    }
    printf("training array is %d bytes\n", sizeof(float)*k*J);

    n = (int*)malloc(sizeof(int)*J);
    if (n == NULL) {
	printf("Error in malloc.\n");
	exit(1);
    }
    for(i=0; i<J; i++)
	n[i] = 0;

    /* now load up train data base and quantise */

    rewind(ftrain);
    ntrain = 0;
    entry = 0;
    while(fread(vec, sizeof(float), k, ftrain) == (size_t)k) {

	/* convert to Hz */

	for(j=0; j<k; j++)
	    vec[j] *= 4000.0/PI;
	
	/* quantise to JND steps */

	locate_lsps_jnd_steps(vec, jnd, k);

	/* see if a match already exists in database */

	pt = train;
	vec_exists = 0;
	for(i=0; i<ntrain; i++) {
	    match = 1;
	    for(j=0; j<k; j++)
		if (vec[j] != pt[j])
		    match = 0;
	    if (match) {
		vec_exists = 1;
		vec_index = i;
	    }
	    pt += k;
	}

	if (vec_exists)
	    n[vec_index]++;
	else {
	    /* add to database */

	    for(j=0; j<k; j++) {
		train[ntrain*k + j] = vec[j];
	    }
	    ntrain++;

	}
	entry++;
	if ((entry % 100) == 0)
	    printf("\rtrain input vectors: %d unique vectors: %d",
		   entry, ntrain);
    }
    printf("\n");

    for(i=0; i<MAX_POP; i++)
	popular[i] = 0;
    for(i=0; i<ntrain; i++) {
	if (n[i] < MAX_POP)
	    popular[n[i]]++;
    }

    for(i=0; i<MAX_POP; i++)
	printf("popular[%d] = %d\n", i, popular[i]);

    /* dump result */

    fvq = fopen(argv[5],"wt");
    if (fvq == NULL) {
	printf("Error opening VQ file: %s\n",argv[4]);
	exit(1);
    }
    
    fprintf(fvq,"%d %d\n", k, popular[pop_thresh]);
    for(i=0; i<ntrain; i++) {
	if (n[i] > pop_thresh) {
	    for(j=0; j<k; j++)
		fprintf(fvq, "%4.1f  ",train[i*k+j]);
	    fprintf(fvq,"\n");
	}
    }
    fclose(fvq);
   
    return 0;
}

/*-----------------------------------------------------------------------*\

				FUNCTIONS

\*-----------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*\
                                                       
  FUNCTION....: locate_lsps_jnd_steps()	     
  AUTHOR......: David Rowe			      
  DATE CREATED: 27/10/2011 

  Applies a form of Bandwidth Expansion (BW) to a vector of LSPs.
  Listening tests have determined that "quantising" the position of
  each LSP (say to 100Hz steps for LSPs 5..10) introduces a "just
  noticable difference" in the synthesised speech.

  This operation can be used before quantisation to limit the input
  data to the quantiser to a number of discrete steps.

\*---------------------------------------------------------------------------*/

void locate_lsps_jnd_steps(float lsps[], float step, int k)
{
    int   i;

    for(i=0; i<k; i++) {
	lsps[i] = floor(lsps[i]/step + 0.5)*step;
	if (i) {
	    if (lsps[i] == lsps[i-1])
		lsps[i] += step;

	}
    }

}


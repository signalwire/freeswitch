/*--------------------------------------------------------------------------*\

	FILE........: vqtrainph.c
	AUTHOR......: David Rowe
	DATE CREATED: 27 July 2012

	This program trains phase vector quantisers.  Modified from
	vqtrain.c

\*--------------------------------------------------------------------------*/

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

/*-----------------------------------------------------------------------*\

				INCLUDES

\*-----------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>

typedef struct {
    float real;
    float imag;
} COMP;

/*-----------------------------------------------------------------------* \

				DEFINES

\*-----------------------------------------------------------------------*/

#define	DELTAQ 	0.01		/* quiting distortion			*/
#define	MAX_STR	80		/* maximum string length		*/
#define PI      3.141592654

/*-----------------------------------------------------------------------*\

			FUNCTION PROTOTYPES

\*-----------------------------------------------------------------------*/

void zero(COMP v[], int d);
void acc(COMP v1[], COMP v2[], int d);
void norm(COMP v[], int k);
int quantise(COMP cb[], COMP vec[], int d, int e, float *se);
void print_vec(COMP cb[], int d, int e);

/*-----------------------------------------------------------------------* \

				MAIN

\*-----------------------------------------------------------------------*/

int main(int argc, char *argv[]) {
    int    d,e;		/* dimension and codebook size			*/
    COMP   *vec;	/* current vector 				*/
    COMP   *cb;		/* vector codebook				*/
    COMP   *cent;	/* centroids for each codebook entry		*/
    int    *n;		/* number of vectors in this interval		*/
    int     J;		/* number of vectors in training set		*/
    int     ind;	/* index of current vector			*/
    float   se;	        /* total squared error for this iteration       */
    float   var;        /* variance                                     */ 
    float   var_1;	/* previous variance            	        */
    float   delta;	/* improvement in distortion 			*/
    FILE   *ftrain;	/* file containing training set			*/
    FILE   *fvq;	/* file containing vector quantiser		*/
    int     ret;
    int     i,j, finished, iterations;
    float   b;          /* equivalent number of bits                    */
    float   improvement;
    float   sd_vec, sd_element, sd_theory, bits_theory;
    int     var_n;

    /* Interpret command line arguments */

    if (argc != 5)	{
	printf("usage: %s TrainFile D(dimension) E(number of entries) VQFile\n", argv[0]);
	exit(1);
    }

    /* Open training file */

    ftrain = fopen(argv[1],"rb");
    if (ftrain == NULL) {
	printf("Error opening training database file: %s\n",argv[1]);
	exit(1);
    }

    /* determine k and m, and allocate arrays */

    d = atoi(argv[2]);
    e = atoi(argv[3]);
    printf("\n");
    printf("dimension D=%d  number of entries E=%d\n", d, e);
    vec = (COMP*)malloc(sizeof(COMP)*d);
    cb = (COMP*)malloc(sizeof(COMP)*d*e);
    cent = (COMP*)malloc(sizeof(COMP)*d*e);
    n = (int*)malloc(sizeof(int)*e);
    if (cb == NULL || cb == NULL || cent == NULL || vec == NULL) {
	printf("Error in malloc.\n");
	exit(1);
    }

    /* determine size of training set */

    J = 0;
    var_n = 0;
    while(fread(vec, sizeof(COMP), d, ftrain) == (size_t)d) {
	for(j=0; j<d; j++)
	    if ((vec[j].real != 0.0) && (vec[j].imag != 0.0))
		var_n++;
	J++;
    }
    printf("J=%d sparse vectors in training set, %d non-zero phases\n", J, var_n);

    /* set up initial codebook state from samples of training set */

    rewind(ftrain);
    ret = fread(cb, sizeof(COMP), d*e, ftrain);

    /* codebook can't have any zero phase angle entries, these need to be set to
       zero angle so cmult used to find phase angle differences works */

    for(i=0; i<d*e; i++)
	if ((cb[i].real == 0.0) && (cb[i].imag == 0.0)) {
	    cb[i].real = 1.0;
	    cb[i].imag = 0.0;
	}
	    
    //print_vec(cb, d, 1);

    /* main loop */

    printf("\n");
    printf("Iteration  delta  var    std dev\n");
    printf("--------------------------------\n");

    b = log10((float)e)/log10(2.0);
    sd_theory = (PI/sqrt(3.0))*pow(2.0, -b/(float)d);

    iterations = 0;
    finished = 0;
    delta = 0;
    var_1 = 0.0;

    do {
	/* zero centroids */

	for(i=0; i<e; i++) {
	    zero(&cent[i*d], d);
	    n[i] = 0;
	}

	/* quantise training set */

	se = 0.0;
	rewind(ftrain);
	for(i=0; i<J; i++) {
	    ret = fread(vec, sizeof(COMP), d, ftrain);
	    ind = quantise(cb, vec, d, e, &se);
	    //printf("%d ", ind);
	    n[ind]++;
	    acc(&cent[ind*d], vec, d);
	}
	
	/* work out stats */

	var = se/var_n;	
	sd_vec = sqrt(var);

	/* we need to know dimension of cb (which varies from vector to vector) 
           to calc bits_theory.  Maybe measure and use average dimension....
	*/
	//bits_theory = d*log10(PI/(sd_element*sqrt(3.0)))/log10(2.0);
	//improvement = bits_theory - b;

	//print_vec(cent, d, 1);

	//print_vec(cb, d, 1);

	iterations++;
	if (iterations > 1) {
	    if (var > 0.0) {
		delta = (var_1 - var)/var;
	    }
	    else
		delta = 0;
	    if (delta < DELTAQ)
		finished = 1;
	}      
		     
	if (!finished) {
	    /* determine new codebook from centroids */

	    for(i=0; i<e; i++) {
		norm(&cent[i*d], d);
		memcpy(&cb[i*d], &cent[i*d], d*sizeof(COMP));
	    }
	}

	printf("%2d         %4.3f  %4.3f  %4.3f \n",iterations, delta, var, sd_vec);
	
	var_1 = var;
    } while (!finished);


    //print_vec(cb, d, 1);
    
    /* save codebook to disk */

    fvq = fopen(argv[4],"wt");
    if (fvq == NULL) {
	printf("Error opening VQ file: %s\n",argv[4]);
	exit(1);
    }

    fprintf(fvq,"%d %d\n",d,e);
    for(j=0; j<e; j++) {
	for(i=0; i<d; i++)
	    fprintf(fvq,"% 4.3f ", atan2(cb[j*d+i].imag, cb[j*d+i].real));
	fprintf(fvq,"\n");
    }
    fclose(fvq);

    return 0;
}

/*-----------------------------------------------------------------------*\

				FUNCTIONS

\*-----------------------------------------------------------------------*/

void print_vec(COMP cb[], int d, int e)
{
    int i,j;

    for(j=0; j<e; j++) {
	for(i=0; i<d; i++) 
	    printf("%f %f ", cb[j*d+i].real, cb[j*d+i].imag);
	printf("\n");
    }
}


static COMP cconj(COMP a)
{
    COMP res;

    res.real = a.real;
    res.imag = -a.imag;

    return res;
}

static COMP cmult(COMP a, COMP b)
{
    COMP res;

    res.real = a.real*b.real - a.imag*b.imag;
    res.imag = a.real*b.imag + a.imag*b.real;

    return res;
}

static COMP cadd(COMP a, COMP b)
{
    COMP res;

    res.real = a.real + b.real;
    res.imag = a.imag + b.imag;

    return res;
}

/*---------------------------------------------------------------------------*\

	FUNCTION....: zero()

	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	Zeros a vector of length d.

\*---------------------------------------------------------------------------*/

void zero(COMP v[], int d)
{
    int	i;

    for(i=0; i<d; i++) {
	v[i].real = 0.0;
	v[i].imag = 0.0;
    }
}

/*---------------------------------------------------------------------------*\

	FUNCTION....: acc()

	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	Adds d dimensional vectors v1 to v2 and stores the result back
	in v1.  We add them like vectors on the complex plane, summing
	the real and imag terms.  

	An unused entry in a sparse vector has both the real and imag
	parts set to zero so won't affect the accumulation process.

\*---------------------------------------------------------------------------*/

void acc(COMP v1[], COMP v2[], int d)
{
    int	   i;

    for(i=0; i<d; i++)
	v1[i] = cadd(v1[i], v2[i]);
}

/*---------------------------------------------------------------------------*\

	FUNCTION....: norm()

	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	Normalises each element in d dimensional vector.

\*---------------------------------------------------------------------------*/

void norm(COMP v[], int d)
{
    int	   i;
    float  mag;

    for(i=0; i<d; i++) {
	mag = sqrt(v[i].real*v[i].real + v[i].imag*v[i].imag);
	if (mag == 0.0) {
	    /* can't have zero cb entries as cmult will break in quantise().
	       We effectively set sparese phases to an angle of 0. */
	    v[i].real = 1.0;
	    v[i].imag = 0.0;
	}
	else {
	    v[i].real /= mag;
	    v[i].imag /= mag;
	}
    }
}

/*---------------------------------------------------------------------------*\

	FUNCTION....: quantise()

	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	Quantises vec by choosing the nearest vector in codebook cb, and
	returns the vector index.  The squared error of the quantised vector
	is added to se.  

	Unused entries in sparse vectors are ignored.

\*---------------------------------------------------------------------------*/

int quantise(COMP cb[], COMP vec[], int d, int e, float *se)
{
   float   error;	/* current error		*/
   int     besti;	/* best index so far		*/
   float   best_error;	/* best error so far		*/
   int	   i,j;
   int     ignore;
   COMP    diff;

   besti = 0;
   best_error = 1E32;
   for(j=0; j<e; j++) {
	error = 0.0;
	for(i=0; i<d; i++) {
	    ignore = (vec[i].real == 0.0) && (vec[i].imag == 0.0);
	    if (!ignore) {
		diff = cmult(cb[j*d+i], cconj(vec[i]));
		error += pow(atan2(diff.imag, diff.real), 2.0);
	    }
	}
	if (error < best_error) {
	    best_error = error;
	    besti = j;
	}
   }

   *se += best_error;

   return(besti);
}


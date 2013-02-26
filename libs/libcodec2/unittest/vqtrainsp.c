/*--------------------------------------------------------------------------*\

	FILE........: vqtrainsp.c
	AUTHOR......: David Rowe
	DATE CREATED: 7 August 2012

	This program trains sparse amplitude vector quantisers.
	Modified from vqtrainph.c

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

/*-----------------------------------------------------------------------*\

			FUNCTION PROTOTYPES

\*-----------------------------------------------------------------------*/

void zero(float v[], int d);
void acc(float v1[], float v2[], int d);
void norm(float v[], int k, int n[]);
int quantise(float cb[], float vec[], int d, int e, float *se);
void print_vec(float cb[], int d, int e);
void split(float cb[], int d, int b);
int gain_shape_quantise(float cb[], float vec[], int d, int e, float *se, float *best_gain);

/*-----------------------------------------------------------------------* \

				MAIN

\*-----------------------------------------------------------------------*/

int main(int argc, char *argv[]) {
    int    d,e;		/* dimension and codebook size			*/
    float  *vec;	/* current vector 				*/
    float  *cb;		/* vector codebook				*/
    float  *cent;	/* centroids for each codebook entry		*/
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
    float   sd;
    int     var_n, bits, b, levels;

    /* Interpret command line arguments */

    if (argc < 5)	{
	printf("usage: %s TrainFile D(dimension) B(number of bits) VQFile [error.txt file]\n", argv[0]);
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
    bits = atoi(argv[3]);
    e = 1<<bits;
    printf("\n");
    printf("dimension D=%d  number of bits B=%d entries E=%d\n", d, bits, e);
    vec = (float*)malloc(sizeof(float)*d);
    cb = (float*)malloc(sizeof(float)*d*e);
    cent = (float*)malloc(sizeof(float)*d*e);
    n = (int*)malloc(sizeof(int)*d*e);
    if (cb == NULL || cb == NULL || cent == NULL || vec == NULL) {
	printf("Error in malloc.\n");
	exit(1);
    }

    /* determine size of training set */

    J = 0;
    var_n = 0;
    while(fread(vec, sizeof(float), d, ftrain) == (size_t)d) {
	for(j=0; j<d; j++)
	    if (vec[j] != 0.0)
		var_n++;
	J++;
    }
    printf("J=%d sparse vectors in training set, %d non-zero values\n", J, var_n);

    /* set up initial codebook from centroid of training set */

    //#define DBG

    zero(cent, d);
    for(j=0; j<d; j++)
	n[j] = 0;
    rewind(ftrain);
    #ifdef DBG
    printf("initial codebook...\n");
    #endif
    for(i=0; i<J; i++) {
	ret = fread(vec, sizeof(float), d, ftrain);
        #ifdef DBG
	print_vec(vec, d, 1);
	#endif
	acc(cent, vec, d);
	for(j=0; j<d; j++)
	    if (vec[j] != 0.0)
		n[j]++;
    }
    norm(cent, d, n);
    memcpy(cb, cent, d*sizeof(float));
    #ifdef DBG
    printf("\n");
    print_vec(cb, d, 1);
    #endif

    /* main loop */

    printf("\n");
    printf("bits  Iteration  delta  var     std dev\n");
    printf("---------------------------------------\n");

    for(b=1; b<=bits; b++) {
	levels = 1<<b;
	iterations = 0;
	finished = 0;
	delta = 0;
	var_1 = 0.0;

	split(cb, d, levels/2);
	//print_vec(cb, d, levels);

	do {
	    /* zero centroids */

	    for(i=0; i<levels; i++) {
		zero(&cent[i*d], d);
		for(j=0; j<d; j++)
		    n[i*d+j] = 0;
	    }

	    //#define DBG
            #ifdef DBG
	    printf("cb...\n");
	    print_vec(cb, d, levels);
	    printf("\n\nquantise...\n");
            #endif

	    /* quantise training set */

	    se = 0.0;
	    rewind(ftrain);
	    for(i=0; i<J; i++) {
		ret = fread(vec, sizeof(float), d, ftrain);
		ind = quantise(cb, vec, d, levels, &se);
		//ind = gain_shape_quantise(cb, vec, d, levels, &se, &best_gain);
 		//for(j=0; j<d; j++)
		//	    if (vec[j] != 0.0)
		//	vec[j] += best_gain;
                #ifdef DBG
		print_vec(vec, d, 1);
		printf("      ind %d se: %f\n", ind, se);
                #endif
		acc(&cent[ind*d], vec, d);
		for(j=0; j<d; j++)
		    if (vec[j] != 0.0)
			n[ind*d+j]++;
	    }
	
            #ifdef DBG
	    printf("cent...\n");
	    print_vec(cent, d, e);
	    printf("\n");
            #endif

	    /* work out stats */

	    var = se/var_n;	
	    sd = sqrt(var);

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

		for(i=0; i<levels; i++) {
		    norm(&cent[i*d], d, &n[i*d]);
		    memcpy(&cb[i*d], &cent[i*d], d*sizeof(float));
		}
	    }

            #ifdef DBG
	    printf("new cb ...\n");
	    print_vec(cent, d, e);
	    printf("\n");
            #endif

	    printf("%2d    %2d         %4.3f  %6.3f  %4.3f\r",b,iterations, delta, var, sd);
	    fflush(stdout);

	    var_1 = var;
	} while (!finished);
	printf("\n");
    }
    

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
	    fprintf(fvq,"% 7.3f ", cb[j*d+i]);
	fprintf(fvq,"\n");
    }
    fclose(fvq);

    /* optionally dump error file for multi-stage work */

    if (argc == 6) {	
	FILE *ferr = fopen(argv[5],"wt");
	assert(ferr != NULL);	
	rewind(ftrain);
	for(i=0; i<J; i++) {
	    ret = fread(vec, sizeof(float), d, ftrain);
	    ind = quantise(cb, vec, d, levels, &se);
	    for(j=0; j<d; j++) {
		if (vec[j] != 0.0)
		    vec[j] -= cb[ind*d+j];
		fprintf(ferr, "%f ", vec[j]);
	    }
	    fprintf(ferr, "\n");
	}
    }

    return 0;
}

/*-----------------------------------------------------------------------*\

				FUNCTIONS

\*-----------------------------------------------------------------------*/

void print_vec(float cb[], int d, int e)
{
    int i,j;

    for(j=0; j<e; j++) {
	printf("    ");
	for(i=0; i<d; i++) 
	    printf("% 7.3f ", cb[j*d+i]);
	printf("\n");
    }
}


/*---------------------------------------------------------------------------*\

	FUNCTION....: zero()

	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	Zeros a vector of length d.

\*---------------------------------------------------------------------------*/

void zero(float v[], int d)
{
    int	i;

    for(i=0; i<d; i++) {
	v[i] = 0.0;
    }
}

/*---------------------------------------------------------------------------*\

	FUNCTION....: acc()

	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	Adds d dimensional vectors v1 to v2 and stores the result back
	in v1.  

	An unused entry in a sparse vector is set to zero so won't
	affect the accumulation process.

\*---------------------------------------------------------------------------*/

void acc(float v1[], float v2[], int d)
{
    int	   i;

    for(i=0; i<d; i++)
	v1[i] += v2[i];
}

/*---------------------------------------------------------------------------*\

	FUNCTION....: norm()

	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	Normalises each element in d dimensional vector.

\*---------------------------------------------------------------------------*/

void norm(float v[], int d, int n[])
{
    int	   i;

    for(i=0; i<d; i++) {
	if (n[i] != 0)
	    v[i] /= n[i];
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

int quantise(float cb[], float vec[], int d, int e, float *se)
{
   float   error;	/* current error		*/
   int     besti;	/* best index so far		*/
   float   best_error;	/* best error so far		*/
   int	   i,j;
   float   diff;

   besti = 0;
   best_error = 1E32;
   for(j=0; j<e; j++) {
       error = 0.0;
       for(i=0; i<d; i++) {
	   if (vec[i] != 0.0) {
	       diff = cb[j*d+i] - vec[i];
	       error += diff*diff;
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

int gain_shape_quantise(float cb[], float vec[], int d, int e, float *se, float *best_gain)
{
   float   error;	/* current error		*/
   int     besti;	/* best index so far		*/
   float   best_error;	/* best error so far		*/
   int	   i,j,m;
   float   diff, metric, best_metric, gain, sumAm, sumCb;

   besti = 0;
   best_metric = best_error = 1E32;
   for(j=0; j<e; j++) {

       /* compute optimum gain */

       sumAm = sumCb = 0.0;
       m = 0;
       for(i=0; i<d; i++) {
	   if (vec[i] != 0.0) {
	       m++;
	       sumAm += vec[i];
	       sumCb += cb[j*d+i];
	   }
       }
       gain = (sumAm - sumCb)/m;
       
       /* compute error */

       metric = error = 0.0;
       for(i=0; i<d; i++) {
	   if (vec[i] != 0.0) {
	       diff = vec[i] - cb[j*d+i] - gain;
	       error += diff*diff;
	       metric += diff*diff;
	   }
       }
       if (metric < best_metric) {
	   best_error = error;
	   best_metric = metric;
	   *best_gain = gain;
	   besti = j;
       }
   }

   *se += best_error;

   return(besti);
}

void split(float cb[], int d, int levels)
{
    int i,j;

    for (i=0;i<levels;i++) {
	for (j=0;j<d;j++) {
	    float delta = .01*(rand()/(float)RAND_MAX-.5);
	    cb[i*d+j] += delta;
	    cb[(i+levels)*d+j] = cb[i*d+j] - delta;
	}
    }
}


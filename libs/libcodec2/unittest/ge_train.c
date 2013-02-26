/*
 ge_train.c
 Jean Marc Valin Feb 2012

 Joint pitch and energy VQ training program

 usage: 

   cat GE | ./ge_train 2 1000000 8 > quantized

 The first column is the log2 of the pitch compared to the lowest freq,
 so log2(wo/pi*4000/50) where wo is the frequency your patch outputs. The
 second column is the energy in dB, so 10*log10(1e-4+E)
*/

/*
  Copyright (C) 2012 Jean-Marc Valin 

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

#include <valgrind/memcheck.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
//#define COEF 0.0

static float COEF[2] = {0.8, 0.9};
//static float COEF[2] = {0.0, 0.};

#define MAX_ENTRIES 16384

void compute_weights2(const float *x, const float *xp, float *w, int ndim)
{
  w[0] = 30;
  w[1] = 1;
  if (x[1]<0)
  {
     w[0] *= .6;
     w[1] *= .3;
  }
  if (x[1]<-10)
  {
     w[0] *= .3;
     w[1] *= .3;
  }
  /* Higher weight if pitch is stable */
  if (fabs(x[0]-xp[0])<.2)
  {
     w[0] *= 2;
     w[1] *= 1.5;
  } else if (fabs(x[0]-xp[0])>.5) /* Lower if not stable */
  {
     w[0] *= .5;
  }

  /* Lower weight for low energy */
  if (x[1] < xp[1]-10)
  {
     w[1] *= .5;
  }
  if (x[1] < xp[1]-20)
  {
     w[1] *= .5;
  }

  //w[0] = 30;
  //w[1] = 1;
  
  /* Square the weights because it's applied on the squared error */
  w[0] *= w[0];
  w[1] *= w[1];

}

int find_nearest_weighted(const float *codebook, int nb_entries, float *x, const float *w, int ndim)
{
  int i, j;
  float min_dist = 1e15;
  int nearest = 0;
  
  for (i=0;i<nb_entries;i++)
  {
    float dist=0;
    for (j=0;j<ndim;j++)
      dist += w[j]*(x[j]-codebook[i*ndim+j])*(x[j]-codebook[i*ndim+j]);
    if (dist<min_dist)
    {
      min_dist = dist;
      nearest = i;
    }
  }
  return nearest;
}

int quantize_ge(const float *x, const float *codebook1, int nb_entries, float *xq, int ndim)
{
  int i, n1;
  float err[ndim];
  float w[ndim];
  
  compute_weights2(x, xq, w, ndim);
  
  for (i=0;i<ndim;i++)
    err[i] = x[i]-COEF[i]*xq[i];
  n1 = find_nearest_weighted(codebook1, nb_entries, err, w, ndim);
  
  for (i=0;i<ndim;i++)
  {
    xq[i] = COEF[i]*xq[i] + codebook1[ndim*n1+i];
    err[i] -= codebook1[ndim*n1+i];
  }
  return 0;
}

void split(float *codebook, int nb_entries, int ndim)
{
  int i,j;
  for (i=0;i<nb_entries;i++)
  {
    for (j=0;j<ndim;j++)
    {
      float delta = .01*(rand()/(float)RAND_MAX-.5);
      codebook[i*ndim+j] += delta;
      codebook[(i+nb_entries)*ndim+j] = codebook[i*ndim+j] - delta;
    }
  }
}


void update_weighted(float *data, float *weight, int nb_vectors, float *codebook, int nb_entries, int ndim)
{
  int i,j;
  float count[MAX_ENTRIES][ndim];
  int nearest[nb_vectors];
  
  //fprintf(stderr, "weighted: %d %d\n", nb_entries, ndim);
  for (i=0;i<nb_entries;i++)
    for (j=0;j<ndim;j++)
      count[i][j] = 0;
  
  for (i=0;i<nb_vectors;i++)
  {
    nearest[i] = find_nearest_weighted(codebook, nb_entries, data+i*ndim, weight+i*ndim, ndim);
  }
  for (i=0;i<nb_entries*ndim;i++)
    codebook[i] = 0;
  
  for (i=0;i<nb_vectors;i++)
  {
    int n = nearest[i];
    for (j=0;j<ndim;j++)
    {
      float w = sqrt(weight[i*ndim+j]);
      count[n][j]+=w;
      codebook[n*ndim+j] += w*data[i*ndim+j];
    }
  }

  //float w2=0;
  for (i=0;i<nb_entries;i++)
  { 
    for (j=0;j<ndim;j++)
      codebook[i*ndim+j] *= (1./count[i][j]);
    //w2 += (count[i]/(float)nb_vectors)*(count[i]/(float)nb_vectors);
  }
  //fprintf(stderr, "%f / %d\n", 1./w2, nb_entries);
}

void vq_train_weighted(float *data, float *weight, int nb_vectors, float *codebook, int nb_entries, int ndim)
{
  int i, j, e;
  e = 1;
  for (j=0;j<ndim;j++)
    codebook[j] = 0;
  for (i=0;i<nb_vectors;i++)
    for (j=0;j<ndim;j++)
      codebook[j] += data[i*ndim+j];
  for (j=0;j<ndim;j++)
    codebook[j] *= (1./nb_vectors);
  
  
  while (e< nb_entries)
  {
#if 1
    split(codebook, e, ndim);
    e<<=1;
#else
    split1(codebook, e, data, nb_vectors, ndim);
    e++;
#endif
    fprintf(stderr, "%d\n", e);
    for (j=0;j<10;j++)
      update_weighted(data, weight, nb_vectors, codebook, e, ndim);
  }
}


int main(int argc, char **argv)
{
  int i,j;
  int nb_vectors, nb_entries, ndim;
  float *data, *pred, *codebook, *codebook2, *codebook3;
  float *weight, *weight2, *weight3;
  float *delta;
  double err[2] = {0, 0};
  double werr[2] = {0, 0};
  double wsum[2] = {0, 0};
  
  ndim = atoi(argv[1]);
  nb_vectors = atoi(argv[2]);
  nb_entries = 1<<atoi(argv[3]);
  
  data = malloc(nb_vectors*ndim*sizeof(*data));
  weight = malloc(nb_vectors*ndim*sizeof(*weight));
  weight2 = malloc(nb_vectors*ndim*sizeof(*weight2));
  weight3 = malloc(nb_vectors*ndim*sizeof(*weight3));
  pred = malloc(nb_vectors*ndim*sizeof(*pred));
  codebook = malloc(nb_entries*ndim*sizeof(*codebook));
  codebook2 = malloc(nb_entries*ndim*sizeof(*codebook2));
  codebook3 = malloc(nb_entries*ndim*sizeof(*codebook3));
  
  for (i=0;i<nb_vectors;i++)
  {
    if (feof(stdin))
      break;
    for (j=0;j<ndim;j++)
    {
      scanf("%f ", &data[i*ndim+j]);
    }
  }
  nb_vectors = i;
  VALGRIND_CHECK_MEM_IS_DEFINED(data, nb_entries*ndim);

  for (i=0;i<nb_vectors;i++)
  {
    if (i==0)
       compute_weights2(data+i*ndim, data+i*ndim, weight+i*ndim, ndim);
    else
       compute_weights2(data+i*ndim, data+(i-1)*ndim, weight+i*ndim, ndim);
  }
  for (i=0;i<ndim;i++)
    pred[i] = data[i];
  for (i=1;i<nb_vectors;i++)
  {
    for (j=0;j<ndim;j++)
      pred[i*ndim+j] = data[i*ndim+j] - COEF[j]*data[(i-1)*ndim+j];
  }

  VALGRIND_CHECK_MEM_IS_DEFINED(pred, nb_entries*ndim);
  vq_train_weighted(pred, weight, nb_vectors, codebook, nb_entries, ndim);
  printf("%d %d\n", ndim, nb_entries);
  for (i=0;i<nb_entries;i++)
  {
   for (j=0;j<ndim;j++)
    {
      printf("%f ", codebook[i*ndim+j]);
    }
    printf("\n");
  }
  
  delta = malloc(nb_vectors*ndim*sizeof(*data));
  float xq[2] = {0,0};
  for (i=0;i<nb_vectors;i++)
  {
    //int nearest = find_nearest_weighted(codebook, nb_entries, &pred[i*ndim], &weight[i*ndim], ndim);
    quantize_ge(&data[i*ndim], codebook, nb_entries, xq, ndim);
    //printf("%f %f\n", xq[0], xq[1]);
    for (j=0;j<ndim;j++)
    {
      delta[i*ndim+j] = xq[j]-data[i*ndim+j];
      err[j] += (delta[i*ndim+j])*(delta[i*ndim+j]);
      werr[j] += weight[i*ndim+j]*(delta[i*ndim+j])*(delta[i*ndim+j]);
      wsum[j] += weight[i*ndim+j];
      //delta[i*ndim+j] = pred[i*ndim+j] - codebook[nearest*ndim+j];
      //printf("%f ", delta[i*ndim+j]);
      //err[j] += (delta[i*ndim+j])*(delta[i*ndim+j]);
    }
    //printf("\n");
  }
  fprintf(stderr, "GE RMS error: %f %f\n", sqrt(err[0]/nb_vectors), sqrt(err[1]/nb_vectors));
  fprintf(stderr, "Weighted GE error: %f %f\n", sqrt(werr[0]/wsum[0]), sqrt(werr[1]/wsum[1]));

  return 0;
}

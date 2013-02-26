/*---------------------------------------------------------------------------*\
                                                                          
  FILE........: vq_train_jvm.c                                                  
  AUTHOR......: Jean-Marc Valin                                            
  DATE CREATED: 21 Jan 2012
                                                               
  Multi-stage Vector Quantoser training program developed by Jean-Marc at 
  linux.conf.au 2012.  Minor mods by David Rowe
                                                              
\*---------------------------------------------------------------------------*/

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


#ifdef VALGRIND
#include <valgrind/memcheck.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
#define COEF 0.0f
#define MAX_ENTRIES 16384

void compute_weights(const float *x, float *w, int ndim)
{
  int i;
  w[0] = MIN(x[0], x[1]-x[0]);
  for (i=1;i<ndim-1;i++)
    w[i] = MIN(x[i]-x[i-1], x[i+1]-x[i]);
  w[ndim-1] = MIN(x[ndim-1]-x[ndim-2], M_PI-x[ndim-1]);
  
  for (i=0;i<ndim;i++)
    w[i] = 1./(.01+w[i]);
  w[0]*=3;
  w[1]*=2;
}

int find_nearest(const float *codebook, int nb_entries, float *x, int ndim, float *min_dist)
{
  int i, j;
  int nearest = 0;
  
  *min_dist = 1E15;
  
  for (i=0;i<nb_entries;i++)
  {
    float dist=0;
    for (j=0;j<ndim;j++)
      dist += (x[j]-codebook[i*ndim+j])*(x[j]-codebook[i*ndim+j]);
    if (dist<*min_dist)
    {
      *min_dist = dist;
      nearest = i;
    }
  }
  return nearest;
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

int quantize_lsp(const float *x, const float *codebook1, const float *codebook2, 
		 const float *codebook3, int nb_entries, float *xq, int ndim)
{
  int i, n1, n2, n3;
  float err[ndim], err2[ndim], err3[ndim];
  float w[ndim], w2[ndim], w3[ndim], min_dist;
  
  w[0] = MIN(x[0], x[1]-x[0]);
  for (i=1;i<ndim-1;i++)
    w[i] = MIN(x[i]-x[i-1], x[i+1]-x[i]);
  w[ndim-1] = MIN(x[ndim-1]-x[ndim-2], M_PI-x[ndim-1]);
  
  /*
  for (i=0;i<ndim;i++)
    w[i] = 1./(.003+w[i]);
  w[0]*=3;
  w[1]*=2;*/
  compute_weights(x, w, ndim);
  
  for (i=0;i<ndim;i++)
    err[i] = x[i]-COEF*xq[i];
  n1 = find_nearest(codebook1, nb_entries, err, ndim, &min_dist);
  
  for (i=0;i<ndim;i++)
  {
    xq[i] = COEF*xq[i] + codebook1[ndim*n1+i];
    err[i] -= codebook1[ndim*n1+i];
  }
  for (i=0;i<ndim/2;i++)
  {
    err2[i] = err[2*i];  
    err3[i] = err[2*i+1];
    w2[i] = w[2*i];  
    w3[i] = w[2*i+1];
  }
  n2 = find_nearest_weighted(codebook2, nb_entries, err2, w2, ndim/2);
  n3 = find_nearest_weighted(codebook3, nb_entries, err3, w3, ndim/2);
  
  for (i=0;i<ndim/2;i++)
  {
    xq[2*i] += codebook2[ndim*n2/2+i];
    xq[2*i+1] += codebook3[ndim*n3/2+i];
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

void update(float *data, int nb_vectors, float *codebook, int nb_entries, int ndim)
{
  int i,j;
  int count[nb_entries];
  int nearest[nb_vectors];
  float min_dist;
  float total_min_dist = 0;

  for (i=0;i<nb_entries;i++)
    count[i] = 0;
  
  for (i=0;i<nb_vectors;i++)
  {
      nearest[i] = find_nearest(codebook, nb_entries, data+i*ndim, ndim, &min_dist);
      total_min_dist += min_dist;
  }
  for (i=0;i<nb_entries*ndim;i++)
    codebook[i] = 0;
  
  for (i=0;i<nb_vectors;i++)
  {
    int n = nearest[i];
    count[n]++;
    for (j=0;j<ndim;j++)
      codebook[n*ndim+j] += data[i*ndim+j];
  }

  float w2=0;
  for (i=0;i<nb_entries;i++)
  { 
    for (j=0;j<ndim;j++)
      codebook[i*ndim+j] *= (1./count[i]);
    w2 += (count[i]/(float)nb_vectors)*(count[i]/(float)nb_vectors);
  }
  fprintf(stderr, "%f / %d var = %f\n", 1./w2, nb_entries, total_min_dist/nb_vectors );
}

void update_weighted(float *data, float *weight, int nb_vectors, float *codebook, int nb_entries, int ndim)
{
  int i,j;
  float count[MAX_ENTRIES][ndim];
  int nearest[nb_vectors];
  
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

void vq_train(float *data, int nb_vectors, float *codebook, int nb_entries, int ndim)
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
    split(codebook, e, ndim);
    fprintf(stderr, "%d\n", e);
    e<<=1;
    for (j=0;j<ndim;j++)
      update(data, nb_vectors, codebook, e, ndim);
  }
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
   
  while (e<nb_entries)
  {
    split(codebook, e, ndim);
    fprintf(stderr, "%d\n", e);
    e<<=1;
    for (j=0;j<ndim;j++)
      update_weighted(data, weight, nb_vectors, codebook, e, ndim);
  }
}


int main(int argc, char **argv)
{
  int i,j;
  FILE *ftrain;
  int nb_vectors, nb_entries, ndim;
  float *data, *pred, *codebook, *codebook2, *codebook3;
  float *weight, *weight2, *weight3;
  float *delta, *delta2;
  float tmp, err, min_dist, total_min_dist;
  int ret;
  char filename[256];
  FILE *fcb;

  printf("Jean-Marc Valin's Split VQ training program....\n");

  if (argc != 5) {
      printf("usage: %s TrainTextFile K(dimension) M(codebook size) VQFilesPrefix\n", argv[0]);
      exit(1);      
  }
  
  ndim = atoi(argv[2]);
  nb_vectors = atoi(argv[3]);
  nb_entries = atoi(argv[3]);

  /* determine size of training file */

  ftrain = fopen(argv[1],"rt");  assert(ftrain != NULL);
  nb_vectors = 0;
  while (1) {
    if (feof(ftrain))
      break;
    for (j=0;j<ndim;j++)
    {
	ret = fscanf(ftrain, "%f ", &tmp);
    }
    nb_vectors++;
    if ((nb_vectors % 1000) == 0)
	printf("\r%d lines",nb_vectors);
  }

  rewind(ftrain);

  printf("\nndim %d nb_vectors %d nb_entries %d\n", ndim, nb_vectors, nb_entries);

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
    if (feof(ftrain))
      break;
    for (j=0;j<ndim;j++)
    {
	ret = fscanf(ftrain, "%f ", &data[i*ndim+j]);
    }
  }
  nb_vectors = i;

#ifdef VALGRIND
  VALGRIND_CHECK_MEM_IS_DEFINED(data, nb_entries*ndim);
#endif

  /* determine weights for each training vector */

  for (i=0;i<nb_vectors;i++)
  {
    compute_weights(data+i*ndim, weight+i*ndim, ndim);
    for (j=0;j<ndim/2;j++)
    {
      weight2[i*ndim/2+j] = weight[i*ndim+2*j];
      weight3[i*ndim/2+j] = weight[i*ndim+2*j+1];
    }
  }

  /* 20ms (two frame gaps) initial predictor state */

  for (i=0;i<ndim;i++) {
    pred[i+ndim] = pred[i] = data[i] - M_PI*(i+1)/(ndim+1);
  }

  /* generate predicted data for training */

  for (i=2;i<nb_vectors;i++)
  {
    for (j=0;j<ndim;j++)
      pred[i*ndim+j] = data[i*ndim+j] - COEF*data[(i-2)*ndim+j];
  }

#ifdef VALGRIND
  VALGRIND_CHECK_MEM_IS_DEFINED(pred, nb_entries*ndim);
#endif

  /* train first stage */

  vq_train(pred, nb_vectors, codebook, nb_entries, ndim);
  
  delta = malloc(nb_vectors*ndim*sizeof(*data));
  err = 0;
  total_min_dist = 0;
  for (i=0;i<nb_vectors;i++)
  {
      int nearest = find_nearest(codebook, nb_entries, &pred[i*ndim], ndim, &min_dist);
      total_min_dist += min_dist;
    for (j=0;j<ndim;j++)
    {
      //delta[i*ndim+j] = data[i*ndim+j] - codebook[nearest*ndim+j];
      //printf("%f ", delta[i*ndim+j]);
      //err += (delta[i*ndim+j])*(delta[i*ndim+j]);
      delta[i*ndim/2+j/2+(j&1)*nb_vectors*ndim/2] = pred[i*ndim+j] - codebook[nearest*ndim+j];
      //printf("%f ", delta[i*ndim/2+j/2+(j&1)*nb_vectors*ndim/2]);
      err += (delta[i*ndim/2+j/2+(j&1)*nb_vectors*ndim/2])*(delta[i*ndim/2+j/2+(j&1)*nb_vectors*ndim/2]);
    }
    //printf("\n");
  }
  fprintf(stderr, "Stage 1 LSP RMS error: %f\n", sqrt(err/nb_vectors/ndim));
  fprintf(stderr, "Stage 1 LSP variance.: %f\n", total_min_dist/nb_vectors);
  
#if 1
  vq_train(delta, nb_vectors, codebook2, nb_entries, ndim/2);
  vq_train(delta+ndim*nb_vectors/2, nb_vectors, codebook3, nb_entries, ndim/2);
#else
  vq_train_weighted(delta, weight2, nb_vectors, codebook2, nb_entries, ndim/2);
  vq_train_weighted(delta+ndim*nb_vectors/2, weight3, nb_vectors, codebook3, nb_entries, ndim/2);
#endif

  err = 0;
  total_min_dist = 0; 
 
  delta2 = delta + nb_vectors*ndim/2;

  for (i=0;i<nb_vectors;i++)
  {
    int n1, n2;
    n1 = find_nearest(codebook2, nb_entries, &delta[i*ndim/2], ndim/2, &min_dist);
    for (j=0;j<ndim/2;j++)
    {
      delta[i*ndim/2+j] = delta[i*ndim/2+j] - codebook2[n1*ndim/2+j];
      err += (delta[i*ndim/2+j])*(delta[i*ndim/2+j]);
    }
    total_min_dist += min_dist;

    n2 = find_nearest(codebook3, nb_entries, &delta2[i*ndim/2], ndim/2, &min_dist);
    for (j=0;j<ndim/2;j++)
    {
      delta[i*ndim/2+j] = delta[i*ndim/2+j] - codebook2[n2*ndim/2+j];
      err += (delta2[i*ndim/2+j])*(delta2[i*ndim/2+j]);
    }
    total_min_dist += min_dist;
  }
  fprintf(stderr, "Stage 2 LSP RMS error: %f\n", sqrt(err/nb_vectors/ndim));
  fprintf(stderr, "Stage 2 LSP Variance.: %f\n", total_min_dist/nb_vectors);
  
  float xq[ndim];
  for (i=0;i<ndim;i++)
    xq[i] = M_PI*(i+1)/(ndim+1);
  
  for (i=0;i<nb_vectors;i++)
  {
    quantize_lsp(data+i*ndim, codebook, codebook2, 
		 codebook3, nb_entries, xq, ndim);
    /*for (j=0;j<ndim;j++)
      printf("%f ", xq[j]);
    printf("\n");*/
  }
  
  /* save output tables to text files */

  sprintf(filename, "%s1.txt", argv[4]);
  fcb = fopen(filename, "wt"); assert(fcb != NULL);
  fprintf(fcb, "%d %d\n", ndim, nb_entries);
  for (i=0;i<nb_entries;i++)
  {
    for (j=0;j<ndim;j++)
	fprintf(fcb, "%f ", codebook[i*ndim+j]);
    fprintf(fcb, "\n");
  }
  fclose(fcb);

  sprintf(filename, "%s2.txt", argv[4]);
  fcb = fopen(filename, "wt"); assert(fcb != NULL);
  fprintf(fcb, "%d %d\n", ndim/2, nb_entries);
  for (i=0;i<nb_entries;i++)
  {
    for (j=0;j<ndim/2;j++)
	fprintf(fcb, "%f ", codebook2[i*ndim/2+j]);
    fprintf(fcb, "\n");
  }
  fclose(fcb);

  sprintf(filename, "%s3.txt", argv[4]);
  fcb = fopen(filename, "wt"); assert(fcb != NULL);
  fprintf(fcb, "%d %d\n", ndim/2, nb_entries);
  for (i=0;i<nb_entries;i++)
  {
    for (j=0;j<ndim/2;j++)
      fprintf(fcb, "%f ", codebook3[i*ndim/2+j]);
    fprintf(fcb, "\n");
  }
  fclose(fcb);

  return 0;
}

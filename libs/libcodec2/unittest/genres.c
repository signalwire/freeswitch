/*---------------------------------------------------------------------------*\
                                                                           
  FILE........: genres.c   
  AUTHOR......: David Rowe                                                      
  DATE CREATED: 24/8/09                                                   
                                                                          
  Generates a file of LPC residual samples from original speech.
                                                                          
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2009 David Rowe

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

#include <stdio.h>
#include <stdlib.h>
#include <lpc.h>

#define N 160
#define P 10

int main(int argc, char *argv[])
{
  FILE *fin,*fres;      /* input and output files */
  short buf[N];         /* buffer of 16 bit speech samples */
  float Sn[P+N];        /* input speech samples */
  float res[N];         /* residual after LPC filtering */
  float E;
  float ak[P+1];        /* LP coeffs */

  int frames;           /* frames processed so far */
  int i;                /* loop variables */

  if (argc < 3) {
    printf("usage: %s InputFile ResidualFile\n", argv[0]);
    exit(1);
  }

  /* Open files */

  if ((fin = fopen(argv[1],"rb")) == NULL) {
    printf("Error opening input file: %s\n",argv[1]);
    exit(0);
  }

  if ((fres = fopen(argv[2],"wb")) == NULL) {
    printf("Error opening output residual file: %s\n",argv[2]);
    exit(0);
  }

  /* Initialise */

  frames = 0;
  for(i=0; i<P; i++) {
    Sn[i] = 0.0;
  }

  /* Main loop */

  while( (fread(buf,sizeof(short),N,fin)) == N) {
    frames++;
    for(i=0; i<N; i++)
      Sn[P+i] = (float)buf[i];

    /* Determine {ak} and filter to find residual */

    find_aks(&Sn[P], ak, N, P, &E);
    inverse_filter(&Sn[P], ak, N, res, P);
    for(i=0; i<N; i++)
      buf[i] = (short)res[i];
    fwrite(buf,sizeof(short),N,fres);
  }

  fclose(fin);
  fclose(fres);

  return 0;
}

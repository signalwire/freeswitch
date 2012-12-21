/*--------------------------------------------------------------------------*\

	FILE........: extract.c
	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	This program extracts a float file of vectors from a text file
	of vectors.  The float files are easier to process quickly
	during VQ training.  A subset of the text file VQ may be
	extracted to faciltate split VQ of scaler VQ design.

\*--------------------------------------------------------------------------*/

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

#define	MAX_STR	2048		/* maximum string length		*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

void scan_line(FILE *fp, float f[], int n);

int main(int argc, char *argv[]) {
    FILE   *ftext;	/* text file of vectors 			*/
    FILE   *ffloat;	/* float file of vectors			*/
    int    st,en;	/* start and end values of vector to copy	*/
    float  *buf;	/* ptr to vector read from ftext		*/
    long   lines;	/* lines read so far				*/

    if (argc != 5) {
	printf("usage: %s TextFile FloatFile start(1 .. 10) end(1 .. 10)\n", argv[0]);
	exit(1);
    }

    /* read command line arguments and open files */

    ftext = fopen(argv[1],"rt");
    if (ftext == NULL) {
	printf("Error opening text file: %s\n",argv[1]);
	exit(1);
    }

    ffloat = fopen(argv[2],"wb");
    if (ffloat == NULL) {
	printf("Error opening float file: %s\n",argv[2]);
	exit(1);
    }

    st = atoi(argv[3]);
    en = atoi(argv[4]);

    buf = (float*)malloc(en*sizeof(float));
    if (buf == NULL) {
	printf("Error in malloc()\n");
	exit(1);
    }

    lines = 0;
    while(!feof(ftext)) {
	scan_line(ftext, buf, en);
	if (!feof(ftext)) {
	    fwrite(&buf[st-1], sizeof(float), en-st+1, ffloat);
	    printf("\r%ld lines",++lines);
	}
    }
    printf("\n");

    /* clean up and exit */

    free(buf);
    fclose(ftext);
    fclose(ffloat);

    return 0;
}

/*---------------------------------------------------------------------------*\

	FUNCTION....: scan_line()

	AUTHOR......: David Rowe
	DATE CREATED: 20/2/95

	This function reads a vector of floats from a line in a text file.

\*---------------------------------------------------------------------------*/

void scan_line(FILE *fp, float f[], int n)
/*  FILE   *fp;		file ptr to text file 		*/
/*  float  f[]; 	array of floats to return 	*/
/*  int    n;		number of floats in line 	*/
{
    char   s[MAX_STR];
    char   *ps,*pe;
    int	   i;
    
    memset(s, 0, MAX_STR);
    ps = pe = fgets(s,MAX_STR,fp); 
    if (ps == NULL)
	return;
    for(i=0; i<n; i++) {
	while( isspace(*pe)) pe++;
	while( !isspace(*pe)) pe++;
	sscanf(ps,"%f",&f[i]);
	ps = pe;
    }
}


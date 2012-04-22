/*--------------------------------------------------------------------------*\

	FILE........: genlsp.c
	AUTHOR......: David Rowe
	DATE CREATED: 23/2/95

	This program genrates a text file of LSP vectors from an input
	speech file.

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
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define P 	10	/* LP order					*/
#define LSP_DELTA1 0.05 /* grid spacing for LSP root searches */
#define NW	279	/* frame size in samples 			*/
#define	N  	80 	/* frame to frame shift				*/
#define THRESH	40.0	/* threshold energy/sample for frame inclusion 	*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lpc.h"	/* LPC analysis functions 			*/
#include "lsp.h"	/* LSP encode/decode functions 			*/

int switch_present(sw,argc,argv)
  char sw[];     /* switch in string form */
  int argc;      /* number of command line arguments */
  char *argv[];  /* array of command line arguments in string form */
{
  int i;       /* loop variable */

  for(i=1; i<argc; i++)
    if (!strcmp(sw,argv[i]))
      return(i);

  return 0;
}

int main(int argc, char *argv[]) {
    FILE   *fspc;	/* input file ptr for test database		*/
    FILE   *flsp;	/* output text file of LSPs 			*/
    short  buf[N];	/* input frame of speech samples 		*/
    float  Sn[NW];	/* float input speech samples 			*/
    float  ak[P+1];	/* LPCs for current frame 			*/
    float  lsp[P];	/* LSPs for current frame 			*/
    float  E;		/* frame energy 				*/
    long   af;		/* number frames with "active" speech 		*/
    float  Eres;	/* LPC residual energy 				*/
    int    i;
    int    roots;
    int    unstables;
    int    lspd;

    /* Initialise ------------------------------------------------------*/

    if (argc < 3) {
	printf("usage: gentest RawFile LSPTextFile [--lspd]\n");
	exit(0);
    }

    /* Open files */

    fspc = fopen(argv[1],"rb");
    if (fspc == NULL) {
	printf("Error opening input SPC file: %s",argv[1]);
	exit(1);
    }

    flsp = fopen(argv[2],"wt");
    if (flsp == NULL) {
	printf("Error opening output LSP file: %s",argv[2]);
	exit(1);
    }

    lspd = switch_present("--lspd", argc, argv);

    for(i=0; i<NW; i++)
	Sn[i] = 0.0;

    /* Read SPC file, and determine aks[] for each frame ------------------*/

    af = 0;
    unstables = 0;
    while(fread(buf,sizeof(short),N,fspc) == N) {

	for(i=0; i<NW-N; i++)
	    Sn[i] = Sn[i+N];
	E = 0.0;
	for(i=0; i<N; i++) {
	    Sn[i+NW-N] = buf[i];
	    E += Sn[i]*Sn[i];
	}

	E = 0.0;
	for(i=0; i<NW; i++) {
	    E += Sn[i]*Sn[i];
	}
	E = 10.0*log10(E/NW);

	/* If energy high enough, include this frame */

	if (E > THRESH) {
	    af++;
	    printf("Active Frame: %ld  unstables: %d\n",af, unstables);

	    find_aks(Sn, ak, NW, P, &Eres);
	    roots = lpc_to_lsp(&ak[1], P , lsp, 5, LSP_DELTA1);
	    if (roots == P) {
		if (lspd) {
		    fprintf(flsp,"%f ",lsp[0]);
		    for(i=1; i<P; i++)
			fprintf(flsp,"%f ",lsp[i]-lsp[i-1]);
		    fprintf(flsp,"\n");
		}
		else {
		    for(i=0; i<P; i++)
			fprintf(flsp,"%f ",lsp[i]);
		    fprintf(flsp,"\n");
		}
	    }
	    else 
		unstables++;
	}
    }

    fclose(fspc);
    fclose(flsp);

    return 0;
}


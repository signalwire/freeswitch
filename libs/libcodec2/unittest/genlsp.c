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
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#define P 	12	/* LP order					*/
#define LSP_DELTA1 0.01 /* grid spacing for LSP root searches */
#define NW	279	/* frame size in samples 			*/
#define	N  	80 	/* frame to frame shift				*/
#define THRESH	40.0	/* threshold energy/sample for frame inclusion 	*/
#define PI         3.141592654	/* mathematical constant                */

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
    float  lsp_prev[P];	/* LSPs for previous frame 			*/
    float  E;		/* frame energy 				*/
    long   f;		/* number of frames                             */
    long   af;		/* number frames with "active" speech 		*/
    float  Eres;	/* LPC residual energy 				*/
    int    i;
    int    roots;
    int    unstables;
    int    lspd, log, lspdt;
    float  diff;

    /* Initialise ------------------------------------------------------*/

    if (argc < 3) {
	printf("usage: %s RawFile LSPTextFile [--lspd] [--log] [--lspdt] \n", argv[0]);
	exit(1);
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
    log = switch_present("--log", argc, argv);
    lspdt = switch_present("--lspdt", argc, argv);

    for(i=0; i<NW; i++)
	Sn[i] = 0.0;

    /* Read SPC file, and determine aks[] for each frame ------------------*/

    f = af = 0;
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

	f++;
	if (E > THRESH) {
	    af++;
	    printf("Active Frame: %ld  unstables: %d\n",af, unstables);

	    find_aks(Sn, ak, NW, P, &Eres);
	    roots = lpc_to_lsp(ak, P , lsp, 5, LSP_DELTA1);
	    if (roots == P) {
		if (lspd) {
		    if (log) {
			fprintf(flsp,"%f ",log10(lsp[0]));
			for(i=1; i<P; i++) {
			    diff = lsp[i]-lsp[i-1];
			    if (diff < (PI/4000.0)*25.0) diff = (PI/4000.0)*25.0;
			    fprintf(flsp,"%f ",log10(diff));
			}
		    } 
		    else {
			fprintf(flsp,"%f ",lsp[0]);
			for(i=1; i<P; i++)
			    fprintf(flsp,"%f ",lsp[i]-lsp[i-1]);
		    }

		    fprintf(flsp,"\n");
		    
		}
		else if (lspdt) {
		    for(i=0; i<P; i++)
			fprintf(flsp,"%f ",lsp[i]-lsp_prev[i]);
		    fprintf(flsp,"\n");
		    
		}
		else {
		    if (log) {
			for(i=0; i<P; i++)
			    fprintf(flsp,"%f ",log10(lsp[i]));
			fprintf(flsp,"\n");
		    }
		    else {
			for(i=0; i<P; i++)
			    fprintf(flsp,"%f ",lsp[i]);
			fprintf(flsp,"\n");
		    }

		}		
		memcpy(lsp_prev, lsp, sizeof(lsp));
	    }
	    else 
		unstables++;
	}
    }

    printf("%3.2f %% active frames\n", 100.0*(float)af/f);
    fclose(fspc);
    fclose(flsp);

    return 0;
}


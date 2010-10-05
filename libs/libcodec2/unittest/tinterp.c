/*---------------------------------------------------------------------------*\
                                                                          
  FILE........: tinterp.c                                                  
  AUTHOR......: David Rowe                                            
  DATE CREATED: 22/8/10                                        
                                                               
  Tests interpolation functions.
                                                                   
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2010 David Rowe

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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "defines.h"
#include "sine.h"
#include "interp.h"

void make_amp(MODEL *model, float f0, float cdB, float mdBHz)
{
    int   i;
    float mdBrad = mdBHz*FS/TWO_PI;

    model->Wo = f0*TWO_PI/FS;
    model->L  = PI/model->Wo;
    for(i=0; i<=model->L; i++)
	model->A[i] = pow(10.0,(cdB + (float)i*model->Wo*mdBrad)/20.0);
    model->voiced = 1;
}

void write_amp(char file[], MODEL *model)
{
    FILE  *f;
    int    i;

    f = fopen(file,"wt");
    for(i=1; i<=model->L; i++)
	fprintf(f, "%f\t%f\n", model->Wo*i, model->A[i]);
    fclose(f);
}

char *get_next_float(char *s, float *num)
{
    char *p = s;
    char  tmp[MAX_STR];

    while(*p && !isspace(*p)) 
	p++;
    memcpy(tmp, s, p-s);
    tmp[p-s] = 0;
    *num = atof(tmp);

    return p+1;
}

char *get_next_int(char *s, int *num)
{
    char *p = s;
    char  tmp[MAX_STR];

    while(*p && !isspace(*p)) 
	p++;
    memcpy(tmp, s, p-s);
    tmp[p-s] = 0;
    *num = atoi(tmp);

    return p+1;
}

void load_amp(MODEL *model, char file[], int frame)
{
    FILE *f;
    int   i;
    char  s[1024];
    char *ps;

    f = fopen(file,"rt");

    for(i=0; i<frame; i++)
	fgets(s, 1023, f);

    ps = s;
    ps = get_next_float(ps, &model->Wo);
    ps = get_next_int(ps, &model->L);
    for(i=1; i<=model->L; i++)
	ps = get_next_float(ps, &model->A[i]);
	
    fclose(f);
}

int main() {
    MODEL  prev, next, interp;

    //make_amp(&prev, 50.0, 60.0, 6E-3);
    //make_amp(&next, 50.0, 40.0, 6E-3);
    load_amp(&prev, "../src/hts1a_model.txt", 32);
    load_amp(&next, "../src/hts1a_model.txt", 34);

    interp.voiced = 1;
    interpolate(&interp, &prev, &next);

    write_amp("tinterp_prev.txt", &prev);
    write_amp("tinterp_interp.txt", &interp);
    write_amp("tinterp_next.txt", &next);

    return 0;
}

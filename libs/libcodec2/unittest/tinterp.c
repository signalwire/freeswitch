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
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

const char *get_next_float(const char *s, float *num)
{
    const char *p = s;
    char  tmp[MAX_STR];

    while(*p && !isspace(*p)) 
	p++;
    assert((p-s) < (int)(sizeof(tmp)-1));
    memcpy(tmp, s, p-s);
    tmp[p-s] = 0;
    *num = atof(tmp);

    return p+1;
}

const char *get_next_int(const char *s, int *num)
{
    const char *p = s;
    char  tmp[MAX_STR];

    while(*p && !isspace(*p)) 
	p++;
    assert((p-s) < (int)(sizeof(tmp)-1));
    memcpy(tmp, s, p-s);
    tmp[p-s] = 0;
    *num = atoi(tmp);

    return p+1;
}

void load_amp(MODEL *model, const char * file, int frame)
{
    FILE *f;
    int   i;
    char  s[1024];
    const char *ps;

    f = fopen(file,"rt");
    assert(f);

    for(i=0; i<frame; i++)
	ps = fgets(s, 1023, f);

    /// can frame ever be 0? what if fgets fails?
    ps = s;
    ps = get_next_float(ps, &model->Wo);
    ps = get_next_int(ps, &model->L);
    for(i=1; i<=model->L; i++)
	ps = get_next_float(ps, &model->A[i]);
	
    fclose(f);
}

void load_or_make_amp(MODEL *model, 
                      const char * filename, int frame,
                      float f0, float cdB, float mdBHz)
{
    struct stat buf;
    int rc = stat(filename, &buf);
    if (rc || !S_ISREG(buf.st_mode) || ((buf.st_mode & S_IRUSR) != S_IRUSR))
    {
        make_amp(model, f0, cdB, mdBHz);
    }
    else
    {
        load_amp(model, filename, frame);
    }
}
int main() {
    MODEL  prev, next, interp;

    load_or_make_amp(&prev,
                     "../src/hts1a_model.txt", 32,
                     50.0, 60.0, 6E-3);
    load_or_make_amp(&next,
                     "../src/hts1a_model.txt", 34,
                     50.0, 40.0, 6E-3);

    interp.voiced = 1;
    interpolate(&interp, &prev, &next);

    write_amp("tinterp_prev.txt", &prev);
    write_amp("tinterp_interp.txt", &interp);
    write_amp("tinterp_next.txt", &next);

    return 0;
}

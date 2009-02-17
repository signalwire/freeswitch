/*
 * iLBC - a library for the iLBC codec
 *
 * hpOutput.c - The iLBC low bit rate speech codec.
 *
 * Adapted by Steve Underwood <steveu@coppice.org> from the reference
 * iLBC code supplied in RFC3951.
 *
 * Original code Copyright (C) The Internet Society (2004).
 * All changes to produce this version Copyright (C) 2008 by Steve Underwood
 * All Rights Reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: hpOutput.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>

#include "constants.h"
#include "hpOutput.h"

/*----------------------------------------------------------------*
 *  Output high-pass filter
 *---------------------------------------------------------------*/

void hpOutput(float *In,    /* (i) vector to filter */
              int len,      /* (i) length of vector to filter */
              float *Out,   /* (o) the resulting filtered vector */
              float *mem)   /* (i/o) the filter state */
{
    int i;
    float *pi;
    float *po;

    /* all-zero section*/
    pi = &In[0];
    po = &Out[0];
    for (i = 0;  i < len;  i++)
    {
        *po = hpo_zero_coefsTbl[0]*(*pi);
        *po += hpo_zero_coefsTbl[1]*mem[0];
        *po += hpo_zero_coefsTbl[2]*mem[1];

        mem[1] = mem[0];
        mem[0] = *pi;
        po++;
        pi++;
    }

    /* all-pole section*/
    po = &Out[0];
    for (i = 0;  i < len;  i++)
    {
        *po -= hpo_pole_coefsTbl[1]*mem[2];
        *po -= hpo_pole_coefsTbl[2]*mem[3];

        mem[3] = mem[2];
        mem[2] = *po;
        po++;
    }
}

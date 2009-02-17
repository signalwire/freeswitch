/*
 * iLBC - a library for the iLBC codec
 *
 * anaFilter.c - The iLBC low bit rate speech codec.
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
 * $Id: anaFilter.c,v 1.2 2008/03/06 12:27:37 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <string.h>

#include "anaFilter.h"
#include "ilbc.h"

/*----------------------------------------------------------------*
 *  LP analysis filter.
 *---------------------------------------------------------------*/
void anaFilter(float *In,   /* (i) Signal to be filtered */
               float *a,    /* (i) LP parameters */
               int len,     /* (i) Length of signal */
               float *Out,  /* (o) Filtered signal */
               float *mem)  /* (i/o) Filter state */
{
    int i;
    int j;
    float *po;
    float *pi;
    float *pm;
    float *pa;

    po = Out;
    /* Filter first part using memory from past */
    for (i = 0;  i < ILBC_LPC_FILTERORDER;  i++)
    {
        pi = &In[i];
        pm = &mem[ILBC_LPC_FILTERORDER - 1];
        pa = a;
        *po = 0.0;

        for (j = 0;  j <= i;  j++)
            *po += (*pa++)*(*pi--);
        for (j = i + 1;  j < ILBC_LPC_FILTERORDER + 1;  j++)
            *po += (*pa++)*(*pm--);
        po++;
    }
    /* Filter last part where the state is entirely
       in the input vector */
    for (i = ILBC_LPC_FILTERORDER;  i < len;  i++)
    {
        pi = &In[i];
        pa = a;
        *po = 0.0;
        for (j = 0;  j < ILBC_LPC_FILTERORDER + 1;  j++)
            *po += (*pa++)*(*pi--);
        po++;
    }
    /* Update state vector */
    memcpy(mem, &In[len - ILBC_LPC_FILTERORDER], ILBC_LPC_FILTERORDER*sizeof(float));
}

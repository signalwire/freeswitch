/*
 * iLBC - a library for the iLBC codec
 *
 * syntFilter.c - The iLBC low bit rate speech codec.
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
 * $Id: syntFilter.c,v 1.2 2008/03/06 12:27:38 steveu Exp $
 */

/*! \file */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <inttypes.h>
#include <string.h>

#include "ilbc.h"
#include "syntFilter.h"

/*----------------------------------------------------------------*
 *  LP synthesis filter.
 *---------------------------------------------------------------*/

void syntFilter(float *Out,     /* (i/o) Signal to be filtered */
                float *a,       /* (i) LP parameters */
                int len,        /* (i) Length of signal */
                float *mem)     /* (i/o) Filter state */
{
    int i;
    int j;
    float *po;
    float *pi;
    float *pa;
    float *pm;

    po = Out;

    /* Filter first part using memory from past */
    for (i = 0;  i < ILBC_LPC_FILTERORDER;  i++)
    {
        pi = &Out[i - 1];
        pa = &a[1];
        pm = &mem[ILBC_LPC_FILTERORDER - 1];
        for (j = 1;  j <= i;  j++)
            *po -= (*pa++)*(*pi--);
        for (j = i + 1;  j < ILBC_LPC_FILTERORDER + 1;  j++)
            *po -= (*pa++)*(*pm--);
        po++;
    }

    /* Filter last part where the state is entirely in
       the output vector */

    for (i = ILBC_LPC_FILTERORDER;  i < len;  i++)
    {
        pi = &Out[i - 1];
        pa = &a[1];
        for (j = 1;  j < ILBC_LPC_FILTERORDER + 1;  j++)
            *po -= (*pa++)*(*pi--);
        po++;
    }

    /* Update state vector */
    memcpy(mem, &Out[len - ILBC_LPC_FILTERORDER], ILBC_LPC_FILTERORDER*sizeof(float));
}

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bert_tests.c - Tests for the BER tester.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: bert_tests.c,v 1.28 2009/05/30 15:23:13 steveu Exp $
 */

/*! \file */

/*! \page bert_tests_page BERT tests
\section bert_tests_page_sec_1 What does it do?
These tests exercise each of the BERT standards supported by the BERT module.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"

/* Use a local random generator, so the results are consistent across platforms */
static int my_rand(void)
{
    static int rndnum = 1234567;

    return (rndnum = 1664525U*rndnum + 1013904223U);
}
/*- End of function --------------------------------------------------------*/

static void reporter(void *user_data, int reason, bert_results_t *results)
{
    int channel;

    channel = (int) (intptr_t) user_data;
    printf("BERT report '%s' ", bert_event_to_str(reason));
    switch (reason)
    {
    case BERT_REPORT_REGULAR:
        printf("%d bits, %d bad bits, %d resyncs", results->total_bits, results->bad_bits, results->resyncs);
        break;
    }
    printf("\r");
}
/*- End of function --------------------------------------------------------*/

int8_t test[0x800000];

int main(int argc, char *argv[])
{
    bert_state_t tx_bert;
    bert_state_t rx_bert;
    bert_state_t bert;
    bert_results_t bert_results;
    int i;
    int bit;
    int zeros;
    int max_zeros;
    int failed;

    bert_init(&tx_bert, 0, BERT_PATTERN_ZEROS, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ZEROS, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    bert_result(&rx_bert, &bert_results);
    printf("Zeros:     Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 950)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_ONES, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ONES, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    bert_result(&rx_bert, &bert_results);
    printf("Ones:      Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 950)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_1_TO_7, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_1_TO_7, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    bert_result(&rx_bert, &bert_results);
    printf("1 to 7:    Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 950)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_1_TO_3, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_1_TO_3, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    bert_result(&rx_bert, &bert_results);
    printf("1 to 3:    Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 950)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_1_TO_1, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_1_TO_1, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    bert_result(&rx_bert, &bert_results);
    printf("1 to 1:    Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 950)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_3_TO_1, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_3_TO_1, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    bert_result(&rx_bert, &bert_results);
    printf("3 to 1:    Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 950)
    {
        printf("Test failed.\n");
        exit(2);
    }
    
    bert_init(&tx_bert, 0, BERT_PATTERN_7_TO_1, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_7_TO_1, 300, 20);
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);
    }
    bert_result(&rx_bert, &bert_results);
    printf("7 to 1:    Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 950)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O153_9, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O153_9, 300, 20);
    for (i = 0;  i < 0x200;  i++)
        test[i] = 0;
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 511*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx.reg]++;
    }
    failed = FALSE;
    if (test[0] != 0)
    {
        printf("XXX %d %d\n", 0, test[0]);
        failed = TRUE;
    }
    for (i = 1;  i < 0x200;  i++)
    {
        if (test[i] != 2)
        {
            printf("XXX %d %d\n", i, test[i]);
            failed = TRUE;
        }
    }
    bert_result(&rx_bert, &bert_results);
    printf("O.153(9):  Bad bits %d/%d, max zeros %d\n", bert_results.bad_bits, bert_results.total_bits, max_zeros);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 986  ||  failed)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O152_11, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O152_11, 300, 20);
    for (i = 0;  i < 0x800;  i++)
        test[i] = 0;
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 2047*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx.reg]++;
    }
    failed = FALSE;
    if (test[0] != 0)
    {
        printf("XXX %d %d\n", 0, test[0]);
        failed = TRUE;
    }
    for (i = 1;  i < 0x800;  i++)
    {
        if (test[i] != 2)
        {
            printf("XXX %d %d\n", i, test[i]);
            failed = TRUE;
        }
    }
    bert_result(&rx_bert, &bert_results);
    printf("O.152(11): Bad bits %d/%d, max zeros %d\n", bert_results.bad_bits, bert_results.total_bits, max_zeros);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 4052  ||  failed)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O151_15, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O151_15, 300, 20);
    for (i = 0;  i < 0x8000;  i++)
        test[i] = 0;
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 32767*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx.reg]++;
    }
    failed = FALSE;
    if (test[0] != 0)
    {
        printf("XXX %d %d\n", 0, test[0]);
        failed = TRUE;
    }
    for (i = 1;  i < 0x8000;  i++)
    {
        if (test[i] != 2)
        {
            printf("XXX %d %d\n", i, test[i]);
            failed = TRUE;
        }
    }
    bert_result(&rx_bert, &bert_results);
    printf("O.151(15): Bad bits %d/%d, max zeros %d\n", bert_results.bad_bits, bert_results.total_bits, max_zeros);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 65480  ||  failed)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O151_20, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O151_20, 300, 20);
    for (i = 0;  i < 0x100000;  i++)
        test[i] = 0;    
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 1048575*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx.reg]++;
    }
    failed = FALSE;
    if (test[0] != 0)
    {
        printf("XXX %d %d\n", 0, test[0]);
        failed = TRUE;
    }
    for (i = 1;  i < 0x100000;  i++)
    {
        if (test[i] != 2)
            printf("XXX %d %d\n", i, test[i]);
    }
    bert_result(&rx_bert, &bert_results);
    printf("O.151(20): Bad bits %d/%d, max zeros %d\n", bert_results.bad_bits, bert_results.total_bits, max_zeros);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 2097066  ||  failed)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_ITU_O151_23, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_ITU_O151_23, 300, 20);
    for (i = 0;  i < 0x800000;  i++)
        test[i] = 0;    
    max_zeros = 0;
    zeros = 0;
    for (i = 0;  i < 8388607*2;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        if (bit)
        {
            if (zeros > max_zeros)
                max_zeros = zeros;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        bert_put_bit(&rx_bert, bit);        
        test[tx_bert.tx.reg]++;
    }
    failed = FALSE;
    if (test[0] != 0)
    {
        printf("XXX %d %d\n", 0, test[0]);
        failed = TRUE;
    }
    for (i = 1;  i < 0x800000;  i++)
    {
        if (test[i] != 2)
            printf("XXX %d %d\n", i, test[i]);
    }
    bert_result(&rx_bert, &bert_results);
    printf("O.151(23): Bad bits %d/%d, max zeros %d\n", bert_results.bad_bits, bert_results.total_bits, max_zeros);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 16777136  ||  failed)
    {
        printf("Test failed.\n");
        exit(2);
    }

    bert_init(&tx_bert, 0, BERT_PATTERN_QBF, 300, 20);
    bert_init(&rx_bert, 0, BERT_PATTERN_QBF, 300, 20);
    for (i = 0;  i < 100000;  i++)
    {
        bit = bert_get_bit(&tx_bert);
        bert_put_bit(&rx_bert, bit);        
    }
    bert_result(&rx_bert, &bert_results);
    printf("QBF:       Bad bits %d/%d\n", bert_results.bad_bits, bert_results.total_bits);
    if (bert_results.bad_bits  ||  bert_results.total_bits != 100000)
    {
        printf("Test failed.\n");
        exit(2);
    }

    /* Test the mechanism for categorising the error rate into <10^x bands */
    /* TODO: The result of this test is not checked automatically */
    bert_init(&bert, 15000000, BERT_PATTERN_ITU_O152_11, 300, 20);
    bert_set_report(&bert, 100000, reporter, (intptr_t) 0);
    for (;;)
    {
        if ((bit = bert_get_bit(&bert)) == SIG_STATUS_END_OF_DATA)
        {
            bert_result(&bert, &bert_results);
            printf("Rate test: %d bits, %d bad bits, %d resyncs\n", bert_results.total_bits, bert_results.bad_bits, bert_results.resyncs);
            if (bert_results.total_bits != 15000000 - 42
                ||
                bert_results.bad_bits != 58
                ||
                bert_results.resyncs != 0)
            {
                printf("Tests failed\n");
                exit(2);
            }
            break;
            //bert_init(&bert, 15000000, BERT_PATTERN_ITU_O152_11, 300, 20);
            //bert_set_report(&bert, 100000, reporter, (intptr_t) 0);
            //continue;
        }
        if ((my_rand() & 0x3FFFF) == 0)
            bit ^= 1;
        //if ((my_rand() & 0xFFF) == 0)
        //    bert_put_bit(&bert, bit);
        bert_put_bit(&bert, bit);
    }
    
    printf("Tests passed.\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

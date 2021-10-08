/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bert.c - Bit error rate tests.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
 *
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 2.1,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/async.h"
#include "spandsp/bert.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/bert.h"

#define MEASUREMENT_STEP    100

static const char *qbf = "VoyeZ Le BricK GeanT QuE J'ExaminE PreS Du WharF 123 456 7890 + - * : = $ % ( )"
                         "ThE QuicK BrowN FoX JumpS OveR ThE LazY DoG 123 456 7890 + - * : = $ % ( )";

SPAN_DECLARE(const char *) bert_event_to_str(int event)
{
    switch (event)
    {
    case BERT_REPORT_SYNCED:
        return "synced";
    case BERT_REPORT_UNSYNCED:
        return "unsync'ed";
    case BERT_REPORT_REGULAR:
        return "regular";
    case BERT_REPORT_GT_10_2:
        return "error rate > 1 in 10^2";
    case BERT_REPORT_LT_10_2:
        return "error rate < 1 in 10^2";
    case BERT_REPORT_LT_10_3:
        return "error rate < 1 in 10^3";
    case BERT_REPORT_LT_10_4:
        return "error rate < 1 in 10^4";
    case BERT_REPORT_LT_10_5:
        return "error rate < 1 in 10^5";
    case BERT_REPORT_LT_10_6:
        return "error rate < 1 in 10^6";
    case BERT_REPORT_LT_10_7:
        return "error rate < 1 in 10^7";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bert_get_bit(bert_state_t *s)
{
    int bit;

    if (s->limit  &&  s->tx.bits >= s->limit)
        return SIG_STATUS_END_OF_DATA;
    bit = 0;
    switch (s->pattern_class)
    {
    case 0:
        bit = s->tx.reg & 1;
        s->tx.reg = (s->tx.reg >> 1) | ((s->tx.reg & 1) << s->shift2);
        break;
    case 1:
        bit = s->tx.reg & 1;
        s->tx.reg = (s->tx.reg >> 1) | (((s->tx.reg ^ (s->tx.reg >> s->shift)) & 1) << s->shift2);
        if (s->max_zeros)
        {
            /* This generator suppresses runs >s->max_zeros */
            if (bit)
            {
                if (++s->tx.zeros > s->max_zeros)
                {
                    s->tx.zeros = 0;
                    bit ^= 1;
                }
            }
            else
            {
                s->tx.zeros = 0;
            }
        }
        bit ^= s->invert;
        break;
    case 2:
        if (s->tx.step_bit == 0)
        {
            s->tx.step_bit = 7;
            s->tx.reg = qbf[s->tx.step++];
            if (s->tx.reg == 0)
            {
                s->tx.reg = 'V';
                s->tx.step = 1;
            }
        }
        bit = s->tx.reg & 1;
        s->tx.reg >>= 1;
        s->tx.step_bit--;
        break;
    }
    s->tx.bits++;
    return bit;
}
/*- End of function --------------------------------------------------------*/

static void assess_error_rate(bert_state_t *s)
{
    int i;
    int j;
    int sum;
    bool test;

    /* We assess the error rate in decadic steps. For each decade we assess the error over 10 times
       the number of bits, to smooth the result. This means we assess the 1 in 100 rate based on 1000 bits
       (i.e. we look for >=10 errors in 1000 bits). We make an assessment every 100 bits, using a sliding
       window over the last 1000 bits. We assess the 1 in 1000 rate over 10000 bits in a similar way, and
       so on for the lower error rates. */
    test = true;
    for (i = 2;  i <= 7;  i++)
    {
        if (++s->decade_ptr[i] < 10)
            break;
        /* This decade has reached 10 snapshots, so we need to touch the next decade */
        s->decade_ptr[i] = 0;
        /* Sum the last 10 snapshots from this decade, to see if we overflow into the next decade */
        for (sum = 0, j = 0;  j < 10;  j++)
            sum += s->decade_bad[i][j];
        if (test  &&  sum > 10)
        {
            /* We overflow into the next decade */
            test = false;
            if (s->error_rate != i  &&  s->reporter)
                s->reporter(s->user_data, BERT_REPORT_GT_10_2 + i - 2, &s->results);
            s->error_rate = i;
        }
        s->decade_bad[i][0] = 0;
        if (i < 7)
            s->decade_bad[i + 1][s->decade_ptr[i + 1]] = sum;
    }
    if (i > 7)
    {
        if (s->decade_ptr[i] >= 10)
            s->decade_ptr[i] = 0;
        if (test)
        {
            if (s->error_rate != i  &&  s->reporter)
                s->reporter(s->user_data, BERT_REPORT_GT_10_2 + i - 2, &s->results);
            s->error_rate = i;
        }
    }
    else
    {
        s->decade_bad[i][s->decade_ptr[i]] = 0;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) bert_put_bit(bert_state_t *s, int bit)
{
    if (bit < 0)
    {
        /* Special conditions */
        printf("Status is %s (%d)\n", signal_status_to_str(bit), bit);
        return;
    }
    bit = (bit & 1) ^ s->invert;
    s->rx.bits++;
    switch (s->pattern_class)
    {
    case 0:
        if (s->rx.resync)
        {
            s->rx.reg = (s->rx.reg >> 1) | (bit << s->shift2);
            s->rx.ref_reg = (s->rx.ref_reg >> 1) | ((s->rx.ref_reg & 1) << s->shift2);
            if (s->rx.reg == s->rx.ref_reg)
            {
                if (++s->rx.resync > s->resync_time)
                {
                    s->rx.resync = 0;
                    if (s->reporter)
                        s->reporter(s->user_data, BERT_REPORT_SYNCED, &s->results);
                }
            }
            else
            {
                s->rx.resync = 2;
                s->rx.ref_reg = s->rx.master_reg;
            }
        }
        else
        {
            s->results.total_bits++;
            if ((bit ^ s->rx.ref_reg) & 1)
                s->results.bad_bits++;
            s->rx.ref_reg = (s->rx.ref_reg >> 1) | ((s->rx.ref_reg & 1) << s->shift2);
        }
        break;
    case 1:
        if (s->rx.resync)
        {
            /* If we get a reasonable period for which we correctly predict the
               next bit, we must be in sync. */
            /* Don't worry about max. zeros tests when resyncing.
               It might just extend the resync time a little. Trying
               to include the test might affect robustness. */
            if (bit == (int) ((s->rx.reg >> s->shift) & 1))
            {
                if (++s->rx.resync > s->resync_time)
                {
                    s->rx.resync = 0;
                    if (s->reporter)
                        s->reporter(s->user_data, BERT_REPORT_SYNCED, &s->results);
                }
            }
            else
            {
                s->rx.resync = 2;
                s->rx.reg ^= s->mask;
            }
        }
        else
        {
            s->results.total_bits++;
            if (s->max_zeros)
            {
                /* This generator suppresses runs >s->max_zeros */
                if ((s->rx.reg & s->mask))
                {
                    if (++s->rx.zeros > s->max_zeros)
                    {
                        s->rx.zeros = 0;
                        bit ^= 1;
                    }
                }
                else
                {
                    s->rx.zeros = 0;
                }
            }
            if (bit != (int) ((s->rx.reg >> s->shift) & 1))
            {
                s->results.bad_bits++;
                s->rx.resync_bad_bits++;
                s->decade_bad[2][s->decade_ptr[2]]++;
            }
            if (--s->rx.measurement_step <= 0)
            {
                /* Every hundred bits we need to do the error rate measurement */
                s->rx.measurement_step = MEASUREMENT_STEP;
                assess_error_rate(s);
            }
            if (--s->rx.resync_cnt <= 0)
            {
                /* Check if there were enough bad bits during this period to
                   justify a resync. */
                if (s->rx.resync_bad_bits >= (s->rx.resync_len*s->rx.resync_percent)/100)
                {
                    s->rx.resync = 1;
                    s->results.resyncs++;
                    if (s->reporter)
                        s->reporter(s->user_data, BERT_REPORT_UNSYNCED, &s->results);
                }
                s->rx.resync_cnt = s->rx.resync_len;
                s->rx.resync_bad_bits = 0;
            }
        }
        s->rx.reg = (s->rx.reg >> 1) | (((s->rx.reg ^ (s->rx.reg >> s->shift)) & 1) << s->shift2);
        break;
    case 2:
        s->rx.reg = (s->rx.reg >> 1) | (bit << 6);
        /* TODO: There is no mechanism for synching yet. This only works if things start in sync. */
        if (++s->rx.step_bit == 7)
        {
            s->rx.step_bit = 0;
            if ((int) s->rx.reg != qbf[s->rx.step])
            {
                /* We need to work out the number of actual bad bits here. We need to look at the
                   error rate, and see it a resync is needed. etc. */
                s->results.bad_bits++;
            }
            if (qbf[++s->rx.step] == '\0')
                s->rx.step = 0;
        }
        s->results.total_bits++;
        break;
    }
    if (s->report_frequency > 0)
    {
        if (--s->rx.report_countdown <= 0)
        {
            if (s->reporter)
                s->reporter(s->user_data, BERT_REPORT_REGULAR, &s->results);
            s->rx.report_countdown = s->report_frequency;
        }
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bert_result(bert_state_t *s, bert_results_t *results)
{
    results->total_bits = s->results.total_bits;
    results->bad_bits = s->results.bad_bits;
    results->resyncs = s->results.resyncs;
    return sizeof(*results);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) bert_set_report(bert_state_t *s, int freq, bert_report_func_t reporter, void *user_data)
{
    s->report_frequency = freq;
    s->reporter = reporter;
    s->user_data = user_data;

    s->rx.report_countdown = s->report_frequency;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bert_state_t *) bert_init(bert_state_t *s, int limit, int pattern, int resync_len, int resync_percent)
{
    int i;
    int j;

    if (s == NULL)
    {
        if ((s = (bert_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->pattern = pattern;
    s->limit = limit;
    s->reporter = NULL;
    s->user_data = NULL;
    s->report_frequency = 0;

    s->resync_time = 72;
    s->invert = 0;
    switch (s->pattern)
    {
    case BERT_PATTERN_ZEROS:
        s->tx.reg = 0;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_ONES:
        s->tx.reg = ~((uint32_t) 0);
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_7_TO_1:
        s->tx.reg = 0xFEFEFEFE;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_3_TO_1:
        s->tx.reg = 0xEEEEEEEE;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_1_TO_1:
        s->tx.reg = 0xAAAAAAAA;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_1_TO_3:
        s->tx.reg = 0x11111111;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_1_TO_7:
        s->tx.reg = 0x01010101;
        s->shift2 = 31;
        s->pattern_class = 0;
        break;
    case BERT_PATTERN_QBF:
        s->tx.reg = 0;
        s->pattern_class = 2;
        break;
    case BERT_PATTERN_ITU_O151_23:
        s->pattern_class = 1;
        s->tx.reg = 0x7FFFFF;
        s->mask = 0x20;
        s->shift = 5;
        s->shift2 = 22;
        s->invert = 1;
        s->resync_time = 56;
        s->max_zeros = 0;
        break;
    case BERT_PATTERN_ITU_O151_20:
        s->pattern_class = 1;
        s->tx.reg = 0xFFFFF;
        s->mask = 0x8;
        s->shift = 3;
        s->shift2 = 19;
        s->invert = 1;
        s->resync_time = 50;
        s->max_zeros = 14;
        break;
    case BERT_PATTERN_ITU_O151_15:
        s->pattern_class = 1;
        s->tx.reg = 0x7FFF;
        s->mask = 0x2;
        s->shift = 1;
        s->shift2 = 14;
        s->invert = 1;
        s->resync_time = 40;
        s->max_zeros = 0;
        break;
    case BERT_PATTERN_ITU_O152_11:
        s->pattern_class = 1;
        s->tx.reg = 0x7FF;
        s->mask = 0x4;
        s->shift = 2;
        s->shift2 = 10;
        s->invert = 0;
        s->resync_time = 32;
        s->max_zeros = 0;
        break;
    case BERT_PATTERN_ITU_O153_9:
        s->pattern_class = 1;
        s->tx.reg = 0x1FF;
        s->mask = 0x10;
        s->shift = 4;
        s->shift2 = 8;
        s->invert = 0;
        s->resync_time = 28;
        s->max_zeros = 0;
        break;
    }
    s->tx.bits = 0;
    s->tx.step = 0;
    s->tx.step_bit = 0;
    s->tx.zeros = 0;

    s->rx.reg = s->tx.reg;
    s->rx.ref_reg = s->rx.reg;
    s->rx.master_reg = s->rx.ref_reg;
    s->rx.bits = 0;
    s->rx.step = 0;
    s->rx.step_bit = 0;

    s->rx.resync = 1;
    s->rx.resync_cnt = resync_len;
    s->rx.resync_bad_bits = 0;
    s->rx.resync_len = resync_len;
    s->rx.resync_percent = resync_percent;

    s->results.total_bits = 0;
    s->results.bad_bits = 0;
    s->results.resyncs = 0;

    s->rx.report_countdown = 0;

    for (i = 0;  i < 8;  i++)
    {
        for (j = 0;  j < 10;  j++)
            s->decade_bad[i][j] = 0;
        s->decade_ptr[i] = 0;
    }
    s->error_rate = 8;
    s->rx.measurement_step = MEASUREMENT_STEP;

    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "BERT");

    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bert_release(bert_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bert_free(bert_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

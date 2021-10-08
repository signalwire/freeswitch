/*
 * SpanDSP - a series of DSP components for telephony
 *
 * tsb85_tests.c
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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

/*! \file */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#include "floating_fudge.h"
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sndfile.h>

#if defined(HAVE_LIBXML_XMLMEMORY_H)
#include <libxml/xmlmemory.h>
#endif
#if defined(HAVE_LIBXML_PARSER_H)
#include <libxml/parser.h>
#endif
#if defined(HAVE_LIBXML_XINCLUDE_H)
#include <libxml/xinclude.h>
#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#include "fax_utils.h"
#include "fax_tester.h"

#define OUTPUT_TIFF_FILE_NAME   "tsb85.tif"

#define OUTPUT_WAVE_FILE_NAME   "tsb85.wav"

#define SAMPLES_PER_CHUNK       160

SNDFILE *out_handle;

const char *output_tiff_file_name;

bool log_audio = false;

faxtester_state_t *state;

static void exchange(faxtester_state_t *s)
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    int len;
    int i;
    int total_audio_time;
    logging_state_t *logging;

    output_tiff_file_name = OUTPUT_TIFF_FILE_NAME;

    if (log_audio)
    {
        if ((out_handle = sf_open_telephony_write(OUTPUT_WAVE_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            printf("Test failed\n");
            exit(2);
        }
        /*endif*/
    }
    /*endif*/

    total_audio_time = 0;

    faxtester_set_transmit_on_idle(s, true);

    s->far_fax = fax_init(NULL, false);
    s->far_t30 = fax_get_t30_state(s->far_fax);
    s->far_tag = 'A';

    if (s->far_fax)
        logging = fax_get_logging_state(s->far_fax);
    else
        logging = t38_terminal_get_logging_state(s->far_t38);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "A");

    logging = t30_get_logging_state(s->far_t30);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "A");

#if 0
    span_log_set_level(&fax.modems.v27ter_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.modems.v27ter_rx.logging, "A");
    span_log_set_level(&fax.modems.v29_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.modems.v29_rx.logging, "A");
    span_log_set_level(&fax.modems.v17_rx.logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(&fax.modems.v17_rx.logging, "A");
#endif
    while (faxtester_next_step(s) == 0)
        ;
    /*endwhile*/
    for (;;)
    {
        len = fax_tx(s->far_fax, amp, SAMPLES_PER_CHUNK);
        faxtester_rx(s, amp, len);
        if (log_audio)
        {
            for (i = 0;  i < len;  i++)
                out_amp[2*i + 0] = amp[i];
            /*endfor*/
        }
        /*endif*/

        total_audio_time += SAMPLES_PER_CHUNK;

        logging = t30_get_logging_state(s->far_t30);
        span_log_bump_samples(logging, len);
#if 0
        span_log_bump_samples(&fax.modems.v27ter_rx.logging, len);
        span_log_bump_samples(&fax.modems.v29_rx.logging, len);
        span_log_bump_samples(&fax.modems.v17_rx.logging, len);
#endif
        logging = fax_get_logging_state(s->far_fax);
        span_log_bump_samples(logging, len);

        logging = faxtester_get_logging_state(s);
        span_log_bump_samples(logging, len);

        len = faxtester_tx(s, amp, SAMPLES_PER_CHUNK);
        if (fax_rx(s->far_fax, amp, len))
            break;
        /*endif*/
        if (log_audio)
        {
            for (i = 0;  i < len;  i++)
                out_amp[2*i + 1] = amp[i];
            /*endfor*/
            if (sf_writef_short(out_handle, out_amp, SAMPLES_PER_CHUNK) != SAMPLES_PER_CHUNK)
                break;
            /*endif*/
        }
        /*endif*/
        if (s->test_for_call_clear  &&  !s->far_end_cleared_call)
        {
            s->call_clear_timer += len;
            if (!t30_call_active(s->far_t30))
            {
                span_log(faxtester_get_logging_state(s), SPAN_LOG_FLOW, "Far end cleared after %dms (limits %dms to %dms)\n", s->call_clear_timer/8, s->timein_x, s->timeout);
                if (s->call_clear_timer/8 < s->timein_x  ||  s->call_clear_timer/8 > s->timeout_x)
                {
                    printf("Test failed\n");
                    exit(2);
                }
                span_log(faxtester_get_logging_state(s), SPAN_LOG_FLOW, "Clear time OK\n");
                s->far_end_cleared_call = true;
                s->test_for_call_clear = false;
                while (faxtester_next_step(s) == 0)
                    ;
                /*endwhile*/
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    if (log_audio)
    {
        if (sf_close_telephony(out_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            printf("Test failed\n");
            exit(2);
        }
        /*endif*/
    }
    /*endif*/
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    const char *xml_file_name;
    const char *test_name;
    logging_state_t *logging;
    int opt;

#if 0
    string_test();
#endif

    xml_file_name = "../spandsp/tsb85.xml";
    test_name = "MRGN01";
    log_audio = false;
    while ((opt = getopt(argc, argv, "lx:")) != -1)
    {
        switch (opt)
        {
        case 'l':
            log_audio = true;
            break;
        case 'x':
            xml_file_name = optarg;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    argc -= optind;
    argv += optind;
    if (argc > 0)
        test_name = argv[0];

    if ((state = faxtester_init(NULL, xml_file_name, test_name)) == NULL)
    {
        fprintf(stderr, "Cannot start FAX tester instance\n");
        printf("Test failed\n");
        exit(2);
    }
    logging = faxtester_get_logging_state(state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "B");
    /* We found the test we want, so run it. */
    exchange(state);
    faxtester_free(state);
    printf("Done\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * v8_tests.c
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
 */

/*! \page v8_tests_page V.8 tests
\section v8_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sndfile.h>

//#if defined(WITH_SPANDSP_INTERNALS)
#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
//#endif

#include "spandsp.h"
#include "spandsp-sim.h"

#define FALSE 0
#define TRUE (!FALSE)

#define SAMPLES_PER_CHUNK   160

#define OUTPUT_FILE_NAME    "v8.wav"

int negotiations_ok = 0;

#if 0
static int select_modulation(int mask)
{
    /* Select the fastest data modem available */
    if (mask & V8_MOD_V90)
        return V8_MOD_V90;
    /*endif*/
    if (mask & V8_MOD_V34)
        return V8_MOD_V34;
    /*endif*/
    if (mask & V8_MOD_V32)
        return V8_MOD_V32;
    /*endif*/
    if (mask & V8_MOD_V23)
        return V8_MOD_V23;
    /*endif*/
    if (mask & V8_MOD_V21)
        return V8_MOD_V21;
    /*endif*/
    return -1;
}
/*- End of function --------------------------------------------------------*/
#endif

static void handler(void *user_data, v8_parms_t *result)
{
    const char *s;
    
    s = (const char *) user_data;

    printf("%s ", s);
    switch (result->status)
    {
    case V8_STATUS_IN_PROGRESS:
        printf("V.8 negotiation in progress\n");
        return;
    case V8_STATUS_V8_OFFERED:
        printf("V.8 offered by the other party\n");
        break;
    case V8_STATUS_V8_CALL:
        printf("V.8 call negotiation successful\n");
        break;
    case V8_STATUS_NON_V8_CALL:
        printf("Non-V.8 call negotiation successful\n");
        break;
    case V8_STATUS_FAILED:
        printf("V.8 call negotiation failed\n");
        return;
    default:
        printf("Unexpected V.8 status %d\n", result->status);
        break;
    }
    /*endswitch*/

    printf("  Modem connect tone '%s' (%d)\n", modem_connect_tone_to_str(result->modem_connect_tone), result->modem_connect_tone);
    printf("  Call function '%s' (%d)\n", v8_call_function_to_str(result->call_function), result->call_function);
    printf("  Far end modulations 0x%X\n", result->modulations);
    printf("  Protocol '%s' (%d)\n", v8_protocol_to_str(result->protocol), result->protocol);
    printf("  PSTN access '%s' (%d)\n", v8_pstn_access_to_str(result->pstn_access), result->pstn_access);
    printf("  PCM modem availability '%s' (%d)\n", v8_pcm_modem_availability_to_str(result->pcm_modem_availability), result->pcm_modem_availability);
    if (result->t66 >= 0)
        printf("  T.66 '%s' (%d)\n", v8_t66_to_str(result->t66), result->t66);
    /*endif*/
    if (result->nsf >= 0)
        printf("  NSF %d\n", result->nsf);
    /*endif*/

    switch (result->status)
    {
    case V8_STATUS_V8_OFFERED:
        /* Edit the result information appropriately */
        //result->call_function = V8_CALL_T30_TX;
        result->modulations &= (V8_MOD_V17
                              | V8_MOD_V21
                              //| V8_MOD_V22
                              //| V8_MOD_V23HDX
                              //| V8_MOD_V23
                              //| V8_MOD_V26BIS
                              //| V8_MOD_V26TER
                              | V8_MOD_V27TER
                              | V8_MOD_V29
                              //| V8_MOD_V32
                              | V8_MOD_V34HDX
                              //| V8_MOD_V34
                              //| V8_MOD_V90
                              | V8_MOD_V92);
        break;
    case V8_STATUS_V8_CALL:
        if (result->call_function == V8_CALL_V_SERIES
            &&
            result->protocol == V8_PROTOCOL_LAPM_V42)
        {
            negotiations_ok++;
        }
        /*endif*/
        break;
    case V8_STATUS_NON_V8_CALL:
        negotiations_ok = 42;
        break;
    }
    /*endswitch*/
}
/*- End of function --------------------------------------------------------*/

static int v8_calls_v8_tests(SNDFILE *outhandle)
{
    v8_state_t *v8_caller;
    v8_state_t *v8_answerer;
    logging_state_t *caller_logging;
    logging_state_t *answerer_logging;
    int caller_available_modulations;
    int answerer_available_modulations;
    int i;
    int samples;
    int remnant;
    int outframes;
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    v8_parms_t v8_call_parms;
    v8_parms_t v8_answer_parms;

    caller_available_modulations = V8_MOD_V17
                                 | V8_MOD_V21
                                 | V8_MOD_V22
                                 | V8_MOD_V23HDX
                                 | V8_MOD_V23
                                 | V8_MOD_V26BIS
                                 | V8_MOD_V26TER
                                 | V8_MOD_V27TER
                                 | V8_MOD_V29
                                 | V8_MOD_V32
                                 | V8_MOD_V34HDX
                                 | V8_MOD_V34
                                 | V8_MOD_V90
                                 | V8_MOD_V92;
    answerer_available_modulations = V8_MOD_V17
                                   | V8_MOD_V21
                                   | V8_MOD_V22
                                   | V8_MOD_V23HDX
                                   | V8_MOD_V23
                                   | V8_MOD_V26BIS
                                   | V8_MOD_V26TER
                                   | V8_MOD_V27TER
                                   | V8_MOD_V29
                                   | V8_MOD_V32
                                   | V8_MOD_V34HDX
                                   | V8_MOD_V34
                                   | V8_MOD_V90
                                   | V8_MOD_V92;
    negotiations_ok = 0;

    v8_call_parms.modem_connect_tone = MODEM_CONNECT_TONES_NONE;
    v8_call_parms.send_ci = TRUE;
    v8_call_parms.v92 = -1;
    v8_call_parms.call_function = V8_CALL_V_SERIES;
    v8_call_parms.modulations = caller_available_modulations;
    v8_call_parms.protocol = V8_PROTOCOL_LAPM_V42;
    v8_call_parms.pcm_modem_availability = 0;
    v8_call_parms.pstn_access = 0;
    v8_call_parms.nsf = -1;
    v8_call_parms.t66 = -1;
    v8_caller = v8_init(NULL,
                        TRUE,
                        &v8_call_parms,
                        handler,
                        (void *) "caller");
    v8_answer_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    v8_answer_parms.send_ci = TRUE;
    v8_answer_parms.v92 = -1;
    v8_answer_parms.call_function = V8_CALL_V_SERIES;
    v8_answer_parms.modulations = answerer_available_modulations;
    v8_answer_parms.protocol = V8_PROTOCOL_LAPM_V42;
    v8_answer_parms.pcm_modem_availability = 0;
    v8_answer_parms.pstn_access = 0;
    v8_answer_parms.nsf = -1;
    v8_answer_parms.t66 = -1;
    v8_answerer = v8_init(NULL,
                          FALSE,
                          &v8_answer_parms,
                          handler,
                          (void *) "answerer");
    caller_logging = v8_get_logging_state(v8_caller);
    span_log_set_level(caller_logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(caller_logging, "caller");
    answerer_logging = v8_get_logging_state(v8_answerer);
    span_log_set_level(answerer_logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(answerer_logging, "answerer");
    for (i = 0;  i < 1000;  i++)
    {
        samples = v8_tx(v8_caller, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(amp + samples, SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
        span_log_bump_samples(caller_logging, samples);
        remnant = v8_rx(v8_answerer, amp, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = amp[i];
        /*endfor*/

        samples = v8_tx(v8_answerer, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(amp + samples, SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
        span_log_bump_samples(answerer_logging, samples);
        if (v8_rx(v8_caller, amp, samples)  &&  remnant)
            break;
        /*endif*/
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = amp[i];
        /*endfor*/

        if (outhandle)
        {
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    v8_free(v8_caller);
    v8_free(v8_answerer);

    if (negotiations_ok != 2)
    {
        printf("Tests failed.\n");
        exit(2);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int non_v8_calls_v8_tests(SNDFILE *outhandle)
{
    silence_gen_state_t *non_v8_caller_tx;
    modem_connect_tones_rx_state_t *non_v8_caller_rx;
    v8_state_t *v8_answerer;
    logging_state_t *answerer_logging;
    int answerer_available_modulations;
    int i;
    int samples;
    int remnant;
    int outframes;
    int tone;
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    v8_parms_t v8_answer_parms;

    answerer_available_modulations = V8_MOD_V17
                                   | V8_MOD_V21
                                   | V8_MOD_V22
                                   | V8_MOD_V23HDX
                                   | V8_MOD_V23
                                   | V8_MOD_V26BIS
                                   | V8_MOD_V26TER
                                   | V8_MOD_V27TER
                                   | V8_MOD_V29
                                   | V8_MOD_V32
                                   | V8_MOD_V34HDX
                                   | V8_MOD_V34
                                   | V8_MOD_V90
                                   | V8_MOD_V92;
    negotiations_ok = 0;

    non_v8_caller_tx = silence_gen_init(NULL, 10*SAMPLE_RATE);
    non_v8_caller_rx = modem_connect_tones_rx_init(NULL, MODEM_CONNECT_TONES_ANS_PR, NULL, NULL);

    v8_answer_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
    v8_answer_parms.send_ci = TRUE;
    v8_answer_parms.v92 = -1;
    v8_answer_parms.call_function = V8_CALL_V_SERIES;
    v8_answer_parms.modulations = answerer_available_modulations;
    v8_answer_parms.protocol = V8_PROTOCOL_LAPM_V42;
    v8_answer_parms.pcm_modem_availability = 0;
    v8_answer_parms.pstn_access = 0;
    v8_answer_parms.nsf = -1;
    v8_answer_parms.t66 = -1;
    v8_answerer = v8_init(NULL,
                          FALSE,
                          &v8_answer_parms,
                          handler,
                          (void *) "answerer");
    answerer_logging = v8_get_logging_state(v8_answerer);
    span_log_set_level(answerer_logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(answerer_logging, "answerer");
    for (i = 0;  i < 1000;  i++)
    {
        samples = silence_gen(non_v8_caller_tx, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(amp + samples, SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
        remnant = v8_rx(v8_answerer, amp, samples);
        if (remnant)
            break;
        /*endif*/
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = amp[i];
        /*endfor*/
    
        samples = v8_tx(v8_answerer, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(amp + samples, SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
        span_log_bump_samples(answerer_logging, samples);
        modem_connect_tones_rx(non_v8_caller_rx, amp, samples);
        if ((tone = modem_connect_tones_rx_get(non_v8_caller_rx)) != MODEM_CONNECT_TONES_NONE)
        {
            printf("Detected %s (%d)\n", modem_connect_tone_to_str(tone), tone);
            if (tone == MODEM_CONNECT_TONES_ANSAM_PR)
                negotiations_ok++;
            /*endif*/
        }
        /*endif*/
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = amp[i];
        /*endfor*/

        if (outhandle)
        {
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    silence_gen_free(non_v8_caller_tx);
    modem_connect_tones_rx_free(non_v8_caller_rx);
    v8_free(v8_answerer);

    if (negotiations_ok != 1)
    {
        printf("Tests failed.\n");
        exit(2);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

static int v8_calls_non_v8_tests(SNDFILE *outhandle)
{
    v8_state_t *v8_caller;
    modem_connect_tones_tx_state_t *non_v8_answerer_tx;
    logging_state_t *caller_logging;
    int caller_available_modulations;
    int i;
    int samples;
    int outframes;
    int16_t amp[SAMPLES_PER_CHUNK];
    int16_t out_amp[2*SAMPLES_PER_CHUNK];
    v8_parms_t v8_call_parms;

    caller_available_modulations = V8_MOD_V17
                                 | V8_MOD_V21
                                 | V8_MOD_V22
                                 | V8_MOD_V23HDX
                                 | V8_MOD_V23
                                 | V8_MOD_V26BIS
                                 | V8_MOD_V26TER
                                 | V8_MOD_V27TER
                                 | V8_MOD_V29
                                 | V8_MOD_V32
                                 | V8_MOD_V34HDX
                                 | V8_MOD_V34
                                 | V8_MOD_V90
                                 | V8_MOD_V92;
    negotiations_ok = 0;

    v8_call_parms.modem_connect_tone = MODEM_CONNECT_TONES_NONE;
    v8_call_parms.send_ci = TRUE;
    v8_call_parms.v92 = -1;
    v8_call_parms.call_function = V8_CALL_V_SERIES;
    v8_call_parms.modulations = caller_available_modulations;
    v8_call_parms.protocol = V8_PROTOCOL_LAPM_V42;
    v8_call_parms.pcm_modem_availability = 0;
    v8_call_parms.pstn_access = 0;
    v8_call_parms.nsf = -1;
    v8_call_parms.t66 = -1;
    v8_caller = v8_init(NULL,
                        TRUE,
                        &v8_call_parms,
                        handler,
                        (void *) "caller");
    non_v8_answerer_tx = modem_connect_tones_tx_init(NULL, MODEM_CONNECT_TONES_ANS_PR);
    caller_logging = v8_get_logging_state(v8_caller);
    span_log_set_level(caller_logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_SAMPLE_TIME);
    span_log_set_tag(caller_logging, "caller");
    for (i = 0;  i < 1000;  i++)
    {
        samples = v8_tx(v8_caller, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(amp + samples, SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
        span_log_bump_samples(caller_logging, samples);
        for (i = 0;  i < samples;  i++)
            out_amp[2*i] = amp[i];
        /*endfor*/
    
        samples = modem_connect_tones_tx(non_v8_answerer_tx, amp, SAMPLES_PER_CHUNK);
        if (samples < SAMPLES_PER_CHUNK)
        {
            vec_zeroi16(amp + samples, SAMPLES_PER_CHUNK - samples);
            samples = SAMPLES_PER_CHUNK;
        }
        /*endif*/
        if (v8_rx(v8_caller, amp, samples))
            break;
        /*endif*/
        for (i = 0;  i < samples;  i++)
            out_amp[2*i + 1] = amp[i];
        /*endfor*/

        if (outhandle)
        {
            outframes = sf_writef_short(outhandle, out_amp, samples);
            if (outframes != samples)
            {
                fprintf(stderr, "    Error writing audio file\n");
                exit(2);
            }
            /*endif*/
        }
        /*endif*/
    }
    /*endfor*/
    v8_free(v8_caller);
    modem_connect_tones_tx_free(non_v8_answerer_tx);

    if (negotiations_ok != 42)
    {
        printf("Tests failed.\n");
        exit(2);
    }
    /*endif*/
    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int16_t amp[SAMPLES_PER_CHUNK];
    int samples;
    int caller_available_modulations;
    int answerer_available_modulations;
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    int opt;
    int log_audio;
    char *decode_test_file;
    v8_state_t *v8_caller;
    v8_state_t *v8_answerer;
    v8_parms_t v8_call_parms;
    v8_parms_t v8_answer_parms;
    logging_state_t *logging;

    decode_test_file = NULL;
    log_audio = FALSE;
    while ((opt = getopt(argc, argv, "d:l")) != -1)
    {
        switch (opt)
        {
        case 'd':
            decode_test_file = optarg;
            break;
        case 'l':
            log_audio = TRUE;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    if (decode_test_file)
    {
        caller_available_modulations = V8_MOD_V17
                                     | V8_MOD_V21
                                     | V8_MOD_V22
                                     | V8_MOD_V23HDX
                                     | V8_MOD_V23
                                     | V8_MOD_V26BIS
                                     | V8_MOD_V26TER
                                     | V8_MOD_V27TER
                                     | V8_MOD_V29
                                     | V8_MOD_V32
                                     | V8_MOD_V34HDX
                                     | V8_MOD_V34
                                     | V8_MOD_V90
                                     | V8_MOD_V92;
        answerer_available_modulations = V8_MOD_V17
                                       | V8_MOD_V21
                                       | V8_MOD_V22
                                       | V8_MOD_V23HDX
                                       | V8_MOD_V23
                                       | V8_MOD_V26BIS
                                       | V8_MOD_V26TER
                                       | V8_MOD_V27TER
                                       | V8_MOD_V29
                                       | V8_MOD_V32
                                       | V8_MOD_V34HDX
                                       | V8_MOD_V34
                                       | V8_MOD_V90
                                       | V8_MOD_V92;

        printf("Decode file '%s'\n", decode_test_file);
        v8_call_parms.modem_connect_tone = MODEM_CONNECT_TONES_NONE;
        v8_call_parms.send_ci = TRUE;
        v8_call_parms.v92 = -1;
        v8_call_parms.call_function = V8_CALL_V_SERIES;
        v8_call_parms.modulations = caller_available_modulations;
        v8_call_parms.protocol = V8_PROTOCOL_LAPM_V42;
        v8_call_parms.pcm_modem_availability = 0;
        v8_call_parms.pstn_access = 0;
        v8_call_parms.nsf = -1;
        v8_call_parms.t66 = -1;
        v8_caller = v8_init(NULL,
                            TRUE,
                            &v8_call_parms,
                            handler,
                            (void *) "caller");
        logging = v8_get_logging_state(v8_caller);
        span_log_set_level(logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(logging, "caller");

        v8_answer_parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
        v8_answer_parms.send_ci = TRUE;
        v8_answer_parms.v92 = -1;
        v8_answer_parms.call_function = V8_CALL_V_SERIES;
        v8_answer_parms.modulations = answerer_available_modulations;
        v8_answer_parms.protocol = V8_PROTOCOL_LAPM_V42;
        v8_answer_parms.pcm_modem_availability = 0;
        v8_answer_parms.pstn_access = 0;
        v8_answer_parms.nsf = -1;
        v8_answer_parms.t66 = -1;
        v8_answerer = v8_init(NULL,
                              FALSE,
                              &v8_answer_parms,
                              handler,
                              (void *) "answerer");
        logging = v8_get_logging_state(v8_answerer);
        span_log_set_level(logging, SPAN_LOG_FLOW | SPAN_LOG_SHOW_TAG);
        span_log_set_tag(logging, "answerer");

        if ((inhandle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
        {
            fprintf(stderr, "    Cannot open speech file '%s'\n", decode_test_file);
            exit (2);
        }
        /*endif*/

        while ((samples = sf_readf_short(inhandle, amp, SAMPLES_PER_CHUNK)))
        {
            v8_rx(v8_caller, amp, samples);
            v8_rx(v8_answerer, amp, samples);
            v8_tx(v8_caller, amp, samples);
            v8_tx(v8_answerer, amp, samples);
        }
        /*endwhile*/

        v8_free(v8_caller);
        v8_free(v8_answerer);
        if (sf_close_telephony(inhandle))
        {
            fprintf(stderr, "    Cannot close speech file '%s'\n", decode_test_file);
            exit(2);
        }
        /*endif*/
    }
    else
    {
        outhandle = NULL;
        if (log_audio)
        {
            if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 2)) == NULL)
            {
                fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_FILE_NAME);
                exit(2);
            }
            /*endif*/
        }
        /*endif*/

        printf("Test 1: V.8 terminal calls V.8 terminal\n");
        v8_calls_v8_tests(outhandle);

        printf("Test 2: non-V.8 terminal calls V.8 terminal\n");
        non_v8_calls_v8_tests(outhandle);

        printf("Test 3: V.8 terminal calls non-V.8 terminal\n");
        v8_calls_non_v8_tests(outhandle);

        if (outhandle)
        {
            if (sf_close_telephony(outhandle))
            {
                fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
                exit(2);
            }
            /*endif*/
        }
        /*endif*/

        printf("Tests passed.\n");
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

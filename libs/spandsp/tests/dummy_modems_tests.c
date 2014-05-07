/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dummy_modems_tests.c - Tests for data_modems connected together by sockets.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

/*! \file */

/*! \page dummy_modems_tests_page Dummy data modems tests
\section dummy_modems_tests_page_sec_1 What does it do?
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)  &&  defined(HAVE_FL_FL_AUDIO_METER_H)
#define ENABLE_GUI
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <sndfile.h>

//#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"
#include "spandsp-sim.h"

#include "pseudo_terminals.h"
#include "socket_harness.h"

#if defined(ENABLE_GUI)
#include "media_monitor.h"
#endif

#define OUTPUT_WAVE_FILE_NAME   "dummy_modems.wav"

#define SAMPLES_PER_CHUNK 160

SNDFILE *wave_handle = NULL;
int16_t wave_buffer[4096];

data_modems_state_t *data_modem_state;

static int get_msg(void *user_data, uint8_t msg[], int len)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void put_msg(void *user_data, const uint8_t msg[], int len)
{
    if (len < 0)
        printf("Status %s\n", signal_status_to_str(len));
    else
        printf("Put %d '%s'\n", len, msg);
}
/*- End of function --------------------------------------------------------*/

static void terminal_callback(void *user_data, const uint8_t msg[], int len)
{
    printf("terminal callback %d\n", len);
}
/*- End of function --------------------------------------------------------*/

static int termios_callback(void *user_data, struct termios *termios)
{
    printf("termios callback\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/

static void hangup_callback(void *user_data, int status)
{
}
/*- End of function --------------------------------------------------------*/

static int terminal_free_space_callback(void *user_data)
{
    return 42;
}
/*- End of function --------------------------------------------------------*/

static int rx_callback(void *user_data, const int16_t amp[], int samples)
{
    int i;
    int out_samples;

    out_samples = data_modems_rx((data_modems_state_t *) user_data, amp, samples);
    if (wave_handle)
    {
        for (i = 0;  i < samples;  i++)
            wave_buffer[2*i] = amp[i];
    }
    return out_samples;
}
/*- End of function --------------------------------------------------------*/

static int rx_fillin_callback(void *user_data, int samples)
{
    return data_modems_rx_fillin((data_modems_state_t *) user_data, samples);
}
/*- End of function --------------------------------------------------------*/

static int tx_callback(void *user_data, int16_t amp[], int samples)
{
    int i;
    int out_samples;

    out_samples = data_modems_tx((data_modems_state_t *) user_data, amp, samples);
    if (wave_handle)
    {
        if (out_samples < samples)
            memset(&amp[out_samples], 0, (samples - out_samples)*2);
        for (i = 0;  i < samples;  i++)
            wave_buffer[2*i + 1] = amp[i];
        sf_writef_short(wave_handle, wave_buffer, samples);
    }
    return samples;
}
/*- End of function --------------------------------------------------------*/

static int modem_tests(int use_gui, int log_audio, bool calling_party)
{
    logging_state_t *logging;
    socket_harness_state_t *s;

    /* Now set up and run the modems */
    if ((data_modem_state = data_modems_init(NULL,
                                             calling_party,
                                             put_msg,
                                             get_msg,
                                             NULL)) == NULL)
    {
        fprintf(stderr, "    Cannot start the data modem\n");
        exit(2);
    }
    logging = data_modems_get_logging_state(data_modem_state);
    span_log_set_level(logging, SPAN_LOG_DEBUG | SPAN_LOG_SHOW_TAG | SPAN_LOG_SHOW_DATE);
    span_log_set_tag(logging, "Modem");

    if ((s = socket_harness_init(NULL,
                                 "/tmp/modemsocket",
                                 "modemA",
                                 calling_party,
                                 terminal_callback,
                                 termios_callback,
                                 hangup_callback,
                                 terminal_free_space_callback,
                                 rx_callback,
                                 rx_fillin_callback,
                                 tx_callback,
                                 data_modem_state)) == NULL)
    {
        fprintf(stderr, "    Cannot start the socket harness\n");
        exit(2);
    }

    wave_handle = NULL;
    if (log_audio)
    {
        if ((wave_handle = sf_open_telephony_write(OUTPUT_WAVE_FILE_NAME, 2)) == NULL)
        {
            fprintf(stderr, "    Cannot create audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            exit(2);
        }
    }

    socket_harness_run(s);

    if (log_audio)
    {
        if (sf_close_telephony(wave_handle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_WAVE_FILE_NAME);
            exit(2);
        }
    }

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int log_audio;
    int use_gui;
    int opt;
    bool calling_party;

    log_audio = false;
    calling_party = false;
    use_gui = false;
    while ((opt = getopt(argc, argv, "acgl")) != -1)
    {
        switch (opt)
        {
        case 'a':
            calling_party = false;
            break;
        case 'c':
            calling_party = true;
            break;
        case 'g':
#if defined(ENABLE_GUI)
            use_gui = true;
#else
            fprintf(stderr, "Graphical monitoring not available\n");
            exit(2);
#endif
            break;
        case 'l':
            log_audio = true;
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }

    if (modem_tests(use_gui, log_audio, calling_party))
        exit(2);
    printf("Tests passed\n");
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

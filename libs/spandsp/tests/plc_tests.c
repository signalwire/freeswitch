/*
 * SpanDSP - a series of DSP components for telephony
 *
 * plc_tests.c
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

/*! \page plc_tests_page Packet loss concealment tests
\section plc_tests_page_sec_1 What does it do?
These tests run a speech file through the packet loss concealment routines.
The loss rate, in percent, and the packet size, in samples, may be specified
on the command line.

\section plc_tests_page_sec_2 How are the tests run?
These tests process a speech file called pre_plc.wav. This file should contain
8000 sample/second 16 bits/sample linear audio. The tests read this file in
blocks, of a size specified on the command line. Some of these blocks are
dropped, to simulate packet loss. The rate of loss is also specified on the
command line. The PLC module is then used to reconstruct an acceptable
approximation to the original signal. The resulting audio is written to a new
audio file, called post_plc.wav. This file contains 8000 sample/second
16 bits/sample linear audio.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

#define INPUT_FILE_NAME     "../test-data/local/short_nb_voice.wav"
#define OUTPUT_FILE_NAME    "post_plc.wav"

int main(int argc, char *argv[])
{
    SNDFILE *inhandle;
    SNDFILE *outhandle;
    plc_state_t plc;
    int inframes;
    int outframes;
    int16_t amp[1024];
    int block_no;
    int lost_blocks;
    int block_len;
    int loss_rate;
    int dropit;
    int block_real;
    int block_synthetic;
    int tone;
    int i;
    uint32_t phase_acc;
    int32_t phase_rate;
    int opt;

    loss_rate = 25;
    block_len = 160;
    block_real = FALSE;
    block_synthetic = FALSE;
    tone = -1;
    while ((opt = getopt(argc, argv, "b:l:rst:")) != -1)
    {
        switch (opt)
        {
        case 'b':
            block_len = atoi(optarg);
            break;
        case 'l':
            loss_rate = atoi(optarg);
            break;
        case 'r':
            block_real = TRUE;
            break;
        case 's':
            block_synthetic = TRUE;
            break;
        case 't':
            tone = atoi(optarg);
            break;
        }
    }
    phase_rate = 0;
    inhandle = NULL;
    if (tone < 0)
    {
        if ((inhandle = sf_open_telephony_read(INPUT_FILE_NAME, 1)) == NULL)
        {
            fprintf(stderr, "    Failed to open audio file '%s'\n", INPUT_FILE_NAME);
            exit(2);
        }
    }
    else
    {
        phase_rate = dds_phase_ratef((float) tone);
    }
    if ((outhandle = sf_open_telephony_write(OUTPUT_FILE_NAME, 1)) == NULL)
    {
        fprintf(stderr, "    Failed to open audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    plc_init(&plc);
    lost_blocks = 0;
    for (block_no = 0;  ;  block_no++)
    {
        if (tone < 0)
        {
            inframes = sf_readf_short(inhandle, amp, block_len);
            if (inframes != block_len)
                break;
        }
        else
        {
            if (block_no > 10000)
                break;
            for (i = 0;  i < block_len;  i++)
                amp[i] = (int16_t) dds_modf(&phase_acc, phase_rate, 10000.0, 0);
            inframes = block_len;
        }
        dropit = rand()/(RAND_MAX/100);
        if (dropit > loss_rate)
        {
            plc_rx(&plc, amp, inframes);
            if (block_real)
                memset(amp, 0, sizeof(int16_t)*inframes);
        }
        else
        {
            lost_blocks++;
            plc_fillin(&plc, amp, inframes);
            if (block_synthetic)
                memset(amp, 0, sizeof(int16_t)*inframes);
        }
        outframes = sf_writef_short(outhandle, amp, inframes);
        if (outframes != inframes)
        {
            fprintf(stderr, "    Error writing out sound\n");
            exit(2);
        }
    }
    printf("Dropped %d of %d blocks\n", lost_blocks, block_no);
    if (tone < 0)
    {
        if (sf_close_telephony(inhandle))
        {
            fprintf(stderr, "    Cannot close audio file '%s'\n", INPUT_FILE_NAME);
            exit(2);
        }
    }
    if (sf_close_telephony(outhandle))
    {
        fprintf(stderr, "    Cannot close audio file '%s'\n", OUTPUT_FILE_NAME);
        exit(2);
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * r2_mf_tests.c - Test the R2 MF detector against the spec., whatever the
 *                 spec. may be :)
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

/*! \page r2_mf_tests_page R2 MF tone generation and detection tests
\section r2_mf_tests_page_sec_1 What does it do?
These tests are fashioned after those on the CM7291 test tape from
Mitel. Those tests are for DTMF, rather than R2 MF, but make a
fair starting point for a set of meaningful tests of R2 MF.

These tests include conversion to and from A-law. It is assumed the
distortion this produces is comparable to u-law, so it should be
a fair test of performance in a real PSTN channel.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sndfile.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES

#include "spandsp.h"

/* R2 tone generation specs.
 *  Power: -11.5dBm +- 1dB
 *  Frequency: within +-4Hz
 *  Mismatch between the start time of a pair of tones: <=1ms.
 *  Mismatch between the end time of a pair of tones: <=1ms.
 */
/* Basic MFC/R2 tone detection specs:
 *  Receiver response range: -5dBm to -35dBm
 *  Difference in level for a pair of frequencies
 *      Adjacent tones: <5dB
 *      Non-adjacent tones: <7dB
 *  Receiver not to detect a signal of 2 frequencies of level -5dB and
 *  duration <7ms.
 *  Receiver not to recognise a signal of 2 frequencies having a difference
 *  in level >=20dB.
 *  Max received signal frequency error: +-10Hz
 *  The sum of the operate and release times of a 2 frequency signal not to
 *  exceed 80ms (there are no individual specs for the operate and release
 *  times).
 *  Receiver not to release for signal interruptions <=7ms.
 *  System malfunction due to signal interruptions >7ms (typically 20ms) is
 *  prevented by further logic elements.
 */

#define MF_DURATION                 (68*8)
#define MF_PAUSE                    (68*8)
#define MF_CYCLE                    (MF_DURATION + MF_PAUSE)

/*!
    MF tone generator descriptor for tests.
*/
typedef struct
{
    float       f1;         /* First freq */
    float       f2;         /* Second freq */
    int8_t      level1;     /* Level of the first freq (dB) */
    int8_t      level2;     /* Level of the second freq (dB) */
    uint8_t     on_time;    /* Tone on time (ms) */
    uint8_t     off_time;   /* Minimum post tone silence (ms) */
} mf_digit_tones_t;

static const mf_digit_tones_t r2_mf_fwd_tones[] =
{
    {1380.0, 1500.0, -11, -11, 1, 0},
    {1380.0, 1620.0, -11, -11, 1, 0},
    {1500.0, 1620.0, -11, -11, 1, 0},
    {1380.0, 1740.0, -11, -11, 1, 0},
    {1500.0, 1740.0, -11, -11, 1, 0},
    {1620.0, 1740.0, -11, -11, 1, 0},
    {1380.0, 1860.0, -11, -11, 1, 0},
    {1500.0, 1860.0, -11, -11, 1, 0},
    {1620.0, 1860.0, -11, -11, 1, 0},
    {1740.0, 1860.0, -11, -11, 1, 0},
    {1380.0, 1980.0, -11, -11, 1, 0},
    {1500.0, 1980.0, -11, -11, 1, 0},
    {1620.0, 1980.0, -11, -11, 1, 0},
    {1740.0, 1980.0, -11, -11, 1, 0},
    {1860.0, 1980.0, -11, -11, 1, 0},
    {0.0, 0.0, 0, 0, 0, 0}
};

static const mf_digit_tones_t r2_mf_back_tones[] =
{
    {1140.0, 1020.0, -11, -11, 1, 0},
    {1140.0,  900.0, -11, -11, 1, 0},
    {1020.0,  900.0, -11, -11, 1, 0},
    {1140.0,  780.0, -11, -11, 1, 0},
    {1020.0,  780.0, -11, -11, 1, 0},
    { 900.0,  780.0, -11, -11, 1, 0},
    {1140.0,  660.0, -11, -11, 1, 0},
    {1020.0,  660.0, -11, -11, 1, 0},
    { 900.0,  660.0, -11, -11, 1, 0},
    { 780.0,  660.0, -11, -11, 1, 0},
    {1140.0,  540.0, -11, -11, 1, 0},
    {1020.0,  540.0, -11, -11, 1, 0},
    { 900.0,  540.0, -11, -11, 1, 0},
    { 780.0,  540.0, -11, -11, 1, 0},
    { 660.0,  540.0, -11, -11, 1, 0},
    {0.0, 0.0, 0, 0, 0, 0}
};

static tone_gen_descriptor_t my_mf_digit_tones[16];

static char r2_mf_tone_codes[] = "1234567890BCDEF";

int callback_ok;
int callback_roll;

static void my_mf_gen_init(float low_fudge,
                           int low_level,
                           float high_fudge,
                           int high_level,
                           int duration,
                           int fwd)
{
    const mf_digit_tones_t *tone;
    int i;

    for (i = 0;  i < 15;  i++)
    {
        if (fwd)
            tone = &r2_mf_fwd_tones[i];
        else
            tone = &r2_mf_back_tones[i];
        tone_gen_descriptor_init(&my_mf_digit_tones[i],
                                 tone->f1*(1.0 + low_fudge),
                                 low_level,
                                 tone->f2*(1.0 + high_fudge),
                                 high_level,
                                 duration,
                                 0,
                                 0,
                                 0,
                                 false);
    }
}
/*- End of function --------------------------------------------------------*/

static int my_mf_generate(int16_t amp[], char digit)
{
    int len;
    char *cp;
    tone_gen_state_t *tone;

    len = 0;
    if ((cp = strchr(r2_mf_tone_codes, digit)))
    {
        tone = tone_gen_init(NULL, &my_mf_digit_tones[cp - r2_mf_tone_codes]);
        len += tone_gen(tone, amp + len, 9999);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static void codec_munge(int16_t amp[], int len)
{
    int i;
    uint8_t alaw;

    for (i = 0;  i < len;  i++)
    {
        alaw = linear_to_alaw (amp[i]);
        amp[i] = alaw_to_linear (alaw);
    }
}
/*- End of function --------------------------------------------------------*/

static void digit_delivery(void *data, int digit, int level, int delay)
{
    char ch;

    if (data != (void *) 0x12345678)
    {
        callback_ok = false;
        return;
    }
    if ((callback_roll & 1))
        ch = 0;
    else
        ch = r2_mf_tone_codes[callback_roll >> 1];
    if (ch == digit)
        callback_ok = true;
    else
        callback_ok = false;
    if (r2_mf_tone_codes[callback_roll >> 1])
        callback_roll++;
    else
        callback_ok = false;
}
/*- End of function --------------------------------------------------------*/

static int test_a_tone_set(int fwd)
{
    int i;
    int j;
    int len;
    int sample;
    const char *s;
    char digit;
    int actual;
    int nplus;
    int nminus;
    float rrb;
    float rcfo;
    int16_t amp[100000];
    r2_mf_rx_state_t *mf_state;
    awgn_state_t *noise_source;

    mf_state = r2_mf_rx_init(NULL, fwd, NULL, NULL);

    /* Test 1: Mitel's test 1 isn't really a test. Its a calibration step,
       which has no meaning here. */

    printf("Test 1: Calibration\n");
    printf("    Passed\n");

    /* Test 2: Decode check
       This is a sanity check, that all digits are reliably detected
       under ideal conditions.  Each possible digit is repeated 10 times,
       with 68ms bursts. The level of each tone is about 6dB down from clip */

    printf("Test 2: Decode check\n");
    my_mf_gen_init(0.0, -3, 0.0, -3, 68, fwd);
    s = r2_mf_tone_codes;
    while (*s)
    {
        digit = *s++;
        for (i = 0;  i < 10;  i++)
        {
            len = my_mf_generate(amp, digit);
            codec_munge (amp, len);
            r2_mf_rx(mf_state, amp, len);
            actual = r2_mf_rx_get(mf_state);
            if (actual != digit)
            {
                printf("    Sent     '%c'\n", digit);
                printf("    Received 0x%X\n", actual);
                printf("    Failed\n");
                exit(2);
            }
        }
    }
    printf("    Passed\n");

    /* Test 3: Recognition bandwidth and channel centre frequency check.
       Use all digits. Each digit types requires four tests to complete
       the check. Each section contains 40 pulses of 68ms duration,
       with an amplitude of -20dB from clip per frequency.

       Four sections covering the tests for one tone (1 digit) are:
       a. H frequency at 0% deviation from center, L frequency at +0.1%.
          L frequency is then increments in +01.% steps up to +4%. The
          number of tone bursts is noted and designated N+.
       b. H frequency at 0% deviation, L frequency at -0.1%. L frequency
          is then incremental in -0.1% steps, up to -4%. The number of
          tone bursts is noted and designated N-.
       c. The test in (a) is repeated with the L frequency at 0% and the
          H frequency varied up to +4%.
       d. The test in (b) is repeated with the L frequency and 0% and the
          H frequency varied to -4%.

       Receiver Recognition Bandwidth (RRB) is calculated as follows:
            RRB% = (N+ + N-)/10
       Receiver Center Frequency Offset (RCFO) is calculated as follows:
            RCFO% = X + (N+ - N-)/20

       Note that this test doesn't test what it says it is testing at all,
       and the results are quite inaccurate, if not a downright lie! However,
       it follows the Mitel procedure, so how can it be bad? :)

       The spec calls for +-4 +-10Hz (ie +-14Hz) of bandwidth. */

    printf("Test 3: Recognition bandwidth and channel centre frequency check\n");
    s = r2_mf_tone_codes;
    j = 0;
    while (*s)
    {
        digit = *s++;
        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_mf_gen_init((float) i/1000.0, -17, 0.0, -17, 68, fwd);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            r2_mf_rx(mf_state, amp, len);
            if (r2_mf_rx_get(mf_state) == digit)
                nplus++;
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_mf_gen_init((float) i/1000.0, -17, 0.0, -17, 68, fwd);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            r2_mf_rx(mf_state, amp, len);
            if (r2_mf_rx_get(mf_state) == digit)
                nminus++;
        }
        rrb = (float) (nplus + nminus)/10.0;
        rcfo = (float) (nplus - nminus)/10.0;
        printf("    %c (low)  rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
               digit,
               rrb,
               rcfo,
               (float) nminus/10.0,
               (float) nplus/10.0);

        if (rrb < rcfo + (2.0*100.0*14.0/r2_mf_fwd_tones[j].f1)  ||  rrb >= 15.0 + rcfo)
        {
            printf("    Failed\n");
            exit(2);
        }

        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_mf_gen_init(0.0, -17, (float) i/1000.0, -17, 68, fwd);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            r2_mf_rx(mf_state, amp, len);
            if (r2_mf_rx_get(mf_state) == digit)
                nplus++;
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_mf_gen_init(0.0, -17, (float) i/1000.0, -17, 68, fwd);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            r2_mf_rx(mf_state, amp, len);
            if (r2_mf_rx_get(mf_state) == digit)
                nminus++;
        }
        rrb = (float) (nplus + nminus)/10.0;
        rcfo = (float) (nplus - nminus)/10.0;
        printf("    %c (high) rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
               digit,
               rrb,
               rcfo,
               (float) nminus/10.0,
               (float) nplus/10.0);
        if (rrb < rcfo + (2.0*100.0*14.0/r2_mf_fwd_tones[j].f2)  ||  rrb >= 15.0 + rcfo)
        {
            printf("    Failed\n");
            exit(2);
        }
        j++;
    }
    printf("    Passed\n");

    /* Test 4: Acceptable amplitude ratio (twist).
       Twist all digits in both directions, and check the maximum twist
       we can accept. The way this is done is styled after the Mitel DTMF
       test, and has good and bad points. */

    printf("Test 4: Acceptable amplitude ratio (twist)\n");
    s = r2_mf_tone_codes;
    while (*s)
    {
        digit = *s++;
        for (nplus = 0, i = -50;  i >= -250;  i--)
        {
            my_mf_gen_init(0.0, -5, 0.0, i/10, 68, fwd);

            len = my_mf_generate(amp, digit);
            codec_munge (amp, len);
            r2_mf_rx(mf_state, amp, len);
            if (r2_mf_rx_get(mf_state) == digit)
                nplus++;
        }
        printf("    %c normal twist  = %.2fdB\n", digit, (float) nplus/10.0);
        if (nplus < 70)
        {
            printf("    Failed\n");
            exit(2);
        }
        for (nminus = 0, i = -50;  i >= -250;  i--)
        {
            my_mf_gen_init(0.0, i/10, 0.0, -5, 68, fwd);

            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            r2_mf_rx(mf_state, amp, len);
            if (r2_mf_rx_get(mf_state) == digit)
                nminus++;
        }
        printf("    %c reverse twist = %.2fdB\n", digit, (float) nminus/10.0);
        if (nminus < 70)
        {
            printf("    Failed\n");
            exit(2);
        }
    }
    printf("    Passed\n");

    /* Test 5: Dynamic range
       This test sends all possible digits, with gradually increasing
       amplitude. We determine the span over which we achieve reliable
       detection. */

    printf("Test 5: Dynamic range\n");
    for (nplus = nminus = -1000, i = -50;  i <= 3;  i++)
    {
        s = r2_mf_tone_codes;
        while (*s)
        {
            digit = *s++;
            my_mf_gen_init(0.0, i, 0.0, i, 68, fwd);
            for (j = 0;  j < 100;  j++)
            {
                len = my_mf_generate(amp, digit);
                codec_munge(amp, len);
                r2_mf_rx(mf_state, amp, len);
                if (r2_mf_rx_get(mf_state) != digit)
                    break;
            }
            if (j < 100)
                break;
        }
        if (j == 100)
        {
            if (nplus == -1000)
                nplus = i;
        }
        else
        {
            if (nplus != -1000  &&  nminus == -1000)
                nminus = i;
        }
    }
    printf("    Dynamic range = %ddB to %ddB\n", nplus, nminus - 1);
    if (nplus > -35  ||  nminus <= -5)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    /* Test 6: Guard time
       This test sends all possible digits, with a gradually reducing
       duration. */

    printf("Test 6: Guard time\n");
    for (i = 30;  i < 62;  i++)
    {
        s = r2_mf_tone_codes;
        j = 0;
        while (*s)
        {
            digit = *s++;
            my_mf_gen_init(0.0, -5, 0.0, -3, i, fwd);
            for (j = 0;  j < 500;  j++)
            {
                len = my_mf_generate(amp, digit);
                codec_munge(amp, len);
                r2_mf_rx(mf_state, amp, len);
                if (r2_mf_rx_get(mf_state) != digit)
                    break;
            }
            if (j < 500)
                break;
        }
        if (j == 500)
            break;
    }
    printf("    Guard time = %dms\n", i);
    if (i > 61)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    /* Test 7: Acceptable signal to noise ratio
       We send all possible digits at -6dBm from clip, mixed with AWGN.
       We gradually reduce the noise until we get clean detection. */

    printf("Test 7: Acceptable signal to noise ratio\n");
    my_mf_gen_init(0.0, -3, 0.0, -3, 68, fwd);
    for (i = -3;  i > -50;  i--)
    {
        s = r2_mf_tone_codes;
        while (*s)
        {
            digit = *s++;
            noise_source = awgn_init_dbm0(NULL, 1234567, (float) i);
            for (j = 0;  j < 500;  j++)
            {
                len = my_mf_generate(amp, digit);
                for (sample = 0;  sample < len;  sample++)
                    amp[sample] = sat_add16(amp[sample], awgn(noise_source));
                codec_munge(amp, len);
                r2_mf_rx(mf_state, amp, len);
                if (r2_mf_rx_get(mf_state) != digit)
                    break;
            }
            if (j < 500)
                break;
        }
        if (j == 500)
            break;
    }
    printf("    Acceptable S/N ratio is %ddB\n", -3 - i);
    if (-3 - i > 26)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    printf("Test 8: Callback digit delivery mode.\n");
    callback_ok = false;
    callback_roll = 0;
    mf_state = r2_mf_rx_init(NULL, fwd, digit_delivery, (void *) 0x12345678);
    my_mf_gen_init(0.0, -3, 0.0, -3, 68, fwd);
    s = r2_mf_tone_codes;
    noise_source = awgn_init_dbm0(NULL, 1234567, -40.0f);
    while (*s)
    {
        digit = *s++;
        len = my_mf_generate(amp, digit);
        for (sample = 0;  sample < len;  sample++)
            amp[sample] = sat_add16(amp[sample], awgn(noise_source));
        codec_munge(amp, len);
        r2_mf_rx(mf_state, amp, len);
        len = 160;
        memset(amp, '\0', len*sizeof(int16_t));
        for (sample = 0;  sample < len;  sample++)
            amp[sample] = sat_add16(amp[sample], awgn(noise_source));
        codec_munge(amp, len);
        r2_mf_rx(mf_state, amp, len);
    }
    if (!callback_ok)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    /* The remainder of the Mitel tape is the talk-off test. This is
       meaningless for R2 MF. However the decoder's tolerance of
       out of band noise is significant. */
    /* TODO: add a OOB tolerance test. */

    return 0;
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    time_t now;
    time_t duration;

    now = time(NULL);
    printf("R2 forward tones\n");
    test_a_tone_set(true);
    printf("R2 backward tones\n");
    test_a_tone_set(false);
    duration = time(NULL) - now;
    printf("Tests passed in %lds\n", duration);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

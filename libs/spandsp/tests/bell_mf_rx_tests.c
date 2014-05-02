/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bell_mf_tests.c - Test the Bell MF detector against the spec., whatever the
 *                   spec. may be :)
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

/*! \page bell_mf_tests_page Bell MF tone generation and detection tests
\section bell_mf_tests_page_sec_1 What does it do?
These tests are fashioned after those on the CM7291 test tape from
Mitel. Those tests are for DTMF, rather than Bell MF, but make a
fair starting point for a set of meaningful tests of Bell MF.

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

/* Basic Bell MF specs:
 *
 * Signal generation:
 *   Tone on time = KP: 100+-7ms. All other signals: 68+-7ms
 *   Tone off time (between digits) = 68+-7ms
 *   Frequency tolerance +- 1.5%
 *   Signal level -7+-1dBm per frequency
 *
 * Signal reception:
 *   Frequency tolerance +- 1.5% +-10Hz
 *   Signal level -14dBm to 0dBm
 *   Perform a "two and only two tones present" test.
 *   Twist <= 6dB accepted
 *   Receiver sensitive to signals above -22dBm per frequency
 *   Test for a minimum of 55ms if KP, or 30ms of other signals.
 *   Signals to be recognised if the two tones arrive within 8ms of each other.
 *   Invalid signals result in the return of the re-order tone.
 */

#define MF_DURATION                 (68*8)
#define MF_PAUSE                    (68*8)
#define MF_CYCLE                    (MF_DURATION + MF_PAUSE)

/*!
    MF tone descriptor for tests.
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

static const mf_digit_tones_t bell_mf_tones[] =
{
    { 700.0,  900.0, -7, -7,  68, 68},
    { 700.0, 1100.0, -7, -7,  68, 68},
    { 900.0, 1100.0, -7, -7,  68, 68},
    { 700.0, 1300.0, -7, -7,  68, 68},
    { 900.0, 1300.0, -7, -7,  68, 68},
    {1100.0, 1300.0, -7, -7,  68, 68},
    { 700.0, 1500.0, -7, -7,  68, 68},
    { 900.0, 1500.0, -7, -7,  68, 68},
    {1100.0, 1500.0, -7, -7,  68, 68},
    {1300.0, 1500.0, -7, -7,  68, 68},
    { 700.0, 1700.0, -7, -7,  68, 68}, /* ST''' - use 'C' */
    { 900.0, 1700.0, -7, -7,  68, 68}, /* ST'   - use 'A' */
    {1100.0, 1700.0, -7, -7, 100, 68}, /* KP    - use '*' */
    {1300.0, 1700.0, -7, -7,  68, 68}, /* ST''  - use 'B' */
    {1500.0, 1700.0, -7, -7,  68, 68}, /* ST    - use '#' */
    {0.0, 0.0, 0, 0, 0, 0}
};

static tone_gen_descriptor_t my_mf_digit_tones[16];

static char bell_mf_tone_codes[] = "1234567890CA*B#";

bool callback_ok;
int callback_roll;

static void my_mf_gen_init(float low_fudge,
                           int low_level,
                           float high_fudge,
                           int high_level,
                           int duration,
                           int gap)
{
    int i;

    /* The fiddle factor on the tone duration is to make KP consistently
       50% longer than the other digits, as the digit durations are varied
       for the tests. This is an approximation, as the real scaling should
       be 100/68 */
    for (i = 0;  i < 15;  i++)
    {
        tone_gen_descriptor_init(&my_mf_digit_tones[i],
                                 bell_mf_tones[i].f1*(1.0 + low_fudge),
                                 low_level,
                                 bell_mf_tones[i].f2*(1.0 + high_fudge),
                                 high_level,
                                 (i == 12)  ?  3*duration/2  :  duration,
                                 gap,
                                 0,
                                 0,
                                 false);
    }
}
/*- End of function --------------------------------------------------------*/

static int my_mf_generate(int16_t amp[], const char *digits)
{
    int len;
    char *cp;
    tone_gen_state_t tone;

    len = 0;
    while (*digits)
    {
        if ((cp = strchr(bell_mf_tone_codes, *digits)))
        {
            tone_gen_init(&tone, &my_mf_digit_tones[cp - bell_mf_tone_codes]);
            len += tone_gen(&tone, amp + len, 9999);
        }
        digits++;
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

#define ALL_POSSIBLE_DIGITS     "1234567890CA*B#"

static void digit_delivery(void *data, const char *digits, int len)
{
    int i;
    int seg;
    const char *s = ALL_POSSIBLE_DIGITS;
    const char *t;

    if (data != (void *) 0x12345678)
    {
        callback_ok = false;
        return;
    }
    callback_ok = true;
    t = s + callback_roll;
    seg = 15 - callback_roll;
    for (i = 0;  i < len;  i += seg, seg = 15)
    {
        if (i + seg > len)
            seg = len - i;
        if (memcmp(digits + i, t, seg))
        {
            callback_ok = false;
            printf("Fail at %d %d\n", i, seg);
            break;
        }
        t = s;
        callback_roll = (callback_roll + seg)%15;
    }
}
/*- End of function --------------------------------------------------------*/

static int16_t amp[1000000];

int main(int argc, char *argv[])
{
    int duration;
    int i;
    int j;
    int len;
    int sample;
    const char *s;
    char digit[2];
    char buf[MAX_BELL_MF_DIGITS + 1];
    int actual;
    int nplus;
    int nminus;
    float rrb;
    float rcfo;
    time_t now;
    bell_mf_rx_state_t *mf_state;
    awgn_state_t noise_source;

    time(&now);
    mf_state = bell_mf_rx_init(NULL, NULL, NULL);

    /* Test 1: Mitel's test 1 isn't really a test. Its a calibration step,
       which has no meaning here. */
    printf("Test 1: Calibration\n");
    printf("    Passed\n");

    /* Test 2: Decode check
       This is a sanity check, that all digits are reliably detected
       under ideal conditions.  Each possible digit is repeated 10 times,
       with 68ms bursts. The level of each tone is about 6dB down from clip */
    printf("Test 2: Decode check\n");
    my_mf_gen_init(0.0, -3, 0.0, -3, 68, 68);
    s = ALL_POSSIBLE_DIGITS;
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (i = 0;  i < 10;  i++)
        {
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            actual = bell_mf_rx_get(mf_state, buf, 128);
            if (actual != 1  ||  buf[0] != digit[0])
            {
                printf("    Sent     '%s'\n", digit);
                printf("    Received '%s' [%d]\n", buf, actual);
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

       The spec calls for +-1.5% +-10Hz of bandwidth.
    */
    printf("Test 3: Recognition bandwidth and channel centre frequency check\n");
    s = ALL_POSSIBLE_DIGITS;
    digit[1] = '\0';
    j = 0;
    while (*s)
    {
        digit[0] = *s++;
        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_mf_gen_init((float) i/1000.0, -17, 0.0, -17, 68, 68);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            nplus += bell_mf_rx_get(mf_state, buf, 128);
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_mf_gen_init((float) i/1000.0, -17, 0.0, -17, 68, 68);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            nminus += bell_mf_rx_get(mf_state, buf, 128);
        }
        rrb = (float) (nplus + nminus)/10.0;
        rcfo = (float) (nplus - nminus)/10.0;
        printf("    %c (low)  rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
               digit[0],
               rrb,
               rcfo,
               (float) nminus/10.0,
               (float) nplus/10.0);

        if (rrb < 3.0 + rcfo + (2.0*100.0*10.0/bell_mf_tones[j].f1)  ||  rrb >= 15.0 + rcfo)
        {
            printf("    Failed\n");
            exit(2);
        }

        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_mf_gen_init(0.0, -17, (float) i/1000.0, -17, 68, 68);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            nplus += bell_mf_rx_get(mf_state, buf, 128);
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_mf_gen_init(0.0, -17, (float) i/1000.0, -17, 68, 68);
            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            nminus += bell_mf_rx_get(mf_state, buf, 128);
        }
        rrb = (float) (nplus + nminus)/10.0;
        rcfo = (float) (nplus - nminus)/10.0;
        printf("    %c (high) rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
               digit[0],
               rrb,
               rcfo,
               (float) nminus/10.0,
               (float) nplus/10.0);
        if (rrb < 3.0 + rcfo + (2.0*100.0*10.0/bell_mf_tones[j].f2)  ||  rrb >= 15.0 + rcfo)
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
    s = ALL_POSSIBLE_DIGITS;
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (nplus = 0, i = -50;  i >= -250;  i--)
        {
            my_mf_gen_init(0.0, -5, 0.0, i/10, 68, 68);

            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            nplus += bell_mf_rx_get(mf_state, buf, 128);
        }
        printf("    %c normal twist  = %.2fdB\n", digit[0], (float) nplus/10.0);
        if (nplus < 60)
        {
            printf("    Failed\n");
            exit(2);
        }
        for (nminus = 0, i = -50;  i >= -250;  i--)
        {
            my_mf_gen_init(0.0, i/10, 0.0, -5, 68, 68);

            len = my_mf_generate(amp, digit);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            nminus += bell_mf_rx_get(mf_state, buf, 128);
        }
        printf("    %c reverse twist = %.2fdB\n", digit[0], (float) nminus/10.0);
        if (nminus < 60)
        {
            printf("    Failed\n");
            exit(2);
        }
    }
    printf("    Passed\n");

    /* Test 5: Dynamic range
       This test sends all possible digits, with gradually increasing
       amplitude. We determine the span over which we achieve reliable
       detection. The spec says we should detect between -14dBm and 0dBm,
       but the tones clip above -3dBm, so this cannot really work. */

    printf("Test 5: Dynamic range\n");
    for (nplus = nminus = -1000, i = -50;  i <= 3;  i++)
    {
        my_mf_gen_init(0.0, i, 0.0, i, 68, 68);
        for (j = 0;  j < 100;  j++)
        {
            len = my_mf_generate(amp, ALL_POSSIBLE_DIGITS);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            if (bell_mf_rx_get(mf_state, buf, 128) != 15)
                break;
            if (strcmp(buf, ALL_POSSIBLE_DIGITS) != 0)
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
    if (nplus > -22  ||  nminus <= -3)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    /* Test 6: Guard time
       This test sends all possible digits, with a gradually reducing
       duration. The spec defines a narrow range of tone duration
       times we can expect, so as long as we detect reliably at the
       specified minimum we should be OK. However, the spec also says
       we should detect on a minimum of 55ms of KP, or 30ms of other
       digits. */

    printf("Test 6: Guard time\n");
    for (i = 30;  i < 62;  i++)
    {
        my_mf_gen_init(0.0, -5, 0.0, -3, i, 68);
        for (j = 0;  j < 500;  j++)
        {
            len = my_mf_generate(amp, ALL_POSSIBLE_DIGITS);
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            if (bell_mf_rx_get(mf_state, buf, 128) != 15)
                break;
            if (strcmp(buf, ALL_POSSIBLE_DIGITS) != 0)
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
    my_mf_gen_init(0.0, -3, 0.0, -3, 68, 68);
    for (i = -10;  i > -50;  i--)
    {
        awgn_init_dbm0(&noise_source, 1234567, (float) i);
        for (j = 0;  j < 500;  j++)
        {
            len = my_mf_generate(amp, ALL_POSSIBLE_DIGITS);
            for (sample = 0;  sample < len;  sample++)
                amp[sample] = sat_add16(amp[sample], awgn(&noise_source));
            codec_munge(amp, len);
            bell_mf_rx(mf_state, amp, len);
            if (bell_mf_rx_get(mf_state, buf, 128) != 15)
                break;
            if (strcmp(buf, ALL_POSSIBLE_DIGITS) != 0)
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

    /* The remainder of the Mitel tape is the talk-off test. This is
       meaningless for Bell MF. However the decoder's tolerance of
       out of band noise is significant. */
    /* TODO: add a OOB tolerance test. */

    /* Test the callback mode for delivering detected digits */

    printf("Test: Callback digit delivery mode.\n");
    callback_ok = false;
    callback_roll = 0;
    mf_state = bell_mf_rx_init(NULL, digit_delivery, (void *) 0x12345678);
    my_mf_gen_init(0.0, -10, 0.0, -10, 68, 68);
    for (i = 1;  i < 10;  i++)
    {
        len = 0;
        for (j = 0;  j < i;  j++)
            len += my_mf_generate(amp + len, ALL_POSSIBLE_DIGITS);
        bell_mf_rx(mf_state, amp, len);
        if (!callback_ok)
            break;
    }
    if (!callback_ok)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    duration = time (NULL) - now;
    printf("Tests passed in %ds\n", duration);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

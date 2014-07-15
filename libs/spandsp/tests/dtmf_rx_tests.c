/*
 * SpanDSP - a series of DSP components for telephony
 *
 * dtmf_rx_tests.c - Test the DTMF detector against the spec., whatever the spec.
 *                   may be :)
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001, 2006 Steve Underwood
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

/*
 * These tests include conversion to and from A-law. I assume the
 * distortion this produces is comparable to u-law, so it should be
 * a fair test.
 *
 * These tests mirror those on the CM7291 test tape from Mitel.
 * Many of these tests are highly questionable, but they are a
 * well accepted industry standard.
 *
 * However standard these tests might be, Mitel appears to have stopped
 * selling copies of their tape.
 *
 * For the talk-off test the Bellcore tapes may be used. However, they are
 * copyright material, so the test data files produced from the Bellcore
 * tapes cannot be distributed as a part of this package.
 *
 */

/*! \page dtmf_rx_tests_page DTMF receiver tests
\section dtmf_rx_tests_page_sec_1 What does it do?

The DTMF detection test suite performs similar tests to the Mitel test tape,
traditionally used for testing DTMF receivers. Mitel seem to have discontinued
this product, but all it not lost.

The first side of the Mitel tape consists of a number of tone and tone+noise
based tests. The test suite synthesizes equivalent test data. Being digitally
generated, this data is rather more predictable than the test data on the nasty
old stretchy cassette tapes which Mitel sold.

The second side of the Mitel tape contains fragments of real speech from real
phone calls captured from the North American telephone network. These are
considered troublesome for DTMF detectors. A good detector is expected to
achieve a reasonably low number of false detections on this data. Fresh clean
copies of this seem to be unobtainable. However, Bellcore produce a much more
aggressive set of three cassette tapes. All six side (about 30 minutes each) are
filled with much tougher fragments of real speech from the North American
telephone network. If you can do well in this test, nobody cares about your
results against the Mitel test tape.

A fresh set of tapes was purchased for these tests, and digitised, producing 6
wave files of 16 bit signed PCM data, sampled at 8kHz. They were transcribed
using a speed adjustable cassette player. The test tone at the start of the
tapes is pretty accurate, and the new tapes should not have had much opportunity
to stretch. It is believed these transcriptions are about as good as the source
material permits.

PLEASE NOTE

These transcriptions may be freely used by anyone who has a legitimate copy of
the original tapes. However, if you don't have a legitimate copy of those tapes,
you also have no right to use this data. The original tapes are the copyright
material of BellCore, and they charge over US$200 for a set. I doubt they sell
enough copies to consider this much of a business. However, it is their data,
and it is their right to do as they wish with it. Currently I see no indication
they wish to give it away for free.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sndfile.h>

#include "spandsp.h"
#include "spandsp-sim.h"

/* Basic DTMF specs:
 *
 * Minimum tone on = 40ms
 * Minimum tone off = 50ms
 * Maximum digit rate = 10 per second
 * Normal twist <= 8dB accepted
 * Reverse twist <= 4dB accepted
 * S/N >= 15dB will detect OK
 * Attenuation <= 26dB will detect OK
 * Frequency tolerance +- 1.5% will detect, +-3.5% will reject
 */

#define DEFAULT_DTMF_TX_LEVEL       -10
#define DEFAULT_DTMF_TX_ON_TIME     50
#define DEFAULT_DTMF_TX_OFF_TIME    50

#define SAMPLES_PER_CHUNK           160

#define ALL_POSSIBLE_DIGITS         "123A456B789C*0#D"

#define MITEL_DIR                   "../test-data/mitel/"
#define BELLCORE_DIR                "../test-data/bellcore/"

const char *bellcore_files[] =
{
    MITEL_DIR    "mitel-cm7291-talkoff.wav",
    BELLCORE_DIR "tr-tsy-00763-1.wav",
    BELLCORE_DIR "tr-tsy-00763-2.wav",
    BELLCORE_DIR "tr-tsy-00763-3.wav",
    BELLCORE_DIR "tr-tsy-00763-4.wav",
    BELLCORE_DIR "tr-tsy-00763-5.wav",
    BELLCORE_DIR "tr-tsy-00763-6.wav",
    ""
};

static tone_gen_descriptor_t my_dtmf_digit_tones[16];

float dtmf_row[] =
{
     697.0f,  770.0f,  852.0f,  941.0f
};
float dtmf_col[] =
{
    1209.0f, 1336.0f, 1477.0f, 1633.0f
};

char dtmf_positions[] = "123A" "456B" "789C" "*0#D";

bool callback_hit;
bool callback_ok;
int callback_roll;
int step;

float max_forward_twist;
float max_reverse_twist;

bool use_dialtone_filter = false;

char *decode_test_file = NULL;

static int16_t amp[1000000];
static int16_t amp2[1000000];

codec_munge_state_t *munge = NULL;

static void my_dtmf_gen_init(float low_fudge,
                             int low_level,
                             float high_fudge,
                             int high_level,
                             int duration,
                             int gap)
{
    int row;
    int col;

    for (row = 0;  row < 4;  row++)
    {
        for (col = 0;  col < 4;  col++)
        {
            tone_gen_descriptor_init(&my_dtmf_digit_tones[row*4 + col],
                                     dtmf_row[row]*(1.0f + low_fudge),
                                     low_level,
                                     dtmf_col[col]*(1.0f + high_fudge),
                                     high_level,
                                     duration,
                                     gap,
                                     0,
                                     0,
                                     false);
        }
    }
}
/*- End of function --------------------------------------------------------*/

static int my_dtmf_generate(int16_t amp[], const char *digits)
{
    int len;
    char *cp;
    tone_gen_state_t tone;

    len = 0;
    while (*digits)
    {
        cp = strchr(dtmf_positions, *digits);
        if (cp)
        {
            tone_gen_init(&tone, &my_dtmf_digit_tones[cp - dtmf_positions]);
            len += tone_gen(&tone, amp + len, 1000);
        }
        digits++;
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

static void digit_delivery(void *data, const char *digits, int len)
{
    int i;
    int seg;
    const char *s = ALL_POSSIBLE_DIGITS;
    const char *t;

    callback_hit = true;
    if (data == (void *) 0x12345678)
    {
        t = s + callback_roll;
        seg = 16 - callback_roll;
        for (i = 0;  i < len;  i += seg, seg = 16)
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
            callback_roll = (callback_roll + seg)%16;
        }
    }
    else
    {
        callback_ok = false;
    }
}
/*- End of function --------------------------------------------------------*/

static void digit_status(void *data, int signal, int level, int delay)
{
    const char *s = ALL_POSSIBLE_DIGITS;
    int len;
    static int last_step = 0;
    static int first = true;

    //printf("Digit status %d %d %d\n", signal, level, delay);
    callback_hit = true;
    len = step - last_step;
    if (data == (void *) 0x12345678)
    {
        if (len < 320  ||  len > 480)
        {
            if (first)
            {
                /* At the beginning the apparent duration is expected to be wrong */
                first = false;
            }
            else
            {
                printf("Failed for signal %s length %d at %d\n", (callback_roll & 1)  ?  "on"  :  "off", len, step);
                callback_ok = false;
            }
        }
        if (callback_roll & 1)
        {
            if (signal != 0)
            {
                printf("Failed for signal 0x%X instead of 0\n", signal);
                callback_ok = false;
            }
        }
        else
        {
            if (signal != s[callback_roll >> 1])
            {
                printf("Failed for signal 0x%X instead of 0x%X\n", signal, s[callback_roll >> 1]);
                callback_ok = false;
            }
            if (level < DEFAULT_DTMF_TX_LEVEL + 3 - 1  ||  level > DEFAULT_DTMF_TX_LEVEL + 3 + 1)
            {
                printf("Failed for level %d instead of %d\n", level, DEFAULT_DTMF_TX_LEVEL + 3);
                callback_ok = false;
            }
        }
        if (++callback_roll >= 32)
            callback_roll = 0;
    }
    else
    {
        callback_ok = false;
    }
    last_step = step;
}
/*- End of function --------------------------------------------------------*/

static void mitel_cm7291_side_1_tests(void)
{
    int i;
    int j;
    int len;
    int sample;
    const char *s;
    char digit[2];
    char buf[128 + 1];
    int actual;
    int nplus;
    int nminus;
    float rrb;
    float rcfo;
    dtmf_rx_state_t *dtmf_state;
    awgn_state_t noise_source;
    logging_state_t *logging;

    dtmf_state = dtmf_rx_init(NULL, NULL, NULL);
    if (use_dialtone_filter  ||  max_forward_twist >= 0.0f  ||  max_reverse_twist >= 0.0f)
        dtmf_rx_parms(dtmf_state, use_dialtone_filter, max_forward_twist, max_reverse_twist, -99.0f);
    logging = dtmf_rx_get_logging_state(dtmf_state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "DTMF-rx");

    /* Test 1: Mitel's test 1 isn't really a test. Its a calibration step,
       which has no meaning here. */
    printf("Test 1: Calibration\n");
    printf("    Passed\n");

    /* Test 2: Decode check
       This is a sanity check, that all digits are reliably detected
       under ideal conditions.  Each possible digit repeated 10 times,
       with 50ms bursts. The level of each tone is about 6dB down from clip.
       6dB down actually causes trouble with G.726, so we use 7dB down. */
    printf("Test 2: Decode check\n");
    my_dtmf_gen_init(0.0f, -4, 0.0f, -4, 50, 50);
    s = ALL_POSSIBLE_DIGITS;
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (i = 0;  i < 10;  i++)
        {
            len = my_dtmf_generate(amp, digit);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);

            actual = dtmf_rx_get(dtmf_state, buf, 128);

            if (actual != 1  ||  buf[0] != digit[0])
            {
                printf("    Sent     '%s'\n", digit);
                printf("    Received '%s'\n", buf);
                printf("    Failed\n");
                exit(2);
            }
        }
    }
    printf("    Passed\n");

    /* Test 3: Recognition bandwidth and channel centre frequency check.
       Use only the diagonal pairs of tones (digits 1, 5, 9 and D). Each
       tone pair requires four test to complete the check, making 16
       sections overall. Each section contains 40 pulses of
       50ms duration, with an amplitude of -20dB from clip per
       frequency.

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
    */
    printf("Test 3: Recognition bandwidth and channel centre frequency check\n");
    s = "159D";
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_dtmf_gen_init((float) i/1000.0f, -17, 0.0f, -17, 50, 50);
            len = my_dtmf_generate(amp, digit);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);
            nplus += dtmf_rx_get(dtmf_state, buf, 128);
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_dtmf_gen_init((float) i/1000.0f, -17, 0.0f, -17, 50, 50);
            len = my_dtmf_generate(amp, digit);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);
            nminus += dtmf_rx_get(dtmf_state, buf, 128);
        }
        rrb = (float) (nplus + nminus)/10.0f;
        rcfo = (float) (nplus - nminus)/10.0f;
        printf("    %c (low)  rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
               digit[0],
               rrb,
               rcfo,
               (float) nminus/10.0f,
               (float) nplus/10.0f);
        if (rrb < 3.0f + rcfo  ||  rrb >= 15.0f + rcfo)
        {
            printf("    Failed\n");
            exit(2);
        }

        for (nplus = 0, i = 1;  i <= 60;  i++)
        {
            my_dtmf_gen_init(0.0f, -17, (float) i/1000.0f, -17, 50, 50);
            len = my_dtmf_generate(amp, digit);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);
            nplus += dtmf_rx_get(dtmf_state, buf, 128);
        }
        for (nminus = 0, i = -1;  i >= -60;  i--)
        {
            my_dtmf_gen_init(0.0f, -17, (float) i/1000.0f, -17, 50, 50);
            len = my_dtmf_generate(amp, digit);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);
            nminus += dtmf_rx_get(dtmf_state, buf, 128);
        }
        rrb = (float) (nplus + nminus)/10.0f;
        rcfo = (float) (nplus - nminus)/10.0f;
        printf("    %c (high) rrb = %5.2f%%, rcfo = %5.2f%%, max -ve = %5.2f, max +ve = %5.2f\n",
               digit[0],
               rrb,
               rcfo,
               (float) nminus/10.0f,
               (float) nplus/10.0f);
        if (rrb < 3.0f + rcfo  ||  rrb >= 15.0f + rcfo)
        {
            printf("    Failed\n");
            exit(2);
        }
    }
    printf("    Passed\n");

    /* Test 4: Acceptable amplitude ratio (twist).
       Use only the diagonal pairs of tones (digits 1, 5, 9 and D).
       There are eight sections to the test. Each section contains 200
       pulses with a 50ms duration for each pulse. Initially the amplitude
       of both tones is 6dB down from clip. The two sections to test one
       tone pair are:

       a. Standard Twist: H tone amplitude is maintained at -6dB from clip,
          L tone amplitude is attenuated gradually until the amplitude ratio
          L/H is -20dB. Note the number of responses from the receiver.
       b. Reverse Twist: L tone amplitude is maintained at -6dB from clip,
          H tone amplitude is attenuated gradually until the amplitude ratio
          is 20dB. Note the number of responses from the receiver.

       All tone bursts are of 50ms duration.

       The Acceptable Amplitude Ratio in dB is equal to the number of
       responses registered in (a) or (b), divided by 10.

       TODO: This is supposed to work in 1/10dB steps, but here I used 1dB
             steps, as the current tone generator has its amplitude set in
             1dB steps.
    */
    printf("Test 4: Acceptable amplitude ratio (twist)\n");
    s = "159D";
    digit[1] = '\0';
    while (*s)
    {
        digit[0] = *s++;
        for (nplus = 0, i = -30;  i >= -230;  i--)
        {
            my_dtmf_gen_init(0.0f, -3, 0.0f, i/10, 50, 50);

            len = my_dtmf_generate(amp, digit);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);
            nplus += dtmf_rx_get(dtmf_state, buf, 128);
        }
        printf("    %c normal twist  = %.2fdB\n", digit[0], (float) nplus/10.0);
        if (nplus < 80)
        {
            printf("    Failed\n");
            exit(2);
        }
        for (nminus = 0, i = -30;  i >= -230;  i--)
        {
            my_dtmf_gen_init(0.0f, i/10, 0.0f, -3, 50, 50);

            len = my_dtmf_generate(amp, digit);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);
            nminus += dtmf_rx_get(dtmf_state, buf, 128);
        }
        printf("    %c reverse twist = %.2fdB\n", digit[0], (float) nminus/10.0);
        if (nminus < 40)
        {
            printf("    Failed\n");
            exit(2);
        }
    }
    printf("    Passed\n");

    /* Test 5: Dynamic range
       This test utilizes tone pair L1 H1 (digit 1). Thirty-five tone pair
       pulses are transmitted, with both frequencies stating at -6dB from
       clip. The amplitude of each is gradually attenuated by -35dB at a
       rate of 1dB per pulse. The Dynamic Range in dB is equal to the
       number of responses from the receiver during the test.

       Well not really, but that is the Mitel test. Lets sweep a bit further,
       and see what the real range is */
    printf("Test 5: Dynamic range\n");
    for (nplus = 0, i = +3;  i >= -50;  i--)
    {
        my_dtmf_gen_init(0.0f, i, 0.0f, i, 50, 50);

        len = my_dtmf_generate(amp, "1");
        codec_munge(munge, amp, len);
        dtmf_rx(dtmf_state, amp, len);
        nplus += dtmf_rx_get(dtmf_state, buf, 128);
    }
    printf("    Dynamic range = %ddB\n", nplus);
    /* We ought to set some pass/fail condition, even if Mitel did not. If
       we don't, regression testing is weakened. */
    if (nplus < 35)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    /* Test 6: Guard time
       This test utilizes tone pair L1 H1 (digit 1). Four hundred pulses
       are transmitted at an amplitude of -6dB from clip per frequency.
       Pulse duration starts at 49ms and is gradually reduced to 10ms.
       Guard time in ms is equal to (500 - number of responses)/10.

       That is the Mitel test, and we will follow it. Its totally bogus,
       though. Just what the heck is a pass or fail here? */

    printf("Test 6: Guard time\n");
    for (nplus = 0, i = 490;  i >= 100;  i--)
    {
        my_dtmf_gen_init(0.0f, -3, 0.0f, -3, i/10, 50);

        len = my_dtmf_generate(amp, "1");
        codec_munge(munge, amp, len);
        dtmf_rx(dtmf_state, amp, len);
        nplus += dtmf_rx_get(dtmf_state, buf, 128);
    }
    printf("    Guard time = %dms\n", (500 - nplus)/10);
    printf("    Passed\n");

    /* Test 7: Acceptable signal to noise ratio
       This test utilizes tone pair L1 H1, transmitted on a noise background.
       The test consists of three sections in which the tone pair is
       transmitted 1000 times at an amplitude -6dB from clip per frequency,
       but with a different white noise level for each section. The first
       level is -24dBV, the second -18dBV and the third -12dBV.. The
       acceptable signal to noise ratio is the lowest ratio of signal
       to noise in the test where the receiver responds to all 1000 pulses.

       Well, that is the Mitel test, but it doesn't tell you what the
       decoder can really do. Lets do a more comprehensive test */

    printf("Test 7: Acceptable signal to noise ratio\n");
    my_dtmf_gen_init(0.0f, -4, 0.0f, -4, 50, 50);

    for (j = -13;  j > -50;  j--)
    {
        awgn_init_dbm0(&noise_source, 1234567, (float) j);
        for (i = 0;  i < 1000;  i++)
        {
            len = my_dtmf_generate(amp, "1");

            // TODO: Clip
            for (sample = 0;  sample < len;  sample++)
                amp[sample] = sat_add16(amp[sample], awgn(&noise_source));

            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);

            if (dtmf_rx_get(dtmf_state, buf, 128) != 1)
                break;
        }
        if (i == 1000)
            break;
    }
    printf("    Acceptable S/N ratio is %ddB\n", -4 - j);
    if (-4 - j > 26)
    {
        printf("    Failed\n");
        exit(2);
    }
    dtmf_rx_free(dtmf_state);
    printf("    Passed\n");
}
/*- End of function --------------------------------------------------------*/

static void mitel_cm7291_side_2_and_bellcore_tests(void)
{
    int i;
    int j;
    int len;
    int hits;
    int hit_types[256];
    char buf[128 + 1];
    SNDFILE *inhandle;
    int frames;
    dtmf_rx_state_t *dtmf_state;
    logging_state_t *logging;

    dtmf_state = dtmf_rx_init(NULL, NULL, NULL);
    if (use_dialtone_filter  ||  max_forward_twist >= 0.0f  ||  max_reverse_twist >= 0.0f)
        dtmf_rx_parms(dtmf_state, use_dialtone_filter, max_forward_twist, max_reverse_twist, -99.0f);
    logging = dtmf_rx_get_logging_state(dtmf_state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "DTMF-rx");

    /* The remainder of the Mitel tape is the talk-off test */
    /* Here we use the Bellcore test tapes (much tougher), in six
      files - 1 from each side of the original 3 cassette tapes */
    /* Bellcore say you should get no more than 470 false detections with
       a good receiver. Dialogic claim 20. Of course, we can do better than
       that, eh? */
    printf("Test 8: Talk-off test\n");
    memset(hit_types, '\0', sizeof(hit_types));
    for (j = 0;  bellcore_files[j][0];  j++)
    {
        if ((inhandle = sf_open_telephony_read(bellcore_files[j], 1)) == NULL)
        {
            printf("    Cannot open speech file '%s'\n", bellcore_files[j]);
            exit(2);
        }
        hits = 0;
        while ((frames = sf_readf_short(inhandle, amp, SAMPLE_RATE)))
        {
            dtmf_rx(dtmf_state, amp, frames);
            len = dtmf_rx_get(dtmf_state, buf, 128);
            if (len > 0)
            {
                for (i = 0;  i < len;  i++)
                    hit_types[(int) buf[i]]++;
                hits += len;
            }
        }
        if (sf_close_telephony(inhandle))
        {
            printf("    Cannot close speech file '%s'\n", bellcore_files[j]);
            exit(2);
        }
        printf("    File %d gave %d false hits.\n", j + 1, hits);
    }
    for (i = 0, j = 0;  i < 256;  i++)
    {
        if (hit_types[i])
        {
            printf("    Digit %c had %d false hits.\n", i, hit_types[i]);
            j += hit_types[i];
        }
    }
    printf("    %d false hits in total.\n", j);
    if (j > 470)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");
    dtmf_rx_free(dtmf_state);
}
/*- End of function --------------------------------------------------------*/

static void dial_tone_tolerance_tests(void)
{
    int i;
    int j;
    int len;
    int sample;
    char buf[128 + 1];
    dtmf_rx_state_t *dtmf_state;
    tone_gen_descriptor_t dial_tone_desc;
    tone_gen_state_t dial_tone;
    logging_state_t *logging;

    dtmf_state = dtmf_rx_init(NULL, NULL, NULL);
    if (use_dialtone_filter  ||  max_forward_twist >= 0.0f  ||  max_reverse_twist >= 0.0f)
        dtmf_rx_parms(dtmf_state, use_dialtone_filter, max_forward_twist, max_reverse_twist, -99.0f);
    logging = dtmf_rx_get_logging_state(dtmf_state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "DTMF-rx");

    /* Test dial tone tolerance */
    printf("Test: Dial tone tolerance.\n");
    my_dtmf_gen_init(0.0f, -15, 0.0f, -15, DEFAULT_DTMF_TX_ON_TIME, DEFAULT_DTMF_TX_OFF_TIME);

    for (j = -30;  j < -3;  j++)
    {
        tone_gen_descriptor_init(&dial_tone_desc, 350, j, 440, j, 1, 0, 0, 0, true);
        tone_gen_init(&dial_tone, &dial_tone_desc);
        for (i = 0;  i < 10;  i++)
        {
            len = my_dtmf_generate(amp, ALL_POSSIBLE_DIGITS);
            tone_gen(&dial_tone, amp2, len);

            for (sample = 0;  sample < len;  sample++)
                amp[sample] = sat_add16(amp[sample], amp2[sample]);
            codec_munge(munge, amp, len);
            dtmf_rx(dtmf_state, amp, len);

            if (dtmf_rx_get(dtmf_state, buf, 128) != strlen(ALL_POSSIBLE_DIGITS))
                break;
        }
        if (i != 10)
            break;
    }
    printf("    Acceptable signal to dial tone ratio is %ddB\n", -15 - j);
    if ((use_dialtone_filter  &&  (-15 - j) > -12)
        ||
        (!use_dialtone_filter  &&  (-15 - j) > 10))
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");
    dtmf_rx_free(dtmf_state);
}
/*- End of function --------------------------------------------------------*/

static void callback_function_tests(void)
{
    int i;
    int j;
    int len;
    int sample;
    dtmf_rx_state_t *dtmf_state;
    logging_state_t *logging;

    /* Test the callback mode for delivering detected digits */
    printf("Test: Callback digit delivery mode.\n");
    callback_hit = false;
    callback_ok = true;
    callback_roll = 0;
    dtmf_state = dtmf_rx_init(NULL, digit_delivery, (void *) 0x12345678);
    if (use_dialtone_filter  ||  max_forward_twist >= 0.0f  ||  max_reverse_twist >= 0.0f)
        dtmf_rx_parms(dtmf_state, use_dialtone_filter, max_forward_twist, max_reverse_twist, -99.0f);
    logging = dtmf_rx_get_logging_state(dtmf_state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "DTMF-rx");

    my_dtmf_gen_init(0.0f, DEFAULT_DTMF_TX_LEVEL, 0.0f, DEFAULT_DTMF_TX_LEVEL, DEFAULT_DTMF_TX_ON_TIME, DEFAULT_DTMF_TX_OFF_TIME);
    for (i = 1;  i < 10;  i++)
    {
        len = 0;
        for (j = 0;  j < i;  j++)
            len += my_dtmf_generate(amp + len, ALL_POSSIBLE_DIGITS);
        dtmf_rx(dtmf_state, amp, len);
        if (!callback_hit  ||  !callback_ok)
            break;
    }
    if (!callback_hit  ||  !callback_ok)
    {
        printf("    Failed\n");
        exit(2);
    }
    printf("    Passed\n");

    /* Test the realtime callback mode for reporting detected digits */
    printf("Test: Realtime callback digit delivery mode.\n");
    callback_hit = false;
    callback_ok = true;
    callback_roll = 0;
    dtmf_rx_init(dtmf_state, NULL, NULL);
    dtmf_rx_set_realtime_callback(dtmf_state, digit_status, (void *) 0x12345678);
    if (use_dialtone_filter  ||  max_forward_twist >= 0.0f  ||  max_reverse_twist >= 0.0f)
        dtmf_rx_parms(dtmf_state, use_dialtone_filter, max_forward_twist, max_reverse_twist, -99.0f);
    logging = dtmf_rx_get_logging_state(dtmf_state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "DTMF-rx");

    my_dtmf_gen_init(0.0f, DEFAULT_DTMF_TX_LEVEL, 0.0f, DEFAULT_DTMF_TX_LEVEL, DEFAULT_DTMF_TX_ON_TIME, DEFAULT_DTMF_TX_OFF_TIME);
    step = 0;
    for (i = 1;  i < 10;  i++)
    {
        len = 0;
        for (j = 0;  j < i;  j++)
            len += my_dtmf_generate(amp + len, ALL_POSSIBLE_DIGITS);
        for (sample = 0, j = SAMPLES_PER_CHUNK;  sample < len;  sample += SAMPLES_PER_CHUNK, j = ((len - sample) >= SAMPLES_PER_CHUNK)  ?  SAMPLES_PER_CHUNK  :  (len - sample))
        {
            dtmf_rx(dtmf_state, &amp[sample], j);
            if (!callback_ok)
                break;
            step += j;
        }
        if (!callback_hit  ||  !callback_ok)
            break;
    }
    if (!callback_hit  ||  !callback_ok)
    {
        printf("    Failed\n");
        exit(2);
    }
    dtmf_rx_free(dtmf_state);
}
/*- End of function --------------------------------------------------------*/

static void decode_test(const char *test_file)
{
    int16_t amp[SAMPLES_PER_CHUNK];
    SNDFILE *inhandle;
    dtmf_rx_state_t *dtmf_state;
    char buf[128 + 1];
    int actual;
    int samples;
    int total;
    logging_state_t *logging;

    dtmf_state = dtmf_rx_init(NULL, NULL, NULL);
    if (use_dialtone_filter  ||  max_forward_twist >= 0.0f  ||  max_reverse_twist >= 0.0f)
        dtmf_rx_parms(dtmf_state, use_dialtone_filter, max_forward_twist, max_reverse_twist, -99.0f);
    logging = dtmf_rx_get_logging_state(dtmf_state);
    span_log_set_level(logging, SPAN_LOG_SHOW_SEVERITY | SPAN_LOG_SHOW_PROTOCOL | SPAN_LOG_FLOW);
    span_log_set_tag(logging, "DTMF-rx");

    /* We will decode the audio from a file. */

    if ((inhandle = sf_open_telephony_read(decode_test_file, 1)) == NULL)
    {
        fprintf(stderr, "    Cannot open audio file '%s'\n", decode_test_file);
        exit(2);
    }

    total = 0;
    while ((samples = sf_readf_short(inhandle, amp, SAMPLES_PER_CHUNK)) > 0)
    {
        codec_munge(munge, amp, samples);
        dtmf_rx(dtmf_state, amp, samples);
        //printf("Status 0x%X\n", dtmf_rx_status(dtmf_state));
        if ((actual = dtmf_rx_get(dtmf_state, buf, 128)) > 0)
            printf("Received '%s'\n", buf);
        total += actual;
    }
    printf("%d digits received\n", total);
}
/*- End of function --------------------------------------------------------*/

int main(int argc, char *argv[])
{
    int duration;
    time_t now;
    int channel_codec;
    int opt;

    use_dialtone_filter = false;
    channel_codec = MUNGE_CODEC_NONE;
    decode_test_file = NULL;
    max_forward_twist = -1.0f;
    max_reverse_twist = -1.0f;
    while ((opt = getopt(argc, argv, "c:d:F:fR:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            channel_codec = atoi(optarg);
            break;
        case 'd':
            decode_test_file = optarg;
            break;
        case 'F':
            max_forward_twist = atof(optarg);
            break;
        case 'f':
            use_dialtone_filter = true;
            break;
        case 'R':
            max_reverse_twist = atof(optarg);
            break;
        default:
            //usage();
            exit(2);
            break;
        }
    }
    munge = codec_munge_init(channel_codec, 0);

    if (decode_test_file)
    {
        decode_test(decode_test_file);
    }
    else
    {
        time(&now);
        mitel_cm7291_side_1_tests();
        mitel_cm7291_side_2_and_bellcore_tests();
        dial_tone_tolerance_tests();
        callback_function_tests();
        printf("    Passed\n");
        duration = time(NULL) - now;
        printf("Tests passed in %ds\n", duration);
    }

    codec_munge_free(munge);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

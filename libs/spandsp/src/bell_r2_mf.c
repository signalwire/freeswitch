/*
 * SpanDSP - a series of DSP components for telephony
 *
 * bell_r2_mf.c - Bell MF and MFC/R2 tone generation and detection.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2001 Steve Underwood
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

#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/logging.h"
#include "spandsp/fast_convert.h"
#include "spandsp/queue.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/dtmf.h"
#include "spandsp/bell_r2_mf.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/queue.h"
#include "spandsp/private/tone_generate.h"
#include "spandsp/private/bell_r2_mf.h"

#if !defined(M_PI)
/* C99 systems may not define M_PI */
#define M_PI 3.14159265358979323846264338327
#endif

/*!
    MF tone descriptor.
*/
typedef struct
{
    int         f1;         /* First freq */
    int         f2;         /* Second freq */
    int8_t      level1;     /* Level of the first freq (dB) */
    int8_t      level2;     /* Level of the second freq (dB) */
    uint8_t     on_time;    /* Tone on time (ms) */
    uint8_t     off_time;   /* Minimum post tone silence (ms) */
} mf_digit_tones_t;

int bell_mf_gen_inited = false;
tone_gen_descriptor_t bell_mf_digit_tones[15];

int r2_mf_gen_inited = false;
tone_gen_descriptor_t r2_mf_fwd_digit_tones[15];
tone_gen_descriptor_t r2_mf_back_digit_tones[15];

#if 0
tone_gen_descriptor_t socotel_mf_digit_tones[18];
#endif

/* Bell R1 tone generation specs.
 *  Power: -7dBm +- 1dB
 *  Frequency: within +-1.5%
 *  Mismatch between the start time of a pair of tones: <=6ms.
 *  Mismatch between the end time of a pair of tones: <=6ms.
 *  Tone duration: 68+-7ms, except KP which is 100+-7ms.
 *  Inter-tone gap: 68+-7ms.
 */
static const mf_digit_tones_t bell_mf_tones[] =
{
    { 700,  900, -7, -7,  68, 68},
    { 700, 1100, -7, -7,  68, 68},
    { 900, 1100, -7, -7,  68, 68},
    { 700, 1300, -7, -7,  68, 68},
    { 900, 1300, -7, -7,  68, 68},
    {1100, 1300, -7, -7,  68, 68},
    { 700, 1500, -7, -7,  68, 68},
    { 900, 1500, -7, -7,  68, 68},
    {1100, 1500, -7, -7,  68, 68},
    {1300, 1500, -7, -7,  68, 68},
    { 700, 1700, -7, -7,  68, 68}, /* ST''' - use 'C' */
    { 900, 1700, -7, -7,  68, 68}, /* ST'   - use 'A' */
    {1100, 1700, -7, -7, 100, 68}, /* KP    - use '*' */
    {1300, 1700, -7, -7,  68, 68}, /* ST''  - use 'B' */
    {1500, 1700, -7, -7,  68, 68}, /* ST    - use '#' */
    {0, 0, 0, 0, 0, 0}
};

/* The order of the digits here must match the list above */
static const char bell_mf_tone_codes[] = "1234567890CA*B#";

/* R2 tone generation specs.
 *  Power: -11.5dBm +- 1dB
 *  Frequency: within +-4Hz
 *  Mismatch between the start time of a pair of tones: <=1ms.
 *  Mismatch between the end time of a pair of tones: <=1ms.
 */
static const mf_digit_tones_t r2_mf_fwd_tones[] =
{
    {1380, 1500, -11, -11, 1, 0},
    {1380, 1620, -11, -11, 1, 0},
    {1500, 1620, -11, -11, 1, 0},
    {1380, 1740, -11, -11, 1, 0},
    {1500, 1740, -11, -11, 1, 0},
    {1620, 1740, -11, -11, 1, 0},
    {1380, 1860, -11, -11, 1, 0},
    {1500, 1860, -11, -11, 1, 0},
    {1620, 1860, -11, -11, 1, 0},
    {1740, 1860, -11, -11, 1, 0},
    {1380, 1980, -11, -11, 1, 0},
    {1500, 1980, -11, -11, 1, 0},
    {1620, 1980, -11, -11, 1, 0},
    {1740, 1980, -11, -11, 1, 0},
    {1860, 1980, -11, -11, 1, 0},
    {0, 0, 0, 0, 0, 0}
};

static const mf_digit_tones_t r2_mf_back_tones[] =
{
    {1140, 1020, -11, -11, 1, 0},
    {1140,  900, -11, -11, 1, 0},
    {1020,  900, -11, -11, 1, 0},
    {1140,  780, -11, -11, 1, 0},
    {1020,  780, -11, -11, 1, 0},
    { 900,  780, -11, -11, 1, 0},
    {1140,  660, -11, -11, 1, 0},
    {1020,  660, -11, -11, 1, 0},
    { 900,  660, -11, -11, 1, 0},
    { 780,  660, -11, -11, 1, 0},
    {1140,  540, -11, -11, 1, 0},
    {1020,  540, -11, -11, 1, 0},
    { 900,  540, -11, -11, 1, 0},
    { 780,  540, -11, -11, 1, 0},
    { 660,  540, -11, -11, 1, 0},
    {0, 0, 0, 0, 0, 0}
};

/* The order of the digits here must match the lists above */
static const char r2_mf_tone_codes[] = "1234567890BCDEF";

#if 0
static const mf_digit_tones_t socotel_tones[] =
{
    { 700,  900, -11, -11, 1, 0},
    { 700, 1100, -11, -11, 1, 0},
    { 900, 1100, -11, -11, 1, 0},
    { 700, 1300, -11, -11, 1, 0},
    { 900, 1300, -11, -11, 1, 0},
    {1100, 1300, -11, -11, 1, 0},
    { 700, 1500, -11, -11, 1, 0},
    { 900, 1500, -11, -11, 1, 0},
    {1100, 1500, -11, -11, 1, 0},
    {1300, 1500, -11, -11, 1, 0},
    {1500, 1700, -11, -11, 1, 0},
    { 700, 1700, -11, -11, 1, 0},
    { 900, 1700, -11, -11, 1, 0},
    {1300, 1700, -11, -11, 1, 0},
    {1100, 1700, -11, -11, 1, 0},
    {1700,    0, -11, -11, 1, 0},   /* Use 'F' */
    {1900,    0, -11, -11, 1, 0},   /* Use 'G' */
    {0, 0, 0, 0, 0, 0}
};

/* The order of the digits here must match the list above */
static char socotel_mf_tone_codes[] = "1234567890ABCDEFG";
#endif

#if defined(SPANDSP_USE_FIXED_POINT)
#define BELL_MF_THRESHOLD           204089              /* -30.5dBm0 */
#define BELL_MF_TWIST               3.981f              /* 6dB */
#define BELL_MF_RELATIVE_PEAK       12.589f             /* 11dB */
#define BELL_MF_SAMPLES_PER_BLOCK   120

#define R2_MF_THRESHOLD             62974               /* -36.5dBm0 */
#define R2_MF_TWIST                 5.012f              /* 7dB */
#define R2_MF_RELATIVE_PEAK         12.589f             /* 11dB */
#define R2_MF_SAMPLES_PER_BLOCK     133
#else
#define BELL_MF_THRESHOLD           3343803100.0f       /* -30.5dBm0 [((120.0*32768.0/1.4142)*10^((-30.5 - DBM0_MAX_SINE_POWER)/20.0))^2 => 3343803100.0] */
#define BELL_MF_TWIST               3.981f              /* 6dB [10^(6/10) => 3.981] */
#define BELL_MF_RELATIVE_PEAK       12.589f             /* 11dB */
#define BELL_MF_SAMPLES_PER_BLOCK   120

#define R2_MF_THRESHOLD             1031766650.0f       /* -36.5dBm0 [((133.0*32768.0/1.4142)*10^((-36.5 - DBM0_MAX_SINE_POWER)/20.0))^2 => 1031766650.0] */
#define R2_MF_TWIST                 5.012f              /* 7dB */
#define R2_MF_RELATIVE_PEAK         12.589f             /* 11dB */
#define R2_MF_SAMPLES_PER_BLOCK     133
#endif

static goertzel_descriptor_t bell_mf_detect_desc[6];

static goertzel_descriptor_t mf_fwd_detect_desc[6];
static goertzel_descriptor_t mf_back_detect_desc[6];

static const int bell_mf_frequencies[] =
{
     700,  900, 1100, 1300, 1500, 1700
};

/* Use the follow characters for the Bell MF special signals:
    KP    - use '*'
    ST    - use '#'
    ST'   - use 'A'
    ST''  - use 'B'
    ST''' - use 'C' */
static const char bell_mf_positions[] = "1247C-358A--69*---0B----#";

static const int r2_mf_fwd_frequencies[] =
{
    1380, 1500, 1620, 1740, 1860, 1980
};

static const int r2_mf_back_frequencies[] =
{
    1140, 1020,  900,  780,  660,  540
};

/* Use codes '1' to 'F' for the R2 signals 1 to 15, except for signal 'A'.
   Use '0' for this, so the codes match the digits 0-9. */
static const char r2_mf_positions[] = "1247B-358C--69D---0E----F";

static void bell_mf_gen_init(void)
{
    int i;
    const mf_digit_tones_t *tones;

    if (bell_mf_gen_inited)
        return;
    i = 0;
    tones = bell_mf_tones;
    while (tones->on_time)
    {
        /* Note: The duration of KP is longer than the other signals. */
        tone_gen_descriptor_init(&bell_mf_digit_tones[i++],
                                 tones->f1,
                                 tones->level1,
                                 tones->f2,
                                 tones->level2,
                                 tones->on_time,
                                 tones->off_time,
                                 0,
                                 0,
                                 false);
        tones++;
    }
    bell_mf_gen_inited = true;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bell_mf_tx(bell_mf_tx_state_t *s, int16_t amp[], int max_samples)
{
    int len;
    const char *cp;
    int digit;

    len = 0;
    if (s->tones.current_section >= 0)
    {
        /* Deal with the fragment left over from last time */
        len = tone_gen(&(s->tones), amp, max_samples);
    }
    while (len < max_samples  &&  (digit = queue_read_byte(&s->queue.queue)) >= 0)
    {
        /* Step to the next digit */
        if ((cp = strchr(bell_mf_tone_codes, digit)) == NULL)
            continue;
        tone_gen_init(&(s->tones), &bell_mf_digit_tones[cp - bell_mf_tone_codes]);
        len += tone_gen(&(s->tones), amp + len, max_samples - len);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bell_mf_tx_put(bell_mf_tx_state_t *s, const char *digits, int len)
{
    size_t space;

    /* This returns the number of characters that would not fit in the buffer.
       The buffer will only be loaded if the whole string of digits will fit,
       in which case zero is returned. */
    if (len < 0)
    {
        if ((len = strlen(digits)) == 0)
            return 0;
    }
    if ((space = queue_free_space(&s->queue.queue)) < (size_t) len)
        return len - (int) space;
    if (queue_write(&s->queue.queue, (const uint8_t *) digits, len) >= 0)
        return 0;
    return -1;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bell_mf_tx_state_t *) bell_mf_tx_init(bell_mf_tx_state_t *s)
{
    if (s == NULL)
    {
        if ((s = (bell_mf_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    if (!bell_mf_gen_inited)
        bell_mf_gen_init();
    tone_gen_init(&(s->tones), &bell_mf_digit_tones[0]);
    s->current_sample = 0;
    queue_init(&s->queue.queue, MAX_BELL_MF_DIGITS, QUEUE_READ_ATOMIC | QUEUE_WRITE_ATOMIC);
    s->tones.current_section = -1;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bell_mf_tx_release(bell_mf_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bell_mf_tx_free(bell_mf_tx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_tx(r2_mf_tx_state_t *s, int16_t amp[], int samples)
{
    int len;

    if (s->digit == 0)
    {
        len = samples;
        memset(amp, 0, len*sizeof(int16_t));
    }
    else
    {
        len = tone_gen(&s->tone, amp, samples);
    }
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_tx_put(r2_mf_tx_state_t *s, char digit)
{
    char *cp;

    if (digit  &&  (cp = strchr(r2_mf_tone_codes, digit)))
    {
        if (s->fwd)
            tone_gen_init(&s->tone, &r2_mf_fwd_digit_tones[cp - r2_mf_tone_codes]);
        else
            tone_gen_init(&s->tone, &r2_mf_back_digit_tones[cp - r2_mf_tone_codes]);
        s->digit = digit;
    }
    else
    {
        s->digit = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(r2_mf_tx_state_t *) r2_mf_tx_init(r2_mf_tx_state_t *s, bool fwd)
{
    int i;
    const mf_digit_tones_t *tones;

    if (s == NULL)
    {
        if ((s = (r2_mf_tx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    if (!r2_mf_gen_inited)
    {
        i = 0;
        tones = r2_mf_fwd_tones;
        while (tones->on_time)
        {
            tone_gen_descriptor_init(&r2_mf_fwd_digit_tones[i++],
                                     tones->f1,
                                     tones->level1,
                                     tones->f2,
                                     tones->level2,
                                     tones->on_time,
                                     tones->off_time,
                                     0,
                                     0,
                                     (tones->off_time == 0));
            tones++;
        }
        i = 0;
        tones = r2_mf_back_tones;
        while (tones->on_time)
        {
            tone_gen_descriptor_init(&r2_mf_back_digit_tones[i++],
                                     tones->f1,
                                     tones->level1,
                                     tones->f2,
                                     tones->level2,
                                     tones->on_time,
                                     tones->off_time,
                                     0,
                                     0,
                                     (tones->off_time == 0));
            tones++;
        }
        r2_mf_gen_inited = true;
    }
    s->fwd = fwd;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_tx_release(r2_mf_tx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_tx_free(r2_mf_tx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bell_mf_rx(bell_mf_rx_state_t *s, const int16_t amp[], int samples)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t energy[6];
    int16_t xamp;
#else
    float energy[6];
    float xamp;
#endif
    int i;
    int j;
    int sample;
    int best;
    int second_best;
    int limit;
    uint8_t hit;

    for (sample = 0;  sample < samples;  sample = limit)
    {
        if ((samples - sample) >= (BELL_MF_SAMPLES_PER_BLOCK - s->current_sample))
            limit = sample + (BELL_MF_SAMPLES_PER_BLOCK - s->current_sample);
        else
            limit = samples;
        for (j = sample;  j < limit;  j++)
        {
            xamp = goertzel_preadjust_amp(amp[j]);
            goertzel_samplex(&s->out[0], xamp);
            goertzel_samplex(&s->out[1], xamp);
            goertzel_samplex(&s->out[2], xamp);
            goertzel_samplex(&s->out[3], xamp);
            goertzel_samplex(&s->out[4], xamp);
            goertzel_samplex(&s->out[5], xamp);
        }
        s->current_sample += (limit - sample);
        if (s->current_sample < BELL_MF_SAMPLES_PER_BLOCK)
            continue;

        /* We are at the end of an MF detection block */
        /* Find the two highest energies. The spec says to look for
           two tones and two tones only. Taking this literally -ie
           only two tones pass the minimum threshold - doesn't work
           well. The sinc function mess, due to rectangular windowing
           ensure that! Find the two highest energies and ensure they
           are considerably stronger than any of the others. */
        energy[0] = goertzel_result(&s->out[0]);
        energy[1] = goertzel_result(&s->out[1]);
        if (energy[0] > energy[1])
        {
            best = 0;
            second_best = 1;
        }
        else
        {
            best = 1;
            second_best = 0;
        }
        for (i = 2;  i < 6;  i++)
        {
            energy[i] = goertzel_result(&s->out[i]);
            if (energy[i] >= energy[best])
            {
                second_best = best;
                best = i;
            }
            else if (energy[i] >= energy[second_best])
            {
                second_best = i;
            }
        }
        /* Basic signal level and twist tests */
        hit = 0;
        if (energy[best] >= BELL_MF_THRESHOLD
            &&
            energy[second_best] >= BELL_MF_THRESHOLD
            &&
            energy[best] < energy[second_best]*BELL_MF_TWIST
            &&
            energy[best]*BELL_MF_TWIST > energy[second_best])
        {
            /* Relative peak test */
            hit = 'X';
            for (i = 0;  i < 6;  i++)
            {
                if (i != best  &&  i != second_best)
                {
                    if (energy[i]*BELL_MF_RELATIVE_PEAK >= energy[second_best])
                    {
                        /* The best two are not clearly the best */
                        hit = 0;
                        break;
                    }
                }
            }
        }
        if (hit)
        {
            /* Get the values into ascending order */
            if (second_best < best)
            {
                i = best;
                best = second_best;
                second_best = i;
            }
            best = best*5 + second_best - 1;
            hit = bell_mf_positions[best];
            /* Look for two successive similar results */
            /* The logic in the next test is:
               For KP we need 4 successive identical clean detects, with
               two blocks of something different preceeding it. For anything
               else we need two successive identical clean detects, with
               two blocks of something different preceeding it. */
            if (hit == s->hits[4]
                &&
                hit == s->hits[3]
                &&
                   ((hit != '*'  &&  hit != s->hits[2]  &&  hit != s->hits[1])
                    ||
                    (hit == '*'  &&  hit == s->hits[2]  &&  hit != s->hits[1]  &&  hit != s->hits[0])))
            {
                if (s->current_digits < MAX_BELL_MF_DIGITS)
                {
                    s->digits[s->current_digits++] = (char) hit;
                    s->digits[s->current_digits] = '\0';
                    if (s->digits_callback)
                    {
                        s->digits_callback(s->digits_callback_data, s->digits, s->current_digits);
                        s->current_digits = 0;
                    }
                }
                else
                {
                    s->lost_digits++;
                }
            }
        }
        s->hits[0] = s->hits[1];
        s->hits[1] = s->hits[2];
        s->hits[2] = s->hits[3];
        s->hits[3] = s->hits[4];
        s->hits[4] = hit;
        s->current_sample = 0;
    }
    if (s->current_digits  &&  s->digits_callback)
    {
        s->digits_callback(s->digits_callback_data, s->digits, s->current_digits);
        s->digits[0] = '\0';
        s->current_digits = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(size_t) bell_mf_rx_get(bell_mf_rx_state_t *s, char *buf, int max)
{
    if (max > s->current_digits)
        max = s->current_digits;
    if (max > 0)
    {
        memcpy(buf, s->digits, max);
        memmove(s->digits, s->digits + max, s->current_digits - max);
        s->current_digits -= max;
    }
    buf[max] = '\0';
    return max;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(bell_mf_rx_state_t *) bell_mf_rx_init(bell_mf_rx_state_t *s,
                                                   digits_rx_callback_t callback,
                                                   void *user_data)
{
    int i;
    static int initialised = false;

    if (s == NULL)
    {
        if ((s = (bell_mf_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    if (!initialised)
    {
        for (i = 0;  i < 6;  i++)
            make_goertzel_descriptor(&bell_mf_detect_desc[i], (float) bell_mf_frequencies[i], BELL_MF_SAMPLES_PER_BLOCK);
        initialised = true;
    }
    s->digits_callback = callback;
    s->digits_callback_data = user_data;

    s->hits[0] =
    s->hits[1] =
    s->hits[2] =
    s->hits[3] =
    s->hits[4] = 0;

    for (i = 0;  i < 6;  i++)
        goertzel_init(&s->out[i], &bell_mf_detect_desc[i]);
    s->current_sample = 0;
    s->lost_digits = 0;
    s->current_digits = 0;
    s->digits[0] = '\0';
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bell_mf_rx_release(bell_mf_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) bell_mf_rx_free(bell_mf_rx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_rx(r2_mf_rx_state_t *s, const int16_t amp[], int samples)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t energy[6];
    int16_t xamp;
#else
    float energy[6];
    float xamp;
#endif
    int i;
    int j;
    int sample;
    int best;
    int second_best;
    int hit;
    int hit_digit;
    int limit;

    for (sample = 0;  sample < samples;  sample = limit)
    {
        if ((samples - sample) >= (R2_MF_SAMPLES_PER_BLOCK - s->current_sample))
            limit = sample + (R2_MF_SAMPLES_PER_BLOCK - s->current_sample);
        else
            limit = samples;
        for (j = sample;  j < limit;  j++)
        {
            xamp = goertzel_preadjust_amp(amp[j]);
            goertzel_samplex(&s->out[0], xamp);
            goertzel_samplex(&s->out[1], xamp);
            goertzel_samplex(&s->out[2], xamp);
            goertzel_samplex(&s->out[3], xamp);
            goertzel_samplex(&s->out[4], xamp);
            goertzel_samplex(&s->out[5], xamp);
        }
        s->current_sample += (limit - sample);
        if (s->current_sample < R2_MF_SAMPLES_PER_BLOCK)
            continue;

        /* We are at the end of an MF detection block */
        /* Find the two highest energies */
        energy[0] = goertzel_result(&s->out[0]);
        energy[1] = goertzel_result(&s->out[1]);
        if (energy[0] > energy[1])
        {
            best = 0;
            second_best = 1;
        }
        else
        {
            best = 1;
            second_best = 0;
        }

        for (i = 2;  i < 6;  i++)
        {
            energy[i] = goertzel_result(&s->out[i]);
            if (energy[i] >= energy[best])
            {
                second_best = best;
                best = i;
            }
            else if (energy[i] >= energy[second_best])
            {
                second_best = i;
            }
        }
        /* Basic signal level and twist tests */
        hit = false;
        if (energy[best] >= R2_MF_THRESHOLD
            &&
            energy[second_best] >= R2_MF_THRESHOLD
            &&
            energy[best] < energy[second_best]*R2_MF_TWIST
            &&
            energy[best]*R2_MF_TWIST > energy[second_best])
        {
            /* Relative peak test */
            hit = true;
            for (i = 0;  i < 6;  i++)
            {
                if (i != best  &&  i != second_best)
                {
                    if (energy[i]*R2_MF_RELATIVE_PEAK >= energy[second_best])
                    {
                        /* The best two are not clearly the best */
                        hit = false;
                        break;
                    }
                }
            }
        }
        if (hit)
        {
            /* Get the values into ascending order */
            if (second_best < best)
            {
                i = best;
                best = second_best;
                second_best = i;
            }
            best = best*5 + second_best - 1;
            hit_digit = r2_mf_positions[best];
        }
        else
        {
            hit_digit = 0;
        }
        if (s->current_digit != hit_digit  &&  s->callback)
        {
            i = (hit_digit)  ?  -10  :  -99;
            s->callback(s->callback_data, hit_digit, i, 0);
        }
        s->current_digit = hit_digit;
        s->current_sample = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_rx_get(r2_mf_rx_state_t *s)
{
    return s->current_digit;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(r2_mf_rx_state_t *) r2_mf_rx_init(r2_mf_rx_state_t *s,
                                               bool fwd,
                                               tone_report_func_t callback,
                                               void *user_data)
{
    int i;
    static int initialised = false;

    if (s == NULL)
    {
        if ((s = (r2_mf_rx_state_t *) span_alloc(sizeof(*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));

    s->fwd = fwd;

    if (!initialised)
    {
        for (i = 0;  i < 6;  i++)
        {
            make_goertzel_descriptor(&mf_fwd_detect_desc[i], (float) r2_mf_fwd_frequencies[i], R2_MF_SAMPLES_PER_BLOCK);
            make_goertzel_descriptor(&mf_back_detect_desc[i], (float) r2_mf_back_frequencies[i], R2_MF_SAMPLES_PER_BLOCK);
        }
        initialised = true;
    }
    if (fwd)
    {
        for (i = 0;  i < 6;  i++)
            goertzel_init(&s->out[i], &mf_fwd_detect_desc[i]);
    }
    else
    {
        for (i = 0;  i < 6;  i++)
            goertzel_init(&s->out[i], &mf_back_detect_desc[i]);
    }
    s->callback = callback;
    s->callback_data = user_data;
    s->current_digit = 0;
    s->current_sample = 0;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_rx_release(r2_mf_rx_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) r2_mf_rx_free(r2_mf_rx_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

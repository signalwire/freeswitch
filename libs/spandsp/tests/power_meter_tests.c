/*
 * SpanDSP - a series of DSP components for telephony
 *
 * power_meter_tests.c
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
 *
 * $Id: power_meter_tests.c,v 1.21 2008/05/13 13:17:26 steveu Exp $
 */

/*! \page power_meter_tests_page Power meter tests
\section power_meter_tests_page_sec_1 What does it do?
These tests assess the accuracy of power meters built from the power meter module.
Both tones and noise are used to check the meter's behaviour.

\section power_meter_tests_page_sec_2 How does it work?
???.
*/

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>

#include "spandsp.h"

int main(int argc, char *argv[])
{
    awgn_state_t noise_source;
    power_meter_t meter;
    tone_gen_descriptor_t tone_desc;
    tone_gen_state_t gen;
    int i;
    int idum = 1234567;
    int16_t amp[1000];
    int len;
    int32_t level;

    power_meter_init(&meter, 7);

    printf("Testing with a square wave 10dB from maximum\n");
    for (i = 0;  i < 1000;  i++)
    {
        amp[i] = (i & 1)  ?  10362  :  -10362;
        level = power_meter_update(&meter, amp[i]);
        //printf("%12d %fdBm0 %fdBov\n", level, power_meter_current_dbm0(&meter), power_meter_current_dbov(&meter));
    }
    printf("Level: expected %" PRId32 "/%" PRId32 ", got %" PRId32 "\n", power_meter_level_dbov(-10.0f), power_meter_level_dbm0(-10.0f + DBM0_MAX_POWER), level);
    printf("Power: expected %fdBm0, got %fdBm0\n", -10.0f + DBM0_MAX_POWER, power_meter_current_dbm0(&meter));
    printf("Power: expected %fdBOv, got %fdBOv\n", -10.0f, power_meter_current_dbov(&meter));
    if (level < power_meter_level_dbov(-10.0f)*0.99f
        ||
        level > power_meter_level_dbov(-10.0f)*1.01f)
    {
        printf("Test failed (level)\n");
        exit(2);
    }
    if (0.1f < fabsf(power_meter_current_dbm0(&meter) + 10.0f - DBM0_MAX_POWER))
    {
        printf("Test failed (dBm0)\n");
        exit(2);
    }
    if (0.1f < fabsf(power_meter_current_dbov(&meter) + 10.0))
    {
        printf("Test failed (dBOv)\n");
        exit(2);
    }

    printf("Testing with a sine wave tone 10dB from maximum\n");
    make_tone_gen_descriptor(&tone_desc,
                             1000,
                             -4,
                             0,
                             1,
                             1,
                             0,
                             0,
                             0,
                             TRUE);
    tone_gen_init(&gen, &tone_desc);
    len = tone_gen(&gen, amp, 1000);
    for (i = 0;  i < len;  i++)
    {
        level = power_meter_update(&meter, amp[i]);
        //printf("%12d %fdBm0 %fdBov\n", level, power_meter_current_dbm0(&meter), power_meter_current_dbov(&meter));
    }
    printf("Level: expected %" PRId32 "/%" PRId32 ", got %" PRId32 "\n", power_meter_level_dbov(-10.0f), power_meter_level_dbm0(-10.0f + DBM0_MAX_POWER), level);
    printf("Power: expected %fdBm0, got %fdBm0\n", -10.0f + DBM0_MAX_POWER, power_meter_current_dbm0(&meter));
    printf("Power: expected %fdBOv, got %fdBOv\n", -10.0f, power_meter_current_dbov(&meter));
    if (level < power_meter_level_dbov(-10.0f)*0.95f
        ||
        level > power_meter_level_dbov(-10.0f)*1.05f)
    {
        printf("Test failed (level)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbm0(&meter) + 10.0f - DBM0_MAX_POWER))
    {
        printf("Test failed (dBm0)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbov(&meter) + 10.0))
    {
        printf("Test failed (dBOv)\n");
        exit(2);
    }

    printf("Testing with AWGN 10dB from maximum\n");
    awgn_init_dbov(&noise_source, idum, -10.0f);
    for (i = 0;  i < 1000;  i++)
        amp[i] = awgn(&noise_source);
    for (i = 0;  i < 1000;  i++)
    {
        level = power_meter_update(&meter, amp[i]);
        //printf("%12d %fdBm0 %fdBov\n", level, power_meter_current_dbm0(&meter), power_meter_current_dbov(&meter));
    }
    printf("Level: expected %" PRId32 "/%" PRId32 ", got %" PRId32 "\n", power_meter_level_dbov(-10.0f), power_meter_level_dbm0(-10.0f + DBM0_MAX_POWER), level);
    printf("Power: expected %fdBm0, got %fdBm0\n", -10.0f + DBM0_MAX_POWER, power_meter_current_dbm0(&meter));
    printf("Power: expected %fdBOv, got %fdBOv\n", -10.0f, power_meter_current_dbov(&meter));
    if (level < power_meter_level_dbov(-10.0f)*0.95f
        ||
        level > power_meter_level_dbov(-10.0f)*1.05f)
    {
        printf("Test failed (level)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbm0(&meter) + 10.0f - DBM0_MAX_POWER))
    {
        printf("Test failed (dBm0)\n");
        exit(2);
    }
    if (0.2f < fabsf(power_meter_current_dbov(&meter) + 10.0f))
    {
        printf("Test failed (dBOv)\n");
        exit(2);
    }
    printf("Tests passed\n");
    return  0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

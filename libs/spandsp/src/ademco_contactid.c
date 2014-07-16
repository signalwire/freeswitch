/*
 * SpanDSP - a series of DSP components for telephony
 *
 * ademco_contactid.c - Ademco ContactID alarm protocol
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2012 Steve Underwood
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
#include <memory.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "spandsp/telephony.h"
#include "spandsp/alloc.h"
#include "spandsp/fast_convert.h"
#include "spandsp/logging.h"
#include "spandsp/queue.h"
#include "spandsp/complex.h"
#include "spandsp/dds.h"
#include "spandsp/power_meter.h"
#include "spandsp/async.h"
#include "spandsp/vector_float.h"
#include "spandsp/complex_vector_float.h"
#include "spandsp/vector_int.h"
#include "spandsp/complex_vector_int.h"
#include "spandsp/tone_detect.h"
#include "spandsp/tone_generate.h"
#include "spandsp/super_tone_rx.h"
#include "spandsp/dtmf.h"
#include "spandsp/ademco_contactid.h"

#include "spandsp/private/logging.h"
#include "spandsp/private/queue.h"
#include "spandsp/private/tone_detect.h"
#include "spandsp/private/tone_generate.h"
#include "spandsp/private/dtmf.h"
#include "spandsp/private/ademco_contactid.h"

/*
Ademco ContactID Protocol

Answer
Wait 0.5s to 2s for the line to settle
Send 1400Hz for 100ms
Send silence for 100ms
Send 2300Hz for 100ms
Receiver now waits

(both timing and frequency errors specified as 3%, but sender side should accept these tones with 5% frequency error.)

Sender waits 250-300ms after end of 2300Hz tone

Send ACCT MT QXYZ GG CCC S

ACCT = 4 digit account code (0-9, B-F)
MT = 2 digit message type (18 preferred, 98 optional)
Q = 1 digit event qualifier. 1 = New event or opening. 3 = New restore or closing. 6 = Previous condition still present
XYZ = 3 digit event code (0-9, B-F)
GG = 2 digit group or partition number (0-9, B-F). 00=no specific group
CCC = 3 digit zone number (event reports) or user number (open/close reports). 000=no specific zone or user information
S = 1 digit hex checksum (sum all message digits + S) mod 15 == 0

DTMF tones are 50-60ms on 50-60ms off

0       10 (counted as 10 in checksum calculations)
1       1
2       2
3       3
4       4
5       5
6       6
7       7
8       8
9       9
B (*)   11
C (#)   12
D (A)   13
E (B)   14
F (C)   15

DTMF D is not used

Wait 1.25s for a kiss-off tone
Detect at least 400ms of kissoff to be valid, then wait for end of tone

Wait 250-300ms before sending the next DTMF message

If kissoff doesn't start within 1.25s of the end of the DTMF, repeat the DTMF message

Receiver sends 750-1000ms of 1400Hz as the kissoff tone

Sender shall make 4 attempts before giving up. One successful kissoff resets the attempt counter


Ademco Express 4/1

    ACCT MT C

ACCT = 4 digit account code (0-9, B-F)
MT = 2 digit message type (17)
C = alarm code
S = 1 digit hex checksum

Ademco Express 4/2

    ACCT MT C Z S

ACCT = 4 digit account code (0-9, B-F)
MT = 2 digit message type (27)
C = 1 digit alarm code
Z = 1 digit zone or user number
S = 1 digit hex checksum

Ademco High speed

    ACCT MT PPPPPPPP X S

ACCT = 4 digit account code (0-9, B-F)
MT = 2 digit message type (55)
PPPPPPPP = 8 digit status of each zone
X = 1 digit type of information in the PPPPPPPP field
S = 1 digit hex checksum

Each P digit contains one of the following values:
        1  new alarm
        2  new opening
        3  new restore
        4  new closing
        5  normal
        6  outstanding
The X field contains one of the following values:
        0  AlarmNet messages
        1  ambush or duress
        2  opening by user (the first P field contains the user number)
        3  bypass (the P fields indicate which zones are bypassed)
        4  closing by user (the first P field contain the user number)
        5  trouble (the P fields contain which zones are in trouble)
        6  system trouble
        7  normal message (the P fields indicate zone status)
        8  low battery (the P fields indicate zone status)
        9  test (the P fields indicate zone status)

Ademco Super fast

    ACCT MT PPPPPPPP X S

ACCT = 4 digit account code (0-9, B-F)
MT = 2 digit message type (56)

There are versions somewhat like the above, with 8, 16 or 24 'P' digits,
and no message type
    ACCT PPPPPPPP X
    ACCT PPPPPPPPPPPPPPPP X
    ACCT PPPPPPPPPPPPPPPPPPPPPPPP X

ACCT = 4 digit account code (0-9, B-F)
PPPPPPPP = 8, 16 or 24 digit status of each zone
X = 1 digit status of the communicator
S = 1 digit hex checksum

*/

struct ademco_code_s
{
    int code;
    const char *name;
    int data_type;
};

static const struct ademco_code_s ademco_codes[] =
{
    {0x100, "Medical",                                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x101, "Personal emergency",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x102, "Fail to report in",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x110, "Fire",                                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x111, "Smoke",                                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x112, "Combustion",                               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x113, "Water flow",                               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x114, "Heat",                                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x115, "Pull station",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x116, "Duct",                                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x117, "Flame",                                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x118, "Near alarm",                               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x120, "Panic",                                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x121, "Duress",                                   ADEMCO_CONTACTID_DATA_IS_USER},
    {0x122, "Silent",                                   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x123, "Audible",                                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x124, "Duress - Access granted",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x125, "Duress - Egress granted",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x130, "Burglary",                                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x131, "Perimeter",                                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x132, "Interior",                                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x133, "24 hour (safe)",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x134, "Entry/Exit",                               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x135, "Day/Night",                                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x136, "Outdoor",                                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x137, "Tamper",                                   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x138, "Near alarm",                               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x139, "Intrusion verifier",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x140, "General alarm",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x141, "Polling loop open",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x142, "Polling loop short",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x143, "Expansion module failure",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x144, "Sensor tamper",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x145, "Expansion module tamper",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x146, "Silent burglary",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x147, "Sensor supervision failure",               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x150, "24 hour non-burglary",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x151, "Gas detected",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x152, "Refrigeration",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x153, "Loss of heat",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x154, "Water leakage",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x155, "Foil break",                               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x156, "Day trouble",                              ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x157, "Low bottled gas level",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x158, "High temp",                                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x159, "Low temp",                                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x161, "Loss of air flow",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x162, "Carbon monoxide detected",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x163, "Tank level",                               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x200, "Fire supervisory",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x201, "Low water pressure",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x202, "Low CO2",                                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x203, "Gate valve sensor",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x204, "Low water level",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x205, "Pump activated",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x206, "Pump failure",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x300, "System trouble",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x301, "AC loss",                                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x302, "Low system battery",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x303, "RAM checksum bad",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x304, "ROM checksum bad",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x305, "System reset",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x306, "Panel programming changed",                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x307, "Self-test failure",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x308, "System shutdown",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x309, "Battery test failure",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x310, "Ground fault",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x311, "Battery missing/dead",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x312, "Power supply overcurrent",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x313, "Engineer reset",                           ADEMCO_CONTACTID_DATA_IS_USER},
    {0x320, "Sounder/relay",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x321, "Bell 1",                                   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x322, "Bell 2",                                   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x323, "Alarm relay",                              ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x324, "Trouble relay",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x325, "Reversing relay",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x326, "Notification appliance ckt. #3",           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x327, "Notification appliance ckt. #4",           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x330, "System peripheral trouble",                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x331, "Polling loop open",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x332, "Polling loop short",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x333, "Expansion module failure",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x334, "Repeater failure",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x335, "Local printer out of paper",               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x336, "Local printer failure",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x337, "Exp. module DC loss",                      ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x338, "Exp. module low battery",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x339, "Exp. module reset",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x341, "Exp. module tamper",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x342, "Exp. module AC loss",                      ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x343, "Exp. module self-test fail",               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x344, "RF receiver jam detect",                   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x350, "Communication trouble",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x351, "Telco 1 fault",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x352, "Telco 2 fault",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x353, "Long range radio transmitter fault",       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x354, "Failure to communicate event",             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x355, "Loss of radio supervision",                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x356, "Loss of central polling",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x357, "Long range radio VSWR problem",            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x370, "Protection loop",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x371, "Protection loop open",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x372, "Protection loop short",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x373, "Fire trouble",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x374, "Exit error alarm (zone)",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x375, "Panic zone trouble",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x376, "Hold-up zone trouble",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x377, "Swinger trouble",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x378, "Cross-zone trouble",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x380, "Sensor trouble",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x381, "Loss of supervision - RF",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x382, "Loss of supervision - RPM",                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x383, "Sensor tamper",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x384, "RF low battery",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x385, "Smoke detector high sensitivity",          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x386, "Smoke detector low sensitivity",           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x387, "Intrusion detector high sensitivity",      ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x388, "Intrusion detector low sensitivity",       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x389, "Sensor self-test failure",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x391, "Sensor Watch trouble",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x392, "Drift compensation error",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x393, "Maintenance alert",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x400, "Open/Close",                               ADEMCO_CONTACTID_DATA_IS_USER},
    {0x401, "O/C by user",                              ADEMCO_CONTACTID_DATA_IS_USER},
    {0x402, "Group O/C",                                ADEMCO_CONTACTID_DATA_IS_USER},
    {0x403, "Automatic O/C",                            ADEMCO_CONTACTID_DATA_IS_USER},
    {0x404, "Late to O/C",                              ADEMCO_CONTACTID_DATA_IS_USER},
    {0x405, "Deferred O/C",                             ADEMCO_CONTACTID_DATA_IS_USER},
    {0x406, "Cancel",                                   ADEMCO_CONTACTID_DATA_IS_USER},
    {0x407, "Remote arm/disarm",                        ADEMCO_CONTACTID_DATA_IS_USER},
    {0x408, "Quick arm",                                ADEMCO_CONTACTID_DATA_IS_USER},
    {0x409, "Keyswitch O/C",                            ADEMCO_CONTACTID_DATA_IS_USER},
    {0x441, "Armed STAY",                               ADEMCO_CONTACTID_DATA_IS_USER},
    {0x442, "Keyswitch Armed STAY",                     ADEMCO_CONTACTID_DATA_IS_USER},
    {0x450, "Exception O/C",                            ADEMCO_CONTACTID_DATA_IS_USER},
    {0x451, "Early O/C",                                ADEMCO_CONTACTID_DATA_IS_USER},
    {0x452, "Late O/C",                                 ADEMCO_CONTACTID_DATA_IS_USER},
    {0x453, "Failed to open",                           ADEMCO_CONTACTID_DATA_IS_USER},
    {0x454, "Failed to close",                          ADEMCO_CONTACTID_DATA_IS_USER},
    {0x455, "Auto-arm failed",                          ADEMCO_CONTACTID_DATA_IS_USER},
    {0x456, "Partial arm",                              ADEMCO_CONTACTID_DATA_IS_USER},
    {0x457, "Exit error (user)",                        ADEMCO_CONTACTID_DATA_IS_USER},
    {0x458, "User on Premises",                         ADEMCO_CONTACTID_DATA_IS_USER},
    {0x459, "Recent close",                             ADEMCO_CONTACTID_DATA_IS_USER},
    {0x461, "Wrong code entry",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x462, "Legal code entry",                         ADEMCO_CONTACTID_DATA_IS_USER},
    {0x463, "Re-arm after alarm",                       ADEMCO_CONTACTID_DATA_IS_USER},
    {0x464, "Auto-arm time extended",                   ADEMCO_CONTACTID_DATA_IS_USER},
    {0x465, "Panic alarm reset",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x466, "Service on/off premises",                  ADEMCO_CONTACTID_DATA_IS_USER},
    {0x411, "Callback request made",                    ADEMCO_CONTACTID_DATA_IS_USER},
    {0x412, "Successful download/access",               ADEMCO_CONTACTID_DATA_IS_USER},
    {0x413, "Unsuccessful access",                      ADEMCO_CONTACTID_DATA_IS_USER},
    {0x414, "System shutdown command received",         ADEMCO_CONTACTID_DATA_IS_USER},
    {0x415, "Dialer shutdown command received",         ADEMCO_CONTACTID_DATA_IS_USER},
    {0x416, "Successful Upload",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x421, "Access denied",                            ADEMCO_CONTACTID_DATA_IS_USER},
    {0x422, "Access report by user",                    ADEMCO_CONTACTID_DATA_IS_USER},
    {0x423, "Forced Access",                            ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x424, "Egress Denied",                            ADEMCO_CONTACTID_DATA_IS_USER},
    {0x425, "Egress Granted",                           ADEMCO_CONTACTID_DATA_IS_USER},
    {0x426, "Access Door propped open",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x427, "Access point door status monitor trouble", ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x428, "Access point request to exit trouble",     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x429, "Access program mode entry",                ADEMCO_CONTACTID_DATA_IS_USER},
    {0x430, "Access program mode exit",                 ADEMCO_CONTACTID_DATA_IS_USER},
    {0x431, "Access threat level change",               ADEMCO_CONTACTID_DATA_IS_USER},
    {0x432, "Access relay/trigger fail",                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x433, "Access RTE shunt",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x434, "Access DSM shunt",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x501, "Access reader disable",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x520, "Sounder/Relay disable",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x521, "Bell 1 disable",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x522, "Bell 2 disable",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x523, "Alarm relay disable",                      ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x524, "Trouble relay disable",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x525, "Reversing relay disable",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x526, "Notification appliance ckt. #3 disable",   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x527, "Notification appliance ckt. #4 disable",   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x531, "Module added",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x532, "Module removed",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x551, "Dialer disabled",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x552, "Radio transmitter disabled",               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x553, "Remote upload/download disabled",          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x570, "Zone/Sensor bypass",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x571, "Fire bypass",                              ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x572, "24 hour zone bypass",                      ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x573, "Burg. bypass",                             ADEMCO_CONTACTID_DATA_IS_USER},
    {0x574, "Group bypass",                             ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x575, "Swinger bypass",                           ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x576, "Access zone shunt",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x577, "Access point bypass",                      ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x601, "Manual trigger test report",               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x602, "Periodic test report",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x603, "Periodic RF transmission",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x604, "Fire test",                                ADEMCO_CONTACTID_DATA_IS_USER},
    {0x605, "Status report to follow",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x606, "Listen-in to follow",                      ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x607, "Walk test mode",                           ADEMCO_CONTACTID_DATA_IS_USER},
    {0x608, "Periodic test - system trouble present",   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x609, "Video transmitter active",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x611, "Point tested OK",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x612, "Point not tested",                         ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x613, "Intrusion zone walk tested",               ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x614, "Fire zone walk tested",                    ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x615, "Panic zone walk tested",                   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x616, "Service request",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x621, "Event log reset",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x622, "Event log 50% full",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x623, "Event log 90% full",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x624, "Event log overflow",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x625, "Time/Date reset",                          ADEMCO_CONTACTID_DATA_IS_USER},
    {0x626, "Time/Date inaccurate",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x627, "Program mode entry",                       ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x628, "Program mode exit",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x629, "32 hour event log marker",                 ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x630, "Schedule change",                          ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x631, "Exception schedule change",                ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x632, "Access schedule change",                   ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x641, "Senior watch trouble",                     ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x642, "Latch-key supervision",                    ADEMCO_CONTACTID_DATA_IS_USER},
    {0x651, "Reserved for Ademco use",                  ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x652, "Reserved for Ademco use",                  ADEMCO_CONTACTID_DATA_IS_USER},
    {0x653, "Reserved for Ademco use",                  ADEMCO_CONTACTID_DATA_IS_USER},
    {0x654, "System inactivity",                        ADEMCO_CONTACTID_DATA_IS_ZONE},
    {0x900, "Download abort",                           ADEMCO_CONTACTID_DATA_IS_USER},
    {0x901, "Download start/end",                       ADEMCO_CONTACTID_DATA_IS_USER},
    {0x902, "Download interrupted",                     ADEMCO_CONTACTID_DATA_IS_USER},
    {0x910, "Auto-close with bypass",                   ADEMCO_CONTACTID_DATA_IS_USER},
    {0x911, "Bypass closing",                           ADEMCO_CONTACTID_DATA_IS_USER},
    {0x999, "32 hour no read of event log",             ADEMCO_CONTACTID_DATA_IS_USER},
    {-1,    "???"}
};

#define GOERTZEL_SAMPLES_PER_BLOCK  55              /* We need to detect over a +-5% range */

#if defined(SPANDSP_USE_FIXED_POINT)
#define DETECTION_THRESHOLD         3035            /* -42dBm0 */
#define TONE_TO_TOTAL_ENERGY        45.2233f        /* -0.85dB */
#else
#define DETECTION_THRESHOLD         49728296.6f     /* -42dBm0 [((GOERTZEL_SAMPLES_PER_BLOCK*32768.0/1.4142)*10^((-42 - DBM0_MAX_SINE_POWER)/20.0))^2] */
#define TONE_TO_TOTAL_ENERGY        45.2233f        /* -0.85dB [GOERTZEL_SAMPLES_PER_BLOCK*10^(-0.85/10.0)] */
#endif

static int tone_rx_init = false;
static goertzel_descriptor_t tone_1400_desc;
static goertzel_descriptor_t tone_2300_desc;

SPAN_DECLARE(int) encode_msg(char buf[], const ademco_contactid_report_t *report)
{
    char *s;
    int sum;
    int x;
    static const char remap[] = {'D', '*', '#', 'A', 'B', 'C'};

    sprintf(buf, "%04X%02X%1X%03X%02X%03X", report->acct, report->mt, report->q, report->xyz, report->gg, report->ccc);
    for (sum = 0, s = buf;  *s;  s++)
    {
        if (*s == 'A')
            return -1;
        if (*s > '9')
        {
            x = *s - ('A' - 10);
            /* Remap the Ademco B-F digits to normal DTMF *#ABC digits */
            *s = remap[x - 10];
        }
        else
        {
            x = *s - '0';
            if (x == 0)
                x = 10;
        }
        sum += x;
    }
    sum = ((sum + 15)/15)*15 - sum;
    if (sum == 0)
        sum = 'C';
    else if (sum <= 9)
        sum += '0';
    else
        sum = remap[sum - 10];
    *s++ = sum;
    *s = '\0';
    return s - buf;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) decode_msg(ademco_contactid_report_t *report, const char buf[])
{
    const char *s;
    char *t;
    int sum;
    int x;
    char buf2[20];

    /* We need to remap normal DTMF (0-9, *, #, A-D) to Ademco's pseudo-hex (0-9, B-F, nothing for A)
       and calculate the checksum */
    for (sum = 0, s = buf, t = buf2;  *s;  s++, t++)
    {
        x = *s;
        switch (x)
        {
        case '*':
            x = 'B';
            break;
        case '#':
            x = 'C';
            break;
        case 'A':
            x = 'D';
            break;
        case 'B':
            x = 'E';
            break;
        case 'C':
            x = 'F';
            break;
        case 'D':
            /* This should not happen in the Ademco protocol */
            x = 'A';
            break;
        default:
            x = *s;
            break;
        }
        *t = x;
        if (x > '9')
        {
            x -= ('B' - 11);
        }
        else
        {
            if (x == '0')
                x = 10;
            else
                x -= '0';
        }
        sum += x;
    }
    *t = '\0';
    if (sum%15 != 0)
        return -1;
    if (sscanf(buf2, "%04x%02x%1x%03x%02x%03x", &report->acct, &report->mt, &report->q, &report->xyz, &report->gg, &report->ccc) != 6)
        return -1;
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) ademco_contactid_msg_qualifier_to_str(int q)
{
    switch (q)
    {
    case ADEMCO_CONTACTID_QUALIFIER_NEW_EVENT:
        return "New event";
    case ADEMCO_CONTACTID_QUALIFIER_NEW_RESTORE:
        return "New restore";
    case ADEMCO_CONTACTID_QUALIFIER_STATUS_REPORT:
        return "Status report";
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(const char *) ademco_contactid_event_to_str(int xyz)
{
    int entry;

    for (entry = 0;  ademco_codes[entry].code >= 0;  entry++)
    {
        if (xyz == ademco_codes[entry].code)
            return ademco_codes[entry].name;
    }
    return "???";
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_receiver_log_msg(ademco_contactid_receiver_state_t *s, const ademco_contactid_report_t *report)
{
    const char *t;

    span_log(&s->logging, SPAN_LOG_FLOW, "Ademco Contact ID message:\n");
    span_log(&s->logging, SPAN_LOG_FLOW, "    Account %X\n", report->acct);
    switch (report->mt)
    {
    case ADEMCO_CONTACTID_MESSAGE_TYPE_18:
    case ADEMCO_CONTACTID_MESSAGE_TYPE_98:
        t = "Contact ID";
        break;
    default:
        t = "???";
        break;
    }
    span_log(&s->logging, SPAN_LOG_FLOW, "    Message type %s (%X)\n", t, report->mt);
    t = ademco_contactid_msg_qualifier_to_str(report->q);
    span_log(&s->logging, SPAN_LOG_FLOW, "    Qualifier %s (%X)\n", t, report->q);
    t = ademco_contactid_event_to_str(report->xyz);
    span_log(&s->logging, SPAN_LOG_FLOW, "    Event %s (%X)\n", t, report->xyz);
    span_log(&s->logging, SPAN_LOG_FLOW, "    Group/partition %X\n", report->gg);
    span_log(&s->logging, SPAN_LOG_FLOW, "    User/Zone information %X\n", report->ccc);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_receiver_tx(ademco_contactid_receiver_state_t *s, int16_t amp[], int max_samples)
{
    int i;
    int samples;

    switch (s->step)
    {
    case 0:
        samples = (s->remaining_samples > max_samples)  ?  max_samples  :  s->remaining_samples;
        vec_zeroi16(amp, samples);
        s->remaining_samples -= samples;
        if (s->remaining_samples > 0)
            return samples;
        span_log(&s->logging, SPAN_LOG_FLOW, "Initial silence finished\n");
        s->step++;
        s->tone_phase_rate = dds_phase_rate(1400.0);
        s->tone_level = dds_scaling_dbm0(-11);
        s->tone_phase = 0;
        s->remaining_samples = ms_to_samples(100);
        return samples;
    case 1:
        samples = (s->remaining_samples > max_samples)  ?  max_samples  :  s->remaining_samples;
        for (i = 0;  i < samples;  i++)
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->tone_level, 0);
        s->remaining_samples -= samples;
        if (s->remaining_samples > 0)
            return samples;
        span_log(&s->logging, SPAN_LOG_FLOW, "1400Hz tone finished\n");
        s->step++;
        s->remaining_samples = ms_to_samples(100);
        return samples;
    case 2:
        samples = (s->remaining_samples > max_samples)  ?  max_samples  :  s->remaining_samples;
        vec_zeroi16(amp, samples);
        s->remaining_samples -= samples;
        if (s->remaining_samples > 0)
            return samples;
        span_log(&s->logging, SPAN_LOG_FLOW, "Second silence finished\n");
        s->step++;
        s->tone_phase_rate = dds_phase_rate(2300.0);
        s->tone_level = dds_scaling_dbm0(-11);
        s->tone_phase = 0;
        s->remaining_samples = ms_to_samples(100);
        return samples;
    case 3:
        samples = (s->remaining_samples > max_samples)  ?  max_samples  :  s->remaining_samples;
        for (i = 0;  i < samples;  i++)
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->tone_level, 0);
        s->remaining_samples -= samples;
        if (s->remaining_samples > 0)
            return samples;
        span_log(&s->logging, SPAN_LOG_FLOW, "2300Hz tone finished\n");
        s->step++;
        s->remaining_samples = ms_to_samples(100);
        return samples;
    case 4:
        /* Idle here, waiting for a response */
        return 0;
    case 5:
        samples = (s->remaining_samples > max_samples)  ?  max_samples  :  s->remaining_samples;
        vec_zeroi16(amp, samples);
        s->remaining_samples -= samples;
        if (s->remaining_samples > 0)
            return samples;
        span_log(&s->logging, SPAN_LOG_FLOW, "Sending kissoff\n");
        s->step++;
        s->tone_phase_rate = dds_phase_rate(1400.0);
        s->tone_level = dds_scaling_dbm0(-11);
        s->tone_phase = 0;
        s->remaining_samples = ms_to_samples(850);
        return samples;
    case 6:
        samples = (s->remaining_samples > max_samples)  ?  max_samples  :  s->remaining_samples;
        for (i = 0;  i < samples;  i++)
            amp[i] = dds_mod(&s->tone_phase, s->tone_phase_rate, s->tone_level, 0);
        s->remaining_samples -= samples;
        if (s->remaining_samples > 0)
            return samples;
        span_log(&s->logging, SPAN_LOG_FLOW, "1400Hz tone finished\n");
        s->step = 4;
        s->remaining_samples = ms_to_samples(100);
        return samples;
    }
    return max_samples;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_receiver_rx(ademco_contactid_receiver_state_t *s, const int16_t amp[], int samples)
{
    return dtmf_rx(&s->dtmf, amp, samples);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_receiver_fillin(ademco_contactid_receiver_state_t *s, int samples)
{
    return dtmf_rx_fillin(&s->dtmf, samples);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) ademco_contactid_receiver_get_logging_state(ademco_contactid_receiver_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

static void dtmf_digit_delivery(void *user_data, const char *digits, int len)
{
    ademco_contactid_receiver_state_t *s;
    ademco_contactid_report_t report;

    s = (ademco_contactid_receiver_state_t *) user_data;
    memcpy(&s->rx_digits[s->rx_digits_len], digits, len);
    s->rx_digits_len += len;
    if (s->rx_digits_len == 16)
    {
        s->rx_digits[16] = '\0';
        if (decode_msg(&report, s->rx_digits) == 0)
        {
            ademco_contactid_receiver_log_msg(s, &report);
            if (s->callback)
                s->callback(s->callback_user_data, &report);
            s->step++;
        }
        s->rx_digits_len = 0;
    }
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) ademco_contactid_receiver_set_realtime_callback(ademco_contactid_receiver_state_t *s,
                                                                   ademco_contactid_report_func_t callback,
                                                                   void *user_data)
{
    s->callback = callback;
    s->callback_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(ademco_contactid_receiver_state_t *) ademco_contactid_receiver_init(ademco_contactid_receiver_state_t *s,
                                                                                 ademco_contactid_report_func_t callback,
                                                                                 void *user_data)
{
    if (s == NULL)
    {
        if ((s = (ademco_contactid_receiver_state_t *) span_alloc(sizeof (*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "Ademco");

    dtmf_rx_init(&s->dtmf, dtmf_digit_delivery, (void *) s);
    s->rx_digits_len = 0;

    s->callback = callback;
    s->callback_user_data = user_data;

    s->step = 0;
    s->remaining_samples = ms_to_samples(500);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_receiver_release(ademco_contactid_receiver_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_receiver_free(ademco_contactid_receiver_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_sender_tx(ademco_contactid_sender_state_t *s, int16_t amp[], int max_samples)
{
    int sample;
    int samples;

    for (sample = 0;  sample < max_samples;  sample += samples)
    {
        switch (s->step)
        {
        case 0:
            if (!s->clear_to_send)
                return 0;
            s->clear_to_send = false;
            s->step++;
            s->remaining_samples = ms_to_samples(250);
            /* Fall through */
        case 1:
            samples = (s->remaining_samples > (max_samples - sample))  ?  (max_samples - sample)  :  s->remaining_samples;
            vec_zeroi16(&amp[sample], samples);
            s->remaining_samples -= samples;
            if (s->remaining_samples > 0)
                return samples;
            span_log(&s->logging, SPAN_LOG_FLOW, "Pre-send silence finished\n");
            s->step++;
            break;
        case 2:
            samples = dtmf_tx(&s->dtmf, &amp[sample], max_samples - sample);
            if (samples == 0)
            {
                s->clear_to_send = false;
                s->step = 0;
                return sample;
            }
            break;
        default:
            return sample;
        }
    }
    return sample;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_sender_rx(ademco_contactid_sender_state_t *s, const int16_t amp[], int samples)
{
#if defined(SPANDSP_USE_FIXED_POINT)
    int32_t energy_1400;
    int32_t energy_2300;
    int16_t xamp;
#else
    float energy_1400;
    float energy_2300;
    float xamp;
#endif
    int sample;
    int limit;
    int hit;
    int j;

    for (sample = 0;  sample < samples;  sample = limit)
    {
        if ((samples - sample) >= (GOERTZEL_SAMPLES_PER_BLOCK - s->current_sample))
            limit = sample + (GOERTZEL_SAMPLES_PER_BLOCK - s->current_sample);
        else
            limit = samples;
        for (j = sample;  j < limit;  j++)
        {
            xamp = amp[j];
            xamp = goertzel_preadjust_amp(xamp);
#if defined(SPANDSP_USE_FIXED_POINT)
            s->energy += ((int32_t) xamp*xamp);
#else
            s->energy += xamp*xamp;
#endif
            goertzel_samplex(&s->tone_1400, xamp);
            goertzel_samplex(&s->tone_2300, xamp);
        }
        s->current_sample += (limit - sample);
        if (s->current_sample < GOERTZEL_SAMPLES_PER_BLOCK)
            continue;

        energy_1400 = goertzel_result(&s->tone_1400);
        energy_2300 = goertzel_result(&s->tone_2300);
        hit = 0;
        if (energy_1400 > DETECTION_THRESHOLD  ||  energy_2300 > DETECTION_THRESHOLD)
        {
            if (energy_1400 > energy_2300)
            {
                if (energy_1400 > TONE_TO_TOTAL_ENERGY*s->energy)
                    hit = 1;
            }
            else
            {
                if (energy_2300 > TONE_TO_TOTAL_ENERGY*s->energy)
                    hit = 2;
            }
        }
        if (hit != s->in_tone  &&  hit == s->last_hit)
        {
            /* We have two successive indications that something has changed to a
               specific new state. */
            switch (s->tone_state)
            {
            case 0:
                if (hit == 1)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Receiving initial 1400Hz\n");
                    s->in_tone = hit;
                    s->tone_state = 1;
                    s->duration = 0;
                }
                break;
            case 1:
                /* We are looking for a burst of 1400Hz which is 100ms +- 5% long */
                if (hit == 0)
                {
                    if (s->duration < ms_to_samples(70)  ||  s->duration > ms_to_samples(130))
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Bad initial 1400Hz tone duration\n");
                        s->tone_state = 0;
                    }
                    else
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Received 1400Hz tone\n");
                        s->tone_state = 2;
                    }
                    s->in_tone = hit;
                    s->duration = 0;
                }
                break;
            case 2:
                /* We are looking for 100ms +-5% of silence after the 1400Hz tone */
                if (s->duration < ms_to_samples(70)  ||  s->duration > ms_to_samples(130))
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Bad silence length\n");
                    s->tone_state = 0;
                    s->in_tone = hit;
                }
                else if (hit == 2)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Received silence\n");
                    s->tone_state = 3;
                    s->in_tone = hit;
                }
                else
                {
                    s->tone_state = 0;
                    s->in_tone = 0;
                }
                s->duration = 0;
                break;
            case 3:
                /* We are looking for a burst of 2300Hz which is 100ms +- 5% long */
                if (hit == 0)
                {
                    if (s->duration < ms_to_samples(70)  ||  s->duration > ms_to_samples(130))
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Bad initial 2300Hz tone duration\n");
                        s->tone_state = 0;
                    }
                    else
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Received 2300Hz\n");
                        if (s->callback)
                            s->callback(s->callback_user_data, -1, 0, 0);
                        s->tone_state = 4;
                        /* Release the transmit side, and it will time the 250ms post tone delay */
                        s->clear_to_send = true;
                        s->tries = 0;
                        if (s->tx_digits_len)
                            s->timer = ms_to_samples(3000);
                    }
                    s->in_tone = hit;
                    s->duration = 0;
                }
                break;
            case 4:
                if (hit == 1)
                {
                    span_log(&s->logging, SPAN_LOG_FLOW, "Receiving kissoff\n");
                    s->tone_state = 5;
                    s->in_tone = hit;
                    s->duration = 0;
                }
                break;
            case 5:
                if (hit == 0)
                {
                    s->busy = false;
                    if (s->duration < ms_to_samples(400)  ||  s->duration > ms_to_samples(1500))
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Bad kissoff duration %d\n", s->duration);
                        if (++s->tries < 4)
                        {
                            dtmf_tx_put(&s->dtmf, s->tx_digits, s->tx_digits_len);
                            s->timer = ms_to_samples(3000);
                            s->tone_state = 4;
                        }
                        else
                        {
                            s->timer = 0;
                            if (s->callback)
                                s->callback(s->callback_user_data, false, 0, 0);
                        }
                    }
                    else
                    {
                        span_log(&s->logging, SPAN_LOG_FLOW, "Received good kissoff\n");
                        s->clear_to_send = true;
                        s->tx_digits_len = 0;
                        if (s->callback)
                            s->callback(s->callback_user_data, true, 0, 0);
                        s->tone_state = 4;
                        s->clear_to_send = true;
                        s->tries = 0;
                        if (s->tx_digits_len)
                            s->timer = ms_to_samples(3000);
                    }
                    s->in_tone = hit;
                    s->duration = 0;
                }
                break;
            }
        }
        s->last_hit = hit;
        s->duration += GOERTZEL_SAMPLES_PER_BLOCK;
        if (s->timer > 0)
        {
            s->timer -= GOERTZEL_SAMPLES_PER_BLOCK;
            if (s->timer <= 0)
            {
                span_log(&s->logging, SPAN_LOG_FLOW, "Timer expired\n");
                if (s->tone_state == 4  &&  s->tx_digits_len)
                {
                    if (++s->tries < 4)
                    {
                        dtmf_tx_put(&s->dtmf, s->tx_digits, s->tx_digits_len);
                        s->timer = ms_to_samples(3000);
                    }
                    else
                    {
                        s->timer = 0;
                        if (s->callback)
                            s->callback(s->callback_user_data, false, 0, 0);
                    }
                }
            }
        }
        s->energy = 0;
        s->current_sample = 0;
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_sender_fillin(ademco_contactid_sender_state_t *s, int samples)
{
    /* Restart any Goertzel and energy gathering operation we might be in the middle of. */
    goertzel_reset(&s->tone_1400);
    goertzel_reset(&s->tone_2300);
#if defined(SPANDSP_USE_FIXED_POINT)
    s->energy = 0;
#else
    s->energy = 0.0f;
#endif
    s->current_sample = 0;
    /* Don't update the hit detection. Pretend it never happened. */
    /* TODO: Surely we can be cleverer than this. */
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_sender_put(ademco_contactid_sender_state_t *s, const ademco_contactid_report_t *report)
{
    if (s->busy)
        return -1;
    if ((s->tx_digits_len = encode_msg(s->tx_digits, report)) < 0)
        return -1;
    s->busy = true;
    return dtmf_tx_put(&s->dtmf, s->tx_digits, s->tx_digits_len);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(logging_state_t *) ademco_contactid_sender_get_logging_state(ademco_contactid_sender_state_t *s)
{
    return &s->logging;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) ademco_contactid_sender_set_realtime_callback(ademco_contactid_sender_state_t *s,
                                                                 tone_report_func_t callback,
                                                                 void *user_data)
{
    s->callback = callback;
    s->callback_user_data = user_data;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(ademco_contactid_sender_state_t *) ademco_contactid_sender_init(ademco_contactid_sender_state_t *s,
                                                                             tone_report_func_t callback,
                                                                             void *user_data)
{
    if (s == NULL)
    {
        if ((s = (ademco_contactid_sender_state_t *) span_alloc(sizeof (*s))) == NULL)
            return NULL;
    }
    memset(s, 0, sizeof(*s));
    span_log_init(&s->logging, SPAN_LOG_NONE, NULL);
    span_log_set_protocol(&s->logging, "Ademco");

    if (!tone_rx_init)
    {
        make_goertzel_descriptor(&tone_1400_desc, 1400.0f, GOERTZEL_SAMPLES_PER_BLOCK);
        make_goertzel_descriptor(&tone_2300_desc, 2300.0f, GOERTZEL_SAMPLES_PER_BLOCK);
        tone_rx_init = true;
    }
    goertzel_init(&s->tone_1400, &tone_1400_desc);
    goertzel_init(&s->tone_2300, &tone_2300_desc);
    s->current_sample = 0;

    s->callback = callback;
    s->callback_user_data = user_data;

    s->step = 0;
    s->remaining_samples = ms_to_samples(100);
    dtmf_tx_init(&s->dtmf, NULL, NULL);
    /* The specified timing is 50-60ms on, 50-60ms off */
    dtmf_tx_set_timing(&s->dtmf, 55, 55);
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_sender_release(ademco_contactid_sender_state_t *s)
{
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) ademco_contactid_sender_free(ademco_contactid_sender_state_t *s)
{
    span_free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

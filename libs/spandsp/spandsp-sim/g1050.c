/*
 * SpanDSP - a series of DSP components for telephony
 *
 * g1050.c - IP network modeling, as per G.1050/TIA-921.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2007 Steve Underwood
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#if defined(HAVE_TGMATH_H)
#include <tgmath.h>
#endif
#if defined(HAVE_MATH_H)
#define GEN_CONST
#include <math.h>
#endif
#if defined(HAVE_STDBOOL_H)
#include <stdbool.h>
#else
#include "spandsp/stdbool.h"
#endif
#include "floating_fudge.h"

#include "spandsp.h"
#include "spandsp/g1050.h"

#define PACKET_LOSS_TIME    -1

g1050_constants_t g1050_constants[1] =
{
    {
        {
            {   /* Side A LAN */
                {
                    0.004,          /*! Probability of loss rate change low->high */
                    0.1             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.0,        /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.5,
                        0.0
                    }
                },
                1.0,                /*! Impulse height, based on MTU and bit rate */
                0.0,                /*! Impulse decay coefficient */
                0.001,              /*! Probability of packet loss due to occupancy. */
                0.15                /*! Probability of packet loss due to a multiple access collision. */
            },
            {   /* Side A access link */
                {
                    0.0002,         /*! Probability of loss rate change low->high */
                    0.2             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.001,      /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.3,
                        0.4
                    }
                },
                40.0,               /*! Impulse height, based on MTU and bit rate */
                0.75,               /*! Impulse decay coefficient */
                0.0005,             /*! Probability of packet loss due to occupancy. */
                0.0                 /*! Probability of packet loss due to a multiple access collision. */
            },
            {   /* Side B access link */
                {
                    0.0002,         /*! Probability of loss rate change low->high */
                    0.2             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.001,      /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.3,
                        0.4
                    }
                },
                40.0,               /*! Impulse height, based on MTU and bit rate */
                0.75,               /*! Impulse decay coefficient */
                0.0005,             /*! Probability of packet loss due to occupancy. */
                0.0                 /*! Probability of packet loss due to a multiple access collision. */
            },
            {   /* Side B LAN */
                {
                    0.004,          /*! Probability of loss rate change low->high */
                    0.1             /*! Probability of loss rate change high->low */
                },
                {
                    {
                        0.0,        /*! Probability of an impulse */
                        0.0,
                    },
                    {
                        0.5,
                        0.0
                    }
                },
                1.0,                /*! Impulse height, based on MTU and bit rate */
                0.0,                /*! Impulse decay coefficient */
                0.001,              /*! Probability of packet loss due to occupancy. */
                0.15                /*! Probability of packet loss due to a multiple access collision. */
            }
        }
    }
};

g1050_channel_speeds_t g1050_speed_patterns[168] =
{
    {  4000000, 0,   128000,   768000, 0,   4000000, 0,   128000,   768000, 0, 0.360},
    {  4000000, 0,   128000,   768000, 0,  20000000, 0,   128000,   768000, 0, 0.720},
    {  4000000, 0,   128000,   768000, 0, 100000000, 0,   128000,   768000, 0, 0.360},
    { 20000000, 0,   128000,   768000, 0,  20000000, 0,   128000,   768000, 0, 0.360},
    { 20000000, 0,   128000,   768000, 0, 100000000, 0,   128000,   768000, 0, 0.360},
    {100000000, 0,   128000,   768000, 0, 100000000, 0,   128000,   768000, 0, 0.090},
    {  4000000, 0,   128000,  1536000, 0,   4000000, 0,   384000,   768000, 0, 0.720},
    {  4000000, 0,   128000,  1536000, 0,  20000000, 0,   384000,   768000, 0, 1.470},
    {  4000000, 0,   128000,  1536000, 0, 100000000, 0,   384000,   768000, 0, 0.840},
    { 20000000, 0,   128000,  1536000, 0,  20000000, 0,   384000,   768000, 0, 0.750},
    { 20000000, 0,   128000,  1536000, 0, 100000000, 0,   384000,   768000, 0, 0.855},
    {100000000, 0,   128000,  1536000, 0, 100000000, 0,   384000,   768000, 0, 0.240},
    {  4000000, 0,   128000,  3000000, 0,   4000000, 0,   384000,   768000, 0, 0.120},
    {  4000000, 0,   128000,  3000000, 0,  20000000, 0,   384000,   768000, 0, 0.420},
    {  4000000, 0,   128000,  3000000, 0, 100000000, 0,   384000,   768000, 0, 0.840},
    { 20000000, 0,   128000,  3000000, 0,  20000000, 0,   384000,   768000, 0, 0.300},
    { 20000000, 0,   128000,  3000000, 0, 100000000, 0,   384000,   768000, 0, 0.930},
    {100000000, 0,   128000,  3000000, 0, 100000000, 0,   384000,   768000, 0, 0.390},
    {  4000000, 0,   384000,   768000, 0,   4000000, 0,   128000,  1536000, 0, 0.720},
    {  4000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  1536000, 0, 1.470},
    {  4000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  1536000, 0, 0.840},
    { 20000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  1536000, 0, 0.750},
    { 20000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  1536000, 0, 0.855},
    {100000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  1536000, 0, 0.240},
    {  4000000, 0,   384000,  1536000, 0,   4000000, 0,   384000,  1536000, 0, 1.440},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 3.000},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 1.920},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 1.563},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 2.000},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.640},
    {  4000000, 0,   384000,  3000000, 0,   4000000, 0,   384000,  1536000, 0, 0.240},
    {  4000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.850},
    {  4000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 1.720},
    { 20000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.625},
    { 20000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 2.025},
    {100000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 1.040},
    {  4000000, 0,   384000,   768000, 0,   4000000, 0,   128000,  3000000, 0, 0.120},
    {  4000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  3000000, 0, 0.420},
    {  4000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  3000000, 0, 0.840},
    { 20000000, 0,   384000,   768000, 0,  20000000, 0,   128000,  3000000, 0, 0.300},
    { 20000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  3000000, 0, 0.930},
    {100000000, 0,   384000,   768000, 0, 100000000, 0,   128000,  3000000, 0, 0.390},
    {  4000000, 0,   384000,  1536000, 0,   4000000, 0,   384000,  3000000, 0, 0.240},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  3000000, 0, 0.850},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  3000000, 0, 1.720},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,   384000,  3000000, 0, 0.625},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  3000000, 0, 2.025},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,   384000,  3000000, 0, 1.040},
    {  4000000, 0,   384000,  3000000, 0,   4000000, 0,   384000,  3000000, 0, 0.040},
    {  4000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  3000000, 0, 0.200},
    {  4000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  3000000, 0, 0.520},
    { 20000000, 0,   384000,  3000000, 0,  20000000, 0,   384000,  3000000, 0, 0.250},
    { 20000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  3000000, 0, 1.300},
    {100000000, 0,   384000,  3000000, 0, 100000000, 0,   384000,  3000000, 0, 1.690},
    {  4000000, 0,   128000,  1536000, 0,  20000000, 0,   768000,  1536000, 0, 0.090},
    {  4000000, 0,   128000,  1536000, 0, 100000000, 0,   768000,  1536000, 0, 0.360},
    { 20000000, 0,   128000,  1536000, 0,  20000000, 0,   768000,  1536000, 0, 0.090},
    { 20000000, 0,   128000,  1536000, 0, 100000000, 0,   768000,  1536000, 0, 0.405},
    {100000000, 0,   128000,  1536000, 0, 100000000, 0,   768000,  1536000, 0, 0.180},
    {  4000000, 0,   128000,  7000000, 0,  20000000, 0,   768000,   768000, 0, 0.270},
    {  4000000, 0,   128000,  7000000, 0, 100000000, 0,   768000,   768000, 0, 1.080},
    { 20000000, 0,   128000,  7000000, 0,  20000000, 0,   768000,   768000, 0, 0.270},
    { 20000000, 0,   128000,  7000000, 0, 100000000, 0,   768000,   768000, 0, 1.215},
    {100000000, 0,   128000,  7000000, 0, 100000000, 0,   768000,   768000, 0, 0.540},
    {  4000000, 0,   128000, 13000000, 0,  20000000, 0,   768000, 13000000, 0, 0.030},
    {  4000000, 0,   128000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.120},
    { 20000000, 0,   128000, 13000000, 0,  20000000, 0,   768000, 13000000, 0, 0.030},
    { 20000000, 0,   128000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.135},
    {100000000, 0,   128000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.060},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  1536000, 0, 0.180},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.720},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  1536000, 0, 0.188},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.870},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.480},
    {  4000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  1536000, 0, 0.540},
    {  4000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 2.160},
    { 20000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  1536000, 0, 0.563},
    { 20000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 2.610},
    {100000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 1.440},
    {  4000000, 0,   384000, 13000000, 0,  20000000, 0,  1536000, 13000000, 0, 0.060},
    {  4000000, 0,   384000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.240},
    { 20000000, 0,   384000, 13000000, 0,  20000000, 0,  1536000, 13000000, 0, 0.063},
    { 20000000, 0,   384000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.290},
    {100000000, 0,   384000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.160},
    {  4000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  3000000, 0, 0.030},
    {  4000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  3000000, 0, 0.120},
    { 20000000, 0,   384000,  1536000, 0,  20000000, 0,  1536000,  3000000, 0, 0.075},
    { 20000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  3000000, 0, 0.495},
    {100000000, 0,   384000,  1536000, 0, 100000000, 0,  1536000,  3000000, 0, 0.780},
    {  4000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  3000000, 0, 0.090},
    {  4000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  3000000, 0, 0.360},
    { 20000000, 0,   384000,  7000000, 0,  20000000, 0,   768000,  3000000, 0, 0.225},
    { 20000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  3000000, 0, 1.485},
    {100000000, 0,   384000,  7000000, 0, 100000000, 0,   768000,  3000000, 0, 2.340},
    {  4000000, 0,   384000, 13000000, 0,  20000000, 0,  3000000, 13000000, 0, 0.010},
    {  4000000, 0,   384000, 13000000, 0, 100000000, 0,  3000000, 13000000, 0, 0.040},
    { 20000000, 0,   384000, 13000000, 0,  20000000, 0,  3000000, 13000000, 0, 0.025},
    { 20000000, 0,   384000, 13000000, 0, 100000000, 0,  3000000, 13000000, 0, 0.165},
    {100000000, 0,   384000, 13000000, 0, 100000000, 0,  3000000, 13000000, 0, 0.260},
    {  4000000, 0,   768000,  1536000, 0,  20000000, 0,   128000,  1536000, 0, 0.090},
    { 20000000, 0,   768000,  1536000, 0,  20000000, 0,   128000,  1536000, 0, 0.090},
    { 20000000, 0,   768000,  1536000, 0, 100000000, 0,   128000,  1536000, 0, 0.405},
    {  4000000, 0,   768000,  1536000, 0, 100000000, 0,   128000,  1536000, 0, 0.360},
    {100000000, 0,   768000,  1536000, 0, 100000000, 0,   128000,  1536000, 0, 0.180},
    {  4000000, 0,  1536000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 0.180},
    { 20000000, 0,  1536000,  1536000, 0,  20000000, 0,   384000,  1536000, 0, 0.188},
    { 20000000, 0,  1536000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.870},
    {  4000000, 0,  1536000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.720},
    {100000000, 0,  1536000,  1536000, 0, 100000000, 0,   384000,  1536000, 0, 0.480},
    {  4000000, 0,  1536000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.030},
    { 20000000, 0,  1536000,  3000000, 0,  20000000, 0,   384000,  1536000, 0, 0.075},
    { 20000000, 0,  1536000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 0.495},
    {  4000000, 0,  1536000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 0.120},
    {100000000, 0,  1536000,  3000000, 0, 100000000, 0,   384000,  1536000, 0, 0.780},
    {  4000000, 0,   768000,   768000, 0,  20000000, 0,   128000,  7000000, 0, 0.270},
    { 20000000, 0,   768000,   768000, 0,  20000000, 0,   128000,  7000000, 0, 0.270},
    { 20000000, 0,   768000,   768000, 0, 100000000, 0,   128000,  7000000, 0, 1.215},
    {  4000000, 0,   768000,   768000, 0, 100000000, 0,   128000,  7000000, 0, 1.080},
    {100000000, 0,   768000,   768000, 0, 100000000, 0,   128000,  7000000, 0, 0.540},
    {  4000000, 0,   768000,  1536000, 0,  20000000, 0,   384000,  7000000, 0, 0.540},
    { 20000000, 0,   768000,  1536000, 0,  20000000, 0,   384000,  7000000, 0, 0.563},
    { 20000000, 0,   768000,  1536000, 0, 100000000, 0,   384000,  7000000, 0, 2.610},
    {  4000000, 0,   768000,  1536000, 0, 100000000, 0,   384000,  7000000, 0, 2.160},
    {100000000, 0,   768000,  1536000, 0, 100000000, 0,   384000,  7000000, 0, 1.440},
    {  4000000, 0,   768000,  3000000, 0,  20000000, 0,   384000,  7000000, 0, 0.090},
    { 20000000, 0,   768000,  3000000, 0,  20000000, 0,   384000,  7000000, 0, 0.225},
    { 20000000, 0,   768000,  3000000, 0, 100000000, 0,   384000,  7000000, 0, 1.485},
    {  4000000, 0,   768000,  3000000, 0, 100000000, 0,   384000,  7000000, 0, 0.360},
    {100000000, 0,   768000,  3000000, 0, 100000000, 0,   384000,  7000000, 0, 2.340},
    {  4000000, 0,   768000, 13000000, 0,  20000000, 0,   128000, 13000000, 0, 0.030},
    { 20000000, 0,   768000, 13000000, 0,  20000000, 0,   128000, 13000000, 0, 0.030},
    { 20000000, 0,   768000, 13000000, 0, 100000000, 0,   128000, 13000000, 0, 0.135},
    {  4000000, 0,   768000, 13000000, 0, 100000000, 0,   128000, 13000000, 0, 0.120},
    {100000000, 0,   768000, 13000000, 0, 100000000, 0,   128000, 13000000, 0, 0.060},
    {  4000000, 0,  1536000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.060},
    { 20000000, 0,  1536000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.063},
    { 20000000, 0,  1536000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.290},
    {  4000000, 0,  1536000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.240},
    {100000000, 0,  1536000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.160},
    {  4000000, 0,  3000000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.010},
    { 20000000, 0,  3000000, 13000000, 0,  20000000, 0,   384000, 13000000, 0, 0.025},
    { 20000000, 0,  3000000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.165},
    {  4000000, 0,  3000000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.040},
    {100000000, 0,  3000000, 13000000, 0, 100000000, 0,   384000, 13000000, 0, 0.260},
    { 20000000, 0,  1536000,  1536000, 0,  20000000, 0,  1536000,  1536000, 0, 0.023},
    { 20000000, 0,  1536000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.180},
    {100000000, 0,  1536000,  1536000, 0, 100000000, 0,  1536000,  1536000, 0, 0.360},
    { 20000000, 0,  1536000,  7000000, 0,  20000000, 0,   768000,  1536000, 0, 0.068},
    { 20000000, 0,  1536000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 0.540},
    {100000000, 0,  1536000,  7000000, 0, 100000000, 0,   768000,  1536000, 0, 1.080},
    { 20000000, 0,  1536000, 13000000, 0,  20000000, 0,  1536000, 13000000, 0, 0.015},
    { 20000000, 0,  1536000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.120},
    {100000000, 0,  1536000, 13000000, 0, 100000000, 0,  1536000, 13000000, 0, 0.240},
    { 20000000, 0,   768000,  1536000, 0,  20000000, 0,  1536000,  7000000, 0, 0.068},
    { 20000000, 0,   768000,  1536000, 0, 100000000, 0,  1536000,  7000000, 0, 0.540},
    {100000000, 0,   768000,  1536000, 0, 100000000, 0,  1536000,  7000000, 0, 1.080},
    { 20000000, 0,   768000,  7000000, 0,  20000000, 0,   768000,  7000000, 0, 0.203},
    { 20000000, 0,   768000,  7000000, 0, 100000000, 0,   768000,  7000000, 0, 1.620},
    {100000000, 0,   768000,  7000000, 0, 100000000, 0,   768000,  7000000, 0, 3.240},
    { 20000000, 0,   768000, 13000000, 0,  20000000, 0,  7000000, 13000000, 0, 0.023},
    { 20000000, 0,   768000, 13000000, 0, 100000000, 0,  7000000, 13000000, 0, 0.180},
    {100000000, 0,   768000, 13000000, 0, 100000000, 0,  7000000, 13000000, 0, 0.360},
    { 20000000, 0,  7000000, 13000000, 0,  20000000, 0,   768000, 13000000, 0, 0.023},
    { 20000000, 0,  7000000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.180},
    {100000000, 0,  7000000, 13000000, 0, 100000000, 0,   768000, 13000000, 0, 0.360},
    { 20000000, 0, 13000000, 13000000, 0,  20000000, 0, 13000000, 13000000, 0, 0.003},
    { 20000000, 0, 13000000, 13000000, 0, 100000000, 0, 13000000, 13000000, 0, 0.020},
    {100000000, 0, 13000000, 13000000, 0, 100000000, 0, 13000000, 13000000, 0, 0.040}
};

g1050_model_t g1050_standard_models[9] =
{
    {   /* Severity 0 - no impairment */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            0,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            0.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.0,        /*! Basic delay of the regional backbone, in seconds */
            0.0,        /*! Basic delay of the intercontinental backbone, in seconds */
            0.0,        /*! Percentage packet loss of the backbone */
            0.0,        /*! Maximum jitter of the backbone, in seconds */
            0.0,        /*! Interval between the backbone route flapping between two paths, in seconds */
            0.0,        /*! The difference in backbone delay between the two routes we flap between, in seconds */
            0.0,        /*! The interval between link failures, in seconds */
            0.0,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        }
    },
    {   /* Severity A */
        {
            50,         /*! Percentage likelihood of occurance in scenario A */
            5,          /*! Percentage likelihood of occurance in scenario B */
            5,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            1.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.004,      /*! Basic delay of the regional backbone, in seconds */
            0.016,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.0,        /*! Percentage packet loss of the backbone */
            0.005,      /*! Maximum jitter of the backbone, in seconds */
            0.0,        /*! Interval between the backbone route flapping between two paths, in seconds */
            0.0,        /*! The difference in backbone delay between the two routes we flap between, in seconds */
            0.0,        /*! The interval between link failures, in seconds */
            0.0,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            0.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            1.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity B */
        {
            30,         /*! Percentage likelihood of occurance in scenario A */
            25,         /*! Percentage likelihood of occurance in scenario B */
            5,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            1.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.008,      /*! Basic delay of the regional backbone, in seconds */
            0.032,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.01,       /*! Percentage packet loss of the backbone */
            0.01,       /*! Maximum jitter of the backbone, in seconds */
            3600.0,     /*! Interval between the backbone route flapping between two paths, in seconds */
            0.002,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            3600.0,     /*! The interval between link failures, in seconds */
            0.064,      /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            1.0,        /*! Percentage occupancy */
            512,        /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity C */
        {
            15,         /*! Percentage likelihood of occurance in scenario A */
            30,         /*! Percentage likelihood of occurance in scenario B */
            10,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            3.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.016,      /*! Basic delay of the regional backbone, in seconds */
            0.064,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.02,       /*! Percentage packet loss of the backbone */
            0.016,      /*! Maximum jitter of the backbone, in seconds */
            1800.0,     /*! Interval between the backbone route flapping between two paths, in seconds */
            0.004,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            1800.0,     /*! The interval between link failures, in seconds */
            0.128,      /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            2.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            3.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity D */
        {
            5,          /*! Percentage likelihood of occurance in scenario A */
            25,         /*! Percentage likelihood of occurance in scenario B */
            15,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            5.0,         /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            4.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.032,      /*! Basic delay of the regional backbone, in seconds */
            0.128,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.04,       /*! Percentage packet loss of the backbone */
            0.04,       /*! Maximum jitter of the backbone, in seconds */
            900.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.008,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            900.0,      /*! The interval between link failures, in seconds */
            0.256,      /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            4.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            5.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity E */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            10,         /*! Percentage likelihood of occurance in scenario B */
            20,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.064,      /*! Basic delay of the regional backbone, in seconds */
            0.196,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.1,        /*! Percentage packet loss of the backbone */
            0.07,       /*! Maximum jitter of the backbone, in seconds */
            480.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.016,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            480.0,      /*! The interval between link failures, in seconds */
            0.4,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            8.0,        /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity F */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            25,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            12.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            15.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.128,      /*! Basic delay of the regional backbone, in seconds */
            0.256,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.2,        /*! Percentage packet loss of the backbone */
            0.1,        /*! Maximum jitter of the backbone, in seconds */
            240.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.032,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            240.0,      /*! The interval between link failures, in seconds */
            0.8,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            15.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            12.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity G */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            15,         /*! Percentage likelihood of occurance in scenario C */
        },
        {
            16.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            30.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.256,      /*! Basic delay of the regional backbone, in seconds */
            0.512,      /*! Basic delay of the intercontinental backbone, in seconds */
            0.5,        /*! Percentage packet loss of the backbone */
            0.15,       /*! Maximum jitter of the backbone, in seconds */
            120.0,      /*! Interval between the backbone route flapping between two paths, in seconds */
            0.064,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            120.0,      /*! The interval between link failures, in seconds */
            1.6,        /*! The duration of link failures, in seconds */
            0.0,        /*! Probability of packet loss in the backbone, in percent */
            0.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            30.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            16.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    },
    {   /* Severity H */
        {
            0,          /*! Percentage likelihood of occurance in scenario A */
            0,          /*! Percentage likelihood of occurance in scenario B */
            5,          /*! Percentage likelihood of occurance in scenario C */
        },
        {
            20.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        },
        {
            50.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            0.512,      /*! Basic delay of the regional backbone, in seconds */
            0.768,      /*! Basic delay of the intercontinental backbone, in seconds */
            1.0,        /*! Percentage packet loss of the backbone */
            0.5,        /*! Maximum jitter of the backbone, in seconds */
            60.0,       /*! Interval between the backbone route flapping between two paths, in seconds */
            0.128,      /*! The difference in backbone delay between the two routes we flap between, in seconds */
            60.0,       /*! The interval between link failures, in seconds */
            3.0,        /*! The duration of link failures, in seconds */
            1.0,        /*! Probability of packet loss in the backbone, in percent */
            1.0         /*! Probability of a packet going out of sequence in the backbone. */
        },
        {
            50.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0         /*! Peak jitter */
        },
        {
            20.0,       /*! Percentage occupancy */
            1508,       /*! MTU */
            0.0015      /*! Peak jitter */
        }
    }
};

#if defined(HAVE_DRAND48)
static __inline__ void q1050_rand_init(void)
{
    srand48(time(NULL));
}
/*- End of function --------------------------------------------------------*/

static __inline__ double q1050_rand(void)
{
    return drand48();
}
/*- End of function --------------------------------------------------------*/
#else
static __inline__ void q1050_rand_init(void)
{
    srand(time(NULL));
}
/*- End of function --------------------------------------------------------*/

static __inline__ double q1050_rand(void)
{
    return (double) rand()/(double) RAND_MAX;
}
/*- End of function --------------------------------------------------------*/
#endif

static __inline__ double scale_probability(double prob, double scale)
{
    /* Re-calculate probability based on a different time interval */
    return 1.0 - pow(1.0 - prob, scale);
}
/*- End of function --------------------------------------------------------*/

static void g1050_segment_init(g1050_segment_state_t *s,
                               int link_type,
                               g1050_segment_constants_t *constants,
                               g1050_segment_model_t *parms,
                               int bit_rate,
                               int multiple_access,
                               int qos_enabled,
                               int packet_size,
                               int packet_rate)
{
    double x;
    double packet_interval;

    memset(s, 0, sizeof(*s));

    packet_interval = 1000.0/packet_rate;
    /* Some calculatons are common to both LAN and access links, and those that are not. */
    s->link_type = link_type;
    s->prob_loss_rate_change[0] = scale_probability(constants->prob_loss_rate_change[0]*parms->percentage_occupancy, 1.0/packet_interval);

    s->serial_delay = packet_size*8.0/bit_rate;
    if (link_type == G1050_LAN_LINK)
    {
        s->prob_loss_rate_change[1] = scale_probability(constants->prob_loss_rate_change[1], 1.0/packet_interval);
        s->prob_impulse[0] = constants->prob_impulse[0][0];
        s->prob_impulse[1] = constants->prob_impulse[1][0];
        s->impulse_coeff = constants->impulse_coeff;
        s->impulse_height = parms->mtu*(8.0/bit_rate)*(1.0 + parms->percentage_occupancy/constants->impulse_height);
    }
    else if (link_type == G1050_ACCESS_LINK)
    {
        s->prob_loss_rate_change[1] = scale_probability(constants->prob_loss_rate_change[1]/(1.0 + parms->percentage_occupancy), 1.0/packet_interval);
        s->prob_impulse[0] = scale_probability(constants->prob_impulse[0][0] + (parms->percentage_occupancy/2000.0), 1.0/packet_interval);
        s->prob_impulse[1] = scale_probability(constants->prob_impulse[1][0] + (constants->prob_impulse[1][1]*parms->percentage_occupancy/100.0), 1.0/packet_interval);
        s->impulse_coeff = 1.0 - scale_probability(1.0 - constants->impulse_coeff, 1.0/packet_interval);
        x = (1.0 - constants->impulse_coeff)/(1.0 - s->impulse_coeff);
        s->impulse_height = x*parms->mtu*(8.0/bit_rate)*(1.0 + parms->percentage_occupancy/constants->impulse_height);
    }

    /* The following are calculated the same way for LAN and access links */
    s->prob_packet_loss = constants->prob_packet_loss*parms->percentage_occupancy;
    s->qos_enabled = qos_enabled;
    s->multiple_access = multiple_access;
    s->prob_packet_collision_loss = constants->prob_packet_collision_loss;
    s->max_jitter = parms->max_jitter;

    /* The following is common state information to all links. */
    s->high_loss = false;
    s->congestion_delay = 0.0;
    s->last_arrival_time = 0.0;

    /* Count of packets lost in this segment. */
    s->lost_packets = 0;
    s->lost_packets_2 = 0;
}
/*- End of function --------------------------------------------------------*/

static void g1050_core_init(g1050_core_state_t *s, g1050_core_model_t *parms, int packet_rate)
{
    memset(s, 0, sizeof(*s));

    /* Set up route flapping. */
    /* This is the length of the period of both the delayed duration and the non-delayed. */
    s->route_flap_interval = parms->route_flap_interval*G1050_TICKS_PER_SEC;

    /* How much additional delay is added or subtracted during route flaps. */
    s->route_flap_delta = parms->route_flap_delay;

    /* Current tick count. This is initialized so that we are part way into the first
       CLEAN interval before the first change occurs. This is a random portion of the
       period. When we reach the first flap, the flapping in both directions becomes
       periodic. */
    s->route_flap_counter = s->route_flap_interval - 99 - floor(s->route_flap_interval*q1050_rand());
    s->link_failure_interval_ticks = parms->link_failure_interval*G1050_TICKS_PER_SEC;

    /* Link failures occur when the count reaches this number of ticks. */
    /* Duration of a failure. */
    s->link_failure_duration_ticks = floor((G1050_TICKS_PER_SEC*parms->link_failure_duration));
    /* How far into the first CLEAN interval we are. This is like the route flap initialzation. */
    s->link_failure_counter = s->link_failure_interval_ticks - 99 - floor(s->link_failure_interval_ticks*q1050_rand());
    s->link_recovery_counter = s->link_failure_duration_ticks;

    s->base_delay = parms->base_regional_delay;
    s->max_jitter = parms->max_jitter;
    s->prob_packet_loss = parms->prob_packet_loss/100.0;
    s->prob_oos = parms->prob_oos/100.0;
    s->last_arrival_time = 0.0;
    s->delay_delta = 0;

    /* Count of packets lost in this segment. */
    s->lost_packets = 0;
    s->lost_packets_2 = 0;
}
/*- End of function --------------------------------------------------------*/

static void g1050_segment_model(g1050_segment_state_t *s, double delays[], int len)
{
    int i;
    bool lose;
    int was_high_loss;
    double impulse;
    double slice_delay;

    /* Compute delay and loss value for each time slice. */
    for (i = 0;  i < len;  i++)
    {
        lose = false;
        /* Initialize delay to the serial delay plus some jitter. */
        slice_delay = s->serial_delay + s->max_jitter*q1050_rand();
        /* If no QoS, do congestion delay and packet loss analysis. */
        if (!s->qos_enabled)
        {
            /* To match the logic in G.1050 we need to record the current loss state, before
               checking if we should change. */
            was_high_loss = s->high_loss;
            /* Toggle between the low-loss and high-loss states, based on the transition probability. */
            if (q1050_rand() < s->prob_loss_rate_change[was_high_loss])
                s->high_loss = !s->high_loss;
            impulse = 0.0;
            if (q1050_rand() < s->prob_impulse[was_high_loss])
            {
                impulse = s->impulse_height;
                if (!was_high_loss  ||  s->link_type == G1050_LAN_LINK)
                    impulse *= q1050_rand();
            }

            if (was_high_loss  &&  q1050_rand() < s->prob_packet_loss)
                lose = true;
            /* Single pole LPF for the congestion delay impulses. */
            s->congestion_delay = s->congestion_delay*s->impulse_coeff + impulse*(1.0 - s->impulse_coeff);
            slice_delay += s->congestion_delay;
        }
        /* If duplex mismatch on LAN, packet loss based on loss probability. */
        if (s->multiple_access  &&  (q1050_rand() < s->prob_packet_collision_loss))
            lose = true;
        /* Put computed delay into time slice array. */
        if (lose)
        {
            delays[i] = PACKET_LOSS_TIME;
            s->lost_packets++;
        }
        else
        {
            delays[i] = slice_delay;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static void g1050_core_model(g1050_core_state_t *s, double delays[], int len)
{
    int32_t i;
    bool lose;
    double jitter_delay;

    for (i = 0;  i < len;  i++)
    {
        lose = false;
        jitter_delay = s->base_delay + s->max_jitter*q1050_rand();
        /* Route flapping */
        if (--s->route_flap_counter <= 0)
        {
            /* Route changed */
            s->delay_delta = s->route_flap_delta - s->delay_delta;
            s->route_flap_counter = s->route_flap_interval;
        }
        if (q1050_rand() < s->prob_packet_loss)
            lose = true;
        /* Link failures */
        if (--s->link_failure_counter <= 0)
        {
            /* We are in a link failure */
            lose = true;
            if (--s->link_recovery_counter <= 0)
            {
                /* Leave failure state. */
                s->link_failure_counter = s->link_failure_interval_ticks;
                s->link_recovery_counter = s->link_failure_duration_ticks;
                lose = false;
            }
        }
        if (lose)
        {
            delays[i] = PACKET_LOSS_TIME;
            s->lost_packets++;
        }
        else
        {
            delays[i] = jitter_delay + s->delay_delta;
        }
    }
}
/*- End of function --------------------------------------------------------*/

static int g1050_segment_delay(g1050_segment_state_t *s,
                               double base_time,
                               double arrival_times[],
                               double delays[],
                               int num_packets)
{
    int i;
    int32_t departure_time;
    int lost_packets;

    /* Add appropriate delays to the packets for the segments before the core. */
    lost_packets = 0;
    for (i = 0;  i < num_packets;  i++)
    {
        /* Apply half a millisecond of rounding, as we working in millisecond steps. */
        departure_time = (arrival_times[i] + 0.0005 - base_time)*G1050_TICKS_PER_SEC;
        if (arrival_times[i] == PACKET_LOSS_TIME)
        {
            /* Lost already */
        }
        else if (delays[departure_time] == PACKET_LOSS_TIME)
        {
            arrival_times[i] = PACKET_LOSS_TIME;
            lost_packets++;
        }
        else
        {
            arrival_times[i] += delays[departure_time];
            if (arrival_times[i] < s->last_arrival_time)
                arrival_times[i] = s->last_arrival_time;
            else
                s->last_arrival_time = arrival_times[i];
        }
    }
    return lost_packets;
}
/*- End of function --------------------------------------------------------*/

static int g1050_segment_delay_preserve_order(g1050_segment_state_t *s,
                                              double base_time,
                                              double arrival_times_a[],
                                              double arrival_times_b[],
                                              double delays[],
                                              int num_packets)
{
    int i;
    int j;
    int departure_time;
    double last_arrival_time;
    double last_arrival_time_temp;
    int lost_packets;

    /* Add appropriate delays to the packets for the segments after the core. */
    last_arrival_time = 0.0;
    last_arrival_time_temp = 0.0;
    lost_packets = 0;
    for (i = 0;  i < num_packets;  i++)
    {
        /* We need to preserve the order that came out of the core, so we
           use an alternate array for the results.  */
        /* Apply half a millisecond of rounding, as we working in millisecond steps. */
        departure_time = (arrival_times_a[i] + 0.0005 - base_time)*G1050_TICKS_PER_SEC;
        if (arrival_times_a[i] == PACKET_LOSS_TIME)
        {
            /* Lost already */
            arrival_times_b[i] = PACKET_LOSS_TIME;
        }
        else if (delays[departure_time] == PACKET_LOSS_TIME)
        {
            arrival_times_b[i] = PACKET_LOSS_TIME;
            lost_packets++;
        }
        else
        {
            arrival_times_b[i] = arrival_times_a[i] + delays[departure_time];
            if (arrival_times_a[i] < last_arrival_time)
            {
                /* If a legitimate out of sequence packet is detected, search
                   back a fixed amount of time to preserve order. */
                for (j = i - 1;  j >= 0;  j--)
                {
                    if ((arrival_times_a[j] != PACKET_LOSS_TIME)
                        &&
                        (arrival_times_b[j] != PACKET_LOSS_TIME))
                    {
                        if ((arrival_times_a[i] - arrival_times_a[j]) > SEARCHBACK_PERIOD)
                            break;
                        if ((arrival_times_a[j] > arrival_times_a[i])
                            &&
                            (arrival_times_b[j] < arrival_times_b[i]))
                        {
                            arrival_times_b[j] = arrival_times_b[i];
                        }
                    }
                }
            }
            else
            {
                last_arrival_time = arrival_times_a[i];
                if (arrival_times_b[i] < last_arrival_time_temp)
                    arrival_times_b[i] = last_arrival_time_temp;
                else
                    last_arrival_time_temp = arrival_times_b[i];
            }
        }
    }
    return lost_packets;
}
/*- End of function --------------------------------------------------------*/

static int g1050_core_delay(g1050_core_state_t *s,
                            double base_time,
                            double arrival_times[],
                            double delays[],
                            int num_packets)
{
    int i;
    int departure_time;
    int lost_packets;

    /* This element does NOT preserve packet order. */
    lost_packets = 0;
    for (i = 0;  i < num_packets;  i++)
    {
        /* Apply half a millisecond of rounding, as we working in millisecond steps. */
        departure_time = (arrival_times[i] + 0.0005 - base_time)*G1050_TICKS_PER_SEC;
        if (arrival_times[i] == PACKET_LOSS_TIME)
        {
            /* Lost already */
        }
        else if (delays[departure_time] == PACKET_LOSS_TIME)
        {
            arrival_times[i] = PACKET_LOSS_TIME;
            lost_packets++;
        }
        else
        {
            /* Not lost. Compute arrival time. */
            arrival_times[i] += delays[departure_time];
            if (arrival_times[i] < s->last_arrival_time)
            {
                /* This packet is EARLIER than the last one. It is out of order! */
                /* Do we allow it to stay out of order? */
                if (q1050_rand() >= s->prob_oos)
                    arrival_times[i] = s->last_arrival_time;
            }
            else
            {
                /* Packet is in the correct order, relative to the last one. */
                s->last_arrival_time = arrival_times[i];
            }
        }
    }
    return lost_packets;
}
/*- End of function --------------------------------------------------------*/

static void g1050_simulate_chunk(g1050_state_t *s)
{
    int i;

    s->base_time += 1.0;

    memmove(&s->segment[0].delays[0], &s->segment[0].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[0].delays[0]));
    g1050_segment_model(&s->segment[0], &s->segment[0].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->segment[1].delays[0], &s->segment[1].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[1].delays[0]));
    g1050_segment_model(&s->segment[1], &s->segment[1].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->core.delays[0], &s->core.delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->core.delays[0]));
    g1050_core_model(&s->core, &s->core.delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->segment[2].delays[0], &s->segment[2].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[2].delays[0]));
    g1050_segment_model(&s->segment[2], &s->segment[2].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->segment[3].delays[0], &s->segment[3].delays[G1050_TICKS_PER_SEC], 2*G1050_TICKS_PER_SEC*sizeof(s->segment[3].delays[0]));
    g1050_segment_model(&s->segment[3], &s->segment[3].delays[2*G1050_TICKS_PER_SEC], G1050_TICKS_PER_SEC);

    memmove(&s->arrival_times_1[0], &s->arrival_times_1[s->packet_rate], 2*s->packet_rate*sizeof(s->arrival_times_1[0]));
    memmove(&s->arrival_times_2[0], &s->arrival_times_2[s->packet_rate], 2*s->packet_rate*sizeof(s->arrival_times_2[0]));
    for (i = 0;  i < s->packet_rate;  i++)
    {
        s->arrival_times_1[2*s->packet_rate + i] = s->base_time + 2.0 + (double) i/(double) s->packet_rate;
        s->arrival_times_2[2*s->packet_rate + i] = 0.0;
    }

    s->segment[0].lost_packets_2 += g1050_segment_delay(&s->segment[0], s->base_time, s->arrival_times_1, s->segment[0].delays, s->packet_rate);
    s->segment[1].lost_packets_2 += g1050_segment_delay(&s->segment[1], s->base_time, s->arrival_times_1, s->segment[1].delays, s->packet_rate);
    s->core.lost_packets_2 += g1050_core_delay(&s->core, s->base_time, s->arrival_times_1, s->core.delays, s->packet_rate);
    s->segment[2].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[2], s->base_time, s->arrival_times_1, s->arrival_times_2, s->segment[2].delays, s->packet_rate);
    s->segment[3].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[3], s->base_time, s->arrival_times_2, s->arrival_times_1, s->segment[3].delays, s->packet_rate);
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(g1050_state_t *) g1050_init(int model,
                                         int speed_pattern,
                                         int packet_size,
                                         int packet_rate)
{
    g1050_state_t *s;
    g1050_constants_t *constants;
    g1050_channel_speeds_t *sp;
    g1050_model_t *mo;
    int i;

    /* If the random generator has not been seeded it might give endless
       zeroes - it depends on the platform. */
    for (i = 0;  i < 10;  i++)
    {
        if (q1050_rand() != 0.0)
            break;
    }
    if (i >= 10)
        q1050_rand_init();
    if ((s = (g1050_state_t *) malloc(sizeof(*s))) == NULL)
        return NULL;
    memset(s, 0, sizeof(*s));

    constants = &g1050_constants[0];
    sp = &g1050_speed_patterns[speed_pattern - 1];
    mo = &g1050_standard_models[model];

    memset(s, 0, sizeof(*s));

    s->packet_rate = packet_rate;
    s->packet_size = packet_size;

    g1050_segment_init(&s->segment[0],
                       G1050_LAN_LINK,
                       &constants->segment[0],
                       &mo->sidea_lan,
                       sp->sidea_lan_bit_rate,
                       sp->sidea_lan_multiple_access,
                       false,
                       packet_size,
                       packet_rate);
    g1050_segment_init(&s->segment[1],
                       G1050_ACCESS_LINK,
                       &constants->segment[1],
                       &mo->sidea_access_link,
                       sp->sidea_access_link_bit_rate_ab,
                       false,
                       sp->sidea_access_link_qos_enabled,
                       packet_size,
                       packet_rate);
    g1050_core_init(&s->core, &mo->core, packet_rate);
    g1050_segment_init(&s->segment[2],
                       G1050_ACCESS_LINK,
                       &constants->segment[2],
                       &mo->sideb_access_link,
                       sp->sideb_access_link_bit_rate_ba,
                       false,
                       sp->sideb_access_link_qos_enabled,
                       packet_size,
                       packet_rate);
    g1050_segment_init(&s->segment[3],
                       G1050_LAN_LINK,
                       &constants->segment[3],
                       &mo->sideb_lan,
                       sp->sideb_lan_bit_rate,
                       sp->sideb_lan_multiple_access,
                       false,
                       packet_size,
                       packet_rate);

    s->base_time = 0.0;
    /* Start with enough of the future modelled to allow for the worst jitter.
       After this we will always keep at least 2 seconds of the future modelled. */
    g1050_segment_model(&s->segment[0], s->segment[0].delays, 3*G1050_TICKS_PER_SEC);
    g1050_segment_model(&s->segment[1], s->segment[1].delays, 3*G1050_TICKS_PER_SEC);
    g1050_core_model(&s->core, s->core.delays, 3*G1050_TICKS_PER_SEC);
    g1050_segment_model(&s->segment[2], s->segment[2].delays, 3*G1050_TICKS_PER_SEC);
    g1050_segment_model(&s->segment[3], s->segment[3].delays, 3*G1050_TICKS_PER_SEC);

    /* Initialise the arrival times to the departure times */
    for (i = 0;  i < 3*s->packet_rate;  i++)
    {
        s->arrival_times_1[i] = s->base_time + (double) i/(double)s->packet_rate;
        s->arrival_times_2[i] = 0.0;
    }

    s->segment[0].lost_packets_2 += g1050_segment_delay(&s->segment[0], s->base_time, s->arrival_times_1, s->segment[0].delays, s->packet_rate);
    s->segment[1].lost_packets_2 += g1050_segment_delay(&s->segment[1], s->base_time, s->arrival_times_1, s->segment[1].delays, s->packet_rate);
    s->core.lost_packets_2 += g1050_core_delay(&s->core, s->base_time, s->arrival_times_1, s->core.delays, s->packet_rate);
    s->segment[2].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[2], s->base_time, s->arrival_times_1, s->arrival_times_2, s->segment[2].delays, s->packet_rate);
    s->segment[3].lost_packets_2 += g1050_segment_delay_preserve_order(&s->segment[3], s->base_time, s->arrival_times_2, s->arrival_times_1, s->segment[3].delays, s->packet_rate);

    s->first = NULL;
    s->last = NULL;
    return s;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g1050_free(g1050_state_t *s)
{
    free(s);
    return 0;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) g1050_dump_parms(int model, int speed_pattern)
{
    g1050_channel_speeds_t *sp;
    g1050_model_t *mo;

    sp = &g1050_speed_patterns[speed_pattern - 1];
    mo = &g1050_standard_models[model];

    printf("Model %d%c\n", speed_pattern, 'A' + model - 1);
    printf("LOO %.6f%% %.6f%% %.6f%%\n", mo->loo[0]*sp->loo/100.0, mo->loo[1]*sp->loo/100.0, mo->loo[2]*sp->loo/100.0);
    printf("Side A LAN %dbps, %.3f%% occupancy, MTU %d, %s MA\n", sp->sidea_lan_bit_rate, mo->sidea_lan.percentage_occupancy, mo->sidea_lan.mtu, (sp->sidea_lan_multiple_access)  ?  ""  :  "no");
    printf("Side A access %dbps, %.3f%% occupancy, MTU %d, %s QoS\n", sp->sidea_access_link_bit_rate_ab, mo->sidea_access_link.percentage_occupancy, mo->sidea_access_link.mtu, (sp->sidea_access_link_qos_enabled)  ?  ""  :  "no");
    printf("Core delay %.4fs (%.4fs), peak jitter %.4fs, prob loss %.4f%%, prob OOS %.4f%%\n", mo->core.base_regional_delay, mo->core.base_intercontinental_delay, mo->core.max_jitter, mo->core.prob_packet_loss, mo->core.prob_oos);
    printf("     Route flap interval %.4fs, delay change %.4fs\n", mo->core.route_flap_interval, mo->core.route_flap_delay);
    printf("     Link failure interval %.4fs, duration %.4fs\n", mo->core.link_failure_interval, mo->core.link_failure_duration);
    printf("Side B access %dbps, %.3f%% occupancy, MTU %d, %s QoS\n", sp->sideb_access_link_bit_rate_ba, mo->sideb_access_link.percentage_occupancy, mo->sideb_access_link.mtu, (sp->sideb_access_link_qos_enabled)  ?  ""  :  "no");
    printf("Side B LAN %dbps, %.3f%% occupancy, MTU %d, %s MA\n", sp->sideb_lan_bit_rate, mo->sideb_lan.percentage_occupancy, mo->sideb_lan.mtu, (sp->sideb_lan_multiple_access)  ?  ""  :  "no");
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g1050_put(g1050_state_t *s, const uint8_t buf[], int len, int seq_no, double departure_time)
{
    g1050_queue_element_t *element;
    g1050_queue_element_t *e;
    double arrival_time;

    while (departure_time >= s->base_time + 1.0)
        g1050_simulate_chunk(s);
    arrival_time = s->arrival_times_1[(int) ((departure_time - s->base_time)*(double) s->packet_rate + 0.5)];
    if (arrival_time < 0)
    {
        /* This packet is lost */
        return 0;
    }
    if ((element = (g1050_queue_element_t *) malloc(sizeof(*element) + len)) == NULL)
        return -1;
    element->next = NULL;
    element->prev = NULL;
    element->seq_no = seq_no;
    element->departure_time = departure_time;
    element->arrival_time = arrival_time;
    element->len = len;
    memcpy(element->pkt, buf, len);
    /* Add it to the queue, in order */
    if (s->last == NULL)
    {
        /* The queue is empty */
        s->first =
        s->last = element;
    }
    else
    {
        for (e = s->last;  e;  e = e->prev)
        {
            if (e->arrival_time <= arrival_time)
                break;
        }
        if (e)
        {
            element->next = e->next;
            element->prev = e;
            e->next = element;
        }
        else
        {
            element->next = s->first;
            s->first = element;
        }
        if (element->next)
            element->next->prev = element;
        else
            s->last = element;
    }
    //printf(">> Seq %d, departs %f, arrives %f\n", seq_no, departure_time, arrival_time);
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(int) g1050_get(g1050_state_t *s, uint8_t buf[], int max_len, double current_time, int *seq_no, double *departure_time, double *arrival_time)
{
    int len;
    g1050_queue_element_t *element;

    element = s->first;
    if (element == NULL)
    {
        if (seq_no)
            *seq_no = -1;
        if (departure_time)
            *departure_time = -1;
        if (arrival_time)
            *arrival_time = -1;
        return -1;
    }
    if (element->arrival_time > current_time)
    {
        if (seq_no)
            *seq_no = element->seq_no;
        if (departure_time)
            *departure_time = element->departure_time;
        if (arrival_time)
            *arrival_time = element->arrival_time;
        return -1;
    }
    /* Return the first packet in the queue */
    len = element->len;
    memcpy(buf, element->pkt, len);
    if (seq_no)
        *seq_no = element->seq_no;
    if (departure_time)
        *departure_time = element->departure_time;
    if (arrival_time)
        *arrival_time = element->arrival_time;
    //printf("<< Seq %d, arrives %f (%f)\n", element->seq_no, element->arrival_time, current_time);

    /* Remove it from the queue */
    if (s->first == s->last)
        s->last = NULL;
    s->first = element->next;
    if (element->next)
        element->next->prev = NULL;
    free(element);
    return len;
}
/*- End of function --------------------------------------------------------*/

SPAN_DECLARE(void) g1050_queue_dump(g1050_state_t *s)
{
    g1050_queue_element_t *e;

    printf("Queue scanned forewards\n");
    for (e = s->first;  e;  e = e->next)
        printf("Seq %5d, arrival %10.4f, len %3d\n", e->seq_no, e->arrival_time, e->len);
    printf("Queue scanned backwards\n");
    for (e = s->last;  e;  e = e->prev)
        printf("Seq %5d, arrival %10.4f, len %3d\n", e->seq_no, e->arrival_time, e->len);
}
/*- End of function --------------------------------------------------------*/
/*- End of file ------------------------------------------------------------*/

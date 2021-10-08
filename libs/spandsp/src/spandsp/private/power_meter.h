/*
 * SpanDSP - a series of DSP components for telephony
 *
 * private/power_meter.h
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2003 Steve Underwood
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

#if !defined(_SPANDSP_PRIVATE_POWER_METER_H_)
#define _SPANDSP_PRIVATE_POWER_METER_H_

/*!
    Power meter descriptor. This defines the working state for a
    single instance of a power measurement device.
*/
struct power_meter_s
{
    /*! The shift factor, which controls the damping of the power meter. */
    int shift;

    /*! The current power reading. */
    int32_t reading;
};

struct power_surge_detector_state_s
{
    power_meter_t short_term;
    power_meter_t medium_term;
    int signal_present;
    int32_t surge;
    int32_t sag;
    int32_t min;
};

#endif
/*- End of file ------------------------------------------------------------*/

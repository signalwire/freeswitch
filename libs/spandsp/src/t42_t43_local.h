/*
 * SpanDSP - a series of DSP components for telephony
 *
 * t42_t43_local.h - definitions for T.42 and T.43 shared processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2011 Steve Underwood
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

#if !defined(_T42_T43_LOCAL_H_)
#define _T42_T43_LOCAL_H_

#if defined(__cplusplus)
extern "C"
{
#endif

int set_illuminant_from_code(logging_state_t *logging, lab_params_t *s, const uint8_t code[4]);

void set_gamut_from_code(logging_state_t *logging, lab_params_t *s, const uint8_t code[12]);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

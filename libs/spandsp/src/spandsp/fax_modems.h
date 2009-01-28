/*
 * SpanDSP - a series of DSP components for telephony
 *
 * fax_modems.h - definitions for the analogue modem set for fax processing
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2008 Steve Underwood
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
 *
 * $Id: fax_modems.h,v 1.6 2008/10/13 13:14:00 steveu Exp $
 */

/*! \file */

#if !defined(_SPANDSP_FAX_MODEMS_H_)
#define _SPANDSP_FAX_MODEMS_H_

/*!
    The set of modems needed for FAX, plus the auxilliary stuff, like tone generation.
*/
typedef struct fax_modems_state_s fax_modems_state_t;

#if defined(__cplusplus)
extern "C"
{
#endif

/* N.B. the following are currently a work in progress */
int fax_modems_v17_v21_rx(void *user_data, const int16_t amp[], int len);
int fax_modems_v27ter_v21_rx(void *user_data, const int16_t amp[], int len);
int fax_modems_v29_v21_rx(void *user_data, const int16_t amp[], int len);
fax_modems_state_t *fax_modems_init(fax_modems_state_t *s, void *user_data);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * constel.h - Display QAM constellations, using the FLTK toolkit.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2004 Steve Underwood
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
 * $Id: modem_monitor.h,v 1.16 2008/09/03 13:41:42 steveu Exp $
 */

/*! \page constel_page Modem performance monitoring
\section constel_page_sec_1 What does it do?
This code controls a GUI window, which provides monitoring of the internal status
of a modem. It shows, graphically:

    - the constellation, for PSK, QAM and other similar modulations.
    - the equalizer status, for modems with adaptive equalizers.
    - the carrier frequency.
    - the symbol timing correction.

\section constel_page_sec_2 How does it work?
This code uses the FLTK cross platform GUI toolkit. It works on X11 and Windows platforms.
In addition to the basic FLTK toolkit, fltk_cartesian is also required.
*/

#if !defined(_MODEM_MONITOR_H_)
#define _MODEM_MONITOR_H_

struct qam_monitor_s;

typedef struct qam_monitor_s qam_monitor_t;

#if defined(__cplusplus)
extern "C"
{
#endif

qam_monitor_t *qam_monitor_init(float constel_width, const char *tag);
int qam_monitor_clear_constel(qam_monitor_t *s);
int qam_monitor_update_constel(qam_monitor_t *s, const complexf_t *pt);
int qam_monitor_update_equalizer(qam_monitor_t *s, const complexf_t *coeffs, int len);
int qam_monitor_update_int_equalizer(qam_monitor_t *s, const complexi16_t *coeffs, int len);
int qam_monitor_update_symbol_tracking(qam_monitor_t *s, float total_correction);
int qam_monitor_update_carrier_tracking(qam_monitor_t *s, float carrier);
int qam_monitor_update_audio_level(qam_monitor_t *s, const int16_t amp[], int len);
void qam_wait_to_end(qam_monitor_t *s);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

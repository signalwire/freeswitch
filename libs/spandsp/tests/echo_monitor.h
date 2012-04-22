/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo_monitor.h - Display echo canceller status, using the FLTK toolkit.
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
 */

/*! \page echo_monitor_page Echo canceller performance monitoring
\section echo_monitor_page_sec_1 What does it do?
This code controls a GUI window, which provides monitoring of the internal status
of a time domain echo canceller. It shows, graphically:

    - the spectrum of the received signal.
    - the line model in use (when a known line model is being used).
    - the adapted coefficients of the canceller.

\section echo_monitor_page_sec_2 How does it work?
This code uses the FLTK cross platform GUI toolkit. It works on X11 and Windows platforms.
In addition to the basic FLTK toolkit, fltk_cartesian is also required.
*/

#if !defined(_ECHO_MONITOR_H_)
#define _ECHO_MONITOR_H_

#if defined(__cplusplus)
extern "C"
{
#endif

int start_echo_can_monitor(int len);
int echo_can_monitor_can_update(const int16_t *coeffs, int len);
int echo_can_monitor_line_model_update(const int32_t *coeffs, int len);
int echo_can_monitor_line_spectrum_update(const int16_t amp[], int len);
void echo_can_monitor_wait_to_end(void);
void echo_can_monitor_update_display(void);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

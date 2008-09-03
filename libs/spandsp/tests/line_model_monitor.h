/*
 * SpanDSP - a series of DSP components for telephony
 *
 * line_model_monitor.h - Model activity in a telephone line model, using the FLTK toolkit.
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
 * $Id: line_model_monitor.h,v 1.7 2008/04/26 13:39:17 steveu Exp $
 */

/*! \page line_model_monitor_page Telephone line model monitoring
\section line_model_monitor_page_sec_1 What does it do?
This code controls a GUI window, which provides monitoring of the internal status
of a telephone line modem. It shows, graphically:

    - the spectrum of the received signal.
    - the line model in use (when a known line model is being used).
    - the adapted coefficients of the canceller.

\section line_model_monitor_page_sec_2 How does it work?
This code uses the FLTK cross platform GUI toolkit. It works on X11 and Windows platforms.
In addition to the basic FLTK toolkit, fltk_cartesian is also required.
*/

#if !defined(_LINE_MODEL_MONITOR_H_)
#define _LINE_MODEL_MONITOR_H_

#if defined(__cplusplus)
extern "C"
{
#endif

int start_line_model_monitor(int len);
int line_model_monitor_can_update(const float *coeffs, int len);
int line_model_monitor_line_model_update(const float *coeffs, int len);
int line_model_monitor_line_spectrum_update(const int16_t amp[], int len);
void line_model_monitor_wait_to_end(void);
void line_model_monitor_update_display(void);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

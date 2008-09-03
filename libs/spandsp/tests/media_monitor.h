/*
 * SpanDSP - a series of DSP components for telephony
 *
 * media_monitor.h - Display IP streaming media status, using the FLTK toolkit.
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
 *
 * $Id: media_monitor.h,v 1.6 2008/04/26 13:39:17 steveu Exp $
 */

/*! \page media_monitor_page IP streaming media performance monitoring
\section media_monitor_page_sec_1 What does it do?
This code controls a GUI window, which provides monitoring of the status
of an IP media stream. It shows, graphically:

\section media_monitor_page_sec_2 How does it work?
This code uses the FLTK cross platform GUI toolkit. It works on X11 and Windows platforms.
In addition to the basic FLTK toolkit, fltk_cartesian is also required.
*/

#if !defined(_MEDIA_MONITOR_H_)
#define _MEDIA_MONITOR_H_

#if defined(__cplusplus)
extern "C"
{
#endif

int start_media_monitor(void);
void media_monitor_rx(int seq_no, double departure_time, double arrival_time);
void media_monitor_wait_to_end(void);
void media_monitor_update_display(void);

#if defined(__cplusplus)
}
#endif

#endif
/*- End of file ------------------------------------------------------------*/

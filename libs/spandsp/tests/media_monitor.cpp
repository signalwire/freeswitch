/*
 * SpanDSP - a series of DSP components for telephony
 *
 * media_monitor.cpp - Display IP streaming media status, using the FLTK toolkit.
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
 * $Id: media_monitor.cpp,v 1.4 2008/05/27 15:08:21 steveu Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_FL_FL_H)  &&  defined(HAVE_FL_FL_CARTESIAN_H)

#define __STDC_LIMIT_MACROS

#include <inttypes.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include <FL/Fl.H>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Cartesian.H>
#include <FL/fl_draw.H>

#include "spandsp.h"
#include "media_monitor.h"

Fl_Double_Window *w;

Fl_Group *c_right;
Fl_Group *c_sent;
Fl_Group *c_received;

Ca_Canvas *canvas_sent;
Ca_X_Axis *sent_x;
Ca_Y_Axis *sent_y;
Ca_Line *sent_re = NULL;
double sent_re_plot[1000];
double sent_re_plot_min;
double sent_re_plot_max;

Ca_Canvas *canvas_received;
Ca_X_Axis *received_x;
Ca_Y_Axis *received_y;
Ca_Line *received_delays = NULL;
double received_delays_plot[4000];
double received_delays_plot_max;
int min_diff;
int max_diff;

int highest_seq_no_seen = -1;

static int skip = 0;

int in_ptr;

void media_monitor_rx(int seq_no, double departure_time, double arrival_time)
{
    double fdiff;
    int diff;
    int i;

    if (received_delays)
        delete received_delays;

    canvas_received->current(canvas_received);
    fdiff = (arrival_time - departure_time)*1000.0;
    diff = (int) fdiff;
    if (diff < 0)
        diff = 0;
    else if (diff > 1999)
        diff = 1999;
    received_delays_plot[2*diff + 1]++;
    if (received_delays_plot[2*diff + 1] > received_delays_plot_max)
    {
        received_delays_plot_max = received_delays_plot[2*diff + 1];
        received_y->maximum(received_delays_plot_max);
    }
    if (diff > max_diff)
    {
        max_diff = diff;
        received_x->maximum((double) max_diff);
    }
    if (diff < min_diff)
    {
        min_diff = diff - 1;
        received_x->minimum((double) min_diff);
    }

    received_delays = new Ca_Line(2000, received_delays_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    
    if (sent_re)
        delete sent_re;

    canvas_sent->current(canvas_sent);

    if (seq_no > highest_seq_no_seen + 1)
    {
        for (i = highest_seq_no_seen + 1;  i < seq_no;  i++)
            sent_re_plot[2*(i%500) + 1] = 0.0;
    }
    sent_re_plot[2*(seq_no%500) + 1] = fdiff;
    
    if (fdiff > sent_re_plot_max)
    {
        sent_re_plot_max = fdiff;
        sent_y->maximum(sent_re_plot_max);
    }
    if (fdiff < sent_re_plot_min)
    {
        sent_re_plot_min = fdiff - 1.0;
        sent_y->minimum(sent_re_plot_min);
    }
    sent_re = new Ca_Line(500, sent_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);

    if (seq_no > highest_seq_no_seen)
        highest_seq_no_seen = seq_no;

    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
}
/*- End of function --------------------------------------------------------*/

int start_media_monitor(void)
{
    char buf[132 + 1];
    float x;
    float y;
    int i;
    int len;
    
    len = 128;

    w = new Fl_Double_Window(465, 400, "IP streaming media monitor");

    c_right = new Fl_Group(0, 0, 465, 405);

    c_sent = new Fl_Group(0, 0, 465, 200);
    c_sent->box(FL_DOWN_BOX);
    c_sent->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_sent->current();

    canvas_sent = new Ca_Canvas(110, 35, 300, 100, "Packet delays");
    canvas_sent->box(FL_PLASTIC_DOWN_BOX);
    canvas_sent->color(7);
    canvas_sent->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_sent);
    canvas_sent->border(15);

    sent_x = new Ca_X_Axis(115, 135, 290, 30, "Packet");
    sent_x->align(FL_ALIGN_BOTTOM);
    sent_x->minimum(0.0);
    sent_x->maximum(500.0);
    sent_x->label_format("%g");
    sent_x->minor_grid_color(fl_gray_ramp(20));
    sent_x->major_grid_color(fl_gray_ramp(15));
    sent_x->label_grid_color(fl_gray_ramp(10));
    sent_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    sent_x->minor_grid_style(FL_DOT);
    sent_x->major_step(5);
    sent_x->label_step(1);
    sent_x->axis_align(CA_BOTTOM | CA_LINE);
    sent_x->axis_color(FL_BLACK);
    sent_x->current();

    sent_y = new Ca_Y_Axis(60, 40, 50, 90, "Delay\n(ms)");
    sent_y->align(FL_ALIGN_LEFT);
    sent_y->minimum(0.0);
    sent_y->maximum(2000.0);
    sent_y->minor_grid_color(fl_gray_ramp(20));
    sent_y->major_grid_color(fl_gray_ramp(15));
    sent_y->label_grid_color(fl_gray_ramp(10));
    sent_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    sent_y->minor_grid_style(FL_DOT);
    sent_y->major_step(5);
    sent_y->label_step(1);
    sent_y->axis_color(FL_BLACK);
    sent_y->current();

    c_sent->end();

    c_received = new Fl_Group(0, 200, 465, 200);
    c_received->box(FL_DOWN_BOX);
    c_received->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_received->current();

    canvas_received = new Ca_Canvas(110, 235, 300, 100, "Delay spread");
    canvas_received->box(FL_PLASTIC_DOWN_BOX);
    canvas_received->color(7);
    canvas_received->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_received);
    canvas_received->border(15);

    received_x = new Ca_X_Axis(115, 335, 290, 30, "Delay (ms)");
    received_x->align(FL_ALIGN_BOTTOM);
    received_x->minimum(0.0);
    received_x->maximum(2000.0);
    received_x->label_format("%g");
    received_x->minor_grid_color(fl_gray_ramp(20));
    received_x->major_grid_color(fl_gray_ramp(15));
    received_x->label_grid_color(fl_gray_ramp(10));
    received_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    received_x->minor_grid_style(FL_DOT);
    received_x->major_step(5);
    received_x->label_step(1);
    received_x->axis_align(CA_BOTTOM | CA_LINE);
    received_x->axis_color(FL_BLACK);
    received_x->current();

    received_y = new Ca_Y_Axis(60, 240, 50, 90, "Freq");
    received_y->align(FL_ALIGN_LEFT);
    received_y->minimum(0.0);
    received_y->maximum(50.0);
    received_y->minor_grid_color(fl_gray_ramp(20));
    received_y->major_grid_color(fl_gray_ramp(15));
    received_y->label_grid_color(fl_gray_ramp(10));
    received_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    received_y->minor_grid_style(FL_DOT);
    received_y->major_step(5);
    received_y->label_step(1);
    received_y->axis_color(FL_BLACK);
    received_y->current();

    for (i = 0;  i < 2000;  i++)
        received_delays_plot[2*i] = i;
    received_delays_plot_max = 0.0;
    min_diff = 2000;
    max_diff = 0;
    
    for (i = 0;  i < 500;  i++)
        sent_re_plot[2*i] = i;
    sent_re_plot_min = 99999.0;
    sent_re_plot_max = 0.0;

    c_received->end();

    c_right->end();

    Fl_Group::current()->resizable(c_right);
    w->end();
    w->show();

    in_ptr = 0;

    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

void media_monitor_wait_to_end(void) 
{
    fd_set rfds;
    int res;
    struct timeval tv;

    fprintf(stderr, "Processing complete.  Press the <enter> key to end\n");
    do
    {
        usleep(100000);
        Fl::check();
        FD_ZERO(&rfds);
        FD_SET(0, &rfds);
        tv.tv_usec = 100000;
        tv.tv_sec = 0;
        res = select(1, &rfds, NULL, NULL, &tv);
    }
    while (res <= 0);
}
/*- End of function --------------------------------------------------------*/

void media_monitor_update_display(void) 
{
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
    Fl::check();
}
/*- End of function --------------------------------------------------------*/
#endif
/*- End of file ------------------------------------------------------------*/

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

struct line_model_monitor_s
{
    Fl_Double_Window *w;

    Fl_Group *c_right;
    Fl_Group *c_sent;
    Fl_Group *c_received;

    Ca_Canvas *canvas_sent;
    Ca_X_Axis *sent_x;
    Ca_Y_Axis *sent_y;
    Ca_Line *sent_re;
    double sent_re_plot[1000];
    double sent_re_plot_min;
    double sent_re_plot_max;

    Ca_Canvas *canvas_received;
    Ca_X_Axis *received_x;
    Ca_Y_Axis *received_y;
    Ca_Line *received_delays;
    double received_delays_plot[4000];
    double received_delays_plot_max;
    int min_diff;
    int max_diff;

    int highest_seq_no_seen;
};

static int skip = 0;
static struct line_model_monitor_s media;
static struct line_model_monitor_s *s = &media;

void media_monitor_rx(int seq_no, double departure_time, double arrival_time)
{
    double fdiff;
    int diff;
    int i;

    if (s->received_delays)
        delete s->received_delays;

    s->canvas_received->current(s->canvas_received);
    fdiff = (arrival_time - departure_time)*1000.0;
    diff = (int) fdiff;
    if (diff < 0)
        diff = 0;
    else if (diff > 1999)
        diff = 1999;
    s->received_delays_plot[2*diff + 1]++;
    if (s->received_delays_plot[2*diff + 1] > s->received_delays_plot_max)
    {
        s->received_delays_plot_max = s->received_delays_plot[2*diff + 1];
        s->received_y->maximum(s->received_delays_plot_max);
    }
    if (diff > s->max_diff)
    {
        s->max_diff = diff;
        s->received_x->maximum((double) s->max_diff);
    }
    if (diff < s->min_diff)
    {
        s->min_diff = diff - 1;
        s->received_x->minimum((double) s->min_diff);
    }

    s->received_delays = new Ca_Line(2000, s->received_delays_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    
    if (s->sent_re)
        delete s->sent_re;

    s->canvas_sent->current(s->canvas_sent);

    if (seq_no > s->highest_seq_no_seen + 1)
    {
        for (i = s->highest_seq_no_seen + 1;  i < seq_no;  i++)
            s->sent_re_plot[2*(i%500) + 1] = 0.0;
    }
    s->sent_re_plot[2*(seq_no%500) + 1] = fdiff;
    
    if (fdiff > s->sent_re_plot_max)
    {
        s->sent_re_plot_max = fdiff;
        s->sent_y->maximum(s->sent_re_plot_max);
    }
    if (fdiff < s->sent_re_plot_min)
    {
        s->sent_re_plot_min = fdiff - 1.0;
        s->sent_y->minimum(s->sent_re_plot_min);
    }
    s->sent_re = new Ca_Line(500, s->sent_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);

    if (seq_no > s->highest_seq_no_seen)
        s->highest_seq_no_seen = seq_no;

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

    s->w = new Fl_Double_Window(465, 400, "IP streaming media monitor");

    s->c_right = new Fl_Group(0, 0, 465, 405);

    s->c_sent = new Fl_Group(0, 0, 465, 200);
    s->c_sent->box(FL_DOWN_BOX);
    s->c_sent->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    s->c_sent->current();

    s->canvas_sent = new Ca_Canvas(110, 35, 300, 100, "Packet delays");
    s->canvas_sent->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_sent->color(7);
    s->canvas_sent->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(s->canvas_sent);
    s->canvas_sent->border(15);

    s->sent_x = new Ca_X_Axis(115, 135, 290, 30, "Packet");
    s->sent_x->align(FL_ALIGN_BOTTOM);
    s->sent_x->minimum(0.0);
    s->sent_x->maximum(500.0);
    s->sent_x->label_format("%g");
    s->sent_x->minor_grid_color(fl_gray_ramp(20));
    s->sent_x->major_grid_color(fl_gray_ramp(15));
    s->sent_x->label_grid_color(fl_gray_ramp(10));
    s->sent_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->sent_x->minor_grid_style(FL_DOT);
    s->sent_x->major_step(5);
    s->sent_x->label_step(1);
    s->sent_x->axis_align(CA_BOTTOM | CA_LINE);
    s->sent_x->axis_color(FL_BLACK);
    s->sent_x->current();

    s->sent_y = new Ca_Y_Axis(60, 40, 50, 90, "Delay\n(ms)");
    s->sent_y->align(FL_ALIGN_LEFT);
    s->sent_y->minimum(0.0);
    s->sent_y->maximum(2000.0);
    s->sent_y->minor_grid_color(fl_gray_ramp(20));
    s->sent_y->major_grid_color(fl_gray_ramp(15));
    s->sent_y->label_grid_color(fl_gray_ramp(10));
    s->sent_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->sent_y->minor_grid_style(FL_DOT);
    s->sent_y->major_step(5);
    s->sent_y->label_step(1);
    s->sent_y->axis_color(FL_BLACK);
    s->sent_y->current();

    s->c_sent->end();

    s->c_received = new Fl_Group(0, 200, 465, 200);
    s->c_received->box(FL_DOWN_BOX);
    s->c_received->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    s->c_received->current();

    s->canvas_received = new Ca_Canvas(110, 235, 300, 100, "Delay spread");
    s->canvas_received->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_received->color(7);
    s->canvas_received->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(s->canvas_received);
    s->canvas_received->border(15);

    s->received_x = new Ca_X_Axis(115, 335, 290, 30, "Delay (ms)");
    s->received_x->align(FL_ALIGN_BOTTOM);
    s->received_x->minimum(0.0);
    s->received_x->maximum(2000.0);
    s->received_x->label_format("%g");
    s->received_x->minor_grid_color(fl_gray_ramp(20));
    s->received_x->major_grid_color(fl_gray_ramp(15));
    s->received_x->label_grid_color(fl_gray_ramp(10));
    s->received_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->received_x->minor_grid_style(FL_DOT);
    s->received_x->major_step(5);
    s->received_x->label_step(1);
    s->received_x->axis_align(CA_BOTTOM | CA_LINE);
    s->received_x->axis_color(FL_BLACK);
    s->received_x->current();

    s->received_y = new Ca_Y_Axis(60, 240, 50, 90, "Freq");
    s->received_y->align(FL_ALIGN_LEFT);
    s->received_y->minimum(0.0);
    s->received_y->maximum(50.0);
    s->received_y->minor_grid_color(fl_gray_ramp(20));
    s->received_y->major_grid_color(fl_gray_ramp(15));
    s->received_y->label_grid_color(fl_gray_ramp(10));
    s->received_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->received_y->minor_grid_style(FL_DOT);
    s->received_y->major_step(5);
    s->received_y->label_step(1);
    s->received_y->axis_color(FL_BLACK);
    s->received_y->current();

    for (i = 0;  i < 2000;  i++)
        s->received_delays_plot[2*i] = i;
    s->received_delays_plot_max = 0.0;
    s->min_diff = 2000;
    s->max_diff = 0;
    
    s->received_delays = NULL;
    s->highest_seq_no_seen = -1;

    for (i = 0;  i < 500;  i++)
        s->sent_re_plot[2*i] = i;
    s->sent_re_plot_min = 99999.0;
    s->sent_re_plot_max = 0.0;
    s->sent_re = NULL;

    s->c_received->end();

    s->c_right->end();

    Fl_Group::current()->resizable(s->c_right);
    s->w->end();
    s->w->show();

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

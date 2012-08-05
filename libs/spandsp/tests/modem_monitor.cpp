/*
 * SpanDSP - a series of DSP components for telephony
 *
 * modem_monitor.cpp - Display QAM constellations, using the FLTK toolkit.
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
#include <FL/Fl_Audio_Meter.H>
#include <FL/Fl_Output.H>
#include <FL/fl_draw.H>

#include "spandsp.h"

#define SYMBOL_TRACKER_POINTS   12000
#define CARRIER_TRACKER_POINTS  12000

struct qam_monitor_s
{
    float constel_scaling;

    Fl_Double_Window *w;
    Fl_Group *c_const;
    Fl_Group *c_right;
    Fl_Group *c_eq;
    Fl_Group *c_symbol_track;
    
    /* Constellation stuff */
    Ca_Canvas *canvas_const;
    Ca_X_Axis *sig_i;
    Ca_Y_Axis *sig_q;

    Ca_Point *constel_point[100000];
    int constel_window;
    int next_constel_point;
    int skip;

    /* Equalizer stuff */
    Ca_Canvas *canvas_eq;

    Ca_X_Axis *eq_x;
    Ca_Y_Axis *eq_y;

    Ca_Line *eq_re;
    Ca_Line *eq_im;

    double eq_re_plot[200];
    double eq_im_plot[200];

    /* Carrier and symbol tracking stuff */
    Ca_Canvas *canvas_track;

    Ca_X_Axis *track_x;
    Ca_Y_Axis *symbol_track_y;
    Ca_Y_Axis *carrier_y;

    Ca_Line *symbol_track;
    Ca_Line *carrier;

    float symbol_tracker[SYMBOL_TRACKER_POINTS];
    double symbol_track_plot[SYMBOL_TRACKER_POINTS*2];
    int symbol_track_points;
    int symbol_track_ptr;
    int symbol_track_window;

    float carrier_tracker[CARRIER_TRACKER_POINTS];
    double carrier_plot[CARRIER_TRACKER_POINTS*2];
    int carrier_points;
    int carrier_ptr;
    int carrier_window;

    /* Audio meter stuff */
    Fl_Audio_Meter *audio_meter;
    Fl_Output *audio_level;
    int32_t power_reading;
};

#include "modem_monitor.h"

int qam_monitor_clear_constel(qam_monitor_t *s)
{
    int i;

    s->canvas_const->current(s->canvas_const);
    for (i = 0;  i < s->constel_window;  i++)
    {
        if (s->constel_point[i])
        {
            delete s->constel_point[i];
            s->constel_point[i] = NULL;
        }
    }
    s->next_constel_point = 0;
    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

int qam_monitor_update_constel(qam_monitor_t *s, const complexf_t *pt)
{
    int i;

    s->canvas_const->current(s->canvas_const);
    if (s->constel_point[s->next_constel_point])
        delete s->constel_point[s->next_constel_point];
    s->constel_point[s->next_constel_point++] = new Ca_Point(pt->re, pt->im, FL_BLACK);
    if (s->next_constel_point >= s->constel_window)
        s->next_constel_point = 0;
    if (++s->skip >= 100)
    {
        s->skip = 0;
        Fl::check();
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int qam_monitor_update_equalizer(qam_monitor_t *s, const complexf_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    /* Protect against screwy values */
    for (i = 0;  i < len;  i++)
    {
        if (isnan(coeffs[i].re)  ||  isinf(coeffs[i].re))
            break;
        if (isnan(coeffs[i].im)  ||  isinf(coeffs[i].im))
            break;
        if (coeffs[i].re < -20.0f  ||  coeffs[i].re > 20.0f)
            break;
        if (coeffs[i].im < -20.0f  ||  coeffs[i].im > 20.0f)
            break;
    }
    if (i != len)
        return -1;

    if (s->eq_re)
        delete s->eq_re;
    if (s->eq_im)
        delete s->eq_im;

    s->canvas_eq->current(s->canvas_eq);
    i = 0;
    min = coeffs[i].re;
    if (min > coeffs[i].im)
        min = coeffs[i].im;
    max = coeffs[i].re;
    if (max < coeffs[i].im)
        max = coeffs[i].im;
    for (i = 0;  i < len;  i++)
    {
        s->eq_re_plot[2*i] = (i - len/2)/2.0;
        s->eq_re_plot[2*i + 1] = coeffs[i].re*s->constel_scaling;
        if (min > coeffs[i].re)
            min = coeffs[i].re;
        if (max < coeffs[i].re)
            max = coeffs[i].re;

        s->eq_im_plot[2*i] = (i - len/2)/2.0;
        s->eq_im_plot[2*i + 1] = coeffs[i].im*s->constel_scaling;
        if (min > coeffs[i].im)
            min = coeffs[i].im;
        if (max < coeffs[i].im)
            max = coeffs[i].im;
    }
    
    s->eq_x->minimum(-len/4.0);
    s->eq_x->maximum(len/4.0);
    s->eq_y->maximum((max == min)  ?  max + 0.2  :  max);
    s->eq_y->minimum(min);
    s->eq_re = new Ca_Line(len, s->eq_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    s->eq_im = new Ca_Line(len, s->eq_im_plot, 0, 0, FL_RED, CA_NO_POINT);
    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

int qam_monitor_update_int_equalizer(qam_monitor_t *s, const complexi16_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (s->eq_re)
        delete s->eq_re;
    if (s->eq_im)
        delete s->eq_im;

    s->canvas_eq->current(s->canvas_eq);
    i = 0;
    min = coeffs[i].re;
    if (min > coeffs[i].im)
        min = coeffs[i].im;
    max = coeffs[i].re;
    if (max < coeffs[i].im)
        max = coeffs[i].im;
    for (i = 0;  i < len;  i++)
    {
        if (min > coeffs[i].re)
            min = coeffs[i].re;
        if (max < coeffs[i].re)
            max = coeffs[i].re;
        s->eq_re_plot[2*i] = (i - len/2)/2.0f;
        s->eq_re_plot[2*i + 1] = coeffs[i].re*s->constel_scaling;

        if (min > coeffs[i].im)
            min = coeffs[i].im;
        if (max < coeffs[i].im)
            max = coeffs[i].im;
        s->eq_im_plot[2*i] = (i - len/2)/2.0f;
        s->eq_im_plot[2*i + 1] = coeffs[i].im*s->constel_scaling;
    }
    min *= s->constel_scaling;
    max *= s->constel_scaling;

    s->eq_x->minimum(-len/4.0);
    s->eq_x->maximum(len/4.0);
    s->eq_y->maximum((max == min)  ?  max + 0.2  :  max);
    s->eq_y->minimum(min);
    s->eq_re = new Ca_Line(len, s->eq_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    s->eq_im = new Ca_Line(len, s->eq_im_plot, 0, 0, FL_RED, CA_NO_POINT);
    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

int qam_monitor_update_symbol_tracking(qam_monitor_t *s, float total_correction)
{
    int i;
    int j;
    float min;
    float max;

    s->symbol_tracker[s->symbol_track_ptr++] = total_correction;
    if (s->symbol_track_points < SYMBOL_TRACKER_POINTS)
        s->symbol_track_points++;
    if (s->symbol_track_ptr >= SYMBOL_TRACKER_POINTS)
        s->symbol_track_ptr = 0;

    s->canvas_track->current(s->canvas_track);
    if (s->symbol_track)
        delete s->symbol_track;
    s->track_x->current();
    s->symbol_track_y->current();

    min =
    max = s->symbol_tracker[0];
    for (i = s->symbol_track_ptr, j = 0;  i < s->symbol_track_points;  i++, j++)
    {
        if (min > s->symbol_tracker[i])
            min = s->symbol_tracker[i];
        if (max < s->symbol_tracker[i])
            max = s->symbol_tracker[i];
        s->symbol_track_plot[2*j] = j;
        s->symbol_track_plot[2*j + 1] = s->symbol_tracker[i];
    }
    for (i = 0;  i < s->symbol_track_ptr;  i++, j++)
    {
        if (min > s->symbol_tracker[i])
            min = s->symbol_tracker[i];
        if (max < s->symbol_tracker[i])
            max = s->symbol_tracker[i];
        s->symbol_track_plot[2*j] = j;
        s->symbol_track_plot[2*j + 1] = s->symbol_tracker[i];
    }
    s->symbol_track_y->maximum((fabs(max - min) < 0.05)  ?  max + 0.05  :  max);
    s->symbol_track_y->minimum(min);

    s->symbol_track = new Ca_Line(s->symbol_track_points, s->symbol_track_plot, 0, 0, FL_RED, CA_NO_POINT);
    //Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

int qam_monitor_update_audio_level(qam_monitor_t *s, const int16_t amp[], int len)
{
    int i;
    char buf[11];
    double val;
    
    for (i = 0;  i < len;  i++)
    {
        s->audio_meter->sample(amp[i]/32768.0);
        s->power_reading += ((amp[i]*amp[i] - s->power_reading) >> 10);
    }
    val = 10.0*log10((double) s->power_reading/(32767.0*32767.0) + 1.0e-10) + 3.14 + 3.02;

    snprintf(buf, sizeof(buf), "%5.1fdBm0", val);
    s->audio_level->value(buf);
    return 0;
}
/*- End of function --------------------------------------------------------*/

int qam_monitor_update_carrier_tracking(qam_monitor_t *s, float carrier_freq)
{
    int i;
    int j;
    float min;
    float max;

    s->carrier_tracker[s->carrier_ptr++] = carrier_freq;
    if (s->carrier_points < CARRIER_TRACKER_POINTS)
        s->carrier_points++;
    if (s->carrier_ptr >= CARRIER_TRACKER_POINTS)
        s->carrier_ptr = 0;

    s->canvas_track->current(s->canvas_track);
    if (s->carrier)
        delete s->carrier;
    s->track_x->current();
    s->carrier_y->current();

    min =
    max = s->carrier_tracker[0];
    for (i = s->carrier_ptr, j = 0;  i < s->carrier_points;  i++, j++)
    {
        s->carrier_plot[2*j] = j;
        s->carrier_plot[2*j + 1] = s->carrier_tracker[i];
        if (min > s->carrier_tracker[i])
            min = s->carrier_tracker[i];
        if (max < s->carrier_tracker[i])
            max = s->carrier_tracker[i];
    }
    for (i = 0;  i < s->carrier_ptr;  i++, j++)
    {
        s->carrier_plot[2*j] = j;
        s->carrier_plot[2*j + 1] = s->carrier_tracker[i];
        if (min > s->carrier_tracker[i])
            min = s->carrier_tracker[i];
        if (max < s->carrier_tracker[i])
            max = s->carrier_tracker[i];
    }

    s->carrier_y->maximum((fabs(max - min) < 0.05)  ?  max + 0.05  :  max);
    s->carrier_y->minimum(min);

    s->carrier = new Ca_Line(s->carrier_points, s->carrier_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    //Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

qam_monitor_t *qam_monitor_init(float constel_width, float constel_scaling, const char *tag)
{
    char buf[132 + 1];
    float x;
    float y;
    qam_monitor_t *s;
    
    if ((s = (qam_monitor_t *) malloc(sizeof(*s))) == NULL)
        return NULL;
    
    s->w = new Fl_Double_Window(905, 400, (tag)  ?  tag  :  "QAM monitor");

    s->constel_scaling = 1.0/constel_scaling;

    s->c_const = new Fl_Group(0, 0, 380, 400);
    s->c_const->box(FL_DOWN_BOX);
    s->c_const->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);

    s->canvas_const = new Ca_Canvas(60, 30, 300, 300, "Constellation");
    s->canvas_const->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_const->color(7);
    s->canvas_const->align(FL_ALIGN_TOP);
    s->canvas_const->border(15);

    s->sig_i = new Ca_X_Axis(65, 330, 290, 30, "I");
    s->sig_i->align(FL_ALIGN_BOTTOM);
    s->sig_i->minimum(-constel_width);
    s->sig_i->maximum(constel_width);
    s->sig_i->label_format("%g");
    s->sig_i->minor_grid_color(fl_gray_ramp(20));
    s->sig_i->major_grid_color(fl_gray_ramp(15));
    s->sig_i->label_grid_color(fl_gray_ramp(10));
    s->sig_i->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->sig_i->minor_grid_style(FL_DOT);
    s->sig_i->major_step(5);
    s->sig_i->label_step(1);
    s->sig_i->axis_color(FL_BLACK);
    s->sig_i->axis_align(CA_BOTTOM | CA_LINE);

    s->sig_q = new Ca_Y_Axis(20, 35, 40, 290, "Q");
    s->sig_q->align(FL_ALIGN_LEFT);
    s->sig_q->minimum(-constel_width);
    s->sig_q->maximum(constel_width);
    s->sig_q->minor_grid_color(fl_gray_ramp(20));
    s->sig_q->major_grid_color(fl_gray_ramp(15));
    s->sig_q->label_grid_color(fl_gray_ramp(10));
    //s->sig_q->grid_visible(CA_MINOR_TICK | CA_MAJOR_TICK | CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->sig_q->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->sig_q->minor_grid_style(FL_DOT);
    s->sig_q->major_step(5);
    s->sig_q->label_step(1);
    s->sig_q->axis_color(FL_BLACK);

    s->sig_q->current();

    s->c_const->end();

    s->c_right = new Fl_Group(440, 0, 465, 405);

    s->c_eq = new Fl_Group(380, 0, 265, 200);
    s->c_eq->box(FL_DOWN_BOX);
    s->c_eq->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    s->c_eq->current();
    s->canvas_eq = new Ca_Canvas(460, 35, 150, 100, "Equalizer");
    s->canvas_eq->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_eq->color(7);
    s->canvas_eq->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(s->canvas_eq);
    s->canvas_eq->border(15);

    s->eq_x = new Ca_X_Axis(465, 135, 140, 30, "Symbol");
    s->eq_x->align(FL_ALIGN_BOTTOM);
    s->eq_x->minimum(-8.0);
    s->eq_x->maximum(8.0);
    s->eq_x->label_format("%g");
    s->eq_x->minor_grid_color(fl_gray_ramp(20));
    s->eq_x->major_grid_color(fl_gray_ramp(15));
    s->eq_x->label_grid_color(fl_gray_ramp(10));
    s->eq_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->eq_x->minor_grid_style(FL_DOT);
    s->eq_x->major_step(5);
    s->eq_x->label_step(1);
    s->eq_x->axis_align(CA_BOTTOM | CA_LINE);
    s->eq_x->axis_color(FL_BLACK);
    s->eq_x->current();

    s->eq_y = new Ca_Y_Axis(420, 40, 40, 90, "Amp");
    s->eq_y->align(FL_ALIGN_LEFT);
    s->eq_y->minimum(-0.1);
    s->eq_y->maximum(0.1);
    s->eq_y->minor_grid_color(fl_gray_ramp(20));
    s->eq_y->major_grid_color(fl_gray_ramp(15));
    s->eq_y->label_grid_color(fl_gray_ramp(10));
    s->eq_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->eq_y->minor_grid_style(FL_DOT);
    s->eq_y->major_step(5);
    s->eq_y->label_step(1);
    s->eq_y->axis_color(FL_BLACK);
    s->eq_y->current();

    s->c_eq->end();

    s->c_symbol_track = new Fl_Group(380, 200, 525, 200);
    s->c_symbol_track->box(FL_DOWN_BOX);
    s->c_symbol_track->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    s->c_symbol_track->current();

    s->canvas_track = new Ca_Canvas(490, 235, 300, 100, "Symbol and carrier tracking");
    s->canvas_track->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_track->color(7);
    s->canvas_track->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(s->canvas_track);
    s->canvas_track->border(15);

    s->track_x = new Ca_X_Axis(495, 335, 290, 30, "Time (symbols)");
    s->track_x->align(FL_ALIGN_BOTTOM);
    s->track_x->minimum(0.0);
    s->track_x->maximum(2400.0*5.0);
    s->track_x->label_format("%g");
    s->track_x->minor_grid_color(fl_gray_ramp(20));
    s->track_x->major_grid_color(fl_gray_ramp(15));
    s->track_x->label_grid_color(fl_gray_ramp(10));
    s->track_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->track_x->minor_grid_style(FL_DOT);
    s->track_x->major_step(5);
    s->track_x->label_step(1);
    s->track_x->axis_align(CA_BOTTOM | CA_LINE);
    s->track_x->axis_color(FL_BLACK);
    s->track_x->current();

    s->symbol_track_y = new Ca_Y_Axis(420, 240, 70, 90, "Cor");
    s->symbol_track_y->align(FL_ALIGN_LEFT);
    s->symbol_track_y->minimum(-0.1);
    s->symbol_track_y->maximum(0.1);
    s->symbol_track_y->minor_grid_color(fl_gray_ramp(20));
    s->symbol_track_y->major_grid_color(fl_gray_ramp(15));
    s->symbol_track_y->label_grid_color(fl_gray_ramp(10));
    s->symbol_track_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->symbol_track_y->minor_grid_style(FL_DOT);
    s->symbol_track_y->major_step(5);
    s->symbol_track_y->label_step(1);
    s->symbol_track_y->axis_color(FL_RED);
    s->symbol_track_y->current();

    s->carrier_y = new Ca_Y_Axis(790, 240, 70, 90, "Freq");
    s->carrier_y->align(FL_ALIGN_RIGHT);
    s->carrier_y->minimum(-0.1);
    s->carrier_y->maximum(0.1);
    s->carrier_y->minor_grid_color(fl_gray_ramp(20));
    s->carrier_y->major_grid_color(fl_gray_ramp(15));
    s->carrier_y->label_grid_color(fl_gray_ramp(10));
    s->carrier_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->carrier_y->minor_grid_style(FL_DOT);
    s->carrier_y->major_step(5);
    s->carrier_y->label_step(1);
    s->carrier_y->axis_align(CA_RIGHT);
    s->carrier_y->axis_color(FL_BLUE);
    s->carrier_y->current();

    s->c_symbol_track->end();

    s->audio_meter = new Fl_Audio_Meter(672, 10, 16, 150, "");
    s->audio_meter->box(FL_PLASTIC_UP_BOX);
    s->audio_meter->type(FL_VERT_AUDIO_METER);

    s->audio_level = new Fl_Output(650, 170, 60, 20, "");
    s->audio_level->textsize(10);

    s->c_right->end();

    Fl_Group::current()->resizable(s->c_right);
    s->w->end();
    s->w->show();

    s->next_constel_point = 0;
    s->constel_window = 10000;

    s->carrier_points = 0;
    s->carrier_ptr = 0;
    s->carrier_window = 100;

    s->symbol_track_points = 0;
    s->symbol_track_window = 10000;
    Fl::check();
    return s;
}
/*- End of function --------------------------------------------------------*/

void qam_wait_to_end(qam_monitor_t *s) 
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
#endif
/*- End of file ------------------------------------------------------------*/

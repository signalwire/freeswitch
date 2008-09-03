/*
 * SpanDSP - a series of DSP components for telephony
 *
 * line_model_monitor.cpp - Model activity in a telephone line model, using the FLTK toolkit.
 *
 * Written by Steve Underwood <steveu@coppice.org>
 *
 * Copyright (C) 2006 Steve Underwood
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
 * $Id: line_model_monitor.cpp,v 1.4 2008/05/27 15:08:21 steveu Exp $
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
#if defined(HAVE_FFTW3_H)
#include <fftw3.h>
#else
#include <fftw.h>
#endif

#include <FL/Fl.H>
#include <FL/Fl_Overlay_Window.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Cartesian.H>
#include <FL/Fl_Audio_Meter.H>
#include <FL/fl_draw.H>

#include "spandsp.h"
#include "line_model_monitor.h"

Fl_Double_Window *w;

Fl_Audio_Meter *audio_meter;

Fl_Group *c_spec;
Fl_Group *c_right;
Fl_Group *c_can;
Fl_Group *c_line_model;

Ca_Canvas *canvas_spec;
Ca_X_Axis *spec_freq;
Ca_Y_Axis *spec_amp;
Ca_Line *spec_re = NULL;
double spec_re_plot[2*512];

Ca_Canvas *canvas_can;
Ca_X_Axis *can_x;
Ca_Y_Axis *can_y;
Ca_Line *can_re = NULL;
double can_re_plot[512];

Ca_Canvas *canvas_line_model;
Ca_X_Axis *line_model_x;
Ca_Y_Axis *line_model_y;
Ca_Line *line_model_re = NULL;
double line_model_re_plot[512];

static int skip = 0;

int in_ptr;
#if defined(HAVE_FFTW3_H)
double in[1024][2];
double out[1024][2];
#else
fftw_complex in[1024];
fftw_complex out[1024];
#endif
fftw_plan p;

int line_model_monitor_can_update(const float *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (can_re)
        delete can_re;

    canvas_can->current(canvas_can);
    i = 0;
    min = coeffs[i];
    max = coeffs[i];
    for (i = 0;  i < len;  i++)
    {
        can_re_plot[2*i] = i;
        can_re_plot[2*i + 1] = coeffs[i];
        if (min > coeffs[i])
            min = coeffs[i];
        if (max < coeffs[i])
            max = coeffs[i];
    }
    can_y->maximum((max == min)  ?  max + 0.2  :  max);
    can_y->minimum(min);
    can_re = new Ca_Line(len, can_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int line_model_monitor_line_model_update(const float *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (line_model_re)
        delete line_model_re;

    canvas_line_model->current(canvas_line_model);
    i = 0;
    min = coeffs[i];
    max = coeffs[i];
    for (i = 0;  i < len;  i++)
    {
        line_model_re_plot[2*i] = i;
        line_model_re_plot[2*i + 1] = coeffs[i];
        if (min > coeffs[i])
            min = coeffs[i];
        if (max < coeffs[i])
            max = coeffs[i];
    }
    line_model_y->maximum((max == min)  ?  max + 0.2  :  max);
    line_model_y->minimum(min);
    line_model_re = new Ca_Line(len, line_model_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int line_model_monitor_line_spectrum_update(const int16_t amp[], int len)
{
    int i;
    int x;

    for (i = 0;  i < len;  i++)
        audio_meter->sample(amp[i]/32768.0);

    if (in_ptr + len < 512)
    {
        /* Just add this fragment to the buffer. */
        for (i = 0;  i < len;  i++)
#if defined(HAVE_FFTW3_H)
            in[in_ptr + i][0] = amp[i];
#else
            in[in_ptr + i].re = amp[i];
#endif
        in_ptr += len;
        return 0;
    }
    if (len >= 512)
    {
        /* We have enough for a whole block. Use the last 512 samples
           we have. */
        x = len - 512;
        for (i = 0;  i < 512;  i++)
#if defined(HAVE_FFTW3_H)
            in[i][0] = amp[x + i];
#else
            in[i].re = amp[x + i];
#endif
    }
    else
    {
        /* We want the last 512 samples. */
        x = 512 - len;
        for (i = 0;  i < x;  i++)
#if defined(HAVE_FFTW3_H)
            in[i][0] = in[in_ptr - x + i][0];
#else
            in[i].re = in[in_ptr - x + i].re;
#endif
        for (i = x;  i < 512;  i++)
#if defined(HAVE_FFTW3_H)
            in[i][0] = amp[i - x];
#else
            in[i].re = amp[i - x];
#endif
    }
    in_ptr = 0;
#if defined(HAVE_FFTW3_H)    
    fftw_execute(p);
#else
    fftw_one(p, in, out);
#endif
    if (spec_re)
        delete spec_re;
    canvas_spec->current(canvas_spec);
    for (i = 0;  i < 512;  i++)
    {
        spec_re_plot[2*i] = i*4000.0/512.0;
#if defined(HAVE_FFTW3_H)    
        spec_re_plot[2*i + 1] = 20.0*log10(sqrt(out[i][0]*out[i][0] + out[i][1]*out[i][1])/(256.0*32768)) + 3.14;
#else
        spec_re_plot[2*i + 1] = 20.0*log10(sqrt(out[i].re*out[i].re + out[i].im*out[i].im)/(256.0*32768)) + 3.14;
#endif
    }
    spec_re = new Ca_Line(512, spec_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

int start_line_model_monitor(int len)
{
    char buf[132 + 1];
    float x;
    float y;
    int i;

    w = new Fl_Double_Window(850, 400, "Telephone line model monitor");

    c_spec = new Fl_Group(0, 0, 380, 400);
    c_spec->box(FL_DOWN_BOX);
    c_spec->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);

    canvas_spec = new Ca_Canvas(60, 30, 300, 300, "Spectrum");
    canvas_spec->box(FL_PLASTIC_DOWN_BOX);
    canvas_spec->color(7);
    canvas_spec->align(FL_ALIGN_TOP);
    canvas_spec->border(15);

    spec_freq = new Ca_X_Axis(65, 330, 290, 30, "Freq (Hz)");
    spec_freq->align(FL_ALIGN_BOTTOM);
    spec_freq->minimum(0);
    spec_freq->maximum(4000);
    spec_freq->label_format("%g");
    spec_freq->minor_grid_color(fl_gray_ramp(20));
    spec_freq->major_grid_color(fl_gray_ramp(15));
    spec_freq->label_grid_color(fl_gray_ramp(10));
    spec_freq->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    spec_freq->minor_grid_style(FL_DOT);
    spec_freq->major_step(5);
    spec_freq->label_step(1);
    spec_freq->axis_color(FL_BLACK);
    spec_freq->axis_align(CA_BOTTOM | CA_LINE);

    spec_amp = new Ca_Y_Axis(20, 35, 40, 290, "Amp (dBmO)");
    spec_amp->align(FL_ALIGN_LEFT);
    spec_amp->minimum(-80.0);
    spec_amp->maximum(10.0);
    spec_amp->minor_grid_color(fl_gray_ramp(20));
    spec_amp->major_grid_color(fl_gray_ramp(15));
    spec_amp->label_grid_color(fl_gray_ramp(10));
    //spec_amp->grid_visible(CA_MINOR_TICK | CA_MAJOR_TICK | CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    spec_amp->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    spec_amp->minor_grid_style(FL_DOT);
    spec_amp->major_step(5);
    spec_amp->label_step(1);
    spec_amp->axis_color(FL_BLACK);

    spec_amp->current();

    c_spec->end();

    c_right = new Fl_Group(440, 0, 465, 405);

    c_can = new Fl_Group(380, 0, 415, 200);
    c_can->box(FL_DOWN_BOX);
    c_can->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_can->current();

    canvas_can = new Ca_Canvas(460, 35, 300, 100, "??? coefficients");
    canvas_can->box(FL_PLASTIC_DOWN_BOX);
    canvas_can->color(7);
    canvas_can->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_can);
    canvas_can->border(15);

    can_x = new Ca_X_Axis(465, 135, 290, 30, "Tap");
    can_x->align(FL_ALIGN_BOTTOM);
    can_x->minimum(0.0);
    can_x->maximum((float) len);
    can_x->label_format("%g");
    can_x->minor_grid_color(fl_gray_ramp(20));
    can_x->major_grid_color(fl_gray_ramp(15));
    can_x->label_grid_color(fl_gray_ramp(10));
    can_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    can_x->minor_grid_style(FL_DOT);
    can_x->major_step(5);
    can_x->label_step(1);
    can_x->axis_align(CA_BOTTOM | CA_LINE);
    can_x->axis_color(FL_BLACK);
    can_x->current();

    can_y = new Ca_Y_Axis(420, 40, 40, 90, "Amp");
    can_y->align(FL_ALIGN_LEFT);
    can_y->minimum(-0.1);
    can_y->maximum(0.1);
    can_y->minor_grid_color(fl_gray_ramp(20));
    can_y->major_grid_color(fl_gray_ramp(15));
    can_y->label_grid_color(fl_gray_ramp(10));
    can_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    can_y->minor_grid_style(FL_DOT);
    can_y->major_step(5);
    can_y->label_step(1);
    can_y->axis_color(FL_BLACK);
    can_y->current();

    c_can->end();

    c_line_model = new Fl_Group(380, 200, 415, 200);
    c_line_model->box(FL_DOWN_BOX);
    c_line_model->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    c_line_model->current();

    canvas_line_model = new Ca_Canvas(460, 235, 300, 100, "Line impulse response model");
    canvas_line_model->box(FL_PLASTIC_DOWN_BOX);
    canvas_line_model->color(7);
    canvas_line_model->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(canvas_line_model);
    canvas_line_model->border(15);

    line_model_x = new Ca_X_Axis(465, 335, 290, 30, "Tap");
    line_model_x->align(FL_ALIGN_BOTTOM);
    line_model_x->minimum(0.0);
    line_model_x->maximum((float) len);
    line_model_x->label_format("%g");
    line_model_x->minor_grid_color(fl_gray_ramp(20));
    line_model_x->major_grid_color(fl_gray_ramp(15));
    line_model_x->label_grid_color(fl_gray_ramp(10));
    line_model_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    line_model_x->minor_grid_style(FL_DOT);
    line_model_x->major_step(5);
    line_model_x->label_step(1);
    line_model_x->axis_align(CA_BOTTOM | CA_LINE);
    line_model_x->axis_color(FL_BLACK);
    line_model_x->current();

    line_model_y = new Ca_Y_Axis(420, 240, 40, 90, "Amp");
    line_model_y->align(FL_ALIGN_LEFT);
    line_model_y->minimum(-0.1);
    line_model_y->maximum(0.1);
    line_model_y->minor_grid_color(fl_gray_ramp(20));
    line_model_y->major_grid_color(fl_gray_ramp(15));
    line_model_y->label_grid_color(fl_gray_ramp(10));
    line_model_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    line_model_y->minor_grid_style(FL_DOT);
    line_model_y->major_step(5);
    line_model_y->label_step(1);
    line_model_y->axis_color(FL_BLACK);
    line_model_y->current();

    c_line_model->end();

    audio_meter = new Fl_Audio_Meter(810, 40, 10, 250, "");
    audio_meter->box(FL_PLASTIC_UP_BOX);
    audio_meter->type(FL_VERT_AUDIO_METER);

    c_right->end();

    Fl_Group::current()->resizable(c_right);
    w->end();
    w->show();

#if defined(HAVE_FFTW3_H)    
    p = fftw_plan_dft_1d(1024, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);
    for (i = 0;  i < 1024;  i++)
    {
        in[i][0] = 0.0;
        in[i][1] = 0.0;
    }
#else
    p = fftw_create_plan(1024, FFTW_BACKWARD, FFTW_ESTIMATE);
    for (i = 0;  i < 1024;  i++)
    {
        in[i].re = 0.0;
        in[i].im = 0.0;
    }
#endif
    in_ptr = 0;

    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

void line_model_monitor_wait_to_end(void) 
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

void line_model_monitor_update_display(void) 
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

/*
 * SpanDSP - a series of DSP components for telephony
 *
 * echo_monitor.cpp - Display echo canceller status, using the FLTK toolkit.
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
#include "echo_monitor.h"

struct line_model_monitor_s
{
    Fl_Double_Window *w;

    Fl_Audio_Meter *audio_meter;
    Fl_Group *c_spec;
    Fl_Group *c_right;
    Fl_Group *c_can;
    Fl_Group *c_line_model;

    Ca_Canvas *canvas_spec;
    Ca_X_Axis *spec_freq;
    Ca_Y_Axis *spec_amp;
    Ca_Line *spec_re;
    double spec_re_plot[2*512];

    Ca_Canvas *canvas_can;
    Ca_X_Axis *can_x;
    Ca_Y_Axis *can_y;
    Ca_Line *can_re;
    double can_re_plot[512];

    Ca_Canvas *canvas_line_model;
    Ca_X_Axis *line_model_x;
    Ca_Y_Axis *line_model_y;
    Ca_Line *line_model_re;
    double line_model_re_plot[512];

    int in_ptr;
#if defined(HAVE_FFTW3_H)
    double in[1024][2];
    double out[1024][2];
#else
    fftw_complex in[1024];
    fftw_complex out[1024];
#endif
    fftw_plan p;
};


static int skip = 0;
static struct line_model_monitor_s echo;
static struct line_model_monitor_s *s = &echo;

int echo_can_monitor_can_update(const int16_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (s->can_re)
        delete s->can_re;

    s->canvas_can->current(s->canvas_can);
    i = 0;
    min = coeffs[i];
    max = coeffs[i];
    for (i = 0;  i < len;  i++)
    {
        s->can_re_plot[2*i] = i;
        s->can_re_plot[2*i + 1] = coeffs[i];
        if (min > coeffs[i])
            min = coeffs[i];
        if (max < coeffs[i])
            max = coeffs[i];
    }
    s->can_y->maximum((max == min)  ?  max + 0.2  :  max);
    s->can_y->minimum(min);
    s->can_re = new Ca_Line(len, s->can_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int echo_can_monitor_line_model_update(const int32_t *coeffs, int len)
{
    int i;
    float min;
    float max;

    if (s->line_model_re)
        delete s->line_model_re;

    s->canvas_line_model->current(s->canvas_line_model);
    i = 0;
    min = coeffs[i];
    max = coeffs[i];
    for (i = 0;  i < len;  i++)
    {
        s->line_model_re_plot[2*i] = i;
        s->line_model_re_plot[2*i + 1] = coeffs[i];
        if (min > coeffs[i])
            min = coeffs[i];
        if (max < coeffs[i])
            max = coeffs[i];
    }
    s->line_model_y->maximum((max == min)  ?  max + 0.2  :  max);
    s->line_model_y->minimum(min);
    s->line_model_re = new Ca_Line(len, s->line_model_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    if (++skip >= 100)
    {
        skip = 0;
        Fl::check();
    }
    return 0;
}
/*- End of function --------------------------------------------------------*/

int echo_can_monitor_line_spectrum_update(const int16_t amp[], int len)
{
    int i;
    int x;

    for (i = 0;  i < len;  i++)
        s->audio_meter->sample(amp[i]/32768.0);

    if (s->in_ptr + len < 512)
    {
        /* Just add this fragment to the buffer. */
        for (i = 0;  i < len;  i++)
#if defined(HAVE_FFTW3_H)
            s->in[s->in_ptr + i][0] = amp[i];
#else
            s->in[s->in_ptr + i].re = amp[i];
#endif
        s->in_ptr += len;
        return 0;
    }
    if (len >= 512)
    {
        /* We have enough for a whole block. Use the last 512 samples
           we have. */
        x = len - 512;
        for (i = 0;  i < 512;  i++)
#if defined(HAVE_FFTW3_H)
            s->in[i][0] = amp[x + i];
#else
            s->in[i].re = amp[x + i];
#endif
    }
    else
    {
        /* We want the last 512 samples. */
        x = 512 - len;
        for (i = 0;  i < x;  i++)
#if defined(HAVE_FFTW3_H)
            s->in[i][0] = s->in[s->in_ptr - x + i][0];
#else
            s->in[i].re = s->in[s->in_ptr - x + i].re;
#endif
        for (i = x;  i < 512;  i++)
#if defined(HAVE_FFTW3_H)
            s->in[i][0] = amp[i - x];
#else
            s->in[i].re = amp[i - x];
#endif
    }
    s->in_ptr = 0;
#if defined(HAVE_FFTW3_H)    
    fftw_execute(s->p);
#else
    fftw_one(s->p, s->in, s->out);
#endif
    if (s->spec_re)
        delete s->spec_re;
    s->canvas_spec->current(s->canvas_spec);
    for (i = 0;  i < 512;  i++)
    {
        s->spec_re_plot[2*i] = i*4000.0/512.0;
#if defined(HAVE_FFTW3_H)    
        s->spec_re_plot[2*i + 1] = 10.0*log10((s->out[i][0]*s->out[i][0] + s->out[i][1]*s->out[i][1])/(256.0*32768*256.0*32768) + 1.0e-10) + 3.14;
#else
        s->spec_re_plot[2*i + 1] = 10.0*log10((s->out[i].re*s->out[i].re + s->out[i].im*s->out[i].im)/(256.0*32768*256.0*32768) + 1.0e-10) + 3.14;
#endif
    }
    s->spec_re = new Ca_Line(512, s->spec_re_plot, 0, 0, FL_BLUE, CA_NO_POINT);
    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

int start_echo_can_monitor(int len)
{
    char buf[132 + 1];
    float x;
    float y;
    int i;

    s->w = new Fl_Double_Window(850, 400, "Echo canceller monitor");

    s->c_spec = new Fl_Group(0, 0, 380, 400);
    s->c_spec->box(FL_DOWN_BOX);
    s->c_spec->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);

    s->canvas_spec = new Ca_Canvas(60, 30, 300, 300, "Spectrum");
    s->canvas_spec->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_spec->color(7);
    s->canvas_spec->align(FL_ALIGN_TOP);
    s->canvas_spec->border(15);

    s->spec_freq = new Ca_X_Axis(65, 330, 290, 30, "Freq (Hz)");
    s->spec_freq->align(FL_ALIGN_BOTTOM);
    s->spec_freq->minimum(0);
    s->spec_freq->maximum(4000);
    s->spec_freq->label_format("%g");
    s->spec_freq->minor_grid_color(fl_gray_ramp(20));
    s->spec_freq->major_grid_color(fl_gray_ramp(15));
    s->spec_freq->label_grid_color(fl_gray_ramp(10));
    s->spec_freq->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->spec_freq->minor_grid_style(FL_DOT);
    s->spec_freq->major_step(5);
    s->spec_freq->label_step(1);
    s->spec_freq->axis_color(FL_BLACK);
    s->spec_freq->axis_align(CA_BOTTOM | CA_LINE);

    s->spec_amp = new Ca_Y_Axis(20, 35, 40, 290, "Amp (dBmO)");
    s->spec_amp->align(FL_ALIGN_LEFT);
    s->spec_amp->minimum(-80.0);
    s->spec_amp->maximum(10.0);
    s->spec_amp->minor_grid_color(fl_gray_ramp(20));
    s->spec_amp->major_grid_color(fl_gray_ramp(15));
    s->spec_amp->label_grid_color(fl_gray_ramp(10));
    //s->spec_amp->grid_visible(CA_MINOR_TICK | CA_MAJOR_TICK | CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->spec_amp->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->spec_amp->minor_grid_style(FL_DOT);
    s->spec_amp->major_step(5);
    s->spec_amp->label_step(1);
    s->spec_amp->axis_color(FL_BLACK);

    s->spec_amp->current();
    s->spec_re = NULL;

    s->c_spec->end();

    s->c_right = new Fl_Group(440, 0, 465, 405);

    s->c_can = new Fl_Group(380, 0, 415, 200);
    s->c_can->box(FL_DOWN_BOX);
    s->c_can->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    s->c_can->current();

    s->canvas_can = new Ca_Canvas(460, 35, 300, 100, "Canceller coefficients");
    s->canvas_can->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_can->color(7);
    s->canvas_can->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(s->canvas_can);
    s->canvas_can->border(15);

    s->can_x = new Ca_X_Axis(465, 135, 290, 30, "Tap");
    s->can_x->align(FL_ALIGN_BOTTOM);
    s->can_x->minimum(0.0);
    s->can_x->maximum((float) len);
    s->can_x->label_format("%g");
    s->can_x->minor_grid_color(fl_gray_ramp(20));
    s->can_x->major_grid_color(fl_gray_ramp(15));
    s->can_x->label_grid_color(fl_gray_ramp(10));
    s->can_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->can_x->minor_grid_style(FL_DOT);
    s->can_x->major_step(5);
    s->can_x->label_step(1);
    s->can_x->axis_align(CA_BOTTOM | CA_LINE);
    s->can_x->axis_color(FL_BLACK);
    s->can_x->current();

    s->can_y = new Ca_Y_Axis(420, 40, 40, 90, "Amp");
    s->can_y->align(FL_ALIGN_LEFT);
    s->can_y->minimum(-0.1);
    s->can_y->maximum(0.1);
    s->can_y->minor_grid_color(fl_gray_ramp(20));
    s->can_y->major_grid_color(fl_gray_ramp(15));
    s->can_y->label_grid_color(fl_gray_ramp(10));
    s->can_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->can_y->minor_grid_style(FL_DOT);
    s->can_y->major_step(5);
    s->can_y->label_step(1);
    s->can_y->axis_color(FL_BLACK);
    s->can_y->current();

    s->c_can->end();
    s->can_re = NULL;

    s->c_line_model = new Fl_Group(380, 200, 415, 200);
    s->c_line_model->box(FL_DOWN_BOX);
    s->c_line_model->align(FL_ALIGN_TOP | FL_ALIGN_INSIDE);
    s->c_line_model->current();

    s->canvas_line_model = new Ca_Canvas(460, 235, 300, 100, "Line impulse response model");
    s->canvas_line_model->box(FL_PLASTIC_DOWN_BOX);
    s->canvas_line_model->color(7);
    s->canvas_line_model->align(FL_ALIGN_TOP);
    Fl_Group::current()->resizable(s->canvas_line_model);
    s->canvas_line_model->border(15);

    s->line_model_x = new Ca_X_Axis(465, 335, 290, 30, "Tap");
    s->line_model_x->align(FL_ALIGN_BOTTOM);
    s->line_model_x->minimum(0.0);
    s->line_model_x->maximum((float) len);
    s->line_model_x->label_format("%g");
    s->line_model_x->minor_grid_color(fl_gray_ramp(20));
    s->line_model_x->major_grid_color(fl_gray_ramp(15));
    s->line_model_x->label_grid_color(fl_gray_ramp(10));
    s->line_model_x->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->line_model_x->minor_grid_style(FL_DOT);
    s->line_model_x->major_step(5);
    s->line_model_x->label_step(1);
    s->line_model_x->axis_align(CA_BOTTOM | CA_LINE);
    s->line_model_x->axis_color(FL_BLACK);
    s->line_model_x->current();

    s->line_model_y = new Ca_Y_Axis(420, 240, 40, 90, "Amp");
    s->line_model_y->align(FL_ALIGN_LEFT);
    s->line_model_y->minimum(-0.1);
    s->line_model_y->maximum(0.1);
    s->line_model_y->minor_grid_color(fl_gray_ramp(20));
    s->line_model_y->major_grid_color(fl_gray_ramp(15));
    s->line_model_y->label_grid_color(fl_gray_ramp(10));
    s->line_model_y->grid_visible(CA_LABEL_GRID | CA_ALWAYS_VISIBLE);
    s->line_model_y->minor_grid_style(FL_DOT);
    s->line_model_y->major_step(5);
    s->line_model_y->label_step(1);
    s->line_model_y->axis_color(FL_BLACK);
    s->line_model_y->current();

    s->c_line_model->end();
    s->line_model_re = NULL;

    s->audio_meter = new Fl_Audio_Meter(810, 40, 10, 250, "");
    s->audio_meter->box(FL_PLASTIC_UP_BOX);
    s->audio_meter->type(FL_VERT_AUDIO_METER);

    s->c_right->end();

    Fl_Group::current()->resizable(s->c_right);
    s->w->end();
    s->w->show();

#if defined(HAVE_FFTW3_H)    
    s->p = fftw_plan_dft_1d(1024, s->in, s->out, FFTW_BACKWARD, FFTW_ESTIMATE);
    for (i = 0;  i < 1024;  i++)
    {
        s->in[i][0] = 0.0;
        s->in[i][1] = 0.0;
    }
#else
    s->p = fftw_create_plan(1024, FFTW_BACKWARD, FFTW_ESTIMATE);
    for (i = 0;  i < 1024;  i++)
    {
        s->in[i].re = 0.0;
        s->in[i].im = 0.0;
    }
#endif
    s->in_ptr = 0;

    Fl::check();
    return 0;
}
/*- End of function --------------------------------------------------------*/

void echo_can_monitor_wait_to_end(void) 
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

void echo_can_monitor_update_display(void) 
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

/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: dump.h
  AUTHOR......: David Rowe                                                          
  DATE CREATED: 25/8/09                                                       
                                                                             
  Routines to dump data to text files for Octave analysis.

\*---------------------------------------------------------------------------*/

/*
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __DUMP__
#define __DUMP__

void dump_on(char filename_prefix[]);
void dump_off();

void dump_Sn(float Sn[]);
void dump_Sw(COMP Sw[]);
void dump_Sw_(COMP Sw_[]);

/* amplitude modelling */

void dump_model(MODEL *m);
void dump_quantised_model(MODEL *m);
void dump_Pw(COMP Pw[]);
void dump_lsp(float lsp[]);
void dump_ak(float ak[], int order);
void dump_E(float E);

/* phase modelling */

void dump_snr(float snr);
void dump_phase(float phase[], int L);
void dump_phase_(float phase[], int L);

/* NLP states */

void dump_sq(float sq[]);
void dump_dec(COMP Fw[]);
void dump_Fw(COMP Fw[]);
void dump_e(float e_hz[]);

/* post filter */

void dump_bg(float e, float bg_est, float percent_uv);

#endif

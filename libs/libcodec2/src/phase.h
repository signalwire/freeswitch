/*---------------------------------------------------------------------------*\
                                                                             
  FILE........: phase.h                                          
  AUTHOR......: David Rowe                                             
  DATE CREATED: 1/2/09                                                 
                                                                             
  Functions for modelling phase.
                                                                             
\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2009 David Rowe

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

#ifndef __PHASE__
#define __PHASE__

void phase_synth_zero_order(MODEL *model, float aks[], float *ex_phase);

#endif

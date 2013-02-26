/*--------------------------------------------------------------------------*\

	FILE........: sd.h
	AUTHOR......: David Rowe
	DATE CREATED: 22/7/93

	Function to determine spectral distortion between two sets of LPCs.

\*--------------------------------------------------------------------------*/

/*
  Copyright (C) 2009 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __SD__
#define __SD__

float spectral_dist(float ak1[], float ak2[], int p, int n);

#endif	/* __SD__  */

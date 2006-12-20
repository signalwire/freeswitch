/*
** Copyright (C) 1999-2004 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "sfconfig.h"

#include <stdio.h>
#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sndfile.h>

#define	BUFFER_SIZE	(256)

static	char	strbuffer [BUFFER_SIZE] ;

int
main (void)
{	sf_command (NULL, SFC_GET_LIB_VERSION, strbuffer, sizeof (strbuffer)) ;

	printf ("%s", strbuffer) ;

	return 0 ;
} /* main */

/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: 53b0b14d-a52b-4094-b510-df91e4947574
*/

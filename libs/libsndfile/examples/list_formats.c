/*
** Copyright (C) 2001-2005 Erik de Castro Lopo <erikd@mega-nerd.com>
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

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<math.h>

#include	<sndfile.h>

int
main (void)
{	SF_FORMAT_INFO	info ;
	SF_INFO 		sfinfo ;
	char buffer [128] ;
	int format, major_count, subtype_count, m, s ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;
	buffer [0] = 0 ;
	sf_command (NULL, SFC_GET_LIB_VERSION, buffer, sizeof (buffer)) ;
	if (strlen (buffer) < 1)
	{	printf ("Line %d: could not retrieve lib version.\n", __LINE__) ;
		exit (1) ;
		} ;
	printf ("Version : %s\n\n", buffer) ;

	sf_command (NULL, SFC_GET_FORMAT_MAJOR_COUNT, &major_count, sizeof (int)) ;
	sf_command (NULL, SFC_GET_FORMAT_SUBTYPE_COUNT, &subtype_count, sizeof (int)) ;

	sfinfo.channels = 1 ;
	for (m = 0 ; m < major_count ; m++)
	{	info.format = m ;
		sf_command (NULL, SFC_GET_FORMAT_MAJOR, &info, sizeof (info)) ;
		printf ("%s  (extension \"%s\")\n", info.name, info.extension) ;

		format = info.format ;

		for (s = 0 ; s < subtype_count ; s++)
		{	info.format = s ;
			sf_command (NULL, SFC_GET_FORMAT_SUBTYPE, &info, sizeof (info)) ;

			format = (format & SF_FORMAT_TYPEMASK) | info.format ;

			sfinfo.format = format ;
			if (sf_format_check (&sfinfo))
				printf ("   %s\n", info.name) ;
			} ;
		puts ("") ;
		} ;
	puts ("") ;

	return 0 ;
} /* main */

/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: 58127a0c-93a2-46cf-b615-fcb9adacf3f1
*/

/*
** Copyright (C) 2003,2004 Erik de Castro Lopo <erikd@mega-nerd.com>
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
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sndfile.h>

#include "utils.h"

int
main (void)
{	SNDFILE		*sndfile ;
	SF_INFO		sfinfo ;

	FILE		*bad_file ;
	const char	*bad_wav = "bad_wav.wav" ;
	const char	bad_data [] = "RIFF    WAVEfmt            " ;

	print_test_name ("open_fail_test", bad_wav) ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	sndfile = sf_open ("let's hope this file doesn't exist", SFM_READ, &sfinfo) ;

	if (sndfile)
	{	printf ("Line %d: should not have received a valid SNDFILE* pointer.\n", __LINE__) ;
		exit (1) ;
		} ;

	if ((bad_file = fopen (bad_wav, "w")) == NULL)
	{	printf ("Line %d: fopen returned NULL.\n", __LINE__) ;
		exit (1) ;
		} ;

	fwrite (bad_data, sizeof (bad_data), 1, bad_file) ;
	fclose (bad_file) ;

	sndfile = sf_open (bad_wav, SFM_READ, &sfinfo) ;

	if (sndfile)
	{	printf ("Line %d: should not have received a valid SNDFILE* pointer.\n", __LINE__) ;
		exit (1) ;
		} ;

	unlink (bad_wav) ;
	puts ("ok") ;

	return 0 ;
} /* main */


/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: 24440323-00b1-4e4b-87c5-0e3b7e9605e9
*/

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
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sndfile.h>

#include "utils.h"

#define	BUFFER_SIZE		(1<<15)
#define	SHORT_BUFFER	(256)

static void error_number_test (void) ;
static void error_value_test (void) ;

int
main (void)
{	error_number_test () ;
	error_value_test () ;

	return 0 ;
} /* main */

static void
error_number_test (void)
{	const char 	*noerror, *errstr ;
	int			k ;

	print_test_name ("error_number_test", "") ;

	noerror = sf_error_number (0) ;

	for (k = 1 ; k < 300 ; k++)
	{	errstr = sf_error_number (k) ;

		/* Test for termination condition. */
		if (errstr == noerror)
			break ;

		/* Test for error. */
		if (strstr (errstr, "This is a bug in libsndfile."))
			exit (1) ;
		} ;


	puts ("ok") ;
	return ;
} /* error_number_test */

static void
error_value_test (void)
{	static unsigned char aiff_data [0x1b0] =
	{	'F' , 'O' , 'R' , 'M' ,
		0x00, 0x00, 0x01, 0xA8, /* FORM length */

		'A' , 'I' , 'F' , 'C' ,
		} ;

	const char *filename = "error.aiff" ;
	SNDFILE *file ;
	SF_INFO sfinfo ;
	int error_num ;

	print_test_name ("error_value_test", filename) ;

	dump_data_to_file (filename, aiff_data, sizeof (aiff_data)) ;

	file = sf_open (filename, SFM_READ, &sfinfo) ;
	if (file != NULL)
	{	printf ("\n\nLine %d : Should not have been able to open this file.\n\n", __LINE__) ;
		exit (1) ;
		} ;

	if ((error_num = sf_error (NULL)) <= 1 || error_num > 300)
	{	printf ("\n\nLine %d : Should not have had an error number of %d.\n\n", __LINE__, error_num) ;
		exit (1) ;
		} ;

	remove (filename) ;
	puts ("ok") ;
	return ;
} /* error_value_test */

/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch 
** revision control system.
**
** arch-tag: 799eba74-b505-49d9-89a6-22a7f51a31b4
*/

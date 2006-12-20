/*
** Copyright (C) 2005 Erik de Castro Lopo <erikd@mega-nerd.com>
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
#include "sndfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_LOCALE_H
#include <locale.h>
#endif

#include "utils.h"

typedef struct
{	const char *locale ;
	const char *filename ;
	int	width ;
} LOCALE_DATA ;

static void locale_test (const char * locname, const char * filename, int width) ;

int
main (void)
{	LOCALE_DATA ldata [] =
	{	{	"de_DE", "F\303\274\303\237e.au", 7 },
		{	"en_AU", "kangaroo.au", 11 },
		{	"POSIX", "posix.au", 8 },
		{	"pt_PT", "concei\303\247\303\243o.au", 12 },
		{	"ja_JP", "\343\201\212\343\201\257\343\202\210\343\201\206\343\201\224\343\201\226\343\201\204\343\201\276\343\201\231.au", 21 },
		{	"vi_VN", "qu\341\273\221c ng\341\273\257.au", 11 },

		{	NULL, NULL, 0 }
		} ;
	int k ;

	for (k = 0 ; ldata [k].locale != NULL ; k++)
		locale_test (ldata [k].locale, ldata [k].filename, ldata [k].width) ;

	return 0 ;
} /* main */

static void
locale_test (const char * locname, const char * filename, int width)
{
#if (HAVE_LOCALE_H == 0 || HAVE_SETLOCALE == 0)
	locname = filename = NULL ;
	width = 0 ;
	return ;
#else
	const short wdata [] = { 1, 2, 3, 4, 5, 6, 7, 8 } ;
	short rdata [ARRAY_LEN (wdata)] ;
	const char *old_locale ;
	SNDFILE *file ;
	SF_INFO sfinfo ;

	/* Grab the old locale. */
	old_locale = setlocale (LC_ALL, NULL) ;

	if (setlocale (LC_ALL, locname) == NULL)
		return ;

	printf ("    locale_test : %-6s   %s%*c : ", locname, filename, 28 - width, ' ') ;
	fflush (stdout) ;

	sfinfo.format = SF_FORMAT_AU | SF_FORMAT_PCM_16 ;
	sfinfo.channels = 1 ;
	sfinfo.samplerate = 44100 ;

	file = test_open_file_or_die (filename, SFM_WRITE, &sfinfo, 0, __LINE__) ;
	test_write_short_or_die (file, 0, wdata, ARRAY_LEN (wdata), __LINE__) ;
	sf_close (file) ;

	file = test_open_file_or_die (filename, SFM_READ, &sfinfo, 0, __LINE__) ;
	test_read_short_or_die (file, 0, rdata, ARRAY_LEN (rdata), __LINE__) ;
	sf_close (file) ;

	/* Restore old locale. */
	setlocale (LC_ALL, old_locale) ;

	unlink (filename) ;
	puts ("ok") ;
#endif
} /* locale_test */


/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: 087b25a3-03a2-4195-acd2-23fbbc489021
*/

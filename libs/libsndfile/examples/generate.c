/*
** Copyright (C) 2002-2005 Erik de Castro Lopo <erikd@mega-nerd.com>
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
#include <math.h>

#include <sndfile.h>

#define	BUFFER_LEN			4096

static void encode_file (const char *infilename, const char *outfilename, int filetype) ;

int
main (int argc, char **argv)
{
	if (argc != 2)
	{	puts ("\nEncode a single input file into a number of different output ") ;
		puts ("encodings. These output encodings can then be moved to another ") ;
		puts ("OS for testing.\n") ;
		puts ("    Usage : generate <filename>\n") ;
		exit (1) ;
		} ;

	/* A couple of standard WAV files. Make sure Win32 plays these. */
	encode_file (argv [1], "pcmu8.wav"	, SF_FORMAT_WAV | SF_FORMAT_PCM_U8) ;
	encode_file (argv [1], "pcm16.wav"	, SF_FORMAT_WAV | SF_FORMAT_PCM_16) ;
	encode_file (argv [1], "imaadpcm.wav", SF_FORMAT_WAV | SF_FORMAT_MS_ADPCM) ;
	encode_file (argv [1], "msadpcm.wav", SF_FORMAT_WAV | SF_FORMAT_IMA_ADPCM) ;
	encode_file (argv [1], "gsm610.wav"	, SF_FORMAT_WAV | SF_FORMAT_GSM610) ;

	/* Soundforge W64. */
	encode_file (argv [1], "pcmu8.w64"	, SF_FORMAT_W64 | SF_FORMAT_PCM_U8) ;
	encode_file (argv [1], "pcm16.w64"	, SF_FORMAT_W64 | SF_FORMAT_PCM_16) ;
	encode_file (argv [1], "imaadpcm.w64", SF_FORMAT_W64 | SF_FORMAT_MS_ADPCM) ;
	encode_file (argv [1], "msadpcm.w64", SF_FORMAT_W64 | SF_FORMAT_IMA_ADPCM) ;
	encode_file (argv [1], "gsm610.w64"	, SF_FORMAT_W64 | SF_FORMAT_GSM610) ;

	return 0 ;
} /* main */

/*============================================================================================
**	Helper functions and macros.
*/

#define PUT_DOTS(k)					\
			{	while (k--)			\
					putchar ('.') ;	\
				putchar (' ') ;		\
				}

/*========================================================================================
*/

static void
encode_file (const char *infilename, const char *outfilename, int filetype)
{	static float buffer [BUFFER_LEN] ;

	SNDFILE		*infile, *outfile ;
	SF_INFO		sfinfo ;
	int			k, readcount ;

	printf ("    %s -> %s ", infilename, outfilename) ;
	fflush (stdout) ;

	k = 16 - strlen (outfilename) ;
	PUT_DOTS (k) ;

	if (! (infile = sf_open (infilename, SFM_READ, &sfinfo)))
	{	printf ("Error : could not open file : %s\n", infilename) ;
		puts (sf_strerror (NULL)) ;
		exit (1) ;
		}

	sfinfo.format = filetype ;

	if (! sf_format_check (&sfinfo))
	{	sf_close (infile) ;
		printf ("Invalid encoding\n") ;
		return ;
		} ;

	if (! (outfile = sf_open (outfilename, SFM_WRITE, &sfinfo)))
	{	printf ("Error : could not open file : %s\n", outfilename) ;
		puts (sf_strerror (NULL)) ;
		exit (1) ;
		} ;

	while ((readcount = sf_read_float (infile, buffer, BUFFER_LEN)) > 0)
		sf_write_float (outfile, buffer, BUFFER_LEN) ;

	sf_close (infile) ;
	sf_close (outfile) ;

	printf ("ok\n") ;

	return ;
} /* encode_file */


/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: fe28ef37-ae89-4f61-966b-0b1f68e37425
*/

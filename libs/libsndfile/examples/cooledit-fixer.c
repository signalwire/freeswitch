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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#include <sndfile.h>

#define	BUFFER_LEN	1024

static void usage_exit (char *progname) ;
static int is_data_really_float (SNDFILE *sndfile) ;
static void fix_file (char *filename) ;
static off_t file_size (char *filename) ;

static union
{	int i [BUFFER_LEN] ;
	float f [BUFFER_LEN] ;
} buffer ;

int
main (int argc, char *argv [])
{	SNDFILE *sndfile ;
	SF_INFO sfinfo ;
	int k, data_is_float, converted = 0 ;

	puts ("\nCooledit Fixer.\n---------------") ;

	if (argc < 2)
		usage_exit (argv [0]) ;

	for (k = 1 ; k < argc ; k++)
	{	if ((sndfile = sf_open (argv [k], SFM_READ, &sfinfo)) == NULL)
		{	/*-printf ("Failed to open : %s\n", argv [k]) ;-*/
			continue ;
			} ;

		if (sfinfo.format != (SF_FORMAT_WAV | SF_FORMAT_PCM_32))
		{	/*-printf ("%-50s : not a 32 bit PCM WAV file.\n", argv [k]) ;-*/
			sf_close (sndfile) ;
			continue ;
			} ;

		data_is_float = is_data_really_float (sndfile) ;

		sf_close (sndfile) ;

		if (data_is_float == SF_FALSE)
		{	/*-printf ("%-50s : not a Cooledit abomination.\n", argv [k]) ;-*/
			continue ;
			} ;

		fix_file (argv [k]) ;
		converted ++ ;
		} ;

	if (converted == 0)
		puts ("\nNo files converted.") ;

	puts ("") ;

	return 0 ;
} /* main */


static void
usage_exit (char *progname)
{	char *cptr ;

	if ((cptr = strrchr (progname, '/')))
		progname = cptr + 1 ;
	if ((cptr = strrchr (progname, '\\')))
		progname = cptr + 1 ;

	printf ("\n    Usage : %s <filename>\n", progname) ;
	puts ("\n"
		"Fix broken files created by Syntrillium's Cooledit. These files are \n"
		"marked as containing PCM data but actually contain floating point \n"
		"data. Only the broken files created by Cooledit are processed. All \n"
		"other files remain untouched.\n"
		"\n"
		"More than one file may be included on the command line. \n"
		) ;

	exit (1) ;
} /* usage_exit */

static int
is_data_really_float (SNDFILE *sndfile)
{	int 	k, readcount ;

	while ((readcount = sf_read_int (sndfile, buffer.i, BUFFER_LEN)) > 0)
	{	for (k = 0 ; k < readcount ; k++)
		{	if (buffer.i [k] == 0)
				continue ;

			if (fabs (buffer.f [k]) > 32768.0)
				return SF_FALSE ;
			} ;
		} ;

	return SF_TRUE ;
} /* is_data_really_float */

static void
fix_file (char *filename)
{	static	char	newfilename [512] ;

	SNDFILE *infile, *outfile ;
	SF_INFO	sfinfo ;
	int		readcount, k ;
	float	normfactor ;
	char	*cptr ;

	printf ("\nFixing : %s\n", filename) ;

	if ((infile = sf_open (filename, SFM_READ, &sfinfo)) == NULL)
	{	printf ("Not able to open input file %s\n", filename) ;
		exit (1) ;
		} ;

	if (strlen (filename) >= sizeof (newfilename) - 1)
	{	puts ("Error : Path name too long.\n") ;
		exit (1) ;
		} ;

	strncpy (newfilename, filename, sizeof (newfilename)) ;
	newfilename [sizeof (newfilename) - 1] = 0 ;

	if ((cptr = strrchr (newfilename, '/')) == NULL)
		cptr = strrchr (newfilename, '\\') ;

	if (cptr)
	{	cptr [1] = 0 ;
		strncat (newfilename, "fixed.wav", sizeof (newfilename) - strlen (newfilename) - 1) ;
		}
	else
		strncpy (newfilename, "fixed.wav", sizeof (newfilename) - 1) ;

	newfilename [sizeof (newfilename) - 1] = 0 ;

	printf ("    Output   : %s\n", newfilename) ;

	sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT ;

	if ((outfile = sf_open (newfilename, SFM_WRITE, &sfinfo)) == NULL)
	{	printf ("Not able to output open file %s\n", filename) ;
		exit (1) ;
		} ;

	/* Find the file peak. sf-command (SFC_CALC_SIGNAL_MAX) cannot be used. */

	normfactor = 0.0 ;

	while ((readcount = sf_read_int (infile, buffer.i, BUFFER_LEN)) > 0)
	{	for (k = 0 ; k < readcount ; k++)
			if (fabs (buffer.f [k]) > normfactor)
				normfactor = fabs (buffer.f [k]) ;
		} ;

	printf ("    Peak     : %g\n", normfactor) ;

	normfactor = 1.0 / normfactor ;

	sf_seek (infile, 0, SEEK_SET) ;

	while ((readcount = sf_read_int (infile, buffer.i, BUFFER_LEN)) > 0)
	{	for (k = 0 ; k < readcount ; k++)
			buffer.f [k] *= normfactor ;
		sf_write_float (outfile, buffer.f, readcount) ;
		} ;

	sf_close (infile) ;
	sf_close (outfile) ;

	if (abs (file_size (filename) - file_size (newfilename)) > 50)
	{	puts ("Error : file size mismatch.\n") ;
		exit (1) ;
		} ;

	printf ("    Renaming : %s\n", filename) ;

	if (remove (filename) != 0)
	{	perror ("rename") ;
		exit (1) ;
		} ;

	if (rename (newfilename, filename) != 0)
	{	perror ("rename") ;
		exit (1) ;
		} ;

	return ;
} /* fix_file */

static off_t
file_size (char *filename)
{	struct stat buf ;

	if (stat (filename, &buf) != 0)
	{	perror ("stat") ;
		exit (1) ;
		} ;

	return buf.st_size ;
} /* file_size */
/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: 5475655e-3898-40ff-969b-c8ab2351b0e4
*/

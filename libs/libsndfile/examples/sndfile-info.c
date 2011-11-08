/*
** Copyright (C) 1999-2006 Erik de Castro Lopo <erikd@mega-nerd.com>
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
#include	<ctype.h>
#include	<math.h>

#include	<sndfile.h>

#define	BUFFER_LEN		(1 << 16)

#if (defined (WIN32) || defined (_WIN32))
#define	snprintf	_snprintf
#endif

static void print_version (void) ;
static void print_usage (const char *progname) ;

static void info_dump (const char *filename) ;
static void instrument_dump (const char *filename) ;
static void broadcast_dump (const char *filename) ;

int
main (int argc, char *argv [])
{	int	k ;

	print_version () ;

	if (argc < 2 || strcmp (argv [1], "--help") == 0 || strcmp (argv [1], "-h") == 0)
	{	char *progname ;

		progname = strrchr (argv [0], '/') ;
		progname = progname ? progname + 1 : argv [0] ;

		print_usage (progname) ;
		return 1 ;
		} ;

	if (strcmp (argv [1], "-i") == 0)
	{	instrument_dump (argv [2]) ;
		return 0 ;
		} ;

	if (strcmp (argv [1], "-b") == 0)
	{	broadcast_dump (argv [2]) ;
		return 0 ;
		} ;

	for (k = 1 ; k < argc ; k++)
		info_dump (argv [k]) ;

	return 0 ;
} /* main */

/*==============================================================================
**	Print version and usage.
*/

static double	data [BUFFER_LEN] ;

static void
print_version (void)
{	char buffer [256] ;

	sf_command (NULL, SFC_GET_LIB_VERSION, buffer, sizeof (buffer)) ;
	printf ("\nVersion : %s\n\n", buffer) ;
} /* print_version */


static void
print_usage (const char *progname)
{	printf ("Usage :\n  %s <file> ...\n", progname) ;
	printf ("    Prints out information about one or more sound files.\n\n") ;
	printf ("  %s -i <file>\n", progname) ;
	printf ("    Prints out the instrument data for the given file.\n\n") ;
	printf ("  %s -b <file>\n", progname) ;
	printf ("    Prints out the broadcast WAV info for the given file.\n\n") ;
#if (defined (_WIN32) || defined (WIN32))
		printf ("This is a Unix style command line application which\n"
				"should be run in a MSDOS box or Command Shell window.\n\n") ;
		printf ("Sleeping for 5 seconds before exiting.\n\n") ;
		fflush (stdout) ;

		/* This is the officially blessed by microsoft way but I can't get
		** it to link.
		**     Sleep (15) ;
		** Instead, use this:
		*/
		_sleep (5 * 1000) ;
#endif
} /* print_usage */

/*==============================================================================
**	Dumping of sndfile info.
*/

static double	data [BUFFER_LEN] ;

static double
get_signal_max (SNDFILE *file)
{	double	max, temp ;
	int		readcount, k, save_state ;

	save_state = sf_command (file, SFC_GET_NORM_DOUBLE, NULL, 0) ;
	sf_command (file, SFC_SET_NORM_DOUBLE, NULL, SF_FALSE) ;

	max = 0.0 ;
	while ((readcount = sf_read_double (file, data, BUFFER_LEN)))
	{	for (k = 0 ; k < readcount ; k++)
		{	temp = fabs (data [k]) ;
			if (temp > max)
				max = temp ;
			} ;
		} ;

	sf_command (file, SFC_SET_NORM_DOUBLE, NULL, save_state) ;

	return max ;
} /* get_signal_max */

static double
calc_decibels (SF_INFO * sfinfo, double max)
{	double decibels ;

	switch (sfinfo->format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_PCM_U8 :
		case SF_FORMAT_PCM_S8 :
			decibels = max / 0x80 ;
			break ;

		case SF_FORMAT_PCM_16 :
			decibels = max / 0x8000 ;
			break ;

		case SF_FORMAT_PCM_24 :
			decibels = max / 0x800000 ;
			break ;

		case SF_FORMAT_PCM_32 :
			decibels = max / 0x80000000 ;
			break ;

		case SF_FORMAT_FLOAT :
		case SF_FORMAT_DOUBLE :
			decibels = max / 1.0 ;
			break ;

		default :
			decibels = max / 0x8000 ;
			break ;
		} ;

	return 20.0 * log10 (decibels) ;
} /* calc_decibels */

static const char *
generate_duration_str (SF_INFO *sfinfo)
{	static char str [128] ;

	int seconds ;

	memset (str, 0, sizeof (str)) ;

	if (sfinfo->samplerate < 1)
		return NULL ;

	if (sfinfo->frames / sfinfo->samplerate > 0x7FFFFFFF)
		return "unknown" ;

	seconds = sfinfo->frames / sfinfo->samplerate ;

	snprintf (str, sizeof (str) - 1, "%02d:", seconds / 60 / 60) ;

	seconds = seconds % (60 * 60) ;
	snprintf (str + strlen (str), sizeof (str) - strlen (str) - 1, "%02d:", seconds / 60) ;

	seconds = seconds % 60 ;
	snprintf (str + strlen (str), sizeof (str) - strlen (str) - 1, "%02d.", seconds) ;

	seconds = ((1000 * sfinfo->frames) / sfinfo->samplerate) % 1000 ;
	snprintf (str + strlen (str), sizeof (str) - strlen (str) - 1, "%03d", seconds) ;

	return str ;
} /* generate_duration_str */

static void
info_dump (const char *filename)
{	static	char	strbuffer [BUFFER_LEN] ;
	SNDFILE	 	*file ;
	SF_INFO	 	sfinfo ;
	double		signal_max, decibels ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	if ((file = sf_open (filename, SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file %s.\n", filename) ;
		fflush (stdout) ;
		memset (data, 0, sizeof (data)) ;
		sf_command (file, SFC_GET_LOG_INFO, strbuffer, BUFFER_LEN) ;
		puts (strbuffer) ;
		puts (sf_strerror (NULL)) ;
		return ;
		} ;

	printf ("========================================\n") ;
	sf_command (file, SFC_GET_LOG_INFO, strbuffer, BUFFER_LEN) ;
	puts (strbuffer) ;
	printf ("----------------------------------------\n") ;

	if (file == NULL)
	{	printf ("Error : Not able to open input file %s.\n", filename) ;
		fflush (stdout) ;
		memset (data, 0, sizeof (data)) ;
		puts (sf_strerror (NULL)) ;
		}
	else
	{	printf ("Sample Rate : %d\n", sfinfo.samplerate) ;
		if (sfinfo.frames > 0x7FFFFFFF)
			printf ("Frames      : unknown\n") ;
		else
			printf ("Frames      : %ld\n", (long) sfinfo.frames) ;
		printf ("Channels    : %d\n", sfinfo.channels) ;
		printf ("Format      : 0x%08X\n", sfinfo.format) ;
		printf ("Sections    : %d\n", sfinfo.sections) ;
		printf ("Seekable    : %s\n", (sfinfo.seekable ? "TRUE" : "FALSE")) ;
		printf ("Duration    : %s\n", generate_duration_str (&sfinfo)) ;

		/* Do not use sf_signal_max because it doesn work for non-seekable files . */
		signal_max = get_signal_max (file) ;
		decibels = calc_decibels (&sfinfo, signal_max) ;
		printf ("Signal Max  : %g (%4.2f dB)\n\n", signal_max, decibels) ;
		} ;

	sf_close (file) ;

} /* info_dump */

/*==============================================================================
**	Dumping of SF_INSTRUMENT data.
*/

static const char *
str_of_type (int mode)
{	switch (mode)
	{	case SF_LOOP_NONE : return "none" ;
		case SF_LOOP_FORWARD : return "fwd " ;
		case SF_LOOP_BACKWARD : return "back" ;
		case SF_LOOP_ALTERNATING : return "alt " ;
		default : break ;
		} ;

	return "????" ;
} /* str_of_mode */

static void
instrument_dump (const char *filename)
{	SNDFILE	 *file ;
	SF_INFO	 sfinfo ;
	SF_INSTRUMENT inst ;
	int got_inst, k ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	if ((file = sf_open (filename, SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file %s.\n", filename) ;
		fflush (stdout) ;
		memset (data, 0, sizeof (data)) ;
		puts (sf_strerror (NULL)) ;
		return ;
		} ;

	got_inst = sf_command (file, SFC_GET_INSTRUMENT, &inst, sizeof (inst)) ;
	sf_close (file) ;

	if (got_inst == SF_FALSE)
	{	printf ("Error : File '%s' does not contain instrument data.\n\n", filename) ;
		return ;
		} ;

	printf ("Instrument : %s\n\n", filename) ;
	printf ("  Gain        : %d\n", inst.gain) ;
	printf ("  Base note   : %d\n", inst.basenote) ;
	printf ("  Velocity    : %d - %d\n", (int) inst.velocity_lo, (int) inst.velocity_hi) ;
	printf ("  Key         : %d - %d\n", (int) inst.key_lo, (int) inst.key_hi) ;
	printf ("  Loop points : %d\n", inst.loop_count) ;

	for (k = 0 ; k < inst.loop_count ; k++)
		printf ("  %-2d    Mode : %s    Start : %6d   End : %6d   Count : %6d\n", k, str_of_type (inst.loops [k].mode), inst.loops [k].start, inst.loops [k].end, inst.loops [k].count) ;

	putchar ('\n') ;
} /* instrument_dump */

static void
broadcast_dump (const char *filename)
{	SNDFILE	 *file ;
	SF_INFO	 sfinfo ;
	SF_BROADCAST_INFO bext ;
	int got_bext ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	if ((file = sf_open (filename, SFM_READ, &sfinfo)) == NULL)
	{	printf ("Error : Not able to open input file %s.\n", filename) ;
		fflush (stdout) ;
		memset (data, 0, sizeof (data)) ;
		puts (sf_strerror (NULL)) ;
		return ;
		} ;

	memset (&bext, 0, sizeof (SF_BROADCAST_INFO)) ;

	got_bext = sf_command (file, SFC_GET_BROADCAST_INFO, &bext, sizeof (bext)) ;
	sf_close (file) ;

	if (got_bext == SF_FALSE)
	{	printf ("Error : File '%s' does not contain broadcast information.\n\n", filename) ;
		return ;
		} ;

	printf ("Description      : %.*s\n", (int) sizeof (bext.description), bext.description) ;
	printf ("Originator       : %.*s\n", (int) sizeof (bext.originator), bext.originator) ;
	printf ("Origination ref  : %.*s\n", (int) sizeof (bext.originator_reference), bext.originator_reference) ;
	printf ("Origination date : %.*s\n", (int) sizeof (bext.origination_date), bext.origination_date) ;
	printf ("Origination time : %.*s\n", (int) sizeof (bext.origination_time), bext.origination_time) ;
	printf ("BWF version      : %d\n", bext.version) ;
	printf ("UMID             : %.*s\n", (int) sizeof (bext.umid), bext.umid) ;
	printf ("Coding history   : %.*s\n", bext.coding_history_size, bext.coding_history) ;

} /* broadcast_dump */

/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: f59a05db-a182-41de-aedd-d717ce2bb099
*/

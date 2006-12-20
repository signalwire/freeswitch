/*
** Copyright (C) 2001-2005 Erik de Castro Lopo <erikd@mega-nerd.com>
**
** This program is free software ; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation ; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY ; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program ; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "sfconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <math.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if (HAVE_DECL_S_IRGRP == 0)
#include <sf_unistd.h>
#endif

#if (defined (WIN32) || defined (_WIN32))
#include <io.h>
#include <direct.h>
#endif

#include	<sndfile.h>

#include	"utils.h"

#define	BUFFER_LEN		(1<<10)
#define LOG_BUFFER_SIZE	1024

static void	zero_data_test (const char *filename, int typemajor) ;
static void	filesystem_full_test (int typemajor) ;
static void	permission_test (const char *filename, int typemajor) ;

int
main (int argc, char *argv [])
{	int		do_all = 0 ;
	int		test_count = 0 ;

	if (argc != 2)
	{	printf ("Usage : %s <test>\n", argv [0]) ;
		printf ("    Where <test> is one of the following:\n") ;
		printf ("           wav  - test WAV file peak chunk\n") ;
		printf ("           aiff - test AIFF file PEAK chunk\n") ;
		printf ("           all  - perform all tests\n") ;
		exit (1) ;
		} ;

	do_all=!strcmp (argv [1], "all") ;

	if (do_all || ! strcmp (argv [1], "wav"))
	{	zero_data_test ("zerolen.wav", SF_FORMAT_WAV) ;
		filesystem_full_test (SF_FORMAT_WAV) ;
		permission_test ("readonly.wav", SF_FORMAT_WAV) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "aiff"))
	{	zero_data_test ("zerolen.aiff", SF_FORMAT_AIFF) ;
		filesystem_full_test (SF_FORMAT_AIFF) ;
		permission_test ("readonly.aiff", SF_FORMAT_AIFF) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "au"))
	{	zero_data_test ("zerolen.au", SF_FORMAT_AU) ;
		filesystem_full_test (SF_FORMAT_AU) ;
		permission_test ("readonly.au", SF_FORMAT_AU) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "caf"))
	{	zero_data_test ("zerolen.caf", SF_FORMAT_CAF) ;
		filesystem_full_test (SF_FORMAT_CAF) ;
		permission_test ("readonly.caf", SF_FORMAT_CAF) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "svx"))
	{	zero_data_test ("zerolen.svx", SF_FORMAT_SVX) ;
		filesystem_full_test (SF_FORMAT_SVX) ;
		permission_test ("readonly.svx", SF_FORMAT_SVX) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "nist"))
	{	zero_data_test ("zerolen.nist", SF_FORMAT_NIST) ;
		filesystem_full_test (SF_FORMAT_NIST) ;
		permission_test ("readonly.nist", SF_FORMAT_NIST) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "paf"))
	{	zero_data_test ("zerolen.paf", SF_FORMAT_PAF) ;
		filesystem_full_test (SF_FORMAT_PAF) ;
		permission_test ("readonly.paf", SF_FORMAT_PAF) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "ircam"))
	{	zero_data_test ("zerolen.ircam", SF_FORMAT_IRCAM) ;
		filesystem_full_test (SF_FORMAT_IRCAM) ;
		permission_test ("readonly.ircam", SF_FORMAT_IRCAM) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "voc"))
	{	zero_data_test ("zerolen.voc", SF_FORMAT_VOC) ;
		filesystem_full_test (SF_FORMAT_VOC) ;
		permission_test ("readonly.voc", SF_FORMAT_VOC) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "w64"))
	{	zero_data_test ("zerolen.w64", SF_FORMAT_W64) ;
		filesystem_full_test (SF_FORMAT_W64) ;
		permission_test ("readonly.w64", SF_FORMAT_W64) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "mat4"))
	{	zero_data_test ("zerolen.mat4", SF_FORMAT_MAT4) ;
		filesystem_full_test (SF_FORMAT_MAT4) ;
		permission_test ("readonly.mat4", SF_FORMAT_MAT4) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "mat5"))
	{	zero_data_test ("zerolen.mat5", SF_FORMAT_MAT5) ;
		filesystem_full_test (SF_FORMAT_MAT5) ;
		permission_test ("readonly.mat5", SF_FORMAT_MAT5) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "pvf"))
	{	zero_data_test ("zerolen.pvf", SF_FORMAT_PVF) ;
		filesystem_full_test (SF_FORMAT_PVF) ;
		permission_test ("readonly.pvf", SF_FORMAT_PVF) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "htk"))
	{	zero_data_test ("zerolen.htk", SF_FORMAT_HTK) ;
		filesystem_full_test (SF_FORMAT_HTK) ;
		permission_test ("readonly.htk", SF_FORMAT_HTK) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "avr"))
	{	zero_data_test ("zerolen.avr", SF_FORMAT_AVR) ;
		filesystem_full_test (SF_FORMAT_AVR) ;
		permission_test ("readonly.avr", SF_FORMAT_AVR) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "sds"))
	{	zero_data_test ("zerolen.sds", SF_FORMAT_SDS) ;
		filesystem_full_test (SF_FORMAT_SDS) ;
		permission_test ("readonly.sds", SF_FORMAT_SDS) ;
		test_count++ ;
		} ;

	if (test_count == 0)
	{	printf ("Mono : ************************************\n") ;
		printf ("Mono : *  No '%s' test defined.\n", argv [1]) ;
		printf ("Mono : ************************************\n") ;
		return 1 ;
		} ;

	return 0 ;
} /* main */


/*============================================================================================
**	Here are the test functions.
*/

static void
zero_data_test (const char *filename, int typemajor)
{	SNDFILE		*file ;
	SF_INFO		sfinfo ;
	int			frames ;

	print_test_name ("zero_data_test", filename) ;

	sfinfo.samplerate = 44100 ;
	sfinfo.format = (typemajor | SF_FORMAT_PCM_16) ;
	sfinfo.channels = 1 ;
	sfinfo.frames = 0 ;

	frames = BUFFER_LEN / sfinfo.channels ;

	file = test_open_file_or_die (filename, SFM_WRITE, &sfinfo, SF_TRUE, __LINE__) ;

	sf_close (file) ;

	memset (&sfinfo, 0, sizeof (sfinfo)) ;

	file = test_open_file_or_die (filename, SFM_READ, &sfinfo, SF_TRUE, __LINE__) ;

	sf_close (file) ;

	unlink (filename) ;
	puts ("ok") ;
} /* zero_data_test */

static void
filesystem_full_test (int typemajor)
{	SNDFILE		*file ;
	SF_INFO		sfinfo ;
	struct stat buf ;

	const char	*filename = "/dev/full", *errorstr ;
	int			frames ;

#if (defined (WIN32) || defined (_WIN32))
	/* Can't run this test on Win32 so return. */
	return ;
#endif

	print_test_name ("filesystem_full_test", filename) ;

	if (stat (filename, &buf) != 0)
	{	puts ("/dev/full missing") ;
		return ;
		} ;

	if (S_ISCHR (buf.st_mode) == 0 && S_ISBLK (buf.st_mode) == 0)
	{	puts ("/dev/full is not a device file") ;
		return ;
		} ;

	sfinfo.samplerate = 44100 ;
	sfinfo.format = (typemajor | SF_FORMAT_PCM_16) ;
	sfinfo.channels = 1 ;
	sfinfo.frames = 0 ;

	frames = BUFFER_LEN / sfinfo.channels ;

	if ((file = sf_open (filename, SFM_WRITE, &sfinfo)) != NULL)
	{	printf ("\n\nLine %d : Error, file should not have openned.\n", __LINE__ - 1) ;
		exit (1) ;
		} ;

	errorstr = sf_strerror (file) ;

	if (strstr (errorstr, " space ") == NULL || strstr (errorstr, "device") == NULL)
	{	printf ("\n\nLine %d : Error bad error string : %s.\n", __LINE__ - 1, errorstr) ;
		exit (1) ;
		} ;

	puts ("ok") ;
} /* filesystem_full_test */

static void
permission_test (const char *filename, int typemajor)
{
#if (OS_IS_WIN32)
	/* Avoid compiler warnings. */
	filename = filename ;
	typemajor = typemajor ;

	/* Can't run this test on Win32 so return. */
	return ;
#else

	FILE		*textfile ;
	SNDFILE		*file ;
	SF_INFO		sfinfo ;
	const char	*errorstr ;
	int			frames ;

	if (getuid () == 0)
	{	/* If running as root bypass this test.
		** Root is allowed to open a readonly file for write.
		*/
		return ;
		} ;

	print_test_name ("permission_test", filename) ;

	if ((textfile = fopen (filename, "w")) == NULL)
	{	printf ("\n\nLine %d : not able to open text file for write.\n", __LINE__) ;
		exit (1) ;
		} ;

	fprintf (textfile, "This is a read only file.\n") ;
	fclose (textfile) ;

	if (chmod (filename, S_IRUSR | S_IRGRP))
	{	printf ("\n\nLine %d : chmod failed", __LINE__) ;
		fflush (stdout) ;
		perror ("") ;
		exit (1) ;
		} ;

	sfinfo.samplerate = 44100 ;
	sfinfo.format = (typemajor | SF_FORMAT_PCM_16) ;
	sfinfo.channels = 1 ;
	sfinfo.frames = 0 ;

	frames = BUFFER_LEN / sfinfo.channels ;

	if ((file = sf_open (filename, SFM_WRITE, &sfinfo)) != NULL)
	{	printf ("\n\nLine %d : Error, file should not have opened.\n", __LINE__ - 1) ;
		exit (1) ;
		} ;

	errorstr = sf_strerror (file) ;

	if (strstr (errorstr, "ermission denied") == NULL)
	{	printf ("\n\nLine %d : Error bad error string : %s.\n", __LINE__ - 1, errorstr) ;
		exit (1) ;
		} ;

	if (chmod (filename, S_IWUSR | S_IWGRP))
	{	printf ("\n\nLine %d : chmod failed", __LINE__) ;
		fflush (stdout) ;
		perror ("") ;
		exit (1) ;
		} ;

	unlink (filename) ;

	puts ("ok") ;

#endif
} /* permission_test */

/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: efc6c227-8881-4a1d-8680-0d1255975267
*/

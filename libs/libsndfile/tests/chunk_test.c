/*
** Copyright (C) 2003-2012 Erik de Castro Lopo <erikd@mega-nerd.com>
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

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include	<sndfile.h>

#include	"utils.h"

#define	BUFFER_LEN			(1 << 10)
#define LOG_BUFFER_SIZE		1024

static void	chunk_test (const char *filename, int format) ;

static void
chunk_test_helper (const char *filename, int format, const char * testdata) ;

int
main (int argc, char *argv [])
{	int		do_all = 0 ;
	int		test_count = 0 ;

	if (argc != 2)
	{	printf ("Usage : %s <test>\n", argv [0]) ;
		printf ("    Where <test> is one of the following:\n") ;
		printf ("           wav  - test adding chunks to WAV files\n") ;
		printf ("           aiff - test adding chunks to AIFF files\n") ;
		printf ("           all  - perform all tests\n") ;
		exit (1) ;
		} ;

	do_all = ! strcmp (argv [1], "all") ;

	if (do_all || ! strcmp (argv [1], "wav"))
	{	chunk_test ("chunks_pcm16.wav", SF_FORMAT_WAV | SF_FORMAT_PCM_16) ;
		chunk_test ("chunks_pcm16.rifx", SF_ENDIAN_BIG | SF_FORMAT_WAV | SF_FORMAT_PCM_16) ;
		chunk_test ("chunks_pcm16.wavex", SF_FORMAT_WAVEX | SF_FORMAT_PCM_16) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "aiff"))
	{	chunk_test ("chunks_pcm16.aiff", SF_FORMAT_AIFF | SF_FORMAT_PCM_16) ;
		test_count++ ;
		} ;

	if (do_all || ! strcmp (argv [1], "caf"))
	{	chunk_test ("chunks_pcm16.caf", SF_FORMAT_CAF | SF_FORMAT_PCM_16) ;
		chunk_test ("chunks_alac.caf", SF_FORMAT_CAF | SF_FORMAT_ALAC_16) ;
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
chunk_test_helper (const char *filename, int format, const char * testdata)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	SF_CHUNK_INFO	chunk_info ;
	SF_CHUNK_ITERATOR * iterator ;
	uint32_t		length_before ;
	int				err, allow_fd ;

	switch (format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_ALAC_16 :
			allow_fd = SF_FALSE ;
			break ;
		default :
			allow_fd = SF_TRUE ;
			break ;
		} ;

	sfinfo.samplerate	= 44100 ;
	sfinfo.channels		= 1 ;
	sfinfo.frames		= 0 ;
	sfinfo.format		= format ;

	file = test_open_file_or_die (filename, SFM_WRITE, &sfinfo, allow_fd, __LINE__) ;

	/* Set up the chunk to write. */
	memset (&chunk_info, 0, sizeof (chunk_info)) ;
	snprintf (chunk_info.id, sizeof (chunk_info.id), "Test") ;
	chunk_info.id_size = 4 ;
	chunk_info.data = strdup (testdata) ;
	chunk_info.datalen = strlen (chunk_info.data) ;

	length_before = chunk_info.datalen ;

	err = sf_set_chunk (file, &chunk_info) ;
	exit_if_true (
		err != SF_ERR_NO_ERROR,
		"\n\nLine %d : sf_set_chunk returned for testdata '%s' : %s\n\n", __LINE__, testdata, sf_error_number (err)
		) ;

	memset (chunk_info.data, 0, chunk_info.datalen) ;
	free (chunk_info.data) ;

	sf_close (file) ;

	file = test_open_file_or_die (filename, SFM_READ, &sfinfo, allow_fd, __LINE__) ;

	memset (&chunk_info, 0, sizeof (chunk_info)) ;
	snprintf (chunk_info.id, sizeof (chunk_info.id), "Test") ;
	chunk_info.id_size = 4 ;

	iterator = sf_get_chunk_iterator (file, &chunk_info) ;
	err = sf_get_chunk_size (iterator, &chunk_info) ;
	exit_if_true (
		err != SF_ERR_NO_ERROR,
		"\n\nLine %d : sf_get_chunk_size returned for testdata '%s' : %s\n\n", __LINE__, testdata, sf_error_number (err)
		) ;

	exit_if_true (
		length_before > chunk_info.datalen || chunk_info.datalen - length_before > 4,
		"\n\nLine %d : testdata '%s' : Bad chunk length %u (previous length %u)\n\n", __LINE__, testdata, chunk_info.datalen, length_before
		) ;

	chunk_info.data = malloc (chunk_info.datalen) ;
	err = sf_get_chunk_data (iterator, &chunk_info) ;
	exit_if_true (
		err != SF_ERR_NO_ERROR,
		"\n\nLine %d : sf_get_chunk_size returned for testdata '%s' : %s\n\n", __LINE__, testdata, sf_error_number (err)
		) ;

	exit_if_true (
		memcmp (testdata, chunk_info.data, length_before),
		"\n\nLine %d : Data compare failed.\n    %s\n    %s\n\n", __LINE__, testdata, (char*) chunk_info.data
		) ;

	free (chunk_info.data) ;

	sf_close (file) ;
	unlink (filename) ;
} /* chunk_test_helper */

static void
multichunk_test_helper (const char *filename, int format, const char * testdata [], size_t testdata_len)
{	SNDFILE			*file ;
	SF_INFO			sfinfo ;
	SF_CHUNK_INFO	chunk_info ;
	SF_CHUNK_ITERATOR * iterator ;
	uint32_t		length_before [testdata_len] ;
	int				err, allow_fd ;
	size_t			i ;

	sfinfo.samplerate	= 44100 ;
	sfinfo.channels		= 1 ;
	sfinfo.frames		= 0 ;
	sfinfo.format		= format ;

	switch (format & SF_FORMAT_SUBMASK)
	{	case SF_FORMAT_ALAC_16 :
			allow_fd = SF_FALSE ;
			break ;
		default :
			allow_fd = SF_TRUE ;
			break ;
		} ;

	file = test_open_file_or_die (filename, SFM_WRITE, &sfinfo, allow_fd, __LINE__) ;

	/* Set up the chunk to write. */
	for (i = 0 ; i < testdata_len ; i++)
	{	memset (&chunk_info, 0, sizeof (chunk_info)) ;
		snprintf (chunk_info.id, sizeof (chunk_info.id), "Test") ;
		chunk_info.id_size = 4 ;

		chunk_info.data = strdup (testdata [i]) ;
		chunk_info.datalen = strlen (chunk_info.data) ;

		length_before [i] = chunk_info.datalen ;

		err = sf_set_chunk (file, &chunk_info) ;
		exit_if_true (
			err != SF_ERR_NO_ERROR,
			"\n\nLine %d : sf_set_chunk returned for testdata[%d] '%s' : %s\n\n", __LINE__, (int) i, testdata [i], sf_error_number (err)
			) ;

		memset (chunk_info.data, 0, chunk_info.datalen) ;
		free (chunk_info.data) ;
	}

	sf_close (file) ;

	file = test_open_file_or_die (filename, SFM_READ, &sfinfo, allow_fd, __LINE__) ;

	memset (&chunk_info, 0, sizeof (chunk_info)) ;
	snprintf (chunk_info.id, sizeof (chunk_info.id), "Test") ;
	chunk_info.id_size = 4 ;

	iterator = sf_get_chunk_iterator (file, &chunk_info) ;

	i = 0 ;
	while (iterator)
	{	memset (&chunk_info, 0, sizeof (chunk_info)) ;
		err = sf_get_chunk_size (iterator, &chunk_info) ;
		exit_if_true (
			i > testdata_len,
			"\n\nLine %d : iterated to chunk #%d, but only %d chunks have been written\n\n", __LINE__, (int) i, (int) testdata_len
			) ;

		exit_if_true (
			err != SF_ERR_NO_ERROR,
			"\n\nLine %d : sf_get_chunk_size returned for testdata[%d] '%s' : %s\n\n", __LINE__, (int) i, testdata [i], sf_error_number (err)
			) ;

		exit_if_true (
			length_before [i] > chunk_info.datalen || chunk_info.datalen - length_before [i] > 4,
			"\n\nLine %d : testdata[%d] '%s' : Bad chunk length %u (previous length %u)\n\n", __LINE__, (int) i, testdata [i], chunk_info.datalen, length_before [i]
			) ;

		chunk_info.data = malloc (chunk_info.datalen) ;
		err = sf_get_chunk_data (iterator, &chunk_info) ;
		exit_if_true (
			err != SF_ERR_NO_ERROR,
			"\n\nLine %d : sf_get_chunk_size returned for testdata[%d] '%s' : %s\n\n", __LINE__, (int) i, testdata [i], sf_error_number (err)
			) ;

		exit_if_true (
			4 != chunk_info.id_size,
			"\n\nLine %d : testdata[%d] : Bad ID length %u (previous length %u)\n\n", __LINE__, (int) i, chunk_info.id_size, 4
			) ;
		exit_if_true (
			memcmp ("Test", chunk_info.id, 4),
			"\n\nLine %d : ID compare failed at %d.\n    %s\n    %s\n\n", __LINE__, (int) i, "Test", (char*) chunk_info.id
			) ;

		exit_if_true (
			memcmp (testdata [i], chunk_info.data, length_before [i]),
			"\n\nLine %d : Data compare failed at %d.\n    %s\n    %s\n\n", __LINE__, (int) i, testdata [i], (char*) chunk_info.data
			) ;

		free (chunk_info.data) ;
		iterator = sf_next_chunk_iterator (iterator) ;
		i++ ;
	}

	sf_close (file) ;
	unlink (filename) ;
} /* multichunk_test_helper */


static void
chunk_test (const char *filename, int format)
{	const char*		testdata [] =
	{	"There can be only one.", "", "A", "AB", "ABC", "ABCD", "ABCDE" } ;
	uint32_t k ;

	print_test_name (__func__, filename) ;

	for (k = 0 ; k < ARRAY_LEN (testdata) ; k++)
		chunk_test_helper (filename, format, testdata [k]) ;

	multichunk_test_helper (filename, format, testdata, ARRAY_LEN (testdata)) ;

	puts ("ok") ;
} /* chunk_test */

/*
** Copyright (C) 2006 Erik de Castro Lopo <erikd@mega-nerd.com>
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

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if (HAVE_DECL_S_IRGRP == 0)
#include <sf_unistd.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>

#include "utils.h"

#if (defined (WIN32) || defined (_WIN32) || defined (__CYGWIN__))
#define TEST_WIN32		1
#else
#define TEST_WIN32		0
#endif

#if TEST_WIN32
#include <windows.h>

#ifdef __CYGWIN__
#define DLL_NAME	"cygsndfile"
#else
#define DLL_NAME	"libsndfile"
#endif

static const char * locations [] =
{	"../src/", "src/", "../src/.libs/", "src/.libs/",
	NULL
} ; /* locations. */

static int
test_ordinal (HMODULE hmod, const char * func_name, int ordinal)
{	char *lpmsg ;
	void *name, *ord ;

	print_test_name ("win32_ordinal_test", func_name) ;

	ord = GetProcAddress (hmod, (LPSTR) ordinal) ;
	if ((name = GetProcAddress (hmod, func_name)) == NULL)
	{	FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError (),
					MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpmsg, 0, NULL) ;
		/*-puts (lpmsg) ;-*/
		} ;

	if (name != NULL && ord != NULL && name == ord)
	{	puts ("ok") ;
		return 0 ;
		} ;

	puts ("fail") ;
	return 1 ;
} /* test_ordinal */

static void
win32_ordinal_test (void)
{	static char buffer [1024] ;
	static char func_name [1024] ;
	HMODULE hmod = NULL ;
	FILE * file = NULL ;
	int k, ordinal, errors = 0 ;

	for (k = 0 ; locations [k] != NULL ; k++)
	{	snprintf (buffer, sizeof (buffer), "%s/%s.def", locations [k], DLL_NAME) ;
		if ((file = fopen (buffer, "r")) != NULL)
			break ;
		} ;

	if (file == NULL)
	{	puts ("\n\nError : cannot open DEF file.\n") ;
		exit (1) ;
		} ;

	for (k = 0 ; locations [k] != NULL ; k++)
	{	snprintf (buffer, sizeof (buffer), "%s/%s-1.dll", locations [k], DLL_NAME) ;
		if ((hmod = (HMODULE) LoadLibrary (buffer)) != NULL)
			break ;
		} ;

	if (hmod == NULL)
	{	puts ("\n\nError : cannot load DLL.\n") ;
		exit (1) ;
		} ;

	while (fgets (buffer, sizeof (buffer), file) != NULL)
	{	func_name [0] = 0 ;
		ordinal = 0 ;

		if (sscanf (buffer, "%s @%d", func_name, &ordinal) != 2)
			continue ;

		errors += test_ordinal (hmod, func_name, ordinal) ;
		} ;

	FreeLibrary (hmod) ;

	fclose (file) ;

	if (errors > 0)
	{	printf ("\n\nErrors : %d\n\n", errors) ;
		exit (1) ;
		} ;

	return ;
} /* win32_ordinal_test */

#endif

int
main (void)
{
#if TEST_WIN32
	win32_ordinal_test () ;
#endif

	return 0 ;
} /* main */


/*
** Do not edit or modify anything in this comment block.
** The arch-tag line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: 708f6815-e787-4143-bb99-f32c1bb5564e
*/

/*	Simple test program to make sure that Win32 linking to libsndfile is
**	working.
*/

#include <stdio.h>

#include "sndfile.h"

int
main (void)
{	static char strbuffer [256] ;
	sf_command (NULL, SFC_GET_LIB_VERSION, strbuffer, sizeof (strbuffer)) ;
	puts (strbuffer) ;
	return 0 ;
}


/*
** Do not edit or modify anything in this comment block.
** The following line is a file identity tag for the GNU Arch
** revision control system.
**
** arch-tag: 31165fd8-9d91-4e5d-8b31-8efd42ef7645
*/

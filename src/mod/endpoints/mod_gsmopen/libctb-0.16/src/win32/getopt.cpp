/////////////////////////////////////////////////////////////////////////////
// Name:        win32/getopt.cpp
// Purpose:
// Author:      unknown, I found it in the internet
// Id:          $Id: getopt.cpp,v 1.1.1.1 2004/11/24 10:30:11 jb Exp $
// Copyright:   (c) 2001 ?
// Licence:     (I think Open Source)
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>

char    *optarg;        /* Global argument pointer. */
int     optind = 0;     /* Global argv index. */

static char     *scan = NULL;   /* Private scan pointer. */

/* found char, or NULL if none */
static char *index(const char* s,char charwanted)
{
    return(strchr((char*)s, charwanted));
}

int getopt(int argc, char* argv[], char* optstring)
{
    register char c;
    register char *place;

    optarg = NULL;

    if (scan == NULL || *scan == '\0') {
	   if (optind == 0)
		  optind++;
        
	   if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
		  return(EOF);
	   if (strcmp(argv[optind], "--")==0) {
		  optind++;
		  return(EOF);
	   }
        
	   scan = argv[optind]+1;
	   optind++;
    }

    c = *scan++;
    place = index(optstring, c);

    if (place == NULL || c == ':') {
	   fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
	   return('?');
    }

    place++;
    if (*place == ':') {
	   if (*scan != '\0') {
		  optarg = scan;
		  scan = NULL;
	   } else {
		  optarg = argv[optind];
		  optind++;
	   }
    }

    return(c);
}

#ifndef LIBCTB_WIN32_GETOPT_H_INCLUDED_
#define LIBCTB_WIN32_GETOPT_H_INCLUDED_

/////////////////////////////////////////////////////////////////////////////
// Name:        win32/getopt.h
// Purpose:
// Author:      Joachim Buermann
// Copyright:   (c) 2010 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

extern char* optarg;
extern int optind;

int getopt(int argc, char* argv[], char* optstring);

#endif

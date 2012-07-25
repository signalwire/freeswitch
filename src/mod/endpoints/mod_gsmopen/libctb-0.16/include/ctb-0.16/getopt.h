#ifndef LIBCTB_GETOPT_H_INCLUDED_
#define LIBCTB_GETOPT_H_INCLUDED_

/////////////////////////////////////////////////////////////////////////////
// Name:        getopt.h
// Purpose:     Simple wrapper file
// Author:      Joachim Buermann
// Copyright:   (c) 2010 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined (WIN32)
# include "win32/getopt.h"
#else
# include <getopt.h>
#endif

#endif
// __WX_GETOPT_H

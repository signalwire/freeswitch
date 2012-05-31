#ifndef LIBCTB_SERPORT_H_INCLUDED_
#define LIBCTB_SERPORT_H_INCLUDED_

/////////////////////////////////////////////////////////////////////////////
// Name:        serport.h
// Purpose:     simple wrapper file
// Author:      Joachim Buermann
// Copyright:   (c) 2010 Joachim Buermann
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#if defined (WIN32)
# include "ctb-0.16/win32/serport.h"
#else
# include "ctb-0.16/linux/serport.h"
#endif

#endif
// __SERPORT_BASE_H


// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_nls.cc
// *
// * Purpose: Groups macros, initialization and includes
// *          for National Language Support (NLS)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 3.11.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <string>

using namespace std;

#ifdef ENABLE_NLS

using namespace gsmlib;

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

bool InitNLS::initialized = false;

#endif // ENABLE_NLS

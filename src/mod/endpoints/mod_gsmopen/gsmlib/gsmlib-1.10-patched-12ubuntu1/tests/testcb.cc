// *************************************************************************
// * GSM TA/ME library
// *
// * File:    testcb.cc
// *
// * Purpose: Test cell broadcast SMS
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 3.8.2001
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_cb.h>
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_error.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

int main(int argc, char *argv[])
{
  try
  {
    CBMessageRef cbm = new CBMessage("001000320111C3343D0F82C51A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D100");
    
    cout << cbm->toString();
    
    cbm = new CBMessage("001000320111C4EAB3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D168341A8D46A3D100");
    
    cout << cbm->toString();
  }
  catch (GsmException &ge)
  {
    cerr << argv[0] << _("[ERROR]: ") << ge.what() << endl;
  }
}

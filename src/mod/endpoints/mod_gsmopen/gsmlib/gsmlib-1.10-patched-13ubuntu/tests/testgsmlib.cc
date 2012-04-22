// *************************************************************************
// * GSM TA/ME library
// *
// * File:    testgsmlib.cc
// *
// * Purpose: Test general gsm functions (without SMS/phonebook)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 17.5.1999
// *************************************************************************
#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#ifdef WIN32
#include <gsm_config.h>
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#endif
#include <gsmlib/gsm_me_ta.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

// some time-consuming tests can be switched off by commenting out the
// following macros
#define TEST_OPERATOR_INFO

void printForwardReason(string s, ForwardInfo &info)
{
  cout << "  " << s << ": "
       << (info._active ? "active " : "inactive ")
       << "number: " << info._number
       << "  subaddr: " << info._subAddr
       << "  time: " << info._time << endl;
}

int main(int argc, char *argv[])
{
  try
  {
    cout << (string)"Opening device " + argv[1] << endl;
#ifdef WIN32
    Ref<Port> port = new Win32SerialPort((string)argv[1], 38400);
#else
	Ref<Port> port = new UnixSerialPort((string)argv[1], B38400);
#endif

    cout << "Creating MeTa object" << endl;
    MeTa m(port);

    cout << "Getting ME info" << endl;
    MEInfo mei = m.getMEInfo();

    cout << "  Manufacturer: " << mei._manufacturer << endl
         << "  Model: " << mei._model << endl
         << "  Revision: " << mei._revision << endl
         << "  Serial Number: " << mei._serialNumber << endl << endl;

#ifdef TEST_OPERATOR_INFO
    try
    {
      cout << "Getting operator info" << endl;
      vector<OPInfo> opis = m.getAvailableOPInfo();
      for (vector<OPInfo>::iterator i = opis.begin(); i != opis.end(); ++i)
      {
        cout << "  Status: ";
        switch (i->_status)
        {
        case 0: cout << "unknown"; break;
        case 1: cout << "current"; break;
        case 2: cout << "available"; break;
        case 3: cout << "forbidden"; break;
        }
        cout << endl
             << "  Long name: '" << i->_longName << "' "
             << "  Short name: '" << i->_shortName << "' "
             << "  Numeric name: " << i->_numericName << endl;
      }
    }
    catch (GsmException &ge)
    {
      if (ge.getErrorCode() == 0)
        cout << "phone failure ignored" << endl;
      else
        throw;
    }
    cout << endl;
#endif // TEST_OPERATOR_INFO

    cout << "Current operator info" << endl;
    OPInfo opi = m.getCurrentOPInfo();
    cout << "  Long name: '" << opi._longName << "' "
         << "  Short name: '" << opi._shortName << "' "
         << "  Numeric name: " << opi._numericName << endl
         << "  Mode: ";
    switch (opi._mode)
    {
    case 0: cout << "automatic"; break;
    case 1: cout << "manual"; break;
    case 2: cout << "deregister"; break;
    case 4: cout << "manual/automatic"; break;
    }
    cout << endl;

    cout << "Facility lock capabilities" << endl << "  ";
    vector<string> fclc = m.getFacilityLockCapabilities();
    for (vector<string>::iterator i = fclc.begin(); i != fclc.end(); ++i)
      cout << *i << " ";
    cout << endl << endl;

    cout << "Facility lock states" << endl;
    for (vector<string>::iterator k = fclc.begin(); k != fclc.end(); ++k)
      if (*k != "AB" && *k != "AG" && *k != "AC")
      {
        cout << "  " << *k;
        if (m.getFacilityLockStatus(*k, VoiceFacility))
          cout << "  Voice";
        if (m.getFacilityLockStatus(*k, DataFacility))
          cout << "  Data";
        if (m.getFacilityLockStatus(*k, FaxFacility))
          cout << "  Fax";
      }
    cout << endl;
    
    cout << "Facilities with password" << endl;
    vector<PWInfo> pwi = m.getPasswords();
    for (vector<PWInfo>::iterator j = pwi.begin(); j != pwi.end(); ++j)
      cout << "  " << j->_facility << " len " << j->_maxPasswdLen << endl;
    cout << endl;

    cout << "Network caller line identification identification: "
         << (m.getNetworkCLIP() ? "on" : "off") << endl << endl;

    cout << "Call forwarding information" << endl;
    for (int r = 0; r < 4; ++r)
    {
      switch (r)
      {
      case 0: cout << "UnconditionalReason" << endl; break;
      case 1: cout << "MobileBusyReason" << endl; break;
      case 2: cout << "NoReplyReason" << endl; break;
      case 3: cout << "NotReachableReason" << endl; break;
      }
      ForwardInfo voice, fax, data;
      m.getCallForwardInfo((ForwardReason)r, voice, fax, data);
      printForwardReason("Voice", voice);
      printForwardReason("Data", data);
      printForwardReason("Fax", fax);
    }
    cout << endl;

    cout << "Battery charge status" << endl;
    int bcs = m.getBatteryChargeStatus();
    switch (bcs)
    {
    case 0: cout << "ME is powered by the battery" << endl; break;
    case 1: cout << "ME has a battery connected, but is not powered by it"
                 << endl; break;
    case 2: cout << "ME does not have a battery connected" << endl; break;
    case 3: cout << "Recognized power fault, calls inhibited" << endl; break;
    }
    cout << endl;

    cout << "Battery charge: " << m.getBatteryCharge() << endl << endl;

    cout << "Signal strength: " << m.getSignalStrength() << endl << endl;

    cout << "Bit error rate: " << m.getBitErrorRate() << endl << endl;
  }
  catch (GsmException &ge)
  {
    cerr << "GsmException '" << ge.what() << "'" << endl;
    return 1;
  }
  return 0;
}

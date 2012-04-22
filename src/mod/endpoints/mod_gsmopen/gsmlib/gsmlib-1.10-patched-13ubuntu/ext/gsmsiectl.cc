// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmsiectl.cc
// *
// * Purpose: GSM Siemens mobile phone control program
// *
// * Author:  Christian W. Zuckschwerdt  <zany@triq.net>
// *
// * Created: 2001-12-15
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <string>
#if defined(HAVE_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif
#include <strstream>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsm_sie_me.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sysdep.h>
#ifdef WIN32
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#include <unistd.h>
#endif
#include <iostream>

using namespace std;
using namespace gsmlib;

// my ME

static SieMe *m;

// information parameters

enum InfoParameter {AllInfo, // print all info
                    MeInfo,     // MeInfo must be first!
                    OperatorInfo,
                    CurrentOperatorInfo,
                    FacilityLockStateInfo,
                    FacilityLockCapabilityInfo,
                    PasswordInfo,
                    CLIPInfo,
                    CallForwardingInfo,
                    BatteryInfo,
                    BitErrorInfo,
                    SCAInfo,
                    CharSetInfo,
                    PhonebookInfo, // extended Siemens info
		    SignalToneInfo,
		    RingingToneInfo,
		    BinaryInfo,
                    SignalInfo}; // SignalInfo must be last!

// operation parameters

// FIXME operations not implemented yet

// options

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"operation", required_argument, (int*)NULL, 'o'},
  {"device", required_argument, (int*)NULL, 'd'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

// helper function, prints forwarding info

void printForwardReason(string s, ForwardInfo &info)
{
  cout << s << "  "
       << (info._active ? _("active ") : _("inactive "))
       << _("number: ") << info._number
       << _("  subaddr: ") << info._subAddr
       << _("  time: ") << info._time << endl;
}

// helper function, prints integer range

void printIntRange(IntRange ir)
{
  cout << "(" << ir._low;
  if (ir._high != NOT_SET)
    cout << "-" << ir._high;
  cout << ")";
}

// helper function, prints parameter range

void printParameterRange(ParameterRange pr)
{
  cout << "(\"" << pr._parameter << "\",";
  printIntRange(pr._range);
  cout << ")";
}

// print information

static void printInfo(InfoParameter ip)
{
  switch (ip)
  {
  case MeInfo:
  {
    MEInfo mei = m->getMEInfo();
    cout << _("<ME0>  Manufacturer: ") << mei._manufacturer << endl
         << _("<ME1>  Model: ") << mei._model << endl
         << _("<ME2>  Revision: ") << mei._revision << endl
         << _("<ME3>  Serial Number: ") << mei._serialNumber << endl;
    break;
  }
  case OperatorInfo:
  {
    int count = 0;
    vector<OPInfo> opis = m->getAvailableOPInfo();
    for (vector<OPInfo>::iterator i = opis.begin(); i != opis.end(); ++i)
    {
      cout << "<OP" << count << _(">  Status: ");
      switch (i->_status)
      {
      case UnknownOPStatus: cout << _("unknown"); break;
      case CurrentOPStatus: cout << _("current"); break;
      case AvailableOPStatus: cout << _("available"); break;
      case ForbiddenOPStatus: cout << _("forbidden"); break;
      }
      cout << _("  Long name: '") << i->_longName << "' "
           << _("  Short name: '") << i->_shortName << "' "
           << _("  Numeric name: ") << i->_numericName << endl;
      ++count;
    }
    break;
  }
  case CurrentOperatorInfo:
  {
    OPInfo opi = m->getCurrentOPInfo();
    cout << "<CURROP0>"
         << _("  Long name: '") << opi._longName << "' "
         << _("  Short name: '") << opi._shortName << "' "
         << _("  Numeric name: ") << opi._numericName
         << _("  Mode: ");
    switch (opi._mode)
    {
    case AutomaticOPMode: cout << _("automatic"); break;
    case ManualOPMode: cout << _("manual"); break;
    case DeregisterOPMode: cout << _("deregister"); break;
    case ManualAutomaticOPMode: cout << _("manual/automatic"); break;
    }
    cout << endl;
    break;
  }
  case FacilityLockStateInfo:
  {
    int count = 0;
    vector<string> fclc = m->getFacilityLockCapabilities();
    for (vector<string>::iterator i = fclc.begin(); i != fclc.end(); ++i)
      if (*i != "AB" && *i != "AG" && *i != "AC")
      {
        cout << "<FLSTAT" << count <<  ">  '" << *i << "'";
        try
        {
          if (m->getFacilityLockStatus(*i, VoiceFacility))
            cout << _("  Voice");
        }
        catch (GsmException &e)
        {
          cout << _("  unknown");
        }
        try
        {
        if (m->getFacilityLockStatus(*i, DataFacility))
          cout << _("  Data");
        }
        catch (GsmException &e)
        {
          cout << _("  unknown");
        }
        try
        {
        if (m->getFacilityLockStatus(*i, FaxFacility))
          cout << _("  Fax");
        }
        catch (GsmException &e)
        {
          cout << _("  unknown");
        }
        cout << endl;
        ++count;
      }
    break;
  }
  case FacilityLockCapabilityInfo:
  {
    cout << "<FLCAP0>  ";
    vector<string> fclc = m->getFacilityLockCapabilities();
    for (vector<string>::iterator i = fclc.begin(); i != fclc.end(); ++i)
      cout << "'" << *i << "' ";
    cout << endl;
    break;
  }
  case PasswordInfo:
  {
    vector<PWInfo> pwi = m->getPasswords();
    int count = 0;
    for (vector<PWInfo>::iterator i = pwi.begin(); i != pwi.end(); ++i)
    {
      cout << "<PW" << count <<  ">  '"
           << i->_facility << "' " << i->_maxPasswdLen << endl;
      ++count;
    }
    break;
  }
  case CLIPInfo:
  {
    cout << "<CLIP0>  " << (m->getNetworkCLIP() ? _("on") : _("off")) << endl;
    break;
  }
  case CallForwardingInfo:
  {
    for (int r = 0; r < 4; ++r)
    {
      string text;
      switch (r)
      {
      case 0: text = _("UnconditionalReason"); break;
      case 1: text = _("MobileBusyReason"); break;
      case 2: text = _("NoReplyReason"); break;
      case 3: text = _("NotReachableReason"); break;
      }
      ForwardInfo voice, fax, data;
      m->getCallForwardInfo((ForwardReason)r, voice, fax, data);
      cout << "<FORW" << r << ".";
      printForwardReason("0>  " + text + _("  Voice"), voice);
      cout << "<FORW" << r << ".";
      printForwardReason("1>  " + text + _("  Data"), data);
      cout << "<FORW" << r << ".";
      printForwardReason("2>  " + text + _("  Fax"), fax);
    }
    break;
  }
  case BatteryInfo:
  {
    cout << "<BATT0>  ";
    int bcs = m->getBatteryChargeStatus();
    switch (bcs)
    {
    case 0: cout << _("0 ME is powered by the battery") << endl; break;
    case 1: cout << _("1 ME has a battery connected, but is not powered by it")
                 << endl; break;
    case 2: cout << _("2 ME does not have a battery connected") << endl; break;
    case 3:
      cout << _("3 Recognized power fault, calls inhibited") << endl;
      break;
    }
    cout << "<BATT1>  " << m->getBatteryCharge() << endl;
    break;
  }
  case BitErrorInfo:
  {
    cout << "<BITERR0>  " << m->getBitErrorRate() << endl;
    break;
  }
  case SCAInfo:
  {
    cout << "<SCA0>  " << m->getServiceCentreAddress() << endl;
    break;
  }
  case CharSetInfo:
  {
    cout << "<CSET0>  ";
    vector<string> cs = m->getSupportedCharSets();
    for (vector<string>::iterator i = cs.begin(); i != cs.end(); ++i)
      cout << "'" << *i << "' ";
    cout << endl;
    cout << "<CSET1>  '" << m->getCurrentCharSet() << "'" << endl;
    break;
  }
  case SignalInfo:
  {
    cout << "<SIG0>  " << m->getSignalStrength() << endl;
    break;
  }
  case PhonebookInfo:
  {
    cout << "<PBOOK0>  ";
    vector<string> pb = m->getSupportedPhonebooks();
    for (vector<string>::iterator i = pb.begin(); i != pb.end(); ++i)
      cout << "'" << *i << "' ";
    cout << endl;
    cout << "<PBOOK1>  '" << m->getCurrentPhonebook() << "'" << endl;
    break;
  }
  case SignalToneInfo:
  {
    cout << "<SIGNAL0>  ";
    IntRange st = m->getSupportedSignalTones();
    printIntRange(st);
    cout << endl;
//    cout << "<SIGT1>  '" << m->getCurrentSignalTone() << "'" << endl;
    break;
  }
  case RingingToneInfo:
  {
    cout << "<RING0>  ";
    IntRange rt = m->getSupportedRingingTones();
    printIntRange(rt);
    cout << endl;
    cout << "<RING1>  " << m->getCurrentRingingTone() << endl;
    break;
  }
  case BinaryInfo:
  {
    cout << "<BIN0>  ";
    vector<ParameterRange> bnr = m->getSupportedBinaryReads();
    for (vector<ParameterRange>::iterator i = bnr.begin(); i != bnr.end(); ++i)
    {
      printParameterRange(*i);
      cout << " ";
    }
    cout << endl;
    cout << "<BIN1>  ";
    vector<ParameterRange> bnw = m->getSupportedBinaryWrites();
    for (vector<ParameterRange>::iterator i = bnw.begin(); i != bnw.end(); ++i)
    {
      printParameterRange(*i);
      cout << " ";
    }
    cout << endl;
    break;
  }
  default:
    assert(0);
    break;
  }
}

// convert facility class string of the form "", "all", or any combination
// of "v" (voice), "d" (data), or "f" (fax) to numeric form

FacilityClass strToFacilityClass(string facilityClassS)
{
  facilityClassS = lowercase(facilityClassS);
  FacilityClass facilityClass = (FacilityClass)0;
  if (facilityClassS == "all" || facilityClassS == "")
    return (FacilityClass)ALL_FACILITIES;

  // OR in facility class bits
  for (unsigned int i = 0; i < facilityClassS.length(); ++i)
    if (facilityClassS[i] == 'v')
      facilityClass = (FacilityClass)(facilityClass | VoiceFacility);
    else if (facilityClassS[i] == 'd')
      facilityClass = (FacilityClass)(facilityClass | DataFacility);
    else if (facilityClassS[i] == 'f')
      facilityClass = (FacilityClass)(facilityClass | FaxFacility);
    else
      throw GsmException(
        stringPrintf(_("unknown facility class parameter '%c'"),
                     facilityClassS[i]), ParameterError);

  return facilityClass;
}

// check if argc - optind is in range min..max
// throw exception otherwise

void checkParamCount(int optind, int argc, int min, int max)
{
  int paramCount = argc - optind;
  if (paramCount < min)
    throw GsmException(stringPrintf(_("not enough parameters, minimum number "
                                      "of parameters is %d"), min),
                       ParameterError);
  else if (paramCount > max)
    throw GsmException(stringPrintf(_("too many parameters, maximum number "
                                      "of parameters is %d"), max),
                       ParameterError);
}

// *** main program

int main(int argc, char *argv[])
{
  try
  {
    // handle command line options
    string device = "/dev/mobilephone";
    string operation;
    string baudrate;
    string initString = DEFAULT_INIT_STRING;
    bool swHandshake = false;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "I:o:d:b:hvX", longOpts, &dummy))
          != -1)
      switch (opt)
      {
      case 'X':
        swHandshake = true;
        break;
      case 'I':
        initString = optarg;
        break;
      case 'd':
        device = optarg;
        break;
      case 'o':
        operation = optarg;
        break;
      case 'b':
        baudrate = optarg;
        break;
      case 'v':
        cerr << argv[0] << stringPrintf(_(": version %s [compiled %s]"),
                                        VERSION, __DATE__) << endl;
        exit(0);
        break;
      case 'h':
        cerr << argv[0] << _(": [-b baudrate][-d device][-h]"
                             "[-I init string][-o operation]\n"
                             "  [-v][-X]{parameters}") << endl
             << endl
             << _("  -b, --baudrate    baudrate to use for device "
                  "(default: 38400)")
             << endl
             << _("  -d, --device      sets the destination device to "
                  "connect to") << endl
             << _("  -h, --help        prints this message") << endl
             << _("  -I, --init        device AT init sequence") << endl
             << _("  -o, --operation   operation to perform on the mobile \n"
                  "                    phone with the specified parameters")
             << endl
             << _("  -v, --version     prints version and exits") << endl
             << _("  -X, --xonxoff     switch on software handshake") << endl
             << endl
             << _("  parameters        parameters to use for the operation\n"
                  "                    (if an operation is given) or\n"
                  "                    a specification which kind of\n"
                  "                    information to read from the mobile "
                  "phone")
             << endl << endl
             << _("Refer to gsmctl(1) for details on the available parameters"
                  " and operations.")
             << endl << endl;
        exit(0);
        break;
      case '?':
        throw GsmException(_("unknown option"), ParameterError);
        break;
      }

    // open the port and ME/TA
    m = new SieMe(new
#ifdef WIN32
                 Win32SerialPort
#else
                 UnixSerialPort
#endif
                 (device,
                  baudrate == "" ?
                  DEFAULT_BAUD_RATE :
                  baudRateStrToSpeed(baudrate),
                  initString, swHandshake));
    
    if (operation == "")
    {                           // process info parameters
      for (int i = optind; i < argc; ++i)
      {
        string param = lowercase(argv[i]);
        if (param == "all")
          for (int ip = MeInfo; ip <= SignalInfo; ++ip)
            printInfo((InfoParameter)ip);
        else if (param == "me")
          printInfo(MeInfo);
        else if (param == "op")
          printInfo(OperatorInfo);
        else if (param == "currop")
          printInfo(CurrentOperatorInfo);
        else if (param == "flstat")
          printInfo(FacilityLockStateInfo);
        else if (param == "flcap")
          printInfo(FacilityLockCapabilityInfo);
        else if (param == "pw")
          printInfo(PasswordInfo);
        else if (param == "clip")
          printInfo(CLIPInfo);
        else if (param == "forw")
          printInfo(CallForwardingInfo);
        else if (param == "batt")
          printInfo(BatteryInfo);
        else if (param == "biterr")
          printInfo(BitErrorInfo);
        else if (param == "sig")
          printInfo(SignalInfo);
        else if (param == "sca")
          printInfo(SCAInfo);
        else if (param == "cset")
          printInfo(CharSetInfo);
        else if (param == "pbook")
          printInfo(PhonebookInfo);
        else if (param == "signal")
          printInfo(SignalToneInfo);
        else if (param == "ring")
          printInfo(RingingToneInfo);
        else if (param == "binary")
          printInfo(BinaryInfo);
        else
          throw GsmException(
            stringPrintf(_("unknown information parameter '%s'"),
                         param.c_str()),
            ParameterError);
      }
    }
    else
    {                           // process operation
      operation = lowercase(operation);
      if (operation == "dial")
      {
        // dial: number
        checkParamCount(optind, argc, 1, 1);

        m->dial(argv[optind]);
        
        // wait for keypress from stdin
        char c;
        read(1, &c, 1);
      }
      else if (operation == "setop")
      {
        // setop: opmode numeric FIXME allow long and numeric too
        checkParamCount(optind, argc, 2, 2);
        string opmodeS = lowercase(argv[optind]);
        OPModes opmode;
        if (opmodeS == "automatic")
          opmode = AutomaticOPMode;
        else if (opmodeS == "manual")
          opmode = ManualOPMode;
        else if (opmodeS == "deregister")
          opmode = DeregisterOPMode;
        else if (opmodeS == "manualautomatic")
          opmode = ManualAutomaticOPMode;
        else
          throw GsmException(stringPrintf(_("unknown opmode parameter '%s'"),
                                          opmodeS.c_str()), ParameterError);

        m->setCurrentOPInfo(opmode, "" , "", checkNumber(argv[optind + 1]));
      }
      else if (operation == "lock")
      {
        // lock: facility [facilityclass] [passwd]
        checkParamCount(optind, argc, 1, 3);
        string passwd = (argc - optind == 3) ?
          (string)argv[optind + 2] : (string)"";
        
        m->lockFacility(argv[optind],
                        (argc - optind >= 2) ?
                        strToFacilityClass(argv[optind + 1]) :
                        (FacilityClass)ALL_FACILITIES,
                        passwd);
      }
      else if (operation == "unlock")
      {
        // unlock: facility [facilityclass] [passwd]
        checkParamCount(optind, argc, 1, 3);
        string passwd = argc - optind == 3 ? argv[optind + 2] : "";
        
        m->unlockFacility(argv[optind],
                          (argc - optind >= 2) ?
                          strToFacilityClass(argv[optind + 1]) :
                          (FacilityClass)ALL_FACILITIES,
                          passwd);
      }
      else if (operation == "setpw")
      {
        // set password: facility oldpasswd newpasswd
        checkParamCount(optind, argc, 1, 3);
        string oldPasswd = argc - optind >= 2 ? argv[optind + 1] : "";
        string newPasswd = argc - optind == 3 ? argv[optind + 2] : "";

        m->setPassword(argv[optind], oldPasswd, newPasswd);
      }
      else if (operation == "forw")
      {
        // call forwarding: mode reason number [facilityclass] [forwardtime]
        checkParamCount(optind, argc, 2, 5);

        // get optional parameters facility class and forwardtime
        int forwardTime = argc - optind == 5 ? checkNumber(argv[optind + 4]) :
          NOT_SET;
        FacilityClass facilityClass =
          argc - optind >= 4 ? strToFacilityClass(argv[optind + 3]) :
          (FacilityClass)ALL_FACILITIES;
        
        // get forward reason
        string reasonS = lowercase(argv[optind + 1]);
        ForwardReason reason;
        if (reasonS == "unconditional")
          reason = UnconditionalReason;
        else if (reasonS == "mobilebusy")
          reason = MobileBusyReason;
        else if (reasonS == "noreply")
          reason = NoReplyReason;
        else if (reasonS == "notreachable")
          reason = NotReachableReason;
        else if (reasonS == "all")
          reason = AllReasons;
        else if (reasonS == "allconditional")
          reason = AllConditionalReasons;
        else
          throw GsmException(
            stringPrintf(_("unknown forward reason parameter '%s'"),
                         reasonS.c_str()), ParameterError);
        
        // get mode
        string modeS = lowercase(argv[optind]);
        ForwardMode mode;
        if (modeS == "disable")
          mode = DisableMode;
        else if (modeS == "enable")
          mode = EnableMode;
        else if (modeS == "register")
          mode = RegistrationMode;
        else if (modeS == "erase")
          mode = ErasureMode;
        else
          throw GsmException(
            stringPrintf(_("unknown forward mode parameter '%s'"),
                         modeS.c_str()), ParameterError);

        m->setCallForwarding(reason, mode,
                             (argc - optind >= 3) ? argv[optind + 2] : "",
                             "", // subaddr
                             facilityClass, forwardTime);
      }
      else if (operation == "setsca")
      {
        // set sca: number
        checkParamCount(optind, argc, 1, 1);
        m->setServiceCentreAddress(argv[optind]);
      }
      else if (operation == "cset")
      {
        // set charset: string
        checkParamCount(optind, argc, 1, 1);
        m->setCharSet(argv[optind]);
      }
      else if (operation == "signal")
      {
        // play signal tone: int
        checkParamCount(optind, argc, 1, 1);
        int tone = atoi(argv[optind]);
        m->playSignalTone(tone);
      }
      else if (operation == "setrt")
      {
        // set ringing tone: int int
        checkParamCount(optind, argc, 2, 2);
        int tone = atoi(argv[optind]);
        int volume = atoi(argv[optind + 1]);
        m->setRingingTone(tone, volume);
      }
      else if (operation == "playrt")
      {
        // play/stop ringing tone
        m->toggleRingingTone();
      }
      else
         throw GsmException(stringPrintf(_("unknown operation '%s'"),
                                         operation.c_str()), ParameterError);
    }
  }
  catch (GsmException &ge)
  {
    cerr << argv[0] << _("[ERROR]: ") << ge.what() << endl;
    return 1;
  }
  return 0;
}

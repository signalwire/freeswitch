// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmsiexfer.cc
// *
// * Purpose: Siemens ME file transfer program
// *
// * Author:  Christian W. Zuckschwerdt  <zany@triq.net>
// *
// * Created: 2001-12-16
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <string>
#ifdef WIN32
#include <gsmlib/gsm_win32_serial.h>
#else
#include <gsmlib/gsm_unix_serial.h>
#include <unistd.h>
#endif
#if defined(HAVE_GETOPT_LONG) || defined(WIN32)
#include <getopt.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <gsmlib/gsm_me_ta.h>
#include <gsm_sie_me.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sorted_phonebook.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"destination", required_argument, (int*)NULL, 'd'},
  {"source", required_argument, (int*)NULL, 's'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"type", required_argument, (int*)NULL, 't'},
  {"subtype", required_argument, (int*)NULL, 'i'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {"verbose", no_argument, (int*)NULL, 'V'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

// I f*ck up this file IO thing.

// read binary object from stdin
BinaryObject readBinaryFile(istream &ifs, string filename)
{
  size_t size = 10000; // Bad coder, no biscuits!
  BinaryObject bnr;
  bnr._data = new unsigned char[size];
  ifs.read((char*)bnr._data, size);
  bnr._size = ifs.gcount();
  return bnr;
}

// read binary object from file
BinaryObject readFile(string filename)
{
  // open the file
  ifstream ifs(filename.c_str());
  if (ifs.bad())
    throw GsmException(stringPrintf(_("cannot open file '%s'"),
                                    filename.c_str()),
                       OSError);
  // and read the file
  return readBinaryFile(ifs, filename);
}

// read binary object from stdin
BinaryObject readFile(bool fromStdin)
{
  // read from stdin
//  if (fromStdin)
  return readBinaryFile(cin, (string)_("<STDIN>"));
}

// write binary object to file
void writeBinaryFile(ostream &ofs, string filename, BinaryObject bnw)
{
  // well just dump the data
  ofs.write((char*)bnw._data, bnw._size);
}

// write binary object
void writeFile(string filename, BinaryObject obj)
{
  // open the file
  ofstream ofs(filename.c_str());
  if (ofs.bad())
    throw GsmException(stringPrintf(_("cannot open file '%s'"),
                                    filename.c_str()),
                       OSError);
  // and read the file
  writeBinaryFile(ofs, filename, obj);
}

// write binary object to stdout
void writeFile(bool toStdout, BinaryObject obj)
{
//  if (toStdout)
  writeBinaryFile(cout, (string)_("<STDIN>"), obj);
}

// *** main program

int main(int argc, char *argv[])
{
  try
  {
    // handle command line options
    string destination;
    string source;
    string baudrate;
    string type;
    string subtype;
    int subtypeN;
    bool verbose = false;
    string initString = DEFAULT_INIT_STRING;
    bool swHandshake = false;
    Ref<SieMe> sourceMeTa, destMeTa;
    BinaryObject sourceObject;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "XI:s:d:b:hvVt:i:", longOpts,
                             &dummy))
          != -1)
      switch (opt)
      {
      case 'X':
        swHandshake = true;
        break;
      case 'I':
        initString = optarg;
        break;
      case 'V':
        verbose = true;
        break;
      case 't':
        type = optarg;
        break;
      case 'i':
        subtype = optarg;
	subtypeN = atoi(optarg);
        break;
      case 'd':
        destination = optarg;
        break;
      case 's':
        source = optarg;
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
        cerr << argv[0] << _(": [-b baudrate][-c][-d device or file][-h]"
                             "[-I init string]\n"
                             "  [-p phonebook name][-s device or file]"
                             "[-t charset][-v]"
                             "[-V][-y][-X]") << endl
             << endl
             << _("  -b, --baudrate    baudrate to use for device "
                  "(default: 38400)")
             << endl
             << _("  -c, --copy        copy source entries to destination")
             << endl
             << _("  -d, --destination sets the destination device to "
                  "connect \n"
                  "                    to, or the file to write") << endl
             << _("  -D, --destination-backend sets the destination backend")
             << endl
             << _("  -h, --help        prints this message") << endl
             << _("  -i, --index       takes index positions into account")
             << endl
             << _("  -I, --init        device AT init sequence") << endl
             << _("  -p, --phonebook   name of phonebook to use") << endl
             << _("  -s, --source      sets the source device to connect to,\n"
                  "                    or the file to read") << endl
             << _("  -t, --charset     sets the character set to use for\n"
                  "                    phonebook entries") << endl
             << _("  -S, --source-backend sets the source backend")
             << endl
             << _("  -v, --version     prints version and exits") << endl
             << _("  -V, --verbose     print detailed progress messages")
             << endl
             << _("  -X, --xonxoff     switch on software handshake") << endl
             << _("  -y, --synchronize synchronize destination with source\n"
                  "                    entries (destination is overwritten)\n"
                  "                    (see gsmpb(1) for details)")
             << endl << endl;
        exit(0);
        break;
      case '?':
        throw GsmException(_("unknown option"), ParameterError);
        break;
      }

    // check if all parameters all present
    if (destination == "" || source == "")
      throw GsmException(_("both source and destination must be given"),
                         ParameterError);

    // start accessing source mobile phone or file
    if (source == "-")
      sourceObject = readFile(true);
    else if (isFile(source))
      sourceObject = readFile(source);
    else
    {
      if (type == "")
        throw GsmException(_("type be given"), ParameterError);
      if (subtype == "")
        throw GsmException(_("subtype be given"), ParameterError);

      sourceMeTa = new SieMe(new
#ifdef WIN32
                            Win32SerialPort
#else
                            UnixSerialPort
#endif
                            (source,
                             baudrate == "" ? DEFAULT_BAUD_RATE :
                             baudRateStrToSpeed(baudrate), initString,
                             swHandshake));
      sourceObject = sourceMeTa->getBinary(type, subtypeN);
    }

    // make sure destination.c_str file exists
    if (destination != "")
    {
      try
      {
        ofstream f(destination.c_str(), ios::out | ios::app);
      }
      catch (exception)
      {
      }
    }

    // start accessing destination mobile phone or file
    if (destination == "-")
      writeFile(true, sourceObject);
    else if (isFile(destination))
      writeFile(destination, sourceObject);
    else
    {
      if (type == "")
        throw GsmException(_("type must be given"), ParameterError);
      if (subtype == "")
        throw GsmException(_("subtype must be given"), ParameterError);

      destMeTa = new SieMe(new 
#ifdef WIN32
                          Win32SerialPort
#else
                          UnixSerialPort
#endif
                          (destination,
                           baudrate == "" ? DEFAULT_BAUD_RATE :
                           baudRateStrToSpeed(baudrate), initString,
                           swHandshake));
      destMeTa->setBinary(type, subtypeN, sourceObject);
    }
  }
  catch (GsmException &ge)
  {
    cerr << argv[0] << _("[ERROR]: ") << ge.what() << endl;
    return 1;
  }
  return 0;
}

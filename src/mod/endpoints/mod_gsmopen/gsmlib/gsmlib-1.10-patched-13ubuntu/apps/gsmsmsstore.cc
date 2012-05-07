// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmsmsstore.cc
// *
// * Purpose: SMS store management program
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.8.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <string>
#include <ctype.h>
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
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sorted_sms_store.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"store", required_argument, (int*)NULL, 't'},
  {"erase", no_argument, (int*)NULL, 'e'},
  {"add", no_argument, (int*)NULL, 'a'},
  {"list", no_argument, (int*)NULL, 'l'},
  {"destination", required_argument, (int*)NULL, 'd'},
  {"source", required_argument, (int*)NULL, 's'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"sca", required_argument, (int*)NULL, 'C'},
  {"copy", no_argument, (int*)NULL, 'c'},
  {"delete", no_argument, (int*)NULL, 'x'},
  {"backup", no_argument, (int*)NULL, 'k'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {"verbose", no_argument, (int*)NULL, 'V'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

bool verbose = false;           // true if --verbose option given

// type of operation to perform

enum Operation {CopyOp = 'c', BackupOp = 'k', DeleteOp = 'x',
                AddOp = 'a', ListOp = 'l', NoOp = 0};

// aux function, insert entry only if not already present in dest

void backup(SortedSMSStoreRef destStore, SMSStoreEntry &entry)
{
  // the following only works because we know that the default sort order
  // is by date
  assert(destStore->sortOrder() == ByDate);

  Timestamp date = entry.message()->serviceCentreTimestamp();
  pair<SortedSMSStore::iterator, SortedSMSStore::iterator> range =
    destStore->equal_range(date);
          
  for (SortedSMSStore::iterator j = range.first;
       j != range.second; ++j)
    if (entry == *j)
      // do nothing if the entry is already present in the destination
      return;

  if (verbose)
    cout << stringPrintf(_("inserting entry #%d from source into destination"),
                         entry.index()) << endl
         << entry.message()->toString();
  destStore->insert(entry);     // insert
}

// aux function, throw exception if operation != NoOp

void checkNoOp(Operation operation, int opt)
{
  if (operation != NoOp)
    throw GsmException(stringPrintf(_("incompatible options '%c' and '%c'"),
                                    (char)operation, (char)opt),
                       ParameterError);
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
    string storeName;
    char operation = NoOp;
    SortedSMSStoreRef sourceStore, destStore;
    bool useIndices = false;    // use indices in delete, copy, backup op
    string initString = DEFAULT_INIT_STRING;
    bool swHandshake = false;
    // service centre address (set on command line)
    string serviceCentreAddress;
    Ref<MeTa> sourceMeTa, destMeTa;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "I:t:s:d:b:cxlakhvVXC:",
                             longOpts, &dummy))
          != -1)
      switch (opt)
      {
      case 'C':
        serviceCentreAddress = optarg;
        break;
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
        storeName = optarg;
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
      case 'c':
        checkNoOp((Operation)operation, opt);
        operation = CopyOp;
        break;
      case 'x':
        checkNoOp((Operation)operation, opt);
        operation = DeleteOp;
        break;
      case 'l':
        checkNoOp((Operation)operation, opt);
        operation = ListOp;
        break;
      case 'a':
        checkNoOp((Operation)operation, opt);
        operation = AddOp;
        break;
      case 'k':
        checkNoOp((Operation)operation, opt);
        operation = BackupOp;
        break;
      case 'v':
        cerr << argv[0] << stringPrintf(_(": version %s [compiled %s]"),
                                        VERSION, __DATE__) << endl;
        exit(0);
        break;
      case 'h':
        cerr << argv[0] << _(": [-a][-b baudrate][-c][-C sca]"
                             "[-d device or file]\n"
                             "  [-h][-I init string][-k][-l]"
                             "[-s device or file]"
                             "[-t SMS store name]\n  [-v][-V][-x][-X]"
                             "{indices}|[phonenumber text]") << endl
             << endl
             << _("  -a, --add         add new SMS submit message\n"
                  "                    (phonenumber and text) to destination")
             << endl
             << _("  -b, --baudrate    baudrate to use for device "
                  "(default: 38400)")
             << endl
             << _("  -c, --copy        copy source entries to destination\n"
                  "                    (if indices are given, "
                  "copy only these entries)") << endl
             << _("  -C, --sca         SMS service centre address") << endl
             << _("  -d, --destination sets the destination device to\n"
                  "                    connect to, or the file to write to")
             << endl
             << _("  -h, --help        prints this message") << endl
             << _("  -I, --init        device AT init sequence") << endl
             << _("  -k, --backup      backup new entries to destination\n"
                  "                    (if indices are given, "
                  "copy only these entries)") << endl
             << _("  -l, --list        list source to stdout") << endl
             << _("  -s, --source      sets the source device to connect to,\n"
                  "                    or the file to read") << endl
             << _("  -t, --store       name of SMS store to use") << endl
             << _("  -v, --version     prints version and exits") << endl
             << _("  -V, --verbose     print detailed progress messages")
             << endl
             << _("  -x, --delete      delete entries denoted by indices")
             << endl
             << _("  -X, --xonxoff     switch on software handshake") << endl
             << endl;
        exit(0);
        break;
      case '?':
        throw GsmException(_("unknown option"), ParameterError);
        break;
      }

    // check if parameters are complete
    if (operation == NoOp)
      throw GsmException(_("no operation option given"), ParameterError);
    if (operation == BackupOp || operation == CopyOp)
      if (destination.length() == 0 || source.length() == 0)
        throw GsmException(_("both source and destination required"),
                           ParameterError);
    if (operation == ListOp)
    {
      if (destination.length() != 0)
        throw GsmException(_("destination must not be given"), ParameterError);
      if (source.length() == 0)
        throw GsmException(_("source required"), ParameterError);
    }
    if (operation == AddOp || operation == DeleteOp)
    {
      if (source.length() != 0)
        throw GsmException(_("source must not be given"), ParameterError);
      if (destination.length() == 0)
        throw GsmException(_("destination required"), ParameterError);
    }
    if (operation == CopyOp || operation == DeleteOp || operation == BackupOp)
    {
      // check if all indices are numbers
      for (int i = optind; i < argc; ++i)
        for (char *pp = argv[i]; *pp != 0; ++pp)
          if (! isdigit(*pp))
            throw GsmException(stringPrintf(_("expected number, got '%s'"),
                                            argv[i]), ParameterError);
      useIndices = optind != argc;
    }
    else if (operation == AddOp)
    {
      if (optind + 2 < argc)
        throw GsmException(_("more than two parameters given"),
                           ParameterError);
      if (optind + 2 > argc)
        throw GsmException(_("not enough parameters given"),
                           ParameterError);
    }
    else
      if (optind != argc)
        throw GsmException(_("unexpected parameters"), ParameterError);
    
    // start accessing source store or file if required by operation
    if (operation == CopyOp || operation == BackupOp || operation == ListOp)
      if (source == "-")
        sourceStore = new SortedSMSStore(true);
      else if (isFile(source))
        sourceStore = new SortedSMSStore(source);
      else
      {
        if (storeName == "")
          throw GsmException(_("store name must be given"), ParameterError);
        
        sourceMeTa = new MeTa(new
#ifdef WIN32
                              Win32SerialPort
#else
                              UnixSerialPort
#endif
                              (source,
                               baudrate == "" ? DEFAULT_BAUD_RATE :
                               baudRateStrToSpeed(baudrate), initString,
                               swHandshake));
        sourceStore = new SortedSMSStore(sourceMeTa->getSMSStore(storeName));
      }
      
    // make sure destination file exists if specified
    // Use isFile() for its exception-throwing properties, and discard
    // return value cos we don't care (yet) whether it's a device or a
    // regular file.
    if (destination != "")
      isFile(destination);

    // start accessing destination store or file
    if (operation == CopyOp || operation == BackupOp || operation == AddOp ||
        operation == DeleteOp)
      if (destination == "-")
        destStore = new SortedSMSStore(false);
      else if (isFile(destination))
        destStore = new SortedSMSStore(destination);
      else
      {
        if (storeName == "")
          throw GsmException(_("store name must be given"), ParameterError);
        
        destMeTa = new MeTa(new
#ifdef WIN32
                            Win32SerialPort
#else
                            UnixSerialPort
#endif
                            (destination,
                             baudrate == "" ? DEFAULT_BAUD_RATE :
                             baudRateStrToSpeed(baudrate), initString,
                             swHandshake));
        destStore = new SortedSMSStore(destMeTa->getSMSStore(storeName));      
      }

    // now do the actual work
    switch (operation)
    {
    case BackupOp:
    {
      sourceStore->setSortOrder(ByIndex); // needed in loop

      if (useIndices)
        for (int i = optind; i < argc; ++i)
        {
          SortedSMSStore::iterator j = sourceStore->find(atoi(argv[i]));
          if (j == sourceStore->end())
            throw GsmException(stringPrintf(_("no index '%s' in source"),
                                            argv[i]), ParameterError);
          backup(destStore, *j);
        }
      else
        for (SortedSMSStore::iterator i = sourceStore->begin();
             i != sourceStore->end(); ++i)
          backup(destStore, *i);
      break;
    }
    case CopyOp:
    {                        
      destStore->clear();
      if (! useIndices)         // copy all entries
      {
        for (SortedSMSStore::iterator i = sourceStore->begin();
             i != sourceStore->end(); ++i)
        {
          if (verbose)
            cout << stringPrintf(_("inserting entry #%d from source "
                                   "into destination"), i->index()) << endl
                 << i->message()->toString();
          destStore->insert(*i);
        }
      }
      else                      // copy indexed entries
      {
        sourceStore->setSortOrder(ByIndex); // needed in loop

        for (int i = optind; i < argc; ++i)
        {
          SortedSMSStore::iterator j = sourceStore->find(atoi(argv[i]));
          if (j == sourceStore->end())
            throw GsmException(stringPrintf(_("no index '%s' in source"),
                                            argv[i]), ParameterError);
          if (verbose)
            cout << stringPrintf(_("inserting entry #%d from source into "
                                   "destination"), j->index()) << endl
                 << j->message()->toString();
          destStore->insert(*j);
        }
      }
      break;
    }
    case ListOp:
    {
      for (SortedSMSStore::iterator i = sourceStore->begin();
           i != sourceStore->end(); ++i)
        cout << stringPrintf(_("index #%d"), i->index()) << endl
             << i->message()->toString();
      break;
    }
    case AddOp:
    {
      SMSMessageRef sms = new SMSSubmitMessage(argv[optind + 1], argv[optind]);
      // set service centre address in new submit PDU if requested by user
      if (serviceCentreAddress != "")
      {
        Address sca(serviceCentreAddress);
        sms->setServiceCentreAddress(sca);
      }
      if (verbose)
        cout << _("inserting new entry into destination") << endl
             << sms->toString();
      destStore->insert(sms);
      break;
    }
    case DeleteOp:
    {
      destStore->setSortOrder(ByIndex);
      for (int i = optind; i < argc; ++i)
      {
        int index = atoi(argv[i]);
        if (verbose)
        {
          SortedSMSStore::iterator e = destStore->find(index);
          if (e != destStore->end())
            cout << stringPrintf(_("deleting entry #%d from destination"),
                                 index) << endl
                 << e->message()->toString();
        }
        if (destStore->erase(index) != 1)
          throw GsmException(stringPrintf(_("no index '%s' in destination"),
                                          argv[i]), ParameterError);
      }
      break;
    }
    }
  }
  catch (GsmException &ge)
  {
    cerr << argv[0] << _("[ERROR]: ") << ge.what() << endl;
    return 1;
  }
  return 0;
}

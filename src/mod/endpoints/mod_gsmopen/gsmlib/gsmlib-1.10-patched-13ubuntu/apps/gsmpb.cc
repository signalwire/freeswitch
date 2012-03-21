// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsmpb.cc
// *
// * Purpose: phonebook management program
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 24.6.1999
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
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sorted_phonebook.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

#ifdef HAVE_GETOPT_LONG
static struct option longOpts[] =
{
  {"xonxoff", no_argument, (int*)NULL, 'X'},
  {"phonebook", required_argument, (int*)NULL, 'p'},
  {"init", required_argument, (int*)NULL, 'I'},
  {"destination", required_argument, (int*)NULL, 'd'},
  {"source", required_argument, (int*)NULL, 's'},
  {"destination-backend", required_argument, (int*)NULL, 'D'},
  {"source-backend", required_argument, (int*)NULL, 'S'},
  {"baudrate", required_argument, (int*)NULL, 'b'},
  {"charset", required_argument, (int*)NULL, 't'},
  {"copy", no_argument, (int*)NULL, 'c'},
  {"synchronize", no_argument, (int*)NULL, 'y'},
  {"help", no_argument, (int*)NULL, 'h'},
  {"version", no_argument, (int*)NULL, 'v'},
  {"verbose", no_argument, (int*)NULL, 'V'},
  {"indexed", no_argument, (int*)NULL, 'i'},
  {(char*)NULL, 0, (int*)NULL, 0}
};
#else
#define getopt_long(argc, argv, options, longopts, indexptr) \
  getopt(argc, argv, options)
#endif

// insert those entries from sourcePhonebook into destPhonebook
// that are not already present in destPhonebook

void insertNotPresent(SortedPhonebookRef sourcePhonebook,
                      SortedPhonebookRef destPhonebook,
                      bool indexed, bool verbose)
{
  for (SortedPhonebookBase::iterator i = sourcePhonebook->begin();
       i != sourcePhonebook->end(); ++i)
  {
    pair<SortedPhonebookBase::iterator, SortedPhonebookBase::iterator> range;
    if (indexed)
    {
      int index = i->index();
      range = destPhonebook->equal_range(index);
    }
    else
    {
      string text = i->text();
      range = destPhonebook->equal_range(text);
    }

    // do nothing if the entry is already present in the destination
    bool alreadyPresent = false;
    for (SortedPhonebookBase::iterator j = range.first;
         j != range.second; ++j)
    {
      i->setUseIndex(indexed);
      if (i->telephone() == j->telephone())
      {
        alreadyPresent = true;
        break;
      }
    }
    // ... else insert it
    if (! alreadyPresent)
    {
      if (verbose)
      {
        cout << stringPrintf(_("inserting '%s' tel# %s"),
                             i->text().c_str(), i->telephone().c_str());
        if (indexed)
          cout << stringPrintf(_(" (index #%d)"), i->index());
        cout << endl;
      }
      i->setUseIndex(indexed);
      destPhonebook->insert(*i); // insert
    }
  }
}

// update those entries in destPhonebook, that
// - have the same name as one entry in destPhonebook
// - but have a different telephone number
// this is only done if the name in question is unique in the destPhonebook
// the case of several entries having the same in the sourcePhonebook
// is handled - only the first is considered for updating

void updateEntries(SortedPhonebookRef sourcePhonebook,
                   SortedPhonebookRef destPhonebook,
                   bool verbose)
{
  bool firstLoop = true;
  string lastText;

  for (SortedPhonebookBase::iterator i = sourcePhonebook->begin();
       i != sourcePhonebook->end(); ++i)
  {
    string text = i->text();
    if (! firstLoop && text != lastText)
    {
      pair<SortedPhonebookBase::iterator,
        SortedPhonebookBase::iterator> range =
        destPhonebook->equal_range(text);
      
      SortedPhonebookBase::iterator first = range.first;
      if (first != destPhonebook->end() && range.second == ++first)
      {                         // just one text in the destPhonebook
        if (! (*range.first == *i)) // overwrite if different in destination
        {
          if (verbose)
            cout << stringPrintf(_("updating '%s' tel# %s to new tel# %s"),
                                 range.first->text().c_str(),
                                 range.first->telephone().c_str(),
                                 i->telephone().c_str())
                 << endl;
          
          *range.first = *i;
        }
      }
      lastText = text;
    }
    firstLoop = false;
  }
}

// the same but for indexed phonebooks

void updateEntriesIndexed(SortedPhonebookRef sourcePhonebook,
                          SortedPhonebookRef destPhonebook,
                          bool verbose)
{
  for (SortedPhonebookBase::iterator i = sourcePhonebook->begin();
       i != sourcePhonebook->end(); ++i)
  {
    int index = i->index();
    
    SortedPhonebookBase::iterator j = destPhonebook->find(index);
    
    if (j != destPhonebook->end())
    {                           // index present in the destPhonebook
      if (! (*j == *i))         // overwrite if different in destination
      {
        if (verbose)
          cout << stringPrintf(_("updating '%s' tel# %s to new tel# %s"
                                 "(index %d)"),
                               j->text().c_str(),
                               j->telephone().c_str(),
                               i->telephone().c_str(), i->index())
               << endl;
        
        *j = *i;
      }
    }
  }
}

// delete those entries from destPhonebook, that are not present
// in sourcePhonebook

void deleteNotPresent(SortedPhonebookRef sourcePhonebook,
                      SortedPhonebookRef destPhonebook,
                      bool indexed, bool verbose)
{
  for (SortedPhonebookBase::iterator i = destPhonebook->begin();
       i != destPhonebook->end(); ++i)
  {
    pair<SortedPhonebookBase::iterator, SortedPhonebookBase::iterator> range;
    if (indexed)
    {
      int index = i->index();
      range = sourcePhonebook->equal_range(index);
    }
    else
    {
      string text = i->text();
      range = sourcePhonebook->equal_range(text);
    }
        
    bool found = false;
    for (SortedPhonebookBase::iterator j = range.first;
         j != range.second; ++j)
    {
      i->setUseIndex(indexed);
      if (j->telephone() == i->telephone())
      {
        found = true;
        break;
      }
    }
    if (! found)
    {
      if (verbose)
      {
        cout << stringPrintf(_("deleting '%s' tel# %s"),
                             i->text().c_str(), i->telephone().c_str());
        if (indexed)
          cout << stringPrintf(_(" (index #%d)"), i->index());
        cout << endl;
      }
      destPhonebook->erase(i);
#ifdef BUGGY_MAP_ERASE
	  deleteNotPresent(sourcePhonebook, destPhonebook, indexed, verbose);
	  return;
#endif
    }
  }
}

// *** main program

int main(int argc, char *argv[])
{
  try
  {
    // handle command line options
    string destination;
    string source;
    string destinationBackend;
    string sourceBackend;
    string baudrate;
    bool doSynchronize = true;
    string phonebook;
    SortedPhonebookRef sourcePhonebook, destPhonebook;
    bool verbose = false;
    bool indexed = false;
    string initString = DEFAULT_INIT_STRING;
    bool swHandshake = false;
    string charSet;
    Ref<MeTa> sourceMeTa, destMeTa;

    int opt;
    int dummy;
    while((opt = getopt_long(argc, argv, "I:p:s:d:b:cyhvViD:S:Xt:", longOpts,
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
      case 'p':
        phonebook = optarg;
        break;
      case 'd':
        destination = optarg;
        break;
      case 's':
        source = optarg;
        break;
      case 'D':
        destinationBackend = optarg;
        break;
      case 'S':
        sourceBackend = optarg;
        break;
      case 'b':
        baudrate = optarg;
        break;
      case 't':
        charSet = optarg;
        break;
      case 'c':
        doSynchronize = false;
        break;
      case 'i':
        indexed = true;
        break;
      case 'y':
        doSynchronize = true;
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
    if (sourceBackend != "")
      sourcePhonebook =
        CustomPhonebookRegistry::createPhonebook(sourceBackend, source);
    else if (source == "-")
      sourcePhonebook = new SortedPhonebook(true, indexed);
    else if (isFile(source))
      sourcePhonebook = new SortedPhonebook(source, indexed);
    else
    {
      if (phonebook == "")
        throw GsmException(_("phonebook name must be given"), ParameterError);

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
      if (charSet != "")
        sourceMeTa->setCharSet(charSet);
      sourcePhonebook =
        new SortedPhonebook(sourceMeTa->getPhonebook(phonebook));
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
    if (destinationBackend != "")
      destPhonebook =
        CustomPhonebookRegistry::createPhonebook(destinationBackend,
                                                 destination);
    else if (destination == "-")
      destPhonebook = new SortedPhonebook(false, indexed);
    else if (isFile(destination))
      destPhonebook = new SortedPhonebook(destination, indexed);
    else
    {
      if (phonebook == "")
        throw GsmException(_("phonebook name must be given"), ParameterError);

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
      if (charSet != "")
        destMeTa->setCharSet(charSet);
      PhonebookRef destPb = destMeTa->getPhonebook(phonebook);

      // check maximum lengths of source text and phonenumber when writing to
      // mobile phone
      unsigned int maxTextLen = destPb->getMaxTextLen();
      unsigned int maxTelLen = destPb->getMaxTelephoneLen();

      for (SortedPhonebookBase::iterator i = sourcePhonebook->begin();
           i != sourcePhonebook->end(); ++i)
        if (i->text().length() > maxTextLen)
          throw GsmException(
            stringPrintf(_("text '%s' is too large to fit into destination "
                           "(maximum size %d characters)"),
                         i->text().c_str(), maxTextLen),
            ParameterError);
        else if (i->telephone().length() > maxTelLen)
          throw GsmException(
            stringPrintf(_("phone number '%s' is too large to fit into "
                           "destination (maximum size %d characters)"),
                         i->telephone().c_str(), maxTelLen),
            ParameterError);

      // read phonebook
      destPhonebook = new SortedPhonebook(destPb);      
    }

    // now do the actual work
    if (doSynchronize)
    {                           // synchronizing
      if (indexed)
      {
        sourcePhonebook->setSortOrder(ByIndex);
        destPhonebook->setSortOrder(ByIndex);
        // for an explanation see below
        updateEntriesIndexed(sourcePhonebook, destPhonebook, verbose);
        deleteNotPresent(sourcePhonebook, destPhonebook, true, verbose);
        insertNotPresent(sourcePhonebook, destPhonebook, true, verbose);
      }
      else
      {
        sourcePhonebook->setSortOrder(ByText);
        destPhonebook->setSortOrder(ByText);
        // the following is done to avoid superfluous writes to the TA
        // (that takes time) and keep updated (ie. telephone number changed)
        // entries at the same place
        // 1. update entries in place where just the number changed
        updateEntries(sourcePhonebook, destPhonebook, verbose);
        // 2. delete those that are not present anymore
        deleteNotPresent(sourcePhonebook, destPhonebook, false, verbose);
        // 3. insert the new ones
        insertNotPresent(sourcePhonebook, destPhonebook, false, verbose);
      }
    }
    else
    {                           // copying
      destPhonebook->clear();
      for (SortedPhonebookBase::iterator i = sourcePhonebook->begin();
           i != sourcePhonebook->end(); ++i)
      {
        if (verbose)
        {
          cout << stringPrintf(_("inserting '%s' tel# %s"),
                               i->text().c_str(), i->telephone().c_str());
          if (indexed)
            cout << stringPrintf(_(" (index #%d)"), i->index());
          cout << endl;
        }
        destPhonebook->insert(*i);
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

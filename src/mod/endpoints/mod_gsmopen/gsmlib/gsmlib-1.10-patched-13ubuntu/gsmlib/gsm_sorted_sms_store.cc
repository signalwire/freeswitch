// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sorted_sms_store.cc
// *
// * Purpose: Sorted SMS store (residing in files or in the ME)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 14.8.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_sysdep.h>
#include <gsmlib/gsm_sorted_sms_store.h>
#include <iostream>
#include <fstream>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

using namespace std;
using namespace gsmlib;

// SMS message file format:
// version number of file format, unsigned short int, 2 bytes in network byte
// order
// then comes the message:
// 1. length of PDU (see 4. below): unsigned short int,
//    2 bytes in network byte order
// 2. index of message, unique for this file: unsigned long,
//    4 bytes in network byte order
// 3. MessageType (1 byte), any of:
//    0 SMS_DELIVER
//    1 SMS_SUBMIT
//    2 SMS_STATUS_REPORT
// 4. PDU in hexadecimal format

static const unsigned short int SMS_STORE_FILE_FORMAT_VERSION = 1;

// SortedSMSStore members

// aux function read bytes with error handling
// return false if EOF
static bool readnbytes(string &filename,
                       istream &is, int len, char *buf,
                       bool eofIsError = true) throw(GsmException)
{
  is.read(buf, len);
  if (is.bad() || (is.eof() && eofIsError))
    throw GsmException(stringPrintf(_("error reading from file '%s'"),
                                    (filename == "" ? _("<STDIN>") :
                                     filename.c_str())), OSError);
  return ! is.eof();
}

// aux function write bytes with error handling
static void writenbytes(string &filename, ostream &os,
                        int len, const char *buf) throw(GsmException)
{
  os.write(buf, len);
  if (os.bad())
    throw GsmException(stringPrintf(_("error writing to file '%s'"),
                                    (filename == "" ? _("<STDOUT>") :
                                     filename.c_str())), OSError);
}

void SortedSMSStore::readSMSFile(istream &pbs, string filename)
  throw(GsmException)
{
  char numberBuf[4];

  // check the version
  try
  {
    readnbytes(filename, pbs, 2, numberBuf);
  }
  catch (GsmException &ge)
  {
    // ignore error, file might be empty initially
  }
  unsigned_int_2 version = ntohs(*((unsigned_int_2*)numberBuf));
  if (! pbs.eof() && version != SMS_STORE_FILE_FORMAT_VERSION)
    throw GsmException(stringPrintf(_("file '%s' has wrong version"),
                                    filename.c_str()), ParameterError);

  // read entries
  while (1)
  {
    // read PDU length and exit loop if EOF
    if (! readnbytes(filename, pbs, 2, numberBuf, false))
      break;

    unsigned_int_2 pduLen = ntohs(*((unsigned_int_2*)numberBuf));
    if (pduLen > 500)
      throw GsmException(stringPrintf(_("corrupt SMS store file '%s'"),
                                      filename.c_str()), ParameterError);

    // read reserved integer field of message (was formerly index)
    readnbytes(filename, pbs, 4, numberBuf);
    //unsigned_int_4 reserved = ntohl(*((unsigned_int_4*)numberBuf));
    
    // read message type
    readnbytes(filename, pbs, 1, numberBuf);
    SMSMessage::MessageType messageType =
      (SMSMessage::MessageType)numberBuf[0];
    if (messageType > 2)
      throw GsmException(stringPrintf(_("corrupt SMS store file '%s'"),
                                      filename.c_str()), ParameterError);

    char *pduBuf = (char*)alloca(sizeof(char) * pduLen);

    // read pdu
    readnbytes(filename, pbs, pduLen, pduBuf);
    SMSMessageRef message =
      SMSMessage::decode(string(pduBuf, pduLen),
                         (messageType != SMSMessage::SMS_SUBMIT));
    
    SMSStoreEntry *newEntry = new SMSStoreEntry(message, _nextIndex++);
    _sortedSMSStore.insert(
      SMSStoreMap::value_type(
        SMSMapKey(*this, message->serviceCentreTimestamp()),
        newEntry)
      );
  }
}

void SortedSMSStore::sync(bool fromDestructor) throw(GsmException)
{
  if (_fromFile && _changed)
  {
    checkReadonly();

    // if writing to stdout and not called from destructor ignore
    // (avoids writing to stdout multiple times)
    if (_filename == "" && ! fromDestructor) return;

    // create backup file - but only once
    if (! _madeBackupFile && _filename != "") // don't make backup of stdout
    {
      renameToBackupFile(_filename);
      _madeBackupFile = true;
    }

    // open stream
    ostream *pbs = NULL;
    try
    {
      if (_filename == "")
        pbs = &cout;
      else
		pbs = new ofstream(_filename.c_str(), ios::out | ios::binary);
      
      if (pbs->bad())
        throw GsmException(
          stringPrintf(_("error opening file '%s' for writing"),
                       (_filename == "" ? _("<STDOUT>") :
                        _filename.c_str())),
          OSError);

      // write version number
      unsigned_int_2 version = htons(SMS_STORE_FILE_FORMAT_VERSION);
      writenbytes(_filename, *pbs, 2, (char*)&version);

      // and write the entries
      for (SMSStoreMap::iterator i = _sortedSMSStore.begin();
           i != _sortedSMSStore.end(); ++i)
      {
        // create PDU and write length
        string pdu = i->second->message()->encode();
        unsigned_int_2 pduLen = htons(pdu.length());
        writenbytes(_filename, *pbs, 2, (char*)&pduLen);

        // write reserved field (was formerly index)
        unsigned_int_4 reserved = htonl(0);
        writenbytes(_filename, *pbs, 4, (char*)&reserved);
        
        // write message type
        char messageType = i->second->message()->messageType();
        writenbytes(_filename, *pbs, 1, (char*)&messageType);

        // write PDU
        writenbytes(_filename, *pbs, pdu.length(), pdu.data());
      }
    }
    catch(GsmException &e)
    {
      if (pbs != &cout) delete pbs;
      throw;
    }
    // close file
    if (pbs != &cout) delete pbs;

    _changed = false;
  }
}

void SortedSMSStore::checkReadonly() throw(GsmException)
{
  if (_readonly) throw GsmException(
    _("attempt to change SMS store read from <STDIN>"),
    ParameterError);
}

SortedSMSStore::SortedSMSStore(string filename) throw(GsmException) :
  _changed(false), _fromFile(true), _madeBackupFile(false),
  _sortOrder(ByDate), _readonly(false), _filename(filename), _nextIndex(0)
{
  // open the file
  ifstream pbs(filename.c_str(), ios::in | ios::binary);
  if (pbs.bad())
    throw GsmException(stringPrintf(_("cannot open file '%s'"),
                                    filename.c_str()), OSError);
  // and read the file
  readSMSFile(pbs, filename);
}

SortedSMSStore::SortedSMSStore(bool fromStdin) throw(GsmException) :
  _changed(false), _fromFile(true), _madeBackupFile(false),
  _sortOrder(ByDate), _readonly(fromStdin), _nextIndex(0)
  // _filename is "" - this means stdout
{
  // read from stdin
  if (fromStdin)
    readSMSFile(cin, (string)_("<STDIN>"));
}

SortedSMSStore::SortedSMSStore(SMSStoreRef meSMSStore)
  throw(GsmException) :
  _changed(false), _fromFile(false), _madeBackupFile(false),
  _sortOrder(ByDate), _readonly(false), _meSMSStore(meSMSStore)
{
  // It is necessary to count the entries read because
  // the maximum index into the SMS store may be larger than smsStore.size()
  int entriesRead = 0;
  reportProgress(0, _meSMSStore->size());

  for (int i = 0;; ++i)
  {
    if (entriesRead == _meSMSStore->size())
      break;                 // ready
    if (! _meSMSStore()[i].empty())
    {
      _sortedSMSStore.insert(
        SMSStoreMap::value_type(
          SMSMapKey(*this,
                    _meSMSStore()[i].message()->serviceCentreTimestamp()),
          &_meSMSStore()[i])
        );
      ++entriesRead;
      reportProgress(entriesRead);
    }
  }
}

void SortedSMSStore::setSortOrder(SortOrder newOrder)
{
  if (_sortOrder == newOrder) return; // nothing to be done

  SMSStoreMap savedSMSStore = _sortedSMSStore;
  _sortedSMSStore = SMSStoreMap();
  _sortOrder = newOrder;

  switch (newOrder)
  {
  case ByIndex:
  {
    for (SMSStoreMap::iterator i = savedSMSStore.begin();
         i != savedSMSStore.end(); ++i)
      _sortedSMSStore.insert(
        SMSStoreMap::value_type(SMSMapKey(*this, (i->second->index())),
                                i->second));
    break;
  }
  case ByDate:
  {
    for (SMSStoreMap::iterator i = savedSMSStore.begin();
         i != savedSMSStore.end(); ++i)
      _sortedSMSStore.insert(
        SMSStoreMap::value_type(
          SMSMapKey(*this, (i->second->message()->serviceCentreTimestamp())),
          i->second));
    break;
  }
  case ByAddress:
  {
    for (SMSStoreMap::iterator i = savedSMSStore.begin();
         i != savedSMSStore.end(); ++i)
      _sortedSMSStore.insert(
        SMSStoreMap::value_type(
          SMSMapKey(*this, (i->second->message()->address())),
          i->second));
    break;
  }
  case ByType:
  {
    for (SMSStoreMap::iterator i = savedSMSStore.begin();
         i != savedSMSStore.end(); ++i)
      _sortedSMSStore.insert(
        SMSStoreMap::value_type(
          SMSMapKey(*this, (i->second->message()->messageType())),
          i->second));
    break;
  }
  default:
    assert(0);
    break;
  }
}

int SortedSMSStore::max_size() const
{
  if (_fromFile)
    return _sortedSMSStore.max_size();
  else
    return _meSMSStore->max_size();
}

int SortedSMSStore::capacity() const
{
  if (_fromFile)
    return _sortedSMSStore.max_size();
  else
    return _meSMSStore->capacity();
}

SortedSMSStore::iterator
SortedSMSStore::insert(const SMSStoreEntry& x) throw(GsmException)
{
  checkReadonly();
  _changed = true;
  SMSStoreEntry *newEntry;

  if (_fromFile)
    newEntry = new SMSStoreEntry(x.message(), _nextIndex++);
  else
  {
    SMSStoreEntry newMEEntry(x.message());
    newEntry = _meSMSStore->insert(newMEEntry);
  }
  
  switch (_sortOrder)
  {
  case ByIndex:
    return
      _sortedSMSStore.
      insert(SMSStoreMap::value_type(SMSMapKey(*this, newEntry->index()),
                                     newEntry));
    break;
  case ByDate:
    return
      _sortedSMSStore.
      insert(SMSStoreMap::value_type(
        SMSMapKey(*this, newEntry->message()->serviceCentreTimestamp()),
        newEntry));
    break;
  case ByAddress:
    return
      _sortedSMSStore.
      insert(SMSStoreMap::value_type(
        SMSMapKey(*this, newEntry->message()->address()),
        newEntry));
    break;
  case ByType:
    return
      _sortedSMSStore.
      insert(SMSStoreMap::value_type(
        SMSMapKey(*this, newEntry->message()->messageType()),
        newEntry));
    break;
  default:
    assert(0);
    break;
  }
  return SortedSMSStore::iterator();
}

SortedSMSStore::iterator
SortedSMSStore::insert(iterator position, const SMSStoreEntry& x)
  throw(GsmException)
{
  return insert(x);
}

SortedSMSStore::size_type SortedSMSStore::erase(Address &key)
  throw(GsmException)
{
  assert(_sortOrder == ByAddress);

  SMSMapKey mapKey(*this, key);

  // deallocate memory or remove from underlying ME SMS store
  for (SMSStoreMap::iterator i = _sortedSMSStore.find(mapKey);
       i != _sortedSMSStore.end() && i->first == mapKey; ++i)
  {
    checkReadonly();
    _changed = true;
    if (_fromFile)
      delete i->second;
    else
      _meSMSStore->erase((SMSStore::iterator)i->second);
  }

  return _sortedSMSStore.erase(mapKey);
}

SortedSMSStore::size_type SortedSMSStore::erase(int key)
  throw(GsmException)
{
  assert(_sortOrder == ByIndex || _sortOrder == ByType);

  SMSMapKey mapKey(*this, key);

  // deallocate memory or remove from underlying ME SMS store
  for (SMSStoreMap::iterator i = _sortedSMSStore.find(mapKey);
       i != _sortedSMSStore.end() && i->first == mapKey; ++i)
  {
    checkReadonly();
    _changed = true;
    if (_fromFile)
      delete i->second;
    else
      _meSMSStore->erase((SMSStore::iterator)i->second);
  }

  return _sortedSMSStore.erase(mapKey);
}

SortedSMSStore::size_type SortedSMSStore::erase(Timestamp &key)
  throw(GsmException)
{
  assert(_sortOrder == ByDate);

  SMSMapKey mapKey(*this, key);

  // deallocate memory or remove from underlying ME SMS store
  for (SMSStoreMap::iterator i = _sortedSMSStore.find(mapKey);
       i != _sortedSMSStore.end() && i->first == mapKey; ++i)
  {
    checkReadonly();
    _changed = true;
    if (_fromFile)
      delete i->second;
    else
      _meSMSStore->erase((SMSStore::iterator)i->second);
  }

  return _sortedSMSStore.erase(mapKey);
}

void SortedSMSStore::erase(iterator position)
  throw(GsmException)
{
  checkReadonly();
  _changed = true;
  // deallocate memory or remove from underlying ME SMS store
  if (_fromFile)
    delete ((SMSStoreMap::iterator)position)->second;
  else
    _meSMSStore->erase((SMSStore::iterator)
                       ((SMSStoreMap::iterator)position)->second);
  _sortedSMSStore.erase(position);
}

void SortedSMSStore::erase(iterator first, iterator last)
  throw(GsmException)
{
  checkReadonly();
  _changed = true;
  for (SMSStoreMap::iterator i = first; i != last; ++i)
    if (_fromFile)
      delete i->second;
    else
      _meSMSStore->erase((SMSStore::iterator)i->second);
  _sortedSMSStore.erase(first, last);
}

void SortedSMSStore::clear() throw(GsmException)
{
  checkReadonly();
  _changed = true;
  for (iterator i = begin(); i != end(); i++)
    erase(i);
}

SortedSMSStore::~SortedSMSStore()
{
  if (_fromFile)
  {
    sync(true);
    for (SMSStoreMap::iterator i = _sortedSMSStore.begin();
         i != _sortedSMSStore.end(); ++i)
      delete i->second;
  }
}


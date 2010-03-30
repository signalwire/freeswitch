// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sms_store.cc
// *
// * Purpose: SMS functions, SMS store
// *          (ETSI GSM 07.05)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 20.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_sms_store.h>
#include <gsmlib/gsm_parser.h>
#include <gsmlib/gsm_me_ta.h>
#include <iostream>

using namespace std;
using namespace gsmlib;

// SMSStoreEntry members

SMSStoreEntry::SMSStoreEntry() :
   _status(Unknown), _cached(false), _mySMSStore(NULL), _index(0)
{
}


SMSMessageRef SMSStoreEntry::message() const throw(GsmException)
{
  if (! cached())
  {
    assert(_mySMSStore != NULL);
    // these operations are at least "logically const"
    SMSStoreEntry *thisEntry = const_cast<SMSStoreEntry*>(this);
    _mySMSStore->readEntry(_index, thisEntry->_message, thisEntry->_status);
    thisEntry->_cached = true;
  }
  return _message;
}

CBMessageRef SMSStoreEntry::cbMessage() const throw(GsmException)
{
  assert(_mySMSStore != NULL);

  // these operations are at least "logically const"
  SMSStoreEntry *thisEntry = const_cast<SMSStoreEntry*>(this);
  // don't cache CB message for now
  thisEntry->_cached = false;

  CBMessageRef result;
  _mySMSStore->readEntry(_index, result);
  return result;
}

SMSStoreEntry::SMSMemoryStatus SMSStoreEntry::status() const
  throw(GsmException)
{
  if (! cached())
  {
    assert(_mySMSStore != NULL);
    // these operations are at least "logically const"
    SMSStoreEntry *thisEntry = const_cast<SMSStoreEntry*>(this);
    _mySMSStore->readEntry(_index, thisEntry->_message, thisEntry->_status);
    thisEntry->_cached = true;
  }
  return _status;
}

bool SMSStoreEntry::empty() const throw(GsmException)
{
  return message().isnull();
}

unsigned char SMSStoreEntry::send(Ref<SMSMessage> &ackPdu)
  throw(GsmException)
{
  return _mySMSStore->send(_index, ackPdu);
}

unsigned char SMSStoreEntry::send() throw(GsmException)
{
  SMSMessageRef mref;
  return send(mref);
}

bool SMSStoreEntry::cached() const
{
  if (_mySMSStore == NULL)
    return _cached;
  else
    return _cached && _mySMSStore->_useCache;
}

Ref<SMSStoreEntry> SMSStoreEntry::clone()
{
  Ref<SMSStoreEntry> result = new SMSStoreEntry(_message->clone());
  result->_status = _status;
  result->_index = _index;
  return result;
}

bool SMSStoreEntry::operator==(const SMSStoreEntry &e) const
{
  if (_message.isnull() || e._message.isnull())
    return _message.isnull() && e._message.isnull();
  else
    return _message->encode() == e._message->encode();
}

SMSStoreEntry::SMSStoreEntry(const SMSStoreEntry &e)
{
 _message = e._message;
 _status = e._status;
 _cached = e._cached;
 _mySMSStore = e._mySMSStore;
 _index = e._index;
}

SMSStoreEntry &SMSStoreEntry::operator=(const SMSStoreEntry &e)
{
 _message = e._message;
 _status = e._status;
 _cached = e._cached;
 _mySMSStore = e._mySMSStore;
 _index = e._index;
 return *this;
}

// iterator members

SMSStoreEntry &SMSStoreIterator::operator*()
{
  return (*_store)[_index];
}
  
SMSStoreEntry *SMSStoreIterator::operator->()
{
  return &(*_store)[_index];
}

SMSStoreIterator::operator SMSStoreEntry*()
{
  return &(*_store)[_index];
}

SMSStoreIterator &SMSStoreIterator::operator=(const SMSStoreIterator &i)
{
  _index = i._index;
  _store = i._store;
  return *this;
}

const SMSStoreEntry &SMSStoreConstIterator::operator*()
{
  return (*_store)[_index];
}
  
const SMSStoreEntry *SMSStoreConstIterator::operator->()
{
  return &(*_store)[_index];
}

// SMSStore members

void SMSStore::readEntry(int index, SMSMessageRef &message,
                         SMSStoreEntry::SMSMemoryStatus &status)
  throw(GsmException)
{
  // select SMS store
  _meTa.setSMSStore(_storeName, 1);

#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "*** Reading SMS entry " << index << endl;
#endif // NDEBUG

  string pdu;
  Ref<Parser> p;
  try
  {
    p = new Parser(_at->chat("+CMGR=" + intToStr(index + 1), "+CMGR:",
                             pdu, false, true, true));
  }
  catch (GsmException &ge)
  {
    if (ge.getErrorCode() != SMS_INVALID_MEMORY_INDEX)
      throw ge;
    else
    {
      message = SMSMessageRef();
      status = SMSStoreEntry::Unknown;
      return;
    }
  }

  if (pdu.length() == 0)
  {
    message = SMSMessageRef();
    status = SMSStoreEntry::Unknown;
  }
  else
  {
    // add missing service centre address if required by ME
    if (! _at->getMeTa().getCapabilities()._hasSMSSCAprefix)
      pdu = "00" + pdu;

    status = (SMSStoreEntry::SMSMemoryStatus)p->parseInt();

    // ignore the rest of the line
    message = SMSMessageRef(
      SMSMessage::decode(pdu,
                         !(status == SMSStoreEntry::StoredUnsent ||
                           status == SMSStoreEntry::StoredSent),
                         _at.getptr()));
  }
}

void SMSStore::readEntry(int index, CBMessageRef &message)
  throw(GsmException)
{
  // select SMS store
  _meTa.setSMSStore(_storeName, 1);

#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "*** Reading CB entry " << index << endl;
#endif // NDEBUG

  string pdu;
  Ref<Parser> p;
  try
  {
    // this is just one row splitted in two part
    // (msvc6 fail with internal compiler error)
    string s = _at->chat("+CMGR=" + intToStr(index + 1), "+CMGR:",
                         pdu, false, true, true);
    p = new Parser(s);
  }
  catch (GsmException &ge)
  {
    if (ge.getErrorCode() != SMS_INVALID_MEMORY_INDEX)
      throw ge;
    else
    {
      message = CBMessageRef();
      return;
    }
  }

  if (pdu.length() == 0)
    message = CBMessageRef();
  else
    message = CBMessageRef(new CBMessage(pdu));
}

void SMSStore::writeEntry(int &index, SMSMessageRef message)
  throw(GsmException)
{
  // select SMS store
  _meTa.setSMSStore(_storeName, 2);

#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "*** Writing SMS entry " << index << endl;
#endif
  
  // compute length of pdu
  string pdu = message->encode();

  // set message status to "RECEIVED READ" for SMS_DELIVER, SMS_STATUS_REPORT
  string statusString;

  // Normally the ",1" sets the message status to "REC READ" (received read)
  // which is appropriate for all non-submit messages
  // Motorola Timeport 260 does not like this code, though
  // DELIVER messages are magically recognized anyway
  if (message->messageType() != SMSMessage::SMS_SUBMIT &&
      ! _at->getMeTa().getCapabilities()._wrongSMSStatusCode)
    statusString = ",1";

  Parser p(_at->sendPdu("+CMGW=" +
                        intToStr(pdu.length() / 2 -
                                 message->getSCAddressLen()) + statusString,
                        "+CMGW:", pdu));
  index = p.parseInt() - 1;
}

void SMSStore::eraseEntry(int index) throw(GsmException)
{
  // Select SMS store
  _meTa.setSMSStore(_storeName, 1);

#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "*** Erasing SMS entry " << index << endl;
#endif
  
  _at->chat("+CMGD=" + intToStr(index + 1));
}

unsigned char SMSStore::send(int index, Ref<SMSMessage> &ackPdu)
 throw(GsmException)
{
  Parser p(_at->chat("+CMSS=" + intToStr(index + 1), "+CMSS:"));
  unsigned char messageReference = p.parseInt();

  if (p.parseComma(true))
  {
    string pdu = p.parseEol();

    // add missing service centre address if required by ME
    if (! _at->getMeTa().getCapabilities()._hasSMSSCAprefix)
      pdu = "00" + pdu;

    ackPdu = SMSMessage::decode(pdu);
  }
  else
    ackPdu = SMSMessageRef();

  return messageReference;
}

int SMSStore::doInsert(SMSMessageRef message)
  throw(GsmException)
{
  int index;
  writeEntry(index, message);
  // it is safer to force reading back the SMS from the ME
  resizeStore(index + 1);
  _store[index]->_cached = false;
  return index;
}

SMSStore::SMSStore(string storeName, Ref<GsmAt> at, MeTa &meTa)
  throw(GsmException) :
  _storeName(storeName), _at(at), _meTa(meTa), _useCache(true)
{
  // select SMS store
  Parser p(_meTa.setSMSStore(_storeName, true, true));
  
  p.parseInt();                 // skip number of used mems
  p.parseComma();

  resizeStore(p.parseInt());    // ignore rest of line
}

void SMSStore::resizeStore(int newSize)
{
  int oldSize = _store.size();
  if (newSize > oldSize)
  {
    //    cout << "*** Resizing from " << oldSize << " to " << newSize << endl;
    _store.resize(newSize);
    
    // initialize store entries
    for (int i = oldSize; i < newSize; i++)
    {
      _store[i] = new SMSStoreEntry();
      _store[i]->_index = i;
      _store[i]->_cached = false;
      _store[i]->_mySMSStore = this;
    }
  }
}

SMSStore::iterator SMSStore::begin()
{
  return SMSStoreIterator(0, this);
}

SMSStore::const_iterator SMSStore::begin() const
{
  return SMSStoreConstIterator(0, this);
}

SMSStore::iterator SMSStore::end()
{
  return SMSStoreIterator(_store.size(), this);
}

SMSStore::const_iterator SMSStore::end() const
{
  return SMSStoreConstIterator(_store.size(), this);
}

SMSStore::reference SMSStore::operator[](int n)
{
  resizeStore(n + 1);
  return *_store[n];
}

SMSStore::const_reference SMSStore::operator[](int n) const
{
  const_cast<SMSStore*>(this)->resizeStore(n + 1);
  return *_store[n];
}

SMSStore::reference SMSStore::front()
{
  return *_store[0];
}

SMSStore::const_reference SMSStore::front() const
{
  return *_store[0];
}

SMSStore::reference SMSStore::back()
{
  return *_store.back();
}

SMSStore::const_reference SMSStore::back() const
{
  return *_store.back();
}

int SMSStore::size() const throw(GsmException)
{
  // select SMS store
  Parser p(_meTa.setSMSStore(_storeName, 1, true));
  
  return p.parseInt();  
}

SMSStore::iterator SMSStore::insert(iterator position,
                                    const SMSStoreEntry& x)
  throw(GsmException)
{
  int index = doInsert(x.message());
  return SMSStoreIterator(index, this);
}

SMSStore::iterator SMSStore::insert(const SMSStoreEntry& x)
  throw(GsmException)
{
  int index = doInsert(x.message());
  return SMSStoreIterator(index, this);
}

void SMSStore::insert (iterator pos, int n, const SMSStoreEntry& x)
  throw(GsmException)
{
  for (int i = 0; i < n; i++)
    doInsert(x.message());
}

void SMSStore::insert (iterator pos, long n, const SMSStoreEntry& x)
  throw(GsmException)
{
  for (long i = 0; i < n; i++)
    doInsert(x.message());
}

SMSStore::iterator SMSStore::erase(iterator position)
  throw(GsmException)
{
  eraseEntry(position->_index);
  position->_cached = false;
  return position + 1;
}

SMSStore::iterator SMSStore::erase(iterator first, iterator last)
  throw(GsmException)
{
  iterator i(0, this);
  for (i = first; i != last; ++i)
    erase(i);
  return i;
}

void SMSStore::clear() throw(GsmException)
{
  for (iterator i = begin(); i != end(); ++i)
    erase(i);
}

SMSStore::~SMSStore()
{
  for (vector<SMSStoreEntry*>::iterator i = _store.begin();
       i != _store.end(); ++i)
    delete *i;
}


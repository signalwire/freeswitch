// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_phonebook.cc
// *
// * Purpose: Phonebook management functions
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 6.5.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_nls.h>
#include <gsmlib/gsm_sysdep.h>
#include <gsmlib/gsm_phonebook.h>
#include <gsmlib/gsm_parser.h>
#include <gsmlib/gsm_me_ta.h>
#include <strstream>
#include <iostream>
#include <assert.h>
#include <ctype.h>

using namespace std;
using namespace gsmlib;

// PhonebookEntry members

PhonebookEntry::PhonebookEntry(const PhonebookEntryBase &e)
  throw(GsmException) : _cached(true), _myPhonebook(NULL)
{
  set(e.telephone(), e.text(), e.index(), e.useIndex());
}

void PhonebookEntry::set(string telephone, string text, int index,
                         bool useIndex)
  throw(GsmException)
{
  checkTextAndTelephone(text, telephone);

  if (_myPhonebook != NULL)
  {
    if (text.length() > _myPhonebook->getMaxTextLen())
      throw GsmException(
        stringPrintf(_("length of text '%s' exceeds maximum text "
                       "length (%d characters) of phonebook '%s'"),
                     text.c_str(), _myPhonebook->getMaxTextLen(),
                     _myPhonebook->name().c_str()),
        ParameterError);
    
    if (telephone.length() > _myPhonebook->getMaxTelephoneLen())
      throw GsmException(
        stringPrintf(_("length of telephone number '%s' " 
                       "exceeds maximum telephone number "
                       "length (%d characters) of phonebook '%s'"),
                     telephone.c_str(), _myPhonebook->getMaxTelephoneLen(),
                     _myPhonebook->name().c_str()),
        ParameterError);

    _myPhonebook->writeEntry(_index, telephone, text);
  }
  else
    _index = index;

  _useIndex = useIndex;
  _cached = true;
  _telephone = telephone;
  _text = text;
  _changed = true;
}

string PhonebookEntry::text() const throw(GsmException)
{
  if (! cached())
  {
    assert(_myPhonebook != NULL);
    // these operations are at least "logically const"
    PhonebookEntry *thisEntry = const_cast<PhonebookEntry*>(this);
    _myPhonebook->readEntry(_index, thisEntry->_telephone, thisEntry->_text);
    thisEntry->_cached = true;
  }
  return _text;
}

string PhonebookEntry::telephone() const throw(GsmException)
{
  if (! cached())
  {
    assert(_myPhonebook != NULL);
    // these operations are at least "logically const"
    PhonebookEntry *thisEntry = const_cast<PhonebookEntry*>(this);
    _myPhonebook->readEntry(_index, thisEntry->_telephone, thisEntry->_text);
    thisEntry->_cached = true;
  }
  return _telephone;
}

bool PhonebookEntry::cached() const
{
  if (_myPhonebook == NULL)
    return _cached;
  else
    return _cached && _myPhonebook->_useCache;
}

PhonebookEntry::PhonebookEntry(const PhonebookEntry &e) throw(GsmException)
{
  set(e._telephone, e._text, e._index, e._useIndex);
}

PhonebookEntry &PhonebookEntry::operator=(const PhonebookEntry &e)
  throw(GsmException)
{
  set(e._telephone, e._text, e._index, e._useIndex);
  return *this;
}

// Phonebook members

int Phonebook::parsePhonebookEntry(string response,
                                   string &telephone, string &text)
{
  // this is a workaround for a bug that occurs with my ME/TA combination
  // some texts are truncated and don't have a trailing "
  if (response.length() > 0 && response[response.length() - 1] != '"')
    response += '"';
  Parser p(response);

  int index = p.parseInt();
  p.parseComma();

  // handle case of empty entry
  if (p.getEol().substr(0, 5) == "EMPTY")
  {
    telephone = "";
    text = "";
    return index;
  }

  telephone = p.parseString();
  p.parseComma();
  unsigned int numberFormat = p.parseInt();
  if (numberFormat != UnknownNumberFormat &&
      numberFormat != InternationalNumberFormat)
    cerr << "*** GSMLIB WARNING: Unexpected number format when reading from "
         << "phonebook: " << numberFormat << " ***" << endl;
  p.parseComma();
  text = p.parseString(false, true);
  if (lowercase(_myMeTa.getCurrentCharSet()) == "gsm")
    text = gsmToLatin1(text);
  if (numberFormat == InternationalNumberFormat)
  {
    // skip leading "+" signs that may already exist
    while (telephone.length() > 0 && telephone[0] == '+')
      telephone = telephone.substr(1);
    telephone = "+" + telephone;
  }

  return index;
}

void Phonebook::readEntry(int index, string &telephone, string &text)
  throw(GsmException)
{
  // select phonebook
  _myMeTa.setPhonebook(_phonebookName);

  // read entry
  string response = _at->chat("+CPBR=" + intToStr(index), "+CPBR:",
                              false, // dont't ignore errors
                              true); // but accept empty responses
  // (the latter is necessary for some mobile phones that return nothing
  // if the entry is empty)

  if (response.length() == 0)   // OK phone returned empty response
  {
    telephone = text = "";      // then the entry is empty as well
  }
  else
    parsePhonebookEntry(response, telephone, text);

#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "*** Reading PB entry " << index << " number " << telephone 
         << " text " << text << endl;
#endif
}

void Phonebook::findEntry(string text, int &index, string &telephone)
  throw(GsmException)
{
  // select phonebook
  _myMeTa.setPhonebook(_phonebookName);

  // read entry
  string response = _at->chat("+CPBF=\"" + text + "\"", "+CPBF:",
                              false, // dont't ignore errors
                              true); // but accept empty responses
  // (the latter is necessary for some mobile phones that return nothing
  // if the entry is empty)

  if (response.length() == 0)   // OK phone returned empty response
  {
    telephone = "";      // then the entry is empty as well
    index = 0;
  }
  else
    index=parsePhonebookEntry(response, telephone, text);

#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "*** Finding PB entry " << text << " number " << telephone 
         << " index " << index << endl;
#endif
}

void Phonebook::writeEntry(int index, string telephone, string text)
  throw(GsmException)
{
#ifndef NDEBUG
  if (debugLevel() >= 1)
    cerr << "*** Writing PB entry #" << index << " number '" << telephone
         << "' text '" << text << "'" << endl;
#endif
  // select phonebook
  _myMeTa.setPhonebook(_phonebookName);

  // write entry
  string s;
  if (telephone == "" && text == "")
  {
    ostrstream os;
    os << "+CPBW=" << index;
    os << ends;
    char *ss = os.str();
    s = string(ss);
    delete[] ss;
  }
  else
  {
    int type;
    if (telephone.find('+') == string::npos)
      type = UnknownNumberFormat;
    else
      type = InternationalNumberFormat;
    string gsmText = text;
    if (lowercase(_myMeTa.getCurrentCharSet()) == "gsm")
      gsmText = latin1ToGsm(gsmText);
    ostrstream os;
    os << "+CPBW=" << index << ",\"" << telephone << "\"," << type
       << ",\"";
    os << ends;
    char *ss = os.str();
    s = string(ss);
    delete[] ss;
    // this cannot be added with ostrstream because the gsmText can
    // contain a zero (GSM default alphabet for '@')
    s +=  gsmText + "\"";
  }
  _at->chat(s);
}

Phonebook::iterator Phonebook::insertFirstEmpty(string telephone, string text)
  throw(GsmException)
{
  for (int i = 0; i < _maxSize; i++)
    if (_phonebook[i].empty())
    {
      _phonebook[i].set(telephone, text);
      adjustSize(1);
      return begin() + i;
    }
  throw GsmException(_("phonebook full"), OtherError);
}

Phonebook::iterator Phonebook::insert(const string telephone,
                                      const string text,
                                      const int index)
{
  for (int i = 0; i < _maxSize; i++)
    if (_phonebook[i].index() == index)
      if (_phonebook[i].empty())
      {
        _phonebook[i].set(telephone, text);
        adjustSize(1);
        return begin() + i;
      }
      else
        throw GsmException(_("attempt to overwrite phonebook entry"),
                           OtherError);
  return end();
}

Phonebook::Phonebook(string phonebookName, Ref<GsmAt> at, MeTa &myMeTa,
                     bool preload) throw(GsmException) :
  _phonebookName(phonebookName), _at(at), _myMeTa(myMeTa), _useCache(true)
{
  // select phonebook
  _myMeTa.setPhonebook(_phonebookName);

  // query size and maximum capacity of phonebook
  _size = -1;                   // -1 means not known yet
  _maxSize = -1;
  Parser q(_at->chat("+CPBS?", "+CPBS:"));
  string dummy = q.parseString();
  if (q.parseComma(true))       // this means that
  {                             // used and total result is supported by ME
    _size = q.parseInt();
    q.parseComma();
    _maxSize = q.parseInt();
  }
  
  // get basic phonebook info from ME
  Parser p(_at->chat("+CPBR=?", "+CPBR:"));

  // get index of actually available entries in the phonebook
  vector<bool> availablePositions = p.parseIntList();
  p.parseComma();
  _maxNumberLength = p.parseInt();
  p.parseComma();
  _maxTextLength = p.parseInt();
  
  // find out capacity of phonebook in ME
  // Note: The phonebook in the ME may be sparse, eg. the range of
  // allowed index numbers may be something like (3-4, 20-100, 120).
  // The standard allows this, even though it is unlikely to be 
  // implemented like that by anyone.
  // In memory we store only phonebook entries that may actually be
  // used, ie. the phonebook in memory is not sparse.
  // Each entry has a member _index that corresponds to the index in the ME.
  if (_maxSize == -1)
  {
    _maxSize = 0;
    for (vector<bool>::iterator i = availablePositions.begin();
         i != availablePositions.end(); ++i)
      if (*i) ++_maxSize;
  }

  // for use with preload below
  int *meToPhonebookIndexMap =
    (int*)alloca(sizeof(int) * (availablePositions.size() + 1));

  // initialize phone book entries
  if (_maxSize == 0)
    _phonebook = NULL;
  else
    _phonebook = new PhonebookEntry[_maxSize];
  int nextAvailableIndex = 0;
  int i;
  for (i = 0; i < _maxSize; i++)
  {
    while (! availablePositions[nextAvailableIndex])
      nextAvailableIndex++;
    _phonebook[i]._index = nextAvailableIndex;
    _phonebook[i]._cached = false;
    _phonebook[i]._myPhonebook = this;
    meToPhonebookIndexMap[nextAvailableIndex++] = i;
  }

  // find out first index number of phonebook
  int firstIndex = -1;
  for (i = 0; i < _maxSize; i++)
    if (availablePositions[i])
    {
      firstIndex = i;
      break;
    }

  // preload phonebook
  // Note: this contains a workaround for the bug that
  // some MEs can not return the entire phonebook with one AT command
  // To detect this condition, _size must be known
  // also, this code only handles non-sparse phonebooks
  if (preload && _size != -1 && 
      (int)availablePositions.size() == _maxSize + firstIndex)
  {
    int entriesRead = 0;
    int startIndex = firstIndex;

    while (entriesRead < _size)
    {
      reportProgress(0, _maxSize); // chatv also calls reportProgress()
      vector<string> responses =
        _at->chatv("+CPBR=" + intToStr(startIndex) +
                   "," + intToStr(_maxSize + firstIndex - 1),
                   "+CPBR:", true);

      // this means that we have read nothing even though not all
      // entries have been retrieved (entriesRead < _size)
      // this could be due to a malfunction of the ME...
      // anyway, missing entries can be read later by readEntry()
      if (responses.size() == 0)
      {
#ifndef NDEBUG
        if (debugLevel() >= 1)
          cerr << "*** error when preloading phonebook: "
            "not all entries returned" << endl;
#endif
        break;
      }

      for (vector<string>::iterator i = responses.begin();
           i != responses.end(); ++i)
      {
        string telephone, text;
        int meIndex = parsePhonebookEntry(*i, telephone, text);
        _phonebook[meToPhonebookIndexMap[meIndex]]._cached = true;
        _phonebook[meToPhonebookIndexMap[meIndex]]._telephone = telephone;
        _phonebook[meToPhonebookIndexMap[meIndex]]._text = text;
        assert(_phonebook[meToPhonebookIndexMap[meIndex]]._index == meIndex);

        ++entriesRead;
        startIndex = meIndex + 1;
#ifndef NDEBUG
        if (debugLevel() >= 1)
          cerr << "*** Preloading PB entry " << meIndex
               << " number " << telephone 
               << " text " << text << endl;
#endif
      }
    }
  }
}

Phonebook::iterator Phonebook::begin()
{
  return &_phonebook[0];
}

Phonebook::const_iterator Phonebook::begin() const
{
  return &_phonebook[0];
}

Phonebook::iterator Phonebook::end()
{
  return &_phonebook[_maxSize];
}

Phonebook::const_iterator Phonebook::end() const
{
  return &_phonebook[_maxSize];
}

Phonebook::reference Phonebook::operator[](int n)
{
  return _phonebook[n];
}

Phonebook::const_reference Phonebook::operator[](int n) const
{
  return _phonebook[n];
}

Phonebook::reference Phonebook::front()
{
  return _phonebook[0];
}

Phonebook::const_reference Phonebook::front() const
{
  return _phonebook[0];
}

Phonebook::reference Phonebook::back()
{
  return _phonebook[_maxSize - 1];
}

Phonebook::const_reference Phonebook::back() const
{
  return _phonebook[_maxSize - 1];
}

int Phonebook::size() const throw(GsmException)
{
  if (_size != -1)
    return _size;
  else
  {
    int result = 0;
    for (int i = 0; i < _maxSize; i++)
      if (! _phonebook[i].empty())
        result++;
    Phonebook *thisPhonebook = const_cast<Phonebook*>(this);
    thisPhonebook->_size = result;
    return result;
  }
}

Phonebook::iterator Phonebook::insert(iterator position,
                                      const PhonebookEntry& x)
  throw(GsmException)
{
  if (x.useIndex() && x.index() != -1)
    return insert(x.telephone(), x.text(), x.index());
  else
    return insertFirstEmpty(x.telephone(), x.text());
}

void Phonebook::insert (iterator pos, int n, const PhonebookEntry& x)
  throw(GsmException)
{
  for (int i = 0; i < n; i++)
    if (x.useIndex() && x.index() != -1)
      insert(x.telephone(), x.text(), x.index());
    else
      insertFirstEmpty(x.telephone(), x.text());
}

void Phonebook::insert (iterator pos, long n, const PhonebookEntry& x)
  throw(GsmException)
{
  for (long i = 0; i < n; i++)
    if (x.useIndex() && x.index() != -1)
      insert(x.telephone(), x.text(), x.index());
    else
      insertFirstEmpty(x.telephone(), x.text());
}

Phonebook::iterator Phonebook::erase(iterator position)
  throw(GsmException)
{
  if (! position->empty())
  {
    position->set("", "");
    adjustSize(-1);
  }
  return position + 1;
}

Phonebook::iterator Phonebook::erase(iterator first, iterator last)
  throw(GsmException)
{
  iterator i;
  for (i = first; i != last; ++i)
    erase(i);
  return i;
}

void Phonebook::clear() throw(GsmException)
{
  for (iterator i = begin(); i != end(); ++i)
    erase(i);
}

Phonebook::iterator Phonebook::find(string text) throw(GsmException)
{
  int index;
  string telephone;

  int i;
  for (i = 0; i < _maxSize; i++)
    if (_phonebook[i].text() == text)
      return begin() + i;

  findEntry(text, index, telephone);
  
  for (i = 0; i < _maxSize; i++)
    if (_phonebook[i].index() == index)
      if (_phonebook[i].cached())
      {
        // if entry was already (= cached) and is now different
        // the SIM card or it's contents were changed
        if (_phonebook[i]._telephone != telephone ||
            _phonebook[i]._text != text)
          throw GsmException(_("SIM card changed while accessing phonebook"),
                             OtherError);
      }
      else
      {
        _phonebook[i]._cached = true;
        _phonebook[i]._telephone = telephone;
        _phonebook[i]._text = text;
        return begin() + i;
      }
  return end();
}

Phonebook::~Phonebook()
{
  delete []_phonebook;
}

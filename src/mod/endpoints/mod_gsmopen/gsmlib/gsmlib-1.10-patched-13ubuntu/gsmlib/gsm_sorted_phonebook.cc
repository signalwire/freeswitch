// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sorted_phonebook.cc
// *
// * Purpose: Alphabetically sorted phonebook
// *          (residing in files or in the ME)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 25.6.1999
// *************************************************************************

#ifdef HAVE_CONFIG_H
#include <gsm_config.h>
#endif
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_sorted_phonebook.h>
#include <gsmlib/gsm_nls.h>
#include <iostream>
#include <fstream>
#include <limits.h>
#include <cstring>

const int MAX_LINE_SIZE = 1000;

using namespace std;
using namespace gsmlib;

string SortedPhonebook::escapeString(string s)
{
  string result;
  
  for (const char *pp = s.c_str(); *pp != 0; ++pp)
  {
    if (*pp == CR)
      result += "\\r";
    else if (*pp == LF)
      result += "\\n";
    else if (*pp == '\\')
      result += "\\\\";
    else if (*pp == '|')
      result += "\\|";
    else
      result += *pp;
  }
  return result;
}

string SortedPhonebook::unescapeString(char *line, unsigned int &pos)
{
  string result;
  bool escaped = false;

  while (! (line[pos] == '|' && ! escaped) && line[pos] != 0 &&
         line[pos] != CR && line[pos] != LF)
  {
    if (escaped)
    {
      escaped = false;
      if (line[pos] == 'r')
        result += CR;
      else if (line[pos] == 'n')
        result += LF;
      else if (line[pos] == '\\')
        result += '\\';
      else if (line[pos] == '|')
        result += '|';
      else
        result += line[pos];
    }
    else
      if (line[pos] == '\\')
        escaped = true;
      else
        result += line[pos];

    ++pos;
  }
  return result;
}

void SortedPhonebook::readPhonebookFile(istream &pbs, string filename)
  throw(GsmException)
{
  // read entries
  while (! pbs.eof())
  {
    char line[MAX_LINE_SIZE];
    pbs.getline(line, MAX_LINE_SIZE);

    if (strlen(line) == 0)
      continue;                 // skip empty lines

    if (pbs.bad())
      throw GsmException(stringPrintf(_("error reading from file '%s"),
                                      filename.c_str()),
                         OSError);

    // convert line to newEntry (line format : [index] '|' text '|' number
    string text, telephone;
    unsigned int pos = 0;

    // parse index
    string indexS = unescapeString(line, pos);
    int index = -1;
    if (indexS.length() == 0)
    {
      if (_useIndices)
        throw GsmException(stringPrintf(_("entry '%s' lacks index"), line),
                           ParserError);
    }
    else
    {
      index = checkNumber(indexS);
      _useIndices = true;
    }
    if (line[pos++] != '|')
      throw GsmException(stringPrintf(_("line '%s' has invalid format"), line),
                         ParserError);

    // parse text
    text = unescapeString(line, pos);
    if (line[pos++] != '|')
      throw GsmException(stringPrintf(_("line '%s' has invalid format"), line),
                         ParserError);

    // parse telephone number
    telephone = unescapeString(line, pos);

    insert(PhonebookEntryBase(telephone, text, index));
  }
}

void SortedPhonebook::sync(bool fromDestructor) throw(GsmException)
{
  // if not in file it already is stored in ME/TA
  if (! _fromFile) return;

  // if writing to stdout and not called from destructor ignore
  // (avoids writing to stdout multiple times)
  if (_filename == "" && ! fromDestructor) return;

  // find out if any of the entries have been updated
  if (! _changed)    // only look if we're not writing the file anyway
    for (iterator i = begin(); i != end(); i++)
      if (i->changed())
      {
        _changed = true;
        break;
      }

  if (_changed)
  {
    checkReadonly();
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
        pbs = new ofstream(_filename.c_str());
      
      if (pbs->bad())
        throw GsmException(
          stringPrintf(_("error opening file '%s' for writing"),
                       (_filename == "" ? _("<STDOUT>") :
                        _filename.c_str())),
          OSError);
    
      // and write the entries
      for (PhonebookMap::iterator i = _sortedPhonebook.begin();
           i != _sortedPhonebook.end(); ++i)
      {
        // convert entry to output line
        string line =
          (_useIndices ? intToStr(i->second->index()) : "") + "|" +
          escapeString(i->second->text()) + "|" +
          escapeString(i->second->telephone());
      
        // write out the line
        *pbs << line << endl;
        if (pbs->bad())
          throw GsmException(
            stringPrintf(_("error writing to file '%s'"),
                         (_filename == "" ? _("<STDOUT>") :
                          _filename.c_str())),
            OSError);
      }
    }
    catch(GsmException &e)
    {
      if (pbs != &cout) delete pbs;
      throw;
    }
    // close file
    if (pbs != &cout) delete pbs;

    // reset all changed states
    _changed = false;
    for (iterator j = begin(); j != end(); j++)
      j->resetChanged();
  }
}

void SortedPhonebook::checkReadonly() throw(GsmException)
{
  if (_readonly) throw GsmException(
    _("attempt to change phonebook read from <STDIN>"),
    ParameterError);
}

SortedPhonebook::SortedPhonebook(string filename, bool useIndices)
  throw(GsmException) :
  _changed(false), _fromFile(true), _madeBackupFile(false),
  _sortOrder(ByIndex), _useIndices(useIndices), _readonly(false),
  _filename(filename)
{
  // open the file
  ifstream pbs(filename.c_str());
  if (pbs.bad())
    throw GsmException(stringPrintf(_("cannot open file '%s'"),
                                    filename.c_str()),
                       OSError);
  // and read the file
  readPhonebookFile(pbs, filename);
}

SortedPhonebook::SortedPhonebook(bool fromStdin, bool useIndices)
  throw(GsmException) :
  _changed(false), _fromFile(true), _madeBackupFile(false),
  _sortOrder(ByIndex), _useIndices(useIndices), _readonly(fromStdin)
  // _filename is "" - this means stdout
{
  // read from stdin
  if (fromStdin)
    readPhonebookFile(cin, (string)_("<STDIN>"));
}

SortedPhonebook::SortedPhonebook(PhonebookRef mePhonebook)
  throw(GsmException) :
  _changed(false), _fromFile(false), _madeBackupFile(false),
  _sortOrder(ByIndex), _readonly(false), _mePhonebook(mePhonebook)
{
  int entriesRead = 0;
  reportProgress(0, _mePhonebook->end() - _mePhonebook->begin());

  for (Phonebook::iterator i = _mePhonebook->begin();
       i != _mePhonebook->end(); ++i)
  {
    if (! i->empty())
    {
      _sortedPhonebook.insert(
        PhonebookMap::value_type(PhoneMapKey(*this, lowercase(i->text())), i));
      ++entriesRead;
      if (entriesRead == _mePhonebook->size())
        return;                 // ready
    }
    reportProgress(i - _mePhonebook->begin());
  }
}

void SortedPhonebook::setSortOrder(SortOrder newOrder)
{
  if (newOrder == _sortOrder) return; // nothing to do

  PhonebookMap savedPhonebook = _sortedPhonebook; // save phonebook
  _sortedPhonebook = PhonebookMap(); // empty old phonebook
  _sortOrder = newOrder;

  // re-insert entries
  switch (newOrder)
  {
  case ByTelephone:
  {
    for (PhonebookMap::iterator i = savedPhonebook.begin();
         i != savedPhonebook.end(); ++i)
      _sortedPhonebook.
        insert(PhonebookMap::value_type(
          PhoneMapKey(*this, lowercase(i->second->telephone())), i->second));
    break;
  }
  case ByText:
  {
    for (PhonebookMap::iterator i = savedPhonebook.begin();
         i != savedPhonebook.end(); ++i)
      _sortedPhonebook.
        insert(PhonebookMap::value_type(
          PhoneMapKey(*this, lowercase(i->second->text())), i->second));
    break;
  }
  case ByIndex:
  {
    for (PhonebookMap::iterator i = savedPhonebook.begin();
         i != savedPhonebook.end(); ++i)
      _sortedPhonebook.
        insert(PhonebookMap::value_type(
          PhoneMapKey(*this, i->second->index()), i->second));
    break;
  }
  default:
    assert(0);
    break;
  }
}

unsigned int SortedPhonebook::getMaxTelephoneLen() const
{
  if (_fromFile)
    return UINT_MAX;
  else
    return _mePhonebook->getMaxTelephoneLen();
}

unsigned int SortedPhonebook::getMaxTextLen() const
{
  if (_fromFile)
    return UINT_MAX;
  else
    return _mePhonebook->getMaxTextLen();
}

int SortedPhonebook::max_size() const
{
  if (_fromFile)
    return _sortedPhonebook.max_size();
  else
    return _mePhonebook->max_size();
}

int SortedPhonebook::capacity() const
{
  if (_fromFile)
    return _sortedPhonebook.max_size();
  else
    return _mePhonebook->capacity();
}

SortedPhonebook::iterator
SortedPhonebook::insert(const PhonebookEntryBase& x) throw(GsmException)
{
  checkReadonly();
  _changed = true;
  PhonebookEntryBase *newEntry;

  if (_fromFile)
    if (_useIndices)
    {
      if (x.index() != -1)      // check that index is unique
      {
        for (PhonebookMap::iterator i = _sortedPhonebook.begin();
             i != _sortedPhonebook.end(); ++i)
          if (i->second->index() == x.index())
            throw GsmException(_("indices must be unique in phonebook"),
                               ParameterError);
        newEntry = new PhonebookEntryBase(x);
      }
      else                      // set index
      {
        SortOrder saveSortOrder = _sortOrder;
        setSortOrder(ByIndex);
        int index = 0;
        for (PhonebookMap::iterator i = _sortedPhonebook.begin();
             i != _sortedPhonebook.end(); ++i, ++index)
          if (i->second->index() != index)
            break;
        setSortOrder(saveSortOrder);
        newEntry = new PhonebookEntryBase();
        newEntry->set(x.telephone(), x.text(), index, true);
      }
    }
    else                        // index info in x is ignored
      newEntry = new PhonebookEntryBase(x);
  else
  {
    PhonebookEntry newMEEntry(x);
    newEntry = _mePhonebook->insert((PhonebookEntry*)NULL, newMEEntry);
  }
  switch (_sortOrder)
  {
  case ByTelephone:
    return
      _sortedPhonebook.
      insert(PhonebookMap::value_type(
        PhoneMapKey(*this, lowercase(newEntry->telephone())), newEntry));
  case ByText:
    return
      _sortedPhonebook.
      insert(PhonebookMap::value_type(
        PhoneMapKey(*this, lowercase(newEntry->text())), newEntry));
  case ByIndex:
    return
      _sortedPhonebook.
      insert(PhonebookMap::value_type(
        PhoneMapKey(*this, newEntry->index()), newEntry));
  default:
    assert(0);
    break;
  }
  return SortedPhonebook::iterator();
}

SortedPhonebook::iterator
SortedPhonebook::insert(iterator position, const PhonebookEntryBase& x)
  throw(GsmException)
{
  return insert(x);
}

SortedPhonebook::size_type SortedPhonebook::erase(string &key)
  throw(GsmException)
{
  // deallocate memory or remove from underlying ME phonebook
  for (PhonebookMap::iterator i =
         _sortedPhonebook.find(PhoneMapKey(*this, lowercase(key)));
       i != _sortedPhonebook.end() &&
         i->first == PhoneMapKey(*this, lowercase(key));
       ++i)
  {
    checkReadonly();
    _changed = true;
    if (_fromFile)
      delete i->second;
    else
      _mePhonebook->erase((Phonebook::iterator)i->second);
  }

  return _sortedPhonebook.erase(PhoneMapKey(*this, lowercase(key)));
}

SortedPhonebook::size_type SortedPhonebook::erase(int key)
  throw(GsmException)
{
  // deallocate memory or remove from underlying ME phonebook
  for (PhonebookMap::iterator i =
         _sortedPhonebook.find(PhoneMapKey(*this, key));
       i != _sortedPhonebook.end() && i->first == PhoneMapKey(*this, key);
       ++i)
  {
    checkReadonly();
    _changed = true;
    if (_fromFile)
      delete i->second;
    else
      _mePhonebook->erase((Phonebook::iterator)i->second);
  }

  return _sortedPhonebook.erase(PhoneMapKey(*this, key));
}

void SortedPhonebook::erase(iterator position)
  throw(GsmException)
{
  checkReadonly();
  _changed = true;
  // deallocate memory or remove from underlying ME phonebook
  if (_fromFile)
    delete ((PhonebookMap::iterator)position)->second;
  else
    _mePhonebook->erase((Phonebook::iterator)
                        ((PhonebookMap::iterator)position)->second);
  _sortedPhonebook.erase(position);
}

void SortedPhonebook::erase(iterator first, iterator last)
  throw(GsmException)
{
  checkReadonly();
  _changed = true;
  for (PhonebookMap::iterator i = first; i != last; ++i)
    if (_fromFile)
      delete i->second;
    else
      _mePhonebook->erase((Phonebook::iterator)i->second);
  _sortedPhonebook.erase(first, last);
}

void SortedPhonebook::clear() throw(GsmException)
{
  checkReadonly();
  _changed = true;
  for (iterator i = begin(); i != end(); i++)
    erase(i);
}

SortedPhonebook::~SortedPhonebook()
{
  if (_fromFile)
  {
    sync(true);
    for (PhonebookMap::iterator i = _sortedPhonebook.begin();
         i != _sortedPhonebook.end(); ++i)
      delete i->second;
  }
}

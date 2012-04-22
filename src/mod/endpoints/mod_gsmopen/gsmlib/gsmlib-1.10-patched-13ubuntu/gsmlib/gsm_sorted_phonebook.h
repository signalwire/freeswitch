// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sorted_phonebook.h
// *
// * Purpose: Alphabetically sorted phonebook
// *          (residing in files or in the ME)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 25.6.1999
// *************************************************************************

#ifndef GSM_SORTED_PHONEBOOK_H
#define GSM_SORTED_PHONEBOOK_H

#include <gsmlib/gsm_sorted_phonebook_base.h>
#include <gsmlib/gsm_phonebook.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_map_key.h>
#include <string>
#include <map>
#include <fstream>

using namespace std;

namespace gsmlib
{

  // The class SortedPhonebook makes the phonebook more manageable:
  // - empty slots in the ME phonebook are hidden by the API
  // - the class transparently handles phonebooks that reside in files

  class SortedPhonebook : public SortedPhonebookBase
  {
  private:
    bool _changed;              // true if file has changed after last save
    bool _fromFile;             // true if phonebook read from file
    bool _madeBackupFile;       // true if backup file was created
    SortOrder _sortOrder;       // sort order for the phonebook
    bool _useIndices;           // if phonebook from file: input file had
                                // indices; will write indices, too
    bool _readonly;             // =true if read from stdin
    string _filename;           // name of the file if phonebook from file
    PhonebookMap _sortedPhonebook; // phonebook from file
    PhonebookRef _mePhonebook;  // phonebook if from ME

    // convert CR and LF in string to "\r" and "\n" respectively
    string escapeString(string s);

    // convert "\r" and "\n" to CR and LF respectively
    // start parsing with pos, stop when CR, LF, 0, or '|' is encountered
    string unescapeString(char *line, unsigned int &pos);

    // initial read of phonebook file
    void readPhonebookFile(istream &pbs, string filename) throw(GsmException);

    // synchronize SortedPhonebook with file (no action if in ME)
    void sync(bool fromDestructor) throw(GsmException);
    
    // throw an exception if _readonly is set
    void checkReadonly() throw(GsmException);

  public:
    // iterator defs
    typedef SortedPhonebookIterator iterator;
    typedef PhonebookMap::size_type size_type;

    // constructor for file-based phonebook
    // expect indices in file if useIndices == true
    // read from file
    SortedPhonebook(string filename, bool useIndices)
      throw(GsmException);
    // read from stdin or start empty and write to stdout
    SortedPhonebook(bool fromStdin, bool useIndices)
      throw(GsmException);

    // constructor for ME-based phonebook
    SortedPhonebook(PhonebookRef mePhonebook) throw(GsmException);

    // return maximum telephone number length
    unsigned int getMaxTelephoneLen() const;

    // return maximum entry description length
    unsigned int getMaxTextLen() const;

    // handle sorting
    void setSortOrder(SortOrder newOrder);
    SortOrder sortOrder() const {return _sortOrder;}
    
    // phonebook traversal commands
    // these are suitable to use stdc++ lib algorithms and iterators
    // ME have fixed storage space implemented as memory slots
    // that may either be empty or used
    
    // traversal commands
    iterator begin() {return _sortedPhonebook.begin();}
    iterator end() {return _sortedPhonebook.end();}

    // the size macros return the number of used entries
    int size() const {return _sortedPhonebook.size();}
    int max_size() const;
    int capacity() const;
    bool empty() const throw(GsmException) {return size() == 0;}

    // existing iterators remain valid after an insert or erase operation
    // note: inserting many entries in indexed mode is inefficient
    // if the sort order is not set to indexed before

    // return position
    // insert only writes to available positions
    // warning: insert fails silently if size() == max_size()
    iterator insert(const PhonebookEntryBase& x) throw(GsmException);
    iterator insert(iterator position, const PhonebookEntryBase& x)
      throw(GsmException);

    PhonebookMap::size_type count(string &key)
      {return _sortedPhonebook.count(PhoneMapKey(*this, lowercase(key)));}
    iterator find(string &key)
      {return _sortedPhonebook.find(PhoneMapKey(*this, lowercase(key)));}
    iterator lower_bound(string &key)
      {return _sortedPhonebook.lower_bound(PhoneMapKey(*this,
                                                       lowercase(key)));}
    iterator upper_bound(string &key)
      {return _sortedPhonebook.upper_bound(PhoneMapKey(*this,
                                                       lowercase(key)));}
    pair<iterator, iterator> equal_range(string &key)
      {return _sortedPhonebook.equal_range(PhoneMapKey(*this,
                                                       lowercase(key)));}

    PhonebookMap::size_type count(int key)
      {return _sortedPhonebook.count(PhoneMapKey(*this, key));}
    iterator find(int key)
      {return _sortedPhonebook.find(PhoneMapKey(*this, key));}
    iterator lower_bound(int key)
      {return _sortedPhonebook.lower_bound(PhoneMapKey(*this, key));}
    iterator upper_bound(int key)
      {return _sortedPhonebook.upper_bound(PhoneMapKey(*this, key));}
    pair<iterator, iterator> equal_range(int key)
      {return _sortedPhonebook.equal_range(PhoneMapKey(*this, key));}

    size_type erase(string &key) throw(GsmException);
    size_type erase(int key) throw(GsmException);
    void erase(iterator position) throw(GsmException);
    void erase(iterator first, iterator last) throw(GsmException);
    void clear() throw(GsmException);

    // synchronize SortedPhonebook with file (no action if in ME)
    void sync() throw(GsmException) {sync(false);}
    
    // destructor
    // writes back change to file if phonebook is in file
    virtual ~SortedPhonebook();
  };

  //  typedef Ref<SortedPhonebook> SortedPhonebookRef;
};

#endif // GSM_SORTED_PHONEBOOK_H

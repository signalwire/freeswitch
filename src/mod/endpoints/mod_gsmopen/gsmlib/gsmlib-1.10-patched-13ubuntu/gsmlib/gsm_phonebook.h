// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_phonebook.h
// *
// * Purpose: Phonebook management functions
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 4.5.1999
// *************************************************************************

#ifndef GSM_PHONEBOOK_H
#define GSM_PHONEBOOK_H

#include <gsmlib/gsm_sorted_phonebook_base.h>
#include <gsmlib/gsm_at.h>
#include <gsmlib/gsm_util.h>
#include <string>
#include <iterator>
#include <vector>

using namespace std;

namespace gsmlib
{
  // forward declarations
  class Phonebook;

  // a single entry in the phonebook that corresponds to an ME entry

  class PhonebookEntry : public PhonebookEntryBase
  {
  private:
    // this constructor is only used by Phonebook
    PhonebookEntry() {}
    bool _cached;               // true, if this entry corresponds to info
                                // in the ME
    Phonebook *_myPhonebook;

  public:
    PhonebookEntry(string telephone, string text) :
      PhonebookEntryBase(telephone, text),
      _cached(true), _myPhonebook(NULL) {}
    PhonebookEntry(const PhonebookEntryBase &e) throw(GsmException);

    // accessor functions, inherited from PhonebookEntryBase
    // set() does not use the index argument
    void set(string telephone, string text, int index = -1,
             bool useIndex = false)
      throw(GsmException);
    string text() const throw(GsmException);
    string telephone() const throw(GsmException);

    // return true if entry is cached (and caching is enabled)
    bool cached() const;

    PhonebookEntry(const PhonebookEntry &e) throw(GsmException);
    PhonebookEntry &operator=(const PhonebookEntry &e) throw(GsmException);

    virtual ~PhonebookEntry() {}

    friend class Phonebook;
  };

  // this class corresponds to a phonebook in the ME
  // all functions directly update storage in the ME
  // if the ME is exchanged, the storage may become corrupted because
  // of internal buffering in the Phonebook class

  class Phonebook : public RefBase, public NoCopy
  {
  public:
    // iterator defs
    typedef PhonebookEntry *iterator;
    typedef const PhonebookEntry *const_iterator;
    typedef PhonebookEntry &reference;
    typedef const PhonebookEntry &const_reference;

  private:
    PhonebookEntry *_phonebook; // array of size _maxSize of entries
    int _maxSize;               // maximum size of pb (-1 == not known yet)
    int _size;                  // current size of pb (-1 == not known yet)
    string _phonebookName;      // name of the phonebook, 2-byte like "ME"
    unsigned int _maxNumberLength; // maximum length of telephone number
    unsigned int _maxTextLength; // maximum length of descriptive text
    Ref<GsmAt> _at;             // my GsmAt class
    vector<int> _positionMap;   // maps in-memory index to ME index
    MeTa &_myMeTa;              // the MeTa object that created this Phonebook
    bool _useCache;             // true if entries should be cached

    // helper function, parse phonebook response returned by ME/TA
    // returns index of entry
    int parsePhonebookEntry(string response, string &telephone, string &text);

    // internal access functions
    // read/write/find entry from/to ME
    void readEntry(int index, string &telephone, string &text)
      throw(GsmException);
    void writeEntry(int index, string telephone, string text)
      throw(GsmException);
    void findEntry(string text, int &index, string &telephone)
      throw(GsmException);

    // adjust size only if it was set once
    void adjustSize(int sizeAdjust)
      {
        if (_size != -1) _size += sizeAdjust;
      }

    // insert into first empty position and return position where inserted
    iterator insertFirstEmpty(const string telephone, const string text)
      throw(GsmException);

    // insert into specified index position
    iterator insert(const string telephone, const string text,
                    const int index);

    // used my class MeTa
    // load phonebook name phonebookName, use AT handler at
    // preload entire phonebook if preload == true
    Phonebook(string phonebookName, Ref<GsmAt> at,
              MeTa &myMeTa, bool preload = false) throw(GsmException);

  public:
    // set cache mode on or off
    void setCaching(bool useCache) {_useCache = useCache;}

    // return name of this phonebook (2-character string)
    string name() const {return _phonebookName;}

    // return maximum telephone number length
    unsigned int getMaxTelephoneLen() const {return _maxNumberLength;}

    // return maximum entry description length
    unsigned int getMaxTextLen() const { return _maxTextLength;}

    // phonebook traversal commands
    // these are suitable to use stdc++ lib algorithms and iterators
    // ME have fixed storage space implemented as memory slots
    // that may either be empty or used
    
    // traversal commands
    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;
    reference front();
    const_reference front() const;
    reference back();
    const_reference back() const;
    reference operator[](int n);
    const_reference operator[](int n) const;

    // the size macros return the number of used entries
    int size() const throw(GsmException);
    int max_size() const {return _maxSize;}
    int capacity() const {return _maxSize;}
    bool empty() const throw(GsmException) {return size() == 0;}

    // insert iterators insert into the first empty cell regardless of position
    // - existing iterators are not invalidated after an insert operation
    // - return position where it was actually inserted (may be != position)
    // - insert only writes to available positions
    // - throw an exception if size() == max_size() (ie. not empty slots)
    iterator insert(iterator position, const PhonebookEntry& x)
      throw(GsmException);

    // insert n times, same procedure as above
    void insert(iterator pos, int n, const PhonebookEntry& x)
      throw(GsmException);
    void insert(iterator pos, long n, const PhonebookEntry& x)
      throw(GsmException);

    // erase operators set used slots to "empty"
    iterator erase(iterator position) throw(GsmException);
    iterator erase(iterator first, iterator last) throw(GsmException);
    void clear() throw(GsmException);

    // finds an entry given the text
    iterator find(string text) throw(GsmException);
    
    // destructor
    virtual ~Phonebook();

    friend class PhonebookEntry;
    friend class MeTa;
  };

  // useful phonebook types
  typedef Ref<Phonebook> PhonebookRef;
  typedef vector<PhonebookRef> PhonebookVector;
};

#endif // GSM_PHONEBOOK_H

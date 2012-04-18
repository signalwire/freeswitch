// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sorted_phonebook_base.h
// *
// * Purpose: Virtual base class for alphabetically sorted phonebook
// *          The infrastructure in this module allows custom backends for
// *          storing phonebook entries to be integrated into gsmlib
// *          (eg. LDAP- or RDBMS-based phonebook stores).
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 5.6.2000
// *************************************************************************

#ifndef GSM_SORTED_PHONEBOOK_BASE_H
#define GSM_SORTED_PHONEBOOK_BASE_H

#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_map_key.h>
#include <string>
#include <map>
#include <fstream>

using namespace std;

namespace gsmlib
{
  // a single entry in a phonebook

  class PhonebookEntryBase : public RefBase
  {
  protected:
    bool _changed;              // set to true if _telephone or _text changed
    string _telephone;
    string _text;
    int _index;                 // my position in the phonebook
                                // == -1 if not used (can only happen if
                                // phonebook is read from file)
    bool _useIndex;             // compare indices in operator==,
                                // use _index for inserting into
                                // Phonebook

  public:
    PhonebookEntryBase() :
      _changed(false), _index(-1), _useIndex(false) {}

    // convenience constructor
    PhonebookEntryBase(string telephone, string text, int index = -1) :
      _changed(false), _telephone(telephone), _text(text),
      _index(index), _useIndex(false) {}

    // accessor functions
    virtual void set(string telephone, string text, int index = -1,
                     bool useIndex = false)
      throw(GsmException);
    virtual string text() const throw(GsmException);
    virtual string telephone() const throw(GsmException);

    // return true if both telephone and text are empty
    bool empty() const throw(GsmException);

    // set to true if operator== should compare the _index as well
    void setUseIndex(bool useIndex)
      {_useIndex = useIndex;}
    bool useIndex() const {return _useIndex;}
    
    // equality operator
    // if one of the operands has _useIndex == true
    // takes _index and e._index into account
    bool operator==(const PhonebookEntryBase &e) const;

    // return index
    int index() const {return _index;}

    // return true if entry changed
    bool changed() const {return _changed;}

    // reset the changed status (ie. if synced to file)
    void resetChanged() {_changed = false;}

    // return deep copy of this entry
    virtual Ref<PhonebookEntryBase> clone();
    
    PhonebookEntryBase(const PhonebookEntryBase &e) throw(GsmException);
    PhonebookEntryBase &operator=(const PhonebookEntryBase &e)
      throw(GsmException);

    virtual ~PhonebookEntryBase() {}
  };

  // MapKey for sortedPhonebook
  
  class SortedPhonebookBase;
  typedef MapKey<SortedPhonebookBase> PhoneMapKey;

  // maps text or telephone to entry
  
  typedef multimap<PhoneMapKey, PhonebookEntryBase*> PhonebookMap;

  // iterator for SortedPhonebook that hides the "second" member of the map
  
  typedef PhonebookMap::iterator PhonebookMapIterator;
  class SortedPhonebookIterator : public PhonebookMapIterator
  {
  public:
    SortedPhonebookIterator() {}
    SortedPhonebookIterator(PhonebookMap::iterator i) :
      PhonebookMapIterator(i) {}

    PhonebookEntryBase &operator*()
      {return *((PhonebookMap::iterator)*this)->second;}

    PhonebookEntryBase *operator->()
      {return ((PhonebookMap::iterator)*this)->second;}
  };

  // virtual base class for sorted phonebooks

  class SortedPhonebookBase : public RefBase, public NoCopy
  {
  public:
    // iterator defs
    typedef SortedPhonebookIterator iterator;
    typedef PhonebookMap::size_type size_type;

    // return maximum telephone number length
    virtual unsigned int getMaxTelephoneLen() const = 0;

    // return maximum entry description length
    virtual unsigned int getMaxTextLen() const = 0;

    // handle sorting
    virtual void setSortOrder(SortOrder newOrder) = 0;
    virtual SortOrder sortOrder() const = 0;
    
    // phonebook traversal commands
    // these are suitable to use stdc++ lib algorithms and iterators
    
    // traversal commands
    virtual iterator begin() = 0;
    virtual iterator end() = 0;

    // the size macros return the number of used entries
    virtual int size() const = 0;
    virtual int max_size() const = 0;
    virtual int capacity() const = 0;
    virtual bool empty() const throw(GsmException) = 0;

    // existing iterators remain valid after an insert or erase operation

    // return position
    // insert only writes to available positions
    // warning: insert fails silently if size() == max_size()
    virtual iterator insert(const PhonebookEntryBase& x) throw(GsmException)
      = 0;
    virtual iterator insert(iterator position, const PhonebookEntryBase& x)
      throw(GsmException) = 0;

    virtual PhonebookMap::size_type count(string &key) = 0;
    virtual iterator find(string &key) = 0;
    virtual iterator lower_bound(string &key) = 0;
    virtual iterator upper_bound(string &key) = 0;
    virtual pair<iterator, iterator> equal_range(string &key) = 0;

    virtual PhonebookMap::size_type count(int key) = 0;
    virtual iterator find(int key) = 0;
    virtual iterator lower_bound(int key) = 0;
    virtual iterator upper_bound(int key) = 0;
    virtual pair<iterator, iterator> equal_range(int key) = 0;

    virtual size_type erase(string &key) throw(GsmException) = 0;
    virtual size_type erase(int key) throw(GsmException) = 0;
    virtual void erase(iterator position) throw(GsmException) = 0;
    virtual void erase(iterator first, iterator last) throw(GsmException) = 0;
    virtual void clear() throw(GsmException) = 0;

    // synchronize SortedPhonebookBase with storage
    virtual void sync() throw(GsmException) = 0;

    virtual ~SortedPhonebookBase() {}
  };

  typedef Ref<SortedPhonebookBase> SortedPhonebookRef;


  // base factory class for custom backends
  class CustomPhonebookFactory
  {
  public:
    // return sorted phonebook object given the source specification
    // (eg. database name, URL, etc.)
    virtual SortedPhonebookRef createPhonebook(string source)
      throw(GsmException) = 0;
  };

  // registry for custom backends
  
  class CustomPhonebookRegistry
  {
    // registered factories
    static map<string, CustomPhonebookFactory*> *_factoryList;

  public:
    // register a factory class for a specific backend
    // (case does not matter for backend name)
    static void registerCustomPhonebookFactory(string backendName,
                                        CustomPhonebookFactory *factory)
      throw(GsmException);
      
    
    // return a phonebook object given the backend name and the source
    // specification
    static SortedPhonebookRef
    createPhonebook(string backendName, string source) throw(GsmException);
  };

};

#endif // GSM_SORTED_PHONEBOOK_BASE_H

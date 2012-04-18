// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_sorted_sms_store.h
// *
// * Purpose: Sorted SMS store (residing in files or in the ME)
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 14.8.1999
// *************************************************************************

#ifndef GSM_SORTED_SMS_STORE_H
#define GSM_SORTED_SMS_STORE_H

#include <gsmlib/gsm_sms_store.h>
#include <gsmlib/gsm_util.h>
#include <gsmlib/gsm_map_key.h>
#include <string>
#include <map>
#include <assert.h>

using namespace std;

namespace gsmlib
{
  // MapKey for SortedSMSStore
  
  class SortedSMSStore;
  typedef MapKey<SortedSMSStore> SMSMapKey;

  // maps key (see SortedSMSStore::SortOrder) to entry
  
  typedef multimap<SMSMapKey, SMSStoreEntry*> SMSStoreMap;

  // iterator for SortedSMSStore that hides the "second" member of the map
  
  typedef SMSStoreMap::iterator SMSStoreMapIterator;
  class SortedSMSStoreIterator : public SMSStoreMapIterator
  {
  public:
    SortedSMSStoreIterator() {}
    SortedSMSStoreIterator(SMSStoreMap::iterator i) :
      SMSStoreMapIterator(i) {}

    SMSStoreEntry &operator*()
      {return *((SMSStoreMap::iterator)*this)->second;}

    SMSStoreEntry *operator->()
      {return ((SMSStoreMap::iterator)*this)->second;}
  };

  // The class SortedSMSStore makes the SMS store more manageable:
  // - empty slots in the ME phonebook are hidden by the API
  // - the class transparently handles stores that reside in files

  class SortedSMSStore : public RefBase, public NoCopy
  {
  private:

    bool _changed;              // true if file has changed after last save
    bool _fromFile;             // true if store read from file
    bool _madeBackupFile;       // true if backup file was created
    SortOrder _sortOrder;       // sort order of the _sortedSMSStore
                                // (default is ByDate)
    bool _readonly;             // =true if read from stdin
    string _filename;           // name of the file if store from file
    SMSStoreMap _sortedSMSStore; // store from file
    SMSStoreRef _meSMSStore;    // store if from ME

    unsigned int _nextIndex;    // next index to use for file-based store

    // initial read of SMS file
    void readSMSFile(istream &pbs, string filename) throw(GsmException);
    
    // synchronize SortedSMSStore with file (no action if in ME)
    void sync(bool fromDestructor) throw(GsmException);
    
    // throw an exception if _readonly is set
    void checkReadonly() throw(GsmException);

  public:
    // iterator defs
    typedef SortedSMSStoreIterator iterator;
    typedef SMSStoreMap::size_type size_type;

    // constructor for file-based store
    // read from file
    SortedSMSStore(string filename) throw(GsmException);
    // read from stdin or start empty and write to stdout
    SortedSMSStore(bool fromStdin) throw(GsmException);

    // constructor for ME-based store
    SortedSMSStore(SMSStoreRef meSMSStore) throw(GsmException);

    // handle sorting
    void setSortOrder(SortOrder newOrder);
    SortOrder sortOrder() const {return _sortOrder;}
    
    // store traversal commands
    // these are suitable to use stdc++ lib algorithms and iterators
    
    // traversal commands
    iterator begin() {return _sortedSMSStore.begin();}
    iterator end() {return _sortedSMSStore.end();}

    // the size macros return the number of used entries
    int size() const {return _sortedSMSStore.size();}
    int max_size() const;
    int capacity() const;
    bool empty() const throw(GsmException) {return size() == 0;}

    // existing iterators may be invalidated after an insert operation
    // return position
    // insert only writes to available positions
    // warning: insert fails silently if size() == max_size()
    iterator insert(const SMSStoreEntry& x) throw(GsmException);
    iterator insert(iterator position, const SMSStoreEntry& x)
      throw(GsmException);

    SMSStoreMap::size_type count(Address &key)
      {
        assert(_sortOrder == ByAddress);
        return _sortedSMSStore.count(SMSMapKey(*this, key));
      }
    iterator find(Address &key)
      {
        assert(_sortOrder == ByAddress);
        return _sortedSMSStore.find(SMSMapKey(*this, key));
      }
    iterator lower_bound(Address &key)
      {
        assert(_sortOrder == ByAddress);
        return _sortedSMSStore.lower_bound(SMSMapKey(*this, key));
      }
    iterator upper_bound(Address &key)
      {
        assert(_sortOrder == ByAddress);
        return _sortedSMSStore.upper_bound(SMSMapKey(*this, key));
      }
    pair<iterator, iterator> equal_range(Address &key)
      {
        assert(_sortOrder == ByAddress);
        return _sortedSMSStore.equal_range(SMSMapKey(*this, key));
      }

    SMSStoreMap::size_type count(Timestamp &key)
      {
        assert(_sortOrder == ByDate);
        return _sortedSMSStore.count(SMSMapKey(*this, key));
      }
    iterator find(Timestamp &key)
      {
        assert(_sortOrder == ByDate);
        return _sortedSMSStore.find(SMSMapKey(*this, key));
      }
    iterator lower_bound(Timestamp &key)
      {
        assert(_sortOrder == ByDate);
        return _sortedSMSStore.lower_bound(SMSMapKey(*this, key));
      }
    iterator upper_bound(Timestamp &key)
      {
        assert(_sortOrder == ByDate);
        return _sortedSMSStore.upper_bound(SMSMapKey(*this, key));
      }
    pair<iterator, iterator> equal_range(Timestamp &key)
      {
        assert(_sortOrder == ByDate);
        return _sortedSMSStore.equal_range(SMSMapKey(*this, key));
      }

    SMSStoreMap::size_type count(int key)
      {
        assert(_sortOrder == ByIndex || _sortOrder == ByType);
        return _sortedSMSStore.count(SMSMapKey(*this, key));
      }
    iterator find(int key)
      {
        assert(_sortOrder == ByIndex || _sortOrder == ByType);
        return _sortedSMSStore.find(SMSMapKey(*this, key));
      }
    iterator lower_bound(int key)
      {
        assert(_sortOrder == ByIndex || _sortOrder == ByType);
        return _sortedSMSStore.lower_bound(SMSMapKey(*this, key));
      }
    iterator upper_bound(int key)
      {
        assert(_sortOrder == ByIndex || _sortOrder == ByType);
        return _sortedSMSStore.upper_bound(SMSMapKey(*this, key));
      }
    pair<iterator, iterator> equal_range(int key)
      {
        assert(_sortOrder == ByIndex || _sortOrder == ByType);
        return _sortedSMSStore.equal_range(SMSMapKey(*this, key));
      }

    size_type erase(Address &key) throw(GsmException);
    size_type erase(int key) throw(GsmException);
    size_type erase(Timestamp &key) throw(GsmException);
    void erase(iterator position) throw(GsmException);
    void erase(iterator first, iterator last) throw(GsmException);
    void clear() throw(GsmException);

    // synchronize SortedPhonebook with file (no action if in ME)
    void sync() throw(GsmException) {sync(false);}
    
    // destructor
    // writes back change to file if store is in file
    ~SortedSMSStore();
  };

  typedef Ref<SortedSMSStore> SortedSMSStoreRef;
};

#endif // GSM_SORTED_SMS_STORE_H

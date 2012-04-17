// *************************************************************************
// * GSM TA/ME library
// *
// * File:    gsm_map_key.h
// *
// * Purpose: Common MapKey implementation for the multimaps in
// *          gsm_sorted_sms_store and gsm_sorted_phonebook
// *
// * Author:  Peter Hofmann (software@pxh.de)
// *
// * Created: 5.11.1999
// *************************************************************************

#ifndef GSM_MAP_KEY_H
#define GSM_MAP_KEY_H

#include <gsmlib/gsm_sms_codec.h>

namespace gsmlib
{
  // sort order for MapKeys

  enum SortOrder {ByText = 0, ByTelephone = 1, ByIndex = 2, ByDate = 3,
                  ByType = 4, ByAddress = 5};

  // wrapper for map key, can access Sortedtore to get sortOrder()

  template <class SortedStore> class MapKey
  {
  public:
    SortedStore &_myStore;   // my store
    // different type keys
    Address _addressKey;
    Timestamp _timeKey;
    int _intKey;
    string _strKey;

  public:
    // constructors for the different sort keys
    MapKey(SortedStore &myStore, Address key) :
      _myStore(myStore), _addressKey(key) {}
    MapKey(SortedStore &myStore, Timestamp key) :
      _myStore(myStore), _timeKey(key) {}
    MapKey(SortedStore &myStore, int key) :
      _myStore(myStore), _intKey(key) {}
    MapKey(SortedStore &myStore, string key) :
      _myStore(myStore), _strKey(key) {}

/*
    friend
    bool operator< 
#ifndef WIN32
	<>
#endif
	                 (const MapKey<SortedStore> &x,
                      const MapKey<SortedStore> &y);
    friend
    bool operator==
#ifndef WIN32
	<>
#endif
	                  (const MapKey<SortedStore> &x,
                       const MapKey<SortedStore> &y);
*/
  };

  // compare two keys
  template <class SortedStore>
    extern bool operator<(const MapKey<SortedStore> &x,
                          const MapKey<SortedStore> &y);
  template <class SortedStore>
    extern bool operator==(const MapKey<SortedStore> &x,
                           const MapKey<SortedStore> &y);
  
  // MapKey members
  
  template <class SortedStore>
    bool operator<(const MapKey<SortedStore> &x,
                           const MapKey<SortedStore> &y)
    {
      assert(&x._myStore == &y._myStore);

      switch (x._myStore.sortOrder())
      {
      case ByDate:
        return x._timeKey < y._timeKey;
      case ByAddress:
        return x._addressKey < y._addressKey;
      case ByIndex:
      case ByType:
        return x._intKey < y._intKey;
      case ByTelephone:
        return Address(x._strKey) < Address(y._strKey);
      case ByText:
        return x._strKey < y._strKey;
      default:
        assert(0);
        return true;
      }
    }

  template <class SortedStore>
    bool operator==(const MapKey<SortedStore> &x,
                            const MapKey<SortedStore> &y)
    {
      assert(&x._myStore == &y._myStore);

      switch (x._myStore.sortOrder())
      {
      case ByDate:
        return x._timeKey == y._timeKey;
      case ByAddress:
        return x._addressKey == y._addressKey;
      case ByIndex:
      case ByType:
        return x._intKey == y._intKey;
      case ByTelephone:
        return Address(x._strKey) == Address(y._strKey);
      case ByText:
        return x._strKey == y._strKey;
      default:
        assert(0);
        return true;
      }
    }
};

#endif // GSM_MAP_KEY_H

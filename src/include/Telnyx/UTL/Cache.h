// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_CACHE_H_INCLUDED
#define TELNYX_CACHE_H_INCLUDED

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/any.hpp>
#include <boost/noncopyable.hpp>

#include "Telnyx/Telnyx.h"


namespace Telnyx {

class Cacheable : boost::noncopyable
{
public:
  typedef boost::shared_ptr<Cacheable> Ptr;
  Cacheable(const std::string& id,  const boost::any& data);
    /// Create a new cachable object.
    /// A cacheable object accepts a polymorphic type
    /// identifiable by an string ID

  boost::any& data();
    /// Return the data

  const std::string& getIdentifier() const;
    /// Return the cache identifier

private:
  boost::any _data;
  std::string _identifier;
};


class CacheManager : boost::noncopyable
{
public:
  CacheManager(int expireInSeconds);
    /// Create a new cache mamanager
  
  ~CacheManager();
    /// Delete cache manager

  void add(const std::string& id, const boost::any& obj);
    /// Insert a new cacheable object into the cache.
    /// If an object with the same id already exists,
    /// it will be overwritten

  void add(Cacheable::Ptr pCacheable);
    /// Insert a new cacheable object into the cache.
    /// If an object with the same id already exists,
    /// it will be overwritten

  Cacheable::Ptr get(const std::string& id) const;
    /// Get a pointer to a cacheable object specified by id

  Cacheable::Ptr pop(const std::string& id);
    /// Pop out a cacheable object specified by id

  void remove(const std::string& id);
    /// Erase a cacheable specified by id

  bool has(const std::string& id) const;
    /// Return true if the object is in cache

  void clear();
    /// Clear all entries

private:
  Telnyx::TELNYX_HANDLE _manager;
};

//
// Inlines
//

inline boost::any& Cacheable::data()
{
  return _data;
}

inline const std::string& Cacheable::getIdentifier() const
{
  return _identifier;
}

class StringPairCache : boost::noncopyable
{
public:
  StringPairCache(int expireInSeconds);
    /// Create a new cache mamanager

  ~StringPairCache();
    /// Delete cache manager

  void add(const std::string& id, const std::string& value);
    /// Insert a new cacheable object into the cache.
    /// If an object with the same id already exists,
    /// it will be overwritten

  std::string get(const std::string& id) const;
    /// Get a pointer to a cacheable object specified by id

  std::string pop(const std::string& id);
    /// Pop out a cacheable object specified by id

  void remove(const std::string& id);
    /// Erase a cacheable specified by id

  bool has(const std::string& id) const;
    /// Return true if the object is in cache

  void clear();
    /// Clear all entries

private:
  Telnyx::TELNYX_HANDLE _manager;
};

} // OSS

#endif // TELNYX_CACHE_H_INCLUDED


